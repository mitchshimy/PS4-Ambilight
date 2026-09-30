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
attr_public uint32_t g_pluginVersion = 0x00000309; // v3.8 -> v3.9:
// The sampler asks the kernel whether it may read a display buffer before it
// touches it, and makes a GPU-only one (Mortal Kombat 11) CPU-readable instead
// of crashing the game -- see buffer_guard.c and
// docs/debugging/gpu-only-buffers.md.
// v3.7 -> v3.8:
// Game and Movie presets. The ini holds two complete presets and the plugin
// reads the one [presets] active= names, or Movie for a title in
// media_titles.h. An ini from before v3.8 reads as it always did -- see
// preset_select.h and settings.c.
// v3.6 -> v3.7:
// AMBIENT_SAMPLE_LAG defaults to 2 (two flips back), which needs a second
// tracked slot, g_prevPrevDisplayBufferIndex below. The sampler no longer
// reads the hook-reported slot while that history fills in, and re-reads
// zones that jump to catch a buffer that changes mid-read -- see
// sample_thread.c and docs/debugging/sample-lag-and-boot-flash.md.
// v3.5 -> v3.6:
// Format 0x80002200 (HITMAN 3, RDR2) is decided per frame from the alpha byte
// of 8 zone words, before the older smoothness check gets a say -- see
// detectHdr2200Fast in zones.c, hdr2200_vote.h and
// docs/debugging/hdr2200-alpha-detection.md.
// v3.4 -> v3.5:
// Format 0x88740000 is decoded as 8-bit A8R8G8B8 on the frames where the
// buffer holds that (YouTube playing SDR video with HDR on), decided every
// pass from the alpha byte -- see detectPq8bitMisregistration in zones.c and
// docs/debugging/youtube-hdr-8bit.md.
// v3.3 -> v3.4:
// Letterbox zones placed at the measured bar depth instead of the
// quantized (rounded-down) one, and only when the axis's two edges
// agree -- see docs/debugging/letterbox-placement.md.
// v3.2 -> v3.3:
// Sampler reads the previous flip's buffer instead of the one the flip
// hook just reported (g_prevDisplayBufferIndex, record_flip_index below),
// fixing flicker from sampling a cleared / half-drawn frame. See
// CHANGELOG.md v3.3 for the capture evidence.
// v3.1 -> v3.2:
// The v3.1 flip hooks above fire in EVERY process GoldHEN injects this
// plugin into -- including this project's own companion app, which is
// just another titleid (SHMY00091) as far as GoldHEN is concerned. Two
// consequences, both real: the companion app's own screen flips got
// captured and streamed to the strip as if it were a game, conflicting
// with whatever real testing/adjustment was already running; and worse,
// ambient_sample_thread then applied this file's game-buffer tiling math
// to the companion app's differently-shaped surface, producing a wild
// address and a real SIGSEGV (crash report: thread
// "ambient_sample_thread", proc "eboot.bin", AppName "PS4 Ambilight",
// TitleID "SHMY00091", page fault on a read from an address matching
// neither this title's real GNM buffer addresses seen in other
// captures). Fixed at the top of plugin_load (main.c), before any
// dlsym/hook/thread work: read procInfo.titleid via sys_sdk_proc_info()
// and bail out entirely if it's SHMY00091. One check fixes both the
// crash and the color conflict, since neither can happen if no hook is
// ever installed in that process to begin with.
//
// v3.0 -> v3.1:
// A title (Shadow of the Tomb Raider, both its SDR and HDR display
// modes -- confirmed the same title, not two separate ones, after an
// earlier capture session got that wrong) never lit up the strip at
// all. Buffer registration looked completely normal --
// sceVideoOutRegisterBuffersPtr_hook fired once, format came back
// recognized both times (0x80000000 A8R8G8B8_SRGB on the SDR run,
// 0x88740000 A2R10G10B10_BT2020_PQ on the HDR run) -- so this wasn't
// another entry for the pixel-format table.
//
// Added real telemetry instead of guessing further:
// g_registerHookCallCount / g_flipHookCallCount (below), sent once a
// second via a new send_flip_diag_packet (network.c), __FINAL__==0-
// gated same as every other diagnostic packet in this file. Captured
// against the failing title: register climbing normally, flip stuck
// at 0 for the entire session, g_currentDisplayBufferIndex never
// leaving its 0xFFFFFFFF sentinel. sceGnmSubmitAndFlipCommandBuffers
// was simply never being called by this title -- not a timing/
// throttle artifact, an unconditional per-second counter that never
// once incremented across an ~11-second capture.
//
// First hypothesis: a second, real flip entrypoint,
// sceVideoOutSubmitFlip (libSceVideoOut) -- confirmed to exist with a
// real, separate NID via shadPS4's independently reverse-engineered
// video_out.cpp (fetched and read directly, not recited from memory,
// same standard of evidence already used for
// sceSystemServiceGetStatus above). Added as a second hook,
// resolved/hooked non-fatally (a title with no use for it must not
// lose the rest of this plugin). Recaptured: resolved fine
// (submit_flip_ptr_resolved=1 in the extended diagnostic packet,
// disambiguating "never resolved" from "resolved but never called"),
// call count still stuck at 0. Wrong hypothesis, but kept hooked --
// harmless, and some other title may yet use it.
//
// Real cause, again found in shadPS4 source rather than guessed:
// gnmdriver.cpp shows sceGnmSubmitAndFlipCommandBuffers (the hook
// this file already had) is itself just a thin wrapper --
//   return sceGnmSubmitAndFlipCommandBuffersForWorkload(count, count, ...);
// -- around a SEPARATELY exported symbol with its own distinct NID. A
// title submitting explicit multi-workload GPU work can call the
// ForWorkload entrypoint directly, skipping the wrapper this plugin
// hooked, entirely. Added as a third hook, same non-fatal pattern.
//
// CONFIRMED on real hardware: gnm_for_workload_hook_call_count
// climbing (0 -> 1 -> 30 -> 95...) over a live capture,
// g_currentDisplayBufferIndex off its sentinel and alternating
// between the title's two swap-chain slots, g_bufferAddrs resolving
// to real addresses, and the existing __FINAL__==0 raw-pixel
// diagnostic firing at its normal ~10x/sec cadence. Strip lit up.
//
// While in here: g_pluginVersion had been sitting at the v2.7.2
// comment above since that release, unbumped through v2.7.3-v3.0.
// Catching it up now that there's a real reason to touch this file
// again -- not attempting to backfill the missing intermediate
// comment entries, see CHANGELOG.md for those.
//
// v2.7.1 -> v2.7.2:
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
// below, and this repo's for the full investigation trail
// (kept as a separate document rather than squeezed into this file's
// existing section-numbered project notes, whose numbering already collides across
// forks.

