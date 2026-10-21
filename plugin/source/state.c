// Reports the foreground bit (fg=1 after the first send, then follows g_isBackgrounded) over the loopback lease (lease.h); the state file is the fallback.
// Never infer ownership from output, flips or seq. File writes are fixed-width at offset 0, no O_TRUNC. [docs/code-comments.md]
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "state_file.h"
#include "lease.h"

// Also declared here for the lightweight host test, whose kernel stubs provide these symbols.
extern uint64_t sceKernelGetProcessTimeCounter(void);
extern uint64_t sceKernelGetProcessTimeCounterFrequency(void);
const LeaseOps *ambient_lease_ops(void); // lease.c (the sockets); the host test supplies a fake

#define STATE_TICK_US 100000u

typedef struct {
    uint32_t seq;
    int      wasForeground; // what the FILE says (see ambient_state_tick): fg=1 only while no lease connection carries the claim
    int      wroteOnce;
    int      firstSendSeen; // launch latch: set once g_wledOutputCount first becomes non-zero, never cleared
    unsigned shellPollTicks; // ticks since the last shell-feed heartbeat check
    unsigned shellTicks;     // ticks spent checking so far; checking stops at SHELL_POLL_CAP_TICKS
    LeaseClient lease;       // the loopback TCP lease to the shell feed (lease.h): the primary hand-off channel
    int      leaseInit;
    int      leaseSeen;      // a lease connection existed once: from then on it, not the heartbeat file, says whether the shell feed runs
    unsigned relayTicks;     // ticks until the held strip is announced to the external-source signal again
} StateRunState;

#define RELAY_REFRESH_TICKS 50u     // about every 5 s while this game holds the strip
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

static uint32_t lease_now_ms(void)
{
    uint64_t hz = sceKernelGetProcessTimeCounterFrequency();
    return hz ? (uint32_t)(sceKernelGetProcessTimeCounter() / (hz / 1000u ? hz / 1000u : 1u)) : 0u;
}

// Does the plugin want to know whether a shell feed is there, even with nothing to claim? (Same window as the heartbeat check below.)
static bool ambient_state_wants_shell_link(const StateRunState *rs)
{
    return (!rs->firstSendSeen && rs->shellTicks < SHELL_POLL_CAP_TICKS) || g_config.relaySignalEnabled;
}

// One turn of the state thread: the lease carries the claim; the file is written only while no lease connection exists, and kept at fg=0 otherwise.
// Separate from the thread so tools/test_state.c can exercise the exact state logic. Design: docs/development/shell-feed-coordination.md
static int ambient_state_tick(StateRunState *rs)
{
    if (!rs->firstSendSeen && g_wledOutputCount != 0) rs->firstSendSeen = 1;
    bool foreground = !g_isBackgrounded && rs->firstSendSeen;
    char text[STATE_RECORD_LEN + 1];

    if (!rs->leaseInit) { lease_client_init(&rs->lease); rs->leaseInit = 1; }
    bool wasLeased = lease_client_connected(&rs->lease);
    lease_client_set(&rs->lease, foreground ? 1 : 0, g_titleId);
    lease_client_tick(&rs->lease, lease_now_ms(), ambient_lease_ops(), ambient_state_wants_shell_link(rs) ? 1 : 0);
    bool leased = lease_client_connected(&rs->lease);
    if (leased != wasLeased)
        ambient_handoff_diag("HANDOFF_DIAG LEASE %s fg=%d", leased ? "CONNECTED" : "LOST", foreground ? 1 : 0);

    // The game holds the strip, so it announces that itself every 5 s (and at once if the shell feed that used to is gone): the signal
    // must not depend on the shell feed running. Silent while backgrounded: then the shell feed, or nobody, owns the strip.
    if (g_config.relaySignalEnabled && foreground) {
        if (rs->relayTicks == 0 || (wasLeased && !leased)) { relay_send_external_source(true); rs->relayTicks = RELAY_REFRESH_TICKS; }
        else rs->relayTicks--;
    } else rs->relayTicks = 0;

    bool fileFg = foreground && !leased; // what the file should say right now
    if (!rs->wroteOnce || (int)fileFg != rs->wasForeground) {
        uint32_t nextSeq = rs->seq;
        if (fileFg && (!rs->wroteOnce || !rs->wasForeground)) {
            nextSeq = (nextSeq + 1u) % 1000000u;
        }
        size_t len = state_format(text, sizeof(text), fileFg ? 1 : 0, nextSeq, g_titleId);
        if (len == 0 || state_write_file(text, len) != 0) return 0;
        rs->seq = nextSeq;
        rs->wasForeground = fileFg ? 1 : 0;
        rs->wroteOnce = 1;
        // HANDOFF_DIAG: event-only instrumentation; never runs on unchanged ticks.
        ambient_handoff_diag("HANDOFF_DIAG GAME_STATE_WRITE fg=%d seq=%u tsc=%llu hz=%llu",
               fileFg ? 1 : 0, (unsigned)nextSeq,
               (unsigned long long)sceKernelGetProcessTimeCounter(),
               (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
        return 1;
    }

    return 0; // nothing changed: no lease message, no file write, no heartbeat
}

// Is the shell feed really running?
// More: docs/code-comments.md (plugin/source/state.c:101)
static void ambient_state_shell_poll(StateRunState *rs)
{
    // A lease connection IS the proof: the shell feed is listening, so it is running and enabled (it closes the listener when it
    // stops or is switched off, and the connection with it). No file is read.
    if (rs->leaseInit && lease_client_connected(&rs->lease)) { g_shellFeedAlive = 1; rs->leaseSeen = 1; return; }
    if (rs->leaseSeen) { g_shellFeedAlive = 0; return; } // it closed the lease (stopped or switched off): not running, no 3 s of doubt
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
