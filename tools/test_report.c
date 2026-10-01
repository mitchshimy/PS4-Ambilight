// test_report.c -- host test for the per-title report file (v3.9.1): plugin/include/report_file.h
// (the text) and plugin/source/report.c (when it writes, and the write itself).
//
// report.c is included, not linked, so this runs the real ambient_report_tick() against made-up
// counters. What stands in for the console is small: the three raw file calls (redirected from
// /data to a scratch directory, and honoring the flags report.c really passes, so a missing
// O_TRUNC shows up as a stale tail) and the plugin globals report.c reads.
//
// Build and run from the repo root (one line):
//   gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_report tools/test_report.c && /tmp/test_report
//
// Exits 0 if every check passed. What this can't show: that a real console lets a game process
// write /data, which is the same thing settings.c already relies on for the default ini, or what
// the counters read on a title nobody has tried.
#include "plugin_common.h"
#include "ambient_internal.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../plugin/source/report.c"

// ---- the plugin globals report.c reads (defined elsewhere in the plugin) ----
char g_titleId[16];
volatile uint32_t g_registerHookCallCount, g_flipHookCallCount, g_videoOutSubmitFlipHookCallCount, g_gnmForWorkloadHookCallCount;
volatile int32_t  g_bufferCount;
volatile int      g_haveValidFormat;
volatile uint32_t g_activeFormat;
bool g_isBackgrounded;
volatile uint32_t g_guardAvailable, g_guardRejectCount;
volatile int32_t  g_guardLastRet;
volatile uint64_t g_guardLastBadAddr;
volatile uint32_t g_gpuOnlyRemap = 3, g_remapCreated, g_remapFailed, g_remapPasses, g_remapLastMethod;
volatile int32_t  g_remapLastRet;

// ---- stand-in for the console's file calls ----
#define SCRATCH "/tmp/test_report_data"
static int  g_opens, g_failOpens, g_lastFlags, g_lastMode;
static char g_lastPath[256];
static void host_path(char *out, size_t n, const char *p)
{
    snprintf(out, n, "%s/%s", SCRATCH, strncmp(p, "/data/", 6) == 0 ? p + 6 : p);
}
int32_t sceKernelOpen(const char *path, int flags, int mode)
{
    g_opens++; g_lastFlags = flags; g_lastMode = mode;
    snprintf(g_lastPath, sizeof(g_lastPath), "%s", path);
    if (g_failOpens > 0) { g_failOpens--; return -1; }
    char hp[512]; host_path(hp, sizeof(hp), path);
    int of = ((flags & 0x0001) ? O_WRONLY : O_RDONLY) | ((flags & 0x0200) ? O_CREAT : 0) | ((flags & 0x0400) ? O_TRUNC : 0);
    return open(hp, of, 0644);
}
int64_t sceKernelLseek(int32_t fd, int64_t off, int wh) { return lseek(fd, off, wh); }
ssize_t sceKernelRead(int32_t fd, void *b, size_t n)    { return read(fd, b, n); }
ssize_t sceKernelWrite(int32_t fd, const void *b, size_t n) { return write(fd, b, n); }
int sceKernelClose(int32_t fd) { return close(fd); }
void klog(const char *fmt, ...) { (void)fmt; }

