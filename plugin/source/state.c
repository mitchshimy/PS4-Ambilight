// state.c -- direct foreground/background state writer for the shell-feed gate.
//
// The shell feed must not infer game ownership from successful WLED transmissions,
// output counters, flip counters, sequence freshness, or sampler progress. The rule is
// deliberately simple: while this game plugin reports the title in the foreground,
// publish fg=1; when the title is backgrounded, publish fg=0.
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

#define STATE_TICK_US 100000u

typedef struct {
    uint32_t seq;
    int      wasForeground;
    int      wroteOnce;
} StateRunState;

// O_WRONLY|O_CREAT, deliberately WITHOUT O_TRUNC. A fresh fd starts at offset zero.
static int state_write_file(const char *text, size_t len)
{
    int32_t fd = sceKernelOpen(STATE_PATH, 0x0001 | 0x0200, 0777);
    if (fd < 0) return -1;
    ssize_t w = sceKernelWrite(fd, text, len);
    sceKernelClose(fd);
    return (w == (ssize_t)len) ? 0 : -1;
}

// One turn of the state thread. Writes only at startup or when foreground status changes.
// Kept separate from the thread so tools/test_state.c can exercise the exact state logic.
static int ambient_state_tick(StateRunState *rs)
{
    bool foreground = !g_isBackgrounded;
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
        return 1;
    }

    return 0; // unchanged foreground state: no transmission checks, no heartbeat, no write
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
