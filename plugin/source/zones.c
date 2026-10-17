// zones.c -- part of ps4_ambient_light, split out of the original single-file main.c. Screen-edge zone...
// More: docs/code-comments.md (plugin/source/zones.c:1)

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

// Zones: screen-edge points read as a small averaged neighborhood (not a single pixel) for stability.
// More: docs/code-comments.md (plugin/source/zones.c:21)

#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

// (MAX_TOTAL_ZONES is defined earlier, near DDP_HEADER_SIZE -- needed before wled_send_rgb_zones's...
// More: docs/code-comments.md (plugin/source/zones.c:39)
uint32_t g_zoneX[MAX_TOTAL_ZONES];
uint32_t g_zoneY[MAX_TOTAL_ZONES];
uint32_t g_numZones = 0; // set by buildZoneGeometry(), 0 until then

typedef enum { EDGE_LEFT, EDGE_TOP, EDGE_RIGHT, EDGE_BOTTOM } Edge;

// v3.0: effective margin per edge = whatever letterbox.c's auto- detection currently has committed for...
// More: docs/code-comments.md (plugin/source/zones.c:48)
static inline uint32_t effMarginTop(void)
{
    return g_autoLetterboxTop > (SCREEN_HEIGHT / 2 - 1) ? (SCREEN_HEIGHT / 2 - 1) : g_autoLetterboxTop;
}
static inline uint32_t effMarginBottom(void)
{
    return g_autoLetterboxBottom > (SCREEN_HEIGHT / 2 - 1) ? (SCREEN_HEIGHT / 2 - 1) : g_autoLetterboxBottom;
}
static inline uint32_t effMarginLeft(void)
{
    return g_autoLetterboxLeft > (SCREEN_WIDTH / 2 - 1) ? (SCREEN_WIDTH / 2 - 1) : g_autoLetterboxLeft;
}
static inline uint32_t effMarginRight(void)
{
    return g_autoLetterboxRight > (SCREEN_WIDTH / 2 - 1) ? (SCREEN_WIDTH / 2 - 1) : g_autoLetterboxRight;
}

// Generates `count` points along one screen edge, in either its canonical clockwise-from-bottom-left...
// More: docs/code-comments.md (plugin/source/zones.c:82)
static void generateEdgePoints(Edge edge, bool reversed, uint32_t count, uint32_t *outIdx)
{
    for (uint32_t i = 0; i < count && *outIdx < MAX_TOTAL_ZONES; i++) {
        uint32_t t = reversed ? (count - 1 - i) : i;
        uint32_t x, y;
        uint32_t mTop = effMarginTop(), mRight = effMarginRight(), mBottom = effMarginBottom(), mLeft = effMarginLeft();
        switch (edge) {
        case EDGE_LEFT:
            x = mLeft;
            y = (SCREEN_HEIGHT - 1 - mBottom) -
                (t * (SCREEN_HEIGHT - 1 - mTop - mBottom)) / count;
            break;
        case EDGE_TOP:
            x = mLeft +
                (t * (SCREEN_WIDTH - 1 - mLeft - mRight)) / count;
            y = mTop;
            break;
        case EDGE_RIGHT:
            x = SCREEN_WIDTH - 1 - mRight;
            y = mTop +
                (t * (SCREEN_HEIGHT - 1 - mTop - mBottom)) / count;
            break;
        default: // EDGE_BOTTOM
            x = (SCREEN_WIDTH - 1 - mRight) -
                (t * (SCREEN_WIDTH - 1 - mLeft - mRight)) / count;
            y = SCREEN_HEIGHT - 1 - mBottom;
            break;
        }
        g_zoneX[*outIdx] = x;
        g_zoneY[*outIdx] = y;
        (*outIdx)++;
    }
}

