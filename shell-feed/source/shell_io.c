// shell_io.c -- the shell feed's file I/O worker.
// Why: docs/debugging/filesystem-stalls.md
#include "shell_internal.h"

volatile uint64_t g_shLoopTicks = 0;
volatile int      g_shDisableSeen = 0;
static volatile int g_ioActive = 0; // live_send() is running; the worker only polls then
static int          g_ioStarted = 0;

#define IO_TICK_US 20000 // 20 ms: fine enough for the 50 ms state poll, nothing else needs it

static void *shell_io_thread(void *args)
{
    (void)args;
    uint64_t nextSlowTsc = 0, nextEnabledTsc = 0, nextBeatTsc = 0;
    for (;;) {
        usleep(IO_TICK_US);
        if (!g_ioActive) { nextSlowTsc = nextEnabledTsc = nextBeatTsc = 0; continue; }
        uint64_t now = rdtsc17();
        shell_state_io_poll(now, g_tscHz);   // claim bit, 20 Hz
        shell_cfg_watch(now, g_tscHz);       // ini change detection, every config_reload_check_seconds
        if (now >= nextSlowTsc) {            // the once-a-second work
            nextSlowTsc = now + g_tscHz;
            uint64_t t0 = sceKernelGetProcessTimeCounter();
            int32_t dfd = sceKernelOpen(DISABLE_FILE, 0 /* O_RDONLY */, 0);
            if (dfd >= 0) { sceKernelClose(dfd); g_shDisableSeen = 1; }
            shell_fs_slow_note("disable_file", t0);
            t0 = sceKernelGetProcessTimeCounter();
            shell_enabled_poll(now, g_tscHz, &nextEnabledTsc);
            shell_fs_slow_note("enabled_poll", t0);
            t0 = sceKernelGetProcessTimeCounter();
            shell_heartbeat(now, g_tscHz, &nextBeatTsc, g_shLoopTicks); // the loop's tick count: it stops changing if the loop stalls
            shell_fs_slow_note("heartbeat", t0);
        }
    }
    return NULL;
}

void shell_io_begin(void)
{
    g_shDisableSeen = 0;
    shell_cfg_drop_pending();
    if (!g_ioStarted) {
        OrbisPthread th;
        if (scePthreadCreate(&th, NULL, shell_io_thread, NULL, "shell_io_thr") == 0) g_ioStarted = 1;
        else printf(TAG " I/O worker could not start: the send loop polls files itself, as before\n");
    }
    if (!g_ioStarted) return;
    shell_state_io_poll(rdtsc17(), g_tscHz); // first claim bit read here, so the very first tick already knows who owns the strip
    g_shIoRunning = 1;
    g_ioActive = 1;
}

void shell_io_end(void)
{
    g_ioActive = 0;
}

// Called every tick by the send loop. With the worker it only counts the tick; without one it does the polls inline.
int shell_io_tick(uint64_t nowTsc, uint64_t tscHz, uint64_t *nextEnabledPollTsc, uint64_t *nextBeatTsc)
{
    uint64_t ticks = ++g_shLoopTicks;
    if (g_shIoRunning) return g_shDisableSeen;
    int32_t dfd = sceKernelOpen(DISABLE_FILE, 0 /* O_RDONLY */, 0);
    if (dfd >= 0) { sceKernelClose(dfd); return 1; }
    shell_enabled_poll(nowTsc, tscHz, nextEnabledPollTsc);
    shell_heartbeat(nowTsc, tscHz, nextBeatTsc, ticks);
    return 0;
}
