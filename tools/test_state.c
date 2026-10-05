// test_state.c -- host test for the cross-process state file (v3.9.2): plugin/include/state_file.h
// (the record format) and plugin/source/state.c (when it writes, and the write itself).
//
// state.c is included, not linked, so this runs the real ambient_state_tick() against made-up
// flip counters and backgrounded/foreground flags. What stands in for the console is the two raw
// file calls (redirected from /data to a scratch directory, honoring the flags state.c really
// passes, so an accidental O_TRUNC would show up here) and the plugin globals state.c reads.
//
// Build and run from the repo root (one line):
//   gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_state tools/test_state.c && /tmp/test_state
//
// Exits 0 if every check passed. What this can't show: that a real console lets a game process
// write /data (settings.c's default-ini write already relies on the same thing), or how another
// process polling this file actually behaves under real timing -- that is the research repo's side.
#include "plugin_common.h"
#include "ambient_internal.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../plugin/source/state.c"

// ---- the plugin globals state.c reads (defined elsewhere in the plugin) ----
char g_titleId[16];
volatile uint32_t g_registerHookCallCount, g_flipHookCallCount, g_videoOutSubmitFlipHookCallCount, g_gnmForWorkloadHookCallCount;
bool g_isBackgrounded;

// ---- stand-in for the console's file calls ----
#define SCRATCH "/tmp/test_state_data"
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
static char *read_state(size_t *lenOut)   // what a reader in another process would see
{
    return slurp(SCRATCH "/ps4_ambient_state", lenOut);
}
static void reset_globals(void)
{
    g_registerHookCallCount = 0;
    g_flipHookCallCount = g_videoOutSubmitFlipHookCallCount = g_gnmForWorkloadHookCallCount = 0;
    g_isBackgrounded = false;
    g_opens = 0; g_failOpens = 0;
}
static void fresh_run(StateRunState *rs, const char *title)
{
    reset_globals();
    snprintf(g_titleId, sizeof(g_titleId), "%s", title);
    memset(rs, 0, sizeof(*rs));
}
// Advances one of the three flip counters, the way a real flip would.
static void flip_once(void) { g_flipHookCallCount++; }

