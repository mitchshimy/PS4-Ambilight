// state.c -- direct foreground/background state writer for the shell-feed gate.
//
// The shell feed must not infer game ownership from successful WLED transmissions,
// output counters, flip counters, sequence freshness, or sampler progress. The rule is
// deliberately simple: while this game plugin reports the title in the foreground,
// publish fg=1; when the title is backgrounded, publish fg=0.
//
// ONE exception, a launch-only latch (docs/debugging/launch-gap-test.md): fg=1 is not published
// until this process has completed its first DDP send. Before that the game is "foreground" but
// cannot drive the strip yet (launch captures: first flip hook ~3.1 s after load), and publishing
// fg=1 early made the shell feed back off into a ~3.2 s silence. The latch is read once, becomes
// true once, and is never cleared; after it, ownership follows g_isBackgrounded exactly as before
// (no per-frame or per-send inference, no heartbeat). A title that never sends (unsupported,
// unreadable buffers) stays fg=0, so the shell feed keeps the strip instead of leaving it dark.
//
// The same thread also keeps g_shellFeedAlive for network.c's startup hold: from load until the first
// send it asks ambient_shell_feed_expected() (settings.c) about every 300 ms whether the shell feed's
// heartbeat file is still changing, then stops, so normal play does no file or ini reads. It also stops
// after about one minute (SHELL_POLL_CAP_TICKS) if the title never sends, so an unsupported title does not
// open files and parse the ini for the whole session; g_shellFeedAlive then keeps its last answer. That is
// a read of a different file, not a write of this one; this file still has no heartbeat.
//
// This thread polls the already-maintained g_isBackgrounded flag at 10 Hz and writes
// only the initial state and actual state transitions. File I/O stays off the sampler's
// hot path. Records remain fixed-width and are written at offset zero without O_TRUNC.
//
// Limitation: if the plugin/process exits or crashes without publishing fg=0, the last
// state may remain on disk. The current reader intentionally does not use seq freshness
// as an ownership signal, so stale-state recovery must be handled separately rather than
// silently reintroducing output/heartbeat inference. This prototype does not yet hook unload.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "state_file.h"

extern uint64_t sceKernelGetProcessTimeCounter(void);
extern uint64_t sceKernelGetProcessTimeCounterFrequency(void);

#define STATE_TICK_US 100000u

typedef struct {
    uint32_t seq;
    int      wasForeground;
    int      wroteOnce;
    int      firstSendSeen; // launch latch: set once g_wledOutputCount first becomes non-zero, never cleared
    unsigned shellPollTicks; // ticks since the last shell-feed heartbeat check
    unsigned shellTicks;     // ticks spent checking so far; checking stops at SHELL_POLL_CAP_TICKS
} StateRunState;

#define SHELL_POLL_EVERY_TICKS 3u   // about every 300 ms
#define SHELL_POLL_CAP_TICKS   600u // about one minute from load; a title that never sends stops being polled

// O_WRONLY|O_CREAT, deliberately WITHOUT O_TRUNC. A fresh fd starts at offset zero.
static int state_write_file(const char *text, size_t len)
{
    int32_t fd = sceKernelOpen(STATE_PATH, 0x0001 | 0x0200, 0777);
    if (fd < 0) return -1;
    ssize_t w = sceKernelWrite(fd, text, len);
    sceKernelClose(fd);
    return (w == (ssize_t)len) ? 0 : -1;
}

// One turn of the state thread. Writes only at startup or when the published foreground status changes.
// Kept separate from the thread so the state logic reads on its own.
static int ambient_state_tick(StateRunState *rs)
{
    if (!rs->firstSendSeen && g_wledOutputCount != 0) rs->firstSendSeen = 1;
    bool foreground = !g_isBackgrounded && rs->firstSendSeen;
    char text[STATE_RECORD_LEN + 1];

    if (!rs->wroteOnce || (int)foreground != rs->wasForeground) {
        uint32_t nextSeq = rs->seq;
        if (foreground && (!rs->wroteOnce || !rs->wasForeground)) {
            nextSeq = (nextSeq + 1u) % 1000000u;
        }
        size_t len = state_format(text, sizeof(text), foreground ? 1 : 0, nextSeq, g_titleId);
        if (len == 0 || state_write_file(text, len) != 0) return 0;
        rs->seq = nextSeq;
        rs->wasForeground = foreground ? 1 : 0;
        rs->wroteOnce = 1;
        // HANDOFF_DIAG: event-only instrumentation; never runs on unchanged ticks.
        ambient_handoff_diag("HANDOFF_DIAG GAME_STATE_WRITE fg=%d seq=%u tsc=%llu hz=%llu",
               foreground ? 1 : 0, (unsigned)nextSeq,
               (unsigned long long)sceKernelGetProcessTimeCounter(),
               (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
        return 1;
    }

    return 0; // unchanged foreground state: no transmission checks, no heartbeat, no write
}

// Is the shell feed really running? Judged from plugin load until the first send, so the answer is already
// there when the game feed's first frame arrives (network.c's startup hold reads it). About every 300 ms.
// Stops for good once the first send has gone out, and after about one minute if none ever does (an
// unsupported title), so neither normal play nor a title that never sends keeps reading files or the ini;
// g_shellFeedAlive keeps its last answer. Separate from the thread.
static void ambient_state_shell_poll(StateRunState *rs)
{
    if (rs->firstSendSeen || rs->shellTicks >= SHELL_POLL_CAP_TICKS) return;
    rs->shellTicks++;
    if (++rs->shellPollTicks < SHELL_POLL_EVERY_TICKS) return;
    rs->shellPollTicks = 0;
    g_shellFeedAlive = ambient_shell_feed_expected() ? 1 : 0;
}

void *ambient_state_thread(void *args)
{
    (void)args;
    static StateRunState rs; // static: zero-initialized, off the stack
    for (;;) {
        ambient_state_tick(&rs);
        ambient_state_shell_poll(&rs);
        usleep(STATE_TICK_US);
    }
    return NULL;
}
