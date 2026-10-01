// ambient_internal.h -- shared types, config path, and cross-module
// externs for ps4_ambient_light, split out of what used to be one
// 3240-line main.c (see main.c's own top comment for the file's real
// history/scope notes -- this header only exists to let that same
// code compile as separate translation units; it changes no
// behavior). Every struct/enum/global declared here was previously a
// file-local `static` inside main.c -- promoted to extern linkage
// ONLY where a different .c file genuinely needed it, nothing more.
//
// Not a public API: this is glue between this plugin's OWN source
// files, not something another plugin or the companion app includes.
#ifndef AMBIENT_INTERNAL_H
#define AMBIENT_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <orbis/libkernel.h>
#include <orbis/_types/video.h> // OrbisVideoOutBufferAttribute, used by the hook prototypes below
#include "pq8bit_vote.h"        // v3.5, pure alpha-byte vote so a PC test can build it
#include "hdr2200_vote.h"      // v3.6, same idea for 0x80002200, also PC-buildable

// ------------------------------------------------------------
// [settings] -- AmbientConfig struct, g_config instance lives in
// settings.c. See settings.c for the defaults and the full field-by-
// field history (why each default is what it is).
// ------------------------------------------------------------
#define AMBIENT_CONFIG_PATH "/data/ps4_ambient_light.ini"
#define NUM_GAMMA_LUTS 8

typedef enum { CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT, CORNER_TOP_LEFT, CORNER_TOP_RIGHT } StartCorner;
typedef enum { DIR_CLOCKWISE, DIR_COUNTERCLOCKWISE } LedDirection;
typedef enum { ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR } ColorOrder;

typedef struct {
    // [network]
    char wledHost[64];
    uint16_t wledPort;
    // v2.3: separate destination for the external-source on/off signal
    // (see relay_send_external_source below) -- the relay that owns
    // this (wled-relay's tv_external_source.py) runs on its own host,
    // NOT the WLED controller itself, so this can't just reuse
    // wledHost/wledPort.
    char relayHost[64];
    uint16_t relayPort;
    // v2.3.1: this whole external-source signal is a personal-setup
    // integration with one specific wled-relay project, not something
    // every user of this plugin has (or should have to configure a
    // dead host for) -- OFF by default, only sends anything if a user
    // explicitly opts in via the ini.
    bool relaySignalEnabled;
    // [layout]
    uint32_t ledCountTop, ledCountRight, ledCountBottom, ledCountLeft;
    StartCorner startCorner;
    LedDirection direction;
    int32_t ledOffset;          // rotates which physical LED index 0 lands on
    uint32_t scanDepth;         // sample radius: (2*scanDepth+1)^2 pixels averaged per zone
    // v3.0: auto letterbox/black-bar detection is now the ONLY inset
    // applied before sampling -- the old manual capture_margin_{top,
    // right,bottom,left} fields were removed (see CHANGELOG). Almost
    // everything plays fullscreen with no letterboxing at all, in
    // which case this correctly detects and applies zero margin; the
    // cases it exists for -- cutscenes, aspect-ratio-locked menus, a
    // status bar along one edge -- are exactly the cases a fixed
    // manual number couldn't handle anyway (it's either wrong during
    // the bar, or wrong for the other 99% of gameplay that has none).
    // On by default for that reason: disabling it now means sampling
    // starts at the true pixel 0/edge on every side, unconditionally.
    bool autoLetterboxEnabled;
    uint32_t autoLetterboxThreshold;         // 0-255: a probed pixel counts as "black" if every channel is below this
    uint32_t autoLetterboxStabilityFrames;   // consecutive matching detections required before a new border is committed -- kills flicker across a scene cut
    uint32_t autoLetterboxCheckIntervalFrames; // how many sample-thread iterations to wait between re-detections (each check is a bounded per-edge probe, up to MAX_BAR_DEPTH_V/H deep in letterbox.c, and every sample is a tiled read -- not free)
    // [color]
    uint32_t brightness;        // 0-255 global scale, applied after gamma
    uint32_t gammaLutIndex;     // index into kGammaLuts -- see NUM_GAMMA_LUTS below
    int32_t saturation;         // -100 (grayscale) .. 0 (unchanged) .. SATURATION_MAX (2x boost)
    ColorOrder colorOrder;
    uint32_t blackLevel;        // v2.1: 0-100, percent of 255 below which output clips to 0
    uint32_t whiteLevel;        // v2.1: 0-100, percent of 255 at/above which output clips to 255 (100 = no change)
    uint32_t darkThreshold;     // v2.1: 0-255, max(R,G,B) below this forces a zone fully black (0 = disabled)
    // v2.2: ported from the Android "inspiration" project's own
    // ColorProcessor.kt. These are ADDITIONS on top
    // of the existing brightness/gamma fields above, not replacements
    // -- brightness/gamma keep their original 0-255 / fixed-LUT-string
    // meaning so nobody's existing ini silently changes behavior.
    // Percentages below use Android's own convention (100 = neutral),
    // NOT this file's usual "0 = neutral" convention -- documented
    // per-field in the generated ini template.
    int32_t contrast;           // -100..CONTRAST_MAX, 0 = unchanged (100=neutral Android pct minus 100, same convention as saturation above)
    uint32_t brightnessR, brightnessG, brightnessB; // 0-500, 100 = unchanged. Multiplies with the global brightness above.
    uint32_t gammaR, gammaG, gammaB;                // 10-500, 100 = unchanged. Independent per-channel curves, applied PER SAMPLE
                                                     // before zone-averaging (see sampleZoneAverage) -- unlike every other
                                                     // knob here, which is applied once to the already-averaged zone color.
                                                     // This matters because gamma is non-linear: correcting-then-averaging
                                                     // and averaging-then-correcting are NOT the same operation, and the
                                                     // Android source applies its gamma per-pixel, before any downsampling.
    // v2.2.5: opt-in developer telemetry destination. Deliberately NOT
    // part of ambient_create_default_config()'s generated template
    // (see AMBIENT_DEFAULT_INI below) -- a person has to type a [dev]
    // section into their own ini by hand for this to ever do anything.
    // Empty devIp / false devLoggingEnabled (the zero-init default,
    // same as every other field in this struct before a real ini is
    // read) means debug_send_raw() sends nothing at all, full stop --
    // see its own comment for why this check lives there and not
    // scattered across each call site.
    char devIp[16];          // dotted-quad only, e.g. "192.168.2.117"; empty = disabled regardless of devLoggingEnabled
    bool devLoggingEnabled;  // both this AND a non-empty devIp are required -- neither alone is enough
    // [timing]
    uint32_t updateFrequencyHz;
    int smoothingEnabled;
    uint32_t settlingTimeMs;
    uint32_t configReloadCheckSeconds; // v2.1: 0 = load once at start only (v2.0 behavior), like before
} AmbientConfig;

