// report.c -- part of ps4_ambient_light (v3.9.1).
// More: docs/code-comments.md (plugin/source/report.c:1)
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

// Same raw-kernel-call route settings.c uses for the ini (sceKernelOpen, not fopen).
// More: docs/code-comments.md (plugin/source/report.c:58)
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
