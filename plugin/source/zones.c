// zones.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. Screen-edge zone geometry, HDR2200 live format auto-detect, 0x88740000 8-bit check, zone sampling.
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

// v3.0: effective margin per edge = whatever letterbox.c's auto-
// detection currently has committed for that edge (0 when the feature
// is off or hasn't found a bar there -- see that file). This used to
// be a manual g_config.margin* PLUS the auto value; the manual fields
// were removed once auto letterbox could fully cover what they were
// for (see CHANGELOG) -- these are kept as their own named functions,
// rather than reading g_autoLetterboxTop etc. directly below, purely
// for the safety clamp: no edge's margin can reach past the screen's
// own midpoint. That's the same limit BorderProcessor.applyKnownBorderCrop
// itself applies ("never crop more than half of either axis") -- here
// it also protects the unsigned subtraction below (SCREEN_HEIGHT - 1 -
// marginTop - marginBottom, etc.) from underflowing, which in
// principle a bad detection on two opposite edges could still cause
// without it. Detection itself is now bounded far tighter than this
// (MAX_BAR_DEPTH_V/H in letterbox.c, screen/6), so in practice this
// clamp is only a backstop against a bad value, not a limit that
// detection ever gets close to.
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

// v3.6: per-pass alpha byte check for 0x80002200, tried before the smoothness
// detector below. HITMAN 3 and RDR2 register this ID whether HDR is on or off,
// and the smoothness detector took 53 s to flip on a real HDR-on capture
// (about 46 s of dark frames under its noise floor, then 4 checks 2.2 s
// apart), with the strip decoded as SDR the whole time. The words were PQ from
// the first frame. Vote logic and the numbers behind it are in hdr2200_vote.h
// and docs/debugging/hdr2200-alpha-detection.md.
//
// Same idea as detectPq8bitMisregistration() below, with the sides swapped:
// here 0xff in the alpha byte means SDR and anything that looks like 0b11xxxxxx
// without 0xff means PQ. Returns 1 if the vote was decisive, and then the mode
// is already set for THIS frame and the smoothness detector is skipped. 0 means
// hold, and the caller falls through to it.
static int detectHdr2200Fast(uint64_t bufferAddr)
{
    uint32_t nSamples = g_numZones < HDR2200_VOTE_SAMPLES ? g_numZones : HDR2200_VOTE_SAMPLES;
    uint32_t words[HDR2200_VOTE_SAMPLES];
    uint32_t got = 0;
    for (uint32_t i = 0; i < nSamples; i++) {
        uint64_t off = getTiledElementByteOffset(&kParamsBase, g_zoneX[i], g_zoneY[i]);
        if (off + 4 > BASE_PADDED_BUFFER_BYTES) continue; // same safety check as detectHdr2200Format
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
    // Diagnostic-only, same debug_send_raw idiom as the PQ8C packet below. Sent
    // when the mode changes and otherwise about once every 30 checks.
    //   [0:4]   "HDRV"
    //   [4]     vote, int8: +1 PQ / -1 SDR / 0 hold
    //   [5]     words read
    //   [6]     of those, how many had the PQ signature (top bits 11, top byte not 0xff)
    //   [7]     mode after this check (1 = PQ)
    //   [8:40]  the 8 words, uint32 LE (unread slots are 0)
    //   [40:44] g_numZones
    //   [44:48] low 32 bits of bufferAddr
    // Same 48 byte payload as PQ8C, told apart by the magic.
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
// Cheap by construction: at most HDR2200_DETECT_SAMPLES extra 4-byte
// reads (8, capped regardless of how many zones are configured), only
// every HDR2200_DETECT_INTERVAL sampling passes, and only while the
// active format is the ambiguous 0x80002200 -- for every other format
// (i.e. every other title) this function is never called at all.
// v3.6: the per-pass alpha byte check above runs first, and this smoothness
// check only gets a say on the frames where that one holds.
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

// v3.5: YouTube (the PS4 app) registers 0x88740000 once HDR is on, but while
// it plays SDR video, or shows its own UI, the buffer holds plain 8-bit
// A8R8G8B8. It goes back to real 10-bit PQ for HDR video, and it never
// re-registers in between, so the format ID can't be trusted here. The PQ
// decode reads 0xffRRGGBB as R10 >= 1008 (over 8,700 nits) and clips, which
// is where the red/yellow/magenta came from.
//
// The tell is the alpha byte: a real 10-bit word has alpha in bits 31:30, so
// 0xff there would mean red at 8,700 nits or more. 8-bit content has it on
// every pixel. Vote logic and the numbers behind it are in pq8bit_vote.h and
// docs/debugging/youtube-hdr-8bit.md.
//
// Runs on EVERY pass, unthrottled, and before getUnpackFnForFormat() picks
// the decode function, on the same liveBufferAddr the zone loop reads next,
// so the mode is right for the frame it applies to. The first version waited
// for 3 agreeing checks 10 passes apart, which took about 0.75 s to switch
// and showed wrong colors the whole time, and there was no reason for it: it
// is 8 four byte reads, nothing next to the zone loop.
void detectPq8bitMisregistration(uint64_t bufferAddr)
{
    uint32_t nSamples = g_numZones < PQ8BIT_DETECT_SAMPLES ? g_numZones : PQ8BIT_DETECT_SAMPLES;
    uint32_t words[PQ8BIT_DETECT_SAMPLES];
    uint32_t got = 0;
    for (uint32_t i = 0; i < nSamples; i++) {
        uint64_t off = getTiledElementByteOffset(&kParamsBase, g_zoneX[i], g_zoneY[i]);
        if (off + 4 > BASE_PADDED_BUFFER_BYTES) continue; // same safety check as detectHdr2200Format
        memcpy(&words[got], (const void*)(bufferAddr + off), 4);
        got++;
    }

    int vote = pq8bitVote(words, got);
    int prevMode = g_pq8bitMode;
    if (vote > 0)      g_pq8bitMode = 1;
    else if (vote < 0) g_pq8bitMode = 0;
    // vote == 0: leave the mode alone

#if (__FINAL__) == 0
    // Diagnostic-only, same debug_send_raw idiom as above. Sent when the mode
    // changes and otherwise about once every 30 checks (a check per pass
    // would be ~30 packets a second for nothing).
    //   [0:4]   "PQ8C"
    //   [4]     vote, int8: +1 / -1 / 0
    //   [5]     words read
    //   [6]     of those, how many had alpha byte 0xff
    //   [7]     mode after this check
    //   [8:40]  the 8 words, uint32 LE (unread slots are 0)
    //   [40:44] g_numZones
    //   [44:48] low 32 bits of bufferAddr
    // 48 byte payload (58 on the wire) is a length nothing else uses.
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