// Fills g_zoneX/g_zoneY (and g_numZones) at load time using only
// integer math (no libm linked in this build, see Makefile LIBS).
void buildZoneGeometry(void)
{
    static const Edge kCwFromBL[4] = { EDGE_LEFT, EDGE_TOP, EDGE_RIGHT, EDGE_BOTTOM };
    uint32_t counts[4] = {
        [EDGE_LEFT] = g_config.ledCountLeft, [EDGE_TOP] = g_config.ledCountTop,
        [EDGE_RIGHT] = g_config.ledCountRight, [EDGE_BOTTOM] = g_config.ledCountBottom,
    };

    // Rotate the canonical CW-from-bottom-left order to start at the
    // edge that begins at the configured start corner.
    int startEdgeIdx;
    switch (g_config.startCorner) {
        case CORNER_BOTTOM_LEFT:  startEdgeIdx = 0; break; // LEFT
        case CORNER_TOP_LEFT:     startEdgeIdx = 1; break; // TOP
        case CORNER_TOP_RIGHT:    startEdgeIdx = 2; break; // RIGHT
        default: /* BOTTOM_RIGHT */ startEdgeIdx = 3; break; // BOTTOM
    }

    Edge order[4];
    bool edgeReversed[4];
    if (g_config.direction == DIR_CLOCKWISE) {
        for (int i = 0; i < 4; i++) { order[i] = kCwFromBL[(startEdgeIdx + i) % 4]; edgeReversed[i] = false; }
    } else {
        // CCW = reverse of the CW-for-this-corner visit order, with every edge's own direction also reversed.
        // More: docs/code-comments.md (plugin/source/zones.c:166)
        for (int i = 0; i < 4; i++) { order[i] = kCwFromBL[(startEdgeIdx + (3 - i)) % 4]; edgeReversed[i] = true; }
    }

    uint32_t idx = 0;
    for (int i = 0; i < 4; i++) {
        generateEdgePoints(order[i], edgeReversed[i], counts[order[i]], &idx);
    }
    g_numZones = idx;

    // led_offset: rotate which physical LED index 0 lands on, without changing the shape/order of the...
    // More: docs/code-comments.md (plugin/source/zones.c:178)
    int32_t offset = g_config.ledOffset % (int32_t)g_numZones;
    if (offset < 0) offset += (int32_t)g_numZones; // proper modulo for negative offsets
    if (offset != 0) {
        uint32_t tmpX[MAX_TOTAL_ZONES], tmpY[MAX_TOTAL_ZONES];
        memcpy(tmpX, g_zoneX, g_numZones * sizeof(uint32_t));
        memcpy(tmpY, g_zoneY, g_numZones * sizeof(uint32_t));
        for (uint32_t i = 0; i < g_numZones; i++) {
            uint32_t src = (i + (uint32_t)offset) % g_numZones;
            g_zoneX[i] = tmpX[src];
            g_zoneY[i] = tmpY[src];
        }
    }
}

// v3.6: per-pass alpha byte check for 0x80002200, tried before the smoothness detector below.
// More: docs/code-comments.md (plugin/source/zones.c:197)
static int detectHdr2200Fast(uint64_t bufferAddr)
{
    uint32_t nSamples = g_numZones < HDR2200_VOTE_SAMPLES ? g_numZones : HDR2200_VOTE_SAMPLES;
    uint32_t words[HDR2200_VOTE_SAMPLES];
    uint32_t got = 0;
    for (uint32_t i = 0; i < nSamples; i++) {
        uint64_t off = getTiledElementByteOffset(&kParamsBase, g_zoneX[i], g_zoneY[i]);
        if (off + 4 > g_readableLimit) continue; // same safety check as detectHdr2200Format
        memcpy(&words[got], (const void*)(bufferAddr + off), 4);
        got++;
    }

    int vote = hdr2200Vote(words, got);
    int prevMode = g_hdr2200IsHdr;
    if (vote != 0) {
        g_hdr2200IsHdr = (vote > 0) ? 1 : 0;
        g_hdr2200Streak = 0; // the slow detector's streak must not fight this verdict
    }

#if (__FINAL__) == 0
    // Diagnostic-only, same debug_send_raw idiom as the PQ8C packet below.
    // More: docs/code-comments.md (plugin/source/zones.c:230)
    static uint32_t s_hdrvCount = 0;
    if ((int)g_hdr2200IsHdr != prevMode || (s_hdrvCount++ % 30u) == 0) {
        uint8_t pkt[48];
        memset(pkt, 0, sizeof(pkt));
        memcpy(pkt, "HDRV", 4);
        uint32_t pq = 0;
        for (uint32_t i = 0; i < got; i++)
            if ((words[i] >> 30) == 3u && (words[i] >> 24) != 0xFFu) pq++;
        pkt[4] = (uint8_t)(int8_t)vote;
        pkt[5] = (uint8_t)got;
        pkt[6] = (uint8_t)pq;
        pkt[7] = (uint8_t)g_hdr2200IsHdr;
        memcpy(pkt + 8, words, got * 4);
        uint32_t nz = g_numZones, lo = (uint32_t)bufferAddr;
        memcpy(pkt + 40, &nz, 4);
        memcpy(pkt + 44, &lo, 4);
        debug_send_raw(pkt, (int)sizeof(pkt));
    }
#else
    (void)prevMode;
#endif
    return vote != 0;
}

