// Host regression test for the direct foreground/background state writer.
// Build from the kit root:
//   gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_state tools/test_state.c && /tmp/test_state
// This tests the real ambient_state_tick() and file record format, not PS4 foreground detection.
#include "plugin_common.h"
#include "ambient_internal.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Deterministic stand-ins for the event-only timing diagnostics in state.c.
uint64_t sceKernelGetProcessTimeCounter(void) { static uint64_t t = 1000; return ++t; }
uint64_t sceKernelGetProcessTimeCounterFrequency(void) { return 1000000; }

void ambient_handoff_diag(const char *fmt, ...) { (void)fmt; } // HOF1 mirror is network.c; not under test here

#include "../plugin/source/state.c"

char g_titleId[16];
volatile uint32_t g_registerHookCallCount, g_flipHookCallCount, g_videoOutSubmitFlipHookCallCount, g_gnmForWorkloadHookCallCount;
volatile uint32_t g_wledOutputCount;
volatile int g_shellFeedAlive = 1;
bool ambient_shell_feed_expected(void) { return true; } // heartbeat logic is settings.c; not under test here
bool g_isBackgrounded;

#define SCRATCH "/tmp/test_state_data"
static int g_opens, g_failOpens, g_lastFlags, g_lastMode;
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
ssize_t sceKernelRead(int32_t fd, void *b, size_t n) { return read(fd, b, n); }
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
static char *read_state(size_t *lenOut) { return slurp(SCRATCH "/ps4-ambilight/ps4_ambient_state", lenOut); }
static void reset_globals(void)
{
    g_registerHookCallCount = 0;
    g_flipHookCallCount = g_videoOutSubmitFlipHookCallCount = g_gnmForWorkloadHookCallCount = 0;
    g_wledOutputCount = 0;
    g_isBackgrounded = false;
    g_opens = 0; g_failOpens = 0;
}
static void fresh_run(StateRunState *rs, const char *title)
{
    reset_globals();
    snprintf(g_titleId, sizeof(g_titleId), "%s", title);
    memset(rs, 0, sizeof(*rs));
}

