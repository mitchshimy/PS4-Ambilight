// state.c -- part of ps4_ambient_light (v3.9.2).
//
// Writes /data/ps4_ambient_state: whether THIS plugin is actively driving the strip right now
// (foreground and the flip hooks moved in the last second), a sequence number that only
// advances while that's true, and the title ID. A second, independent process -- a
// compositor-level feed over in the research repo, reading the shell's own screens (Home,
// Settings, the quick menu) -- polls this file so the two never send to WLED at the same time.
// This plugin never reads the file and has no code that knows the other project exists beyond
// this comment; it only ever writes its own state. See docs/development/shell-feed-coordination.md.
//
// Why its own thread and not a call from ambient_sample_thread: same reasoning as report.c --
// a file write takes milliseconds on the console's disk, and the sampler's whole budget is
// 33 ms a pass at 30 Hz. Nothing in the sampler is touched by this file at all; state.c only
// reads globals the sampler and the flip hooks already maintain for other reasons.
//
// Always on, unlike report.c's [compat] report_file=1 opt-in: a missing or stale file is
// already the correct "no claim" signal for the other side (see state_file.h), so there is
// nothing to protect an ordinary install from by making this optional, and the other project's
// whole arbitration depends on the file existing whenever this plugin is loaded.
//
// When it writes (v3.9.3): once at load (fg=0, seq=0, so a reader never has to treat "file
// doesn't exist yet" as a special case during the first second), then IMMEDIATELY on the
// transition to foreground+flipping (fg=1, seq incrementing), then once a second as a heartbeat
// while that stays true, then EXACTLY ONCE on the transition to not-foreground-or-not-flipping
// (fg=0, same seq) -- after that single write the thread stops touching the file until the next
// foreground+flipping moment. A crash leaves the file on fg=1 with a frozen seq, which is what
// the reader's own staleness timeout (not anything this plugin does) is for.
//
// Why the thread now ticks every 100 ms instead of once a second (v3.9.2 slept a full second
// between looks): a reader that backs off while this file says fg=1 can only back off after it
// has seen the record, and v3.9.2 wrote the first fg=1 up to a second after the first flip.
// For that second both this plugin and the reader were sending to the same strip, and the
// strip alternated between them (a 1-2 s flicker at every game launch). "Flipped in the last
// second" is kept as the meaning of fg=1 -- it is now a window of STATE_FLIP_WINDOW_TICKS ticks
// instead of one tick -- so what counts as a claim is unchanged; only how soon it is written is.
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
#define STATE_FLIP_WINDOW_TICKS  10    // a flip within this many ticks (~1 s) still counts as flipping
#define STATE_HEARTBEAT_TICKS    10    // while fg=1, rewrite (seq+1) every this many ticks (~1 s)

typedef struct {
    uint32_t prevFlipGnm, prevFlipVideoOut, prevFlipWorkload; // last tick's counters
    uint32_t seq;
    uint32_t ticksSinceFlip;   // ticks since any flip counter last moved (saturates)
    int      flipSeen;         // has any flip counter moved since load? (zero-init must mean "no")
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
    uint32_t curFlipGnm      = g_flipHookCallCount;
    uint32_t curFlipVideoOut = g_videoOutSubmitFlipHookCallCount;
    uint32_t curFlipWorkload = g_gnmForWorkloadHookCallCount;

    bool flippedThisTick = (curFlipGnm != rs->prevFlipGnm) || (curFlipVideoOut != rs->prevFlipVideoOut)
                         || (curFlipWorkload != rs->prevFlipWorkload);
    rs->prevFlipGnm = curFlipGnm; rs->prevFlipVideoOut = curFlipVideoOut; rs->prevFlipWorkload = curFlipWorkload;

    if (flippedThisTick)                        { rs->ticksSinceFlip = 0; rs->flipSeen = 1; }
    else if (rs->ticksSinceFlip < 0xFFFFFFF0u)    rs->ticksSinceFlip++;
    if (rs->ticksSinceWrite < 0xFFFFFFF0u)        rs->ticksSinceWrite++;

    // Same meaning as v3.9.2 (flipped in the last ~second), no longer tied to which 1 s tick
    // the thread happened to be on.
    bool flipping   = flippedThisTick || (rs->flipSeen && rs->ticksSinceFlip < STATE_FLIP_WINDOW_TICKS);
    bool foreground = !g_isBackgrounded && flipping;
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
        // Rising edge: write NOW. Steady state: heartbeat once per STATE_HEARTBEAT_TICKS.
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
        // Transition to not-foreground: write fg=0 exactly once, then go quiet.
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