// See the big comment above g_hdr2200IsHdr for the full rationale.
// More: docs/code-comments.md (plugin/source/zones.c:265)
void detectHdr2200Format(uint64_t bufferAddr)
{
#if HDR2200_FASTPATH
    if (detectHdr2200Fast(bufferAddr)) return; // decisive: no streak, no throttle, no noise floor
#endif
    if (g_hdr2200Countdown > 0) { g_hdr2200Countdown--; return; }
    g_hdr2200Countdown = HDR2200_DETECT_INTERVAL;

    uint32_t nSamples = g_numZones < HDR2200_DETECT_SAMPLES ? g_numZones : HDR2200_DETECT_SAMPLES;
    if (nSamples < 2) return; // need at least 2 samples to compare anything

    uint8_t sdrR[HDR2200_DETECT_SAMPLES], sdrG[HDR2200_DETECT_SAMPLES], sdrB[HDR2200_DETECT_SAMPLES];
    uint8_t hdrR[HDR2200_DETECT_SAMPLES], hdrG[HDR2200_DETECT_SAMPLES], hdrB[HDR2200_DETECT_SAMPLES];

    uint32_t got = 0;
    for (uint32_t i = 0; i < nSamples; i++) {
        uint64_t off = getTiledElementByteOffset(&kParamsBase, g_zoneX[i], g_zoneY[i]);
        if (off + 4 > g_readableLimit) continue; // same safety check sampleZoneAverage uses
        uint32_t px;
        memcpy(&px, (const void*)(bufferAddr + off), 4);
        unpackA8B8G8R8_to_rgb888(px, &sdrR[got], &sdrG[got], &sdrB[got]);
        unpackA2R10G10B10_BT2020_PQ_to_rgb888(px, &hdrR[got], &hdrG[got], &hdrB[got]);
        got++;
    }
    if (got < 2) return;

    int32_t sdrTv = 0, hdrTv = 0;
    for (uint32_t i = 1; i < got; i++) {
        sdrTv += abs((int)sdrR[i] - (int)sdrR[i-1]) + abs((int)sdrG[i] - (int)sdrG[i-1]) + abs((int)sdrB[i] - (int)sdrB[i-1]);
        hdrTv += abs((int)hdrR[i] - (int)hdrR[i-1]) + abs((int)hdrG[i] - (int)hdrG[i-1]) + abs((int)hdrB[i] - (int)hdrB[i-1]);
    }

    int32_t winnerTv = sdrTv < hdrTv ? sdrTv : hdrTv;
    if (winnerTv < HDR2200_NOISE_FLOOR) return; // too flat/degenerate to trust -- e.g. a loading screen

    if (hdrTv < sdrTv) {
        g_hdr2200Streak = (g_hdr2200Streak > 0) ? (g_hdr2200Streak + 1) : 1;
    } else {
        g_hdr2200Streak = (g_hdr2200Streak < 0) ? (g_hdr2200Streak - 1) : -1;
    }

    if (g_hdr2200Streak >= HDR2200_STREAK_THRESHOLD) {
        g_hdr2200IsHdr = 1;
        g_hdr2200Streak = 0;
    } else if (g_hdr2200Streak <= -HDR2200_STREAK_THRESHOLD) {
        g_hdr2200IsHdr = 0;
        g_hdr2200Streak = 0;
    }

#if (__FINAL__) == 0
    // Diagnostic-only telemetry, same debug_send_raw/[dev]-ini idiom as the format-change packet above.
    // More: docs/code-comments.md (plugin/source/zones.c:323)
    {
        uint8_t hdrDiagPacket[16];
        memcpy(hdrDiagPacket + 0,  &sdrTv,          4);
        memcpy(hdrDiagPacket + 4,  &hdrTv,          4);
        int32_t streakSnapshot = g_hdr2200Streak; // read the volatile once into a plain local -- memcpy's const void* param can't take a volatile pointer without discarding the qualifier (real warning, harmless but worth silencing cleanly)
        memcpy(hdrDiagPacket + 8,  &streakSnapshot,  4);
        uint32_t isHdrU32 = (uint32_t)g_hdr2200IsHdr;
        memcpy(hdrDiagPacket + 12, &isHdrU32,       4);
        debug_send_raw(hdrDiagPacket, sizeof(hdrDiagPacket));
    }
#endif
}