extern AmbientConfig g_config; // defined in settings.c
extern char g_titleId[16];     // defined in settings.c; set by plugin_load, picks the preset (media_titles.h)

// [gamma] -- kGammaLuts is the fixed 8-preset table (gamma.c); the
// per-channel g_gammaLutR/G/B are the continuous, config-driven ones
// (also gamma.c) -- see that file for why both exist. Used directly
// by color_processing.c (kGammaLuts) and zones.c's sampleZoneAverage
// (g_gammaLutR/G/B, applied per-sample before averaging).
extern const uint8_t kGammaLuts[NUM_GAMMA_LUTS][256];
extern float g_gammaLutR[256], g_gammaLutG[256], g_gammaLutB[256];
void ambient_rebuild_perchannel_gamma_luts(void); // gamma.c -- called by settings.c on every config (re)load

// [settings] -- ini load/save + live-reload detection (settings.c)
void ambient_load_config(void);       // called by main.c (plugin_load)
void ambient_check_config_reload(void); // called by sample_thread.c, once per loop pass

// [network] -- WLED/relay/debug transport (network.c)
extern int g_wledSockfd; // read directly by main.c's plugin_unload to close it
void relay_send_external_source(bool active);   // called by main.c (plugin_load/unload) and settings.c (live toggle)
void send_timing_packet(uint32_t minUs, uint32_t maxUs, uint32_t avgUs,
                         uint32_t sampleCount, uint32_t overBudgetCount,
                         uint32_t windowId, uint32_t minCpu,
                         uint32_t maxCpu, uint32_t migrationCount);
