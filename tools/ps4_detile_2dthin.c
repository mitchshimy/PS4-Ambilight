// ps4_detile_2dthin.c Standalone C port of sce::GpuAddress::Tiler2d::detileSurface /...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:1)

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* ---- enums (mirroring Gnm::ArrayMode / MicroTileMode / PipeConfig) ---- */

typedef enum {
    kArrayMode2dTiledThin = 0x4
} ArrayMode;

typedef enum {
    kMicroTileModeDisplay = 0x0
} MicroTileMode;

typedef enum {
    kPipeConfigP8_32x32_16x16 = 0xc,
    kPipeConfigP16            = 0x12
} PipeConfig;

/* Fixed constants (GCN hardware constants, not per-surface) */
#define kMicroTileWidth   8u
#define kMicroTileHeight  8u
#define kBankInterleave   1u   /* from GnmTiler.cpp GnmConstant usage */

typedef struct {
    const char *name;
    PipeConfig  pipeConfig;
    uint32_t    numPipes;
    uint32_t    bankWidth;
    uint32_t    bankHeight;
    uint32_t    numBanks;
    uint32_t    macroTileAspect;
    uint32_t    macroTileWidth;
    uint32_t    macroTileHeight;
    uint32_t    paddedWidth;
    uint32_t    paddedHeight;
    uint32_t    tileSplitBytes;
    uint32_t    pipeInterleaveBytes;
    uint32_t    pipeInterleaveBits;
    uint32_t    pipeBits;
    uint32_t    bankBits;
} TileParams;

// Derived by hand from computeSurfaceTileMode / computeSurfaceMacroTileMode / Tiler2d::init for...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:77)
static const TileParams kParamsBase = {
    .name = "base (non-Neo)",
    .pipeConfig = kPipeConfigP8_32x32_16x16,
    .numPipes = 8,
    .bankWidth = 1,
    .bankHeight = 1,
    .numBanks = 16,
    .macroTileAspect = 2,
    .macroTileWidth = 128,
    .macroTileHeight = 64,
    .paddedWidth = 1920,   /* 1920 / 128 = 15, already aligned */
    .paddedHeight = 1088,  /* ceil(1080/64)*64 = 17*64 */
    .tileSplitBytes = 512,
    .pipeInterleaveBytes = 256,
    .pipeInterleaveBits = 8,   /* log2(256) */
    .pipeBits = 3,             /* log2(8) */
    .bankBits = 4,             /* log2(16) */
};

static const TileParams kParamsNeo = {
    .name = "Neo (PS4 Pro)",
    .pipeConfig = kPipeConfigP16,
    .numPipes = 16,
    .bankWidth = 1,
    .bankHeight = 2,
    .numBanks = 8,
    .macroTileAspect = 1,
    .macroTileWidth = 128,
    .macroTileHeight = 128,
    .paddedWidth = 1920,   /* 1920 / 128 = 15, already aligned */
    .paddedHeight = 1152,  /* ceil(1080/128)*128 = 9*128 */
    .tileSplitBytes = 512,
    .pipeInterleaveBytes = 256,
    .pipeInterleaveBits = 8,
    .pipeBits = 4,             /* log2(16) */
    .bankBits = 3,             /* log2(8) */
};

/* ---- verbatim-ported helper functions (GnmTiler.cpp lines ~128-318) ---- */

// 32bpp display-microtile element swizzle (kMicroTileModeDisplay, bpp=32 case of getElementIndex...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:120)
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

static uint32_t getPipeIndex(uint32_t x, uint32_t y, PipeConfig pipeCfg)
{
    uint32_t pipe = 0;
    switch (pipeCfg) {
    case kPipeConfigP8_32x32_16x16:
        pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
        pipe |= (((x >> 4) ^ (y >> 4))            & 0x1) << 1;
        pipe |= (((x >> 5) ^ (y >> 5))            & 0x1) << 2;
        break;
    case kPipeConfigP16:
        pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
        pipe |= (((x >> 4) ^ (y >> 4))            & 0x1) << 1;
        pipe |= (((x >> 5) ^ (y >> 5))            & 0x1) << 2;
        pipe |= (((x >> 6) ^ (y >> 5))            & 0x1) << 3;
        break;
    default:
        fprintf(stderr, "getPipeIndex: unsupported pipeCfg 0x%x\n", pipeCfg);
        break;
    }
    return pipe;
}

static uint32_t fastIntLog2(uint32_t i)
{
    uint32_t log2 = 0;
    while ((i >>= 1) != 0) log2++;
    return log2;
}

