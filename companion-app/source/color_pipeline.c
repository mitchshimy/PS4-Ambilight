// color_pipeline.c -- the companion app's live-preview color math.
//
// This is NOT a reimplementation. Every function below (the gamma
// LUTs, the fast log2/exp2 approximation, the per-channel gamma LUT
// builder, and applyBrightness/ChannelBrightnessPct/Contrast/
// Saturation/Levels/writeColorOrdered/applyColorProcessing) is copied
// verbatim from ps4_ambient_light v2.2's own main.c, specifically so
// the preview this app shows is numerically identical to what the
// plugin actually produces -- not an approximation that could quietly
// drift from the real pipeline as it evolves.
//
// If ps4_ambient_light's color pipeline changes again, re-sync this
// file from the plugin's main.c -- don't hand-edit it independently.
//
// One deliberate adaptation for the preview use case: the real plugin
// applies per-channel gamma (gamma_r/g/b) PER SAMPLE PIXEL, before
// zone-averaging several samples together. There's no buffer of
// samples here, just one preview swatch color -- treating that single
// color as the one sampled pixel and applying per-channel gamma to it
// directly is the mathematically correct equivalent, not a
// simplification (averaging a single value is a no-op).

#include "color_pipeline.h"
#include <string.h>

#define NUM_GAMMA_LUTS 8
static const uint8_t kGammaLuts[NUM_GAMMA_LUTS][256] = {
    { // gamma 1.0
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
        96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
    },
    { // gamma 1.4
        0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5,
        5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 12, 12, 13, 13,
        14, 15, 15, 16, 16, 17, 18, 18, 19, 20, 20, 21, 22, 22, 23, 24,
        25, 25, 26, 27, 28, 28, 29, 30, 31, 31, 32, 33, 34, 34, 35, 36,
        37, 38, 38, 39, 40, 41, 42, 43, 43, 44, 45, 46, 47, 48, 49, 49,
        50, 51, 52, 53, 54, 55, 56, 57, 57, 58, 59, 60, 61, 62, 63, 64,
        65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96,
        97, 98, 99, 100, 101, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
        115, 116, 117, 118, 119, 120, 121, 122, 124, 125, 126, 127, 128, 129, 130, 132,
        133, 134, 135, 136, 137, 139, 140, 141, 142, 143, 145, 146, 147, 148, 149, 151,
        152, 153, 154, 155, 157, 158, 159, 160, 161, 163, 164, 165, 166, 168, 169, 170,
        171, 173, 174, 175, 176, 178, 179, 180, 181, 183, 184, 185, 187, 188, 189, 190,
        192, 193, 194, 196, 197, 198, 200, 201, 202, 203, 205, 206, 207, 209, 210, 211,
        213, 214, 215, 217, 218, 219, 221, 222, 223, 225, 226, 227, 229, 230, 232, 233,
        234, 236, 237, 238, 240, 241, 242, 244, 245, 247, 248, 249, 251, 252, 254, 255,
    },
    { // gamma 1.8
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2,
        2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6,
        6, 6, 7, 7, 8, 8, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12,
        13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 21,
        21, 22, 22, 23, 24, 24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31,
        32, 32, 33, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 41, 42, 43,
        44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 53, 54, 55, 56, 57,
        58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
        74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 86, 87, 88, 89, 90,
        91, 92, 93, 95, 96, 97, 98, 99, 100, 102, 103, 104, 105, 107, 108, 109,
        110, 111, 113, 114, 115, 116, 118, 119, 120, 122, 123, 124, 126, 127, 128, 129,
        131, 132, 134, 135, 136, 138, 139, 140, 142, 143, 145, 146, 147, 149, 150, 152,
        153, 154, 156, 157, 159, 160, 162, 163, 165, 166, 168, 169, 171, 172, 174, 175,
        177, 178, 180, 181, 183, 184, 186, 188, 189, 191, 192, 194, 195, 197, 199, 200,
        202, 204, 205, 207, 208, 210, 212, 213, 215, 217, 218, 220, 222, 224, 225, 227,
        229, 230, 232, 234, 236, 237, 239, 241, 243, 244, 246, 248, 250, 251, 253, 255,
    },
    { // gamma 2.0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
        4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9,
        9, 9, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16,
        16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 23, 23, 24, 24,
        25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35, 35,
        36, 37, 38, 38, 39, 40, 41, 42, 42, 43, 44, 45, 46, 47, 47, 48,
        49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 84, 85, 86, 87, 88, 89, 91, 92, 93, 94, 95, 97, 98, 99,
        100, 102, 103, 104, 105, 107, 108, 109, 111, 112, 113, 115, 116, 117, 119, 120,
        121, 123, 124, 126, 127, 128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 143,
        145, 146, 148, 149, 151, 152, 154, 155, 157, 158, 160, 162, 163, 165, 166, 168,
        170, 171, 173, 175, 176, 178, 180, 181, 183, 185, 186, 188, 190, 192, 193, 195,
        197, 199, 200, 202, 204, 206, 207, 209, 211, 213, 215, 217, 218, 220, 222, 224,
        226, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255,
    },
    { // gamma 2.2
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6,
        6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12,
        12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
        20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29,
        30, 30, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 39, 39, 40, 41,
        42, 43, 43, 44, 45, 46, 47, 48, 49, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
        73, 74, 75, 76, 77, 78, 79, 81, 82, 83, 84, 85, 87, 88, 89, 90,
        91, 93, 94, 95, 97, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 111,
        113, 114, 116, 117, 119, 120, 121, 123, 124, 126, 127, 129, 130, 132, 133, 135,
        137, 138, 140, 141, 143, 145, 146, 148, 149, 151, 153, 154, 156, 158, 159, 161,
        163, 165, 166, 168, 170, 172, 173, 175, 177, 179, 181, 182, 184, 186, 188, 190,
        192, 194, 196, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
        223, 225, 227, 229, 231, 234, 236, 238, 240, 242, 244, 246, 248, 251, 253, 255,
    },
    { // gamma 2.4
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
        2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9,
        9, 10, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 14, 15, 15,
        16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24,
        24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35,
        35, 36, 37, 38, 39, 39, 40, 41, 42, 43, 43, 44, 45, 46, 47, 48,
        49, 50, 51, 52, 53, 53, 54, 55, 56, 57, 58, 59, 60, 62, 63, 64,
        65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 80, 81, 82,
        83, 85, 86, 87, 88, 90, 91, 92, 94, 95, 96, 98, 99, 100, 102, 103,
        105, 106, 108, 109, 111, 112, 114, 115, 117, 118, 120, 121, 123, 124, 126, 127,
        129, 131, 132, 134, 136, 137, 139, 141, 142, 144, 146, 148, 149, 151, 153, 155,
        156, 158, 160, 162, 164, 166, 167, 169, 171, 173, 175, 177, 179, 181, 183, 185,
        187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 210, 212, 214, 216, 218,
        220, 223, 225, 227, 229, 232, 234, 236, 239, 241, 243, 246, 248, 250, 253, 255,
    },
    { // gamma 2.6
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3,
        3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7,
        7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12,
        13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20,
        20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29,
        30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 38, 38, 39, 40, 41, 42,
        42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
        58, 59, 60, 61, 62, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73, 75,
        76, 77, 78, 80, 81, 82, 84, 85, 86, 88, 89, 90, 92, 93, 94, 96,
        97, 99, 100, 102, 103, 105, 106, 108, 109, 111, 112, 114, 115, 117, 119, 120,
        122, 124, 125, 127, 129, 130, 132, 134, 136, 137, 139, 141, 143, 145, 146, 148,
        150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180,
        182, 184, 186, 188, 191, 193, 195, 197, 199, 202, 204, 206, 209, 211, 213, 215,
        218, 220, 223, 225, 227, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252, 255,
    },
    { // gamma 2.8
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
        2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
        5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
        10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
        17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
        25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
        37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
        51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
        69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
        90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
        115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
        144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
        177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
        215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255,
    },
};


