// shell_state.c -- the foreground handshake with the game plugin. Reads the plugin's state file
// (plugin/source/state.c writes it; docs/development/shell-feed-coordination.md) and writes the shell
// feed's own heartbeat file, which the plugin reads to tell a running shell feed from a missing one.
#include "shell_internal.h"

// Heartbeat for the game plugin: a fixed-width, ever-changing counter rewritten about once a second, so the plugin can
// tell a shell feed that is really running from one that is switched off, crashed, or not installed (it sees the value
// stop changing). Deliberately the same tiny open/write/close the state file uses, without O_TRUNC, and on the same
// 1 s cadence as the ini poll next to it, so it adds nothing the loop was not already doing every second.
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

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
// [v25] Simple foreground-bit gate. The shell reader does not infer ownership from seq
// changes, heartbeat freshness, or WLED send activity. A readable fg=1 backs shell feed off;
// a readable fg=0 releases it. Before any valid record, allow shell feed. After a valid record,
// retain the last foreground bit on read errors rather than failing open.

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
