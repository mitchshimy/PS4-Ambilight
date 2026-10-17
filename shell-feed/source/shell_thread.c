// shell_thread.c -- probe_thread(): the start-up sequence (calibrate, read the aux ini keys, find the...
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:1)
#include "shell_internal.h"

// Reporter thread (probe_thread) [v23] The CUE table and the timed reporter loop have been removed...
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:6)

#if DO_LIVE_DDP

// [v24] Early read of just the three aux keys, so the report file and the [dev] mirror work even when
// live_send() returns before its own ini read (no buffers, no destination, ...) -- exactly when a report is wanted.
static void sh_early_flags_load(void)
{
    ini_table_s *table = ini_table_create();
    if (table != NULL && ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) sh_parse_aux(table);
    if (table != NULL) ini_table_destroy(table);
}

static int g_shGateSig = -1;
// One line whenever the runtime gates (all ini-driven) change; the first call after each load always prints.
void sh_runtime_gates_note(void)
{
    int rep = (SHELL_REPORT_FILE && g_shReportOn) ? 1 : 0;
    int dev = (SHELL_DEV_MIRROR && sh_dev_active(g_shDevLog, g_shDevIp)) ? 1 : 0;
    int heal = SHELL_SELF_HEAL ? 1 : 0;
    int sig = rep | (dev << 1) | (heal << 2);
    if (sig == g_shGateSig) return;
    g_shGateSig = sig;
    shell_event("GATE (runtime, from the ini): report_file=%s dev_mirror=%s self_heal=%s",
                SHELL_REPORT_FILE ? (g_shReportOn ? "ON" : "OFF ([compat] report_file is not 1)") : "OFF (compiled out)",
                SHELL_DEV_MIRROR ? (dev ? "ON" : "OFF ([dev] needs dev_ip AND dev_logging=true)") : "OFF (compiled out)",
                heal ? "ON" : "OFF (compiled out)");
}

// Standby re-reads the ini every second regardless of config_reload_check_seconds: with 0 it used to deadlock
// (enabled=false could never be cleared without a reboot). [docs/code-comments.md]
#define STANDBY_POLL_MS   1000u
#define STANDBY_RELOAD_MS 1000u // [v45] fixed cadence for standby's own ini re-read; does not read
                                 // config_reload_check_seconds, so a value of 0 there can no longer block recovery.
                                 // More: docs/code-comments.md (shell-feed/source/shell_thread.c:61)
static void wait_for_reenable(int stopReason)
{
    g_shState = SH_STANDBY; // [v24]
    // [v47] Told once per pause (this function is entered once per clean stop, not per poll).
    // More: docs/code-comments.md (shell-feed/source/shell_thread.c:70)
    if (stopReason == 1)
        sh_notify(false, "shell feed is paused: " DISABLE_FILE " exists. Delete it to resume.");
    else if (g_shLiveSessions > 0)
        sh_notify(false, "shell feed is paused: [shell] enabled=false in the ini. Set it back to true to resume.");
    sh_dev_send(stopReason == 1 ? "STANDBY: disable file present" : "STANDBY: [shell] enabled=false");
    printf(TAG " STANDBY: feed stopped (%s), waiting to resume (re-checking the ini every %us"
           " regardless of config_reload_check_seconds).\n",
           stopReason == 1 ? "disable file present" : "[shell] enabled=false",
           (unsigned)(STANDBY_RELOAD_MS / 1000u));

    uint32_t elapsedMs = 0;
    for (;;) {
        usleep(STANDBY_POLL_MS * 1000);
        elapsedMs += STANDBY_POLL_MS;

        int32_t dfd = sceKernelOpen(DISABLE_FILE, 0 /* O_RDONLY */, 0);
        if (dfd >= 0) { sceKernelClose(dfd); elapsedMs = 0; continue; } // still held off by the file

        if (elapsedMs >= STANDBY_RELOAD_MS) { // [v45] was gated on config_reload_check_seconds > 0; no longer
            elapsedMs = 0;
            shell_cfg_load(); // sets g_shellEnabled directly; a read failure leaves it as it was, same as shell_cfg_poll()
        }

        if (g_shellEnabled) {
            printf(TAG " RESUMING: re-enabled, restarting the send loop.\n"); sh_dev_send("RESUMING");
            if (g_shLiveSessions > 0) sh_notify(false, "shell feed resumed."); // [v47] not for a first start: that says "running"
            return;
        }
    }
}

