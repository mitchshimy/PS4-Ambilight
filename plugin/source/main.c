// ps4_ambient_light: the real per-frame pipeline (handoff §1 / §21 step
// 4). Everything here is proven infrastructure lifted directly from
// detile_verify_probe.prx (v2.2) -- the hooks, the buffer/format
// tracking, and the tiling math are unchanged from what was verified
// against a real screenshot this session. This file's only new job is
// to run that pipeline every frame instead of once on a button press,
// and turn the result into a live WLED signal instead of a debug dump.
//
// SCOPE (v2.0 -- see handoff §43 for the settings-system addition;
// v1.1/§26/§27 below still describes the worker-thread architecture,
// unchanged by v2.0):
//   - Samples a fixed set of 229 screen zones per frame (not a full
//     frame detile), matching the real measured LED strip layout
//     (42/73/41/73, see buildZoneGeometry below) using ONLY the
//     confirmed-correct base tiling params (handoff §22). The Neo path
//     is dropped here; it served its purpose settling that question in
//     the probe and has no reason to run in the hot path.
//   - Pixel format is read live from sceVideoOutRegisterBuffers, not
//     assumed at compile time -- this is the actual bug an earlier
//     session found (A2R10G10B10 math applied to A8R8G8B8 data). If an
//     unknown format shows up, this plugin explicitly stops sending
//     color rather than guessing.
//   - v1.1: the actual zone-sampling + UDP send no longer runs inside
//     the flip hook. It's been moved to a dedicated worker thread
//     (ambient_sample_thread, same pattern as detile_verify_probe's
//     pad_poll_thread) so a ~2000-sample pass can never stall the
//     game's render/submission thread. The flip hook now only records
//     displayBufferIndex -- see the thread's own comment block for the
//     staleness tradeoff this introduces.
//   - v1.1: sampleZoneAverage now bounds-checks the computed tiled
//     offset against the real padded-buffer size before reading, and
//     skips (treats as black) any sample that would land outside it,
//     instead of trusting the offset formula unconditionally.

//   - v1.2: ambient_sample_thread now measures its own loop time
//     (buffer-resolve + 229-zone sampling + UDP send, i.e. everything
//     except usleep) and reports per-window min/max/avg microseconds
//     plus an over-budget count to DEBUG_IP every ~1s (handoff §30
//     step 2 -- turning "barely noticeable" into an actual number).
//     Gated behind TIMING_ENABLED so it can be compiled out later.
//     Decode with decode_verification_dump.py v3+ (24-byte packets).

// STILL OPEN (do not treat this as fully validated -- see handoff):
//   - §25 (handoff v6) point-1 static-read question: RESOLVED, not a
//     bug -- see handoff §28. Left here only so this comment block
//     doesn't repeat the exact staleness mistake §26 called out.
//   - The worker-thread decoupling in v1.1 has not yet been measured
//     on real hardware for actual frame-time impact of the render-
//     thread-side work that remains (a couple of volatile writes per
//     flip). The v1.2 telemetry below measures the WORKER thread's own
//     loop time, which answers "is 30Hz sampling fast enough to keep
//     up" -- it does NOT measure render-thread-side cost, which is a
//     separate, still-open question if zone count or sample radius
//     increases later.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> // v2.1.5: malloc/free, for ambient_get_real_config_size_and_hash's bounded read buffer -- not needed anywhere else in this TU before now
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // struct timeval, for the SO_SNDTIMEO socket hardening below
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h> // no longer read directly as of v2.1.4 (ambient_check_config_reload's size signal now comes from sceKernelOpen/Lseek, not stat()) -- left included, harmless, in case anything else in this TU ever needs it

#include <orbis/libkernel.h>
#include <orbis/_types/video.h>

#include "plugin_common.h"
#include "config.h"


