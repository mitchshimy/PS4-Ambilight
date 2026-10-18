// ambient_internal.h -- shared types, config path, and cross-module externs for ps4_ambient_light...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:1)
#ifndef AMBIENT_INTERNAL_H
#define AMBIENT_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <orbis/libkernel.h>
#include <orbis/_types/video.h> // OrbisVideoOutBufferAttribute, used by the hook prototypes below
#include "ambilight_paths.h"   // AMBILIGHT_DATA_DIR, the one folder of ours under /data
#include "pq8bit_vote.h"        // v3.5, pure alpha-byte vote so a PC test can build it
#include "hdr2200_vote.h"      // v3.6, same idea for 0x80002200, also PC-buildable

// [settings] -- AmbientConfig struct, g_config instance lives in settings.c. See settings.c for the...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:24)
#define AMBIENT_CONFIG_PATH AMBILIGHT_DATA_DIR "/ps4_ambient_light.ini"
#define NUM_GAMMA_LUTS 8

typedef enum { CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT, CORNER_TOP_LEFT, CORNER_TOP_RIGHT } StartCorner;
typedef enum { DIR_CLOCKWISE, DIR_COUNTERCLOCKWISE } LedDirection;
typedef enum { ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR } ColorOrder;

typedef struct {
    // [network]
    char wledHost[64];
    uint16_t wledPort;
    // v2.3: separate destination for the external-source on/off signal (see relay_send_external_source...
    // More: docs/code-comments.md (plugin/include/ambient_internal.h:40)
    char relayHost[64];
    uint16_t relayPort;
    // v2.3.1: this whole external-source signal is a personal-setup integration with one specific...
    // More: docs/code-comments.md (plugin/include/ambient_internal.h:47)
    bool relaySignalEnabled;
    // [layout]
    uint32_t ledCountTop, ledCountRight, ledCountBottom, ledCountLeft;
    StartCorner startCorner;
    LedDirection direction;
    int32_t ledOffset;          // rotates which physical LED index 0 lands on
    uint32_t scanDepth;         // sample radius: (2*scanDepth+1)^2 pixels averaged per zone
    // v3.0: auto letterbox/black-bar detection is now the ONLY inset applied before sampling -- the old...
    // More: docs/code-comments.md (plugin/include/ambient_internal.h:59)
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
    // v2.2: ported from the Android "inspiration" project's own ColorProcessor.kt.
    // More: docs/code-comments.md (plugin/include/ambient_internal.h:82)
    int32_t contrast;           // -100..CONTRAST_MAX, 0 = unchanged (100=neutral Android pct minus 100, same convention as saturation above)
    uint32_t brightnessR, brightnessG, brightnessB; // 0-500, 100 = unchanged. Multiplies with the global brightness above.
    uint32_t gammaR, gammaG, gammaB;                // 10-500, 100 = unchanged. Independent per-channel curves, applied PER SAMPLE
                                                     // before zone-averaging (see sampleZoneAverage) -- unlike every other knob here, which is applied once...
                                                     // More: docs/code-comments.md (plugin/include/ambient_internal.h:93)
    // v2.2.5: opt-in developer telemetry destination.
    // More: docs/code-comments.md (plugin/include/ambient_internal.h:98)
    char devIp[16];          // dotted-quad only, e.g. "192.168.2.117"; empty = disabled regardless of devLoggingEnabled
    bool devLoggingEnabled;  // both this AND a non-empty devIp are required -- neither alone is enough
    // [notifications] enabled. Optional, default true, not in the shipped ini (hand-added, like [compat])....
    // More: docs/code-comments.md (plugin/include/ambient_internal.h:109)
    bool notificationsEnabled;
    // [timing]
    uint32_t updateFrequencyHz;
    int smoothingEnabled;
    uint32_t settlingTimeMs;
    uint32_t configReloadCheckSeconds; // v2.1: 0 = load once at start only (v2.0 behavior), like before
} AmbientConfig;

