// ps4_ambient_light: the real per-frame pipeline (handoff §1 / §21 step
// 4). Everything here is proven infrastructure lifted directly from
// detile_verify_probe.prx (v2.2) -- the hooks, the buffer/format
// tracking, and the tiling math are unchanged from what was verified
// against a real screenshot this session. This file's only new job is
// to run that pipeline every frame instead of once on a button press,
// and turn the result into a live WLED signal instead of a debug dump.
//
// SCOPE OF THIS FIRST VERSION:
//   - Samples a small, fixed set of screen zones per frame (not a full
//     frame detile -- see the perf note by kZone* below) using ONLY the
//     confirmed-correct base tiling params. The Neo path is dropped
//     here; it served its purpose settling that question in the probe
//     and has no reason to run in the hot path.
//   - Pixel format is read live from sceVideoOutRegisterBuffers, not
//     assumed at compile time -- this is the actual bug this session
//     found (A2R10G10B10 math applied to A8R8G8B8 data). If an unknown
//     format shows up, this plugin explicitly stops sending color
//     rather than guessing.
//   - Network sends are throttled independently of frame rate -- see
//     kMinSendIntervalUs -- so this doesn't put a UDP send in the
//     critical path of every single flip at 60Hz.
//   - Zone-to-physical-LED-segment mapping (Alcove/Cabinet/Bed/Flower)
//     is intentionally NOT done here -- that's a WLED/HA-side concern
//     (segment config / ledmap), not something this plugin should be
//     guessing at. This sends kNumZones consecutive RGB triplets
//     starting at DDP offset 0; wire that up on the WLED side.
//
// STILL OPEN (do not treat this as fully validated -- see handoff):
//   - §20 finding 2 (point 1 reading static values in live play) was
//     never confirmed fixed against *moving* content, only a static
//     menu. Watch the zone colors during actual gameplay before
//     trusting this for real use.
//   - No live-performance measurement has been done yet of what N
//     zone reads + a UDP send actually costs inside this hook on real
//     hardware. Start conservative (kNumZones small, throttled send)
//     and measure before increasing either.

#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#include <orbis/libkernel.h>
#include <orbis/_types/video.h>

#include "plugin_common.h"

// --- WLED DDP transport (identical pattern used throughout this project) ---
#define WLED_IP         "192.168.2.110"   // the real WLED light -- NOT the dev-PC debug listener IP used by detile_verify_probe
#define WLED_PORT       4048
#define DDP_HEADER_SIZE 10

static int g_wledSockfd = -1; // one socket, held open -- avoid a socket()/close() per send at frame rate

static int wled_ensure_socket(void)
{
    if (g_wledSockfd >= 0) return g_wledSockfd;
    g_wledSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    return g_wledSockfd;
}

#define MAX_ZONES 229 // real TV backlight strip: 73 top + 42 left + 73 bottom + 41 right

static void wled_send_rgb_zones(const uint8_t *rgbTriplets, int numZones)
{
    int sockfd = wled_ensure_socket();
    if (sockfd < 0) return;

    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(WLED_PORT);
    if (inet_pton(AF_INET, WLED_IP, &destAddr.sin_addr) != 1) return;

    if (numZones > MAX_ZONES) numZones = MAX_ZONES;
    int dataSize = numZones * 3;
    uint8_t packet[DDP_HEADER_SIZE + 3 * MAX_ZONES];
    packet[0] = 0x40 | 0x01;
    packet[1] = 0;
    packet[2] = 0x0B;
    packet[3] = 0x01;
    packet[4] = 0; packet[5] = 0; packet[6] = 0; packet[7] = 0; // pixel offset 0
    packet[8] = (uint8_t)((dataSize >> 8) & 0xFF);
    packet[9] = (uint8_t)(dataSize & 0xFF);
    memcpy(packet + DDP_HEADER_SIZE, rgbTriplets, dataSize);

    sendto(sockfd, packet, DDP_HEADER_SIZE + dataSize, 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
}

// ============================================================
// Detile math -- confirmed-correct BASE params only (§21/§22).
// Identical formulas to ps4_detile_2dthin.c / detile_verify_probe,
// just without the Neo branch, which has no reason to run every frame
// now that base is the settled answer for this hardware.
// ============================================================

typedef struct {
    uint32_t pipeConfig, numPipes, bankWidth, bankHeight, numBanks;
    uint32_t macroTileWidth, macroTileHeight, paddedWidth, tileSplitBytes;
    uint32_t pipeInterleaveBits, pipeBits, bankBits;
} TileParams;

static const TileParams kParamsBase = {
    0xc, 8, 1, 1, 16, 128, 64, 1920, 512, 8, 3, 4
};

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

static uint64_t getTiledElementByteOffset(const TileParams *p, uint32_t x, uint32_t y)
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

// ============================================================
// Runtime pixel-format dispatch -- same principle as
// ps4_detile_2dthin.c's getUnpackFnForFormat(): read live off the
// registration hook, never assumed at compile time. Unknown format ->
// NULL -> this plugin explicitly stops sending color instead of
// guessing (see the flip hook below).
// ============================================================

typedef void (*PixelUnpackFn)(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b);

static void unpackA2R10G10B10_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint32_t r10 = (px >> 20) & 0x3FF;
    uint32_t g10 = (px >> 10) & 0x3FF;
    uint32_t b10 = (px >> 0)  & 0x3FF;
    *r = (uint8_t)(r10 >> 2);
    *g = (uint8_t)(g10 >> 2);
    *b = (uint8_t)(b10 >> 2);
}