// Standby for a missing or unusable wled_host.
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:105)
static void wait_for_destination(void)
{
    char failedHost[sizeof(g_config.wledHost)];
    strncpy(failedHost, g_config.wledHost, sizeof(failedHost) - 1);
    failedHost[sizeof(failedHost) - 1] = '\0';
    g_shState = SH_STANDBY;
    sh_dev_send("STANDBY: no usable wled_host");
    printf(TAG " STANDBY: no usable wled_host (\"%s\"), waiting for one (re-checking the ini every %us).\n",
           failedHost, (unsigned)(STANDBY_RELOAD_MS / 1000u));
    uint32_t elapsedMs = 0;
    for (;;) {
        usleep(STANDBY_POLL_MS * 1000);
        elapsedMs += STANDBY_POLL_MS;
        if (elapsedMs < STANDBY_RELOAD_MS) continue;
        elapsedMs = 0;
        shell_cfg_load(); // a read failure leaves g_config as it was, same as shell_cfg_poll()
        if (g_config.wledHost[0] != '\0' && strcmp(g_config.wledHost, failedHost) != 0) {
            printf(TAG " RESUMING: wled_host is now \"%s\", starting the send loop.\n", g_config.wledHost);
            sh_dev_send("RESUMING: wled_host set");
            return;
        }
    }
}

#endif

void *probe_thread(void *args)
{
    // [v23] printedEvents, prevFlip, nextCue removed: the post-send reporter loop they served is gone.
    printf(TAG " probe_thread started (v24: runs until reboot or clean stop).\n");

    // [v17] sampler setup (unchanged); [v18] scene_run is not called this version.
    build_points();
    printf(TAG " SAMPLER: %d zones x %d points (+%d scatter points) built; points outside the buffer: %d\n", NZ, ZPTS, SC_PTS, g_badPts);
    calibrate_tsc();

#if DO_LIVE_DDP
    sh_early_flags_load();   // [v24] report_file / [dev] known before the first live_send(), even if it returns at once
    sh_runtime_gates_note(); // [v24]
    // [v19] FIX 2: g_nBuf is only ever set by find_bufs(), which v17 reached through scene_run(1,..). v18...
    // More: docs/code-comments.md (shell-feed/source/shell_thread.c:150)
    map_scan(0);
    find_bufs();
    // [v23] live_send() runs until a clean stop fires (disable file, [shell] enabled=false) or a...
    // More: docs/code-comments.md (shell-feed/source/shell_thread.c:155)
    for (;;) {
        int stopReason = live_send();
        if (stopReason == 0 && (strcmp(g_shNotStartedWhy, "no destination host") == 0 ||
                                strcmp(g_shNotStartedWhy, "destination is not an address") == 0)) {
            // A host problem is fixed in the ini or the companion app, so wait for that instead of needing a reboot.
            if (strcmp(g_shNotStartedWhy, "no destination host") == 0)
                sh_notify(true, "shell feed is waiting: no WLED address is set. Set wled_host in the ini or the companion app and it will start on its own.");
            else
                sh_notify(true, "shell feed is waiting: wled_host isn't an IP address. Fix it in the ini or the companion app and it will start on its own.");
            wait_for_destination();
            continue;
        }
        if (stopReason == 0) {
            printf(TAG " probe_thread: send loop stopped and will not retry (no buffers or"
                   " socket setup failed -- see the LIVE line above); hooks stay installed. Reboot to retry.\n");
            g_shState = SH_STOPPED; // [v24] the report file says why (not_started:)
            shell_event("STOPPED for good: %s", g_shNotStartedWhy[0] ? g_shNotStartedWhy : "see the LIVE line");
            // [v47] The two host reasons are handled above (they wait for a fix); what is left is not retried, so say how to.
            sh_notify(false, "shell feed stopped: %s. Restart the console to try again.", g_shNotStartedWhy[0] ? g_shNotStartedWhy : "it couldn't start");
            break;
        }
        wait_for_reenable(stopReason);
    }
#else
    printf(TAG " LIVE: compiled out (DO_LIVE_DDP=0): this build has no send loop and will never drive the strip.\n");
    g_shState = SH_STOPPED;
#endif

    return NULL;
}
