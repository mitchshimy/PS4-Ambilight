#ifndef LAYOUT_H
#define LAYOUT_H

// Ported from the Android app's LedLayoutGeometry.kt (calculateLedPositions + getEdgeOrder) so the PS4...
// More: docs/code-comments.md (companion-app/include/layout.h:4)

#include <stdint.h>
#include <stdbool.h>
#include "color_pipeline.h"

#define LAYOUT_MAX_LEDS 512

typedef enum { ZONE_TOP, ZONE_RIGHT, ZONE_BOTTOM, ZONE_LEFT } LedZone;

typedef struct {
    float cx, cy;   // center position in the caller's coordinate space
    float w, h;      // rect size, same space
    LedZone zone;
} LedSlot;

// Fills outSlots (capacity maxSlots) with one entry per LED configured in cfg (ledCountTop +...
// More: docs/code-comments.md (companion-app/include/layout.h:32)
int layout_build(const AmbientConfig *cfg, LedSlot *outSlots, int maxSlots,
                  float screenW, float screenH, float padding);

// Returns a hue (0-360) derived purely from slot's (cx, cy, zone) -- its fixed physical-position...
// More: docs/code-comments.md (companion-app/include/layout.h:50)
float layout_hue_for_slot(const LedSlot *slot, float screenW, float screenH, float padding);

#endif // LAYOUT_H
