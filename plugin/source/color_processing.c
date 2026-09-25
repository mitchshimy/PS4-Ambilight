// color_processing.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. gamma->brightness->contrast->saturation->levels->order pipeline, plus dark-threshold hysteresis.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.

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

// ============================================================
// v2.2 color processing : gamma -> brightness ->
// contrast -> saturation -> levels -> color order. Most of this is
// still applied once per zone AFTER averaging, for the same
// cheapness argument as before -- EXCEPT the new per-channel
// gamma_r/g/b, which is applied per-sample BEFORE averaging (see
// sampleZoneAverage), because gamma is non-linear and this project's
// own comment claiming "averaging first then correcting matches the
// Android inspiration project" was simply wrong -- the real Android
// source (ColorProcessor.kt, called from e.g. ScreenEncoder before
// any zone/border cropping) corrects every pixel first, then
// averages. That distinction is invisible for linear ops (brightness,
// levels) but real for gamma, so only gamma moved.
//
// No libm anywhere in this path: the legacy global gamma is a LUT
// lookup (kGammaLuts, generated offline), the new per-channel gammas
// are LUTs too (g_gammaLutR/G/B, built from a libm-free fast-pow
// approximation at config load -- see ambient_rebuild_perchannel_
// gamma_luts above), saturation/contrast are pure integer math
// (verified in Python before porting), brightness is a plain integer
// scale.
// ============================================================

// ------------------------------------------------------------
// v2.2: brightness/contrast/saturation below all take and return
// UNCLAMPED int32_t, not uint8_t, and NONE of them clamp to 0-255
// internally. This matters, and isn't just a style choice: Android's
// ColorProcessor.kt keeps every intermediate value as an unclamped
// Float and only clamps+rounds once, at the very end of the whole
// pipeline. Clamping to a byte between every stage (the v2.0/v2.1
// behavior) throws away sign/overflow information that a LATER,
// luma-relative stage (saturation) actually needs -- e.g. a channel
// that contrast pushed to -37 is meaningfully different input to
// saturation's luma math than a channel clamped to 0 first. Verified
// in Python before porting: on ordinary test inputs this produced
// final answers up to ~13/255 away from Android's real result, not
// mere rounding noise. The only clamp+round in this
// whole chain is in applyColorProcessing, right before the wire-order
// write.
// ------------------------------------------------------------

static int32_t applyBrightness(int32_t c, uint32_t brightness)
{
    return (c * (int32_t)brightness) / 255;
}

// v2.2: per-channel brightness multiplier, ported from Android's
// (brightness/100) * (brightnessChannel/100) combined factor. Kept as
// its own integer scale (0-500, 100=neutral) separate from the 0-255
// global `applyBrightness` above rather than trying to unify the two
// scales -- see the AmbientConfig struct comment for why.
static int32_t applyChannelBrightnessPct(int32_t c, uint32_t pct)
{
    if (pct == 100) return c; // no-op fast path
    return (c * (int32_t)pct) / 100;
}

// v2.2: contrast, ported from Android's `128 + (c-128)*cf` with
// cf = 1 + contrast/100. Uses this file's usual "0 = unchanged"
// convention (same as saturation), NOT Android's raw "100 = unchanged"
// percentage -- see the AmbientConfig struct comment.
static void applyContrast(int32_t r, int32_t g, int32_t b, int32_t contrast,
                           int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (contrast == 0) { *outR = r; *outG = g; *outB = b; return; }
    *outR = 128 + ((r - 128) * (100 + contrast)) / 100;
    *outG = 128 + ((g - 128) * (100 + contrast)) / 100;
    *outB = 128 + ((b - 128) * (100 + contrast)) / 100;
}

// v2.2: input/output are unclamped int32_t now (see the big comment
// above applyBrightness) -- a channel that arrived negative (from
// contrast) or >255 (from a >100% per-channel brightness) still needs
// to pull the luma calculation in that same, un-clamped direction, or
// this stage disagrees with what Android's real float math does.
static void applySaturation(int32_t r, int32_t g, int32_t b, int32_t sat,
                             int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (sat == 0) { *outR = r; *outG = g; *outB = b; return; }
    // Integer luma using the real BT.601 weights (Android uses
    // 0.299/0.587/0.114 as floats; /1000 here instead of the old
    // >>8-and-round-to-256ths approximation, now that this whole path
    // carries enough precision to make the more exact weights worth
    // using.
    int32_t luma = (r * 299 + g * 587 + b * 114) / 1000;
    *outR = luma + ((r - luma) * (100 + sat)) / 100;
    *outG = luma + ((g - luma) * (100 + sat)) / 100;
    *outB = luma + ((b - luma) * (100 + sat)) / 100;
}