// SPLIT NOTE: this file used to contain the entire pipeline above
// (~3240 lines, one translation unit). It's now split into
// gamma.c, settings.c, network.c, tiling.c, pixel_formats.c, zones.c,
// color_processing.c, hooks.c, and sample_thread.c -- purely a file
// reorganization, no logic changed. main.c now only owns plugin
// lifecycle (plugin_load/plugin_unload/module_start/module_stop),
// which is what actually has to live at a fixed, GoldHEN-visible
// entry point. See ambient_internal.h for the shared types/externs
// tying the split files together, and each file's own top comment
// for what moved there.

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

#include <orbis/libkernel.h>
#include <orbis/_types/video.h>

#include "plugin_common.h"
#include "ambient_internal.h"

int32_t attr_public plugin_load(int32_t argc, const char* argv[])
{
    ambient_load_config(); // v2.0: read /data/ps4_ambient_light.ini, or write a default template if absent
    buildZoneGeometry();   // fill g_zoneX/g_zoneY (now config-driven counts/corner/direction/offset/margins)

    int32_t hVideoOut = 0, hGnm = 0;
    sys_dynlib_load_prx("libSceVideoOut.sprx", &hVideoOut);
    sys_dynlib_load_prx("libSceGnmDriver.sprx", &hGnm);
    if (hVideoOut == 0 || hGnm == 0) return 0;

    sys_dynlib_dlsym(hVideoOut, "sceVideoOutRegisterBuffers", (void**)&sceVideoOutRegisterBuffersPtr);
    sys_dynlib_dlsym(hGnm, "sceGnmSubmitAndFlipCommandBuffers", (void**)&sceGnmSubmitAndFlipCommandBuffersPtr);
    if (sceVideoOutRegisterBuffersPtr == NULL || sceGnmSubmitAndFlipCommandBuffersPtr == NULL) return 0;

    // v2.4: resolved separately from the two required loads above, and
    // deliberately NOT fatal if it fails -- foreground/background
    // detection is additive. If this symbol can't be resolved (wrong
    // firmware, sprx renamed, etc.), ambient_sample_thread's own check
    // below just no-ops (sceSystemServiceGetStatusPtr stays NULL) and
    // this plugin falls back to its pre-v2.4 behavior: freezes the
    // strip on the last frame when suspended, per the known, pre-
    // existing handoff §36 gap -- not a new regression, just not yet
    // fixed on whatever system rejected this symbol.
    int32_t hSystemService = 0;
    sys_dynlib_load_prx("libSceSystemService.sprx", &hSystemService);
    if (hSystemService != 0) {
        sys_dynlib_dlsym(hSystemService, "sceSystemServiceGetStatus", (void**)&sceSystemServiceGetStatusPtr);
    }

    // Same forwarding-stub guard as detile_verify_probe -- proven
    // necessary, do not skip.
    if (*((uint8_t*)sceVideoOutRegisterBuffersPtr) == 0xE9 ||
        *((uint8_t*)sceGnmSubmitAndFlipCommandBuffersPtr) == 0xE9) {
        return 0;
    }

    HOOK32(sceVideoOutRegisterBuffersPtr);
    HOOK32(sceGnmSubmitAndFlipCommandBuffersPtr);

    OrbisPthread thread;

    // --- Resource-isolation status: MEASURE, not guessed (see handoff) ---
    // Confirmed real, from the same <orbis/libkernel.h> this file already
    // includes:
    //   scePthreadAttrInit(OrbisPthreadAttr*)
    //   scePthreadAttrSetaffinity(OrbisPthreadAttr*, uint64_t mask)  -- must
    //     be set on the attr BEFORE scePthreadCreate; the post-create
    //     scePthreadSetaffinity() is declared with NO arguments in this
    //     header (unresolved stub), so it is not a usable path.
    //   scePthreadSetprio(OrbisPthread, int)                        -- set
    //     AFTER create, on the returned handle.
    //   sceKernelGetCurrentCpu(void)                                -- returns
    //     the core index the CALLING thread is on right now.
    //
    // NOT yet known, and not fabricated here:
    //   1) The uint64_t mask's bit-to-core mapping. The toolchain header's
    //      own comment on this line says the real type ("OrbisKernelCpumask")
    //      still needs porting -- i.e. even upstream hasn't confirmed
    //      bit N == core N for this platform. Assumed elsewhere on other
    //      platforms, not established here.
    //   2) Which PS4 core(s) game threads actually run on / avoid. No public
    //      source found documenting a game-vs-system core split for PS4,
    //      the way e.g. Xbox 360 docs spell out reserved hardware threads.
    //   3) scePthreadSetprio's number scale (does lower mean higher priority
    //      here, and by how much) -- signature confirmed, semantics not.
    //
    // TIMING_ENABLED (already 1 by default) now also samples
    // sceKernelGetCurrentCpu() once per telemetry window, so you can see,
    // empirically, which core this thread actually lands on across a real
    // session -- the same "measure it for real" approach as the loop-time
    // work in handoff §31. Once you've also logged sceKernelGetCurrentCpu()
    // from a thread you know is the game's own render/submission path (or
    // found another reliable way to identify it), fill in CORE_MASK below to
    // exclude that core, and verify with the same telemetry that the switch
    // actually took effect, before trusting it.
#if TIMING_ENABLED
    // Leave unset until (1) and (2) above are resolved. Setting affinity
    // with an unverified mask is a real risk here, not just a style
    // preference -- verified-wrong is one of this project's exact
    // recurring bug patterns (§7/§22/§38's format-mismatch bugs, §29's
    // scePadOpen frequency bug), all traced back to this same category of
    // "looked plausible, wasn't checked."
    // #define CORE_MASK (1ULL << N)   // fill in N once measured, not before
#endif

    scePthreadCreate(&thread, NULL, ambient_sample_thread, NULL, "ambient_sample_thread");
    // scePthreadAttrSetaffinity(&attr, CORE_MASK) + scePthreadCreate(&thread,
    // &attr, ...) is the confirmed path once CORE_MASK is real. Same for
    // scePthreadSetprio(thread, N) once N's semantics are confirmed against
    // scePthreadGetprio()'s (also confirmed) default reading.

    // v2.3: only sent here, at the very end of a load that actually got
    // this far (hooks installed, sample thread created) -- every early
    // `return 0` above means this plugin never actually starts driving
    // the strip, so telling the relay "on" from one of those paths
    // would be a lie it has no way to detect or recover from on its
    // own (nothing would ever send the matching "off").
    // v2.6: explicit guard added now that relay_send_external_source()
    // no longer gates itself internally -- a user who never opted in
    // must not get a real send here.
    if (g_config.relaySignalEnabled) {
        relay_send_external_source(true);
    }

    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char* argv[])
{
    // v2.3: sent first and unconditionally, before any of the teardown
    // below -- this must fire even if plugin_load returned early above
    // and never actually hooked anything, since the relay's own state
    // has no other way to notice this plugin is gone. Harmless if the
    // matching "on" was never sent (tv_state.external_source_active is
    // idempotent either way -- see tv_external_source.handle_payload).
    // v2.6: explicit guard added now that relay_send_external_source()
    // no longer gates itself internally. Checking the CURRENT value of
    // relaySignalEnabled here is deliberately fine even if it differs
    // from whatever it was when "on" was last sent: if true now,
    // sending "off" is correct and harmless whether or not an "on"
    // actually preceded it this session (idempotent on the relay
    // side). If false now, either it was never enabled this session
    // (no real "on" was ever sent, so none is owed back either), or it
    // WAS enabled and got disabled via a live reload -- which already
    // sent its own "off" at the moment it changed (see
    // ambient_check_config_reload's flip-handler), making a second
    // one here redundant, not missing.
    if (g_config.relaySignalEnabled) {
        relay_send_external_source(false);
    }

    UNHOOK(sceVideoOutRegisterBuffersPtr);
    UNHOOK(sceGnmSubmitAndFlipCommandBuffersPtr);
    if (g_wledSockfd >= 0) { close(g_wledSockfd); g_wledSockfd = -1; }
    return 0;
}

s32 attr_module_hidden module_start(s64 argc, const void *args) { return 0; }
s32 attr_module_hidden module_stop(s64 argc, const void *args) { return 0; }