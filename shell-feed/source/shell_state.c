// shell_state.c -- the foreground handshake with the game plugin.
// More: docs/code-comments.md (shell-feed/source/shell_state.c:1)
#include "shell_internal.h"

// Heartbeat for the game plugin: a fixed-width, ever-changing counter rewritten about once a second...
// More: docs/code-comments.md (shell-feed/source/shell_state.c:6)
#define SHELL_ALIVE_PATH AMBILIGHT_DATA_DIR "/ps4_shell_alive"
void shell_heartbeat(uint64_t nowTsc, uint64_t tscHz, uint64_t *nextBeatTsc, uint64_t counter)
{
    if (tscHz == 0 || nowTsc < *nextBeatTsc) return;
    *nextBeatTsc = nowTsc + tscHz; // 1 s
    char text[32];
    int len = snprintf(text, sizeof(text), "%020llu\n", (unsigned long long)counter);
    if (len <= 0) return;
    int32_t fd = sceKernelOpen(SHELL_ALIVE_PATH, 0x0001 | 0x0200, 0777);
    if (fd < 0) return;
    (void)sceKernelWrite(fd, text, (size_t)len);
    sceKernelClose(fd);
}

// [v25] Simple foreground-bit gate.
// More: docs/code-comments.md (shell-feed/source/shell_state.c:24)

static int state_read_foreground(int *fgOut)
{
    int32_t fd = sceKernelOpen(STATE_PATH, 0 /* O_RDONLY */, 0);
    if (fd < 0) return -1;
    uint8_t buf[STATE_RECORD_LEN + 4];
    int32_t n = sceKernelRead(fd, buf, sizeof(buf));
    sceKernelClose(fd);
    if (n < 8) return -1;
    if (buf[0] != 'v' || buf[1] != '1' || buf[2] != ' ' ||
        buf[3] != 'f' || buf[4] != 'g' || buf[5] != '=') return -1;
    int fg = buf[6] - '0';
    if ((fg != 0 && fg != 1) || buf[7] != ' ') return -1;
    *fgOut = fg;
    return 0;
}

static uint64_t g_claimNextPollTsc = 0;
int      g_claimEverSeen = 0;
int      g_claimActive = 0;
static int      g_stateForeground = 0;
uint64_t g_liveStartTsc = 0;

// Returns 1 when the game state says foreground and shell feed must back off.
// File I/O is rate-limited to 10 Hz; the live-send loop only does a TSC comparison otherwise.
int claim_poll(uint64_t nowTsc, uint64_t tscHz)
{
    if (nowTsc >= g_claimNextPollTsc) {
        g_claimNextPollTsc = nowTsc + (tscHz > 0 ? (tscHz * STATE_POLL_MS / 1000) : 0);
        int fg;
        if (state_read_foreground(&fg) == 0) {
            static int handoffDiagFirstState = 0;
            if (!handoffDiagFirstState) {
                handoffDiagFirstState = 1;
                shell_event("HANDOFF_DIAG SHELL_FIRST_VALID_STATE fg=%d tsc=%llu hz=%llu", fg,
                       (unsigned long long)sceKernelGetProcessTimeCounter(),
                       (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
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