static uint32_t getBankIndex(uint32_t x, uint32_t y, uint32_t bank_width,
                              uint32_t bank_height, uint32_t num_banks, uint32_t num_pipes)
{
    const uint32_t x_shift_offset = fastIntLog2(bank_width * num_pipes);
    const uint32_t y_shift_offset = fastIntLog2(bank_height);
    const uint32_t xs = x >> x_shift_offset;
    const uint32_t ys = y >> y_shift_offset;
    uint32_t bank = 0;
    switch (num_banks) {
    case 2:
        bank |= (((xs >> 3) ^ (ys >> 3)) & 0x1) << 0;
        break;
    case 4:
        bank |= (((xs >> 3) ^ (ys >> 4)) & 0x1) << 0;
        bank |= (((xs >> 4) ^ (ys >> 3)) & 0x1) << 1;
        break;
    case 8:
        bank |= (((xs >> 3) ^ (ys >> 5))            & 0x1) << 0;
        bank |= (((xs >> 4) ^ (ys >> 4) ^ (ys >> 5)) & 0x1) << 1;
        bank |= (((xs >> 5) ^ (ys >> 3))             & 0x1) << 2;
        break;
    case 16:
        bank |= (((xs >> 3) ^ (ys >> 6))             & 0x1) << 0;
        bank |= (((xs >> 4) ^ (ys >> 5) ^ (ys >> 6)) & 0x1) << 1;
        bank |= (((xs >> 5) ^ (ys >> 4))             & 0x1) << 2;
        bank |= (((xs >> 6) ^ (ys >> 3))             & 0x1) << 3;
        break;
    default:
        fprintf(stderr, "getBankIndex: invalid num_banks %u\n", num_banks);
        break;
    }
    return bank;
}

// core address-swizzle function, ported from Tiler2d::getTiledElementBitOffset (GnmTiler.cpp lines...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:198)
static uint64_t getTiledElementByteOffset(const TileParams *p, uint32_t x, uint32_t y)
{
    uint64_t element_index = getElementIndex32(x, y);

    uint64_t pipe = getPipeIndex(x, y, p->pipeConfig);
    uint64_t bank = getBankIndex(x, y, p->bankWidth, p->bankHeight, p->numBanks, p->numPipes);

    uint32_t tile_bytes = (kMicroTileWidth * kMicroTileHeight * 1 /*thickness*/ * 32 /*bpe*/ * 1 /*frag*/ + 7) / 8;
    /* = 256 bytes for our case */

    uint64_t element_offset = element_index * 32; /* * bitsPerElement */

    /* tile split: our tile_bytes (256) < tileSplitBytes (512), so this
     * branch never triggers for our surface -- kept for completeness. */
    uint64_t slices_per_tile = 1;
    uint64_t tile_split_slice = 0;
    if (tile_bytes > p->tileSplitBytes) {
        slices_per_tile = tile_bytes / p->tileSplitBytes;
        tile_split_slice = element_offset / (p->tileSplitBytes * 8);
        element_offset %= (p->tileSplitBytes * 8);
        tile_bytes = p->tileSplitBytes;
    }

    uint64_t macro_tile_bytes = (p->macroTileWidth / kMicroTileWidth) * (p->macroTileHeight / kMicroTileHeight)
                                 * tile_bytes / (p->numPipes * p->numBanks);
    uint64_t macro_tiles_per_row = p->paddedWidth / p->macroTileWidth;
    uint64_t macro_tile_row_index = y / p->macroTileHeight;
    uint64_t macro_tile_column_index = x / p->macroTileWidth;
    uint64_t macro_tile_index = (macro_tile_row_index * macro_tiles_per_row) + macro_tile_column_index;
    uint64_t macro_tile_offset = macro_tile_index * macro_tile_bytes;
    /* single slice (z=0), so slice_offset is always 0 for our case */
    uint64_t slice_offset = 0;
    (void)slices_per_tile;
    (void)tile_split_slice;

    uint64_t tile_row_index = (y / kMicroTileHeight) % p->bankHeight;
    uint64_t tile_column_index = ((x / kMicroTileWidth) / p->numPipes) % p->bankWidth;
    uint64_t tile_index = (tile_row_index * p->bankWidth) + tile_column_index;
    uint64_t tile_offset = tile_index * tile_bytes;

    /* no bank/pipe swizzle mask set (plain display buffer), no slice
     * rotation (single slice) -- pipe/bank pass through unmodified */

    uint64_t total_offset = (slice_offset + macro_tile_offset + tile_offset) * 8 + element_offset;
    uint64_t bitOffset = total_offset & 0x7;
    total_offset /= 8;

    uint32_t pipeInterleaveMask = (1u << p->pipeInterleaveBits) - 1;
    uint64_t pipe_interleave_offset = total_offset & pipeInterleaveMask;
    uint64_t offset = total_offset >> p->pipeInterleaveBits;

    uint64_t finalByteOffset = pipe_interleave_offset |
        (pipe   << (p->pipeInterleaveBits)) |
        (bank   << (p->pipeInterleaveBits + p->pipeBits)) |
        (offset << (p->pipeInterleaveBits + p->pipeBits + p->bankBits));

    uint64_t finalBitOffset = (finalByteOffset << 3) | bitOffset;
    return finalBitOffset / 8; /* our element is always byte-aligned (32bpp) */
}

