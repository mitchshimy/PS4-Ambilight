#ifndef DDP_H
#define DDP_H

#include <stdint.h>
#include <stdbool.h>

#define DDP_HEADER_SIZE 10
#define DDP_MAX_ZONES 512

// Sends numZones RGB triplets (in whatever wire order the caller has
// already arranged them in -- see colorpipeline_write_color_ordered)
// to host:port via a single DDP UDP packet. Wire format copied
// verbatim from ps4_ambient_light's wled_send_rgb_zones -- same
// header bytes, same offset-0 behavior, so a real WLED controller
// treats this exactly like a frame from the plugin itself.
//
// Returns false if host isn't a literal IPv4 address (inet_pton
// rejects hostnames -- e.g. "wled.local" typed into the on-screen
// keyboard instead of an IP) or the socket couldn't be created.
// Previously void: a bad host failed the same way every frame at
// 30fps with nothing ever telling the user why the light wasn't
// responding. Does NOT detect an unreachable-but-valid IP -- UDP
// sendto() doesn't report that synchronously.
bool ddp_send_rgb_zones(const char *host, uint16_t port, const uint8_t *rgbTriplets, int numZones);

#endif // DDP_H