extern AmbientConfig g_config; // defined in settings.c
extern char g_titleId[16];     // defined in settings.c; set by plugin_load, picks the preset (media_titles.h)

// v3.9.4: force ambient_load_config() to use a specific [preset_*] section regardless of [presets]...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:124)
extern const char *g_presetOverride; // defined in settings.c, default NULL

// [gamma] -- kGammaLuts is the fixed 8-preset table (gamma.c); the per-channel g_gammaLutR/G/B are the...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:136)
extern const uint8_t kGammaLuts[NUM_GAMMA_LUTS][256];
extern float g_gammaLutR[256], g_gammaLutG[256], g_gammaLutB[256];
void ambient_rebuild_perchannel_gamma_luts(void); // gamma.c -- called by settings.c on every config (re)load

// [settings] -- ini load/save + live-reload detection (settings.c)
void ambient_load_config(void);       // called by main.c (plugin_load)
void ambient_check_config_reload(void); // inline check + apply; sample_thread.c uses it only while no watcher thread runs
void ambient_config_watch_tick(void);    // the watcher thread's body: one read + hash; parks a changed ini for the call below (exposed so a PC test can drive it)
bool ambient_apply_pending_config(void); // sample_thread.c, every pass: applies an ini copy the watcher parked (memory only); true if it reloaded
void *ambient_config_thread(void *args);  // main.c starts it: the file reads for live reload happen here, not on the sampler
extern volatile int g_cfgWatcherRunning;  // 1 once that thread exists
void ambient_shell_feed_rearm(void);    // forget the heartbeat history: the next checks start with the normal 3 s of grace
bool ambient_shell_feed_expected(void); // false when the shell feed is not running (heartbeat file stopped changing, or switched off); stateful, state thread only
extern volatile int g_shellFeedAlive;   // state.c's latest answer from the above, read by the startup hold in network.c; 1 until proven otherwise
bool ambient_get_real_config_size_and_hash(int64_t *outSize, uint32_t *outHash); // v3.9.4: exposed for shell-feed's own reload poll
bool ambient_read_config_snapshot(int64_t *outSize, uint32_t *outHash, uint8_t **outData, size_t *outLen); // same read, keeps the bytes (caller frees *outData; NULL for an empty file)

// [network] -- WLED/relay/debug transport (network.c)
extern int g_wledSockfd; // read directly by main.c's plugin_unload to close it
extern volatile uint32_t g_wledOutputCount; // network.c: increments only after a complete WLED DDP sendto() succeeds
void relay_send_external_source(bool active);   // called by main.c (plugin_load/unload) and settings.c (live toggle)
void relay_release_external_source(void);       // "off" to the relay, unless the shell feed is running and still holds the strip
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
void debug_send_raw(const uint8_t *data, int len);
void ambient_handoff_diag(const char *fmt, ...); // event-only HOF1 record over the opt-in dev UDP channel
void send_config_reload_debug_packet(uint32_t event, uint32_t statErrno,
                                      uint64_t curMtime, uint64_t lastMtime,
                                      uint64_t curSize, uint64_t lastSize,
                                      uint32_t checkCount,
                                      const uint8_t *contentPreview,
                                      uint32_t curHash, uint32_t lastHash); // called by settings.c
void wled_send_rgb_zones(const uint8_t *rgbTriplets, int numZones); // called by sample_thread.c
// v2.8: called only from sample_thread.c's no-valid-frame branch (unknown pixel format while...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:179)
void wled_send_keepalive_if_stale(uint64_t nowTicks, uint64_t tscFreq);

#define WLED_PORT       4048
#define DDP_HEADER_SIZE 10
#define MAX_TOTAL_ZONES 512
#define MAX_ZONES MAX_TOTAL_ZONES

