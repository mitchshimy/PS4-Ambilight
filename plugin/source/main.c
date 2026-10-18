// ps4_ambient_light: the real per-frame pipeline.
// More: docs/code-comments.md (plugin/source/main.c:1)

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


// SPLIT NOTE: this file used to contain the entire pipeline above (~3240 lines, one translation unit).
// More: docs/code-comments.md (plugin/source/main.c:70)

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
#include "plugin_entry_alignment.h"

static __attribute__((noinline, used)) int32_t plugin_load_impl(int32_t argc, const char* argv[])
{
    // Bail out before any hook or thread when running inside the companion app (SHMY00091): hooking its flips crashed
    // it with game tiling math. Keep the id in sync with companion-app/build.bat and CI.yml. [docs/code-comments.md]
    struct proc_info procInfo = {0};
    if (sys_sdk_proc_info(&procInfo) == 0 && strcmp(procInfo.titleid, "SHMY00091") == 0) {
        return 0;
    }
    // v3.8: the title picks the preset (see media_titles.h). Left empty if
    // sys_sdk_proc_info failed, which just means no title matches.
    if (procInfo.titleid[0] != '\0') {
        strncpy(g_titleId, procInfo.titleid, sizeof(g_titleId) - 1);
        g_titleId[sizeof(g_titleId) - 1] = '\0';
    }

    sceKernelMkdir(AMBILIGHT_DATA_DIR, 0777); // the project folder may not exist yet (app never run); an existing one is not an error
    ambient_load_config(); // v2.0: read /data/ps4-ambilight/ps4_ambient_light.ini, or write a default template if absent
    buildZoneGeometry();   // fill g_zoneX/g_zoneY (now config-driven counts/corner/direction/offset/margins)

    int32_t hVideoOut = 0, hGnm = 0;
    sys_dynlib_load_prx("libSceVideoOut.sprx", &hVideoOut);
    sys_dynlib_load_prx("libSceGnmDriver.sprx", &hGnm);
    if (hVideoOut == 0 || hGnm == 0) return 0;

    sys_dynlib_dlsym(hVideoOut, "sceVideoOutRegisterBuffers", (void**)&sceVideoOutRegisterBuffersPtr);
    sys_dynlib_dlsym(hGnm, "sceGnmSubmitAndFlipCommandBuffers", (void**)&sceGnmSubmitAndFlipCommandBuffersPtr);
    if (sceVideoOutRegisterBuffersPtr == NULL || sceGnmSubmitAndFlipCommandBuffersPtr == NULL) {
        ambient_notice_hook_failed("the video functions weren't found");
        return 0;
    }

    // v3.1: two more real flip entrypoints -- see hooks.c's g_pluginVersion comment for why.
    // More: docs/code-comments.md (plugin/source/main.c:152)
    sys_dynlib_dlsym(hVideoOut, "sceVideoOutSubmitFlip", (void**)&sceVideoOutSubmitFlipPtr);
    g_submitFlipPtrResolved = (sceVideoOutSubmitFlipPtr != NULL) ? 1 : 0;
    sys_dynlib_dlsym(hGnm, "sceGnmSubmitAndFlipCommandBuffersForWorkload",
                      (void**)&sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr);
    g_gnmForWorkloadPtrResolved = (sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr != NULL) ? 1 : 0;

    // v2.4: resolved separately from the two required loads above, and deliberately NOT fatal if it fails...
    // More: docs/code-comments.md (plugin/source/main.c:168)
    int32_t hSystemService = 0;
    sys_dynlib_load_prx("libSceSystemService.sprx", &hSystemService);
    if (hSystemService != 0) {
        sys_dynlib_dlsym(hSystemService, "sceSystemServiceGetStatus", (void**)&sceSystemServiceGetStatusPtr);
    }

    // v3.9: resolve sceKernelVirtualQuery for the buffer readability guard (buffer_guard.c).
    // More: docs/code-comments.md (plugin/source/main.c:183)
    {
        int32_t hKernel = 0;
        sys_dynlib_load_prx("libkernel.sprx", &hKernel);
        if (hKernel != 0) {
            sys_dynlib_dlsym(hKernel, "sceKernelVirtualQuery", (void**)&sceKernelVirtualQueryPtr);
            // v3.9: ways to make a GPU-only buffer CPU-readable (buffer_guard.c). All optional.
            sys_dynlib_dlsym(hKernel, "sceKernelMapDirectMemory2", (void**)&sceKernelMapDirectMemory2Ptr);
            sys_dynlib_dlsym(hKernel, "sceKernelMapDirectMemory", (void**)&sceKernelMapDirectMemoryPtr);
            sys_dynlib_dlsym(hKernel, "sceKernelMunmap", (void**)&sceKernelMunmapPtr);
            sys_dynlib_dlsym(hKernel, "sceKernelMprotect", (void**)&sceKernelMprotectPtr); // v3.9
        }
        g_guardAvailable = (sceKernelVirtualQueryPtr != NULL) ? 1 : 0;
    }

    // Forwarding-stub guard: proven necessary, do not skip. The two optional hook pointers are guarded only if resolved.
    // More: docs/code-comments.md (plugin/source/main.c:200)
    if (*((uint8_t*)sceVideoOutRegisterBuffersPtr) == 0xE9 ||
        *((uint8_t*)sceGnmSubmitAndFlipCommandBuffersPtr) == 0xE9 ||
        (sceVideoOutSubmitFlipPtr != NULL && *((uint8_t*)sceVideoOutSubmitFlipPtr) == 0xE9) ||
        (sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr != NULL &&
         *((uint8_t*)sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr) == 0xE9)) {
        ambient_notice_hook_failed("the video functions can't be hooked on this firmware");
        return 0;
    }

    HOOK32(sceVideoOutRegisterBuffersPtr);
    HOOK32(sceGnmSubmitAndFlipCommandBuffersPtr);
    // v3.1: best-effort -- only hook whichever of these actually resolved.
    if (sceVideoOutSubmitFlipPtr != NULL) {
        HOOK32(sceVideoOutSubmitFlipPtr);
    }
    if (sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr != NULL) {
        HOOK32(sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr);
    }

    // Notifications for what the load found (ini, WLED address, update, debug logging). Here, after
    // the early returns above, so a process the plugin never actually hooks stays quiet.
    ambient_notices_on_load();

    OrbisPthread thread;

    // --- Resource-isolation status: MEASURE, not guessed --- Confirmed real, from the same...
    // More: docs/code-comments.md (plugin/source/main.c:230)
#if TIMING_ENABLED
    // Leave thread affinity and priority unset until the CPU mask is measured: an unverified mask is a real risk.
    // More: docs/code-comments.md (plugin/source/main.c:265)
#endif

    // The live-reload file reads run on their own thread, started first so the sampler already knows it exists. If it
    // cannot start, the sampler falls back to checking inline. [docs/debugging/filesystem-stalls.md]
    OrbisPthread configThread;
    if (scePthreadCreate(&configThread, NULL, ambient_config_thread, NULL, "ambient_config_thread") == 0)
        g_cfgWatcherRunning = 1;

    scePthreadCreate(&thread, NULL, ambient_sample_thread, NULL, "ambient_sample_thread");
    // v3.9.1: the report file writer (report.c). Its own thread so a disk write never lands in
    // the sampler's 33 ms budget. Always started; it does nothing unless [compat] report_file=1.
    OrbisPthread reportThread;
    scePthreadCreate(&reportThread, NULL, ambient_report_thread, NULL, "ambient_report_thread");
    // v3.9.2: the cross-process state file (state.c), so a second, independent process -- a...
    // More: docs/code-comments.md (plugin/source/main.c:279)
    OrbisPthread stateThread;
    scePthreadCreate(&stateThread, NULL, ambient_state_thread, NULL, "ambient_state_thread");
    // scePthreadAttrSetaffinity(&attr, CORE_MASK) + scePthreadCreate(&thread, &attr, ...) is the confirmed...
    // More: docs/code-comments.md (plugin/source/main.c:286)

    // Tell the relay "on" only after a load that got all the way through, and only if relay_signal_enabled;
    // an early return means this plugin never drives the strip and nothing would send the matching "off". [docs/code-comments.md]
    if (g_config.relaySignalEnabled) {
        relay_send_external_source(true);
    }

    return 0;
}

