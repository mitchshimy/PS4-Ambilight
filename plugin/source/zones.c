// zones.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. Screen-edge zone geometry, HDR2200 live format auto-detect, zone sampling.
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
// Zones: screen-edge points read as a small averaged neighborhood
// (not a single pixel) for stability. This is NOT a full-frame detile
// -- see the header note on cost.
//
// v2.0: per-edge counts, start corner, direction, index offset, and
// per-edge capture margin are all now config-driven (g_config, see
// the settings block above) instead of fixed #defines. The traversal-
// order logic (which corner/direction maps to which edge visit order
// and per-edge direction) was verified in Python for all 8 corner x
// direction combinations -- including an exact regression check
// against v1.0-v1.3's hardcoded bottom-left/clockwise output -- before
// being ported here. See.
// ============================================================

#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

// (MAX_TOTAL_ZONES is defined earlier, near DDP_HEADER_SIZE -- needed
// before wled_send_rgb_zones's first use of it via the MAX_ZONES alias.)
// g_numZones (actual, config-driven count) is always <= MAX_TOTAL_ZONES.
uint32_t g_zoneX[MAX_TOTAL_ZONES];
uint32_t g_zoneY[MAX_TOTAL_ZONES];
uint32_t g_numZones = 0; // set by buildZoneGeometry(), 0 until then

typedef enum { EDGE_LEFT, EDGE_TOP, EDGE_RIGHT, EDGE_BOTTOM } Edge;

