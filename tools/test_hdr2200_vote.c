// Host test for hdr2200Vote() in plugin/include/hdr2200_vote.h.
//
//   gcc -Wall -o /tmp/test_hdr2200_vote tools/test_hdr2200_vote.c
//   /tmp/test_hdr2200_vote                       (run from the repo root)
//   /tmp/test_hdr2200_vote path/to/hdr2200_frames.csv
//
// Two kinds of input. Hand-written cases cover the vote's edges: SDR alpha
// 0xff or 0x00, PQ black, the hold band, cleared buffers, fade frames. The
// recorded ones are tools/data/hdr2200_frames.csv, 281 real 8-word frames the
// plugin read on a PS4 (HDRV telemetry, debug build) in HITMAN 3 and RDR2 with
// the console's HDR on and off. Neither game applies an HDR change until it is
// restarted, so every frame in a capture has the mode its file name says.
// See docs/debugging/hdr2200-alpha-detection.md.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../plugin/include/hdr2200_vote.h"

static int fails = 0;
#define CHECK(name, want, arr, n) do { \
    int v = hdr2200Vote(arr, n); \
    printf("%-62s got %+d want %+d %s\n", name, v, want, v == want ? "ok" : "FAIL"); \
    if (v != want) fails++; \
} while (0)

static void recorded(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { printf("cannot open %s (run from the repo root, or pass the path)\n", path); fails++; return; }
    static const char *names[] = {"hitman3_hdr_on_v36", "hitman3_hdr_off_v36", "hitman3_hdr_on",
                                  "hitman3_hdr_off", "rdr2_hdr_on", "rdr2_hdr_off"};
    static const int truth[] = {+1, -1, +1, -1, +1, -1}; // what the buffer really held
    // Replays each capture the way zones.c uses the vote: the mode starts at SDR, a decisive vote
    // sets it, a hold leaves it. A frame is "wrong mode" if it had real (non-zero) words and the
    // mode at that point wasn't the truth. A cleared-buffer frame before the first real one isn't.
    int total[6] = {0}, pq[6] = {0}, sdr[6] = {0}, hold[6] = {0}, wrongMode[6] = {0}, mode[6] = {0};
    int firstReal[6] = {0}, firstRealSeen[6] = {0};
    char line[512];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        char src[64]; double t; uint32_t w[8];
        if (sscanf(line, "%63[^,],%lf,%x,%x,%x,%x,%x,%x,%x,%x", src, &t,
                   &w[0], &w[1], &w[2], &w[3], &w[4], &w[5], &w[6], &w[7]) != 10) continue;
        int k = -1;
        for (int i = 0; i < 6; i++) if (strcmp(src, names[i]) == 0) k = i;
        if (k < 0) { printf("unknown source %s\n", src); fails++; continue; }
        int v = hdr2200Vote(w, 8);
        total[k]++;
        if (v > 0) pq[k]++; else if (v < 0) sdr[k]++; else hold[k]++;
        if (v != 0) mode[k] = (v > 0);
        int real = 0; for (int i = 0; i < 8; i++) if (w[i]) real = 1;
        if (real) {
            if (!firstRealSeen[k]) { firstRealSeen[k] = 1; firstReal[k] = 1; }
            if (mode[k] != (truth[k] > 0)) wrongMode[k]++;
        }
    }
    fclose(f);
    printf("\nrecorded frames, %d in all (vote PQ / SDR / hold, then frames decoded in the wrong mode):\n",
           total[0] + total[1] + total[2] + total[3] + total[4] + total[5]);
    for (int k = 0; k < 6; k++) {
        printf("  %-22s %3d frames  %3d / %3d / %3d   wrong mode: %d %s\n",
               names[k], total[k], pq[k], sdr[k], hold[k], wrongMode[k], wrongMode[k] ? "FAIL" : "ok");
        if (wrongMode[k] || !firstReal[k]) fails++;
        // a vote in the wrong direction is a failure even if a later frame fixed the mode
        if ((truth[k] > 0 ? sdr[k] : pq[k]) != 0) { printf("    FAIL: a frame voted the wrong way\n"); fails++; }
    }
    printf("  (holds are cleared buffers, fades and dark flat frames; the mode stays as it was)\n");
}

