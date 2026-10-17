// wled-relay hand-off, personal and opt-in (relay_signal_enabled + relay_host): "on" while the loop runs, repeated every 5 s;
// "off" on a clean stop unless a game holds fg=1, in which case the plugin sends it later. [docs/code-comments.md]
#include "shell_internal.h"

static int      g_relayHeld = 0;   // we have sent "on" and not yet "off"
static char     g_relayHost[sizeof(g_config.relayHost)];
static uint16_t g_relayPort = 0;
static uint64_t g_relayNextResendTsc = 0;
#define RELAY_RESEND_SEC 5

static void relay_send(const char *host, uint16_t port, int on)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;
    struct sockaddr_in d; memset(&d, 0, sizeof(d));
    d.sin_family = AF_INET; d.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &d.sin_addr) == 1) {
        const char *payload = on ? "on" : "off";
        sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&d, sizeof(d));
    }
    close(sock);
}

// Call with shellActive=1 when the send loop starts and after every ini reload, 0 when it stops.
// More: docs/code-comments.md (shell-feed/source/shell_relay.c:42)
void shell_relay_update(int shellActive)
{
    int want = shellActive && g_config.relaySignalEnabled && g_config.relayHost[0] != '\0';
    uint16_t port = g_config.relayPort;

    if (g_relayHeld && (!want || port != g_relayPort || strcmp(g_config.relayHost, g_relayHost) != 0)) {
        if (g_claimActive) {
            shell_event("RELAY: not sending off, a game holds the strip");
        } else {
            relay_send(g_relayHost, g_relayPort, 0);
            shell_event("RELAY: off to %s:%u", g_relayHost, (unsigned)g_relayPort);
        }
        g_relayHeld = 0;
    }
    if (want && !g_relayHeld) {
        strncpy(g_relayHost, g_config.relayHost, sizeof(g_relayHost) - 1);
        g_relayHost[sizeof(g_relayHost) - 1] = '\0';
        g_relayPort = port;
        relay_send(g_relayHost, g_relayPort, 1);
        g_relayHeld = 1;
        g_relayNextResendTsc = 0; // re-armed by the first shell_relay_tick()
        shell_event("RELAY: on to %s:%u", g_relayHost, (unsigned)g_relayPort);
    }
}

// Called every tick of the send loop (cheap: one compare). Repeats "on" while this feed holds the relay down.
void shell_relay_tick(uint64_t nowTsc, uint64_t tscHz)
{
    if (!g_relayHeld || tscHz == 0) return;
    if (g_relayNextResendTsc == 0) { g_relayNextResendTsc = nowTsc + (uint64_t)RELAY_RESEND_SEC * tscHz; return; }
    if (nowTsc < g_relayNextResendTsc) return;
    g_relayNextResendTsc = nowTsc + (uint64_t)RELAY_RESEND_SEC * tscHz;
    relay_send(g_relayHost, g_relayPort, 1);
}
