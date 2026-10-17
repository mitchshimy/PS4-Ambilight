// shell_probe v24: v23 plus the four Phase 3 leftovers (docs/integration_plan_v2.md): report file, [dev] UDP
// mirror, self-heal, one log line per gate. Nothing about v23's behaviour changes; see this overlay's README.
//  1. REPORT FILE. [compat] report_file=1 -> /data/ps4-ambilight/ps4_ambient_report_SHELL.txt, written by its own thread on
//     start, on a change, and every 60 s. No WLED address, no ini values. Last line "end of report".
//  2. [dev] MIRROR. [dev] dev_ip + dev_logging=true (both, like the sister) -> the 5 s status row and the stop,
//     standby, resume, claim and self-heal events go to dev_ip:4048 as DDP-wrapped text starting "SHL1".
//  3. SELF-HEAL. 30 bad buffer checks in a row (~1 s) -> map_scan(0); find_bufs();. At most one re-scan per
//     10 s, doubling to 60 s while re-scans do not help.
//  4. GATES. One "GATE:" line per compile-time gate at start (an undefined gate macro is now a compile error)
//     and one "GATE (runtime ...)" line whenever the ini changes report_file / [dev].
//
// ---- v23 header, unchanged ----
// shell_probe v23: runs until reboot; disable file and [shell] enabled= switch; no crash-loop guard.
//
// Basis (v22, hardware-verified, docs/2026-10-06_probe-v22_A.md + v22_B.md): ini read at start, live reload
// confirmed (two edits, order 0->1->0, strip visibly rotated). What v22 did not do: run past 300 s, or
// offer a way to silence the feed without a reboot.
//
// WHAT'S NEW (v23)
//  1. RUN UNTIL REBOOT. The 300-second LIVE_SECONDS limit is gone. The send loop runs until the process
//     exits (reboot), or until a clean stop fires (items 2-3 below). The LIVE: header says "until reboot"
//     and the LIVE done line reports elapsed seconds.
//  2. DISABLE FILE. If /data/ps4-ambilight/ps4_shell_feed_off exists (create over FTP or the app) the send loop exits
//     cleanly with "DISABLED: disable file found". Delete the file to silence the feed until next reboot.
//  3. [shell] enabled=false in the ini. Same effect as the disable file, picked up by the live-reload poll
//     with no reboot. Default: true (absent key = enabled). "DISABLED: shell disabled in ini" is logged.
//
// NO CRASH-LOOP GUARD. v21 and v22 wrote /data/shell_probe_vXX_ran on first run and refused to re-run if
// it existed; v23 drops this entirely. The guard never actually triggered during testing (v1-v22) because
// the person always rebooted between sessions -- it only would have fired on a second injection in the same
// boot without a reboot, which never happened. Recovery from a crash loop is the same as any other plugin:
// remove the SPRX or disable AutoRun, exactly as a person would do for any bad GoldHEN plugin. The manual
// delete-to-override step was complexity that worked against the automation goal.
//
// v22 note (kept for context): the shell feed runs on YOUR ini -- the same settings, the same zone geometry,
// the same colour chain and the same [preset_game] the PS4-Ambilight plugin uses for a game.
//
// Basis (v21-A, hardware-verified, docs/2026-10-06_probe-v21_A.md): the claim gate, the 42-LED order fix and
// the fast handoff work. What v21 did not do was follow the ini: its order was a hard-coded rotation, its
// sampling was its own (four points per zone in a band), and its colours were raw. v22 replaces that.
//
// WHAT'S NEW
//  1. INI. /data/ps4-ambilight/ps4_ambient_light.ini is read at start and re-checked every [timing]
//     config_reload_check_seconds (the sister's own 2 s), by size AND content hash read through raw
//     sceKernelOpen/Read (stat() is garbage on this filesystem, the sister's hardware-proven lesson). A change
//     reloads the settings, rebuilds the zone geometry and restarts smoothing, with no reboot.
//  2. PRESET. Settings that a preset owns come from [preset_game], whatever [presets] active= says, because
//     active=movie would otherwise turn the shell into "movie" too. If the ini has no game preset the same
//     fallbacks as the sister apply (the movie preset, then the old flat [layout]/[color]/[timing] keys).
//  3. GEOMETRY. buildZoneGeometry() from the sister: led_count_*, led_start_corner, led_direction and
//     led_offset decide where zone i sits and which LED it is. v21's hard-coded LED_ROT is no longer used by
//     the live send (it only remains for the old scene previews).
//  4. SAMPLING AND COLOUR. sampleZoneAverage() with scan_depth, the per-channel gamma, then
//     applyColorProcessing() (gamma -> brightness -> contrast -> saturation -> levels -> colour order),
//     applyDarkThreshold() and the sister's smoothing (settling_time_ms). update_frequency_hz sets the
//     pace, wled_host/wled_port the destination (the compile-time host is only a fallback).
//
// HOW THE SISTER'S CODE GOT IN. Through v24 (shell_probe's own history, as a research-repo probe) it was
// pasted, not rewritten: every region between a "sister-begin" and a "sister-end" marker was cut out of
// the sister's own sources by function name (PS4-Ambilight 89199fe) by tools/make_probe_v22.py, and
// tools/host_ini_check/run.sh cut the same regions out of that file and compared them to the sister's
// files byte for byte. As of the PS4-Ambilight/ps4-shell-capture-research merge's Stage C, this plugin
// (now living in the sister's own repo, as shell-feed/) instead compiles tiling.c, pixel_formats.c,
// zones.c, gamma.c, color_processing.c, settings.c and config.c straight from ../plugin/source/ -- see
// the comment above the real #include block further down for the rest of that story, and
// shell-feed/README.md for the provenance of the one piece that is still local to this file (the
// probe's own v17 tiling math). Stage D then moved the per-frame smoothing out of both this file and
// sample_thread.c into plugin/include/smoothing.h, so there is no "sister-begin" paste left here.
//
// NOT PORTED, on purpose (these exist for a game's process or a game's pixels, not the shell's):
//  - auto letterbox (the shell has no black bars; its margins stay 0), HDR/PQ format detection,
//    the sample-lag buffer choice and read-verify (v21 samples the buffer the compositor just flipped),
//  - [network] relay, [dev] UDP logging, [compat], the report file,
//  - writing a default ini: if the file is missing v22 keeps the sister's compiled defaults and says so.
//  The values in an INI line in the log are therefore the sister's own, with the shell's choice of preset.
//
// Foreground-bit reader gate; the shell feed polls the state file at 10 Hz.
//
// ENTRY POINT: module_start.
// CRASH-LOOP GUARD: marker /data/shell_probe_v22_ran (delete over FTP to rerun).
// HOOKS STAY INSTALLED after the window and keep counting; never unhooked. Reboot to clear.