static char *slurp(const char *path, size_t *lenOut)
{
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1); assert(b);
    size_t got = fread(b, 1, (size_t)n, f); b[got] = '\0'; fclose(f);
    if (lenOut) *lenOut = got;
    return b;
}
static char *read_report(const char *title)   // what a tester would send
{
    char p[256]; snprintf(p, sizeof(p), "%s/ps4_ambient_report_%s.txt", SCRATCH, title);
    return slurp(p, NULL);
}
static void reset_globals(void)
{
    g_registerHookCallCount = g_flipHookCallCount = g_videoOutSubmitFlipHookCallCount = g_gnmForWorkloadHookCallCount = 0;
    g_bufferCount = 0; g_haveValidFormat = 0; g_activeFormat = 0; g_isBackgrounded = false;
    g_guardAvailable = 1; g_guardRejectCount = 0; g_guardLastRet = 0; g_guardLastBadAddr = 0;
    g_gpuOnlyRemap = 3; g_remapCreated = g_remapFailed = g_remapPasses = g_remapLastMethod = 0; g_remapLastRet = 0;
    g_reportEnabled = 1; g_reportLoopPasses = 0;   // on, so the tests below can see it write; the shipped default is checked first in main g_reportTimingWindows = g_reportPassUsAvg = g_reportPassUsMax = 0;
    g_opens = 0; g_failOpens = 0;
}
static void fresh_run(ReportRunState *rs, const char *title)
{
    reset_globals();
    snprintf(g_titleId, sizeof(g_titleId), "%s", title);
    memset(rs, 0, sizeof(*rs));
}

