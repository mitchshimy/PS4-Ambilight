// led_frame.c -- see led_frame.h.

#include "led_frame.h"
#include "ui_theme.h"

#include <stddef.h>
#include <stdbool.h>

#define TICK      7.0f
#define ORIGIN    11.0f

// A muted, unmistakably-not-real-data placeholder for when there's no
// color to show yet (no rgbTriplets, or fewer than this LED's index).
// Deliberately desaturated so it can't be confused with an actual
// sent color, which is the whole point of this rewrite.
static const UiColor kNoDataColor = { 120, 122, 128, 140 };

void led_frame_draw(UiCanvas *c, const LedSlot *slots, int slotCount,
                     const uint8_t *rgbTriplets, int ledCount)
{
    if (!slots || slotCount <= 0) return;
    bool haveColors = (rgbTriplets != NULL && ledCount > 0);

    for (int i = 0; i < slotCount; i++) {
        float x = slots[i].cx, y = slots[i].cy;
        UiColor col = (haveColors && i < ledCount)
            ? ui_rgb(rgbTriplets[i * 3 + 0], rgbTriplets[i * 3 + 1], rgbTriplets[i * 3 + 2])
            : kNoDataColor;

        if (i == 0) {
            // LED 0 -- the wire's physical start -- still gets its own
            // real colour, with a small amber ring overlaid to mark it
            // as the origin (not a substitute for showing its color).
            // Only one of these is ever drawn per frame, so it stays
            // on the full AA path -- no performance reason to fast-
            // path something that happens once.
            UiRect o = ui_rect(x - ORIGIN * 0.5f, y - ORIGIN * 0.5f, ORIGIN, ORIGIN);
            ui_fill_round_rect(c, o, ui_radius_all(1.0f), col);
            UiRect halo = ui_rect(o.x - 2.0f, o.y - 2.0f, ORIGIN + 4.0f, ORIGIN + 4.0f);
            ui_stroke_round_rect(c, halo, ui_radius_all(1.0f), 2.0f, COL_BG0);
            ui_stroke_round_rect(c, o, ui_radius_all(1.0f), 2.0f, COL_AMBER);
        } else {
            // Every other tick uses the fast, unantialiased fill --
            // there can be several hundred of these drawn every single
            // animation frame while Test Strip is running, and at 7px
            // across the missing edge AA was never going to be visible
            // anyway. This was the single biggest per-frame cost left
            // after caching the scene background, since the full AA
            // path builds a small polygon and runs a 5x-subsampled
            // scanline pass for every tick, hundreds of times a frame.
            UiRect r = ui_rect(x - TICK * 0.5f, y - TICK * 0.5f, TICK, TICK);
            ui_fill_rect_fast(c, r, col);
        }
    }
}
