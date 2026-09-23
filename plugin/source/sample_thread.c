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

// handoff §30 step 2: window size for the timing telemetry below.
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
// across a range of settling times (handoff §43), but "settling_time_ms
// = 200" should be read as "roughly", not as a precise guarantee.
static uint8_t g_smoothedRgb[MAX_TOTAL_ZONES][3];
bool g_smoothedRgbValid = false; // false until the first real frame, so startup doesn't fade in from black
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
        // name against a real signature this session could only
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
            // v2.4: closes the real, confirmed gap from handoff §36 --
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

        if (liveBufferAddr != 0 && g_haveValidFormat) {
            if (g_activeFormat == 0x80002200) {
                detectHdr2200Format(liveBufferAddr); // cheap, throttled -- see its own comment
            }
            PixelUnpackFn unpack = getUnpackFnForFormat(g_activeFormat);
            if (unpack != NULL) { // re-check -- format could have gone unknown since the last read
                uint8_t rgbTriplets[MAX_TOTAL_ZONES * 3];
                for (uint32_t i = 0; i < g_numZones; i++) {
                    uint8_t r, g, b;
                    sampleZoneAverage(&kParamsBase, liveBufferAddr, unpack,
                                       g_zoneX[i], g_zoneY[i], &r, &g, &b);

                    uint8_t processed[3];
                    applyColorProcessing(r, g, b, processed);
                    applyDarkThreshold(i, &processed[0], &processed[1], &processed[2]);

                    if (g_config.smoothingEnabled) {
                        if (!g_smoothedRgbValid) {
                            g_smoothedRgb[i][0] = processed[0];
                            g_smoothedRgb[i][1] = processed[1];
                            g_smoothedRgb[i][2] = processed[2];
                        } else {
                            for (int c = 0; c < 3; c++) {
                                int32_t prev = g_smoothedRgb[i][c];
                                int32_t raw = processed[c];
                                g_smoothedRgb[i][c] = (uint8_t)(prev + ((raw - prev) * (int32_t)smoothingAlpha) / 256);
                            }
                        }
                        rgbTriplets[i*3+0] = g_smoothedRgb[i][0];
                        rgbTriplets[i*3+1] = g_smoothedRgb[i][1];
                        rgbTriplets[i*3+2] = g_smoothedRgb[i][2];
                    } else {
                        rgbTriplets[i*3+0] = processed[0];
                        rgbTriplets[i*3+1] = processed[1];
                        rgbTriplets[i*3+2] = processed[2];
                    }
                }
                g_smoothedRgbValid = true;
                wled_send_rgb_zones(rgbTriplets, (int)g_numZones);

#if (__FINAL__) == 0
                // v2.2.4: gated behind __FINAL__==0 (make DEBUG=1), same
                // idiom frame_logger/force_1080p_display already use in
                // this repo. As written through v2.2.3, this ran
                // unconditionally on every real release build for every
                // end user: 3 extra sampleZoneAverage calls plus a UDP
                // send, on an ongoing basis for as long as HDR stayed
                // active -- real, avoidable CPU and network cost that
                // was still present in the exact build meant to REDUCE
                // CPU cost after the WLED-timeout root cause was found
                // (see handoff §53). It has already served its purpose
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
                // 8-bit RGB this session already has visibility into,
                // so the raw captured value itself -- not just this
                // session's interpretation of it -- can be checked.
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
                        if (off + 4 <= BASE_PADDED_BUFFER_BYTES)
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
            }
        }
        // If g_haveValidFormat is 0 (unknown/unconfirmed format), we
        // deliberately send nothing rather than guess -- this is the
        // direct fix for how the original §22 bug happened in the
        // first place: an unverified format assumption silently
        // producing wrong color instead of visibly doing nothing.

#if TIMING_ENABLED
        // Measured window intentionally covers the buffer-resolve +
        // sampling + color processing + UDP send above -- i.e.
        // everything this thread does per iteration except the sleep
        // itself. That's the number §26/§30 actually cares about: is
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
            minTicks = UINT64_MAX; maxTicks = 0; sumTicks = 0;
            windowSamples = 0; overBudgetCount = 0;
            minCpuSeen = INT32_MAX; maxCpuSeen = -1; migrationCount = 0;
        }
#endif

        usleep(sampleIntervalUs);
    }
    return NULL;
}

