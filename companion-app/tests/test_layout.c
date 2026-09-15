// test_layout.c -- pure-logic isolation test for layout.c, same
// convention as test_pipeline.c/test_settings.c: gcc -Iinclude -o
// test_layout test_layout.c source/layout.c && ./test_layout

#include <stdio.h>
#include "include/layout.h"

static AmbientConfig make_cfg(uint32_t top, uint32_t right, uint32_t bottom, uint32_t left,
                               StartCorner corner, LedDirection dir, int32_t ledOffset, uint32_t scanDepth)
{
    AmbientConfig cfg = {0};
    cfg.ledCountTop = top;
    cfg.ledCountRight = right;
    cfg.ledCountBottom = bottom;
    cfg.ledCountLeft = left;
    cfg.startCorner = corner;
    cfg.direction = dir;
    cfg.ledOffset = ledOffset;
    cfg.scanDepth = scanDepth;
    return cfg;
}

int main(void)
{
    int fails = 0;

    // 1) Total count must equal the sum of all 4 sides -- this is the
    // direct regression test for the original bug: the old preview
    // only ever sent 5 swatches no matter what these counts were.
    {
        AmbientConfig cfg = make_cfg(10, 6, 10, 6, CORNER_BOTTOM_LEFT, DIR_CLOCKWISE, 0, 5);
        LedSlot slots[LAYOUT_MAX_LEDS];
        int n = layout_build(&cfg, slots, LAYOUT_MAX_LEDS, 960.0f, 560.0f, 26.0f);
        printf("total count: got %d, expect 32\n", n);
        if (n != 32) { printf("FAIL\n"); fails++; }
    }

    // 2) Zero-LED sides must not appear, and the four zones must
    // together account for every slot (nothing dropped, nothing
    // duplicated).
    {
        AmbientConfig cfg = make_cfg(8, 0, 8, 0, CORNER_TOP_LEFT, DIR_CLOCKWISE, 0, 5);
        LedSlot slots[LAYOUT_MAX_LEDS];
        int n = layout_build(&cfg, slots, LAYOUT_MAX_LEDS, 960.0f, 560.0f, 26.0f);
        int top = 0, right = 0, bottom = 0, left = 0;
        for (int i = 0; i < n; i++) {
            if (slots[i].zone == ZONE_TOP) top++;
            else if (slots[i].zone == ZONE_RIGHT) right++;
            else if (slots[i].zone == ZONE_BOTTOM) bottom++;
            else left++;
        }
        printf("zero-side zones: n=%d top=%d right=%d bottom=%d left=%d -- expect n=16 top=8 right=0 bottom=8 left=0\n",
               n, top, right, bottom, left);
        if (!(n == 16 && top == 8 && right == 0 && bottom == 8 && left == 0)) { printf("FAIL\n"); fails++; }
    }

    // 3) startCorner should determine which zone slot 0 belongs to,
    // e.g. TOP_LEFT + clockwise should start on the top edge.
    {
        AmbientConfig cfg = make_cfg(10, 5, 10, 5, CORNER_TOP_LEFT, DIR_CLOCKWISE, 0, 5);
        LedSlot slots[LAYOUT_MAX_LEDS];
        int n = layout_build(&cfg, slots, LAYOUT_MAX_LEDS, 960.0f, 560.0f, 26.0f);
        printf("top_left/cw slot0 zone: got %d, expect ZONE_TOP(%d)\n", slots[0].zone, ZONE_TOP);
        if (n <= 0 || slots[0].zone != ZONE_TOP) { printf("FAIL\n"); fails++; }
    }

    // 4) startCorner=BOTTOM_RIGHT should start on the bottom edge.
    {
        AmbientConfig cfg = make_cfg(10, 5, 10, 5, CORNER_BOTTOM_RIGHT, DIR_CLOCKWISE, 0, 5);
        LedSlot slots[LAYOUT_MAX_LEDS];
        int n = layout_build(&cfg, slots, LAYOUT_MAX_LEDS, 960.0f, 560.0f, 26.0f);
        printf("bottom_right/cw slot0 zone: got %d, expect ZONE_BOTTOM(%d)\n", slots[0].zone, ZONE_BOTTOM);
        if (n <= 0 || slots[0].zone != ZONE_BOTTOM) { printf("FAIL\n"); fails++; }
    }

    // 5) ledOffset rotation should be a pure permutation -- same set
    // of positions, just starting at a different index -- and offset
    // N (== total count) should be a full-circle no-op.
    {
        AmbientConfig cfgBase = make_cfg(10, 6, 10, 6, CORNER_TOP_LEFT, DIR_CLOCKWISE, 0, 5);
        AmbientConfig cfgRot  = cfgBase; cfgRot.ledOffset = 3;
        AmbientConfig cfgFull = cfgBase; cfgFull.ledOffset = 32; // == total count

        LedSlot base[LAYOUT_MAX_LEDS], rot[LAYOUT_MAX_LEDS], full[LAYOUT_MAX_LEDS];
        int nBase = layout_build(&cfgBase, base, LAYOUT_MAX_LEDS, 960.0f, 560.0f, 26.0f);
        int nRot  = layout_build(&cfgRot,  rot,  LAYOUT_MAX_LEDS, 960.0f, 560.0f, 26.0f);
        int nFull = layout_build(&cfgFull, full, LAYOUT_MAX_LEDS, 960.0f, 560.0f, 26.0f);

        int rotOk = (nBase == nRot) && (base[0].cx == rot[3].cx) && (base[0].cy == rot[3].cy);
        int fullOk = (nBase == nFull);
        for (int i = 0; fullOk && i < nBase; i++) {
            if (base[i].cx != full[i].cx || base[i].cy != full[i].cy) fullOk = 0;
        }
        printf("ledOffset=3 permutes correctly: %s\n", rotOk ? "yes" : "no");
        printf("ledOffset==count is a no-op: %s\n", fullOk ? "yes" : "no");
        if (!rotOk || !fullOk) { printf("FAIL\n"); fails++; }
    }

    // 6) Positions must stay roughly within the [padding, dim-padding]
    // box on both axes. Tolerance is half a cell's pitch, not zero:
    // the upstream Android formula (padding + i*step + step/2, ported
    // unchanged here on purpose to match the phone app's own preview)
    // centers each LED's box on a fencepost grid, so the very last
    // LED on a densely-packed edge can sit up to half a cell-width
    // past the nominal edge -- a pre-existing cosmetic quirk in the
    // reference implementation, not something introduced by this
    // port. A real overflow (positions wildly outside the box) would
    // still be caught by this check.
    {
        AmbientConfig cfg = make_cfg(12, 8, 12, 8, CORNER_TOP_RIGHT, DIR_COUNTERCLOCKWISE, 0, 10);
        LedSlot slots[LAYOUT_MAX_LEDS];
        float w = 960.0f, h = 560.0f, pad = 26.0f;
        int n = layout_build(&cfg, slots, LAYOUT_MAX_LEDS, w, h, pad);
        int inBounds = 1;
        for (int i = 0; i < n; i++) {
            float tol = (slots[i].w > slots[i].h ? slots[i].w : slots[i].h) / 2.0f + 1.0f;
            if (slots[i].cx < pad - tol || slots[i].cx > w - pad + tol ||
                slots[i].cy < pad - tol || slots[i].cy > h - pad + tol) {
                inBounds = 0;
                printf("  out of bounds: slot %d cx=%.1f cy=%.1f w=%.1f h=%.1f\n",
                       i, slots[i].cx, slots[i].cy, slots[i].w, slots[i].h);
                break;
            }
        }
        printf("all positions in bounds (+/- half a cell): %s\n", inBounds ? "yes" : "no");
        if (!inBounds) { printf("FAIL\n"); fails++; }
    }

    // 7) BUGFIX regression: real-hardware report was that changing
    // startCorner/direction/ledOffset rotated the on-screen preview
    // rectangle but produced zero visible change on the real strip.
    // Root cause was hue being assigned by array index (post-reorder)
    // instead of by the slot's actual (cx,cy,zone) position -- DDP
    // offset i is a fixed physical bulb, so a color that only depends
    // on i never changes no matter what gets reordered into that
    // slot. Two things must now both be true:
    //   (a) the SAME physical location gets the SAME hue regardless
    //       of startCorner/direction/ledOffset (hue is tied to
    //       position, not to where settings happen to place it in
    //       the array)
    //   (b) wire-offset 0's hue DOES change between two different
    //       startCorner settings (this is the actual user-visible
    //       fix -- a fixed physical bulb now shows a different color
    //       when you change these settings, which is the whole point
    //       of a *live* preview for tuning them)
    {
        AmbientConfig cfgA = make_cfg(20, 10, 20, 10, CORNER_BOTTOM_LEFT, DIR_CLOCKWISE, 0, 5);
        AmbientConfig cfgB = make_cfg(20, 10, 20, 10, CORNER_TOP_RIGHT, DIR_COUNTERCLOCKWISE, 7, 5);
        LedSlot slotsA[LAYOUT_MAX_LEDS], slotsB[LAYOUT_MAX_LEDS];
        float w = 960.0f, h = 560.0f, pad = 26.0f;
        int nA = layout_build(&cfgA, slotsA, LAYOUT_MAX_LEDS, w, h, pad);
        int nB = layout_build(&cfgB, slotsB, LAYOUT_MAX_LEDS, w, h, pad);

        // (a) find a slot in A and a slot in B that land at (nearly)
        // the same physical position but via different settings, and
        // confirm they get the same hue.
        int foundMatch = 0;
        for (int i = 0; i < nA && !foundMatch; i++) {
            for (int j = 0; j < nB; j++) {
                float dx = slotsA[i].cx - slotsB[j].cx;
                float dy = slotsA[i].cy - slotsB[j].cy;
                if (dx * dx + dy * dy < 0.01f && slotsA[i].zone == slotsB[j].zone) {
                    float hueA = layout_hue_for_slot(&slotsA[i], w, h, pad);
                    float hueB = layout_hue_for_slot(&slotsB[j], w, h, pad);
                    float diff = hueA - hueB; if (diff < 0) diff = -diff;
                    printf("same physical position (zone %d): hueA=%.2f hueB=%.2f (diff=%.4f)\n",
                           slotsA[i].zone, hueA, hueB, diff);
                    if (diff > 0.01f) { printf("FAIL: same position, different hue\n"); fails++; }
                    foundMatch = 1;
                    break;
                }
            }
        }
        if (!foundMatch) { printf("FAIL: no matching physical position found between A and B to compare\n"); fails++; }

        // (b) wire-offset 0 (the very first LED sent, DDP pixel 0 --
        // a fixed physical bulb on the real strip) must get a
        // different hue under the two different settings.
        float hueA0 = layout_hue_for_slot(&slotsA[0], w, h, pad);
        float hueB0 = layout_hue_for_slot(&slotsB[0], w, h, pad);
        float diff0 = hueA0 - hueB0; if (diff0 < 0) diff0 = -diff0;
        printf("wire-offset 0 hue under cfgA=%.2f vs cfgB=%.2f (diff=%.4f)\n", hueA0, hueB0, diff0);
        if (diff0 < 0.01f) {
            printf("FAIL: wire-offset 0's hue didn't change between different startCorner settings "
                   "-- this is the exact real-hardware bug (real strip shows no visible change)\n");
            fails++;
        }
    }

    if (fails) {
        printf("\n%d check(s) FAILED\n", fails);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
