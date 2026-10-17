// shell_frame.c -- one frame for the strip: every zone sampled with the plugin's sampleZoneAverage(), run
// through the plugin's colour chain and dark threshold, then smoothed, in LED order.
#include "shell_internal.h"

// ---------------------------------------------------------------------
// [v22] Glue: this project's own code from here to the end of the block.
// ---------------------------------------------------------------------
static uint8_t g_smoothedRgb[MAX_TOTAL_ZONES][3];  // this file's own smoothing STATE (the maths is smoothing.h's) -- private, per the
                                                     // comment above ambient_internal.h's g_smoothedRgbValid extern
bool g_smoothedRgbValid = false; // ambient_internal.h declares this extern (real definition's normally sample_thread.c's,
                                  // which this project doesn't link); false until the first real frame, and again after a reload

// The compositor's display buffers are A8R8G8B8 words (0xAARRGGBB), the same layout v17's sampler reads.
static void unpack_argb(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((px >> 16) & 255); *g = (uint8_t)((px >> 8) & 255); *b = (uint8_t)(px & 255);
}

// One frame: every zone sampled with scan_depth, run through the sister's colour chain, dark threshold and
// smoothing, written to rgbTriplets in LED order (zone i IS LED i: buildZoneGeometry() already applied the
// start corner, direction and offset). Returns the zone count. This is the sister's sample_thread.c pass 3
// without the game-process-only parts (see the header comment).
uint32_t shell_process_frame(uint64_t bufferAddr, uint32_t sampleIntervalUs, uint8_t *rgbTriplets)
{
    uint32_t smoothingAlpha = smoothing_alpha(sampleIntervalUs, g_config.settlingTimeMs);
    for (uint32_t i = 0; i < g_numZones; i++) {
        uint8_t r, g, b;
        sampleZoneAverage(&kParamsBase, bufferAddr, unpack_argb, g_zoneX[i], g_zoneY[i], &r, &g, &b);
        uint8_t verified[3];
        applyColorProcessing(r, g, b, verified);
        applyDarkThreshold(i, &verified[0], &verified[1], &verified[2]);
                    if (g_config.smoothingEnabled) {
                        smoothing_step(g_smoothedRgb[i], verified, smoothingAlpha, g_smoothedRgbValid);
                        rgbTriplets[i*3+0] = g_smoothedRgb[i][0];
                        rgbTriplets[i*3+1] = g_smoothedRgb[i][1];
                        rgbTriplets[i*3+2] = g_smoothedRgb[i][2];
                    } else {
                        rgbTriplets[i*3+0] = verified[0];
                        rgbTriplets[i*3+1] = verified[1];
                        rgbTriplets[i*3+2] = verified[2];
                    }
    }
    g_smoothedRgbValid = true;
    return g_numZones;
}