// v3.5: YouTube (the PS4 app) registers 0x88740000 once HDR is on, but while it plays SDR video, or...
// More: docs/code-comments.md (plugin/source/zones.c:353)
void detectPq8bitMisregistration(uint64_t bufferAddr)
{
    uint32_t nSamples = g_numZones < PQ8BIT_DETECT_SAMPLES ? g_numZones : PQ8BIT_DETECT_SAMPLES;
    uint32_t words[PQ8BIT_DETECT_SAMPLES];
    uint32_t got = 0;
    for (uint32_t i = 0; i < nSamples; i++) {
        uint64_t off = getTiledElementByteOffset(&kParamsBase, g_zoneX[i], g_zoneY[i]);
        if (off + 4 > g_readableLimit) continue; // same safety check as detectHdr2200Format
        memcpy(&words[got], (const void*)(bufferAddr + off), 4);
        got++;
    }

    int vote = pq8bitVote(words, got);
    int prevMode = g_pq8bitMode;
    if (vote > 0)      g_pq8bitMode = 1;
    else if (vote < 0) g_pq8bitMode = 0;
    // vote == 0: leave the mode alone

#if (__FINAL__) == 0
    // Diagnostic-only, same debug_send_raw idiom as above.
    // More: docs/code-comments.md (plugin/source/zones.c:390)
    static uint32_t s_pq8Count = 0;
    if ((int)g_pq8bitMode != prevMode || (s_pq8Count++ % 30u) == 0) {
        uint8_t pkt[48];
        memset(pkt, 0, sizeof(pkt));
        memcpy(pkt, "PQ8C", 4);
        uint32_t ff = 0;
        for (uint32_t i = 0; i < got; i++) if ((words[i] >> 24) == 0xFFu) ff++;
        pkt[4] = (uint8_t)(int8_t)vote;
        pkt[5] = (uint8_t)got;
        pkt[6] = (uint8_t)ff;
        pkt[7] = (uint8_t)g_pq8bitMode;
        memcpy(pkt + 8, words, got * 4);
        uint32_t nz = g_numZones, lo = (uint32_t)bufferAddr;
        memcpy(pkt + 40, &nz, 4);
        memcpy(pkt + 44, &lo, 4);
        debug_send_raw(pkt, (int)sizeof(pkt));
    }
#endif
}

void sampleZoneAverage(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                               uint32_t cx, uint32_t cy, uint8_t *outR, uint8_t *outG, uint8_t *outB)
{
    // v2.0: sample radius is g_config.scanDepth (was the fixed ZONE_SAMPLE_RADIUS #define). int32_t cast...
    // More: docs/code-comments.md (plugin/source/zones.c:425)
    int32_t radius = (int32_t)g_config.scanDepth;
    // v2.2: float accumulators, not uint32_t -- the per-sample gamma LUTs (g_gammaLutR/G/B) now return...
    // More: docs/code-comments.md (plugin/source/zones.c:429)
    float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
    uint32_t n = 0;
    for (int32_t dy = -radius; dy <= radius; dy++) {
        for (int32_t dx = -radius; dx <= radius; dx++) {
            int32_t sx = (int32_t)cx + dx;
            int32_t sy = (int32_t)cy + dy;
            if (sx < 0 || sy < 0 || sx >= SCREEN_WIDTH || sy >= SCREEN_HEIGHT) continue;
            uint64_t off = getTiledElementByteOffset(p, (uint32_t)sx, (uint32_t)sy);
            // Keep the bounds check before touching real memory: an out-of-range tiled offset has crashed a title before.
            // More: docs/code-comments.md (plugin/source/zones.c:444)
            if (off + 4 > g_readableLimit) continue;
            uint32_t px;
            memcpy(&px, (const void*)(bufferAddr + off), 4);
            uint8_t r, g, b;
            unpack(px, &r, &g, &b);
            // v2.2: per-channel gamma (gamma_r/g/b) applied to THIS sample now, before it's folded into the zone...
            // More: docs/code-comments.md (plugin/source/zones.c:455)
            sumR += g_gammaLutR[r];
            sumG += g_gammaLutG[g];
            sumB += g_gammaLutB[b];
            n++;
        }
    }
    if (n == 0) n = 1;
    float avgR = sumR / (float)n, avgG = sumG / (float)n, avgB = sumB / (float)n;
    // This IS the one deliberate round-to-byte boundary in the whole v2.2 pipeline before the final...
    // More: docs/code-comments.md (plugin/source/zones.c:468)
    #define CLAMPF(v) ((v) < 0.0f ? 0.0f : ((v) > 255.0f ? 255.0f : (v)))
    *outR = (uint8_t)(CLAMPF(avgR) + 0.5f);
    *outG = (uint8_t)(CLAMPF(avgG) + 0.5f);
    *outB = (uint8_t)(CLAMPF(avgB) + 0.5f);
    #undef CLAMPF
}