//
// SOURCE LAYOUT. This file used to hold everything below; it is now split by job (map in
// shell-feed/include/shell_internal.h): shell_modules.c, shell_hooks.c, shell_buffers.c, shell_config.c,
// shell_frame.c, shell_state.c, shell_log.c, shell_report.c, shell_live.c, shell_thread.c, shell_shim.c and
// the legacy shell_probe17.c. This file keeps the entry point only. The comment above is the module's
// history; names in it such as live_send() and map_scan() now live in those files.

#include "shell_internal.h"

// [Stage D] Build identity. common/git_ver.h is written by the Makefile on every build (commit, branch,
// commit count, date) and plugin_common.h includes it; plugin_common.h also defines BUILD_TYPE. The
// fallbacks only matter for a PC test whose stand-in git_ver.h carries less than the real one.
#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif
#ifndef GIT_VER
#define GIT_VER "unknown"
#endif
#ifndef GIT_NUM
#define GIT_NUM 0
#endif
#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif
#ifndef BUILD_TYPE
#define BUILD_TYPE "(unknown)"
#endif

// Resolved in module_start for the record; nothing calls them (DO_FLIPSTATUS_CALL / DO_RESOLUTIONSTATUS_CALL
// are off).
static void             *g_pFlipStatusFn = NULL;   // [v11] resolved in module_start, used by probe_thread
static void             *g_pResStatusFn  = NULL;   // [v11] resolved in module_start, used by probe_thread
static volatile int      g_statusCallDone = 0;     // [v11] one-shot latch

// ---------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------