// Detile a full 1920x1080 32bpp A2R10G10B10_SRGB surface. tiledPixels: pointer to the full tiled...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:266)
void detileSurface_1920x1080_A2R10G10B10(const TileParams *p,
                                          const uint8_t *tiledPixels,
                                          uint8_t *outLinearPixels)
{
    const uint32_t width = 1920;
    const uint32_t height = 1080;
    const uint32_t bytesPerPixel = 4;
    const uint32_t destPitch = width; /* pixels per row, unpadded */

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint64_t tiledByteOffset = getTiledElementByteOffset(p, x, y);
            uint64_t linearByteOffset = ((uint64_t)y * destPitch + x) * bytesPerPixel;
            memcpy(outLinearPixels + linearByteOffset,
                   tiledPixels + tiledByteOffset,
                   bytesPerPixel);
        }
    }
}

// Detile a single pixel -- useful for the empirical-verification step (via the ground-truth capture...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:297)
uint32_t detilePixel_A2R10G10B10(const TileParams *p, const uint8_t *tiledPixels,
                                  uint32_t x, uint32_t y)
{
    uint64_t off = getTiledElementByteOffset(p, x, y);
    uint32_t px;
    memcpy(&px, tiledPixels + off, 4);
    return px;
}

// Unpack A2R10G10B10 (MSB-first, blue at LSB) into 8-bit RGB for the WLED DDP path. sRGB gamma is left...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:312)
void unpackA2R10G10B10_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint32_t r10 = (px >> 20) & 0x3FF;
    uint32_t g10 = (px >> 10) & 0x3FF;
    uint32_t b10 = (px >> 0)  & 0x3FF;
    *r = (uint8_t)(r10 >> 2); /* 10-bit -> 8-bit, simple truncation */
    *g = (uint8_t)(g10 >> 2);
    *b = (uint8_t)(b10 >> 2);
}

// Unpack A8R8G8B8 (MSB-first: A,R,G,B; little-endian uint32 = 0xAARRGGBB) into 8-bit RGB.
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:326)
void unpackA8R8G8B8_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((px >> 16) & 0xFF);
    *g = (uint8_t)((px >> 8)  & 0xFF);
    *b = (uint8_t)(px & 0xFF);
}

// Runtime format dispatch -- takes the raw format value straight off a live sceVideoOutRegisterBuffers...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:340)
typedef void (*PixelUnpackFn)(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b);

PixelUnpackFn getUnpackFnForFormat(uint32_t format)
{
    switch (format) {
    case 0x88000000: /* A2R10G10B10_SRGB */
    case 0x88060000: /* A2R10G10B10 */
    case 0x88740000: /* A2R10G10B10_BT2020_PQ */
        return unpackA2R10G10B10_to_rgb888;
    case 0x80000000: /* A8R8G8B8_SRGB -- confirmed live format */
        return unpackA8R8G8B8_to_rgb888;
    default:
        return NULL; /* unknown format -- caller must NOT guess */
    }
}

#ifdef PS4_DETILE_TEST_MAIN
// Quick sanity check: print the tiled byte offset for a handful of pixel coordinates under both...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:365)
int main(void)
{
    uint32_t testCoords[][2] = { {0,0}, {1,0}, {0,1}, {7,7}, {8,0}, {0,8},
                                  {127,0}, {128,0}, {1919,1079} };
    for (int mode = 0; mode < 2; mode++) {
        const TileParams *p = mode == 0 ? &kParamsBase : &kParamsNeo;
        printf("=== %s ===\n", p->name);
        for (size_t i = 0; i < sizeof(testCoords)/sizeof(testCoords[0]); i++) {
            uint32_t x = testCoords[i][0], y = testCoords[i][1];
            uint64_t off = getTiledElementByteOffset(p, x, y);
            printf("  (%4u,%4u) -> tiled byte offset 0x%llx\n", x, y, (unsigned long long)off);
        }
    }
    return 0;
}
#endif
