// shell_probe v24: v23 plus the four Phase 3 leftovers (docs/integration_plan_v2.md): report file...
// More: docs/code-comments.md (shell-feed/source/main.c:1)

// SOURCE LAYOUT. This file used to hold everything below; it is now split by job (map in...
// More: docs/code-comments.md (shell-feed/source/main.c:83)

#include "shell_internal.h"

// [Stage D] Build identity. common/git_ver.h is written by the Makefile on every build (commit...
// More: docs/code-comments.md (shell-feed/source/main.c:92)
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

// Entry point
// More: docs/code-comments.md (shell-feed/source/main.c:117)

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