s32 attr_module_hidden module_start(s64 argc, const void *args)
{
    printf(TAG " ---- begin v24 compositor hooks + sampler (pid %d) ----\n", (int)getpid());
    // One line that says exactly which build this is, so a capture can prove what was running. It is the
    // HEAD at make time: uncommitted edits in the working tree are not reflected in the hash.
    printf(TAG " BUILD: commit=%s branch=%s num=%d built=%s %s\n", GIT_COMMIT, GIT_VER, (int)GIT_NUM, BUILD_DATE, BUILD_TYPE);

    shell_gates_log(); // [v24] one line per compile-time gate

    // 1. Who are we? Hooks are installed only in the compositor.
    struct proc_info procInfo = {0};
    int rc = sys_sdk_proc_info(&procInfo);
    printf(TAG " sys_sdk_proc_info rc=%d pid=%d name=%s titleid=%s\n",
           rc, (int)procInfo.pid, procInfo.name, procInfo.titleid);
    int isCompositor = name_has_ci(procInfo.name, "gnmcompositor") && name_has_ci(procInfo.titleid, "npxs20976");
    printf(TAG " expected target: name containing \"GnmCompositor\", titleid NPXS20976%s\n",
           isCompositor ? " -- MATCHES" : " -- DOES NOT MATCH, we are somewhere else");
    int mayHook = isCompositor || ALLOW_ANY_PROCESS;
    if (!mayHook)
        printf(TAG " not the compositor: this run is passive (census + resolve only); NO hooks will be installed.\n");

    // 2. ONE scan; nothing is loaded.
    list_modules_api();
    struct ScanResult s = scan_and_discover("census");
    printf(TAG " census: %d modules answered%s; gnmdriver modules: %d\n",
           s.found, s.hitCap ? ", HIT HARD CAP" : "", s.gnmCount);

    // 3. Resolve. VideoOut for the hooks; Gnm candidates for the record only.
    void *pReg  = resolve(s.hVideoOut, "libSceVideoOut.sprx", "sceVideoOutRegisterBuffers");
    void *pFlip = resolve(s.hVideoOut, "libSceVideoOut.sprx", "sceVideoOutSubmitFlip");
    // [v11] resolve-only, always -- no risk, confirms the symbols exist and
    // where, before anything decides whether to call them.
    g_pFlipStatusFn = resolve(s.hVideoOut, "libSceVideoOut.sprx", "sceVideoOutGetFlipStatus");
    g_pResStatusFn  = resolve(s.hVideoOut, "libSceVideoOut.sprx", "sceVideoOutGetResolutionStatus");
    int gnmAnyFlip = 0;
    for (int i = 0; i < s.gnmCount && i < MAX_GNM_MODS; i++) {
        int gh = s.gnmHandles[i];
        void *g1 = resolve(gh, "gnmdriver(substring match)", "sceGnmSubmitAndFlipCommandBuffers");
        void *g2 = resolve(gh, "gnmdriver(substring match)", "sceGnmSubmitAndFlipCommandBuffersForWorkload");
        void *g3 = resolve(gh, "gnmdriver(substring match)", "sceGnmSubmitCommandBuffers");
        void *g4 = resolve(gh, "gnmdriver(substring match)", "sceGnmSubmitDone");
        if (g1 != NULL || g2 != NULL) gnmAnyFlip = 1;
        (void)g3; (void)g4;
    }
    printf(TAG " VERDICT-INPUTS: videoout_resident=%d gnm_modules=%d reg=%d flip=%d gnm_public_flip_names_resolve=%d\n",
           s.hVideoOut >= 0, s.gnmCount, pReg != NULL, pFlip != NULL, gnmAnyFlip);

    if (!mayHook) {
        printf(TAG " module_start done (passive).\n");
        return 0;
    }

    // 4. Hooks, each with a before/after byte check (shell_hooks.c).
    int patchedReg = 0, patchedFlip = 0;
    shell_hooks_install(pReg, pFlip, &patchedReg, &patchedFlip);

    printf(TAG " patch summary: register=%d flip=%d\n", patchedReg, patchedFlip);
    if (!patchedReg && !patchedFlip) {
        printf(TAG " no hook was installed: nothing to count, no reporter started. Zero counts would mean nothing here.\n");
        return 0;
    }

#if DO_CONTROL_CALLS
    shell_hooks_control_calls(pReg, pFlip, patchedReg, patchedFlip); // 5. positive control (shell_hooks.c)
#endif

    OrbisPthread thread;
    scePthreadCreate(&thread, NULL, probe_thread, NULL, "shell_probe_thr");
#if SHELL_REPORT_FILE
    {   // [v24] the report thread: idle until [compat] report_file=1
        OrbisPthread rthread;
        scePthreadCreate(&rthread, NULL, shell_report_thread, NULL, "shell_report_thr");
    }
#else
    printf(TAG " report thread: compiled out (SHELL_REPORT_FILE=0)\n");
#endif

    printf(TAG " module_start done, reporter thread running\n");
    return 0;
}

s32 attr_module_hidden module_stop(s64 argc, const void *args) { return 0; }