// ---- verbatim from ps4_ambient_light v2.2 main.c (fast log2/exp2 for
// the arbitrary-percentage per-channel gamma controls -- no libm
// linked in either project, see each file's own Makefile) ----

static float ambient_fast_log2(float x)
{
    union { float f; uint32_t i; } vx = { x };
    union { uint32_t i; float f; } mx = { (vx.i & 0x007FFFFFu) | 0x3f000000u };
    float y = (float)vx.i;
    y *= 1.0f / (1 << 23);
    return y - 124.22551499f - 1.498030302f * mx.f - 1.72587999f / (0.3520887068f + mx.f);
}

static float ambient_fast_exp2(float p)
{
    float offset = (p < 0.0f) ? 1.0f : 0.0f;
    float clipp = (p < -126.0f) ? -126.0f : p;
    int32_t w = (int32_t)clipp;
    float z = clipp - (float)w + offset;
    union { uint32_t i; float f; } v = {
        (uint32_t)((1 << 23) * (clipp + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z))
    };
    return v.f;
}

static float ambient_fast_pow01(float x, float p)
{
    if (x <= 0.0f) return 0.0f;
    return ambient_fast_exp2(p * ambient_fast_log2(x));
}

static float g_gammaLutR[256], g_gammaLutG[256], g_gammaLutB[256];

static void ambient_build_one_gamma_lut(uint32_t pct, float *outLut)
{
    if (pct == 100) {
        for (uint32_t i = 0; i < 256; i++) outLut[i] = (float)i;
        return;
    }
    float invGamma = 100.0f / (float)pct;
    for (uint32_t i = 0; i < 256; i++) {
        float norm = (float)i / 255.0f;
        outLut[i] = ambient_fast_pow01(norm, invGamma) * 255.0f;
    }
}

void colorpipeline_rebuild_perchannel_gamma_luts(const AmbientConfig *cfg)
{
    ambient_build_one_gamma_lut(cfg->gammaR, g_gammaLutR);
    ambient_build_one_gamma_lut(cfg->gammaG, g_gammaLutG);
    ambient_build_one_gamma_lut(cfg->gammaB, g_gammaLutB);
}

