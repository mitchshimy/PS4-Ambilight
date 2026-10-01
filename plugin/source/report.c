// report.c -- part of ps4_ambient_light (v3.9.1).
//
// Writes /data/ps4_ambient_report_<TITLEID>.txt from a release build, so a tester can send
// one small file instead of running a debug build and capturing UDP. The text itself is
// built by report_file.h (string logic only, host-tested). This file does the two things
// that need the console: reading the plugin's existing globals, and writing the file.
//
// Why a thread of its own and not a call from ambient_sample_thread: a file write takes
// milliseconds on the console's disk, and the sampler's whole budget is 33 ms a pass at
// 30 Hz. The sampler is not touched except for a counter increment per loop and three
// stores once per 30-pass timing window (sample_thread.c). This thread only ever reads.
//
// When it writes. Once at start, then whenever the stage, the format or the guard/remap
// outcome changes (report_changed), and every REPORT_REFRESH_SECS otherwise so the timing
// and counters stay current. The change-driven writes are the point: a title that kills the
// game on its first sampling pass (Mortal Kombat 11 before v3.9) never gets to a "final"
// write, but the file on disk still says which stage it reached and what the guard saw.
//
// Opt-in: OFF unless the ini has [compat] report_file=1. Optional and not in the shipped ini, so a
// normal install never writes the file; a tester adds the key. 0, or no key, writes nothing.
// The thread still runs (a live ini reload can switch it on mid-game) but does nothing while off.
// After 3 failed writes in a row (no /data, disk full) it stops trying for the session.
// A title the plugin never hooks (plugin_load returned early) gets no file at all, and that
// absence is itself a finding, see docs/debugging/report-file.md.
//
// Known gap, accepted: the file is truncated and rewritten in place, so a crash in the
// middle of a write leaves an empty or cut-off file. The "end of report" line marks a
// complete one. Writing to a temporary name and renaming would need sceKernelRename.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "report_file.h"

volatile uint32_t g_reportEnabled       = 0;   // ini [compat] report_file (optional): 0 (default) off, 1 on
volatile uint32_t g_reportLoopPasses    = 0;   // sampler loop iterations that reached the bottom (sample_thread.c)
volatile uint32_t g_reportTimingWindows = 0;   // completed 30-pass timing windows (sample_thread.c)
volatile uint32_t g_reportPassUsAvg     = 0;   // last window, microseconds per pass
volatile uint32_t g_reportPassUsMax     = 0;

#define REPORT_MAX_FAILURES 3

typedef struct {
    ReportSnapshot last;        // what the file on disk says
    uint32_t       uptimeSecs;  // advanced by the thread loop, not by the tick, so a test can set it
    uint32_t       secsSinceWrite;
    uint32_t       failures;    // consecutive failed writes
    int            wroteOnce;
    int            pathBuilt;
    char           path[REPORT_PATH_MAX];
} ReportRunState;

// Same raw-kernel-call route settings.c uses for the ini (sceKernelOpen, not fopen). The flags
// are FreeBSD's: O_WRONLY 0x0001, O_CREAT 0x0200, O_TRUNC 0x0400. The default-ini write in
// settings.c leaves O_TRUNC off because the file is new; this one is rewritten in place, and
// without O_TRUNC a shorter report would leave the old tail behind.
static int report_write_file(const char *path, const char *text, size_t len)
{
    int32_t fd = sceKernelOpen(path, 0x0001 | 0x0200 | 0x0400, 0777);
    if (fd < 0) return -1;
    ssize_t w = sceKernelWrite(fd, text, len);
    sceKernelClose(fd);
    return (w == (ssize_t)len) ? 0 : -1;
}

static void report_take_snapshot(ReportSnapshot *s, uint32_t uptimeSecs)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->version, sizeof(s->version), "%s", AMBIENT_VERSION_STRING);
    snprintf(s->build, sizeof(s->build), "%.7s", GIT_COMMIT);
    snprintf(s->titleId, sizeof(s->titleId), "%s", g_titleId);
    s->uptimeSecs = uptimeSecs;

    s->registerCalls = g_registerHookCallCount;
    s->flipGnm       = g_flipHookCallCount;
    s->flipVideoOut  = g_videoOutSubmitFlipHookCallCount;
    s->flipWorkload  = g_gnmForWorkloadHookCallCount;
    s->bufferCount   = g_bufferCount;
    s->haveValidFormat = g_haveValidFormat;
    s->activeFormat  = g_activeFormat;
    s->loopPasses    = g_reportLoopPasses;
    s->backgrounded  = g_isBackgrounded ? 1 : 0;

    s->timingWindows = g_reportTimingWindows;
    s->passUsAvg     = g_reportPassUsAvg;
    s->passUsMax     = g_reportPassUsMax;

    s->guardAvailable   = g_guardAvailable;
    s->guardRejects     = g_guardRejectCount;
    s->guardLastRet     = g_guardLastRet;
    s->guardLastBadAddr = g_guardLastBadAddr;

    s->remapSetting    = g_gpuOnlyRemap;
    s->remapCreated    = g_remapCreated;
    s->remapFailed     = g_remapFailed;
    s->remapPasses     = g_remapPasses;
    s->remapLastMethod = g_remapLastMethod;
    s->remapLastRet    = g_remapLastRet;
}

// One turn of the thread loop. Returns 1 if it wrote the file this turn. Separate from the
// thread so tools/test_report.c can drive it with made-up counters.
static int ambient_report_tick(ReportRunState *rs)
{
    if (!g_reportEnabled || rs->failures >= REPORT_MAX_FAILURES) return 0;
    if (!rs->pathBuilt) {
        report_build_path(rs->path, sizeof(rs->path), g_titleId);
        rs->pathBuilt = 1;
    }

    ReportSnapshot s;
    report_take_snapshot(&s, rs->uptimeSecs);
    rs->secsSinceWrite++;                // this turn counts toward the periodic refresh
    if (rs->wroteOnce && !report_changed(&rs->last, &s) && rs->secsSinceWrite < REPORT_REFRESH_SECS)
        return 0;

    static char text[REPORT_TEXT_MAX];   // static so it stays off this thread's stack
    size_t len = report_format(text, sizeof(text), &s);
    if (report_write_file(rs->path, text, len) != 0) {
        rs->failures++;                  // last/wroteOnce unchanged, so it retries next turn, up to the limit
        return 0;
    }
    rs->failures = 0;
    rs->last = s;
    rs->wroteOnce = 1;
    rs->secsSinceWrite = 0;
    return 1;
}

void *ambient_report_thread(void *args)
{
    (void)args;
    static ReportRunState rs;            // static: zero-initialized, off the stack
    for (;;) {
        ambient_report_tick(&rs);
        usleep(1000000);
        rs.uptimeSecs++;
    }
    return NULL;
}
