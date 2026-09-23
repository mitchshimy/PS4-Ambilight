// led_frame.h -- the blueprint's #led-frame, showing what's actually
// being sent to the strip.
//
// One tick per physical LED, in real wire order, coloured with the
// literal bytes main.c is sending over DDP for that LED -- not a
// decorative approximation.
//
// Positions are passed in rather than computed here. An earlier
// version called layout_build() itself to get them, which meant every
// animation frame ran layout_build() twice -- once in main.c to get
// the positions used for the actual DDP send, and again in here,
// purely to re-derive the same positions a second time for drawing.
// Harmless for correctness (both calls agree, since they're driven by
// the same cfg), but wasted work on every one of the ~30 frames a
// second Test Strip's animation draws. main.c already has the
// positions it just computed; this just uses them.

#ifndef LED_FRAME_H
#define LED_FRAME_H

#include "ui_canvas.h"
#include "layout.h"
#include <stdint.h>

// slots/slotCount are the exact LedSlot array and count layout_build()
// produced -- the same one used to build the DDP frame, so tick i and
// rgbTriplets[i] always refer to the same physical LED, by
// construction, with no second position calculator that could drift
// out of sync with the first.
//
// rgbTriplets is ledCount*3 bytes, one RGB triplet per LED in that
// same order -- rgbTriplets[i*3 .. i*3+2] must be the same bytes sent
// over DDP for LED i. Pass NULL/0 for rgbTriplets/ledCount when
// nothing has been sent yet (or sending failed): every tick then draws
// as a dim, uncoloured placeholder rather than inventing a color that
// was never actually sent.
void led_frame_draw(UiCanvas *c, const LedSlot *slots, int slotCount,
                     const uint8_t *rgbTriplets, int ledCount);

#endif // LED_FRAME_H
