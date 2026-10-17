#ifndef DDP_H
#define DDP_H

#include <stdint.h>
#include <stdbool.h>

#define DDP_HEADER_SIZE 10
#define DDP_MAX_ZONES 512

// Sends numZones RGB triplets (in whatever wire order the caller has already arranged them in -- see...
// More: docs/code-comments.md (companion-app/include/ddp.h:10)
bool ddp_send_rgb_zones(const char *host, uint16_t port, const uint8_t *rgbTriplets, int numZones);

#endif // DDP_H