// Writes the 3 output bytes in the configured wire order (most
// WS2812B/NeoPixel strips are GRB, not RGB
// Android inspiration project's own color_order setting).
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

// v2.1/v2.2: levels adjustment (black_level/white_level) -- a
// "stretch" of the 0-255 range, verified in Python before porting.
// Applied last in the color pipeline, matching the
// real Android ColorProcessor order (gamma -> brightness -> contrast
// -> saturation -> levels).
//
// This is now ALSO the single final clamp-to-byte for the entire
// v2.2 pipeline (see the big comment above applyBrightness) -- input
// here may be negative or >255 coming out of contrast/saturation, so
// even the "no-op" (blackLevel=0, whiteLevel=100) fast path below
// must still clamp, not just pass through, or an out-of-range
// unclamped value would wrap/truncate incorrectly when cast to
// uint8_t by the caller.
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

// Full per-zone pipeline, v2.2 order -- now matches the Android source
// exactly: gamma -> brightness -> contrast -> saturation -> levels
// (black/white point) -> wire-order bytes. (v2.0/v2.1 had saturation
// BEFORE brightness, contradicting this file's own comment about
// matching Android. Harmless while brightness was
// only ever a single uniform scalar, since a uniform scale commutes
// with saturation's luma-relative math either way, but it stops being
// harmless the moment per-channel brightness enters the picture below,
// so it's fixed here rather than left as a latent trap.)
//
// Note the input (r,g,b) here has ALREADY had the new per-channel
// gamma_r/gamma_g/gamma_b applied, per-sample, before zone-averaging
// (see sampleZoneAverage) -- that part deliberately happens BEFORE
// this function, not in it, because gamma is non-linear and Android
// applies it before any downsampling too. The legacy fixed-LUT
// `gamma=` global below is a SEPARATE, ADDITIONAL curve applied here,
// zone-wide, after averaging -- kept for backward compatibility with
// existing configs. Both default to a no-op, so an unmodified ini
// still behaves exactly like before.
//
// dark_threshold is deliberately NOT applied here -- it needs
// per-zone hysteresis STATE across calls, which lives with the
// smoothing state in the thread loop instead.
void applyColorProcessing(uint8_t r8, uint8_t g8, uint8_t b8, uint8_t *out3)
{
    // --- gamma (legacy global fixed-LUT stage; per-channel gamma_r/g/b
    // already happened per-sample before this call). This is the one
    // remaining byte-rounding step before the pipeline goes fully
    // unclamped int32_t below -- see the struct comment on why the
    // legacy uint8_t LUT itself wasn't also upgraded to float. ---
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

    // --- levels (black/white point) -- the ONE clamp+round back to a
    // byte for this whole unclamped chain, matching Android's own
    // single final clamp+round. ---
    uint8_t lr, lg, lb;
    applyLevels(sr, sg, sb, g_config.blackLevel, g_config.whiteLevel, &lr, &lg, &lb);

    writeColorOrdered(lr, lg, lb, g_config.colorOrder, out3);
}

// v2.1 dark_threshold state: per-zone hysteresis flag (see
// applyDarkThreshold below) -- separate from g_smoothedRgb since it's
// a decision (am I "dark" right now), not a color value.
static bool g_zoneIsDark[MAX_TOTAL_ZONES];

// Forces (r,g,b) to black if the zone should currently be considered
// "dark", with hysteresis to avoid flicker on scenes hovering right at
// the threshold: must drop BELOW darkThreshold to go dark, but must
// rise darkThreshold+10 to come back -- verified in Python against a
// deliberately noisy sequence straddling the threshold before porting.
// Applied to the pre-smoothing target, so smoothing (if
// enabled) naturally fades in/out of black instead of snapping.
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

