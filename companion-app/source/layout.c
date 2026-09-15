// layout.c -- see layout.h. Ported from LedLayoutGeometry.kt
// (calculateLedPositions + getEdgeOrder), kept as close to the
// original edge-by-edge structure as C allows so the two can still be
// compared side by side if the Android layout logic ever changes.

#include "layout.h"
#include <string.h>

typedef enum { EDGE_TOP_LR, EDGE_TOP_RL, EDGE_RIGHT_TB, EDGE_RIGHT_BT,
               EDGE_BOTTOM_RL, EDGE_BOTTOM_LR, EDGE_LEFT_BT, EDGE_LEFT_TB } Edge;

// Direct port of getEdgeOrder(): which side is walked first, and in
// which rotational direction, for each of the 4 physical start
// corners x 2 directions. Order here IS wire order -- pixel 0 on the
// real strip is the first LED of the first edge listed.
static int edge_order(StartCorner corner, LedDirection dir, Edge out[4])
{
    static const Edge kTopLeftCW[4]     = { EDGE_TOP_LR, EDGE_RIGHT_TB, EDGE_BOTTOM_RL, EDGE_LEFT_BT };
    static const Edge kTopLeftCCW[4]    = { EDGE_LEFT_TB, EDGE_BOTTOM_LR, EDGE_RIGHT_BT, EDGE_TOP_RL };
    static const Edge kTopRightCW[4]    = { EDGE_RIGHT_TB, EDGE_BOTTOM_RL, EDGE_LEFT_BT, EDGE_TOP_LR };
    static const Edge kTopRightCCW[4]   = { EDGE_TOP_RL, EDGE_LEFT_TB, EDGE_BOTTOM_LR, EDGE_RIGHT_BT };
    static const Edge kBotRightCW[4]    = { EDGE_BOTTOM_RL, EDGE_LEFT_BT, EDGE_TOP_LR, EDGE_RIGHT_TB };
    static const Edge kBotRightCCW[4]   = { EDGE_RIGHT_BT, EDGE_TOP_RL, EDGE_LEFT_TB, EDGE_BOTTOM_LR };
    static const Edge kBotLeftCW[4]     = { EDGE_LEFT_BT, EDGE_TOP_LR, EDGE_RIGHT_TB, EDGE_BOTTOM_RL };
    static const Edge kBotLeftCCW[4]    = { EDGE_BOTTOM_LR, EDGE_RIGHT_BT, EDGE_TOP_RL, EDGE_LEFT_TB };

    const Edge *src;
    switch (corner) {
        case CORNER_TOP_LEFT:     src = (dir == DIR_CLOCKWISE) ? kTopLeftCW  : kTopLeftCCW;  break;
        case CORNER_TOP_RIGHT:    src = (dir == DIR_CLOCKWISE) ? kTopRightCW : kTopRightCCW; break;
        case CORNER_BOTTOM_RIGHT: src = (dir == DIR_CLOCKWISE) ? kBotRightCW : kBotRightCCW; break;
        case CORNER_BOTTOM_LEFT:  src = (dir == DIR_CLOCKWISE) ? kBotLeftCW  : kBotLeftCCW;  break;
        default:                  src = kTopLeftCW; break;
    }
    memcpy(out, src, sizeof(Edge) * 4);
    return 4;
}

static float step_of(float length, int count)
{
    return (count <= 1) ? 0.0f : (length / (float)(count - 1));
}

