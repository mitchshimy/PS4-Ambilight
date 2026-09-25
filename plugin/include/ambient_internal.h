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
    uint32_t marginTop, marginRight, marginBottom, marginLeft; // pixels inset from the true screen edge before sampling
    uint32_t scanDepth;         // sample radius: (2*scanDepth+1)^2 pixels averaged per zone
    // [color]
    uint32_t brightness;        // 0-255 global scale, applied after gamma
    uint32_t gammaLutIndex;     // index into kGammaLuts -- see NUM_GAMMA_LUTS below
    int32_t saturation;         // -100 (grayscale) .. 0 (unchanged) .. 300 (4x boost, mostly clipped by then)
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
    int32_t contrast;           // -100..300, 0 = unchanged (100=neutral Android pct minus 100, same convention as saturation above)
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

// [zones] -- screen-edge sample points (zones.c)
#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080
extern uint32_t g_zoneX[MAX_TOTAL_ZONES];
extern uint32_t g_zoneY[MAX_TOTAL_ZONES];
extern uint32_t g_numZones;
void buildZoneGeometry(void); // called by main.c and settings.c (live layout reload)
void detectHdr2200Format(uint64_t bufferAddr); // called by sample_thread.c
void sampleZoneAverage(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                        uint32_t cx, uint32_t cy, uint8_t *outR, uint8_t *outG, uint8_t *outB); // called by sample_thread.c

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
extern int32_t (*sceVideoOutRegisterBuffersPtr)(int32_t handle, int32_t startIndex,
                                          void *const *addresses, int32_t bufferNum,
                                          const OrbisVideoOutBufferAttribute *attribute);
extern int32_t (*sceGnmSubmitAndFlipCommandBuffersPtr)(uint32_t count, void *dcbGpuAddrs[],
                                                 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                 uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                 uint32_t displayBufferIndex, uint32_t flipMode,
                                                 int64_t flipArg);

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
HOOK_EXTERN(sceVideoOutRegisterBuffersPtr);
HOOK_EXTERN(sceGnmSubmitAndFlipCommandBuffersPtr);

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
