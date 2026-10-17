// shell_thread.c -- probe_thread(): the start-up sequence (calibrate, read the aux ini keys, find the display
// buffers) and the loop around live_send(), plus the two standby waits it uses: one for a clean stop (disable
// file / [shell] enabled=false), one for a missing or unusable wled_host.
#include "shell_internal.h"

// ---------------------------------------------------------------------
// Reporter thread (probe_thread)
// [v23] The CUE table and the timed reporter loop have been removed: live_send() now runs until the
// process exits and never returns, so the post-send loop was dead code. Status rows (LIVE t=Xs ...) are
// printed by the send loop itself every 5 s and carry all the progress information that was in the CUEs.
// ---------------------------------------------------------------------

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

// [v23-fix] Entered only after a clean, user-triggered stop (live_send() returned 1 or 2), never after a
// configuration/environment failure (0). Polls for both kill-switches to clear -- the disable file gone AND
// [shell] enabled= back to true -- then returns so probe_thread() can call live_send() again. Does not touch
// hooks, map_scan() or find_bufs(): the buffers already found stay as they are, same as a running live_send()
// never re-scans them either. The disable file is checked every poll regardless of the ini's own cadence,
// since it carries no cadence of its own.
//
// [v45] The ini itself used to be re-read on the SAME cadence as live_send()'s own reload poll
// (config_reload_check_seconds) -- which meant a value of 0 (meaning "never poll" while actively
// sending, a deliberate way to skip the file-stat overhead at 30 Hz) also meant "never poll" here,
// where nothing is being sent and there is no overhead to avoid. The result, confirmed on hardware
// (HANDOFF v44): a permanent deadlock -- `[shell] enabled=false` with `config_reload_check_seconds=0`
// could not be cleared by `enabled=true`, nor by raising `config_reload_check_seconds` itself, since
// that value can only be learned by a read this never performed. Only a reboot cleared it.
//
// Fixed by giving standby its own cadence, independent of config_reload_check_seconds: the ini is now
// re-read every STANDBY_RELOAD_MS regardless of that setting. config_reload_check_seconds still governs
// how often live_send() re-reads the ini WHILE SENDING (unchanged, 0 still means "never poll" there,
// same deliberate opt-out as before) -- this only changes the no-cost, not-sending-anything wait here.
#define STANDBY_POLL_MS   1000u
#define STANDBY_RELOAD_MS 1000u // [v45] fixed cadence for standby's own ini re-read; does not read
                                 // config_reload_check_seconds, so a value of 0 there can no longer
                                 // block recovery. Deliberately == STANDBY_POLL_MS (checked every
                                 // wake): standby already wakes once a second to poll the disable
                                 // file, and sceKernelOpen+a config_size_and_hash stat/read is cheap
                                 // next to that, so there is no reason to wait any longer than this
                                 // loop already does.
static void wait_for_reenable(int stopReason)
{
    g_shState = SH_STANDBY; // [v24]
    // [v47] Told once per pause (this function is entered once per clean stop, not per poll). The disable
    // file is always worth saying: forgotten, it leaves a dark strip with no explanation. [shell] enabled=false
    // is a setting the person chose, so it is only announced when the feed was already running and just got
    // turned off -- not at every boot of a console where it has simply been off all along.
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

// Standby for a missing or unusable wled_host. With no compiled-in fallback address, a first-run shell feed has
// nowhere to send; this used to stop the thread for good (a console restart to recover). Instead wait here, re-reading
// the ini once a second (the same cadence wait_for_reenable() uses), and return as soon as wled_host holds something
// other than the value that failed, so setting it in the ini or the companion app starts the feed on its own, the same
// way a destination change is already followed live once the feed is running. No heartbeat is written while waiting
// (there is no loop), so the game plugin correctly treats the shell feed as not running.
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
    // [v19] FIX 2: g_nBuf is only ever set by find_bufs(), which v17 reached through scene_run(1,..).
    // v18 stopped calling scene_run, so g_nBuf stayed 0 and live_send() would have returned
    // "no display buffers found". Scan and match the buffers here, then send.
    map_scan(0);
    find_bufs();
    // [v23] live_send() runs until a clean stop fires (disable file, [shell] enabled=false) or a
    // configuration/environment problem stops it before it ever starts sending. The post-send reporter
    // and watch loops that followed here in v22 are removed: they were dead code from v18 onward
    // (live_send already ran for LIVE_SECONDS, leaving no time) and are doubly dead now.
    // [v23-fix] A clean stop used to end probe_thread() for good ("Reboot to clear"), which meant
    // [shell] enabled=true back in the ini, or removing the disable file, did not actually resume
    // anything -- found on hardware in capture v23-A. Loop here instead: wait in wait_for_reenable()
    // for both kill-switches to clear, then call live_send() again. Buffers found earlier are not
    // re-scanned; a stop with reason 0 (buffers/socket) is not retried automatically (a missing or unusable host now waits in wait_for_destination()).
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
