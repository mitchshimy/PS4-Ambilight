// led_frame.h -- the blueprint's #led-frame, showing what's actually being sent to the strip.
// More: docs/code-comments.md (companion-app/source/led_frame.h:1)

#ifndef LED_FRAME_H
#define LED_FRAME_H

#include "ui_canvas.h"
#include "layout.h"
#include <stdint.h>

// slots/slotCount are the exact LedSlot array and count layout_build() produced -- the same one used...
// More: docs/code-comments.md (companion-app/source/led_frame.h:25)
void led_frame_draw(UiCanvas *c, const LedSlot *slots, int slotCount,
                     const uint8_t *rgbTriplets, int ledCount);

#endif // LED_FRAME_H
