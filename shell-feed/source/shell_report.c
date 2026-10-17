// shell_report.c -- the compile-time gate table, and the report file (/data/ps4-ambilight/...
// More: docs/code-comments.md (shell-feed/source/shell_report.c:1)
#include "shell_internal.h"

// --- gates: one table, one log line each (the v18 lesson). A gate macro that is not defined is a
// compile ERROR here (not a silent #if -> 0), because the table uses the macro's value directly. ---
typedef struct { const char *name; int on; int critical; const char *whenOff; } ShGate;
static const ShGate kGates[] = {
    { "DO_WLED_SEND",             DO_WLED_SEND,             1, "no socket headers, no DDP send: the strip is never driven" },
    { "DO_LIVE_DDP",              DO_LIVE_DDP,              1, "live_send() is not compiled: the strip is never driven (v18's failure)" },
    { "DO_CONTROL_CALLS",         DO_CONTROL_CALLS,         0, "the two hook positive-control calls are skipped" },
    { "ALLOW_ANY_PROCESS",        ALLOW_ANY_PROCESS,        0, "hooks go in GnmCompositor only (intended)" },
    { "ALLOW_KERNEL_PRX_LOAD",    ALLOW_KERNEL_PRX_LOAD,    0, "resolve-only: no kernel PRX is loaded to find symbols" },
    { "DO_MPROTECT_READ",         DO_MPROTECT_READ,         0, "no mprotect/readability probe of candidate regions (intended)" },
    { "DO_FLIPSTATUS_CALL",       DO_FLIPSTATUS_CALL,       0, "sceVideoOutGetFlipStatus is resolved but never called (intended)" },
    { "DO_RESOLUTIONSTATUS_CALL", DO_RESOLUTIONSTATUS_CALL, 0, "sceVideoOutGetResolutionStatus is resolved but never called (intended)" },
    { "DO_DUMP_BUFFERS",          DO_DUMP_BUFFERS,          0, "v17 buffer dumps not compiled in (intended)" },
    { "DO_SAMPLER_SCENES",        DO_SAMPLER_SCENES,        0, "v17 fixed-scene schedule not used (intended; no #if depends on it in v24)" },
    { "SHELL_REPORT_FILE",        SHELL_REPORT_FILE,        0, "no report thread: [compat] report_file=1 does nothing" },
    { "SHELL_DEV_MIRROR",         SHELL_DEV_MIRROR,         0, "no [dev] UDP mirror: dev_logging=true does nothing" },
    { "SHELL_SELF_HEAL",          SHELL_SELF_HEAL,          0, "a bad display buffer is skipped forever, never re-scanned" },
};
#define SH_GATE_COUNT ((int)(sizeof(kGates) / sizeof(kGates[0])))

// Writes the line for gate i into out. ON gates are one short line; OFF gates say what is missing.
static void sh_gate_line(char *out, size_t cap, int i)
{
    const ShGate *g = &kGates[i];
    if (g->on) snprintf(out, cap, "GATE: %-24s = 1 (ON)", g->name);
    else       snprintf(out, cap, "GATE: %-24s = 0 (OFF%s) -- %s", g->name, g->critical ? ", CRITICAL" : "", g->whenOff);
}

// One line per compile-time gate at start: a gate that is off is stated, never silent (the v18 lesson).
void shell_gates_log(void)
{

    int off = 0, crit = 0;
    for (int i = 0; i < SH_GATE_COUNT; i++) {
        char gl[160]; sh_gate_line(gl, sizeof(gl), i);
        printf(TAG " %s\n", gl);
        if (!kGates[i].on) { off++; if (kGates[i].critical) crit++; }
    }
    printf(TAG " GATES: %d total, %d off, %d critical off%s\n", SH_GATE_COUNT, off, crit,
           crit ? " -- THIS BUILD CANNOT DRIVE THE STRIP" : "");

}

// --- the report file: /data/ps4-ambilight/ps4_ambient_report_SHELL.txt, same opt-in and layout idea as the sister's report ---
#define SH_REPORT_PATH          AMBILIGHT_DATA_DIR "/ps4_ambient_report_SHELL.txt"
#define SH_REPORT_TEXT_MAX      2048
#define SH_REPORT_REFRESH_SECS  60    // rewrite this often even when nothing changed (counters)
#define SH_REPORT_MAX_FAILURES  3     // then stop trying for the session, like the sister

