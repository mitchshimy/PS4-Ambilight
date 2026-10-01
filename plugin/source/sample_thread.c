// sample_thread.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. The worker thread: resolves the live buffer, samples zones, applies color, sends UDP -- decoupled from the render/submission path.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "plugin_common.h"
#include "ambient_internal.h"

// v1.1: all the actual work -- resolving the live buffer, sampling 229
// zones, sending the UDP packet -- now happens here, on its own
// thread, at its own ~30Hz pace, never inside the render/submission
// path. Same scePthreadCreate pattern as detile_verify_probe's
// pad_poll_thread.
//
// Tradeoff this introduces (worth understanding, not a hidden cost):
// this thread can read g_currentDisplayBufferIndex / g_bufferAddrs /
// g_activeFormat at any point in a flip's lifecycle, including
// mid-update on another thread. Every one of those is a single
// word-sized volatile read/write (same pattern already used
// throughout this project without locks), so there's no torn-read risk
// on real hardware, but the *value* read could be up to one flip old.
// At ~30Hz sampling against ~60Hz flips, worst case this thread is
// working from a frame that's already been superseded by a newer one
// by the time it finishes reading -- i.e. the light output can lag
// real screen content by up to roughly one extra frame interval versus
// the old in-hook version. That's the price for never risking a stall
// on the thread the game actually needs to stay smooth, and is a far
// better tradeoff for a hobby ambient-light feature than occasional
// frame hitches during real play.
// v2.0: was a fixed #define (33*1000, ~30Hz). Now computed once at
// thread start from g_config.updateFrequencyHz (validated 1-240 at
// load time, see ambient_load_config). Kept as a plain local rather
// than a macro since it's no longer a compile-time constant.

// step 2: window size for the timing telemetry below.
// 30 iterations at the ~30Hz default sample rate is roughly one
// telemetry packet per second of real play -- frequent enough to see
// trends during a session, not so frequent it competes with the actual
// zone-color UDP traffic. TIMING_ENABLED lets this be compiled out
// entirely for a "final" build once the number is in hand, same spirit
// as PROBE_INCLUDE_NEO in detile_verify_probe.
#ifndef TIMING_ENABLED
#define TIMING_ENABLED 1
#endif
#define TIMING_WINDOW_SIZE 30

// v2.0 smoothing state: previous frame's post-processed color per
// zone, so each new sample can be blended toward instead of snapped
// to. This is a simple discrete integer approximation of an RC-style
// exponential smoothing, NOT a calibrated real time-constant -- alpha
// is derived from settlingTimeMs vs the actual sample interval and
// verified in Python to converge monotonically without oscillation
// across a range of settling times, but "settling_time_ms
// = 200" should be read as "roughly", not as a precise guarantee.
static uint8_t g_smoothedRgb[MAX_TOTAL_ZONES][3];
bool g_smoothedRgbValid = false; // false until the first real frame, so startup doesn't fade in from black

// v3.7 read-verify. A zone whose new read differs a lot from the last accepted
// one is re-read from the SAME buffer once the whole pass has finished
// sampling; if the two reads of one buffer disagree, the buffer was still
// being written (or cleared) while we looked, so the new value is discarded
// and the last accepted one is kept.
// This is title-agnostic: it never asks "which buffer / how many flips back",
// only "did this buffer hold still for the length of one pass".
//
// Tried first and dropped: a gate that only trusted a change once it held for
// 2 passes. A stale slot that stays stale repeats, so it "persists" and gets
// trusted, and every real transition paid an extra pass of delay for it (a GOWR
// capture showed the flash get through). Read-verify costs nothing on static
// content (no zone is flagged) and adds no delay to a real change: a stable
// buffer reads the same twice, so it is accepted on the first pass.
//
// Known limit: a stale slot that is genuinely not being touched during the
// pass reads identically twice and is accepted; only a buffer that changes
// under the sampler is caught. The hold is capped (VERIFY_MAX_HOLD passes per
// zone) so a title that rewrites its buffer continuously cannot freeze the
// lights. State resets with g_smoothedRgbValid (first frame, letterbox commit,
// live settings reload) so a deliberate hard cut is accepted immediately.
#define VERIFY_TOLERANCE 8   // raw 0-255 per channel: below this, a change isn't worth re-reading
#define VERIFY_MAX_HOLD  3   // consecutive passes one zone may be held before the new read is accepted anyway
static uint8_t g_acceptedRawRgb[MAX_TOTAL_ZONES][3]; // last verified RAW read per zone (pre color-processing)
static uint8_t g_verifyHold[MAX_TOTAL_ZONES];        // consecutive passes this zone has been held
static uint8_t s_rawFirst[MAX_TOTAL_ZONES][3];       // this pass's first read
static uint8_t s_suspect[MAX_TOTAL_ZONES];           // 1 = differs a lot from accepted, needs the re-read
#if (__FINAL__) == 0
static uint32_t s_rawDiagFrameCounter = 0; // v2.2.2: throttles the raw-pixel diagnostic packet to ~1x/sec (debug-only, see v2.2.4 gating note at its use site)
#endif


