// sample_thread.c -- part of ps4_ambient_light, split out of the original single-file main.c. The...
// More: docs/code-comments.md (plugin/source/sample_thread.c:1)

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
#include "smoothing.h"

// v1.1: all the actual work -- resolving the live buffer, sampling 229 zones, sending the UDP packet...
// More: docs/code-comments.md (plugin/source/sample_thread.c:22)

// step 2: window size for the timing telemetry below. 30 iterations at the ~30Hz default sample rate...
// More: docs/code-comments.md (plugin/source/sample_thread.c:48)
#ifndef TIMING_ENABLED
#define TIMING_ENABLED 1
#endif
#define TIMING_WINDOW_SIZE 30

// v2.0 smoothing state: previous frame's post-processed color per zone, so each new sample can be...
// More: docs/code-comments.md (plugin/source/sample_thread.c:60)
static uint8_t g_smoothedRgb[MAX_TOTAL_ZONES][3];
bool g_smoothedRgbValid = false; // false until the first real frame, so startup doesn't fade in from black

// v3.7 read-verify. A zone whose new read differs a lot from the last accepted one is re-read from the...
// More: docs/code-comments.md (plugin/source/sample_thread.c:71)
#define VERIFY_TOLERANCE 8   // raw 0-255 per channel: below this, a change isn't worth re-reading
#define VERIFY_MAX_HOLD  3   // consecutive passes one zone may be held before the new read is accepted anyway
static uint8_t g_acceptedRawRgb[MAX_TOTAL_ZONES][3]; // last verified RAW read per zone (pre color-processing)
static uint8_t g_verifyHold[MAX_TOTAL_ZONES];        // consecutive passes this zone has been held
static uint8_t s_rawFirst[MAX_TOTAL_ZONES][3];       // this pass's first read
static uint8_t s_suspect[MAX_TOTAL_ZONES];           // 1 = differs a lot from accepted, needs the re-read
#if (__FINAL__) == 0
static uint32_t s_rawDiagFrameCounter = 0; // v2.2.2: throttles the raw-pixel diagnostic packet to ~1x/sec (debug-only, see v2.2.4 gating note at its use site)
#endif


