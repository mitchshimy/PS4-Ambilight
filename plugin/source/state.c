// state.c -- game-side writer for the cross-process strip-ownership claim.
//
// Writes /data/ps4-ambilight/ps4_ambient_state to tell shell-feed whether this plugin
// is actually producing successful WLED DDP sends while the title is foregrounded.
// Do NOT use flip-hook freshness as the ownership signal: the sampler can continue
// sending from its most recently selected buffer while flip counters temporarily stop.
// If the claim followed flips alone, shell-feed could resume and compete with this sender.
//
// network.c increments g_wledOutputCount only when sendto() accepts a complete DDP
// datagram. This independent 100 ms thread observes that counter and writes a seq
// heartbeat while successful output continues. A stalled sender stops advancing seq;
// the state writer then sends one explicit fg=0 after the output-activity window.
// Backgrounding still releases immediately, because sample_thread.c stops sending then.
//
// The state writer does not perform file I/O on the sampler's hot path. Records are
// fixed-width and written at offset zero without O_TRUNC; see state_file.h.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "state_file.h"

// Tick = 100 ms. The window and heartbeat are in ticks so tools/test_state.c can drive them
// without a clock.
#define STATE_TICK_US            100000u
#define STATE_OUTPUT_WINDOW_TICKS 22 // 2.2 s, below shell-feed's 2.5 s stale-claim timeout
#define STATE_HEARTBEAT_TICKS    10    // while fg=1, rewrite (seq+1) every this many ticks (~1 s)

typedef struct {
    uint32_t prevOutputCount;  // last successful DDP send count observed by this thread
    uint32_t seq;
    uint32_t ticksSinceOutput; // ticks since a successful DDP send was observed (saturates)
    int      outputSeen;       // has any successful DDP send been observed since load?
    uint32_t ticksSinceWrite;  // ticks since the last fg=1 write (saturates)
    int      wasForeground;    // did the LAST WRITE say fg=1?
    int      wroteOnce;
} StateRunState;

// Same raw-kernel-call route report.c and settings.c use. O_WRONLY|O_CREAT, deliberately
// WITHOUT O_TRUNC -- see state_file.h's header comment for why. A freshly opened fd's write
// position is 0 either way, so every write lands at the start of the file.
static int state_write_file(const char *text, size_t len)
{
    int32_t fd = sceKernelOpen(STATE_PATH, 0x0001 | 0x0200, 0777);
    if (fd < 0) return -1;
    ssize_t w = sceKernelWrite(fd, text, len);
    sceKernelClose(fd);
    return (w == (ssize_t)len) ? 0 : -1;
}

// One turn of the thread loop (every STATE_TICK_US). Returns 1 if it wrote the file this turn.
// Separate from the thread, same reasoning as report.c's ambient_report_tick, so
// tools/test_state.c can drive it with made-up counters.
static int ambient_state_tick(StateRunState *rs)
{
    uint32_t curOutputCount = g_wledOutputCount;
    bool sentThisTick = (curOutputCount != rs->prevOutputCount);
    rs->prevOutputCount = curOutputCount;

    if (sentThisTick)                            { rs->ticksSinceOutput = 0; rs->outputSeen = 1; }
    else if (rs->ticksSinceOutput < 0xFFFFFFF0u)  rs->ticksSinceOutput++;
    if (rs->ticksSinceWrite < 0xFFFFFFF0u)        rs->ticksSinceWrite++;

    // Ownership follows actual output progress, not flip-hook progress. Keep the claim
    // through brief gaps (including the 1 s keepalive path) but release before the
    // shell reader's 2.5 s claim-staleness deadline if successful sends stop.
    bool outputFresh = rs->outputSeen && rs->ticksSinceOutput < STATE_OUTPUT_WINDOW_TICKS;
    bool foreground = !g_isBackgrounded && outputFresh;
    char text[STATE_RECORD_LEN + 1];

    if (!rs->wroteOnce) {
        // First tick ever: establish the file immediately, regardless of foreground, so a
        // reader never has to special-case "file doesn't exist yet".
        if (foreground) rs->seq++;
        size_t len = state_format(text, sizeof(text), foreground ? 1 : 0, rs->seq, g_titleId);
        if (len == 0 || state_write_file(text, len) != 0) return 0; // retried next tick; wroteOnce stays 0
        rs->wroteOnce = 1;
        rs->wasForeground = foreground;
        rs->ticksSinceWrite = 0;
        return 1;
    }

    if (foreground) {
        // First successful-output observation: write NOW. Steady state: heartbeat once per second.
        if (rs->wasForeground && rs->ticksSinceWrite < STATE_HEARTBEAT_TICKS) return 0;
        rs->seq++;
        size_t len = state_format(text, sizeof(text), 1, rs->seq, g_titleId);
        if (len == 0) { rs->seq--; return 0; }
        if (state_write_file(text, len) != 0) { rs->seq--; return 0; } // retried next tick
        rs->wasForeground = 1;
        rs->ticksSinceWrite = 0;
        return 1;
    }

    if (rs->wasForeground) {
        // Backgrounding or a sustained lack of successful output: write fg=0 once, then go quiet.
        size_t len = state_format(text, sizeof(text), 0, rs->seq, g_titleId);
        if (len == 0 || state_write_file(text, len) != 0) return 0; // retried next tick; wasForeground stays 1
        rs->wasForeground = 0;
        return 1;
    }

    return 0; // already fg=0 on disk, nothing changed: no write
}

void *ambient_state_thread(void *args)
{
    (void)args;
    static StateRunState rs; // static: zero-initialized, off the stack
    for (;;) {
        ambient_state_tick(&rs);
        usleep(STATE_TICK_US);
    }
    return NULL;
}
