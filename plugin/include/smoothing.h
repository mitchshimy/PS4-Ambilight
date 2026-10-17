// smoothing.h -- the per-zone exponential smoothing, in one place.
// More: docs/code-comments.md (plugin/include/smoothing.h:1)
#ifndef SMOOTHING_H
#define SMOOTHING_H

#include <stdint.h>
#include <stdbool.h>

// Blend factor for one frame, 0..256, from the sample interval and the ini's settling_time_ms. 256...
// More: docs/code-comments.md (plugin/include/smoothing.h:20)
static inline uint32_t smoothing_alpha(uint32_t sampleIntervalUs, uint32_t settlingTimeMs)
{
    uint32_t smoothingIntervalMs = sampleIntervalUs / 1000u;
    uint32_t alpha = (smoothingIntervalMs * 256u) / (settlingTimeMs + 1u);
    if (alpha > 256u) alpha = 256u;
    return alpha;
}

// Move one zone's smoothed colour toward `verified` (the colour-processed, dark-thresholded value).
// More: docs/code-comments.md (plugin/include/smoothing.h:32)
static inline void smoothing_step(uint8_t state[3], const uint8_t verified[3],
                                  uint32_t alpha, bool stateValid)
{
    if (!stateValid) {
        state[0] = verified[0];
        state[1] = verified[1];
        state[2] = verified[2];
        return;
    }
    for (int c = 0; c < 3; c++) {
        int32_t prev = state[c];
        int32_t raw = verified[c];
        int32_t delta = ((raw - prev) * (int32_t)alpha) / 256;
        // Force a minimum step of 1 toward the target: integer division truncates toward zero, so smoothing otherwise
        // stalls at a dim residual (about 3) and never reaches black. [docs/code-comments.md]
        if (delta == 0 && raw != prev) delta = (raw > prev) ? 1 : -1;
        state[c] = (uint8_t)(prev + delta);
    }
}

#endif // SMOOTHING_H