static void unpackA8R8G8B8_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((px >> 16) & 0xFF);
    *g = (uint8_t)((px >> 8)  & 0xFF);
    *b = (uint8_t)(px & 0xFF);
}

static PixelUnpackFn getUnpackFnForFormat(uint32_t format)
{
    switch (format) {
    case 0x88000000: case 0x88060000: case 0x88740000:
        return unpackA2R10G10B10_to_rgb888;
    case 0x80000000: // A8R8G8B8_SRGB -- confirmed live format this session, §21/§22
        return unpackA8R8G8B8_to_rgb888;
    default:
        return NULL;
    }
}

// ============================================================
// Zones: a handful of representative screen-edge points, each read as
// a small averaged neighborhood (not a single pixel) for stability --
// matches how ground truth was sampled against the screenshot this
// session, and avoids one noisy pixel flickering a whole zone. This is
// NOT a full-frame detile -- see the header note on cost.
//
// Placeholder positions only: top/bottom/left/right edge midpoints +
// center. Replace with real geometry once physical segment mapping is
// decided on the WLED/HA side.
// ============================================================

#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

// Real TV backlight layout (as measured on the physical strip, not a
// placeholder): 73 LEDs top, 42 left, 73 bottom, 41 right. Index 0 is
// the bottom-left corner LED; the strip runs CLOCKWISE from there --
// which means UP the left edge first, not along the bottom. Walking a
// rectangle's border clockwise from its bottom-left corner always goes
// toward the left edge next (think of the corner sitting at the 7-8
// o'clock position on a clock face -- clockwise from there heads to 9
// o'clock, i.e. up the left side), so the segment order is:
//   LEFT (bottom->top) -> TOP (left->right) -> RIGHT (top->bottom)
//   -> BOTTOM (right->left, closing the loop back at the start)
#define LEFT_COUNT   42
#define TOP_COUNT    73
#define RIGHT_COUNT  41
#define BOTTOM_COUNT 73
#define NUM_ZONES    (LEFT_COUNT + TOP_COUNT + RIGHT_COUNT + BOTTOM_COUNT) // 229

#define ZONE_SAMPLE_RADIUS 1 // (2r+1)^2 = 9 samples per zone -- kept small since
                              // NUM_ZONES=229 already averages spatially across
                              // the edge; see perf note on sceGnmSubmitAndFlip...

static uint32_t g_zoneX[NUM_ZONES];
static uint32_t g_zoneY[NUM_ZONES];

// Fills g_zoneX/g_zoneY once at load time with real screen-edge
// coordinates for each of the 229 physical LED positions, using only
// integer math (no libm linked in this build, see Makefile LIBS).
static void buildZoneGeometry(void)
{
    uint32_t idx = 0;

    // LEFT edge, bottom -> top, x pinned to the left screen edge.
    for (uint32_t i = 0; i < LEFT_COUNT; i++) {
        g_zoneX[idx] = 0;
        g_zoneY[idx] = (SCREEN_HEIGHT - 1) - (i * (SCREEN_HEIGHT - 1)) / (LEFT_COUNT - 1);
        idx++;
    }
    // TOP edge, left -> right, y pinned to the top screen edge.
    for (uint32_t i = 0; i < TOP_COUNT; i++) {
        g_zoneX[idx] = (i * (SCREEN_WIDTH - 1)) / (TOP_COUNT - 1);
        g_zoneY[idx] = 0;
        idx++;
    }
    // RIGHT edge, top -> bottom, x pinned to the right screen edge.
    for (uint32_t i = 0; i < RIGHT_COUNT; i++) {
        g_zoneX[idx] = SCREEN_WIDTH - 1;
        g_zoneY[idx] = (i * (SCREEN_HEIGHT - 1)) / (RIGHT_COUNT - 1);
        idx++;
    }
    // BOTTOM edge, right -> left, y pinned to the bottom screen edge --
    // closes the loop back toward index 0 (bottom-left).
    for (uint32_t i = 0; i < BOTTOM_COUNT; i++) {
        g_zoneX[idx] = (SCREEN_WIDTH - 1) - (i * (SCREEN_WIDTH - 1)) / (BOTTOM_COUNT - 1);
        g_zoneY[idx] = SCREEN_HEIGHT - 1;
        idx++;
    }
    // idx should now equal NUM_ZONES (229) exactly -- LEFT_COUNT +
    // TOP_COUNT + RIGHT_COUNT + BOTTOM_COUNT by construction.
}

