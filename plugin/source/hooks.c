// hooks.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. Plugin metadata + the two real GoldHEN hooks (buffer registration, flip).
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

// --- Plugin metadata ---
attr_public const char *g_pluginName = "ps4_ambient_light";
attr_public const char *g_pluginDesc = "Live per-frame ambient light: detiles the real scanout buffer and streams zone colors to WLED";
attr_public const char *g_pluginAuth = "(null)";
attr_public uint32_t g_pluginVersion = 0x00000212; // v2.7.1 -> v2.7.2:
// CONFIRMED, no longer diagnostic-only. The v2.7.1 dev-telemetry
// (still present, still __FINAL__==0-gated -- see detectHdr2200Format's
// own comment) showed the live detection working exactly as the
// offline replica predicted: on the session that originally looked
// broken, sdrTv/hdrTv/streak/isHdr streamed back showing a clean
// accumulate-then-flip at the correct moment (streak 1->2->3, then
// isHdr 0->1 with streak reset), then stayed locked on HDR with no
// reversal. The earlier "doesn't settle" report turned out to be a
// patience issue, not a bug: the flip took ~43s from boot (a black
// loading screen produces no valid checks at all, then several more
// seconds accumulating a real streak) -- watching for less than that
// looks identical to "broken."
//
// Also tested the specific worry raised after that: would a dark
// gameplay scene flip it back to SDR incorrectly? Captured a real
// ~6.5-minute gameplay session (not menus) and decoded all 53
// diagnostic packets -- exactly ONE flip in the whole session, the
// correct initial SDR->HDR one. The HDR/SDR margin narrowed
// substantially in darker/busier scenes (as low as ~2.3x apart,
// versus 15-40x on simple menu content) but never came close to
// reversing, let alone sustaining 4 consecutive reversed checks.
// Not proof for every possible scene, but a real, varied session
// with no failure is real evidence, not just theory.
//
// v2.7 -> v2.7.1 (mechanism unchanged, kept for the full trail): added
// live HDR-vs-SDR auto-detection for format 0x80002200 specifically --
// this format ID is confirmed AMBIGUOUS on this title (see
// unpackA8B8G8R8_to_rgb888's own comment): it means A8B8G8R8_SRGB with
// HDR off, but the SAME registered format ID actually holds
// A2R10G10B10_BT2020_PQ data when HDR is on, with no reliable signal
// available from any hooked API to tell which is currently true --
// hooking sceVideoOutAddBufferHdrPrivilege (the obvious direct signal)
// was investigated and ruled out, since no real reference
// implementation of it exists anywhere (not in this codebase's history,
// not in fpPS4) to safely derive its call signature from, and a wrong
// guess there risks corrupting a call into Sony's own code. Detected
// instead via decode-smoothness: real image content decodes smoothly
// under the correct hypothesis and noisily under the wrong one. Method
// validated offline first (a Python replica in
// decode_verification_dump.py, run blind against real captures, twice)
// before being ported here. Kept deliberately cheap, per explicit
// instruction not to have this plugin compete with the game for CPU:
// reuses zone coordinates already computed for real LED sampling, caps
// samples at HDR2200_DETECT_SAMPLES, throttled to once every
// HDR2200_DETECT_INTERVAL sampling passes, with a hysteresis streak
// (HDR2200_STREAK_THRESHOLD) and a noise floor (HDR2200_NOISE_FLOOR)
// so a single noisy or degenerate frame can't flip the live decision.
//
// v2.6 -> v2.7 (still true): added
// real support for format 0x80002200 (A8B8G8R8_SRGB) -- this format was
// identified by name several sessions ago (against fpPS4's enum table,
// on a different title: HITMAN 3) but was NEVER actually wired into
// getUnpackFnForFormat; a game reporting this format got NULL back,
// same as any other unrecognized format, and this plugin correctly
// sent nothing rather than guess (see g_haveValidFormat). Root-caused
// via detile_verify_probe (a standalone diagnostic plugin, not this
// one) across a long investigation: pad-conflict trigger issues, then
// three addressing hypotheses (tiled math, naive linear math, wrong
// swap-chain slot) that all decoded plausible-but-wrong colors against
// a real screenshot, then a clean flip-rate and stable videoOutHandle
// that ruled out "wrong render pass" -- until the user independently
// tested turning HDR off, which is what actually fixed it. With HDR
// off, the SAME format ID's buffer content decoded correctly with
// simple tiled addressing, once R and B are swapped relative to this
// file's existing unpackA8R8G8B8_to_rgb888. Confirmed two ways: a
// probe capture checked pixel-for-pixel against a real screenshot at 5
// known coordinates (single-digit-to-teens RGB error at all 5 at
// once, the first hypothesis in the whole investigation to do that),
// and then the user confirming colors look correct live, in this
// plugin, on real hardware. See unpackA8B8G8R8_to_rgb888's own comment
// below, and this repo's handoff for the full investigation trail
// (kept as a separate document rather than squeezed into this file's
// existing §-numbered handoff, whose numbering already collides across
// forks -- see the handoff's own note on why).

