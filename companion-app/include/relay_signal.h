#ifndef RELAY_SIGNAL_H
#define RELAY_SIGNAL_H

#include <stdint.h>
#include <stdbool.h>

// Reintroduced -- this app had this at v17 (ps4_ambient_light's own wled-relay hand-off signal, ported...
// More: docs/code-comments.md (companion-app/include/relay_signal.h:7)
bool relay_send_external_source(const char *host, uint16_t port, bool active);

#endif // RELAY_SIGNAL_H