static void sampleZoneAverage(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                               uint32_t cx, uint32_t cy, uint8_t *outR, uint8_t *outG, uint8_t *outB)
{
    uint32_t sumR = 0, sumG = 0, sumB = 0, n = 0;
    for (int32_t dy = -ZONE_SAMPLE_RADIUS; dy <= ZONE_SAMPLE_RADIUS; dy++) {
        for (int32_t dx = -ZONE_SAMPLE_RADIUS; dx <= ZONE_SAMPLE_RADIUS; dx++) {
            int32_t sx = (int32_t)cx + dx;
            int32_t sy = (int32_t)cy + dy;
            if (sx < 0 || sy < 0 || sx >= SCREEN_WIDTH || sy >= SCREEN_HEIGHT) continue;
            uint64_t off = getTiledElementByteOffset(p, (uint32_t)sx, (uint32_t)sy);
            uint32_t px;
            memcpy(&px, (const void*)(bufferAddr + off), 4);
            uint8_t r, g, b;
            unpack(px, &r, &g, &b);
            sumR += r; sumG += g; sumB += b; n++;
        }
    }
    if (n == 0) n = 1;
    *outR = (uint8_t)(sumR / n);
    *outG = (uint8_t)(sumG / n);
    *outB = (uint8_t)(sumB / n);
}

// --- Plugin metadata ---
attr_public const char *g_pluginName = "ps4_ambient_light";
attr_public const char *g_pluginDesc = "Live per-frame ambient light: detiles the real scanout buffer and streams zone colors to WLED";
attr_public const char *g_pluginAuth = "(null)";
attr_public uint32_t g_pluginVersion = 0x00000100; // v1.0: first real per-frame pipeline, built on detile_verify_probe v2.2's confirmed math/format-tracking

int32_t (*sceVideoOutRegisterBuffersPtr)(int32_t handle, int32_t startIndex,
                                          void *const *addresses, int32_t bufferNum,
                                          const OrbisVideoOutBufferAttribute *attribute);
int32_t (*sceGnmSubmitAndFlipCommandBuffersPtr)(uint32_t count, void *dcbGpuAddrs[],
                                                 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                 uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                 uint32_t displayBufferIndex, uint32_t flipMode,
                                                 int64_t flipArg);

HOOK_INIT(sceVideoOutRegisterBuffersPtr);
HOOK_INIT(sceGnmSubmitAndFlipCommandBuffersPtr);

#define MAX_TRACKED_BUFFERS 16
static volatile uint64_t g_bufferAddrs[MAX_TRACKED_BUFFERS] = {0};
static volatile int32_t  g_bufferCount = 0;
static volatile uint32_t g_activeFormat = 0;       // live format, NOT hardcoded -- the actual fix from this session
static volatile int      g_haveValidFormat = 0;    // 0 until a registration event gives us a known format

int32_t sceVideoOutRegisterBuffersPtr_hook(int32_t handle, int32_t startIndex,
                                            void *const *addresses, int32_t bufferNum,
                                            const OrbisVideoOutBufferAttribute *attribute)
{
    if (addresses != NULL && bufferNum > 0) {
        int32_t n = bufferNum > MAX_TRACKED_BUFFERS ? MAX_TRACKED_BUFFERS : bufferNum;
        for (int32_t i = 0; i < n; i++) {
            int32_t slot = startIndex + i;
            if (slot >= 0 && slot < MAX_TRACKED_BUFFERS) {
                g_bufferAddrs[slot] = (uint64_t)addresses[i];
            }
        }
        if (n > g_bufferCount) g_bufferCount = n;

        if (attribute != NULL) {
            uint32_t fmt = (uint32_t)attribute->format;
            g_activeFormat = fmt;
            g_haveValidFormat = (getUnpackFnForFormat(fmt) != NULL);
            // No wled_signal() color-flash here on purpose -- this hook
            // can now fire repeatedly during normal play (title
            // re-registering on scene changes etc.), and flashing the
            // strip every time would itself be a visible glitch. Add
            // logging here instead if you need to confirm this is
            // firing during a real session.
        }
    }
    return HOOK_CONTINUE(sceVideoOutRegisterBuffersPtr,
                          int32_t(*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *),
                          handle, startIndex, addresses, bufferNum, attribute);
}