// v2.4: sceSystemServiceGetStatus's real field layout, confirmed against shadPS4's independently...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:189)
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
// How many bytes from the buffer base are contiguously CPU-readable right now (set by...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:218)
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
// More: docs/code-comments.md (plugin/include/ambient_internal.h:234)
#define AMBIENT_VERSION_STRING "3.9.4"
extern volatile uint32_t g_reportEnabled;        // ini [compat] report_file (optional): 1 on, default 0 (off)
extern volatile uint32_t g_reportLoopPasses;     // bumped once per completed sampler loop, sample_thread.c
extern volatile uint32_t g_reportTimingWindows, g_reportPassUsAvg, g_reportPassUsMax; // set once per timing window
void *ambient_report_thread(void *args);         // started by main.c's plugin_load, writes the file
void *ambient_state_thread(void *args);          // started by main.c's plugin_load, writes /data/ps4-ambilight/ps4_ambient_state

// [notices] -- notices.c, plugin only.
// More: docs/code-comments.md (plugin/include/ambient_internal.h:245)
extern volatile uint32_t g_configLoadCount;       // bumped by every ambient_load_config()
extern volatile uint32_t g_configDefaultCreated;  // that load wrote the default ini
extern volatile uint32_t g_configParseFailed;     // that load couldn't read the ini
extern volatile uint32_t g_configBadCount;        // keys it ignored as invalid
extern char g_configBadKey[48];                   // "[section] key" of the first one
void ambient_notices_on_load(void);               // plugin_load, once hooks are in
void ambient_notices_on_reload(void);             // sampler thread, after a live config reload
void ambient_notice_hook_failed(const char *reason); // plugin_load, where it gives up
// Sampler thread, every pass: a title whose format or buffer the plugin can't use for a few
// seconds in a row gets one notice per launch.
void ambient_notices_tick(uint64_t nowTicks, uint64_t tscFreq, bool formatUnknown, uint32_t format, bool bufferUnreadable);

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
// zones.c's detectHdr2200Format calls these two unpack functions directly (not through the...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:276)
void unpackA8B8G8R8_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b);
void unpackA2R10G10B10_BT2020_PQ_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b);
// v2.8 HDR-vs-SDR auto-detect state for format 0x80002200 -- written by zones.c's detectHdr2200Format...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:281)
#define HDR2200_DETECT_SAMPLES   8
#define HDR2200_DETECT_INTERVAL  60
#define HDR2200_STREAK_THRESHOLD 4
#define HDR2200_NOISE_FLOOR      24
extern volatile int     g_hdr2200IsHdr;
extern volatile int32_t g_hdr2200Streak;
extern volatile int32_t g_hdr2200Countdown;

// v3.5: 0x88740000 with 8-bit ARGB in the buffer (YouTube, SDR video, HDR on).
// More: docs/code-comments.md (plugin/include/ambient_internal.h:292)
extern volatile int     g_pq8bitMode;

// [zones] -- screen-edge sample points (zones.c)
#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080
// Upper bound for scanDepth (config's scan_depth). sampleZoneAverage in zones.c reads...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:300)
#define SCAN_DEPTH_MAX 4
// Upper bound for saturation and contrast (both -100..MAX, 0 = unchanged).
// More: docs/code-comments.md (plugin/include/ambient_internal.h:314)
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

// [letterbox] -- auto black-bar detection (letterbox.c), the sole source of capture margin (see...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:336)
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

// How many flips back the sampler reads. 0 = the slot the hook just reported (can be cleared / half...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:361)
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

// The actual hook functions (hooks.c) and their Detour_* handles (created by HOOK_INIT in hooks.c)...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:398)
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

// TIMING_ENABLED gates the loop-time telemetry in sample_thread.c -- also referenced (as a plain #if...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:427)
#ifndef TIMING_ENABLED
#define TIMING_ENABLED 1
#endif

// g_smoothedRgb itself (the actual per-zone color array) stays private to sample_thread.c -- nothing...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:434)
extern bool g_smoothedRgbValid;

#endif // AMBIENT_INTERNAL_H