void send_flip_diag_packet(uint32_t registerHookCallCount, uint32_t flipHookCallCount,
                            uint32_t displayBufferIndex, uint64_t liveBufferAddr,
                            uint32_t haveValidFormat, uint32_t activeFormat,
                            uint32_t videoOutSubmitFlipHookCallCount,
                            uint32_t submitFlipPtrResolved,
                            uint32_t gnmForWorkloadHookCallCount,
                            uint32_t gnmForWorkloadPtrResolved); // called by sample_thread.c, v3.1
void ambient_read_content_preview(uint8_t *out, size_t previewLen); // called by settings.c
#define CONFIG_DEBUG_PREVIEW_LEN 16
void debug_send_raw(const uint8_t *data, int len); // also called directly by zones.c's detectHdr2200Format for its own diagnostic packet
void send_config_reload_debug_packet(uint32_t event, uint32_t statErrno,
                                      uint64_t curMtime, uint64_t lastMtime,
                                      uint64_t curSize, uint64_t lastSize,
                                      uint32_t checkCount,
                                      const uint8_t *contentPreview,
                                      uint32_t curHash, uint32_t lastHash); // called by settings.c
void wled_send_rgb_zones(const uint8_t *rgbTriplets, int numZones); // called by sample_thread.c
// v2.8: called only from sample_thread.c's no-valid-frame branch (unknown
// pixel format while foregrounded) -- NOT from the g_isBackgrounded path,
// where going silent is intentional. See network.c for the full comment.
void wled_send_keepalive_if_stale(uint64_t nowTicks, uint64_t tscFreq);

#define WLED_PORT       4048
#define DDP_HEADER_SIZE 10
#define MAX_TOTAL_ZONES 512
#define MAX_ZONES MAX_TOTAL_ZONES

// v2.4: sceSystemServiceGetStatus's real field layout, confirmed
// against shadPS4's independently reverse-engineered source -- see
// network.c for the full verification history/caveats on this struct.
// Resolved by name in main.c's plugin_load, read by sample_thread.c.
#define AMBIENT_SYS_SERVICE_STATUS_PADDING 128
typedef struct {
    int32_t eventNum;
    bool isSystemUiOverlaid;
    bool isInBackgroundExecution; // the one field this whole feature actually reads
    bool isCpuMode7CpuNormal;
    bool isGameLiveStreamingOnAir;
    bool isOutOfVrPlayArea;
    uint8_t reservedPadding[AMBIENT_SYS_SERVICE_STATUS_PADDING];
} AmbientSystemServiceStatus;
extern int32_t (*sceSystemServiceGetStatusPtr)(AmbientSystemServiceStatus *status); // defined in network.c, set in main.c's plugin_load

// [buffer_guard] -- v3.9, buffer_guard.c. Readability check before the sampler touches a display buffer.
extern int32_t (*sceKernelVirtualQueryPtr)(const void *addr, int32_t flags, void *info, uint64_t infoSize);
extern volatile uint32_t g_guardAvailable, g_guardRejectCount;
extern volatile uint64_t g_guardLastBadAddr;
extern volatile int32_t  g_guardLastRet;
extern volatile uint64_t g_guardInfoStart, g_guardInfoEnd, g_guardInfoOffset, g_guardInfoAddr;
extern volatile int32_t  g_guardInfoProt, g_guardInfoMemType;
extern volatile uint32_t g_guardInfoFlags;
void send_guard_info_packet(uint64_t start, uint64_t end, uint64_t offset, uint64_t addr,
                             int32_t prot, int32_t memType, uint32_t flags, int32_t ret); // GRDI, network.c, debug builds only
