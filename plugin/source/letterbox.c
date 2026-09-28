// letterbox.c -- part of ps4_ambient_light. Auto letterbox/pillarbox
// (black bar) detection, ported from the Android "inspiration"
// project's own common/util/BorderProcessor.kt.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
//
// What this ports from BorderProcessor.kt, and why it still applies
// here even though that code works on a decoded RGB byte array and
// this one reads live tiled GPU memory:
//  - Each edge is detected INDEPENDENTLY (not "is there a bar", but
//    "how deep is top's bar / right's bar / ..." separately), so an
//    asymmetric bar -- e.g. a status bar rendered only along the top --
//    is handled correctly instead of being averaged away or ignored.
//  - Each edge is probed at 3 points along the perpendicular axis
//    (25%/50%/75%), not just the center, so a bar with a small non-black
//    HUD element sitting off-center is still detected as a bar rather
//    than mistaken for real picture content at the one point that
//    happens to be dark.
//  - A newly detected border is only committed after
//    autoLetterboxStabilityFrames consecutive re-detections agree,
//    exactly like BorderProcessor's own stabilityDetections gate -- a
//    one-frame flicker (a bright flash, a single corrupted read) can't
//    cause a visible snap in the LED geometry. The very first detection
//    of a session is NOT applied immediately (unlike this plugin's own
//    live config reload, which applies right away) -- it goes through
//    the same stability gate as every later one, matching
//    BorderProcessor's checkNewBorder exactly: startup shouldn't guess
//    from a single frame that could be a loading screen.
//
// What's different from the Android version, and why:
//  - BorderProcessor re-detects on every frame it's handed (it's called
//    from the video encoder's own per-frame callback). This plugin's
//    sample thread runs on its own fixed ~30Hz cadence independent of
//    the game's actual frame delivery, so re-detecting on literally
//    every iteration would mean a full bounded-depth probe pass 30x/sec
//    forever, just to keep re-confirming a border that isn't changing.
//    autoLetterboxCheckIntervalFrames throttles this the same way
//    detectHdr2200Format in zones.c throttles ITS own periodic probe --
//    same idiom, new knob, because this is a different signal.
//  - v3.1: this now probes each candidate row/column with a spread of
//    sample points along the ENTIRE line (isRowBlack/isColBlack below),
//    requiring EDGE_BLACK_PERCENT of them to be black before the scan
//    continues past that line -- this is BorderProcessor.findBorderRgb's
//    majority-vote design (used by every real Android capture path),
//    not findBorderRgba's 3-fixed-point spot check (which, on the
//    Android side, is only ever exercised by unit tests, not live
//    capture -- this plugin had accidentally ported the untested path).
//    The 3-point version stops at the first of its 3 fixed probes
//    (25%/50%/75% of the line) that isn't black, so a dark loading
//    screen with small centered text reads as a giant letterbox: the
//    only non-black pixels are near the middle, so all four edges scan
//    almost all the way to the search limit before "finding" the text
//    and committing a border that crops in around it. Once committed,
//    buildZoneGeometry() moves the edge LED zones in to sit right at
//    that margin (see EDGE_LEFT's x = mLeft, etc. in zones.c) --
//    they're no longer anywhere near the real screen edges. So when
//    real content (e.g. a flash of light) enters from a true edge, nothing
//    reacts until it has physically travelled all the way in to wherever
//    the zones actually got moved to -- a very visible, very real delay,
//    despite the plugin never having gone through a fixed processing lag.
//    Requiring most of a line to be black before treating it as bar
//    means a few dozen pixels of centered text only flip a handful of
//    the sampled points and the scan correctly walks past it to the true
//    edge instead.
//  - v3.1 also bounds the search depth itself (MAX_BAR_DEPTH_V/H) instead
//    of scanning to a full half-screen. On this console, gameplay is
//    always native 16:9 -- the only place a real bar can legitimately
//    appear is a cutscene mastered at a narrower theatrical ratio, and
//    even 2.39:1 scope only asks for ~138px of bar per edge on a 1080p
//    frame (see the constants below for the math). Capping the search
//    well short of height/2 or width/2 does two things: it bounds the
//    worst-case scan cost (see the tiled-offset cost note on
//    probeNonBlack below -- this scan is not cheap per-sample), and it's
//    a second, independent safety net against ever mistaking on-screen
//    content for a border -- even if the majority-vote check somehow
//    still misreads a line, the margin this can commit is capped far
//    short of eating the picture, unlike the old height/2-wide net.
//  - As of v3.0, this behaves exactly like the Android original here:
//    the detected depth IS the margin (see zones.c's effective-margin
//    helpers), full stop. It briefly ADDED to a separate manual
//    capture_margin_{top,right,bottom,left} config in v2.9, when this
//    was introduced alongside those pre-existing fields -- but once
//    this could fully cover what manual margins were for (a fixed
//    number is either wrong during a letterboxed cutscene or wrong for
//    the fullscreen gameplay around it; this isn't), the manual fields
//    were removed rather than carried as unused legacy config. See
//    CHANGELOG for that removal.
//  - No RGBA/stride variant: this plugin only ever has one live pixel
//    access path (getTiledElementByteOffset + a PixelUnpackFn), so
//    there's only one detection function, not BorderProcessor's
//    separate parseBorderRgb/parseBorder split for two different input
//    shapes.

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "plugin_common.h"
#include "ambient_internal.h"