int layout_build(const AmbientConfig *cfg, LedSlot *outSlots, int maxSlots,
                  float screenW, float screenH, float padding)
{
    if (!cfg || !outSlots || maxSlots <= 0) return 0;

    int topCount    = (int)cfg->ledCountTop;
    int rightCount  = (int)cfg->ledCountRight;
    int bottomCount = (int)cfg->ledCountBottom;
    int leftCount   = (int)cfg->ledCountLeft;
    if (topCount < 0) topCount = 0;
    if (rightCount < 0) rightCount = 0;
    if (bottomCount < 0) bottomCount = 0;
    if (leftCount < 0) leftCount = 0;

    float innerW = screenW - padding * 2.0f;
    float innerH = screenH - padding * 2.0f;

    // scanDepth is stored as a raw percent (1-50, same as the Android
    // schema); clamp defensively since it's user-editable.
    float depthPct = (float)cfg->scanDepth;
    if (depthPct < 1.0f) depthPct = 1.0f;
    if (depthPct > 50.0f) depthPct = 50.0f;
    float scanDepthV = innerH * depthPct / 100.0f; if (scanDepthV < 2.0f) scanDepthV = 2.0f;
    float scanDepthH = innerW * depthPct / 100.0f; if (scanDepthH < 2.0f) scanDepthH = 2.0f;

    float stepTop    = step_of(innerW, topCount);
    float stepRight   = step_of(innerH, rightCount);
    float stepBottom = step_of(innerW, bottomCount);
    float stepLeft    = step_of(innerH, leftCount);

    Edge edges[4];
    edge_order(cfg->startCorner, cfg->direction, edges);

    int n = 0;
    for (int e = 0; e < 4 && n < maxSlots && n < LAYOUT_MAX_LEDS; e++) {
        switch (edges[e]) {
            case EDGE_TOP_LR:
                for (int i = 0; i < topCount && n < maxSlots && n < LAYOUT_MAX_LEDS; i++, n++) {
                    float x = (topCount <= 1) ? padding + innerW / 2.0f : padding + i * stepTop + stepTop / 2.0f;
                    outSlots[n] = (LedSlot){ x, padding + scanDepthV / 2.0f, stepTop, scanDepthV, ZONE_TOP };
                }
                break;
            case EDGE_TOP_RL:
                for (int i = 0; i < topCount && n < maxSlots && n < LAYOUT_MAX_LEDS; i++, n++) {
                    int idx = topCount - 1 - i;
                    float x = (topCount <= 1) ? padding + innerW / 2.0f : padding + idx * stepTop + stepTop / 2.0f;
                    outSlots[n] = (LedSlot){ x, padding + scanDepthV / 2.0f, stepTop, scanDepthV, ZONE_TOP };
                }
                break;
            case EDGE_RIGHT_TB:
                for (int i = 0; i < rightCount && n < maxSlots && n < LAYOUT_MAX_LEDS; i++, n++) {
                    float y = (rightCount <= 1) ? padding + innerH / 2.0f : padding + i * stepRight + stepRight / 2.0f;
                    outSlots[n] = (LedSlot){ padding + innerW - scanDepthH / 2.0f, y, scanDepthH, stepRight, ZONE_RIGHT };
                }
                break;
            case EDGE_RIGHT_BT:
                for (int i = 0; i < rightCount && n < maxSlots && n < LAYOUT_MAX_LEDS; i++, n++) {
                    int idx = rightCount - 1 - i;
                    float y = (rightCount <= 1) ? padding + innerH / 2.0f : padding + idx * stepRight + stepRight / 2.0f;
                    outSlots[n] = (LedSlot){ padding + innerW - scanDepthH / 2.0f, y, scanDepthH, stepRight, ZONE_RIGHT };
                }
                break;
            case EDGE_BOTTOM_RL:
                for (int i = 0; i < bottomCount && n < maxSlots && n < LAYOUT_MAX_LEDS; i++, n++) {
                    int idx = bottomCount - 1 - i;
                    float x = (bottomCount <= 1) ? padding + innerW / 2.0f : padding + idx * stepBottom + stepBottom / 2.0f;
                    outSlots[n] = (LedSlot){ x, padding + innerH - scanDepthV / 2.0f, stepBottom, scanDepthV, ZONE_BOTTOM };
                }
                break;
            case EDGE_BOTTOM_LR:
                for (int i = 0; i < bottomCount && n < maxSlots && n < LAYOUT_MAX_LEDS; i++, n++) {
                    float x = (bottomCount <= 1) ? padding + innerW / 2.0f : padding + i * stepBottom + stepBottom / 2.0f;
                    outSlots[n] = (LedSlot){ x, padding + innerH - scanDepthV / 2.0f, stepBottom, scanDepthV, ZONE_BOTTOM };
                }
                break;
            case EDGE_LEFT_BT:
                for (int i = 0; i < leftCount && n < maxSlots && n < LAYOUT_MAX_LEDS; i++, n++) {
                    int idx = leftCount - 1 - i;
                    float y = (leftCount <= 1) ? padding + innerH / 2.0f : padding + idx * stepLeft + stepLeft / 2.0f;
                    outSlots[n] = (LedSlot){ padding + scanDepthH / 2.0f, y, scanDepthH, stepLeft, ZONE_LEFT };
                }
                break;
            case EDGE_LEFT_TB:
                for (int i = 0; i < leftCount && n < maxSlots && n < LAYOUT_MAX_LEDS; i++, n++) {
                    float y = (leftCount <= 1) ? padding + innerH / 2.0f : padding + i * stepLeft + stepLeft / 2.0f;
                    outSlots[n] = (LedSlot){ padding + scanDepthH / 2.0f, y, scanDepthH, stepLeft, ZONE_LEFT };
                }
                break;
        }
    }

    // ledOffset rotation -- same as the Android preview's own
    // "Apply same offset in visualization so the numbering matches"
    // step, so outSlots[0] always corresponds to the pixel the real
    // strip's controller treats as index 0, even if the user's strip
    // physically starts partway around the frame from their chosen
    // startCorner.
    if (n > 0 && cfg->ledOffset != 0) {
        int offset = ((cfg->ledOffset % n) + n) % n;
        if (offset != 0) {
            LedSlot rotated[LAYOUT_MAX_LEDS];
            for (int i = 0; i < n; i++) rotated[(i + offset) % n] = outSlots[i];
            memcpy(outSlots, rotated, sizeof(LedSlot) * n);
        }
    }

    return n;
}

float layout_hue_for_slot(const LedSlot *slot, float screenW, float screenH, float padding)
{
    if (!slot) return 0.0f;

    float innerW = screenW - padding * 2.0f;
    float innerH = screenH - padding * 2.0f;
    float perimeter = 2.0f * innerW + 2.0f * innerH;
    if (perimeter <= 0.0f) return 0.0f;

    float x = slot->cx - padding;
    float y = slot->cy - padding;
    float pos;
    switch (slot->zone) {
        case ZONE_TOP:    pos = x; break;
        case ZONE_RIGHT:  pos = innerW + y; break;
        case ZONE_BOTTOM: pos = innerW + innerH + (innerW - x); break;
        case ZONE_LEFT:   pos = 2.0f * innerW + innerH + (innerH - y); break;
        default:          pos = 0.0f; break;
    }
    if (pos < 0.0f) pos = 0.0f;
    if (pos > perimeter) pos = perimeter;
    return 360.0f * pos / perimeter;
}
