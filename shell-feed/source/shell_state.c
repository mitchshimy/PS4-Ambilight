// shell_state.c -- the foreground handshake with the game plugin.
// More: docs/code-comments.md (shell-feed/source/shell_state.c:1)
#include "shell_internal.h"

// Heartbeat for the game plugin: a fixed-width, ever-changing counter rewritten about once a second...
// More: docs/code-comments.md (shell-feed/source/shell_state.c:6)
#define SHELL_ALIVE_PATH "/data/ps4_shell_alive" // directly in /data, not in our folder: a file rewritten every second must not share a folder with the ini
static int32_t  g_hbFd = -1;
static uint64_t g_hbOpenedTsc = 0;
#define HB_REOPEN_SEC 10
void shell_heartbeat(uint64_t nowTsc, uint64_t tscHz, uint64_t *nextBeatTsc, uint64_t counter)
{
    if (tscHz == 0 || nowTsc < *nextBeatTsc) return;
    *nextBeatTsc = nowTsc + tscHz; // 1 s
    char text[32];
    int len = snprintf(text, sizeof(text), "%020llu\n", (unsigned long long)counter);
    if (len <= 0) return;
    // The file stays open and is rewritten in place (same 21 bytes at offset 0), so a beat is one write and no directory lookup.
    // It is reopened every HB_REOPEN_SEC so a deleted or replaced file is recreated, and after any failed write.
    if (g_hbFd >= 0 && (nowTsc - g_hbOpenedTsc) > (uint64_t)HB_REOPEN_SEC * tscHz) { sceKernelClose(g_hbFd); g_hbFd = -1; }
    if (g_hbFd < 0) {
        g_hbFd = sceKernelOpen(SHELL_ALIVE_PATH, 0x0001 | 0x0200, 0777);
        if (g_hbFd < 0) return;
        g_hbOpenedTsc = nowTsc;
    }
    if (sceKernelLseek(g_hbFd, 0, 0 /* SEEK_SET */) < 0 || sceKernelWrite(g_hbFd, text, (size_t)len) != (ssize_t)len) {
        sceKernelClose(g_hbFd); g_hbFd = -1;
    }
}

// [v25] Simple foreground-bit gate.
// More: docs/code-comments.md (shell-feed/source/shell_state.c:24)

// The state file stays open and is re-read from offset 0 (the plugin rewrites it in place), so a poll is a seek and a read,
// not an open and close 20 times a second. Reopened every ST_REOPEN_SEC and after any failed read, so a replaced file is picked up.
static int32_t      g_stFd = -1;
static uint64_t     g_stOpenedTsc = 0;
static volatile char g_stBusy = 0;
#define ST_REOPEN_SEC 2
static int state_read_locked(int *fgOut, long *seqOut);
// -2 = another thread is inside a read (a worker stuck on the disk while the loop restarts): skip, never share the descriptor.
static int state_read_foreground(int *fgOut, long *seqOut)
{
    if (__atomic_test_and_set(&g_stBusy, __ATOMIC_ACQUIRE)) return -2;
    int rc = state_read_locked(fgOut, seqOut);
    __atomic_clear(&g_stBusy, __ATOMIC_RELEASE);
    return rc;
}
static int state_read_locked(int *fgOut, long *seqOut)
{
    uint64_t now = sceKernelGetProcessTimeCounter(), hz = sceKernelGetProcessTimeCounterFrequency();
    if (g_stFd >= 0 && hz != 0 && (now - g_stOpenedTsc) > (uint64_t)ST_REOPEN_SEC * hz) { sceKernelClose(g_stFd); g_stFd = -1; }
    if (g_stFd < 0) {
        g_stFd = sceKernelOpen(STATE_PATH, 0 /* O_RDONLY */, 0);
        if (g_stFd < 0) return -1;
        g_stOpenedTsc = now;
    }
    uint8_t buf[STATE_RECORD_LEN + 4];
    int32_t n = -1;
    if (sceKernelLseek(g_stFd, 0, 0 /* SEEK_SET */) >= 0) n = sceKernelRead(g_stFd, buf, sizeof(buf));
    if (n < 8) { sceKernelClose(g_stFd); g_stFd = -1; return -1; }
    if (buf[0] != 'v' || buf[1] != '1' || buf[2] != ' ' ||
        buf[3] != 'f' || buf[4] != 'g' || buf[5] != '=') return -1;
    int fg = buf[6] - '0';
    if ((fg != 0 && fg != 1) || buf[7] != ' ') return -1;
    *fgOut = fg;
    // "v1 fg=1 seq=000123 ...": the claim counter, or -1 if it does not parse (then no claim is ever called stale).
    long seq = 0;
    if (n >= 18 && memcmp(buf + 8, "seq=", 4) == 0) {
        for (int i = 12; i < 18 && seq >= 0; i++) seq = (buf[i] >= '0' && buf[i] <= '9') ? seq * 10 + (buf[i] - '0') : -1;
    } else seq = -1;
    *seqOut = seq;
    return 0;
}