// The margin actually applied on each edge -- see zones.c's
// effMargin{Top,Right,Bottom,Left} helpers, the only readers of these.
// All zero (i.e. sample right up to the true screen edge) until a
// border is actually committed, or whenever the feature is off.
uint32_t g_autoLetterboxTop = 0, g_autoLetterboxRight = 0, g_autoLetterboxBottom = 0, g_autoLetterboxLeft = 0;

// One edge-depth reading. -1 means "every line down to the search limit
// (MAX_BAR_DEPTH_V/H) still read as bar" -- kept distinct from a real 0
// (no bar at all, first row/col was already non-black) so it can be
// treated as "inconclusive on this edge" rather than "the whole search
// range is a border": a black loading/transition screen shouldn't
// collapse the capture area to a sliver just because nothing looked
// like picture content in whatever this one probe pass caught.
typedef struct {
    int32_t top, right, bottom, left;
} LetterboxRect;

static inline bool rectsEqual(const LetterboxRect *a, const LetterboxRect *b)
{
    return a->top == b->top && a->right == b->right && a->bottom == b->bottom && a->left == b->left;
}

// Matches BorderRect.isKnown: at least one edge has a real (>0) detected
// depth. A rect where every edge is 0 or -1 means "no letterbox found",
// which is a legitimate, common, stable state (most content isn't
// letterboxed) -- not an error.
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

// How many probe points are swept along a candidate line (a full row
// for top/bottom, a full column for left/right), and what fraction of
// them must read black for that line to still count as "bar" and let
// the scan continue past it. 85% matches BorderProcessor's own
// BLACK_PERCENT. 10 samples is well under Android's 32 -- each sample
// here is a getTiledElementByteOffset() call (real address math), not
// a flat array read, so this is a real per-sample cost, not a free
// knob; 10 across a ~1920px-wide or ~1080px-tall line still needs
// several samples to land on a loading-screen glyph before the line
// stops reading as "mostly black", which is the actual failure case
// this fixes.
#define EDGE_NUM_SAMPLES 10
#define EDGE_BLACK_PERCENT 85

// How far in from each edge to search before giving up (replaces the
// previous SCREEN_HEIGHT/2 and SCREEN_WIDTH/2 limits). Bounded by the
// deepest bar a real theatrical aspect ratio would ever put on a
// 1920x1080 16:9 frame, with headroom:
//   1.85:1 ("flat")  -> content height = 1920/1.85 =~1038px -> ~21px bar per edge
//   2.39:1 ("scope")  -> content height = 1920/2.39 =~ 803px -> ~138px bar per edge
//   4:3 pillarbox     -> content width  = 1080*4/3  =1440px -> ~240px bar per edge
// SCREEN_HEIGHT/6 (180px) clears 2.39:1's ~138px with ~30% headroom;
// SCREEN_WIDTH/6 (320px) clears a 4:3 pillarbox's ~240px with ~33%
// headroom. Gameplay itself is always full 16:9 on this console --
// there's no legitimate case for a deeper bar than a cutscene would
// use, so searching any further was only ever paying for the
// dark-loading-screen worst case, never finding a real bar there.
#define MAX_BAR_DEPTH_V (SCREEN_HEIGHT / 6)
#define MAX_BAR_DEPTH_H (SCREEN_WIDTH / 6)

// Reads one live pixel and reports whether it's NOT part of a black
// bar. Same bounds check sampleZoneAverage/detectHdr2200Format already
// use for the exact same reason -- a correct offset formula should
// never go out of range for an in-bounds (x,y), but this project has
// already paid once for a bad-memory-access crash from an unguarded
// tiled offset. Out-of-range reads as "black" (the safe default for
// this probe: it just means this one sample doesn't count as evidence
// of real picture content, not that anything is wrong).
static bool probeNonBlack(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                           uint32_t x, uint32_t y, uint32_t threshold)
{
    uint64_t off = getTiledElementByteOffset(p, x, y);
    if (off + 4 > BASE_PADDED_BUFFER_BYTES) return false;
    uint32_t px;
    memcpy(&px, (const void*)(bufferAddr + off), 4);
    uint8_t r, g, b;
    unpack(px, &r, &g, &b);
    return !isBlackPixel(r, g, b, threshold);
}

