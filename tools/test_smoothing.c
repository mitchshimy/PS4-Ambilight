// test_smoothing.c -- host test for plugin/include/smoothing.h.
//
// Stage D of the shell-feed merge replaced two copies of the per-zone smoothing
// (inline in plugin/source/sample_thread.c, pasted into shell-feed/source/main.c)
// with the two functions in smoothing.h. This test keeps a verbatim copy of the
// OLD code below as the reference and checks that the new functions give
// bit-identical results on every input that can reach them, then checks the
// property the v2.7.5 fix exists for (a fade always reaches its target).
//
//   gcc -Wall -Iplugin/include -o /tmp/test_smoothing tools/test_smoothing.c && /tmp/test_smoothing
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "smoothing.h"

// ---- reference: the code exactly as it was before Stage D -------------------
static uint32_t ref_alpha(uint32_t sampleIntervalUs, uint32_t settlingTimeMs)
{
    uint32_t smoothingAlpha;
    uint32_t smoothingIntervalMs = sampleIntervalUs / 1000u;
    smoothingAlpha = (smoothingIntervalMs * 256u) / (settlingTimeMs + 1u);
    if (smoothingAlpha > 256u) smoothingAlpha = 256u;
    return smoothingAlpha;
}

static void ref_step(uint8_t g_smoothedRgb_i[3], const uint8_t verified[3],
                     uint32_t smoothingAlpha, bool g_smoothedRgbValid)
{
    if (!g_smoothedRgbValid) {
        g_smoothedRgb_i[0] = verified[0];
        g_smoothedRgb_i[1] = verified[1];
        g_smoothedRgb_i[2] = verified[2];
    } else {
        for (int c = 0; c < 3; c++) {
            int32_t prev = g_smoothedRgb_i[c];
            int32_t raw = verified[c];
            int32_t delta = ((raw - prev) * (int32_t)smoothingAlpha) / 256;
            if (delta == 0 && raw != prev) delta = (raw > prev) ? 1 : -1;
            g_smoothedRgb_i[c] = (uint8_t)(prev + delta);
        }
    }
}
// ---- end reference ----------------------------------------------------------

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; if (fails <= 10) { printf("FAIL: " __VA_ARGS__); printf("\n"); } } } while (0)

int main(void)
{
    // 1. alpha: every interval a real update_frequency_hz can give (1..240 Hz),
    //    against settling_time_ms 0..5000, including the 256 clamp.
    unsigned long alphaCases = 0;
    for (uint32_t hz = 1; hz <= 240; hz++) {
        uint32_t us = 1000000u / hz;
        for (uint32_t ms = 0; ms <= 5000; ms++) {
            CHECK(smoothing_alpha(us, ms) == ref_alpha(us, ms), "alpha hz=%u ms=%u", hz, ms);
            alphaCases++;
        }
    }
    // the project defaults, spelled out so a change here is noticed
    CHECK(smoothing_alpha(33333u, 100u) == 83u, "default alpha (30 Hz, 100 ms) should be 83, got %u",
          smoothing_alpha(33333u, 100u));
    CHECK(smoothing_alpha(33333u, 0u) == 256u, "settling 0 must clamp to 256");

    // 2. step: every previous value x every target x every alpha 0..256, both
    //    valid and not-yet-valid. Channels are independent, so one channel is
    //    swept exhaustively and the other two ride along with other values.
    unsigned long stepCases = 0;
    for (uint32_t a = 0; a <= 256; a++) {
        for (int valid = 0; valid <= 1; valid++) {
            for (int prev = 0; prev < 256; prev++) {
                for (int raw = 0; raw < 256; raw++) {
                    uint8_t s1[3] = { (uint8_t)prev, (uint8_t)(255 - prev), (uint8_t)((prev * 7) & 255) };
                    uint8_t s2[3]; memcpy(s2, s1, 3);
                    uint8_t v[3] = { (uint8_t)raw, (uint8_t)((raw * 3) & 255), (uint8_t)(255 - raw) };
                    smoothing_step(s1, v, a, valid);
                    ref_step(s2, v, a, valid);
                    CHECK(memcmp(s1, s2, 3) == 0,
                          "step a=%u valid=%d prev=%d raw=%d: new=%u,%u,%u ref=%u,%u,%u",
                          a, valid, prev, raw, s1[0], s1[1], s1[2], s2[0], s2[1], s2[2]);
                    stepCases++;
                }
            }
        }
    }

    // 3. the v2.7.5 property: with any alpha >= 1, a fade reaches its target
    //    exactly (no dim residual glow), from every start to every target.
    unsigned long fadeCases = 0;
    for (uint32_t a = 1; a <= 256; a += 5) {
        for (int start = 0; start < 256; start += 3) {
            for (int target = 0; target < 256; target += 51) {
                uint8_t st[3] = { (uint8_t)start, (uint8_t)start, (uint8_t)start };
                uint8_t v[3]  = { (uint8_t)target, (uint8_t)target, (uint8_t)target };
                int frames = 0;
                while ((st[0] != target) && frames < 2000) { smoothing_step(st, v, a, true); frames++; }
                CHECK(st[0] == target, "fade never reached target: a=%u start=%d target=%d stuck at %u",
                      a, start, target, st[0]);
                fadeCases++;
            }
        }
    }
    // the exact bug the fix is for: alpha 83 decaying to black used to stick at 3
    { uint8_t st[3] = {200,200,200}, v[3] = {0,0,0};
      for (int i = 0; i < 200; i++) smoothing_step(st, v, 83u, true);
      CHECK(st[0] == 0 && st[1] == 0 && st[2] == 0, "alpha 83 fade to black stuck at %u,%u,%u", st[0], st[1], st[2]); }

    printf("alpha cases: %lu, step cases: %lu, fade cases: %lu\n", alphaCases, stepCases, fadeCases);
    if (fails) { printf("%d FAILURE(S)\n", fails); return 1; }
    printf("test_smoothing: all checks passed (new smoothing.h is bit-identical to the pre-Stage-D code)\n");
    return 0;
}