int main(void)
{
    system("rm -rf " SCRATCH " && mkdir -p " SCRATCH "/ps4-ambilight");

    // Launch latch: startup publishes fg=0 until the game's first DDP send, so the shell feed keeps the
    // strip through the launch picture. The first send then publishes fg=1 (the launch-gap fix).
    {
        StateRunState rs; fresh_run(&rs, "CUSA00000");
        assert(ambient_state_tick(&rs) == 1);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=0 seq=000000 title=CUSA00000\n")); free(t);
        assert(rs.wroteOnce && !rs.wasForeground);
        for (int i = 0; i < 50; ++i) assert(ambient_state_tick(&rs) == 0); // no send yet: stays fg=0, no writes
        g_flipHookCallCount = 5; g_registerHookCallCount = 1;               // hooks alone do not release the shell
        assert(ambient_state_tick(&rs) == 0);
        g_wledOutputCount = 1;                                              // first complete DDP send
        assert(ambient_state_tick(&rs) == 1);
        t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=1 seq=000001 title=CUSA00000\n")); free(t);
        assert(rs.wasForeground && rs.firstSendSeen);
    }
    printf("fg=0 until the first DDP send, then fg=1 (launch latch): PASSED\n");

    // After the latch, ownership does not depend on output or sequence activity: output going quiet,
    // counters changing or not, nothing moves fg. No heartbeat writes.
    {
        StateRunState rs; fresh_run(&rs, "CUSA11111");
        g_wledOutputCount = 1; g_flipHookCallCount = 0;
        assert(ambient_state_tick(&rs) == 1);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=1 seq=000001 title=CUSA11111\n")); free(t);
        for (int i = 0; i < 100; ++i) { g_wledOutputCount += (i % 3 == 0); assert(ambient_state_tick(&rs) == 0); }
        g_wledOutputCount = 0; // a counter reset or silence must not release or re-latch anything
        for (int i = 0; i < 100; ++i) assert(ambient_state_tick(&rs) == 0);
        t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=1 seq=000001 title=CUSA11111\n")); free(t);
        assert(g_opens == 1); // no heartbeat and no transmission-driven polling/writes
    }
    printf("after the latch fg=1 holds regardless of output activity; no heartbeat writes: PASSED\n");

    // Background transition releases immediately; repeated background ticks stay quiet.
    {
        StateRunState rs; fresh_run(&rs, "CUSA22222");
        g_wledOutputCount = 1;
        assert(ambient_state_tick(&rs) == 1);
        g_isBackgrounded = true;
        assert(ambient_state_tick(&rs) == 1);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=0 seq=000001 title=CUSA22222\n")); free(t);
        int opens = g_opens;
        for (int i = 0; i < 100; ++i) assert(ambient_state_tick(&rs) == 0);
        assert(g_opens == opens);
    }
    printf("background transition writes fg=0 once and then stays quiet: PASSED\n");

    // Foreground transition increments the sequence, but the gate's decision is the fg bit.
    {
        StateRunState rs; fresh_run(&rs, "CUSA33333");
        g_wledOutputCount = 1;
        assert(ambient_state_tick(&rs) == 1);
        g_isBackgrounded = true; assert(ambient_state_tick(&rs) == 1);
        g_isBackgrounded = false; assert(ambient_state_tick(&rs) == 1);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=1 seq=000002 title=CUSA33333\n")); free(t);
    }
    printf("background-to-foreground transition republishes fg=1: PASSED\n");

    // Startup while already backgrounded must publish fg=0, not assume foreground.
    {
        StateRunState rs; fresh_run(&rs, "CUSA44444");
        g_isBackgrounded = true;
        assert(ambient_state_tick(&rs) == 1);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=0 seq=000000 title=CUSA44444\n")); free(t);
    }
    printf("startup while backgrounded publishes fg=0: PASSED\n");

    // Failed writes retry without changing the remembered state or burning a sequence number.
    {
        StateRunState rs; fresh_run(&rs, "CUSA55555");
        g_wledOutputCount = 1;
        g_failOpens = 1;
        assert(ambient_state_tick(&rs) == 0 && !rs.wroteOnce);
        assert(ambient_state_tick(&rs) == 1 && rs.wroteOnce);
        g_isBackgrounded = true;
        g_failOpens = 1;
        assert(ambient_state_tick(&rs) == 0 && rs.wasForeground);
        assert(ambient_state_tick(&rs) == 1 && !rs.wasForeground);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=0 seq=000001 title=CUSA55555\n")); free(t);
    }
    printf("failed file writes retry safely: PASSED\n");

    // A title that never sends (unsupported, unreadable buffers) never claims the strip.
    {
        StateRunState rs; fresh_run(&rs, "CUSA77777");
        assert(ambient_state_tick(&rs) == 1);
        for (int i = 0; i < 300; ++i) assert(ambient_state_tick(&rs) == 0);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=0 seq=000000 title=CUSA77777\n")); free(t);
    }
    printf("a title that never sends stays fg=0 (shell feed keeps the strip): PASSED\n");

    // Backgrounded before the first send: sending later must not publish fg=1 while backgrounded.
    {
        StateRunState rs; fresh_run(&rs, "CUSA88888");
        g_isBackgrounded = true;
        assert(ambient_state_tick(&rs) == 1);
        g_wledOutputCount = 1;
        assert(ambient_state_tick(&rs) == 0);
        g_isBackgrounded = false;
        assert(ambient_state_tick(&rs) == 1);
        char *t = read_state(NULL);
        assert(!strcmp(t, "v1 fg=1 seq=000001 title=CUSA88888\n")); free(t);
    }
    printf("latch never overrides backgrounded; foregrounding then publishes fg=1: PASSED\n");

    // Fixed-width record and safe non-truncating file writes remain unchanged.
    {
        char buf[STATE_RECORD_LEN + 1];
        assert(state_format(buf, sizeof(buf), 0, 0, "CUSA00000") == STATE_RECORD_LEN);
        assert(state_format(buf, sizeof(buf), 1, 999999, "CUSA99999") == STATE_RECORD_LEN);
        StateRunState rs; fresh_run(&rs, "CUSA66666");
        g_wledOutputCount = 1;
        assert(ambient_state_tick(&rs) == 1);
        assert((g_lastFlags & 0x0400) == 0);
        assert((g_lastFlags & (0x0001 | 0x0200)) == (0x0001 | 0x0200));
    }
    printf("fixed-width records and non-truncating writes remain intact: PASSED\n");
    system("rm -rf " SCRATCH);
    return 0;
}