int32_t (*sceVideoOutRegisterBuffersPtr)(int32_t handle, int32_t startIndex,
                                          void *const *addresses, int32_t bufferNum,
                                          const OrbisVideoOutBufferAttribute *attribute);
int32_t (*sceGnmSubmitAndFlipCommandBuffersPtr)(uint32_t count, void *dcbGpuAddrs[],
                                                 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                 uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                 uint32_t displayBufferIndex, uint32_t flipMode,
                                                 int64_t flipArg);
// v3.1: second real flip entrypoint. Real signature, confirmed against
// shadPS4's video_out.cpp, not guessed -- see the g_pluginVersion
// comment above for why this exists. Resolved and hooked non-fatally
// (main.c) -- a title with no use for it is unaffected either way.
int32_t (*sceVideoOutSubmitFlipPtr)(int32_t handle, int32_t bufferIndex,
                                     int32_t flipMode, int64_t flipArg);
// v3.1: third real flip entrypoint, and the one that actually turned
// out to matter -- see the g_pluginVersion comment above. Confirmed
// against shadPS4's gnmdriver.cpp: the plain sceGnmSubmitAndFlipCommand-
// Buffers hook above is itself just a wrapper around this, separately
// exported symbol. Same non-fatal resolve/hook pattern.
int32_t (*sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr)(uint32_t workload, uint32_t count,
                                                             void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes,
                                                             void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
                                                             uint32_t videoOutHandle, uint32_t displayBufferIndex,
                                                             uint32_t flipMode, int64_t flipArg);

HOOK_INIT(sceVideoOutRegisterBuffersPtr);
HOOK_INIT(sceGnmSubmitAndFlipCommandBuffersPtr);
HOOK_INIT(sceVideoOutSubmitFlipPtr);
HOOK_INIT(sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr);

#define MAX_TRACKED_BUFFERS 16
volatile uint64_t g_bufferAddrs[MAX_TRACKED_BUFFERS] = {0};
volatile int32_t  g_bufferCount = 0;
volatile uint32_t g_activeFormat = 0; // live format, NOT hardcoded
volatile int      g_haveValidFormat = 0;    // 0 until a registration event gives us a known format

// v3.1: diagnostic only, no pipeline effect -- what actually found the
// "never lights up" bug. Sent via send_flip_diag_packet (network.c),
// __FINAL__==0-gated same as every other packet in this file; see
// the g_pluginVersion comment above for the investigation these came
// out of. Plain volatile counters, same cost class as
// g_currentDisplayBufferIndex's own single write per hook call.
volatile uint32_t g_registerHookCallCount = 0;
volatile uint32_t g_flipHookCallCount = 0;
volatile uint32_t g_videoOutSubmitFlipHookCallCount = 0;
volatile uint32_t g_submitFlipPtrResolved = 0;          // set once in main.c, right after that hook's dlsym call
volatile uint32_t g_gnmForWorkloadHookCallCount = 0;
volatile uint32_t g_gnmForWorkloadPtrResolved = 0;      // set once in main.c, right after that hook's dlsym call

