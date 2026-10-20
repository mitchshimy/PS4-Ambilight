// shell_io.c -- the shell feed's file I/O worker.
// Why: docs/debugging/filesystem-stalls.md
#include "shell_internal.h"

volatile uint64_t g_shLoopTicks = 0;
volatile int      g_shDisableSeen = 0;
static volatile int g_ioActive = 0; // live_send() is running; the worker only polls then
static int          g_ioStarted = 0;
static volatile int g_leaseWanted = 0; // the lease opened at the last begin: if it is closed under us (an end/begin race), reopen it

#define IO_TICK_US 20000 // 20 ms: fine enough for the 50 ms state poll of the file fallback, nothing else needs it
#define LEASE_WAIT_US 1000000 // with the lease up the worker sleeps in select() this long at most: the housekeeping interval

static void *shell_io_thread(void *args)
{
    (void)args;
    uint64_t nextSlowTsc = 0, nextEnabledTsc = 0, nextBeatTsc = 0;
    for (;;) {
        if (!g_ioActive) {
            if (g_leaseUp) shell_lease_close();   // the send loop stopped (disabled): drop the listener and every connection, so the plugins see it
            nextSlowTsc = nextEnabledTsc = nextBeatTsc = 0;
            usleep(IO_TICK_US);
            continue;
        }
        if (g_leaseWanted && !g_leaseUp) shell_lease_open();
        if (g_leaseUp) shell_lease_wait(LEASE_WAIT_US); // sleeps until a claim, a release or a connect arrives, or the housekeeping timeout
        else usleep(IO_TICK_US);
        if (!g_ioActive) continue;
        uint64_t now = rdtsc17();
        if (!g_leaseUp) shell_state_io_poll(now, g_tscHz); // fallback: the claim file, 20 Hz
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
            if (!g_leaseUp) { // with the lease up, "the shell feed is running" is the listener itself: no heartbeat file
                t0 = sceKernelGetProcessTimeCounter();
                shell_heartbeat(now, g_tscHz, &nextBeatTsc, g_shLoopTicks); // the loop's tick count: it stops changing if the loop stalls
                shell_fs_slow_note("heartbeat", t0);
            }
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
    // The lease listener opens here, before the first tick; the worker takes it over once g_ioActive is set. A game that was already
    // running reconnects within LEASE_RETRY_MS and claims; if the listener cannot open, the claim file is the channel as before.
    g_leaseWanted = shell_lease_open();
    if (!g_leaseWanted) shell_state_io_poll(rdtsc17(), g_tscHz); // first claim bit read here, so the very first tick already knows who owns the strip
    g_shIoRunning = 1;
    g_ioActive = 1;
}

void shell_io_end(void)
{
    g_ioActive = 0;
    g_leaseWanted = 0;
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
