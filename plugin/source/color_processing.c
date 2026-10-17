// color_processing.c -- part of ps4_ambient_light, split out of the original single-file main.c....
// More: docs/code-comments.md (plugin/source/color_processing.c:1)

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "plugin_common.h"
#include "ambient_internal.h"

// v2.2 color processing : gamma -> brightness -> contrast -> saturation -> levels -> color order.
// More: docs/code-comments.md (plugin/source/color_processing.c:21)

// v2.2: brightness/contrast/saturation below all take and return UNCLAMPED int32_t, not uint8_t, and...
// More: docs/code-comments.md (plugin/source/color_processing.c:44)

static int32_t applyBrightness(int32_t c, uint32_t brightness)
{
    return (c * (int32_t)brightness) / 255;
}

// v2.2: per-channel brightness multiplier, ported from Android's (brightness/100) *...
// More: docs/code-comments.md (plugin/source/color_processing.c:67)
static int32_t applyChannelBrightnessPct(int32_t c, uint32_t pct)
{
    if (pct == 100) return c; // no-op fast path
    return (c * (int32_t)pct) / 100;
}

// v2.2: contrast, ported from Android's `128 + (c-128)*cf` with cf = 1 + contrast/100.
// More: docs/code-comments.md (plugin/source/color_processing.c:78)
static void applyContrast(int32_t r, int32_t g, int32_t b, int32_t contrast,
                           int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (contrast == 0) { *outR = r; *outG = g; *outB = b; return; }
    *outR = 128 + ((r - 128) * (100 + contrast)) / 100;
    *outG = 128 + ((g - 128) * (100 + contrast)) / 100;
    *outB = 128 + ((b - 128) * (100 + contrast)) / 100;
}

// v2.2: input/output are unclamped int32_t now (see the big comment above applyBrightness) -- a...
// More: docs/code-comments.md (plugin/source/color_processing.c:91)
static void applySaturation(int32_t r, int32_t g, int32_t b, int32_t sat,
                             int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (sat == 0) { *outR = r; *outG = g; *outB = b; return; }
    // Integer luma using the real BT.601 weights (Android uses 0.299/0.587/0.114 as floats; /1000 here...
    // More: docs/code-comments.md (plugin/source/color_processing.c:100)
    int32_t luma = (r * 299 + g * 587 + b * 114) / 1000;
    *outR = luma + ((r - luma) * (100 + sat)) / 100;
    *outG = luma + ((g - luma) * (100 + sat)) / 100;
    *outB = luma + ((b - luma) * (100 + sat)) / 100;
}

// Writes the 3 output bytes in the configured wire order (most WS2812B/NeoPixel strips are GRB, not...
// More: docs/code-comments.md (plugin/source/color_processing.c:111)
static void writeColorOrdered(uint8_t r, uint8_t g, uint8_t b, ColorOrder order, uint8_t *out3)
{
    switch (order) {
        case ORDER_RGB: out3[0]=r; out3[1]=g; out3[2]=b; break;
        case ORDER_RBG: out3[0]=r; out3[1]=b; out3[2]=g; break;
        case ORDER_GRB: out3[0]=g; out3[1]=r; out3[2]=b; break;
        case ORDER_GBR: out3[0]=g; out3[1]=b; out3[2]=r; break;
        case ORDER_BRG: out3[0]=b; out3[1]=r; out3[2]=g; break;
        default: /* ORDER_BGR */ out3[0]=b; out3[1]=g; out3[2]=r; break;
    }
}