// With the I/O worker running (shell_io.c), the state file is read THERE and published here as one packed word, so the
// send loop never waits on the filesystem for it. Without a worker, claim_poll() reads the file itself, as before.
volatile int g_shIoRunning = 0;
static uint64_t g_stSample = 0;              // bit 63 = valid read, bit 62 = fg, low 32 bits = seq (int32, -1 = none)
static uint64_t g_stNextIoPollTsc = 0;

static int state_sample(int *fgOut, long *seqOut)
{
    if (!g_shIoRunning) return state_read_foreground(fgOut, seqOut);
    uint64_t p = __atomic_load_n(&g_stSample, __ATOMIC_ACQUIRE);
    if (!(p >> 63)) return -1;
    *fgOut = (int)((p >> 62) & 1u);
    *seqOut = (long)(int32_t)(uint32_t)p;
    return 0;
}

// The file half, run by the I/O worker at twice the claim rate so the handoff sees a fresh bit about as soon as before.
void shell_state_io_poll(uint64_t nowTsc, uint64_t tscHz)
{
    if (tscHz == 0 || nowTsc < g_stNextIoPollTsc) return;
    g_stNextIoPollTsc = nowTsc + tscHz * (STATE_POLL_MS / 2) / 1000;
    int fg = 0; long seq = -1;
    uint64_t t0 = sceKernelGetProcessTimeCounter();
    int rc = state_read_foreground(&fg, &seq);
    shell_fs_slow_note("state_read", t0);
    if (rc == -2) return; // another thread is reading: keep the last published sample
    uint64_t packed = (rc == 0) ? ((1ULL << 63) | ((uint64_t)(fg & 1) << 62) | (uint64_t)(uint32_t)(int32_t)seq) : 0;
    __atomic_store_n(&g_stSample, packed, __ATOMIC_RELEASE);
}

static uint64_t g_claimNextPollTsc = 0;
int      g_claimEverSeen = 0;
int      g_claimActive = 0;
static int      g_stateForeground = 0;
// A game that died or was killed while in front leaves fg=1 on /data, and a fresh shell feed would back off for
// good. The first fg=1 seen after this load is treated as stale until its seq changes or fg=0 is read.
static long     g_staleSeq = -1;
uint64_t g_liveStartTsc = 0;

// Returns 1 when the game state says foreground and shell feed must back off.
// File I/O is rate-limited to 10 Hz; the live-send loop only does a TSC comparison otherwise.
int claim_poll(uint64_t nowTsc, uint64_t tscHz)
{
    if (g_leaseUp) {
        // The loopback lease (shell_lease.c): the worker publishes the number of connections holding a claim. No file, no stale
        // claim (a dead owner's socket is closed by the kernel), no rate limit: this is one atomic load.
        if (!g_claimEverSeen) {
            g_claimEverSeen = 1;
            shell_event("HANDOFF_DIAG SHELL_FIRST_VALID_STATE lease claims=%d tsc=%llu hz=%llu", shell_lease_claimed(),
                        (unsigned long long)sceKernelGetProcessTimeCounter(), (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
        }
        g_staleSeq = -1;
        g_stateForeground = shell_lease_claimed() > 0;
    } else if (nowTsc >= g_claimNextPollTsc) {
        g_claimNextPollTsc = nowTsc + (tscHz > 0 ? (tscHz * STATE_POLL_MS / 1000) : 0);
        int fg; long seq;
        if (state_sample(&fg, &seq) == 0) {
            static int handoffDiagFirstState = 0;
            if (!handoffDiagFirstState) {
                handoffDiagFirstState = 1;
                shell_event("HANDOFF_DIAG SHELL_FIRST_VALID_STATE fg=%d tsc=%llu hz=%llu", fg,
                       (unsigned long long)sceKernelGetProcessTimeCounter(),
                       (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
            }
            if (fg == 0) {
                g_staleSeq = -1;
            } else if (!g_claimEverSeen && seq >= 0) {
                g_staleSeq = seq;
                shell_event("STALE_CLAIM fg=1 seq=%ld was already set at load; ignored until it changes", seq);
            }
            if (fg == 1 && g_staleSeq >= 0) {
                if (seq == g_staleSeq) fg = 0;       // still the old claim
                else g_staleSeq = -1;                // a new claim: real
            }
            g_claimEverSeen = 1;
            g_stateForeground = fg;
        } else if (!g_claimEverSeen) {
            g_stateForeground = 0; // no valid state yet: shell feed is allowed
        }
    }

    int active = g_stateForeground;
    if (active != g_claimActive) {
        unsigned long long ms = (tscHz > 0 && nowTsc >= g_liveStartTsc)
            ? (unsigned long long)((nowTsc - g_liveStartTsc) * 1000ULL / tscHz) : 0ULL;
        char mir[96];
        if (active) {
            printf(TAG " FOREGROUND GATE ON: game state is fg=1; shell feed backs off (t=%llums)\n", ms);
            snprintf(mir, sizeof(mir), "FOREGROUND GATE ON t=%llums", ms);
        } else {
            printf(TAG " FOREGROUND GATE OFF: game state is fg=0 or not yet valid; shell feed may send (t=%llums)\n", ms);
            snprintf(mir, sizeof(mir), "FOREGROUND GATE OFF t=%llums", ms);
        }
        sh_dev_send(mir);
        g_claimActive = active;
    }
    return active;
}