// ---- verbatim from ps4_ambient_light v2.2 main.c (the rest of the
// pipeline -- unclamped int32_t between every stage, one clamp+round
// at the very end in applyLevels, matching Android's real float
// pipeline exactly rather than the v2.0/v2.1 byte-clamped version --
// see the plugin's own extensive comments on why, reproduced there,
// not duplicated here) ----

static int32_t applyBrightness(int32_t c, uint32_t brightness)
{
    return (c * (int32_t)brightness) / 255;
}

static int32_t applyChannelBrightnessPct(int32_t c, uint32_t pct)
{
    if (pct == 100) return c;
    return (c * (int32_t)pct) / 100;
}

static void applyContrast(int32_t r, int32_t g, int32_t b, int32_t contrast,
                           int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (contrast == 0) { *outR = r; *outG = g; *outB = b; return; }
    *outR = 128 + ((r - 128) * (100 + contrast)) / 100;
    *outG = 128 + ((g - 128) * (100 + contrast)) / 100;
    *outB = 128 + ((b - 128) * (100 + contrast)) / 100;
}

static void applySaturation(int32_t r, int32_t g, int32_t b, int32_t sat,
                             int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (sat == 0) { *outR = r; *outG = g; *outB = b; return; }
    int32_t luma = (r * 299 + g * 587 + b * 114) / 1000;
    *outR = luma + ((r - luma) * (100 + sat)) / 100;
    *outG = luma + ((g - luma) * (100 + sat)) / 100;
    *outB = luma + ((b - luma) * (100 + sat)) / 100;
}

void colorpipeline_write_color_ordered(uint8_t r, uint8_t g, uint8_t b, ColorOrder order, uint8_t *out3)
{
    switch (order) {
        case ORDER_RGB: out3[0]=r; out3[1]=g; out3[2]=b; break;
        case ORDER_RBG: out3[0]=r; out3[1]=b; out3[2]=g; break;
        case ORDER_GRB: out3[0]=g; out3[1]=r; out3[2]=b; break;
        case ORDER_GBR: out3[0]=g; out3[1]=b; out3[2]=r; break;
        case ORDER_BRG: out3[0]=b; out3[1]=r; out3[2]=g; break;
        default: out3[0]=b; out3[1]=g; out3[2]=r; break;
    }
}

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
    if (range <= 0) { *outR = 0; *outG = 0; *outB = 0; return; }
    #define STRETCH(c) CLAMP255(((c) < blackThresh) ? 0 : ((c) >= whiteThresh) ? 255 : \
                                  (((c) - blackThresh) * 255) / range)
    *outR = STRETCH(r);
    *outG = STRETCH(g);
    *outB = STRETCH(b);
    #undef STRETCH
    #undef CLAMP255
}

// Preview entry point. Applies per-channel gamma first (see the file
// header note on why that's correct for a single swatch color), then
// hands off to the exact same chain the real plugin runs per zone:
// gamma(legacy global LUT) -> brightness(global+per-channel) ->
// contrast -> saturation -> levels -> wire order.
void colorpipeline_process(const AmbientConfig *cfg, uint8_t r8, uint8_t g8, uint8_t b8, uint8_t *out3)
{
    // per-channel gamma (per-sample, pre-averaging in the real
    // plugin -- here just applied once, to the one input color)
    r8 = (uint8_t)(g_gammaLutR[r8] < 0 ? 0 : (g_gammaLutR[r8] > 255 ? 255 : g_gammaLutR[r8]));
    g8 = (uint8_t)(g_gammaLutG[g8] < 0 ? 0 : (g_gammaLutG[g8] > 255 ? 255 : g_gammaLutG[g8]));
    b8 = (uint8_t)(g_gammaLutB[b8] < 0 ? 0 : (g_gammaLutB[b8] > 255 ? 255 : g_gammaLutB[b8]));

    uint32_t gi = cfg->gammaLutIndex < NUM_GAMMA_LUTS ? cfg->gammaLutIndex : 0;
    int32_t r = kGammaLuts[gi][r8];
    int32_t g = kGammaLuts[gi][g8];
    int32_t b = kGammaLuts[gi][b8];

    r = applyBrightness(r, cfg->brightness);
    g = applyBrightness(g, cfg->brightness);
    b = applyBrightness(b, cfg->brightness);
    r = applyChannelBrightnessPct(r, cfg->brightnessR);
    g = applyChannelBrightnessPct(g, cfg->brightnessG);
    b = applyChannelBrightnessPct(b, cfg->brightnessB);

    int32_t cr, cg, cb;
    applyContrast(r, g, b, cfg->contrast, &cr, &cg, &cb);

    int32_t sr, sg, sb;
    applySaturation(cr, cg, cb, cfg->saturation, &sr, &sg, &sb);

    uint8_t lr, lg, lb;
    applyLevels(sr, sg, sb, cfg->blackLevel, cfg->whiteLevel, &lr, &lg, &lb);

    colorpipeline_write_color_ordered(lr, lg, lb, cfg->colorOrder, out3);
}
