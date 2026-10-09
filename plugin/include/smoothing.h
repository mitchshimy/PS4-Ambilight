// smoothing.h -- the per-zone exponential smoothing, in one place.
// Own header, integer maths only, so tools/test_smoothing.c can build it on a
// PC and prove it matches the code it replaced.
//
// Until the Stage D merge step this lived as two copies: inline in
// sample_thread.c's pass 3, and pasted into shell-feed/source/main.c. Both now
// call these two functions. The state array (previous frame's output per zone)
// and the "valid" flag stay in each caller: this header owns the arithmetic
// only, so each process keeps its own, private smoothing state.
//
// Both functions are static inline on purpose: no new .c file, so neither
// Makefile's source list changes, and the compiler can inline them back into
// the same loop the maths used to sit in.
#ifndef SMOOTHING_H
#define SMOOTHING_H

#include <stdint.h>
#include <stdbool.h>

// Blend factor for one frame, 0..256, from the sample interval and the ini's
// settling_time_ms. 256 means "snap to the new value". This is a discrete
// integer approximation of RC-style exponential smoothing, NOT a calibrated
// time constant: read settling_time_ms = 200 as "roughly", not as a guarantee.
static inline uint32_t smoothing_alpha(uint32_t sampleIntervalUs, uint32_t settlingTimeMs)
{
    uint32_t smoothingIntervalMs = sampleIntervalUs / 1000u;
    uint32_t alpha = (smoothingIntervalMs * 256u) / (settlingTimeMs + 1u);
    if (alpha > 256u) alpha = 256u;
    return alpha;
}

// Move one zone's smoothed colour toward `verified` (the colour-processed,
// dark-thresholded value). `state` is that zone's previous output and is
// updated in place; the caller writes it to the LED buffer.
//
// stateValid == false (first frame, or the frame after a settings reload or a
// letterbox commit) seeds the state straight from `verified`: a hard cut, not
// a fade in from black.
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
        // v2.7.5: C's / truncates toward zero, not toward -infinity, so once
        // |raw-prev| is small enough that |delta| rounds down to 0, prev stops
        // moving AT ALL, forever, even though raw != prev. Confirmed by
        // simulation with this project's own defaults (update_frequency_hz=30,
        // settling_time_ms=100 -> alpha=83): decaying toward 0 from ANY
        // starting brightness lands on exactly 3 and sticks there for good --
        // a dim residual glow that never reaches black on its own. This is
        // what was reported as "a colorful scene's colors are retained dimly"
        // after a cut to black. Forcing a minimum step of 1 toward raw
        // whenever the proportional step would round to 0 guarantees it
        // always converges to the exact target within a few extra frames.
        if (delta == 0 && raw != prev) delta = (raw > prev) ? 1 : -1;
        state[c] = (uint8_t)(prev + delta);
    }
}

#endif // SMOOTHING_H