// LAUNCH_GAP milestones (docs/debugging/launch-gap-test.md).
// More: docs/code-comments.md (plugin/source/sample_thread.c:103)
static void launch_gap_milestones(void)
{
    static bool seenRegister = false, seenFlip = false, seenFormat = false;
    uint64_t now = sceKernelGetProcessTimeCounter();
    uint64_t hz  = sceKernelGetProcessTimeCounterFrequency();
    if (!seenRegister && g_registerHookCallCount != 0) {
        seenRegister = true;
        ambient_handoff_diag("HANDOFF_DIAG LG_FIRST_REGISTER_HOOK calls=%u fmt=0x%08x ok=%d bg=%d tsc=%llu hz=%llu",
                             (unsigned)g_registerHookCallCount, (unsigned)g_activeFormat, (int)g_haveValidFormat,
                             g_isBackgrounded ? 1 : 0, (unsigned long long)now, (unsigned long long)hz);
    }
    if (!seenFlip) {
        uint32_t flips = g_flipHookCallCount + g_videoOutSubmitFlipHookCallCount + g_gnmForWorkloadHookCallCount;
        if (flips != 0) {
            seenFlip = true;
            ambient_handoff_diag("HANDOFF_DIAG LG_FIRST_FLIP_HOOK gnm=%u vo=%u wl=%u idx=%u bg=%d tsc=%llu hz=%llu",
                                 (unsigned)g_flipHookCallCount, (unsigned)g_videoOutSubmitFlipHookCallCount,
                                 (unsigned)g_gnmForWorkloadHookCallCount, (unsigned)g_currentDisplayBufferIndex,
                                 g_isBackgrounded ? 1 : 0, (unsigned long long)now, (unsigned long long)hz);
        }
    }
    if (!seenFormat && g_haveValidFormat) {
        seenFormat = true;
        ambient_handoff_diag("HANDOFF_DIAG LG_FORMAT_CONFIRMED fmt=0x%08x bg=%d tsc=%llu hz=%llu",
                             (unsigned)g_activeFormat, g_isBackgrounded ? 1 : 0,
                             (unsigned long long)now, (unsigned long long)hz);
    }

    // Mid-session changes (the milestones above fire once; these catch what happens after).
    // More: docs/code-comments.md (plugin/source/sample_thread.c:135)
    {
        static uint32_t lastFmt = 0, lastCalls = 0;
        static int lastValid = 0;
        static unsigned emitted = 0;
        const unsigned LG_CHANGE_CAP = 16;
        int valid = g_haveValidFormat ? 1 : 0;
        uint32_t fmt = (uint32_t)g_activeFormat;
        uint32_t calls = (uint32_t)g_registerHookCallCount;
        static bool primed = false; // first pass after LG_FORMAT_CONFIRMED only records the baseline
        if (seenFormat && primed && emitted < LG_CHANGE_CAP) {
            if (valid != lastValid || (valid && fmt != lastFmt)) {
                emitted++;
                ambient_handoff_diag("HANDOFF_DIAG LG_FORMAT_CHANGED old=0x%08x new=0x%08x ok=%d bg=%d tsc=%llu hz=%llu",
                                     (unsigned)lastFmt, (unsigned)fmt, valid, g_isBackgrounded ? 1 : 0,
                                     (unsigned long long)now, (unsigned long long)hz);
            }
            if (calls != lastCalls && lastCalls != 0 && emitted < LG_CHANGE_CAP) {
                emitted++;
                ambient_handoff_diag("HANDOFF_DIAG LG_REREGISTER calls=%u fmt=0x%08x ok=%d bg=%d tsc=%llu hz=%llu",
                                     (unsigned)calls, (unsigned)fmt, valid, g_isBackgrounded ? 1 : 0,
                                     (unsigned long long)now, (unsigned long long)hz);
            }
        }
        if (seenFormat) { lastFmt = fmt; lastValid = valid; primed = true; }
        if (seenRegister) lastCalls = calls;
    }
}

