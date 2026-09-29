// Host test for pq8bitVote() in plugin/include/pq8bit_vote.h.
//
//   gcc -Wall -o /tmp/test_pq8bit_vote tools/test_pq8bit_vote.c -lm
//   /tmp/test_pq8bit_vote
//
// Two kinds of input. The recorded ones are real 8-word reads out of PQ8C
// telemetry packets (debug build), copied from captures while YouTube played
// SDR video and then HDR video with the console's HDR on. The synthetic ones
// are real-PQ words built from nits with the ST 2084 curve, because no real
// PQ capture of a title that ISN'T lying about its format is in the repo, so
// the false-positive side is only tested against what PQ should look like.
// See docs/debugging/youtube-hdr-8bit.md.
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "../plugin/include/pq8bit_vote.h"

static uint32_t pq_code(double nits)
{
    double m1 = 2610.0 / 16384, m2 = 2523.0 / 4096 * 128;
    double c1 = 3424.0 / 4096, c2 = 2413.0 / 4096 * 32, c3 = 2392.0 / 4096 * 32;
    double y = pow(nits / 10000.0, m1);
    return (uint32_t)(pow((c1 + c2 * y) / (1 + c3 * y), m2) * 1023.0 + 0.5);
}
// A2R10G10B10 with alpha 3, as a PQ title would write it
static uint32_t pq_word(double r, double g, double b)
{
    return (3u << 30) | (pq_code(r) << 20) | (pq_code(g) << 10) | pq_code(b);
}

static int fails = 0;
#define CHECK(name, want, arr, n) do { \
    int v = pq8bitVote(arr, n); \
    printf("%-58s got %+d want %+d %s\n", name, v, want, v == want ? "ok" : "FAIL"); \
    if (v != want) fails++; \
} while (0)

int main(void)
{
    // recorded: SDR video, HDR on. Alpha byte is 0xff on every word.
    static const uint32_t sdr_dark[8]  = {0xff000000,0xff010203,0xff030305,0xff050609,0xff05070a,0xff0c0907,0xff0f0b09,0xff16100d};
    static const uint32_t sdr_dark2[8] = {0xff000000,0xff030202,0xff060504,0xff0a0806,0xff0d0c09,0xff0a0b09,0xff0d1211,0xff0e110e};
    static const uint32_t ui_bg[8]     = {0xff212121,0xff212121,0xff212121,0xff212121,0xff212121,0xff212121,0xff212121,0xff212121};
    // words from the three labeled screen-color captures (blue, green, red),
    // the 3 sampled zones repeated out to 8
    static const uint32_t blue[8]  = {0xff000eff,0xff00298b,0xff000eff,0xff000eff,0xff00298b,0xff000eff,0xff000eff,0xff000eff};
    static const uint32_t green[8] = {0xff00d700,0xff00de00,0xff00d700,0xff00d700,0xff00de00,0xff00d700,0xff00d700,0xff00d700};
    static const uint32_t red[8]   = {0xffff1800,0xffaa3d00,0xffff1800,0xffff1800,0xffaa3d00,0xffff1800,0xffff1800,0xffff1800};
    CHECK("recorded SDR video: dark frame",           +1, sdr_dark,  8);
    CHECK("recorded SDR video: dark frame 2",         +1, sdr_dark2, 8);
    CHECK("recorded SDR: flat UI background ff212121",+1, ui_bg,     8);
    CHECK("recorded SDR: screen blue",                +1, blue,      8);
    CHECK("recorded SDR: screen green",               +1, green,     8);
    CHECK("recorded SDR: screen red",                 +1, red,       8);

    // recorded: HDR video. Alpha byte is anything but 0xff.
    static const uint32_t hdr_a[8] = {0x00500403,0x00500403,0x0321487c,0x03013c7a,0x03013879,0x0321487c,0x0311447b,0x0120cc5a};
    static const uint32_t hdr_b[8] = {0x00500403,0x00500403,0x04118084,0x0341507c,0x06e23cb7,0x02d13478,0x03013874,0x082290cd};
    static const uint32_t hdr_first[8] = {0x00200402,0x00200402,0x00200402,0x00200402,0x00200402,0x00200402,0x00200402,0x00200402};
    CHECK("recorded HDR video: mid-video read",       -1, hdr_a,     8);
    CHECK("recorded HDR video: mid-video read 2",     -1, hdr_b,     8);
    CHECK("recorded HDR video: first check after switch", -1, hdr_first, 8);

    // synthetic real PQ, must never look like 8-bit
    uint32_t blk[8], w203[8], w1k[8], w4k[8];
    for (int i = 0; i < 8; i++) {
        blk[i] = 0xc0000000u;
        w203[i] = pq_word(203, 203, 203);
        w1k[i]  = pq_word(1000, 1000, 1000);
        w4k[i]  = pq_word(4000, 4000, 4000);
    }
    uint32_t mix[8] = {pq_word(2,2,2), pq_word(80,20,10), pq_word(500,480,450), pq_word(1200,300,50),
                       pq_word(0.1,0.1,0.1), pq_word(203,203,203), pq_word(900,900,900), pq_word(30,60,120)};
    CHECK("synthetic PQ: black 0xc0000000",           -1, blk,  8);
    CHECK("synthetic PQ: 203 nit white",              -1, w203, 8);
    CHECK("synthetic PQ: 1000 nit white",             -1, w1k,  8);
    CHECK("synthetic PQ: 4000 nit white",             -1, w4k,  8);
    CHECK("synthetic PQ: mixed frame",                -1, mix,  8);

    // the hold band
    uint32_t seven[8], six[8], half[8], two[8];
    for (int i = 0; i < 8; i++) {
        seven[i] = (i < 7) ? 0xff212121u : pq_word(203,203,203);
        six[i]   = (i < 6) ? 0xff212121u : pq_word(203,203,203);
        half[i]  = (i < 4) ? 0xff212121u : pq_word(203,203,203);
        two[i]   = (i < 2) ? 0xff212121u : pq_word(203,203,203);
    }
    CHECK("7 of 8 alpha 0xff: stray word, still 8-bit",+1, seven, 8);
    CHECK("6 of 8 (75%): 8-bit",                       +1, six,   8);
    CHECK("4 of 8: hold",                               0, half,  8);
    CHECK("2 of 8 (25%): real PQ",                     -1, two,   8);
    CHECK("3 words readable: not enough to say",        0, ui_bg, 3);

    // where real PQ starts to look like 8-bit
    uint32_t r10 = 0;
    for (uint32_t r = 0; r < 1024; r++)
        if ((((3u << 30) | (r << 20)) >> 24) == 0xff) { r10 = r; break; }
    printf("\nlowest R10 code whose word has alpha byte 0xff: %u (%.0f nits)\n", r10,
           10000.0 * pow(fmax(pow(r10 / 1023.0, 1 / (2523.0/4096*128)) - 3424.0/4096, 0) /
           (2413.0/4096*32 - 2392.0/4096*32 * pow(r10 / 1023.0, 1 / (2523.0/4096*128))), 1 / (2610.0/16384)));
    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