uint64_t ambient_readable_bytes(uint64_t addr, uint64_t len);
void send_guard_diag_packet(uint32_t available, uint32_t rejects, int32_t lastRet, uint64_t lastBadAddr); // GRDC, network.c, __FINAL__==0 callers only
bool ambient_buffer_readable(uint64_t bufferAddr); // false = first page is unmapped/not CPU-readable, do not read at all
// How many bytes from the buffer base are contiguously CPU-readable right now (set by
// ambient_buffer_readable). Every pixel read is bounds-checked against this instead of the
// fixed 1088-row ceiling, so a buffer shorter than the padded size skips its last pixels
// instead of faulting or being rejected outright.
extern volatile uint64_t g_readableLimit;
// [gpu_remap] -- v3.9, buffer_guard.c. Making a GPU-only direct-memory buffer CPU-readable (MK11).
extern int32_t (*sceKernelMapDirectMemoryPtr)(void **addr, uint64_t len, int32_t prot, int32_t flags, int64_t offset, uint64_t align);
extern int32_t (*sceKernelMapDirectMemory2Ptr)(void **addr, uint64_t len, int32_t type, int32_t prot, int32_t flags, int64_t offset, uint64_t align);
extern int32_t (*sceKernelMunmapPtr)(void *addr, uint64_t len);
extern int32_t (*sceKernelMprotectPtr)(const void *addr, uint64_t len, int32_t prot); // v3.9
extern volatile uint32_t g_gpuOnlyRemap, g_remapCreated, g_remapFailed, g_remapPasses, g_remapLastMethod;
extern volatile int32_t  g_remapLastRet;
extern volatile uint64_t g_remapLastAlias;
uint64_t ambient_remap_cpu_view(uint64_t bufferAddr);
uint64_t ambient_resolve_readable(uint64_t bufferAddr); // readable address (original or alias) with g_readableLimit set, or 0
void send_remap_diag_packet(uint32_t flags, uint32_t created, uint32_t failed, uint32_t passes, int32_t lastRet, uint64_t lastAlias); // RMAP, network.c, debug builds only
// [report] -- v3.9.1, report.c. The per-title report file a tester sends instead of a capture.
// AMBIENT_VERSION_STRING is the full version; g_pluginVersion (hooks.c) only holds major.minor,
// because CI compares a tag's major.minor against it. tools/test_report.c fails if the two
// disagree on major.minor.
#define AMBIENT_VERSION_STRING "3.9.1"
extern volatile uint32_t g_reportEnabled;        // ini [compat] report_file (optional): 1 on, default 0 (off)
extern volatile uint32_t g_reportLoopPasses;     // bumped once per completed sampler loop, sample_thread.c
extern volatile uint32_t g_reportTimingWindows, g_reportPassUsAvg, g_reportPassUsMax; // set once per timing window
void *ambient_report_thread(void *args);         // started by main.c's plugin_load, writes the file

extern bool g_isBackgrounded; // defined in network.c, updated by sample_thread.c, read by settings.c

// [tiling] -- confirmed-correct BASE detile params only (tiling.c)
typedef struct {
    uint32_t pipeConfig, numPipes, bankWidth, bankHeight, numBanks;
    uint32_t macroTileWidth, macroTileHeight, paddedWidth, tileSplitBytes;
    uint32_t pipeInterleaveBits, pipeBits, bankBits;
} TileParams;
extern const TileParams kParamsBase;
#define BASE_PADDED_HEIGHT 1088
#define BASE_PADDED_BUFFER_BYTES ((uint64_t)1920 * BASE_PADDED_HEIGHT * 4)
uint64_t getTiledElementByteOffset(const TileParams *p, uint32_t x, uint32_t y); // tiling.c, called by zones.c

// [pixel_formats] -- runtime format dispatch (pixel_formats.c)
typedef void (*PixelUnpackFn)(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b);
PixelUnpackFn getUnpackFnForFormat(uint32_t format); // called by sample_thread.c
// zones.c's detectHdr2200Format calls these two unpack functions
// directly (not through the function-pointer dispatch above) to try
// both hypotheses itself -- see that function for why.
void unpackA8B8G8R8_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b);
void unpackA2R10G10B10_BT2020_PQ_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b);
// v2.8 HDR-vs-SDR auto-detect state for format 0x80002200 -- written by
// zones.c's detectHdr2200Format, read by pixel_formats.c's
// getUnpackFnForFormat. See pixel_formats.c for the full method.
#define HDR2200_DETECT_SAMPLES   8
#define HDR2200_DETECT_INTERVAL  60
#define HDR2200_STREAK_THRESHOLD 4
#define HDR2200_NOISE_FLOOR      24
extern volatile int     g_hdr2200IsHdr;
extern volatile int32_t g_hdr2200Streak;
extern volatile int32_t g_hdr2200Countdown;

// v3.5: 0x88740000 with 8-bit ARGB in the buffer (YouTube, SDR video, HDR on).
// Set by zones.c's detectPq8bitMisregistration, read by pixel_formats.c's
// getUnpackFnForFormat. The vote itself is in pq8bit_vote.h.
extern volatile int     g_pq8bitMode;

