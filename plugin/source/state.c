// The state file reports only the foreground bit (fg=1 after the first successful send, then follows g_isBackgrounded);
// never infer ownership from output, flips or seq. Fixed-width writes at offset 0, no O_TRUNC. [docs/code-comments.md]
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "state_file.h"

// Also declared here for the lightweight host test, whose kernel stubs provide these symbols.
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
    int      shellFrozen;    // the game holds the strip: the heartbeat is silent by design, so the last answer stands
} StateRunState;

#define SHELL_POLL_EVERY_TICKS 3u   // about every 300 ms
#define SHELL_RELAY_POLL_EVERY_TICKS 10u // about every 1 s, once past the startup window, while relay_signal_enabled is on
#define SHELL_POLL_CAP_TICKS   600u // about one minute from load; a title that never sends stops being polled

// O_WRONLY|O_CREAT, deliberately WITHOUT O_TRUNC. A fresh fd starts at offset zero.
static int state_write_file(const char *text, size_t len)
{
    uint64_t t0 = sceKernelGetProcessTimeCounter();
    int32_t fd = sceKernelOpen(STATE_PATH, 0x0001 | 0x0200, 0777);
    if (fd < 0) return -1;
    ssize_t w = sceKernelWrite(fd, text, len);
    sceKernelClose(fd);
    uint64_t hz = sceKernelGetProcessTimeCounterFrequency(), now = sceKernelGetProcessTimeCounter();
    if (hz != 0 && (now - t0) * 1000ULL / hz >= 100ULL) // a slow write delays the hand-off: name it, like the other FS_SLOW lines
        ambient_handoff_diag("HANDOFF_DIAG FS_SLOW op=state_write ms=%llu tsc=%llu hz=%llu",
                             (unsigned long long)((now - t0) * 1000ULL / hz), (unsigned long long)now, (unsigned long long)hz);
    return (w == (ssize_t)len) ? 0 : -1;
}

// One turn of the state thread. Writes only at startup or when the published foreground status changes.
// Kept separate from the thread so tools/test_state.c can exercise the exact state logic.
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

// Is the shell feed really running?
// More: docs/code-comments.md (plugin/source/state.c:101)
static void ambient_state_shell_poll(StateRunState *rs)
{
    // While this game holds the strip the shell feed writes no heartbeat (it stalls /data), so keep the last answer and read nothing.
    if (rs->firstSendSeen && rs->wasForeground) { rs->shellFrozen = 1; return; }
    if (rs->shellFrozen) { rs->shellFrozen = 0; rs->shellPollTicks = 0; ambient_shell_feed_rearm(); } // the strip is free: check afresh
    bool startupWindow = !rs->firstSendSeen && rs->shellTicks < SHELL_POLL_CAP_TICKS;
    // Relay hand-off (network.c, relay_release_external_source): a game that backgrounds or exits must...
    // More: docs/code-comments.md (plugin/source/state.c:109)
    bool relayWatch = g_config.relaySignalEnabled;
    if (!startupWindow && !relayWatch) return;
    if (startupWindow) rs->shellTicks++;
    if (++rs->shellPollTicks < (startupWindow ? SHELL_POLL_EVERY_TICKS : SHELL_RELAY_POLL_EVERY_TICKS)) return;
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
