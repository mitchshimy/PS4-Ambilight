#ifndef LAYOUT_H
#define LAYOUT_H

// Ported from the Android app's LedLayoutGeometry.kt
// (calculateLedPositions + getEdgeOrder) so the PS4 companion app's
// layout preview agrees with the phone app's own preview, and so the
// order LEDs are emitted in here is the same physical wire order the
// real strip expects -- this is what update_live_preview() was
// missing: it only ever built and sent 5 swatches at DDP pixel offset
// 0, so on any real installFor with more than 5 LEDs only the first
// few pixels (wherever offset 0 physically is) ever lit up, no matter
// how ledCountTop/Right/Bottom/Left were configured. This module
// builds one slot per *configured* LED, in the order they're actually
// wired, so the caller can both (a) send a full-strip buffer over DDP
// and (b) draw every LED at its real position around a TV outline,
// same as the phone app's LedVisualization.kt does.

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

// Fills outSlots (capacity maxSlots) with one entry per LED configured
// in cfg (ledCountTop + ledCountRight + ledCountBottom + ledCountLeft
// total, capped at LAYOUT_MAX_LEDS and maxSlots), in real physical
// wire order: outSlots[0] is the first pixel the strip receives,
// outSlots[1] the second, and so on -- startCorner/direction pick
// which edge is walked first and which way (mirrors
// getEdgeOrder()/calculateLedPositions() exactly), and ledOffset then
// rotates that order the same way the Android preview rotates its own
// array, so outSlots[0] still lines up with physical pixel 0 after
// accounting for a strip that doesn't start exactly at your chosen
// corner. Positions are laid out around a screenW x screenH rectangle
// inset by `padding` on every side, with each LED's depth-into-the-
// screen size driven by cfg->scanDepth (as a percent of that side's
// length, mirroring the Android version's scanDepthV/scanDepthH).
// Returns the number of slots written.
int layout_build(const AmbientConfig *cfg, LedSlot *outSlots, int maxSlots,
                  float screenW, float screenH, float padding);

// Returns a hue (0-360) derived purely from slot's (cx, cy, zone) --
// its fixed physical-position identity -- via distance walked around
// a fixed reference perimeter (always top L->R, right T->B, bottom
// R->L, left B->T; this reference has nothing to do with
// startCorner/direction, it only needs to be *some* consistent,
// unchanging parameterization). screenW/screenH/padding must match
// whatever was passed to layout_build() so positions map back
// correctly.
//
// This exists because a hue derived from a slot's position in
// outSlots[] (i.e. from array index, post-reorder) is wrong: DDP
// pixel-offset i always refers to the same physical bulb (WLED has no
// concept of startCorner/direction/ledOffset; it just lights bulb i
// with whatever the i'th triplet says), so a color assigned by index
// alone never changes when those settings reorder *which* physical
// location ends up at index i -- the reordering becomes invisible on
// the real strip even though it's genuinely happening and visibly
// correct in an on-screen preview drawn from the same outSlots[].
// Deriving hue from the slot's own position instead means a given
// physical bulb's color visibly changes as different physical
// locations get rotated into its wire-offset, which is what makes
// these settings something you can actually tune by eye against a
// real light.
float layout_hue_for_slot(const LedSlot *slot, float screenW, float screenH, float padding);

#endif // LAYOUT_H