int32_t sceVideoOutRegisterBuffersPtr_hook(int32_t handle, int32_t startIndex,
                                            void *const *addresses, int32_t bufferNum,
                                            const OrbisVideoOutBufferAttribute *attribute)
{
    g_registerHookCallCount++; // v3.1: diagnostic only -- see its declaration above
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
            if (fmt != prevFormat) g_pq8bitMode = 0; // v3.5: HDR toggled or a new title, decide again from the pixels
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
            // should do.
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
// finding 1. g_currentDisplayBufferIndex is read by the
// worker thread below at its own pace.
volatile uint32_t g_currentDisplayBufferIndex = 0xFFFFFFFFu; // sentinel: no flip seen yet

// Slot of the flip BEFORE the current one (sentinel until two different
// slots have been seen). The hooks fire at SUBMIT time, so the slot they
// report is the one the GPU is about to render into: it can be cleared or
// half drawn. The previous flip's slot has finished rendering. Only advanced
// when the index actually changes, so the two flip hooks both firing for one
// real flip (same index twice) don't overwrite it.
volatile uint32_t g_prevDisplayBufferIndex = 0xFFFFFFFFu;

// Slot of the flip before THAT one (two real flips back). Same sentinel and
// same only-on-a-real-change rule as g_prevDisplayBufferIndex above. Before
// v3.7 AMBIENT_SAMPLE_LAG was only ever tested as ">= 1", so a build set to 2
// compiled to exactly the same behaviour as 1: nothing tracked a second step
// back. Red Dead Redemption (about 58 fps) still flickered at lag 1, and
// three captures taken at "lag 2" were byte-identical to the lag 1 ones.
// See docs/debugging/sample-lag-and-boot-flash.md.
volatile uint32_t g_prevPrevDisplayBufferIndex = 0xFFFFFFFFu;

static inline void record_flip_index(uint32_t idx)
{
    uint32_t cur = g_currentDisplayBufferIndex;
    if (idx != cur) {
        g_prevPrevDisplayBufferIndex = g_prevDisplayBufferIndex;
        g_prevDisplayBufferIndex = cur; // may be the sentinel on the first flip; the sampler waits for history (see sample_thread.c)
        g_currentDisplayBufferIndex = idx;
    }
}

int32_t sceGnmSubmitAndFlipCommandBuffersPtr_hook(uint32_t count, void *dcbGpuAddrs[],
                                                   uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                   uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                   uint32_t displayBufferIndex, uint32_t flipMode,
                                                   int64_t flipArg)
{
    g_flipHookCallCount++; // v3.1: diagnostic only -- see its declaration above
    record_flip_index(displayBufferIndex); // near-zero cost

    return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffersPtr,
                          int32_t(*)(uint32_t, void **, uint32_t *, void **, uint32_t *, uint32_t, uint32_t, uint32_t, int64_t),
                          count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes,
                          videoOutHandle, displayBufferIndex, flipMode, flipArg);
}

// v3.1: second flip entrypoint -- see its pointer declaration above for
// why this is hooked. Record-only, same discipline as the hook above:
// no sampling or network I/O from inside a hook, just the buffer index
// write the worker thread reads at its own pace.
int32_t sceVideoOutSubmitFlipPtr_hook(int32_t handle, int32_t bufferIndex,
                                       int32_t flipMode, int64_t flipArg)
{
    g_videoOutSubmitFlipHookCallCount++; // diagnostic only -- see its declaration above
    record_flip_index((uint32_t)bufferIndex); // (uint32_t)(-1) == the existing sentinel

    return HOOK_CONTINUE(sceVideoOutSubmitFlipPtr,
                          int32_t(*)(int32_t, int32_t, int32_t, int64_t),
                          handle, bufferIndex, flipMode, flipArg);
}

// v3.1: third flip entrypoint -- the one that actually turned out to
// matter, see its pointer declaration above. Same record-only
// discipline; extra leading `workload` parameter versus the plain
// variant above is the real, confirmed difference in signature.
int32_t sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr_hook(uint32_t workload, uint32_t count,
                                                               void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes,
                                                               void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
                                                               uint32_t videoOutHandle, uint32_t displayBufferIndex,
                                                               uint32_t flipMode, int64_t flipArg)
{
    g_gnmForWorkloadHookCallCount++; // diagnostic only -- see its declaration above
    record_flip_index(displayBufferIndex); // near-zero cost

    return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr,
                          int32_t(*)(uint32_t, uint32_t, void **, uint32_t *, void **, uint32_t *, uint32_t, uint32_t, uint32_t, int64_t),
                          workload, count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes,
                          videoOutHandle, displayBufferIndex, flipMode, flipArg);
}