int main(int argc, char **argv)
{
    // hand-written: SDR
    static const uint32_t sdr_ff[8]    = {0xff102030,0xff000000,0xffffffff,0xff808080,0xff010203,0xff404040,0xff9a8b7c,0xff112233};
    static const uint32_t sdr_00[8]    = {0x00102030,0x00ff0000,0x0000ff00,0x00808080,0x00010203,0x00404040,0x009a8b7c,0x00112233};
    static const uint32_t sdr_black[8] = {0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000};
    CHECK("SDR, alpha byte 0xff",                       -1, sdr_ff,    8);
    CHECK("SDR, alpha byte 0x00 (XRGB)",                -1, sdr_00,    8);
    CHECK("SDR black 0xff000000 decides",               -1, sdr_black, 8);

    // hand-written: PQ. Top byte is 0b11RRRRRR and never 0xff.
    static const uint32_t pq_black[8]  = {0xc0000000,0xc0000000,0xc0000000,0xc0000000,0xc0000000,0xc0000000,0xc0000000,0xc0000000};
    static const uint32_t pq_dark[8]   = {0xc0200000,0xc0000003,0xc0300002,0xc0000800,0xc0200401,0xc0100000,0xc0100000,0xc0000001};
    static const uint32_t pq_bright[8] = {0xd2f47d0c,0xdaa6d5c4,0xd364a50d,0xd3a4b512,0xdbd71dd9,0xd404cd16,0xcf43b0d4,0xd675cd7f};
    CHECK("PQ black 0xc0000000 decides",                +1, pq_black,  8);
    CHECK("PQ dark scene with 0xc0 top bytes",          +1, pq_dark,   8);
    CHECK("PQ bright scene",                            +1, pq_bright, 8);

    // cleared buffers and too few words
    static const uint32_t zeros[8] = {0};
    static const uint32_t mixz[8]  = {0,0,0,0,0xc0100000,0xc0200000,0xc0300000,0xc0400000};
    CHECK("all-zero buffer says nothing",                0, zeros,     8);
    CHECK("3 words readable: not enough to say",         0, pq_bright, 3);
    CHECK("zero words count for neither side",          +1, mixz,      8);

    // the hold band, 75% / 25%
    static const uint32_t p6[8]   = {0xc0100000,0xc0100000,0xc0100000,0xc0100000,0xc0100000,0xc0100000,0xff000000,0xff000000};
    static const uint32_t p5[8]   = {0xc0100000,0xc0100000,0xc0100000,0xc0100000,0xc0100000,0xff000000,0xff000000,0xff000000};
    static const uint32_t half[8] = {0xc0100000,0xc0100000,0xc0100000,0xc0100000,0xff000000,0xff000000,0xff000000,0xff000000};
    static const uint32_t p2[8]   = {0xc0100000,0xc0100000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000};
    CHECK("6 of 8 PQ (75%): PQ",                        +1, p6,   8);
    CHECK("5 of 8 PQ: hold",                             0, p5,   8);
    CHECK("4 of 8 PQ: hold",                             0, half, 8);
    CHECK("2 of 8 PQ (25%): SDR",                       -1, p2,   8);

    // a saturated PQ red has top byte 0xff and reads as SDR. One word must not flip the frame.
    static const uint32_t peak[8] = {0xfff00000,0xc0100000,0xc0200000,0xc0300000,0xc0400000,0xc0500000,0xc0600000,0xc0700000};
    CHECK("one peak-red PQ word (0xff top byte) outvoted", +1, peak, 8);

    // alpha 0..2 in bits 31:30 is not opaque, so not PQ
    static const uint32_t a2[8] = {0x40100000,0x40100000,0x40100000,0x40100000,0x80100000,0x80100000,0x80100000,0x80100000};
    CHECK("non-opaque 2-bit alpha is not PQ",           -1, a2, 8);

    // fade frames, real words from the HDR-off capture. Alpha byte is constant or nearly so
    // in 0xc1..0xfe, which looks like PQ and isn't.
    static const uint32_t fade_const[8]  = {0xed010101,0xed1f1c18,0xed010101,0xed010101,0xed1f1c18,0xed010101,0xed010101,0xed1f1c18};
    static const uint32_t fade_in_a[8]   = {0xcc181515,0xcb181615,0xcb181616,0xcb181616,0xca181616,0xca191717,0xca191716,0xc81b1817};
    static const uint32_t fade_in_b[8]   = {0xf90d0c0c,0xf90e0d0d,0xf90e0d0d,0xf90e0d0d,0xf90f0d0d,0xf90f0e0e,0xf90f0e0d,0xf9110f0e};
    CHECK("fade frame, constant alpha 0xed",             0, fade_const, 8);
    CHECK("fade-in frame, alpha cc..c8 (voted PQ in v3.6)", 0, fade_in_a, 8);
    CHECK("fade-in frame, alpha f9",                     0, fade_in_b,  8);
    static const uint32_t fade_low[8] = {0x11010101,0x11010101,0x11010101,0x11010101,0x11010101,0x11010101,0x11010101,0x11010101};
    CHECK("fade frame at a low alpha (0x11) is plain SDR", -1, fade_low, 8);

    // the guard's cost: dark flat PQ frame, top bytes c1..c5, holds until a varied frame
    static const uint32_t pq_dark_flat[8] = {0xc4010045,0xc4310446,0xc4410c48,0xc4911c4b,0xc4011c47,0xc4411c55,0xc4914859,0xc4d1405c};
    CHECK("dark flat PQ frame (c4 on every word): hold", 0, pq_dark_flat, 8);
    // a spread of 5 or more is not a fade
    static const uint32_t spread5[8] = {0xc1100000,0xc6100000,0xc1100000,0xc6100000,0xc1100000,0xc6100000,0xc1100000,0xc6100000};
    CHECK("top bytes 5 apart in c1..c6: PQ",            +1, spread5, 8);

    recorded(argc > 1 ? argv[1] : "tools/data/hdr2200_frames.csv");

    printf("\n%s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
