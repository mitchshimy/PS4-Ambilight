// letterbox.c -- part of ps4_ambient_light.
// More: docs/code-comments.md (plugin/source/letterbox.c:1)

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "plugin_common.h"
#include "ambient_internal.h"

// The margin actually applied on each edge -- see zones.c's effMargin{Top,Right,Bottom,Left} helpers...
// More: docs/code-comments.md (plugin/source/letterbox.c:101)
uint32_t g_autoLetterboxTop = 0, g_autoLetterboxRight = 0, g_autoLetterboxBottom = 0, g_autoLetterboxLeft = 0;

// One edge-depth reading. -1 means "every line down to the search limit (MAX_BAR_DEPTH_V/H) still read...
// More: docs/code-comments.md (plugin/source/letterbox.c:107)
typedef struct {
    int32_t top, right, bottom, left;
} LetterboxRect;

static inline bool rectsEqual(const LetterboxRect *a, const LetterboxRect *b)
{
    return a->top == b->top && a->right == b->right && a->bottom == b->bottom && a->left == b->left;
}

// Matches BorderRect.isKnown: at least one edge has a real (>0) detected depth.
// More: docs/code-comments.md (plugin/source/letterbox.c:123)
static inline bool rectIsKnown(const LetterboxRect *r)
{
    return r->top > 0 || r->right > 0 || r->bottom > 0 || r->left > 0;
}

static const LetterboxRect kZeroRect = { 0, 0, 0, 0 };

// Stability-gate state (checkNewBorder's mPreviousBorder/mConsistentDetections
// in the Kotlin original) and the last actually-committed border.
static bool g_haveCandidate = false;
static LetterboxRect g_candidate = { 0, 0, 0, 0 };
static uint32_t g_consistentCount = 0;
static LetterboxRect g_committed = { 0, 0, 0, 0 };
static bool g_committedKnown = false;
static uint32_t g_frameCountdown = 0;

static inline bool isBlackPixel(uint8_t r, uint8_t g, uint8_t b, uint32_t threshold)
{
    return r < threshold && g < threshold && b < threshold;
}

// How many probe points are swept along a candidate line (a full row for top/bottom, a full column for...
// More: docs/code-comments.md (plugin/source/letterbox.c:148)
#define EDGE_NUM_SAMPLES 10
#define EDGE_BLACK_PERCENT 85

// How far in from each edge to search before giving up (replaces the previous SCREEN_HEIGHT/2 and...
// More: docs/code-comments.md (plugin/source/letterbox.c:162)
#define MAX_BAR_DEPTH_V (SCREEN_HEIGHT / 6)
#define MAX_BAR_DEPTH_H (SCREEN_WIDTH / 6)

// Reads one live pixel and reports whether it's NOT part of a black bar.
// More: docs/code-comments.md (plugin/source/letterbox.c:178)
static bool probeNonBlack(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                           uint32_t x, uint32_t y, uint32_t threshold)
{
    uint64_t off = getTiledElementByteOffset(p, x, y);
    if (off + 4 > g_readableLimit) return false;
    uint32_t px;
    memcpy(&px, (const void*)(bufferAddr + off), 4);
    uint8_t r, g, b;
    unpack(px, &r, &g, &b);
    return !isBlackPixel(r, g, b, threshold);
}

// Rounds a detected depth down to a band of frameSize/32 pixels, same as BorderProcessor's own quantize().
// More: docs/code-comments.md (plugin/source/letterbox.c:198)
static uint32_t quantizeBand(uint32_t frameSize)
{
    uint32_t band = frameSize / 32;
    return band < 1 ? 1 : band;
}

static int32_t quantize(int32_t value, uint32_t frameSize)
{
    if (value <= 0) return value;
    uint32_t band = quantizeBand(frameSize);
    return (value / (int32_t)band) * (int32_t)band;
}

// Real letterbox/pillarbox bars come from cropping to a wider or taller aspect ratio, so the two bars...
// More: docs/code-comments.md (plugin/source/letterbox.c:218)
static bool axisSymmetric(int32_t d1, int32_t d2, uint32_t frameSize)
{
    if (d1 <= 0 || d2 <= 0) return false; // one-sided: not a real crop bar
    int32_t diff = d1 > d2 ? d1 - d2 : d2 - d1;
    return (uint32_t)diff <= 2u * quantizeBand(frameSize);
}

// Sweeps EDGE_NUM_SAMPLES points across the full row y (x = 0..SCREEN_WIDTH-1) and reports whether the...
// More: docs/code-comments.md (plugin/source/letterbox.c:232)
static bool isRowBlack(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                        uint32_t y, uint32_t threshold)
{
    uint32_t step = SCREEN_WIDTH / EDGE_NUM_SAMPLES;
    if (step < 1) step = 1;
    uint32_t samples = 0, blacks = 0;
    for (uint32_t x = 0; x < SCREEN_WIDTH; x += step) {
        samples++;
        if (!probeNonBlack(p, bufferAddr, unpack, x, y, threshold)) blacks++;
    }
    return samples > 0 && blacks * 100 >= samples * EDGE_BLACK_PERCENT;
}

// Column counterpart of isRowBlack, for the left/right scan.
static bool isColBlack(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                        uint32_t x, uint32_t threshold)
{
    uint32_t step = SCREEN_HEIGHT / EDGE_NUM_SAMPLES;
    if (step < 1) step = 1;
    uint32_t samples = 0, blacks = 0;
    for (uint32_t y = 0; y < SCREEN_HEIGHT; y += step) {
        samples++;
        if (!probeNonBlack(p, bufferAddr, unpack, x, y, threshold)) blacks++;
    }
    return samples > 0 && blacks * 100 >= samples * EDGE_BLACK_PERCENT;
}