// v2.1/v2.2: levels adjustment (black_level/white_level) -- a "stretch" of the 0-255 range, verified...
// More: docs/code-comments.md (plugin/source/color_processing.c:126)
static void applyLevels(int32_t r, int32_t g, int32_t b, uint32_t blackLevel, uint32_t whiteLevel,
                         uint8_t *outR, uint8_t *outG, uint8_t *outB)
{
    #define CLAMP255(c) (uint8_t)((c) < 0 ? 0 : ((c) > 255 ? 255 : (c)))
    if (blackLevel == 0 && whiteLevel == 100) {
        *outR = CLAMP255(r); *outG = CLAMP255(g); *outB = CLAMP255(b);
        return;
    }
    int32_t blackThresh = (int32_t)(blackLevel * 255 / 100);
    int32_t whiteThresh = (int32_t)(whiteLevel * 255 / 100);
    int32_t range = whiteThresh - blackThresh;
    if (range <= 0) { *outR = 0; *outG = 0; *outB = 0; return; } // degenerate config -- fail to black, not undefined
    #define STRETCH(c) CLAMP255(((c) < blackThresh) ? 0 : ((c) >= whiteThresh) ? 255 : \
                                  (((c) - blackThresh) * 255) / range)
    *outR = STRETCH(r);
    *outG = STRETCH(g);
    *outB = STRETCH(b);
    #undef STRETCH
    #undef CLAMP255
}

// Full per-zone pipeline, v2.2 order -- now matches the Android source exactly: gamma -> brightness ->...
// More: docs/code-comments.md (plugin/source/color_processing.c:160)
void applyColorProcessing(uint8_t r8, uint8_t g8, uint8_t b8, uint8_t *out3)
{
    // --- gamma (legacy global fixed-LUT stage; per-channel gamma_r/g/b already happened per-sample before...
    // More: docs/code-comments.md (plugin/source/color_processing.c:185)
    uint32_t gi = g_config.gammaLutIndex < NUM_GAMMA_LUTS ? g_config.gammaLutIndex : 0;
    int32_t r = kGammaLuts[gi][r8];
    int32_t g = kGammaLuts[gi][g8];
    int32_t b = kGammaLuts[gi][b8];

    // --- brightness: global (0-255 scale) then per-channel (Android's
    // 0-500pct scale) on top of it. Unclamped from here on. ---
    r = applyBrightness(r, g_config.brightness);
    g = applyBrightness(g, g_config.brightness);
    b = applyBrightness(b, g_config.brightness);
    r = applyChannelBrightnessPct(r, g_config.brightnessR);
    g = applyChannelBrightnessPct(g, g_config.brightnessG);
    b = applyChannelBrightnessPct(b, g_config.brightnessB);

    // --- contrast ---
    int32_t cr, cg, cb;
    applyContrast(r, g, b, g_config.contrast, &cr, &cg, &cb);

    // --- saturation ---
    int32_t sr, sg, sb;
    applySaturation(cr, cg, cb, g_config.saturation, &sr, &sg, &sb);

    // --- levels (black/white point) -- the ONE clamp+round back to a byte for this whole unclamped chain...
    // More: docs/code-comments.md (plugin/source/color_processing.c:212)
    uint8_t lr, lg, lb;
    applyLevels(sr, sg, sb, g_config.blackLevel, g_config.whiteLevel, &lr, &lg, &lb);

    writeColorOrdered(lr, lg, lb, g_config.colorOrder, out3);
}

// v2.1 dark_threshold state: per-zone hysteresis flag (see applyDarkThreshold below) -- separate from...
// More: docs/code-comments.md (plugin/source/color_processing.c:221)
static bool g_zoneIsDark[MAX_TOTAL_ZONES];

// Forces (r,g,b) to black if the zone should currently be considered "dark", with hysteresis to avoid...
// More: docs/code-comments.md (plugin/source/color_processing.c:226)
#define DARK_HYSTERESIS 10
void applyDarkThreshold(uint32_t zoneIdx, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (g_config.darkThreshold == 0) return; // disabled
    uint8_t luma = *r > *g ? (*r > *b ? *r : *b) : (*g > *b ? *g : *b);
    if (g_zoneIsDark[zoneIdx]) {
        if (luma >= g_config.darkThreshold + DARK_HYSTERESIS) g_zoneIsDark[zoneIdx] = false;
    } else if (luma < g_config.darkThreshold) {
        g_zoneIsDark[zoneIdx] = true;
    }
    if (g_zoneIsDark[zoneIdx]) { *r = 0; *g = 0; *b = 0; }
}