// Rounds a detected depth down to a band of frameSize/32 pixels, same
// as BorderProcessor's own quantize(). Without this, a real capture
// with no letterbox still jitters by a pixel or two between checks
// (sensor/scaler noise right at the true edge), which would make
// rectsEqual() fail forever and the stability gate never reach
// autoLetterboxStabilityFrames. -1 (inconclusive/unknown) and exactly 0
// pass through unchanged -- only a real positive depth gets banded.
static int32_t quantize(int32_t value, uint32_t frameSize)
{
    if (value <= 0) return value;
    uint32_t band = frameSize / 32;
    if (band < 1) band = 1;
    return (value / (int32_t)band) * (int32_t)band;
}

// Sweeps EDGE_NUM_SAMPLES points across the full row y (x = 0..SCREEN_WIDTH-1)
// and reports whether the row still counts as "bar" -- i.e. whether the
// top/bottom scan should keep going past it. Matches
// BorderProcessor.isRowBlack's majority-vote design: a narrow bright
// object (a few dozen pixels of centered loading-screen text in an
// otherwise-black frame) only flips a handful of these samples, so the
// row still reads >=EDGE_BLACK_PERCENT% black and the scan correctly
// walks past it toward the true edge -- unlike a single fixed probe
// point, which anything non-black sitting at exactly that x could fool
// into stopping right there.
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

// One full per-edge probe pass. Scans inward from each edge, up to the
// bounded MAX_BAR_DEPTH_V/H search limit, for the first row/column that
// isRowBlack/isColBlack no longer calls "bar" -- that's this edge's
// border depth. -1 means every line down to the search limit still read
// as bar -- kept distinct from a real 0 so it's "inconclusive on this
// edge", not "the whole search range is a border" (see this file's own
// top comment and rectIsKnown below).
static LetterboxRect detectRawBorder(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                                      uint32_t threshold)
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

    LetterboxRect r;
    r.top = quantize(top, SCREEN_HEIGHT);
    r.right = quantize(right, SCREEN_WIDTH);
    r.bottom = quantize(bottom, SCREEN_HEIGHT);
    r.left = quantize(left, SCREEN_WIDTH);
    return r;
}

// Drops any auto-detected margin and resets all detection state. Called
// when the feature is off (so it's a guaranteed no-op) and any time it
// transitions from on to off, so a later re-enable starts clean instead
// of resuming from a stale candidate/committed border from before.
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
        // Only rebuild geometry (and break the smoothing continuity
        // below) if this reset actually changes anything a running
        // session already applied -- a no-op disable (never detected
        // anything, or already reset) shouldn't force useless work
        // every time this function is called while the feature stays off.
        buildZoneGeometry();
        g_smoothedRgbValid = false;
    }
}

// Called once per sample_thread.c loop pass, with the exact same
// (TileParams, bufferAddr, unpack) that pass is already using to sample
// zones -- see that file's call site right before the zone-sampling
// loop, so a border committed this pass is honored by THIS pass's own
// g_zoneX/g_zoneY (buildZoneGeometry runs synchronously here, on commit,
// before returning).
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

    LetterboxRect raw = detectRawBorder(p, bufferAddr, unpack, g_config.autoLetterboxThreshold);

    if (!g_haveCandidate) {
        // First read of this enable just establishes a baseline -- see
        // this file's top comment on why this does NOT apply
        // immediately, unlike live config reload elsewhere in this
        // plugin.
        g_candidate = raw;
        g_consistentCount = 1;
        g_haveCandidate = true;
        return;
    }

    if (!rectsEqual(&raw, &g_candidate)) {
        // Detection moved -- restart the stability count against this
        // new reading rather than the old one, exactly like
        // checkNewBorder's else branch.
        g_candidate = raw;
        g_consistentCount = 1;
        return;
    }

    g_consistentCount++;
    uint32_t needed = g_config.autoLetterboxStabilityFrames < 1 ? 1 : g_config.autoLetterboxStabilityFrames;
    if (g_consistentCount < needed) return;

    // Stable result. "No bars" (isKnown == false) is just as valid and
    // stable a result as a real border -- e.g. the letterbox that was
    // there a minute ago just ended -- and needs to actually clear the
    // previously committed margin, not leave it stuck.
    bool known = rectIsKnown(&raw);
    LetterboxRect desired = known ? raw : kZeroRect;
    if (known == g_committedKnown && rectsEqual(&desired, &g_committed)) return; // already applied -- nothing changed

    g_committed = desired;
    g_committedKnown = known;
    g_autoLetterboxTop = (uint32_t)(desired.top > 0 ? desired.top : 0);
    g_autoLetterboxRight = (uint32_t)(desired.right > 0 ? desired.right : 0);
    g_autoLetterboxBottom = (uint32_t)(desired.bottom > 0 ? desired.bottom : 0);
    g_autoLetterboxLeft = (uint32_t)(desired.left > 0 ? desired.left : 0);
    buildZoneGeometry();
    g_smoothedRgbValid = false; // avoid smoothing across a hard cut in zone geometry, same as a live layout reload in settings.c
}