// Throttle independent of frame rate -- do NOT send a UDP packet on
// every single flip at 60Hz. Not yet measured against real hardware
// (see header note) -- start conservative.
#define MIN_SEND_INTERVAL_US (33 * 1000) // ~30Hz cap
static uint64_t g_lastSendTimeUs = 0;

static uint64_t nowMicros(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

int32_t sceGnmSubmitAndFlipCommandBuffersPtr_hook(uint32_t count, void *dcbGpuAddrs[],
                                                   uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                   uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                   uint32_t displayBufferIndex, uint32_t flipMode,
                                                   int64_t flipArg)
{
    uint64_t liveBufferAddr = (displayBufferIndex < (uint32_t)MAX_TRACKED_BUFFERS)
                                   ? g_bufferAddrs[displayBufferIndex] : 0;

    if (liveBufferAddr != 0 && g_haveValidFormat) {
        uint64_t nowUs = nowMicros();
        if (nowUs - g_lastSendTimeUs >= MIN_SEND_INTERVAL_US) {
            PixelUnpackFn unpack = getUnpackFnForFormat(g_activeFormat);
            if (unpack != NULL) { // re-check -- format could have gone unknown between hooks
                uint8_t rgbTriplets[NUM_ZONES * 3];
                for (int i = 0; i < NUM_ZONES; i++) {
                    sampleZoneAverage(&kParamsBase, liveBufferAddr, unpack,
                                       g_zoneX[i], g_zoneY[i],
                                       &rgbTriplets[i*3+0], &rgbTriplets[i*3+1], &rgbTriplets[i*3+2]);
                }
                wled_send_rgb_zones(rgbTriplets, NUM_ZONES);
                g_lastSendTimeUs = nowUs;
            }
        }
    }
    // If g_haveValidFormat is 0 (unknown/unconfirmed format), we
    // deliberately send nothing rather than guess -- this is the
    // direct fix for how this session's bug happened in the first
    // place: an unverified format assumption silently producing wrong
    // color instead of visibly doing nothing.

    return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffersPtr,
                          int32_t(*)(uint32_t, void **, uint32_t *, void **, uint32_t *, uint32_t, uint32_t, uint32_t, int64_t),
                          count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes,
                          videoOutHandle, displayBufferIndex, flipMode, flipArg);
}

int32_t attr_public plugin_load(int32_t argc, const char* argv[])
{
    buildZoneGeometry(); // fill g_zoneX/g_zoneY for all 229 real LED positions once

    int32_t hVideoOut = 0, hGnm = 0;
    sys_dynlib_load_prx("libSceVideoOut.sprx", &hVideoOut);
    sys_dynlib_load_prx("libSceGnmDriver.sprx", &hGnm);
    if (hVideoOut == 0 || hGnm == 0) return 0;

    sys_dynlib_dlsym(hVideoOut, "sceVideoOutRegisterBuffers", (void**)&sceVideoOutRegisterBuffersPtr);
    sys_dynlib_dlsym(hGnm, "sceGnmSubmitAndFlipCommandBuffers", (void**)&sceGnmSubmitAndFlipCommandBuffersPtr);
    if (sceVideoOutRegisterBuffersPtr == NULL || sceGnmSubmitAndFlipCommandBuffersPtr == NULL) return 0;

    // Same forwarding-stub guard as detile_verify_probe -- proven
    // necessary, do not skip.
    if (*((uint8_t*)sceVideoOutRegisterBuffersPtr) == 0xE9 ||
        *((uint8_t*)sceGnmSubmitAndFlipCommandBuffersPtr) == 0xE9) {
        return 0;
    }

    HOOK32(sceVideoOutRegisterBuffersPtr);
    HOOK32(sceGnmSubmitAndFlipCommandBuffersPtr);
    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char* argv[])
{
    UNHOOK(sceVideoOutRegisterBuffersPtr);
    UNHOOK(sceGnmSubmitAndFlipCommandBuffersPtr);
    if (g_wledSockfd >= 0) { close(g_wledSockfd); g_wledSockfd = -1; }
    return 0;
}

s32 attr_module_hidden module_start(s64 argc, const void *args) { return 0; }
s32 attr_module_hidden module_stop(s64 argc, const void *args) { return 0; }
