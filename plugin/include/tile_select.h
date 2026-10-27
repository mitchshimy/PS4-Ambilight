// tile_select.h -- scoring and verdict for the base-vs-Neo tiling check (v4.0), no SDK calls.
// A wrong layout keeps each 8x8 tile whole but misplaces it, so only steps across a tile edge tell them apart.
#ifndef TILE_SELECT_H
#define TILE_SELECT_H

#include <stdint.h>

#define TILE_MIN_PAIRS        64    // edge and inner pairs each layout needs before it is judged
#define TILE_STEP_CAP         96    // one pair counts at most this (sum over R, G, B), so a few hard edges can't decide it
#define TILE_FLAT_X100        200   // below a 2.00 mean step (of 255) the picture has nothing to judge by
#define TILE_MIN_EXCESS_X100  200   // the worse layout's edge step must top the picture's inner step by this (2.00)
#define TILE_EXCESS_RATIO     3     // and be at least this many times the better layout's excess
#define TILE_WIN_SLACK_X100   100   // the better layout's excess may not top the inner step by more than this (1.00)

typedef enum {
    TILE_VERDICT_NONE = 0,   // not enough readable pairs, or the check has not run
    TILE_VERDICT_FLAT,       // too little picture detail to tell
    TILE_VERDICT_BASE,       // the base layout fits
    TILE_VERDICT_NEO,        // the Neo layout fits
    TILE_VERDICT_UNCLEAR     // neither is clearly better, or the better one still looks scrambled
} TileVerdict;

typedef struct {
    uint64_t edgeSum;  uint32_t edgePairs;   // neighbours on either side of an 8x8 tile edge
    uint64_t innerSum; uint32_t innerPairs;  // neighbours inside one 8x8 tile
} TileScore;

// Sum over R, G, B of the absolute difference between two neighbouring pixels, capped at TILE_STEP_CAP.
static inline uint32_t tile_step(const uint8_t a[3], const uint8_t b[3])
{
    uint32_t d = 0;
    for (int c = 0; c < 3; c++) d += (a[c] > b[c]) ? (uint32_t)(a[c] - b[c]) : (uint32_t)(b[c] - a[c]);
    return d > TILE_STEP_CAP ? TILE_STEP_CAP : d;
}

static inline void tile_score_edge(TileScore *s, const uint8_t a[3], const uint8_t b[3])
{
    s->edgeSum += tile_step(a, b);
    s->edgePairs++;
}

static inline void tile_score_inner(TileScore *s, const uint8_t a[3], const uint8_t b[3])
{
    s->innerSum += tile_step(a, b);
    s->innerPairs++;
}

// Mean step per channel, times 100 (so 3.10 is 310). 0 when there are no pairs.
static inline uint32_t tile_mean_x100(uint64_t sum, uint32_t pairs)
{
    return pairs ? (uint32_t)((sum * 100u) / ((uint64_t)pairs * 3u)) : 0u;
}

// What a step inside a tile looks like in this picture: both layouts' inner means, averaged (x100).
static inline uint32_t tile_inner_ref_x100(const TileScore *base, const TileScore *neo)
{
    return (tile_mean_x100(base->innerSum, base->innerPairs) + tile_mean_x100(neo->innerSum, neo->innerPairs)) / 2u;
}

// The layout whose edge steps stay near the inner step is the one that fits; the other one is rougher by far.
static inline TileVerdict tile_verdict(const TileScore *base, const TileScore *neo)
{
    if (base->edgePairs < TILE_MIN_PAIRS || neo->edgePairs < TILE_MIN_PAIRS) return TILE_VERDICT_NONE;
    if (base->innerPairs < TILE_MIN_PAIRS || neo->innerPairs < TILE_MIN_PAIRS) return TILE_VERDICT_NONE;

    uint32_t eb = tile_mean_x100(base->edgeSum, base->edgePairs);
    uint32_t en = tile_mean_x100(neo->edgeSum, neo->edgePairs);
    if ((eb > en ? eb : en) < TILE_FLAT_X100) return TILE_VERDICT_FLAT;

    uint32_t ref = tile_inner_ref_x100(base, neo);
    uint32_t xb = eb > ref ? eb - ref : 0;
    uint32_t xn = en > ref ? en - ref : 0;
    uint32_t hi = xb > xn ? xb : xn;
    uint32_t lo = xb > xn ? xn : xb;

    if (hi < TILE_MIN_EXCESS_X100) return TILE_VERDICT_UNCLEAR;       // neither is clearly worse
    if (lo * TILE_EXCESS_RATIO > hi) return TILE_VERDICT_UNCLEAR;     // or they are too close to call
    if (lo > TILE_WIN_SLACK_X100) return TILE_VERDICT_UNCLEAR;        // the better one still looks scrambled
    return xb < xn ? TILE_VERDICT_BASE : TILE_VERDICT_NEO;
}

static inline const char *tile_verdict_name(uint32_t v)
{
    switch (v) {
        case TILE_VERDICT_FLAT:    return "flat picture";
        case TILE_VERDICT_BASE:    return "base fits";
        case TILE_VERDICT_NEO:     return "Neo fits";
        case TILE_VERDICT_UNCLEAR: return "unclear";
        default:                   return "no data";
    }
}

#endif // TILE_SELECT_H