// One full per-edge probe pass.
// More: docs/code-comments.md (plugin/source/letterbox.c:269)
static LetterboxRect detectRawBorder(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                                      uint32_t threshold, int32_t outUnq[4])
{
    const uint32_t maxV = MAX_BAR_DEPTH_V;
    const uint32_t maxH = MAX_BAR_DEPTH_H;

    int32_t top = -1;
    for (uint32_t y = 0; y < maxV; y++) {
        if (!isRowBlack(p, bufferAddr, unpack, y, threshold)) { top = (int32_t)y; break; }
    }
    int32_t bottom = -1;
    for (uint32_t i = 0; i < maxV; i++) {
        uint32_t y = SCREEN_HEIGHT - 1 - i;
        if (!isRowBlack(p, bufferAddr, unpack, y, threshold)) { bottom = (int32_t)i; break; }
    }
    int32_t left = -1;
    for (uint32_t x = 0; x < maxH; x++) {
        if (!isColBlack(p, bufferAddr, unpack, x, threshold)) { left = (int32_t)x; break; }
    }
    int32_t right = -1;
    for (uint32_t i = 0; i < maxH; i++) {
        uint32_t x = SCREEN_WIDTH - 1 - i;
        if (!isColBlack(p, bufferAddr, unpack, x, threshold)) { right = (int32_t)i; break; }
    }

    // Unquantized depths (first non-bar row/col index; -1 = search limit hit).
    // More: docs/code-comments.md (plugin/source/letterbox.c:301)
    outUnq[0] = top; outUnq[1] = right; outUnq[2] = bottom; outUnq[3] = left;
    LetterboxRect r;
    r.top = quantize(top, SCREEN_HEIGHT);
    r.right = quantize(right, SCREEN_WIDTH);
    r.bottom = quantize(bottom, SCREEN_HEIGHT);
    r.left = quantize(left, SCREEN_WIDTH);
    return r;
}

// Drops any auto-detected margin and resets all detection state.
// More: docs/code-comments.md (plugin/source/letterbox.c:314)
static void resetLetterboxState(void)
{
    bool hadEffect = g_autoLetterboxTop || g_autoLetterboxRight || g_autoLetterboxBottom || g_autoLetterboxLeft;
    g_autoLetterboxTop = g_autoLetterboxRight = g_autoLetterboxBottom = g_autoLetterboxLeft = 0;
    g_haveCandidate = false;
    g_consistentCount = 0;
    g_committed = kZeroRect;
    g_committedKnown = false;
    g_frameCountdown = 0;
    if (hadEffect) {
        // Only rebuild geometry (and break the smoothing continuity below) if this reset actually changes...
        // More: docs/code-comments.md (plugin/source/letterbox.c:328)
        buildZoneGeometry();
        g_smoothedRgbValid = false;
    }
}

// Called once per sample_thread.c loop pass, with the exact same (TileParams, bufferAddr, unpack) that...
// More: docs/code-comments.md (plugin/source/letterbox.c:338)
static uint32_t placementMargin(int32_t quantized, int32_t unq)
{
    if (quantized <= 0 || unq <= 0) return 0;
    return (uint32_t)unq + g_config.scanDepth + 1u;
}

void ambient_check_letterbox(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack)
{
    if (!g_config.autoLetterboxEnabled) {
        resetLetterboxState();
        return;
    }

    if (g_frameCountdown > 0) {
        g_frameCountdown--;
        return;
    }
    g_frameCountdown = g_config.autoLetterboxCheckIntervalFrames;

    int32_t rawUnq[4];
    LetterboxRect raw = detectRawBorder(p, bufferAddr, unpack, g_config.autoLetterboxThreshold, rawUnq);

    if (!g_haveCandidate) {
        // First read of this enable just establishes a baseline -- see this file's top comment on why this...
        // More: docs/code-comments.md (plugin/source/letterbox.c:377)
        g_candidate = raw;
        g_consistentCount = 1;
        g_haveCandidate = true;
        return;
    }

    if (!rectsEqual(&raw, &g_candidate)) {
        // Detection moved -- restart the stability count against this new reading rather than the old one...
        // More: docs/code-comments.md (plugin/source/letterbox.c:388)
        g_candidate = raw;
        g_consistentCount = 1;
        return;
    }

    g_consistentCount++;
    uint32_t needed = g_config.autoLetterboxStabilityFrames < 1 ? 1 : g_config.autoLetterboxStabilityFrames;
    if (g_consistentCount < needed) return;

    // Stable result. "No bars" (isKnown == false) is just as valid and stable a result as a real border...
    // More: docs/code-comments.md (plugin/source/letterbox.c:400)
    bool known = rectIsKnown(&raw);
    LetterboxRect desired = known ? raw : kZeroRect;
    if (known == g_committedKnown && rectsEqual(&desired, &g_committed)) return; // already applied -- nothing changed

    g_committed = desired;
    g_committedKnown = known;
    bool vSym = axisSymmetric(rawUnq[0], rawUnq[2], SCREEN_HEIGHT);   // top vs bottom
    bool hSym = axisSymmetric(rawUnq[3], rawUnq[1], SCREEN_WIDTH);    // left vs right
    g_autoLetterboxTop = vSym ? placementMargin(desired.top, rawUnq[0]) : 0;
    g_autoLetterboxBottom = vSym ? placementMargin(desired.bottom, rawUnq[2]) : 0;
    g_autoLetterboxLeft = hSym ? placementMargin(desired.left, rawUnq[3]) : 0;
    g_autoLetterboxRight = hSym ? placementMargin(desired.right, rawUnq[1]) : 0;
    buildZoneGeometry();
    g_smoothedRgbValid = false; // avoid smoothing across a hard cut in zone geometry, same as a live layout reload in settings.c
}