void *ambient_sample_thread(void *args)
{
    uint32_t sampleIntervalUs = 1000000u / g_config.updateFrequencyHz;
    uint32_t smoothingAlpha = 256u; // recomputed every iteration below (cheap), so live reload picks up changes
    // sceKernelGetProcessTimeCounter[Frequency] -- same TSC-based timer
    // already proven in this repo (frame_logger.prx). Needed
    // unconditionally now (not just under TIMING_ENABLED) since the
    // config-reload check below uses real elapsed time too.
    uint64_t tscFreq = sceKernelGetProcessTimeCounterFrequency();
    uint64_t lastReloadCheckTicks = sceKernelGetProcessTimeCounter();

#if TIMING_ENABLED
    uint64_t budgetTicks = (tscFreq * (uint64_t)sampleIntervalUs) / 1000000ULL;
    uint64_t minTicks = UINT64_MAX, maxTicks = 0, sumTicks = 0;
    uint32_t windowSamples = 0, overBudgetCount = 0, windowId = 0;
    // Pure observation via the confirmed sceKernelGetCurrentCpu() -- no
    // affinity/priority call involved, just recording where the scheduler
    // is actually placing this thread by default.
    int32_t minCpuSeen = INT32_MAX, maxCpuSeen = -1, lastCpu = -1;
    uint32_t migrationCount = 0;
#endif

    for (;;) {
        // Needed unconditionally now, not just under TIMING_ENABLED --
        // the config-reload check below also uses real elapsed time.
        uint64_t t0 = sceKernelGetProcessTimeCounter();

        // v2.4: throttled to roughly once a second (every ~30
        // iterations of this thread's own ~30Hz loop) rather than
        // every iteration -- this calls into a symbol resolved by
        // name against a real signature, which could only
        // corroborate from a third-party reimplementation (see the
        // struct's own comment above), not Sony's actual SDK or real
        // hardware, so its exposure is kept low while unproven. Once
        // a second is already far faster than a human needs for this.
        if (sceSystemServiceGetStatusPtr != NULL) {
            static uint32_t sysServicePollCounter = 0;
            if ((++sysServicePollCounter % 30) == 0) {
                AmbientSystemServiceStatus status;
                memset(&status, 0, sizeof(status));
                if (sceSystemServiceGetStatusPtr(&status) == 0) {
                    bool nowBackgrounded = status.isInBackgroundExecution;
                    if (nowBackgrounded != g_isBackgrounded) {
                        g_isBackgrounded = nowBackgrounded;
                        // true ("on") = foregrounded again, this plugin
                        // is driving the strip; false ("off") =
                        // backgrounded, hand control back to wled-relay.
                        // v2.6: explicit guard here now that
                        // relay_send_external_source() no longer gates
                        // itself internally -- this transition
                        // (foreground/background) is completely
                        // independent of relaySignalEnabled, so without
                        // this check a user who never opted in would
                        // still get a real send on every suspend/resume.
                        if (g_config.relaySignalEnabled) {
                            relay_send_external_source(!nowBackgrounded);
                        }
                    }
                }
            }
        }

        if (g_config.configReloadCheckSeconds > 0) {
            uint64_t elapsedTicks = t0 - lastReloadCheckTicks;
            if (elapsedTicks >= tscFreq * (uint64_t)g_config.configReloadCheckSeconds) {
                ambient_check_config_reload();
                lastReloadCheckTicks = t0;
                // Recompute everything derived from config that this
                // loop otherwise only computes once at thread start --
                // this IS the "live" part of live reload.
                sampleIntervalUs = 1000000u / g_config.updateFrequencyHz;
#if TIMING_ENABLED
                budgetTicks = (tscFreq * (uint64_t)sampleIntervalUs) / 1000000ULL;
#endif
            }
        }
        uint32_t smoothingIntervalMs = sampleIntervalUs / 1000u;
        smoothingAlpha = (smoothingIntervalMs * 256u) / (g_config.settlingTimeMs + 1u);
        if (smoothingAlpha > 256u) smoothingAlpha = 256u;

        if (g_isBackgrounded) {
            // v2.4: closes the real, confirmed gap from --
            // suspending via the PS button no longer leaves the strip
            // frozen showing the last on-screen frame indefinitely;
            // skip the capture/process/send pipeline below while
            // backgrounded. Placed AFTER the reload check/smoothing
            // recompute above (not before) so live config reload still
            // works even while suspended, and this recovers
            // immediately, with fresh settings, the instant this
            // plugin is foregrounded again.
            usleep(sampleIntervalUs);
            continue;
        }

        uint32_t displayBufferIndex = g_currentDisplayBufferIndex;
        uint64_t liveBufferAddr = (displayBufferIndex != 0xFFFFFFFFu &&
                                    displayBufferIndex < (uint32_t)MAX_TRACKED_BUFFERS)
                                       ? g_bufferAddrs[displayBufferIndex] : 0;
#if AMBIENT_SAMPLE_LAG >= 1
        {
            // Read a slot N flips back, which the GPU has had longer to
            // finish drawing; the slot the hook just reported may be
            // mid-render. Waits (see the v3.7 note below) until enough history
            // is known. A blank/no-frame flip (index sentinel) keeps
            // the old path (liveBufferAddr == 0 -> keepalive) instead of
            // re-reading a real slot.
            //
            // AMBIENT_SAMPLE_LAG only selects WHICH tracked variable to read
            // -- there is no way to derive "N flips back" from the current
            // index alone, so each level needs hooks.c to track its own
            // variable (g_prevDisplayBufferIndex for 1,
            // g_prevPrevDisplayBufferIndex for 2). A value of 2 used to
            // silently read the same slot as 1, since the old `>= 1` check
            // is also true for 2 and nothing tracked a second step back --
            // confirmed by three RDR1 captures at "lag 2" that were
            // byte-identical to lag 1. See docs/debugging/sample-lag-and-boot-flash.md.
#if AMBIENT_SAMPLE_LAG >= 2
            uint32_t prevIdx = g_prevPrevDisplayBufferIndex;
#else
            uint32_t prevIdx = g_prevDisplayBufferIndex;
#endif
            // v3.7: until the lag history exists, do NOT fall back to the
            // slot the hook just reported -- that is exactly the slot the
            // GPU is about to render into. Two GOWR captures (seq 0, flip
            // counter 4, i.e. the first pass after game boot) showed a
            // bright raw read there while all three buffers re-read black in
            // the same pass; that first read also seeds smoothing with no
            // prior value to fall back to, so it decayed on the strip as a
            // visible flash. No frame is sampled (liveBufferAddr = 0, the
            // existing keepalive path) for up to LAG_HISTORY_WAIT_PASSES
            // passes; a title that really never changes slot (single
            // buffer) then gets the old behaviour instead of staying dark.
#define LAG_HISTORY_WAIT_PASSES 30 // ~1 s at 30 Hz
            static uint32_t s_noHistoryPasses = 0;
            if (displayBufferIndex != 0xFFFFFFFFu) {
                if (prevIdx != 0xFFFFFFFFu && prevIdx < (uint32_t)MAX_TRACKED_BUFFERS &&
                    g_bufferAddrs[prevIdx] != 0) {
                    liveBufferAddr = g_bufferAddrs[prevIdx];
                    s_noHistoryPasses = 0;
                } else if (s_noHistoryPasses < LAG_HISTORY_WAIT_PASSES) {
                    s_noHistoryPasses++;
                    liveBufferAddr = 0;
                }
                // else: waited long enough -- keep the reported slot (old fallback)
            }
        }
#endif

        // v3.9: never read a buffer the kernel says is not mapped or CPU-readable. Mortal Kombat 11
        // crashed here (SIGSEGV, page not present) on its first sampling pass: its display buffers
        // are GPU-only (protection 0x30). ambient_resolve_readable() asks the kernel, and for a
        // GPU-only direct-memory buffer makes it CPU-readable (buffer_guard.c), returning the
        // address to read and setting g_readableLimit for the pixel-read bounds checks. 0 means
        // unreadable, which takes the existing keepalive path (liveBufferAddr = 0): the strip
        // holds its last color instead of the game dying.
        if (liveBufferAddr != 0) {
            liveBufferAddr = ambient_resolve_readable(liveBufferAddr);
        }

#if (__FINAL__) == 0
        // v3.1: what actually found "this title never lights up" -- see
        // hooks.c's g_pluginVersion comment. Sent UNCONDITIONALLY, not
        // just from inside the liveBufferAddr!=0 branch below -- the
        // whole point is to keep reporting the true hook-call-count
        // state even when the pipeline is going silent, same reasoning
        // as send_timing_packet's own always-on placement. Throttled to
        // roughly once a second, same ~30-iteration pattern already
        // used for the sceSystemServiceGetStatus poll above.
        {
            static uint32_t s_flipDiagCounter = 0;
            if ((++s_flipDiagCounter % 30) == 0) {
                send_flip_diag_packet(g_registerHookCallCount, g_flipHookCallCount,
                                       displayBufferIndex, liveBufferAddr,
                                       (uint32_t)g_haveValidFormat, g_activeFormat,
                                       g_videoOutSubmitFlipHookCallCount,
                                       g_submitFlipPtrResolved,
                                       g_gnmForWorkloadHookCallCount,
                                       g_gnmForWorkloadPtrResolved);
                send_guard_diag_packet(g_guardAvailable, g_guardRejectCount, g_guardLastRet, g_guardLastBadAddr);
                send_remap_diag_packet((g_gpuOnlyRemap & 0xF) | (sceKernelMapDirectMemory2Ptr ? 0x10u : 0) |
                                       (sceKernelMapDirectMemoryPtr ? 0x20u : 0) | (sceKernelMunmapPtr ? 0x40u : 0) | (sceKernelMprotectPtr ? 0x80u : 0) |
                                       ((uint32_t)g_remapLastMethod << 8),
                                       g_remapCreated, g_remapFailed, g_remapPasses, g_remapLastRet, g_remapLastAlias);
                if (g_guardRejectCount != 0) send_guard_info_packet(g_guardInfoStart, g_guardInfoEnd, g_guardInfoOffset, g_guardInfoAddr,
                                                                    g_guardInfoProt, g_guardInfoMemType, g_guardInfoFlags, g_guardLastRet);
            }
        }
#endif

        if (liveBufferAddr != 0 && g_haveValidFormat) {
            if (g_activeFormat == 0x80002200) {
                detectHdr2200Format(liveBufferAddr); // v3.6: alpha byte vote every pass, then the throttled smoothness check on holds -- see zones.c
            } else if (g_activeFormat == 0x88740000) {
                // v3.5: every pass, not throttled, and before the unpack
                // function is picked below so this frame gets the right one
                detectPq8bitMisregistration(liveBufferAddr);
            }
            PixelUnpackFn unpack = getUnpackFnForFormat(g_activeFormat);
            if (unpack != NULL) { // re-check -- format could have gone unknown since the last read
                // v2.9: throttled internally (autoLetterboxCheckIntervalFrames)
                // and a complete no-op when autoLetterboxEnabled is false --
                // see letterbox.c. Runs before this pass's own zone sampling
                // below so that a border committed just now is reflected in
                // THIS pass's g_zoneX/g_zoneY, not the next one.
                ambient_check_letterbox(&kParamsBase, liveBufferAddr, unpack);

                uint8_t rgbTriplets[MAX_TOTAL_ZONES * 3];
#if (__FINAL__) == 0
                // FLK1 flicker/flash probe (v3.3): raw pipeline read for the 6
                // fixed probe zones, captured here (before color processing)
                // so the debug packet below can show what the pipeline itself
                // saw, same as tools/flicker_capture.py expects. Trivial cost:
                // 6 comparisons per zone, compiled out entirely in release.
                uint8_t probeRaw[6][3];
                uint32_t probeZoneIdx[6];
                for (int pk = 0; pk < 6; pk++) probeZoneIdx[pk] = ((uint32_t)(2 * pk + 1) * g_numZones) / 12u;
#endif
                // Pass 1: sample every zone from the chosen buffer. Nothing
                // stateful (color processing, dark threshold) runs yet, so a
                // read that later gets rejected leaves no trace.
                bool anySuspect = false;
                for (uint32_t i = 0; i < g_numZones; i++) {
                    uint8_t r, g, b;
                    sampleZoneAverage(&kParamsBase, liveBufferAddr, unpack,
                                       g_zoneX[i], g_zoneY[i], &r, &g, &b);
#if (__FINAL__) == 0
                    for (int pk = 0; pk < 6; pk++) {
                        if (probeZoneIdx[pk] == i) { probeRaw[pk][0] = r; probeRaw[pk][1] = g; probeRaw[pk][2] = b; break; }
                    }
#endif
                    s_rawFirst[i][0] = r; s_rawFirst[i][1] = g; s_rawFirst[i][2] = b;
                    s_suspect[i] = 0;
                    if (g_smoothedRgbValid) {
                        for (int c = 0; c < 3; c++) {
                            int32_t d = (int32_t)s_rawFirst[i][c] - (int32_t)g_acceptedRawRgb[i][c];
                            if (d < 0) d = -d;
                            if (d > VERIFY_TOLERANCE) { s_suspect[i] = 1; anySuspect = true; break; }
                        }
                    }
                    if (!s_suspect[i]) g_verifyHold[i] = 0; // quiet zone: hold streak ends (a suspect zone keeps its count across passes)
                }

                // Pass 2: only zones that moved a lot are re-read, from the
                // same buffer, now that the rest of the pass has elapsed. Two
                // reads of one buffer that disagree mean it was mid-write.
                // (When a whole scene really changes every zone is re-read
                // once -- a stable buffer just agrees with itself.)
                if (anySuspect) {
                    for (uint32_t i = 0; i < g_numZones; i++) {
                        if (!s_suspect[i]) { g_verifyHold[i] = 0; continue; }
                        uint8_t r2, g2, b2;
                        sampleZoneAverage(&kParamsBase, liveBufferAddr, unpack,
                                           g_zoneX[i], g_zoneY[i], &r2, &g2, &b2);
                        bool torn = false;
                        int32_t d0 = (int32_t)r2 - (int32_t)s_rawFirst[i][0];
                        int32_t d1 = (int32_t)g2 - (int32_t)s_rawFirst[i][1];
                        int32_t d2 = (int32_t)b2 - (int32_t)s_rawFirst[i][2];
                        if (d0 < 0) d0 = -d0;
                        if (d1 < 0) d1 = -d1;
                        if (d2 < 0) d2 = -d2;
                        if (d0 > VERIFY_TOLERANCE || d1 > VERIFY_TOLERANCE || d2 > VERIFY_TOLERANCE) torn = true;
                        if (torn && g_verifyHold[i] < VERIFY_MAX_HOLD) {
                            g_verifyHold[i]++;
                            // discard: keep the last accepted value for this zone
                            s_rawFirst[i][0] = g_acceptedRawRgb[i][0];
                            s_rawFirst[i][1] = g_acceptedRawRgb[i][1];
                            s_rawFirst[i][2] = g_acceptedRawRgb[i][2];
                        } else {
                            g_verifyHold[i] = 0; // stable, or held long enough: accept
                            if (torn) { s_rawFirst[i][0] = r2; s_rawFirst[i][1] = g2; s_rawFirst[i][2] = b2; } // cap hit: newest read wins
                        }
                    }
                }

                // Pass 3: color processing, smoothing and output on the
                // verified value. Color processing/dark-threshold hysteresis
                // therefore runs exactly once per zone per pass, on a value
                // that was actually accepted.
                for (uint32_t i = 0; i < g_numZones; i++) {
                    g_acceptedRawRgb[i][0] = s_rawFirst[i][0];
                    g_acceptedRawRgb[i][1] = s_rawFirst[i][1];
                    g_acceptedRawRgb[i][2] = s_rawFirst[i][2];

                    uint8_t verified[3];
                    applyColorProcessing(s_rawFirst[i][0], s_rawFirst[i][1], s_rawFirst[i][2], verified);
                    applyDarkThreshold(i, &verified[0], &verified[1], &verified[2]);

                    if (g_config.smoothingEnabled) {
                        if (!g_smoothedRgbValid) {
                            g_smoothedRgb[i][0] = verified[0];
                            g_smoothedRgb[i][1] = verified[1];
                            g_smoothedRgb[i][2] = verified[2];
                        } else {
                            for (int c = 0; c < 3; c++) {
                                int32_t prev = g_smoothedRgb[i][c];
                                int32_t raw = verified[c];
                                int32_t delta = ((raw - prev) * (int32_t)smoothingAlpha) / 256;
                                // v2.7.5: C's / truncates toward zero, not
                                // toward -infinity -- so once |raw-prev| is
                                // small enough that |delta| rounds down to
                                // 0, prev stops moving AT ALL, forever,
                                // even though raw != prev. Confirmed by
                                // simulation with this project's own
                                // defaults (update_frequency_hz=30,
                                // settling_time_ms=100 -> smoothingAlpha=83):
                                // decaying toward a target of 0 from ANY
                                // starting brightness lands on exactly 3
                                // and sticks there permanently -- a dim
                                // residual glow that never reaches black on
                                // its own. This is what was reported as "a
                                // colorful scene's colors are retained
                                // dimly" after a cut to black. Forcing a
                                // minimum step of 1 toward raw whenever the
                                // proper proportional step would otherwise
                                // round to 0 guarantees this always
                                // converges to the exact target within a
                                // few extra frames, instead of stalling
                                // short of it indefinitely.
                                if (delta == 0 && raw != prev) delta = (raw > prev) ? 1 : -1;
                                g_smoothedRgb[i][c] = (uint8_t)(prev + delta);
                            }
                        }
                        rgbTriplets[i*3+0] = g_smoothedRgb[i][0];
                        rgbTriplets[i*3+1] = g_smoothedRgb[i][1];
                        rgbTriplets[i*3+2] = g_smoothedRgb[i][2];
                    } else {
                        rgbTriplets[i*3+0] = verified[0];
                        rgbTriplets[i*3+1] = verified[1];
                        rgbTriplets[i*3+2] = verified[2];
                    }
                }
                g_smoothedRgbValid = true;
                wled_send_rgb_zones(rgbTriplets, (int)g_numZones);

#if (__FINAL__) == 0
                // FLK1 probe (v3.3), decoded by tools/flicker_capture.py.
                // Exists to answer "is this a slot-timing problem, and did
                // the AMBIENT_SAMPLE_LAG fix close it" with a capture, not a
                // guess: for 6 zones spread around the strip, sends the
                // color actually output, the pipeline's raw read (from
                // liveBufferAddr -- i.e. AFTER the lag-1 buffer selection
                // above), AND the same zone re-read directly from buffer
                // slots 0, 1 and 2 right now. If the slot this pass used
                // (curIdx, adjusted by AMBIENT_SAMPLE_LAG) still disagrees
                // with its own re-read, or a bright raw value has no
                // matching bright slot anywhere, the fix did not close it.
                // Layout matches flicker_capture.py's HDR_FMT/ZONE_FMT
                // exactly; see that file's top comment for the byte map.
                {
                    static uint32_t s_probeSeq = 0;
                    static uint8_t  s_prevOut[MAX_TOTAL_ZONES * 3];
                    static bool     s_havePrevOut = false;

                    uint32_t deltaSum = 0;
                    uint32_t nBytes = g_numZones * 3;
                    if (s_havePrevOut) {
                        for (uint32_t k = 0; k < nBytes; k++) {
                            int d = (int)rgbTriplets[k] - (int)s_prevOut[k];
                            deltaSum += (uint32_t)(d < 0 ? -d : d);
                        }
                    }
                    memcpy(s_prevOut, rgbTriplets, nBytes);
                    s_havePrevOut = true;

                    if (g_numZones >= 12) {
                        uint8_t pkt[30 + 6 * 17];
                        memset(pkt, 0, sizeof(pkt));
                        uint32_t seq = s_probeSeq++;
                        uint32_t tsUs = (uint32_t)((t0 * 1000000ULL) / tscFreq);
                        uint32_t fmt = g_activeFormat;
                        uint16_t flipTotal = (uint16_t)((g_flipHookCallCount + g_videoOutSubmitFlipHookCallCount +
                                                         g_gnmForWorkloadHookCallCount) & 0xFFFFu);
                        // curIdx: the RAW slot the flip hook reported (displayBufferIndex), not the
                        // one this pass actually sampled. tools/flicker_capture.py's ring-lag math
                        // is built around that: it walks the ring forward from curIdx to find which
                        // lag is safe, and separately checks which lag the pipeline's own raw read
                        // (probeRaw, below) actually matches -- so it can tell "AMBIENT_SAMPLE_LAG is
                        // active and reading a clean slot" apart from "still reading the risky one",
                        // exactly the same way it already does for the AC3 menu capture. Recovering
                        // curIdx from liveBufferAddr instead (i.e. reporting the already-safe slot as
                        // if it were the raw one) would make every capture with the fix built in look
                        // identical to lag 0 always being clean, and silently drop the comparison this
                        // probe exists to make.
                        uint8_t curIdx = (displayBufferIndex < 255u) ? (uint8_t)displayBufferIndex : 0xFF;
                        int32_t bc = g_bufferCount;
                        uint8_t bufCount = (uint8_t)(bc < 0 ? 0 : (bc > 255 ? 255 : bc));
                        uint8_t flags = 0;
                        if (g_hdr2200IsHdr) flags |= 0x01;
                        if (g_config.smoothingEnabled) flags |= 0x02;
                        if (g_autoLetterboxTop || g_autoLetterboxRight || g_autoLetterboxBottom || g_autoLetterboxLeft) flags |= 0x04;
                        if (fmt == 0x80002200u) flags |= 0x08;
                        if (g_pq8bitMode) flags |= 0x10; // v3.5: 0x88740000 decoded as 8-bit ARGB
                        uint16_t nz = (uint16_t)g_numZones;

                        uint8_t validMask = 0;
                        uint8_t *zp = pkt + 30;
                        for (int k = 0; k < 6; k++) {
                            uint32_t zi = probeZoneIdx[k];
                            uint16_t zi16 = (uint16_t)zi;
                            memcpy(zp + 0, &zi16, 2);
                            memcpy(zp + 2, &rgbTriplets[zi * 3], 3);
                            memcpy(zp + 5, probeRaw[k], 3);
                            for (int j = 0; j < 3; j++) {
                                uint64_t baddr = (j < bc) ? g_bufferAddrs[j] : 0;
                                uint64_t savedLimit = g_readableLimit;
                                if (baddr != 0) baddr = ambient_resolve_readable(baddr); // v3.9: never raw-read a GPU-only slot
                                if (baddr != 0) {
                                    uint8_t br, bg, bb;
                                    sampleZoneAverage(&kParamsBase, baddr, unpack,
                                                       g_zoneX[zi], g_zoneY[zi], &br, &bg, &bb);
                                    zp[8 + j * 3 + 0] = br; zp[8 + j * 3 + 1] = bg; zp[8 + j * 3 + 2] = bb;
                                    validMask |= (uint8_t)(1u << j);
                                }
                                g_readableLimit = savedLimit;
                            }
                            zp += 17;
                        }
                        memcpy(pkt + 0, "FLK1", 4);
                        memcpy(pkt + 4, &seq, 4);
                        memcpy(pkt + 8, &tsUs, 4);
                        memcpy(pkt + 12, &deltaSum, 4);
                        memcpy(pkt + 16, &fmt, 4);
                        memcpy(pkt + 20, &flipTotal, 2);
                        pkt[22] = curIdx;
                        pkt[23] = bufCount;
                        pkt[24] = flags;
                        pkt[25] = 6;
                        pkt[26] = validMask | (uint8_t)(2u << 4); // slot ids trivial (0,1,2): buf2's slot id in bits 4-7
                        pkt[27] = 0x10;                            // buf0 slot id = 0 (lo nibble), buf1 slot id = 1 (hi nibble)
                        memcpy(pkt + 28, &nz, 2);
                        debug_send_raw(pkt, (int)sizeof(pkt));
                    }
                }
#endif

#if (__FINAL__) == 0
                // v2.2.4: verified behind __FINAL__==0 (make DEBUG=1), same
                // idiom frame_logger/force_1080p_display already use in
                // this repo. As written through v2.2.3, this ran
                // unconditionally on every real release build for every
                // end user: 3 extra sampleZoneAverage calls plus a UDP
                // send, on an ongoing basis for as long as HDR stayed
                // active -- real, avoidable CPU and network cost that
                // was still present in the exact build meant to REDUCE
                // CPU cost after the WLED-timeout root cause was found.
                // It has already served its purpose
                // (it's what let root cause get found at all) and has
                // no reason to ship active in a release build.
                //
                // v2.2.2: DIAGNOSTIC ONLY, no color-pipeline change --
                // the format-diagnostic in v2.2.1 confirmed this game's
                // HDR mode correctly registers as A2R10G10B10_BT2020_PQ
                // (recognized=1), and the PQ decode math itself was
                // hand-verified against neutral AND slightly-imbalanced
                // near-black input with no red bias found either way.
                // But the user's own physical observation is that MUCH
                // of the strip goes red, not just the zone nearest a
                // hypothetical red UI element -- which neither of those
                // findings explains, and rules out "it's just real
                // content in one corner". This sends the RAW pre-decode
                // 32-bit pixel word for 3 zones spread across the strip
                // (index 0, middle, last) alongside the already-decoded
                // 8-bit RGB pipeline output, so the raw captured value
                // itself -- not just the decoded interpretation of it --
                // can be checked.
                // Throttled to roughly every 3rd time this block runs
                // (NOT scaled to update_frequency_hz=30 like the first
                // attempt at this was) -- the color packets in the
                // user's own capture arrive roughly once per SECOND,
                // not 30x/sec, so gating on a 30-iteration threshold
                // meant this would take ~30 seconds of real time to
                // fire even once, and never showed up in a ~22-second
                // capture. Whatever's behind that slower real cadence
                // (dedup on unchanged color, or the loop genuinely not
                // running at its configured rate) is a separate
                // question from the one this packet exists to answer,
                // so this just fires on a small fixed count instead of
                // trying to match the real cadence.
                if (++s_rawDiagFrameCounter >= 3) {
                    s_rawDiagFrameCounter = 0;
                    uint32_t diagZones[3] = { 0, g_numZones / 2, g_numZones - 1 };
                    // v2.2.3: entry grew from 12 to 16 bytes -- ADDS the
                    // TRUE zone average (calling the real
                    // sampleZoneAverage, the exact same function/inputs
                    // the real color pipeline uses) alongside the single-
                    // center-pixel peek v2.2.2 already had. This is not
                    // a guess or a proxy: it's the literal value that
                    // gets handed to applyColorProcessing for these
                    // zones. Needed because hand-modeling a uniform raw
                    // input through gamma=2.4/saturation=175/black_level=
                    // 17 shows NO input value reproduces "default clean,
                    // tuned strongly red" -- gamma/saturation/levels are
                    // all zero-preserving and gamma=2.4 crushes small
                    // values further, not less, so guessing at the real
                    // average's magnitude isn't getting this further.
                    uint8_t diagPacket[4 + 3 * 16];
                    uint32_t formatSnapshot = g_activeFormat; // copy out of the volatile
                                                               // once here so memcpy below
                                                               // gets a plain uint32_t*, not
                                                               // a volatile-qualified one
                                                               // (was a harmless but real
                                                               // -Wincompatible-pointer-types
                                                               // -discards-qualifiers warning)
                    memcpy(diagPacket, &formatSnapshot, 4);
                    for (int k = 0; k < 3; k++) {
                        uint32_t zi = diagZones[k];
                        uint64_t off = getTiledElementByteOffset(&kParamsBase, g_zoneX[zi], g_zoneY[zi]);
                        uint32_t rawPx = 0;
                        if (off + 4 <= g_readableLimit)
                            memcpy(&rawPx, (const void*)(liveBufferAddr + off), 4);
                        uint8_t dr, dg, db;
                        unpack(rawPx, &dr, &dg, &db);
                        uint8_t avgR, avgG, avgB;
                        sampleZoneAverage(&kParamsBase, liveBufferAddr, unpack,
                                           g_zoneX[zi], g_zoneY[zi], &avgR, &avgG, &avgB);
                        uint8_t *entry = diagPacket + 4 + k * 16;
                        memcpy(entry + 0, &zi, 4);
                        memcpy(entry + 4, &rawPx, 4);
                        entry[8] = dr; entry[9] = dg; entry[10] = db;
                        entry[11] = avgR; entry[12] = avgG; entry[13] = avgB;
                        entry[14] = 0; entry[15] = 0;
                    }
                    debug_send_raw(diagPacket, sizeof(diagPacket));
                }
#endif
            } else {
                // v2.8: unpack == NULL -- format went unknown between the
                // outer check and here. Still no guessing at a color;
                // just keep WLED's realtime override alive with the last
                // verified one instead of going silent.
                wled_send_keepalive_if_stale(t0, tscFreq);
            }
        } else {
            // v2.8: liveBufferAddr == 0 or g_haveValidFormat == 0. Same
            // reasoning as above -- we're foregrounded (the
            // g_isBackgrounded branch already `continue`d otherwise) but
            // have no valid frame to sample this tick.
            wled_send_keepalive_if_stale(t0, tscFreq);
        }
        // If g_haveValidFormat is 0 (unknown/unconfirmed format), we
        // deliberately send nothing NEW rather than guess -- this is the
        // direct fix for how the original bug happened in the
        // first place: an unverified format assumption silently
        // producing wrong color instead of visibly doing nothing. The
        // heartbeat above only ever repeats a color the real pipeline
        // already verified, on the cases above where new/unknown/no-frame
        // states would otherwise mean total silence long enough to trip
        // WLED's own realtime-timeout fallback (see v2.8 changelog entry).

#if TIMING_ENABLED
        // Measured window intentionally covers the buffer-resolve +
        // sampling + color processing + UDP send above -- i.e.
        // everything this thread does per iteration except the sleep
        // itself. That's the number / actually cares about: is
        // the real work fast enough to comfortably fit inside
        // sampleIntervalUs, not how precisely usleep() is honored.
        uint64_t t1 = sceKernelGetProcessTimeCounter();
        uint64_t elapsedTicks = t1 - t0;
        if (elapsedTicks < minTicks) minTicks = elapsedTicks;
        if (elapsedTicks > maxTicks) maxTicks = elapsedTicks;
        sumTicks += elapsedTicks;
        if (elapsedTicks > budgetTicks) overBudgetCount++;
        windowSamples++;

        int32_t curCpu = sceKernelGetCurrentCpu();
        if (curCpu < minCpuSeen) minCpuSeen = curCpu;
        if (curCpu > maxCpuSeen) maxCpuSeen = curCpu;
        if (lastCpu != -1 && curCpu != lastCpu) migrationCount++;
        lastCpu = curCpu;

        if (windowSamples >= TIMING_WINDOW_SIZE) {
            uint32_t minUs = (uint32_t)((minTicks * 1000000ULL) / tscFreq);
            uint32_t maxUs = (uint32_t)((maxTicks * 1000000ULL) / tscFreq);
            uint32_t avgUs = (uint32_t)(((sumTicks / windowSamples) * 1000000ULL) / tscFreq);
            send_timing_packet(minUs, maxUs, avgUs, windowSamples, overBudgetCount, windowId++,
                                (uint32_t)minCpuSeen, (uint32_t)maxCpuSeen, migrationCount);
            // v3.9.1: kept for the report file (report.c), once per window, not per pass.
            g_reportPassUsAvg = avgUs; g_reportPassUsMax = maxUs; g_reportTimingWindows++;
            minTicks = UINT64_MAX; maxTicks = 0; sumTicks = 0;
            windowSamples = 0; overBudgetCount = 0;
            minCpuSeen = INT32_MAX; maxCpuSeen = -1; migrationCount = 0;
        }
#endif

        g_reportLoopPasses++; // v3.9.1: lets the report file say the sampler got past its first pass
        usleep(sampleIntervalUs);
    }
    return NULL;
}