int main(void)
{
    system("rm -rf " SCRATCH " && mkdir -p " SCRATCH);

    // 0. The very first tick writes immediately, fg=0, regardless of state -- a reader never
    //    has to special-case "file doesn't exist yet" during the first second.
    {
        StateRunState rs; fresh_run(&rs, "CUSA00000");
        assert(ambient_state_tick(&rs) == 1 && g_opens == 1);
        size_t len; char *t = read_state(&len);
        assert(len == STATE_RECORD_LEN);
        assert(!strcmp(t, "v1 fg=0 seq=000000 title=CUSA00000\n"));
        free(t);
    }
    printf("first tick writes immediately, fg=0: PASSED\n");

    // 1. Foreground + flipping: fg=1, seq increments every tick it stays true.
    {
        StateRunState rs; fresh_run(&rs, "CUSA11111");
        ambient_state_tick(&rs);                 // tick 0: the initial fg=0 write
        g_isBackgrounded = false; flip_once();
        assert(ambient_state_tick(&rs) == 1);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=1 seq=000001 title=CUSA11111\n")); free(t);
        flip_once();
        assert(ambient_state_tick(&rs) == 1);
        t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=1 seq=000002 title=CUSA11111\n")); free(t);
    }
    printf("foreground+flipping: fg=1, seq increments: PASSED\n");

    // 2. Foreground but NOT flipping (a static frame, no new flip this tick): not a claim.
    {
        StateRunState rs; fresh_run(&rs, "CUSA22222");
        ambient_state_tick(&rs);                 // initial fg=0
        g_isBackgrounded = false;                // foreground, but counters never move
        assert(ambient_state_tick(&rs) == 0);    // already fg=0 on disk: no write at all
        assert(g_opens == 1);                    // only the initial write happened
    }
    printf("foreground without a flip this tick is not a claim: PASSED\n");

    // 3. Backgrounded while flipping (shouldn't happen on a real console, but the gate must
    //    still hold): not a claim either.
    {
        StateRunState rs; fresh_run(&rs, "CUSA33333");
        ambient_state_tick(&rs);
        g_isBackgrounded = true; flip_once();
        assert(ambient_state_tick(&rs) == 0 && g_opens == 1);
    }
    printf("backgrounded, even while flipping, is not a claim: PASSED\n");

    // 4. The transition itself: fg=1 for a while, then stop flipping/go background -- exactly
    //    ONE fg=0 write, then total silence while it stays that way.
    {
        StateRunState rs; fresh_run(&rs, "CUSA44444");
        ambient_state_tick(&rs);
        g_isBackgrounded = false; flip_once();
        assert(ambient_state_tick(&rs) == 1);     // fg=1 seq=1
        g_isBackgrounded = true;                  // now backgrounded
        assert(ambient_state_tick(&rs) == 1);     // the ONE fg=0 write
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=0 seq=000001 title=CUSA44444\n")); free(t); // seq unchanged on the off-write
        int opensBefore = g_opens;
        for (int i = 0; i < 10; i++) assert(ambient_state_tick(&rs) == 0); // stays quiet
        assert(g_opens == opensBefore);
    }
    printf("exactly one fg=0 write on the transition, then silence: PASSED\n");

    // 5. Resuming: seq keeps counting up from where it left off, does not reset to 0.
    {
        StateRunState rs; fresh_run(&rs, "CUSA55555");
        ambient_state_tick(&rs);
        g_isBackgrounded = false; flip_once(); ambient_state_tick(&rs); // seq=1
        g_isBackgrounded = true; ambient_state_tick(&rs);               // fg=0, seq still 1
        g_isBackgrounded = false; flip_once();
        assert(ambient_state_tick(&rs) == 1);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=1 seq=000002 title=CUSA55555\n")); free(t); // 2, not reset to 1
    }
    printf("seq keeps counting across a background/foreground cycle: PASSED\n");

    // 6. Record length is the same constant for fg 0 and 1, a seq that needs wrapping, and an
    //    empty title -- not just hand-counted, actually checked.
    {
        char buf[STATE_RECORD_LEN + 1];
        assert(state_format(buf, sizeof(buf), 0, 0, "CUSA00000") == STATE_RECORD_LEN);
        assert(state_format(buf, sizeof(buf), 1, 999999, "CUSA99999") == STATE_RECORD_LEN);
        assert(state_format(buf, sizeof(buf), 1, 1000000, "CUSA00000") == STATE_RECORD_LEN); // wraps to 000000
        assert(!strcmp(buf, "v1 fg=1 seq=000000 title=CUSA00000\n"));
        assert(state_format(buf, sizeof(buf), 0, 5, "") == STATE_RECORD_LEN);   // empty title -> "-", padded
        assert(!strcmp(buf, "v1 fg=0 seq=000005 title=-        \n"));
        assert(state_format(buf, sizeof(buf), 0, 5, "WAYTOOLONGATITLEID") == STATE_RECORD_LEN); // truncated, not overrun
    }
    printf("record length is a true constant, incl. seq wraparound and a bad title: PASSED\n");

    // 7. Never O_TRUNC (the whole reason for this file's write style): check the actual flags
    //    passed to sceKernelOpen on an ordinary write.
    {
        StateRunState rs; fresh_run(&rs, "CUSA66666");
        ambient_state_tick(&rs);
        assert((g_lastFlags & 0x0400) == 0);          // O_TRUNC bit not set
        assert((g_lastFlags & (0x0001 | 0x0200)) == (0x0001 | 0x0200)); // O_WRONLY|O_CREAT are
    }
    printf("never opens with O_TRUNC: PASSED\n");

    // 8. A console that can't write /data: the tick just doesn't advance state; no crash, no
    //    special-casing needed by the caller (state.c has no retry-limit of its own, unlike
    //    report.c, because every tick is cheap and idempotent to retry).
    {
        StateRunState rs; fresh_run(&rs, "CUSA77777");
        g_failOpens = 3;
        for (int i = 0; i < 3; i++) assert(ambient_state_tick(&rs) == 0);
        assert(rs.wroteOnce == 0);
        assert(ambient_state_tick(&rs) == 1 && rs.wroteOnce == 1); // works once the disk does
    }
    printf("a failing disk just keeps retrying the same tick, no crash: PASSED\n");

    printf("ALL state.c TESTS PASSED\n");
    return 0;
}