// Entry thunk repairs RSP before any C runs: GoldHEN can enter with RSP 8 off 16-byte alignment, which crashed
// vfprintf (SIGBUS on MOVAPS, CUSA15090). Do not call the implementation directly. [docs/code-comments.md]
AMBIENT_DEFINE_ALIGNED_ENTRY(plugin_load, plugin_load_impl)

static __attribute__((noinline, used)) int32_t plugin_unload_impl(int32_t argc, const char* argv[])
{
    // Release the relay first in unload, even if load returned early, but only if relay_signal_enabled;
    // relay_release_external_source() skips the "off" while the shell feed is running. [docs/code-comments.md]
    if (g_config.relaySignalEnabled) {
        relay_release_external_source();
    }

    UNHOOK(sceVideoOutRegisterBuffersPtr);
    UNHOOK(sceGnmSubmitAndFlipCommandBuffersPtr);
    // v3.1: only unhook whichever of these was actually hooked above.
    if (sceVideoOutSubmitFlipPtr != NULL) {
        UNHOOK(sceVideoOutSubmitFlipPtr);
    }
    if (sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr != NULL) {
        UNHOOK(sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr);
    }
    if (g_wledSockfd >= 0) { close(g_wledSockfd); g_wledSockfd = -1; }
    return 0;
}

// Keep teardown under the same explicit ABI contract as plugin_load: the loader is outside this C...
// More: docs/code-comments.md (plugin/source/main.c:354)
AMBIENT_DEFINE_ALIGNED_ENTRY(plugin_unload, plugin_unload_impl)

s32 attr_module_hidden module_start(s64 argc, const void *args) { return 0; }
s32 attr_module_hidden module_stop(s64 argc, const void *args) { return 0; }