// Generates `count` points along one screen edge, in either its
// canonical clockwise-from-bottom-left direction (reversed=false) or
// the opposite direction (reversed=true), honoring capture margins.
// Canonical directions: LEFT bottom->top, TOP left->right,
// RIGHT top->bottom, BOTTOM right->left -- matches v1.0-v1.3's
// original (now-default) bottom-left/clockwise behavior exactly.
//
// v2.7.4: denominator changed from (count-1) to count -- the old
// inclusive-both-endpoints spacing put a point at t=0 (this edge's
// start corner) AND a point at t=count-1 landing exactly on the FAR
// corner, which the next edge's own t=0 point also lands on (its own
// start corner is the same physical point). Every one of the 4 screen
// corners therefore got two zones reading the identical pixel --
// confirmed on real hardware: zone 0 and zone (numZones-1) sent
// bit-identical raw pixel values on every single sampled frame,
// exactly matching this math for a bottom-left-start clockwise loop
// (zone 0 and the last zone both land on the bottom-left corner).
// Dividing by `count` instead spaces `count` points evenly across
// [0, edge_length) -- t=0 still lands exactly on the start corner,
// but the last point (t=count-1) now falls one step short of the far
// corner, leaving that corner to be owned solely by the next edge's
// own t=0 point. No more double-sampled corners, and no more (count >
// 1 ? ... : 0) special case needed -- count is always >= 1 here (the
// loop above never runs this function body for count == 0), so
// dividing by count alone is always safe.
static void generateEdgePoints(Edge edge, bool reversed, uint32_t count, uint32_t *outIdx)
{
    for (uint32_t i = 0; i < count && *outIdx < MAX_TOTAL_ZONES; i++) {
        uint32_t t = reversed ? (count - 1 - i) : i;
        uint32_t x, y;
        switch (edge) {
        case EDGE_LEFT:
            x = g_config.marginLeft;
            y = (SCREEN_HEIGHT - 1 - g_config.marginBottom) -
                (t * (SCREEN_HEIGHT - 1 - g_config.marginTop - g_config.marginBottom)) / count;
            break;
        case EDGE_TOP:
            x = g_config.marginLeft +
                (t * (SCREEN_WIDTH - 1 - g_config.marginLeft - g_config.marginRight)) / count;
            y = g_config.marginTop;
            break;
        case EDGE_RIGHT:
            x = SCREEN_WIDTH - 1 - g_config.marginRight;
            y = g_config.marginTop +
                (t * (SCREEN_HEIGHT - 1 - g_config.marginTop - g_config.marginBottom)) / count;
            break;
        default: // EDGE_BOTTOM
            x = (SCREEN_WIDTH - 1 - g_config.marginRight) -
                (t * (SCREEN_WIDTH - 1 - g_config.marginLeft - g_config.marginRight)) / count;
            y = SCREEN_HEIGHT - 1 - g_config.marginBottom;
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
        // CCW = reverse of the CW-for-this-corner visit order, with
        // every edge's own direction also reversed. Verified in
        // Python against all 4 corners before porting.
        for (int i = 0; i < 4; i++) { order[i] = kCwFromBL[(startEdgeIdx + (3 - i)) % 4]; edgeReversed[i] = true; }
    }

    uint32_t idx = 0;
    for (int i = 0; i < 4; i++) {
        generateEdgePoints(order[i], edgeReversed[i], counts[order[i]], &idx);
    }
    g_numZones = idx;

    // led_offset: rotate which physical LED index 0 lands on, without
    // changing the shape/order of the sequence itself. Done as a
    // second pass (rotate the filled arrays) rather than folded into
    // the generation loop above, so this stays a single, independently
    // understandable step.
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

// See the big comment above g_hdr2200IsHdr for the full rationale.
// Cheap by construction: at most HDR2200_DETECT_SAMPLES extra 4-byte
// reads (8, capped regardless of how many zones are configured), only
// every HDR2200_DETECT_INTERVAL sampling passes, and only while the
// active format is the ambiguous 0x80002200 -- for every other format
// (i.e. every other title) this function is never called at all.
void detectHdr2200Format(uint64_t bufferAddr)
{
    if (g_hdr2200Countdown > 0) { g_hdr2200Countdown--; return; }
    g_hdr2200Countdown = HDR2200_DETECT_INTERVAL;

    uint32_t nSamples = g_numZones < HDR2200_DETECT_SAMPLES ? g_numZones : HDR2200_DETECT_SAMPLES;
    if (nSamples < 2) return; // need at least 2 samples to compare anything

    uint8_t sdrR[HDR2200_DETECT_SAMPLES], sdrG[HDR2200_DETECT_SAMPLES], sdrB[HDR2200_DETECT_SAMPLES];
    uint8_t hdrR[HDR2200_DETECT_SAMPLES], hdrG[HDR2200_DETECT_SAMPLES], hdrB[HDR2200_DETECT_SAMPLES];

    uint32_t got = 0;
    for (uint32_t i = 0; i < nSamples; i++) {
        uint64_t off = getTiledElementByteOffset(&kParamsBase, g_zoneX[i], g_zoneY[i]);
        if (off + 4 > BASE_PADDED_BUFFER_BYTES) continue; // same safety check sampleZoneAverage uses
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
    // Diagnostic-only telemetry, same debug_send_raw/[dev]-ini idiom as
    // the format-change packet above. This whole detection mechanism
    // was validated offline (a Python replica of this exact algorithm,
    // run against real captures, converged cleanly and plausibly), but
    // live behavior didn't settle -- meaning something between "the
    // algorithm is sound" and "the strip shows the right color" isn't
    // visible from here. Reports every actual check (already throttled
    // to once per HDR2200_DETECT_INTERVAL, so not spammy), not just on
    // a decision change, so the live sdrTv/hdrTv/streak numbers can be
    // compared directly against decode_verification_dump.py's own
    // smoothness-heuristic output for the same real capture.
    //   [0:4]  sdrTv        -- int32 LE
    //   [4:8]  hdrTv        -- int32 LE
    //   [8:12] streak       -- int32 LE (post-update, pre-reset)
    //   [12:16] isHdr       -- uint32 LE, 0/1, current live decision
    // 16-byte length is distinct from the existing 8-byte format-change
    // packet, matching this project's own dispatch-by-length convention.
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

void sampleZoneAverage(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                               uint32_t cx, uint32_t cy, uint8_t *outR, uint8_t *outG, uint8_t *outB)
{
    // v2.0: sample radius is g_config.scanDepth (was the fixed
    // ZONE_SAMPLE_RADIUS #define). int32_t cast since scanDepth is
    // unsigned but used in a signed loop range below.
    int32_t radius = (int32_t)g_config.scanDepth;
    // v2.2: float accumulators, not uint32_t -- the per-sample gamma
    // LUTs (g_gammaLutR/G/B) now return unclamped/unrounded float, so
    // rounding each sample to a byte before summing would reintroduce
    // exactly the precision loss this change is meant to avoid. This
    // only touches (2*radius+1)^2 samples per zone -- basic float
    // add/multiply, no libm -- so it's the same cost class as the
    // uint32_t version it replaces.
    float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
    uint32_t n = 0;
    for (int32_t dy = -radius; dy <= radius; dy++) {
        for (int32_t dx = -radius; dx <= radius; dx++) {
            int32_t sx = (int32_t)cx + dx;
            int32_t sy = (int32_t)cy + dy;
            if (sx < 0 || sy < 0 || sx >= SCREEN_WIDTH || sy >= SCREEN_HEIGHT) continue;
            uint64_t off = getTiledElementByteOffset(p, (uint32_t)sx, (uint32_t)sy);
            // v1.1: bounds-check before touching real memory. A correct
            // offset formula should never produce something out of
            // range for a valid (sx,sy), but this project has already
            // paid for bad-memory-access crashes once --
            // cheap insurance against a future edge case or a param
            // typo, not a sign anything is currently wrong.
            if (off + 4 > BASE_PADDED_BUFFER_BYTES) continue;
            uint32_t px;
            memcpy(&px, (const void*)(bufferAddr + off), 4);
            uint8_t r, g, b;
            unpack(px, &r, &g, &b);
            // v2.2: per-channel gamma (gamma_r/g/b) applied to THIS
            // sample now, before it's folded into the zone sum --
            // see the big comment above applyColorProcessing for why
            // this one specific step moved here instead of staying in
            // the post-average pipeline like everything else.
            sumR += g_gammaLutR[r];
            sumG += g_gammaLutG[g];
            sumB += g_gammaLutB[b];
            n++;
        }
    }
    if (n == 0) n = 1;
    float avgR = sumR / (float)n, avgG = sumG / (float)n, avgB = sumB / (float)n;
    // This IS the one deliberate round-to-byte boundary in the whole
    // v2.2 pipeline before the final output: the legacy post-average
    // stages (applyColorProcessing) take uint8_t in/out to stay a
    // drop-in match for the pre-v2.2 function signature and call
    // site. Everything from here through applyColorProcessing's
    // internals is unclamped int32_t, with only ONE more round+clamp
    // at the very end -- see that function.
    #define CLAMPF(v) ((v) < 0.0f ? 0.0f : ((v) > 255.0f ? 255.0f : (v)))
    *outR = (uint8_t)(CLAMPF(avgR) + 0.5f);
    *outG = (uint8_t)(CLAMPF(avgG) + 0.5f);
    *outB = (uint8_t)(CLAMPF(avgB) + 0.5f);
    #undef CLAMPF
}