int32_t (*sceVideoOutRegisterBuffersPtr)(int32_t handle, int32_t startIndex,
                                          void *const *addresses, int32_t bufferNum,
                                          const OrbisVideoOutBufferAttribute *attribute);
int32_t (*sceGnmSubmitAndFlipCommandBuffersPtr)(uint32_t count, void *dcbGpuAddrs[],
                                                 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                 uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                 uint32_t displayBufferIndex, uint32_t flipMode,
                                                 int64_t flipArg);

HOOK_INIT(sceVideoOutRegisterBuffersPtr);
HOOK_INIT(sceGnmSubmitAndFlipCommandBuffersPtr);

#define MAX_TRACKED_BUFFERS 16
volatile uint64_t g_bufferAddrs[MAX_TRACKED_BUFFERS] = {0};
volatile int32_t  g_bufferCount = 0;
volatile uint32_t g_activeFormat = 0;       // live format, NOT hardcoded -- the actual fix from this session
volatile int      g_haveValidFormat = 0;    // 0 until a registration event gives us a known format

int32_t sceVideoOutRegisterBuffersPtr_hook(int32_t handle, int32_t startIndex,
                                            void *const *addresses, int32_t bufferNum,
                                            const OrbisVideoOutBufferAttribute *attribute)
{
    if (addresses != NULL && bufferNum > 0) {
        int32_t n = bufferNum > MAX_TRACKED_BUFFERS ? MAX_TRACKED_BUFFERS : bufferNum;
        for (int32_t i = 0; i < n; i++) {
            int32_t slot = startIndex + i;
            if (slot >= 0 && slot < MAX_TRACKED_BUFFERS) {
                g_bufferAddrs[slot] = (uint64_t)addresses[i];
            }
        }
        if (n > g_bufferCount) g_bufferCount = n;

        if (attribute != NULL) {
            uint32_t fmt = (uint32_t)attribute->format;
            bool wasValid = g_haveValidFormat;
            uint32_t prevFormat = g_activeFormat;
            g_activeFormat = fmt;
            g_haveValidFormat = (getUnpackFnForFormat(fmt) != NULL);
#if (__FINAL__) == 0
            // v2.2.1: diagnostic-only telemetry, added to answer a real
            // live question (does this specific game's HDR mode register
            // a format this plugin actually recognizes?) rather than
            // guess. Reuses the existing debug_send_raw/DEBUG_IP path
            // (same one send_timing_packet uses), a NEW 8-byte payload so
            // it dispatches distinctly by length in any listener that
            // already keys off len(data) (this project's own convention
            // -- see send_timing_packet's comment above). Only fires when
            // the format actually CHANGES, not on every re-registration
            // (this hook already fires repeatedly during normal play per
            // the comment this replaces), so it won't spam a listener
            // during a real session -- only right when HDR toggles on/off
            // or a title switches formats.
            //   [0:4] format     -- raw attribute->format, uint32 LE
            //   [4:8] wasRecognized -- 0/1, uint32 LE (matches one of the
            //                          getUnpackFnForFormat cases or not)
            // v2.2.4: gated behind __FINAL__==0 (make DEBUG=1), same
            // idiom frame_logger/force_1080p_display already use in this
            // repo. This unconditionally sent a UDP packet to a
            // hardcoded DEBUG_IP for every single end user on every real
            // release build, which is not something a shipped plugin
            // should do -- see handoff.
            if (fmt != prevFormat || g_haveValidFormat != wasValid) {
                uint8_t fmtPacket[8];
                memcpy(fmtPacket + 0, &fmt, 4);
                uint32_t recognized = g_haveValidFormat ? 1u : 0u;
                memcpy(fmtPacket + 4, &recognized, 4);
                debug_send_raw(fmtPacket, sizeof(fmtPacket));
            }
#else
            (void)wasValid; (void)prevFormat; // avoid unused-variable warnings in release builds
#endif
        }
    }
    return HOOK_CONTINUE(sceVideoOutRegisterBuffersPtr,
                          int32_t(*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *),
                          handle, startIndex, addresses, bufferNum, attribute);
}

// v1.1: the flip hook itself only records which buffer is live now --
// no sampling, no network I/O, no loop. This is the actual fix for
// handoff §26 finding 1. g_currentDisplayBufferIndex is read by the
// worker thread below at its own pace.
volatile uint32_t g_currentDisplayBufferIndex = 0xFFFFFFFFu; // sentinel: no flip seen yet

int32_t sceGnmSubmitAndFlipCommandBuffersPtr_hook(uint32_t count, void *dcbGpuAddrs[],
                                                   uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                   uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                   uint32_t displayBufferIndex, uint32_t flipMode,
                                                   int64_t flipArg)
{
    g_currentDisplayBufferIndex = displayBufferIndex; // single volatile write, near-zero cost

    return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffersPtr,
                          int32_t(*)(uint32_t, void **, uint32_t *, void **, uint32_t *, uint32_t, uint32_t, uint32_t, int64_t),
                          count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes,
                          videoOutHandle, displayBufferIndex, flipMode, flipArg);
}