typedef struct {
    int      state;             // SH_*
    int      stopReason;        // live_send()'s last return: 0 never started, 1 disable file, 2 [shell] enabled=false
    char     notStartedWhy[40]; // why live_send() returned 0, "" otherwise
    uint32_t uptimeSecs;
    uint32_t liveSessions;      // times live_send() reached its send loop
    uint64_t sent, skipped, gameOwned;
    uint32_t flips;             // compositor flips counted by the hook
    int      bufCount;
    uint32_t consecFail, heals, healsRecovered, healsFailed;
    int      claimEverSeen, claimActive;
    uint32_t iniReloads;
    int      iniReadOk, shellEnabled, devMirror;
} ShReport;

static const char *sh_state_name(int s)
{
    switch (s) { case SH_STARTING: return "starting"; case SH_LIVE: return "live";
                 case SH_STANDBY: return "standby";   case SH_STOPPED: return "stopped"; default: return "unknown"; }
}
static const char *sh_stop_name(int r)
{
    switch (r) { case 0: return "none"; case 1: return "disable file present"; case 2: return "[shell] enabled=false"; default: return "unknown"; }
}

// True when something a person reading the file cares about moved. Per-frame counters are left out on
// purpose (the refresh timer covers them), or this would rewrite the file every second.
static int sh_report_changed(const ShReport *a, const ShReport *b)
{
    return a->state != b->state || a->stopReason != b->stopReason
        || strcmp(a->notStartedWhy, b->notStartedWhy) != 0
        || a->liveSessions != b->liveSessions || a->bufCount != b->bufCount
        || a->heals != b->heals || a->healsFailed != b->healsFailed
        || a->claimEverSeen != b->claimEverSeen || a->iniReadOk != b->iniReadOk
        || a->shellEnabled != b->shellEnabled || a->devMirror != b->devMirror
        || (a->sent > 0) != (b->sent > 0);
}

// Same rule as the sister's tick: write the first time, on a change, and every REFRESH seconds.
// sinceWrite must already count this turn.
static int sh_report_due(const ShReport *last, const ShReport *cur, int wroteOnce, uint32_t sinceWrite)
{
    if (!wroteOnce) return 1;
    return sh_report_changed(last, cur) || sinceWrite >= SH_REPORT_REFRESH_SECS;
}

// No WLED address, no ini values, nothing typed -- same promise as the sister's file. Last line is always "end of report".
static size_t sh_report_format(char *out, size_t cap, const ShReport *s)
{
    if (cap == 0) return 0;
    size_t n = 0;
    int w = snprintf(out, cap,
        "PS4 Ambilight shell feed report\n"
        "Send this file when you report a problem with the shell feed. It has no personal data in it.\n"
        "\n"
        "module:         shell_probe v24\n"
        "state:          %s\n"
        "last_stop:      %s\n"
        "not_started:    %s\n"
        "uptime_s:       %u\n"
        "live_sessions:  %u\n"
        "\n"
        "ticks:          sent %llu, skipped %llu, game_owned %llu\n"
        "compositor_flips: %u\n"
        "buffers:        %d\n"
        "self_heal:      bad_in_a_row %u, rescans %u, recovered %u, found_nothing %u\n"
        "claim:          ever_seen %s, active %s\n"
        "ini:            reloads %u, last_read %s, shell_enabled %s\n"
        "dev_mirror:     %s\n"
        "\n",
        sh_state_name(s->state), sh_stop_name(s->stopReason), s->notStartedWhy[0] ? s->notStartedWhy : "-",
        (unsigned)s->uptimeSecs, (unsigned)s->liveSessions,
        (unsigned long long)s->sent, (unsigned long long)s->skipped, (unsigned long long)s->gameOwned,
        (unsigned)s->flips, s->bufCount,
        (unsigned)s->consecFail, (unsigned)s->heals, (unsigned)s->healsRecovered, (unsigned)s->healsFailed,
        s->claimEverSeen ? "yes" : "no", s->claimActive ? "yes" : "no",
        (unsigned)s->iniReloads, s->iniReadOk ? "ok" : "FAILED", s->shellEnabled ? "yes" : "no",
        s->devMirror ? "on" : "off");
    if (w < 0) { out[0] = '\0'; return 0; }
    n = (size_t)w;
    for (int i = 0; i < SH_GATE_COUNT && n < cap; i++) {
        char gl[160]; sh_gate_line(gl, sizeof(gl), i);
        w = snprintf(out + n, cap - n, "%s\n", gl);
        if (w < 0) break;
        n += (size_t)w;
    }
    if (n < cap) { w = snprintf(out + n, cap - n, "\nend of report\n"); if (w > 0) n += (size_t)w; }
    if (n >= cap) {
        static const char tail[] = "\n(cut off)\nend of report\n";
        size_t t = sizeof(tail) - 1;
        if (cap > t) memcpy(out + cap - 1 - t, tail, t);
        out[cap - 1] = '\0';
        return cap - 1;
    }
    return n;
}