// [zones] -- screen-edge sample points (zones.c)
#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080
// Upper bound for scanDepth (config's scan_depth). sampleZoneAverage in
// zones.c reads (2*scanDepth+1)^2 pixels PER ZONE, PER SAMPLE-THREAD
// PASS, each one a real getTiledElementByteOffset() tile-address
// computation -- not a flat read. At the unclamped old ceiling of 10,
// worst case is 512 zones (MAX_TOTAL_ZONES) x 441 samples x up to 240Hz
// (update_frequency_hz's own max) =~ 54M tiled-offset computations/sec.
// 4 caps that same worst case at 512 x 81 x 240 =~ 10M/sec -- still the
// upper end, but roughly a 5x cut -- while leaving real headroom over
// the default of 1 (9 samples) for anyone who actually wants a
// noticeably smoother average. Enforced here (settings.c's ini loader)
// so this holds regardless of how scan_depth got set -- a hand-edited
// ini, not just the companion app's own UI, which mirrors this same
// cap on its own Edge depth slider.
#define SCAN_DEPTH_MAX 4
// Upper bound for saturation and contrast (both -100..MAX, 0 = unchanged).
// This used to be 300 (4x), but the result gets clamped to 0-255 per
// channel at the end of the color chain, so most of that range just
// clipped. Simulated over sample colors: at +100 (2x) about 84% of vivid
// colors already have a channel clipped and each further +25 buys about
// a third of the visible change the first steps did; at 300, 99.8% of
// vivid colors clip. For contrast the same 2x factor already clips half
// the tonal range (1 - 1/2), and 4x clips about 75%. Unlike scan_depth,
// settings.c CLAMPS these two on load instead of rejecting an
// out-of-range value: a saved saturation=175 should land on the cap, not
// silently fall back to 0 (no saturation at all).
#define SATURATION_MAX 100
#define CONTRAST_MAX   100
extern uint32_t g_zoneX[MAX_TOTAL_ZONES];
extern uint32_t g_zoneY[MAX_TOTAL_ZONES];
extern uint32_t g_numZones;
void buildZoneGeometry(void); // called by main.c, settings.c (live layout reload), and letterbox.c (a committed auto-border changes effective margins)
void detectHdr2200Format(uint64_t bufferAddr); // called by sample_thread.c
void detectPq8bitMisregistration(uint64_t bufferAddr); // v3.5, called by sample_thread.c
void sampleZoneAverage(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                        uint32_t cx, uint32_t cy, uint8_t *outR, uint8_t *outG, uint8_t *outB); // called by sample_thread.c

// [letterbox] -- auto black-bar detection (letterbox.c), the sole
// source of capture margin (see zones.c's effMargin{Top,Right,Bottom,
// Left} helpers). All four default to 0 (no letterbox found, or the
// feature is off) until ambient_check_letterbox has committed a real
// border.
extern uint32_t g_autoLetterboxTop, g_autoLetterboxRight, g_autoLetterboxBottom, g_autoLetterboxLeft;
void ambient_check_letterbox(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack); // called by sample_thread.c, once per loop pass (throttled internally)

// [color_processing] -- gamma->brightness->contrast->saturation->levels->order (color_processing.c)
void applyColorProcessing(uint8_t r8, uint8_t g8, uint8_t b8, uint8_t *out3); // called by sample_thread.c
void applyDarkThreshold(uint32_t zoneIdx, uint8_t *r, uint8_t *g, uint8_t *b); // called by sample_thread.c

// [sample_thread] -- the worker thread itself (sample_thread.c), started by main.c's plugin_load
void *ambient_sample_thread(void *args);

// [hooks] -- GoldHEN loader metadata + real-hook state (hooks.c)
#define MAX_TRACKED_BUFFERS 16
extern volatile uint64_t g_bufferAddrs[MAX_TRACKED_BUFFERS];
extern volatile int32_t  g_bufferCount;
extern volatile uint32_t g_activeFormat;
extern volatile int      g_haveValidFormat;
extern volatile uint32_t g_currentDisplayBufferIndex;
extern volatile uint32_t g_prevDisplayBufferIndex;
extern volatile uint32_t g_prevPrevDisplayBufferIndex;