void *ambient_sample_thread(void *args)
{
    ambient_handoff_diag("HANDOFF_DIAG LG_SAMPLER_START bg=%d tsc=%llu hz=%llu", g_isBackgrounded ? 1 : 0,
                         (unsigned long long)sceKernelGetProcessTimeCounter(),
                         (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
    uint32_t sampleIntervalUs = 1000000u / g_config.updateFrequencyHz;
    uint32_t smoothingAlpha = 256u; // recomputed every iteration below (cheap), so live reload picks up changes
    // sceKernelGetProcessTimeCounter[Frequency] -- same TSC-based timer already proven in this repo...
    // More: docs/code-comments.md (plugin/source/sample_thread.c:173)
    uint64_t tscFreq = sceKernelGetProcessTimeCounterFrequency();
    uint64_t lastReloadCheckTicks = sceKernelGetProcessTimeCounter();

#if TIMING_ENABLED
    uint64_t budgetTicks = (tscFreq * (uint64_t)sampleIntervalUs) / 1000000ULL;
    uint64_t minTicks = UINT64_MAX, maxTicks = 0, sumTicks = 0;
    uint32_t windowSamples = 0, overBudgetCount = 0, windowId = 0;
    // Pure observation via the confirmed sceKernelGetCurrentCpu() -- no affinity/priority call involved...
    // More: docs/code-comments.md (plugin/source/sample_thread.c:184)
    int32_t minCpuSeen = INT32_MAX, maxCpuSeen = -1, lastCpu = -1;
    uint32_t migrationCount = 0;
#endif

    for (;;) {
        // Needed unconditionally now, not just under TIMING_ENABLED --
        // the config-reload check below also uses real elapsed time.
        uint64_t t0 = sceKernelGetProcessTimeCounter();

        // v2.4: throttled to roughly once a second (every ~30 iterations of this thread's own ~30Hz loop)...
        // More: docs/code-comments.md (plugin/source/sample_thread.c:196)
        if (sceSystemServiceGetStatusPtr != NULL) {
            static uint32_t sysServicePollCounter = 0;
            if ((++sysServicePollCounter % 30) == 0) {
                AmbientSystemServiceStatus status;
                memset(&status, 0, sizeof(status));
                if (sceSystemServiceGetStatusPtr(&status) == 0) {
                    bool nowBackgrounded = status.isInBackgroundExecution;
                    if (nowBackgrounded != g_isBackgrounded) {
                        g_isBackgrounded = nowBackgrounded;
                        ambient_handoff_diag("HANDOFF_DIAG GAME_FG_POLL fg=%d tsc=%llu hz=%llu",
                               nowBackgrounded ? 0 : 1,
                               (unsigned long long)sceKernelGetProcessTimeCounter(),
                               (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
                        // true ("on") = foregrounded again, this plugin is driving the strip; false ("off") = backgrounded...
                        // More: docs/code-comments.md (plugin/source/sample_thread.c:217)
                        if (g_config.relaySignalEnabled) {
                            if (nowBackgrounded) relay_release_external_source();
                            else                 relay_send_external_source(true);
                        }
                    }
                }
            }
        }

        if (g_config.configReloadCheckSeconds > 0) {
            uint64_t elapsedTicks = t0 - lastReloadCheckTicks;
            if (elapsedTicks >= tscFreq * (uint64_t)g_config.configReloadCheckSeconds) {
                uint32_t loadsBefore = g_configLoadCount;
                ambient_check_config_reload();
                if (g_configLoadCount != loadsBefore) ambient_notices_on_reload(); // the file changed and was re-read
                lastReloadCheckTicks = t0;
                // Recompute everything derived from config that this loop otherwise only computes once at thread start...
                // More: docs/code-comments.md (plugin/source/sample_thread.c:245)
                sampleIntervalUs = 1000000u / g_config.updateFrequencyHz;
#if TIMING_ENABLED
                budgetTicks = (tscFreq * (uint64_t)sampleIntervalUs) / 1000000ULL;
#endif
            }
        }
        smoothingAlpha = smoothing_alpha(sampleIntervalUs, g_config.settlingTimeMs);

        launch_gap_milestones(); // once-per-process events; a few loads and compares per pass otherwise

        if (g_isBackgrounded) {
            // v2.4: closes the real, confirmed gap from -- suspending via the PS button no longer leaves the strip...
            // More: docs/code-comments.md (plugin/source/sample_thread.c:259)
            usleep(sampleIntervalUs);
            continue;
        }

        uint32_t displayBufferIndex = g_currentDisplayBufferIndex;
        uint64_t liveBufferAddr = (displayBufferIndex != 0xFFFFFFFFu &&
                                    displayBufferIndex < (uint32_t)MAX_TRACKED_BUFFERS)
                                       ? g_bufferAddrs[displayBufferIndex] : 0;
#if AMBIENT_SAMPLE_LAG >= 1
        {
            // Read a slot N flips back, which the GPU has had longer to finish drawing; the slot the hook just...
            // More: docs/code-comments.md (plugin/source/sample_thread.c:278)
#if AMBIENT_SAMPLE_LAG >= 2
            uint32_t prevIdx = g_prevPrevDisplayBufferIndex;
#else
            uint32_t prevIdx = g_prevDisplayBufferIndex;
#endif
            // Until the lag history exists, do not sample the slot the hook just reported (the GPU is about to render into it):
            // that gave a bright first read and a visible flash. No frame is sampled for a few passes instead. [docs/code-comments.md]
#define LAG_HISTORY_WAIT_PASSES 30 // ~1 s at 30 Hz
            static uint32_t s_noHistoryPasses = 0;
            if (displayBufferIndex != 0xFFFFFFFFu) {
                static bool lgSeenHistory = false, lgSeenFallback = false;
                if (prevIdx != 0xFFFFFFFFu && prevIdx < (uint32_t)MAX_TRACKED_BUFFERS &&
                    g_bufferAddrs[prevIdx] != 0) {
                    liveBufferAddr = g_bufferAddrs[prevIdx];
                    if (!lgSeenHistory) {
                        lgSeenHistory = true;
                        ambient_handoff_diag("HANDOFF_DIAG LG_LAG_HISTORY_READY prev=%u waited=%u tsc=%llu hz=%llu",
                                             (unsigned)prevIdx, (unsigned)s_noHistoryPasses,
                                             (unsigned long long)sceKernelGetProcessTimeCounter(),
                                             (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
                    }
                    s_noHistoryPasses = 0;
                } else if (s_noHistoryPasses < LAG_HISTORY_WAIT_PASSES) {
                    s_noHistoryPasses++;
                    liveBufferAddr = 0;
                } else if (!lgSeenFallback) {
                    lgSeenFallback = true; // waited long enough -- keep the reported slot (old fallback)
                    ambient_handoff_diag("HANDOFF_DIAG LG_LAG_WAIT_EXPIRED passes=%u tsc=%llu hz=%llu",
                                         (unsigned)s_noHistoryPasses,
                                         (unsigned long long)sceKernelGetProcessTimeCounter(),
                                         (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
                }
            }
        }
#endif

        // Never read a buffer the kernel says is unmapped or not CPU-readable (Mortal Kombat 11 crashed here);
        // 0 from ambient_resolve_readable() takes the keepalive path and the strip holds its last colour. [docs/code-comments.md]
        bool bufferUnreadable = false;
        if (liveBufferAddr != 0) {
            liveBufferAddr = ambient_resolve_readable(liveBufferAddr);
            bufferUnreadable = (liveBufferAddr == 0);
        }
        // Notices for a title this plugin can't use (notices.c): it reports only after ~5 s of the same state.
        ambient_notices_tick(t0, tscFreq, !g_haveValidFormat && g_registerHookCallCount != 0 && g_activeFormat != 0,
                              g_activeFormat, bufferUnreadable);

#if (__FINAL__) == 0
        // v3.1: what actually found "this title never lights up" -- see hooks.c's g_pluginVersion comment.
        // More: docs/code-comments.md (plugin/source/sample_thread.c:356)
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
            static bool handoffDiagFirstFrame = false;
            if (!handoffDiagFirstFrame) {
                handoffDiagFirstFrame = true;
                ambient_handoff_diag("HANDOFF_DIAG GAME_FIRST_VALID_FRAME idx=%u fmt=0x%08x zones=%u tsc=%llu hz=%llu",
                       (unsigned)displayBufferIndex, (unsigned)g_activeFormat, (unsigned)g_numZones,
                       (unsigned long long)sceKernelGetProcessTimeCounter(),
                       (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
            }
            if (g_activeFormat == 0x80002200) {
                detectHdr2200Format(liveBufferAddr); // v3.6: alpha byte vote every pass, then the throttled smoothness check on holds -- see zones.c
            } else if (g_activeFormat == 0x88740000) {
                // v3.5: every pass, not throttled, and before the unpack
                // function is picked below so this frame gets the right one
                detectPq8bitMisregistration(liveBufferAddr);
            }
            PixelUnpackFn unpack = getUnpackFnForFormat(g_activeFormat);
            if (unpack != NULL) { // re-check -- format could have gone unknown since the last read
                // v2.9: throttled internally (autoLetterboxCheckIntervalFrames) and a complete no-op when...
                // More: docs/code-comments.md (plugin/source/sample_thread.c:403)
                ambient_check_letterbox(&kParamsBase, liveBufferAddr, unpack);

                uint8_t rgbTriplets[MAX_TOTAL_ZONES * 3];
#if (__FINAL__) == 0
                // FLK1 flicker/flash probe (v3.3): raw pipeline read for the 6 fixed probe zones, captured here...
                // More: docs/code-comments.md (plugin/source/sample_thread.c:412)
                uint8_t probeRaw[6][3];
                uint32_t probeZoneIdx[6];
                for (int pk = 0; pk < 6; pk++) probeZoneIdx[pk] = ((uint32_t)(2 * pk + 1) * g_numZones) / 12u;
#endif
                // Pass 1: sample every zone from the chosen buffer.
                // More: docs/code-comments.md (plugin/source/sample_thread.c:421)
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

                // Pass 2: only zones that moved a lot are re-read, from the same buffer, now that the rest of the pass...
                // More: docs/code-comments.md (plugin/source/sample_thread.c:446)
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

                // Pass 3: color processing, smoothing and output on the verified value.
                // More: docs/code-comments.md (plugin/source/sample_thread.c:478)
                for (uint32_t i = 0; i < g_numZones; i++) {
                    g_acceptedRawRgb[i][0] = s_rawFirst[i][0];
                    g_acceptedRawRgb[i][1] = s_rawFirst[i][1];
                    g_acceptedRawRgb[i][2] = s_rawFirst[i][2];

                    uint8_t verified[3];
                    applyColorProcessing(s_rawFirst[i][0], s_rawFirst[i][1], s_rawFirst[i][2], verified);
                    applyDarkThreshold(i, &verified[0], &verified[1], &verified[2]);

                    if (g_config.smoothingEnabled) {
                        smoothing_step(g_smoothedRgb[i], verified, smoothingAlpha, g_smoothedRgbValid);
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
                // More: docs/code-comments.md (plugin/source/sample_thread.c:506)
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
                        // curIdx: the RAW slot the flip hook reported (displayBufferIndex), not the one this pass actually...
                        // More: docs/code-comments.md (plugin/source/sample_thread.c:543)
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
                // v2.2.4: verified behind __FINAL__==0 (make DEBUG=1), same idiom frame_logger/force_1080p_display...
                // More: docs/code-comments.md (plugin/source/sample_thread.c:607)
                if (++s_rawDiagFrameCounter >= 3) {
                    s_rawDiagFrameCounter = 0;
                    uint32_t diagZones[3] = { 0, g_numZones / 2, g_numZones - 1 };
                    // v2.2.3: entry grew from 12 to 16 bytes -- ADDS the TRUE zone average (calling the real...
                    // More: docs/code-comments.md (plugin/source/sample_thread.c:652)
                    uint8_t diagPacket[4 + 3 * 16];
                    uint32_t formatSnapshot = g_activeFormat; // copy out of the volatile
                                                               // once here so memcpy below gets a plain uint32_t*, not a volatile-qualified one (was a harmless but...
                                                               // More: docs/code-comments.md (plugin/source/sample_thread.c:668)
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
                // v2.8: unpack == NULL -- format went unknown between the outer check and here.
                // More: docs/code-comments.md (plugin/source/sample_thread.c:697)
                wled_send_keepalive_if_stale(t0, tscFreq);
            }
        } else {
            // v2.8: liveBufferAddr == 0 or g_haveValidFormat == 0.
            // More: docs/code-comments.md (plugin/source/sample_thread.c:704)
            wled_send_keepalive_if_stale(t0, tscFreq);
        }
        // With an unknown format, send nothing new rather than guess (the original wrong-colour bug); the heartbeat only
        // repeats an already verified colour so WLED's realtime timeout does not fire. [docs/code-comments.md]

#if TIMING_ENABLED
        // Measured window intentionally covers the buffer-resolve + sampling + color processing + UDP send...
        // More: docs/code-comments.md (plugin/source/sample_thread.c:721)
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