int main(void)
{
    system("rm -rf " SCRATCH " && mkdir -p " SCRATCH);

    // 0. Off by default, as shipped: a normal install, with no key in its ini, never writes the file.
    //    Nothing has touched g_reportEnabled yet, so this is report.c's own initializer.
    {
        assert(g_reportEnabled == 0);
        ReportRunState rs; memset(&rs, 0, sizeof(rs));
        snprintf(g_titleId, sizeof(g_titleId), "%s", "CUSA00000");
        g_registerHookCallCount = 1; g_flipHookCallCount = 7; g_reportLoopPasses = 3;   // a game that is running fine
        for (int i = 0; i < REPORT_REFRESH_SECS * 2; i++) assert(ambient_report_tick(&rs) == 0);
        assert(g_opens == 0);
        char p[256]; snprintf(p, sizeof(p), "%s/ps4_ambient_report_CUSA00000.txt", SCRATCH);
        assert(access(p, F_OK) != 0);
    }
    printf("off by default: no key in the ini, no file, no open call: PASSED\n");

    // 1. The text. An MK11-like snapshot: everything a maintainer needs is on the page, and the
    //    last line is the completeness marker.
    {
        ReportSnapshot s; memset(&s, 0, sizeof(s));
        snprintf(s.version, sizeof(s.version), "3.9.1"); snprintf(s.build, sizeof(s.build), "abc1234");
        snprintf(s.titleId, sizeof(s.titleId), "CUSA11395");
        s.uptimeSecs = 412; s.registerCalls = 1; s.flipGnm = 12345; s.bufferCount = 3;
        s.haveValidFormat = 1; s.activeFormat = 0x88740000; s.loopPasses = 9000;
        s.timingWindows = 300; s.passUsAvg = 2700; s.passUsMax = 3100;
        s.guardAvailable = 1; s.guardRejects = 2; s.guardLastRet = 0; s.guardLastBadAddr = 0x49ff38b6d0ULL;
        s.remapSetting = 3; s.remapCreated = 3; s.remapPasses = 1422; s.remapLastMethod = 3;
        char t[REPORT_TEXT_MAX];
        size_t n = report_format(t, sizeof(t), &s);
        assert(n == strlen(t) && n < sizeof(t) - 256);                 // plenty of headroom in the buffer
        assert(strstr(t, "plugin:         3.9.1\n") && strstr(t, "title:          CUSA11395\n"));
        assert(strstr(t, "stage:          running\n"));
        assert(strstr(t, "format:         0x88740000 (recognized)\n"));
        assert(strstr(t, "guard:          available yes, rejects 2, last_ret 0, last_bad_addr 0x49ff38b6d0\n"));
        assert(strstr(t, "last_method mprotect"));
        assert(strstr(t, "avg 2700, max 3100 (last 30 passes; 300 windows)"));
        assert(strcmp(t + n - strlen("end of report\n"), "end of report\n") == 0);
        assert(strstr(t, "192.168") == NULL && strstr(t, "wled") == NULL);   // nothing from the user's ini
        ReportSnapshot u = s; u.titleId[0] = '\0';
        report_format(t, sizeof(t), &u);
        assert(strstr(t, "title:          unknown\n"));
    }
    printf("report text has what a maintainer needs and ends with the marker: PASSED\n");

    // 2. Stages come from the counters alone, in order.
    {
        ReportSnapshot s; memset(&s, 0, sizeof(s));
        assert(!strcmp(report_stage(&s), "hooked"));
        s.registerCalls = 1;           assert(!strcmp(report_stage(&s), "registered"));
        s.flipWorkload = 1;            assert(!strcmp(report_stage(&s), "flipping"));   // only the ForWorkload entry point counts too
        s.loopPasses = 1;              assert(!strcmp(report_stage(&s), "running"));
        ReportSnapshot f; memset(&f, 0, sizeof(f)); f.flipVideoOut = 5;
        assert(!strcmp(report_stage(&f), "flipping"));                                   // flips without a register call still count
    }
    printf("stage: hooked, registered, flipping, running: PASSED\n");

    // 3. The path can't be steered by a title ID.
    {
        char p[REPORT_PATH_MAX];
        report_build_path(p, sizeof(p), "CUSA11395");   assert(!strcmp(p, "/data/ps4_ambient_report_CUSA11395.txt"));
        report_build_path(p, sizeof(p), "");            assert(!strcmp(p, "/data/ps4_ambient_report_unknown.txt"));
        report_build_path(p, sizeof(p), NULL);          assert(!strcmp(p, "/data/ps4_ambient_report_unknown.txt"));
        report_build_path(p, sizeof(p), "///...");      assert(!strcmp(p, "/data/ps4_ambient_report_unknown.txt"));
        report_build_path(p, sizeof(p), "../../etc/x"); assert(!strcmp(p, "/data/ps4_ambient_report_etcx.txt"));
        report_build_path(p, sizeof(p), "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
        assert(strlen(p) < sizeof(p) && strstr(p, "/data/ps4_ambient_report_") == p && strstr(p, "..") == NULL);
    }
    printf("title ID can't change where the file goes: PASSED\n");

    // 4. What counts as a change. The per-frame counters must not, or it rewrites every second.
    {
        ReportSnapshot a; memset(&a, 0, sizeof(a)); a.registerCalls = 1; a.flipGnm = 10; a.loopPasses = 5; a.guardAvailable = 1;
        ReportSnapshot b = a;
        b.flipGnm = 99999; b.loopPasses = 88888; b.passUsAvg = 3000; b.timingWindows = 4; b.uptimeSecs = 77; b.guardRejects = 0; b.remapPasses = 0;
        assert(!report_changed(&a, &b));
        b = a; b.haveValidFormat = 1;               assert(report_changed(&a, &b));
        b = a; b.activeFormat = 0x80000000;         assert(report_changed(&a, &b));
        b = a; b.guardRejects = 1;                  assert(report_changed(&a, &b));   // first reject
        a.guardRejects = 1; b = a; b.guardRejects = 50; assert(!report_changed(&a, &b)); // more of the same
        b = a; b.guardLastRet = -5;                 assert(report_changed(&a, &b));
        b = a; b.remapCreated = 3;                  assert(report_changed(&a, &b));
        b = a; b.remapFailed = 1;                   assert(report_changed(&a, &b));
        b = a; b.remapLastMethod = 3;               assert(report_changed(&a, &b));
        b = a; b.loopPasses = 0;                    assert(report_changed(&a, &b));   // stage running -> flipping
    }
    printf("only stage, format and guard/remap outcome count as a change: PASSED\n");

    // 5. A buffer that is too small can't overflow and still ends readably.
    {
        ReportSnapshot s; memset(&s, 0, sizeof(s)); snprintf(s.version, sizeof(s.version), "3.9.1");
        char small[96]; memset(small, 'x', sizeof(small));
        size_t n = report_format(small, sizeof(small), &s);
        assert(n == sizeof(small) - 1 && small[n] == '\0' && strstr(small, "end of report\n") != NULL);
        char tiny[8]; n = report_format(tiny, sizeof(tiny), &s);
        assert(n == sizeof(tiny) - 1 && tiny[n] == '\0');
        assert(report_format(tiny, 0, &s) == 0);
    }
    printf("an undersized buffer is cut safely: PASSED\n");

    // 6. The first turn writes straight away, with the flags that make a rewrite safe.
    {
        ReportRunState rs; fresh_run(&rs, "CUSA11395");
        assert(ambient_report_tick(&rs) == 1 && g_opens == 1);
        assert(!strcmp(g_lastPath, "/data/ps4_ambient_report_CUSA11395.txt"));
        assert(g_lastFlags == (0x0001 | 0x0200 | 0x0400));          // O_WRONLY | O_CREAT | O_TRUNC
        char *t = read_report("CUSA11395");
        assert(t && strstr(t, "stage:          hooked\n") && strstr(t, "title:          CUSA11395\n"));
        char want[64]; snprintf(want, sizeof(want), "build:          %.7s\n", GIT_COMMIT);
        assert(strstr(t, want));                                      // the commit the plugin was built from, cut to 7
        free(t);
    }
    printf("first turn writes at once, O_TRUNC requested: PASSED\n");

    // 7. A rewrite leaves no stale tail: pre-fill the file longer than any report, then tick.
    {
        ReportRunState rs; fresh_run(&rs, "CUSA22222");
        char p[256]; snprintf(p, sizeof(p), "%s/ps4_ambient_report_CUSA22222.txt", SCRATCH);
        FILE *f = fopen(p, "w"); for (int i = 0; i < 4000; i++) fputc('Z', f); fclose(f);
        assert(ambient_report_tick(&rs) == 1);
        size_t len; char *t = slurp(p, &len);
        assert(len < 2000 && strchr(t, 'Z') == NULL && strcmp(t + len - 14, "end of report\n") == 0);
        free(t);
    }
    printf("rewrite leaves no stale tail: PASSED\n");

    // 8. Writes follow changes, not frames, and a refresh comes round.
    {
        ReportRunState rs; fresh_run(&rs, "CUSA33333");
        assert(ambient_report_tick(&rs) == 1);                       // hooked
        for (int i = 0; i < 10; i++) assert(ambient_report_tick(&rs) == 0);   // nothing moved
        g_registerHookCallCount = 1; g_bufferCount = 3; g_haveValidFormat = 1; g_activeFormat = 0x88740000;
        assert(ambient_report_tick(&rs) == 1);                       // registered
        g_flipHookCallCount = 1;
        assert(ambient_report_tick(&rs) == 1);                       // flipping: where a crash on the first pass would be left
        char *t = read_report("CUSA33333");
        assert(strstr(t, "stage:          flipping\n") && strstr(t, "0x88740000 (recognized)") && strstr(t, "rejects 0"));
        free(t);
        g_flipHookCallCount = 500; g_videoOutSubmitFlipHookCallCount = 3;
        assert(ambient_report_tick(&rs) == 0);                       // more flips, same stage
        g_reportLoopPasses = 1;
        assert(ambient_report_tick(&rs) == 1);                       // running
        g_reportLoopPasses = 900; g_reportPassUsAvg = 2700; g_reportPassUsMax = 3100; g_reportTimingWindows = 30;
        assert(ambient_report_tick(&rs) == 0);                       // timing alone doesn't rewrite
        g_guardRejectCount = 1; g_guardLastBadAddr = 0x49ff38b6d0ULL;
        assert(ambient_report_tick(&rs) == 1);                       // the first guard reject does
        g_guardRejectCount = 400;
        assert(ambient_report_tick(&rs) == 0);
        g_remapCreated = 3; g_remapLastMethod = 3; g_remapPasses = 1;
        assert(ambient_report_tick(&rs) == 1);                       // remap outcome
        // Nothing moves for REPORT_REFRESH_SECS turns: exactly one refresh, which carries the timing.
        int writes = 0;
        for (int i = 0; i < REPORT_REFRESH_SECS * 2 + 5; i++) writes += ambient_report_tick(&rs);
        assert(writes == 2);
        t = read_report("CUSA33333");
        assert(strstr(t, "avg 2700, max 3100 (last 30 passes; 30 windows)") && strstr(t, "last_method mprotect") && strstr(t, "rejects 400"));
        free(t);
    }
    printf("writes on change and on refresh, not per frame: PASSED\n");

    // 9. The off switch, live: nothing written while off, writing resumes when turned back on.
    {
        ReportRunState rs; fresh_run(&rs, "CUSA44444");
        g_reportEnabled = 0;
        g_registerHookCallCount = 1;
        for (int i = 0; i < 5; i++) assert(ambient_report_tick(&rs) == 0);
        assert(g_opens == 0);
        g_reportEnabled = 1;
        assert(ambient_report_tick(&rs) == 1 && g_opens == 1);
        char *t = read_report("CUSA44444"); assert(strstr(t, "stage:          registered\n")); free(t);
    }
    printf("report_file off writes nothing, and turning it on mid-game starts the file: PASSED\n");

    // 10. A console that can't write /data: three tries, then quiet for the session. A success resets the count.
    {
        ReportRunState rs; fresh_run(&rs, "CUSA55555");
        g_failOpens = 1000;
        for (int i = 0; i < 20; i++) assert(ambient_report_tick(&rs) == 0);
        assert(g_opens == REPORT_MAX_FAILURES);                      // gave up, didn't hammer the disk
        fresh_run(&rs, "CUSA55556");
        g_failOpens = 2;
        assert(ambient_report_tick(&rs) == 0 && ambient_report_tick(&rs) == 0);
        assert(ambient_report_tick(&rs) == 1 && rs.failures == 0);   // third try works
        g_registerHookCallCount = 1; g_failOpens = 2;
        assert(ambient_report_tick(&rs) == 0 && ambient_report_tick(&rs) == 0 && ambient_report_tick(&rs) == 1);
    }
    printf("failed writes stop after 3 in a row, success resets the count: PASSED\n");

    // 11. Unknown title (sys_sdk_proc_info failed): still a file, under a name that says so.
    {
        ReportRunState rs; fresh_run(&rs, "");
        assert(ambient_report_tick(&rs) == 1);
        assert(!strcmp(g_lastPath, "/data/ps4_ambient_report_unknown.txt"));
        char *t = read_report("unknown"); assert(strstr(t, "title:          unknown\n")); free(t);
    }
    printf("unknown title still gets a file: PASSED\n");

    // 12. The version string the report prints can't drift from the one CI checks tags against.
    {
        size_t len; char *h = slurp("plugin/source/hooks.c", &len);
        assert(h && "run from the repo root");
        const char *k = strstr(h, "g_pluginVersion = 0x");
        assert(k);
        unsigned v = (unsigned)strtoul(k + strlen("g_pluginVersion = 0x"), NULL, 16);
        unsigned maj = (v >> 8) & 255, min = v & 255, sMaj = 0, sMin = 0, sPatch = 0;
        assert(sscanf(AMBIENT_VERSION_STRING, "%u.%u.%u", &sMaj, &sMin, &sPatch) == 3);
        assert(sMaj == maj && sMin == min);
        free(h);
        char *c = slurp("CHANGELOG.md", NULL);
        assert(c);
        char head[32]; snprintf(head, sizeof(head), "### v%s\n", AMBIENT_VERSION_STRING);
        const char *plugin = strstr(c, "## Plugin"); assert(plugin);
        assert(strstr(plugin, head) != NULL);                        // the release has a changelog entry
        free(c);
    }
    printf("AMBIENT_VERSION_STRING agrees with g_pluginVersion and the changelog: PASSED\n");

    printf("ALL REPORT CHECKS PASSED\n");
    return 0;
}
