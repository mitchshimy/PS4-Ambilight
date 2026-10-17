// shell_relay.c -- the shell feed's half of the wled-relay hand-off. Personal, opt-in: it does nothing unless the
// ini has relay_signal_enabled=true AND a relay_host (both default to off/blank, so nobody else's console ever
// sends a relay packet from here).
//
// Who holds the strip, as the relay should see it. The relay is told "on" (something else is driving the strip,
// stand down) or "off" (nobody is, take over). Two things can drive it:
//   - the shell feed, from the moment its send loop starts until that loop stops (the heartbeat file proves this
//     to the game plugin: it is written from the same loop), and
//   - a game (or the companion app), whose fg=1 claim in ps4_ambient_state this feed already reads.
// The relay stands down while EITHER holds, so "off" goes out only when the last holder lets go:
//   - this feed starts or resumes sending                    -> "on"
//   - this feed stops cleanly (disable file, [shell] enabled=false) -> "off", unless a game claims fg=1; the
//     game then holds it, and the plugin sends the "off" itself when it backgrounds or exits, because by then it
//     sees this feed's heartbeat is gone (plugin/source/network.c, relay_release_external_source()).
//   - a game backgrounding while this feed is running            -> nothing here: the plugin sends no "off"
//     while it sees the heartbeat, so the relay stays down.
// While held, "on" is repeated every RELAY_RESEND_SEC: the relay clears its flag itself when HA turns gaming
// mode off (tv_gate.py's backstop), and a one-shot "on" would never be asserted again. The relay treats a
// repeat as a no-op.
// A crash or a power-off cannot send anything; the relay's own gaming_mode backstop has to cover that.
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

// Call with shellActive=1 when the send loop starts and after every ini reload, 0 when it stops. Sends only on
// a change, so calling it often is fine. Reads relay_signal_enabled / relay_host / relay_port live, so editing
// them while running does the right thing (off to the old address, on to the new one).
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
