#ifndef RELAY_SIGNAL_H
#define RELAY_SIGNAL_H

#include <stdint.h>
#include <stdbool.h>

// Reintroduced -- this app had this at v17 (ps4_ambient_light's own
// wled-relay hand-off signal, ported over from the real plugin), then
// lost it when v18's UI rebuild started from a pre-v17 baseline. See
// AmbientConfig's relayHost/relayPort/relaySignalEnabled in
// color_pipeline.h and main.c's use of this function for the rest of
// the story.
//
// Sends a single one-shot UDP datagram to host:port carrying the bare
// ASCII text "on" or "off" (no header, no MQTT client needed) --
// wled-relay's own listener treats this exactly like the signal it
// already gets from the real plugin. Same "returns false on a bad
// host, does not detect unreachable-but-valid" caveat as
// ddp_send_rgb_zones -- see that function's own comment in ddp.h.
bool relay_send_external_source(const char *host, uint16_t port, bool active);

#endif // RELAY_SIGNAL_H