// [v24] Report-file side: live_send() publishes its counters into these globals at each 5 s row and at...
// More: docs/code-comments.md (shell-feed/source/shell_report.c:152)

volatile int      g_shState = SH_STARTING;
volatile int      g_shStopReason = 0;
volatile uint32_t g_shLiveSessions = 0;

static volatile uint64_t g_shSent = 0, g_shSkipped = 0, g_shGameOwned = 0;

char              g_shNotStartedWhy[40] = "";

void sh_publish(uint64_t sent, uint64_t skipped, uint64_t gameOwned)
{
    g_shSent = sent; g_shSkipped = skipped; g_shGameOwned = gameOwned;
}

static void sh_report_snapshot(ShReport *s, uint32_t uptimeSecs)
{
    memset(s, 0, sizeof(*s));
    s->state = g_shState; s->stopReason = g_shStopReason;
    snprintf(s->notStartedWhy, sizeof(s->notStartedWhy), "%s", g_shNotStartedWhy);
    s->uptimeSecs = uptimeSecs; s->liveSessions = g_shLiveSessions;
    s->sent = g_shSent; s->skipped = g_shSkipped; s->gameOwned = g_shGameOwned;
    s->flips = g_flipCount; s->bufCount = g_nBuf;
    s->consecFail = g_heal.consecFail; s->heals = g_heal.heals;
    s->healsRecovered = g_heal.recovered; s->healsFailed = g_heal.failed;
    s->claimEverSeen = g_claimEverSeen; s->claimActive = g_claimActive;
    s->iniReloads = g_cfgReloads; s->iniReadOk = g_cfgLastReadOk; s->shellEnabled = g_shellEnabled;
    s->devMirror = sh_dev_active(g_shDevLog, g_shDevIp) && SHELL_DEV_MIRROR;
}

#if SHELL_REPORT_FILE
// Same raw-kernel-call route the probe's dumps use. Flags are FreeBSD's: O_WRONLY 1, O_CREAT 0x200, O_TRUNC 0x400.
static int sh_report_write(const char *text, size_t len)
{
    int32_t fd = sceKernelOpen(SH_REPORT_PATH, 0x0001 | 0x0200 | 0x0400, 0777);
    if (fd < 0) return -1;
    int64_t w = sceKernelWrite(fd, text, len);
    sceKernelClose(fd);
    return (w == (int64_t)len) ? 0 : -1;
}

// Does nothing while [compat] report_file is not 1 (an ini reload can switch it on later).
void *shell_report_thread(void *args)
{
    (void)args;
    static ShReport last, cur;
    static char text[SH_REPORT_TEXT_MAX];
    uint32_t uptime = 0, since = 0, failures = 0;
    int wrote = 0;
    for (;;) {
        if (g_shReportOn && failures < SH_REPORT_MAX_FAILURES) {
            sh_report_snapshot(&cur, uptime);
            since++;
            if (sh_report_due(&last, &cur, wrote, since)) {
                size_t len = sh_report_format(text, sizeof(text), &cur);
                if (sh_report_write(text, len) != 0) failures++;
                else { failures = 0; last = cur; wrote = 1; since = 0; }
            }
        }
        usleep(1000000);
        uptime++;
    }
    return NULL;
}
#endif