// How many flips back the sampler reads. 0 = the slot the hook just reported
// (can be cleared / half drawn: causes flicker), 1 = previous flip (finished),
// 2 = two flips back. sample_thread.c reads g_prevDisplayBufferIndex or
// g_prevPrevDisplayBufferIndex accordingly -- each level needs its own
// tracked variable, there's no way to derive "N flips back" from just the
// current index. Default 2: at about 60 fps one flip back was not enough for
// the GPU to finish the slot (Red Dead Redemption), and a value above 1 did
// nothing at all before v3.7. Values above 2 behave as 2.
// See docs/debugging/sample-lag-and-boot-flash.md.
#ifndef AMBIENT_SAMPLE_LAG
#define AMBIENT_SAMPLE_LAG 2
#endif
extern int32_t (*sceVideoOutRegisterBuffersPtr)(int32_t handle, int32_t startIndex,
                                          void *const *addresses, int32_t bufferNum,
                                          const OrbisVideoOutBufferAttribute *attribute);
extern int32_t (*sceGnmSubmitAndFlipCommandBuffersPtr)(uint32_t count, void *dcbGpuAddrs[],
                                                 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                 uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                 uint32_t displayBufferIndex, uint32_t flipMode,
                                                 int64_t flipArg);
// v3.1: two more real flip entrypoints -- see hooks.c's g_pluginVersion
// comment for why these exist.
extern int32_t (*sceVideoOutSubmitFlipPtr)(int32_t handle, int32_t bufferIndex,
                                            int32_t flipMode, int64_t flipArg);
extern int32_t (*sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr)(uint32_t workload, uint32_t count,
                                                                    void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes,
                                                                    void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
                                                                    uint32_t videoOutHandle, uint32_t displayBufferIndex,
                                                                    uint32_t flipMode, int64_t flipArg);
// v3.1: diagnostic only -- see hooks.c's own declarations/comments.
extern volatile uint32_t g_registerHookCallCount;
extern volatile uint32_t g_flipHookCallCount;
extern volatile uint32_t g_videoOutSubmitFlipHookCallCount;
extern volatile uint32_t g_submitFlipPtrResolved;
extern volatile uint32_t g_gnmForWorkloadHookCallCount;
extern volatile uint32_t g_gnmForWorkloadPtrResolved;

// The actual hook functions (hooks.c) and their Detour_* handles
// (created by HOOK_INIT in hooks.c) both need to be visible from
// main.c's plugin_load/plugin_unload, since that's where the real
// HOOK32(name)/UNHOOK(name) macro invocations live -- HOOK32 expands
// to code referencing Detour_##name and (&name##_hook) directly, by
// textual substitution, so both symbols must be in scope at the call
// site, not just inside hooks.c. HOOK_EXTERN is the SDK's own macro
// for exactly this (Utilities.h) -- same pattern as HOOK_INIT, just
// extern instead of defining.
int32_t sceVideoOutRegisterBuffersPtr_hook(int32_t handle, int32_t startIndex,
                                            void *const *addresses, int32_t bufferNum,
                                            const OrbisVideoOutBufferAttribute *attribute);
int32_t sceGnmSubmitAndFlipCommandBuffersPtr_hook(uint32_t count, void *dcbGpuAddrs[],
                                                   uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                   uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                   uint32_t displayBufferIndex, uint32_t flipMode,
                                                   int64_t flipArg);
int32_t sceVideoOutSubmitFlipPtr_hook(int32_t handle, int32_t bufferIndex,
                                       int32_t flipMode, int64_t flipArg);
int32_t sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr_hook(uint32_t workload, uint32_t count,
                                                               void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes,
                                                               void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
                                                               uint32_t videoOutHandle, uint32_t displayBufferIndex,
                                                               uint32_t flipMode, int64_t flipArg);
HOOK_EXTERN(sceVideoOutRegisterBuffersPtr);
HOOK_EXTERN(sceGnmSubmitAndFlipCommandBuffersPtr);
HOOK_EXTERN(sceVideoOutSubmitFlipPtr);
HOOK_EXTERN(sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr);

// TIMING_ENABLED gates the loop-time telemetry in sample_thread.c --
// also referenced (as a plain #if, not calling anything) by main.c's
// plugin_load for the still-unset CORE_MASK affinity comment block.
#ifndef TIMING_ENABLED
#define TIMING_ENABLED 1
#endif

// g_smoothedRgb itself (the actual per-zone color array) stays private
// to sample_thread.c -- nothing else touches it. Only the validity
// flag crosses files: settings.c's ambient_check_config_reload clears
// it on a live layout change (new zone count/positions shouldn't
// smooth-blend from the old ones).
extern bool g_smoothedRgbValid;

#endif // AMBIENT_INTERNAL_H
