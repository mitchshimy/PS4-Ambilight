// tiling.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. Confirmed-correct BASE detile math (offset formula), no Neo branch.
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
// Detile math -- confirmed-correct BASE params only (/).
// Identical formulas to ps4_detile_2dthin.c / detile_verify_probe,
// just without the Neo branch, which has no reason to run every frame
// now that base is the settled answer for this hardware.
// (TileParams itself is declared in ambient_internal.h -- zones.c
// needs it too, for sampleZoneAverage/detectHdr2200Format's calls
// into getTiledElementByteOffset below.)
// ============================================================

const TileParams kParamsBase = {
    0xc, 8, 1, 1, 16, 128, 64, 1920, 512, 8, 3, 4
};

// paddedHeight isn't in TileParams (only paddedWidth is used by the
// offset math itself), but it's needed here for the v1.1 bounds check
// below -- 1088 for base params, per (ceil(1080/64)*64).
#define BASE_PADDED_HEIGHT 1088
#define BASE_PADDED_BUFFER_BYTES ((uint64_t)1920 * BASE_PADDED_HEIGHT * 4)

static uint32_t getElementIndex32(uint32_t x, uint32_t y)
{
    uint32_t elem = 0;
    elem |= ((x >> 0) & 0x1) << 0;
    elem |= ((x >> 1) & 0x1) << 1;
    elem |= ((y >> 0) & 0x1) << 2;
    elem |= ((x >> 2) & 0x1) << 3;
    elem |= ((y >> 1) & 0x1) << 4;
    elem |= ((y >> 2) & 0x1) << 5;
    return elem;
}

static uint32_t getPipeIndex(uint32_t x, uint32_t y) // base only -> P8_32x32_16x16
{
    uint32_t pipe = 0;
    pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
    pipe |= (((x >> 4) ^ (y >> 4))            & 0x1) << 1;
    pipe |= (((x >> 5) ^ (y >> 5))            & 0x1) << 2;
    return pipe;
}

static uint32_t fastIntLog2_(uint32_t i)
{
    uint32_t log2 = 0;
    while ((i >>= 1) != 0) log2++;
    return log2;
}

static uint32_t getBankIndex(uint32_t x, uint32_t y, const TileParams *p) // base only -> numBanks=16
{
    const uint32_t x_shift_offset = fastIntLog2_(p->bankWidth * p->numPipes);
    const uint32_t y_shift_offset = fastIntLog2_(p->bankHeight);
    const uint32_t xs = x >> x_shift_offset;
    const uint32_t ys = y >> y_shift_offset;
    uint32_t bank = 0;
    bank |= (((xs >> 3) ^ (ys >> 6))             & 0x1) << 0;
    bank |= (((xs >> 4) ^ (ys >> 5) ^ (ys >> 6)) & 0x1) << 1;
    bank |= (((xs >> 5) ^ (ys >> 4))             & 0x1) << 2; // confirmed against GnmTiler.cpp:310, see main.c v2.1 fix
    bank |= (((xs >> 6) ^ (ys >> 3))             & 0x1) << 3;
    return bank;
}

uint64_t getTiledElementByteOffset(const TileParams *p, uint32_t x, uint32_t y)
{
    uint64_t element_index = getElementIndex32(x, y);
    uint64_t pipe = getPipeIndex(x, y);
    uint64_t bank = getBankIndex(x, y, p);

    uint32_t tile_bytes = (8 * 8 * 1 * 32 * 1 + 7) / 8; // 256, our fixed 32bpp case
    uint64_t element_offset = element_index * 32;

    uint64_t macro_tile_bytes = (p->macroTileWidth / 8) * (p->macroTileHeight / 8)
                                 * tile_bytes / (p->numPipes * p->numBanks);
    uint64_t macro_tiles_per_row = p->paddedWidth / p->macroTileWidth;
    uint64_t macro_tile_row_index = y / p->macroTileHeight;
    uint64_t macro_tile_column_index = x / p->macroTileWidth;
    uint64_t macro_tile_index = (macro_tile_row_index * macro_tiles_per_row) + macro_tile_column_index;
    uint64_t macro_tile_offset = macro_tile_index * macro_tile_bytes;

    uint64_t tile_row_index = (y / 8) % p->bankHeight;
    uint64_t tile_column_index = ((x / 8) / p->numPipes) % p->bankWidth;
    uint64_t tile_index = (tile_row_index * p->bankWidth) + tile_column_index;
    uint64_t tile_offset = tile_index * tile_bytes;

    uint64_t total_offset = (macro_tile_offset + tile_offset) * 8 + element_offset;
    uint64_t bitOffset = total_offset & 0x7;
    total_offset /= 8;

    uint32_t pipeInterleaveMask = (1u << p->pipeInterleaveBits) - 1;
    uint64_t pipe_interleave_offset = total_offset & pipeInterleaveMask;
    uint64_t offset = total_offset >> p->pipeInterleaveBits;

    uint64_t finalByteOffset = pipe_interleave_offset |
        (pipe   << (p->pipeInterleaveBits)) |
        (bank   << (p->pipeInterleaveBits + p->pipeBits)) |
        (offset << (p->pipeInterleaveBits + p->pipeBits + p->bankBits));

    return ((finalByteOffset << 3) | bitOffset) / 8;
}

