// tile_check.c -- part of ps4_ambient_light (v4.0), plugin only (the shell feed does not build it).
// Console model and a base-vs-Neo tiling check for the report file. It reads nothing unless report_file=1.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "tile_select.h"

int32_t (*sceKernelIsNeoModePtr)(void) = NULL;   // main.c resolves it; stays NULL where libkernel lacks it

volatile uint32_t g_tileVerdict = TILE_VERDICT_NONE;
volatile uint32_t g_tileBaseX100, g_tileNeoX100, g_tileInnerX100, g_tilePairs, g_tileTries;

#define TILE_GRID_COLS   26    // 26 x 16 points, four pair kinds each: 832 edge + 832 inner pairs per layout
#define TILE_GRID_ROWS   16
#define TILE_CHECK_EVERY 120   // sampler passes between checks
#define TILE_CHECK_TRIES 30    // then it stops for the session

int32_t ambient_console_neo(void)
{
    static int32_t cached = -2;   // -2 not asked yet; only a real answer is kept
    if (cached == -2 && sceKernelIsNeoModePtr != NULL) cached = sceKernelIsNeoModePtr() ? 1 : 0;
    return cached == -2 ? -1 : cached;
}

// Same bound as sampleZoneAverage: an offset past g_readableLimit is skipped, never read.
static bool tile_px(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                    uint32_t x, uint32_t y, uint8_t rgb[3])
{
    uint64_t off = getTiledElementByteOffset(p, x, y);
    if (off + 4 > g_readableLimit) return false;
    uint32_t px;
    memcpy(&px, (const void *)(bufferAddr + off), 4);
    unpack(px, &rgb[0], &rgb[1], &rgb[2]);
    return true;
}

static bool tile_pair_h(const TileParams *p, uint64_t buf, PixelUnpackFn unpack,
                        uint32_t x, uint32_t y, uint8_t a[3], uint8_t b[3])
{
    return tile_px(p, buf, unpack, x, y, a) && tile_px(p, buf, unpack, x + 1, y, b);
}

static bool tile_pair_v(const TileParams *p, uint64_t buf, PixelUnpackFn unpack,
                        uint32_t x, uint32_t y, uint8_t a[3], uint8_t b[3])
{
    return tile_px(p, buf, unpack, x, y, a) && tile_px(p, buf, unpack, x, y + 1, b);
}

// Steps across and inside 8x8 tiles, read through one layout, on a fixed grid.
static void tile_score_layout(const TileParams *p, uint64_t buf, PixelUnpackFn unpack, TileScore *s)
{
    memset(s, 0, sizeof(*s));
    for (uint32_t j = 0; j < TILE_GRID_ROWS; j++) {
        for (uint32_t k = 0; k < TILE_GRID_COLS; k++) {
            uint8_t a[3], b[3];
            uint32_t xt = 8u * (1u + 9u * k);     // left edge of a tile column
            uint32_t yh = 5u + 67u * j;
            uint32_t xv = 11u + 71u * k;
            uint32_t yt = 8u * (1u + 8u * j);     // top edge of a tile row
            if (tile_pair_h(p, buf, unpack, xt - 1u, yh, a, b)) tile_score_edge(s, a, b);
            if (tile_pair_h(p, buf, unpack, xt + 3u, yh, a, b)) tile_score_inner(s, a, b);
            if (tile_pair_v(p, buf, unpack, xv, yt - 1u, a, b)) tile_score_edge(s, a, b);
            if (tile_pair_v(p, buf, unpack, xv, yt + 3u, a, b)) tile_score_inner(s, a, b);
        }
    }
}

static TileVerdict tile_check_evaluate(uint64_t buf, PixelUnpackFn unpack, TileScore *base, TileScore *neo)
{
    tile_score_layout(&kParamsBase, buf, unpack, base);
    tile_score_layout(&kParamsNeo, buf, unpack, neo);
    return tile_verdict(base, neo);
}

void ambient_tile_check_tick(uint64_t bufferAddr, PixelUnpackFn unpack)
{
    static uint32_t passes = 0;
    if (!g_reportEnabled || unpack == NULL || bufferAddr == 0) return;
    if (g_tileTries >= TILE_CHECK_TRIES) return;
    if (g_tileVerdict == TILE_VERDICT_BASE || g_tileVerdict == TILE_VERDICT_NEO) return;
    if ((passes++ % TILE_CHECK_EVERY) != 0) return;

    TileScore base, neo;
    TileVerdict v = tile_check_evaluate(bufferAddr, unpack, &base, &neo);

    g_tileBaseX100  = tile_mean_x100(base.edgeSum, base.edgePairs);
    g_tileNeoX100   = tile_mean_x100(neo.edgeSum, neo.edgePairs);
    g_tileInnerX100 = tile_inner_ref_x100(&base, &neo);
    g_tilePairs     = base.edgePairs < neo.edgePairs ? base.edgePairs : neo.edgePairs;
    g_tileTries     = g_tileTries + 1;
    g_tileVerdict   = (uint32_t)v;   // last, so the report never sees a verdict with old numbers
}
