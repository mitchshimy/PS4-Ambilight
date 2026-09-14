#ifndef DDP_H
#define DDP_H

#include <stdint.h>

#define DDP_HEADER_SIZE 10
#define DDP_MAX_ZONES 512

// Sends numZones RGB triplets (in whatever wire order the caller has
// already arranged them in -- see colorpipeline_write_color_ordered)
// to host:port via a single DDP UDP packet. Wire format copied
// verbatim from ps4_ambient_light's wled_send_rgb_zones -- same
// header bytes, same offset-0 behavior, so a real WLED controller
// treats this exactly like a frame from the plugin itself.
void ddp_send_rgb_zones(const char *host, uint16_t port, const uint8_t *rgbTriplets, int numZones);

#endif // DDP_H
