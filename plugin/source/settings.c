// settings.c -- part of ps4_ambient_light, split out of the original single-file main.c. AmbientConfig...
// More: docs/code-comments.md (plugin/source/settings.c:1)

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
#include <sys/stat.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "config.h"
#include "default_ini.h"
#include "preset_select.h"
#include "media_titles.h"


// Defaults match v1.3's hardcoded behavior exactly -- upgrading from a build with no ini file present...
// More: docs/code-comments.md (plugin/source/settings.c:27)
char g_titleId[16] = "";

AmbientConfig g_config = {
    // wledHost intentionally blank, not pre-filled with this project's own controller's IP -- a build...
    // More: docs/code-comments.md (plugin/source/settings.c:35)
    .wledHost = "",
    .wledPort = 4048,
    // relayHost blank for the same reason as wledHost above -- also moot either way, since...
    // More: docs/code-comments.md (plugin/source/settings.c:42)
    .relayHost = "",
    .relayPort = 24689, // must match wled-relay's tv_external_source.EXTERNAL_SOURCE_PORT
    .relaySignalEnabled = false, // opt-in only -- see the field comment above
    .ledCountTop = 73, .ledCountRight = 41, .ledCountBottom = 73, .ledCountLeft = 42,
    .startCorner = CORNER_BOTTOM_LEFT,
    .direction = DIR_CLOCKWISE,
    .ledOffset = 0,
    .scanDepth = 1,
    // v3.0: on by default -- this is now the only inset applied before sampling (see ambient_internal.h)....
    // More: docs/code-comments.md (plugin/source/settings.c:54)
    .autoLetterboxEnabled = true,
    .autoLetterboxThreshold = 18,
    .autoLetterboxStabilityFrames = 3,
    .autoLetterboxCheckIntervalFrames = 15, // ~2x/sec at the 30Hz default update_frequency_hz
    .brightness = 255,
    .gammaLutIndex = 0, // gamma 1.0 == passthrough, matches v1.3 (no color processing existed)
    .saturation = 0,
    .colorOrder = ORDER_RGB,
    .blackLevel = 0,     // no-op -- matches "no black_level existed before v2.1" exactly
    .whiteLevel = 100,   // no-op
    .darkThreshold = 0,  // v2.1's original "0 = disabled" default -- reverted back
                         // from 10 (which had been kept in sync with the companion
                         // app's own settings_set_defaults()); both now default to 0
    .contrast = 0,       // no-op -- new in v2.2, matches "didn't exist before" like the rest of this block
    .brightnessR = 100, .brightnessG = 100, .brightnessB = 100, // no-op (100 = unchanged, Android convention)
    .gammaR = 100, .gammaG = 100, .gammaB = 100,                 // no-op (100 = unchanged, Android convention)
    .notificationsEnabled = true, // [notifications] enabled; see ambient_internal.h
    .devIp = "", .devLoggingEnabled = false, // v2.2.5: disabled unless a real ini opts in -- see struct comment
    .updateFrequencyHz = 30,
    .smoothingEnabled = 0, // off by default -- v1.3 had no smoothing, don't change behavior silently
    .settlingTimeMs = 200,
    .configReloadCheckSeconds = 5, // matches the auto-generated template's default (5s) -- this is a
                                    // brand-new capability with no prior behavior to preserve, so unlike every other default above it does...
                                    // More: docs/code-comments.md (plugin/source/settings.c:80)
};

static bool ambient_file_exists(const char *filename)
{
    struct stat buff;
    return stat(filename, &buff) == 0;
}

// While a watcher thread's copy of the ini is being applied, the loaders parse that copy and never open the file.
// Set only around ambient_load_config(), on the applying thread. [docs/debugging/filesystem-stalls.md]
static const char *g_cfgSnapData = NULL;
static size_t      g_cfgSnapLen = 0;

void ambient_config_snapshot_set(const char *data, size_t len) // NULL clears it; both loaders then read the file again
{
    g_cfgSnapData = data;
    g_cfgSnapLen = len;
}

bool ambient_config_read_table(ini_table_s *table)
{
    if (g_cfgSnapData != NULL) return ini_table_read_from_buffer(table, g_cfgSnapData, g_cfgSnapLen);
    return ini_table_read_from_file(table, AMBIENT_CONFIG_PATH);
}

// What the last ambient_load_config() found, for the plugin's notifications (notices.c).
// More: docs/code-comments.md (plugin/source/settings.c:92)
volatile uint32_t g_configLoadCount = 0;        // bumped at the end of every ambient_load_config() call
volatile uint32_t g_configDefaultCreated = 0;   // the last call wrote the default ini (there was none)
volatile uint32_t g_configParseFailed = 0;      // the last call found an ini it could not read
volatile uint32_t g_configBadCount = 0;         // keys the last call ignored: present, but not a valid value
char g_configBadKey[48] = "";                   // "[section] key" of the first of them

static void cfg_note_bad(const char *section, const char *key)
{
    if (g_configBadCount++ == 0)
        snprintf(g_configBadKey, sizeof(g_configBadKey), "[%s] %s", section, key);
}

// Same as `ini_table_get_entry_as_int(...) && lo <= *out && *out <= hi`, which is what each of these
// lines used to spell out, except that a key that is present but unusable is recorded for the notice.
static bool cfg_int_in(ini_table_s *table, const char *section, const char *key, int lo, int hi, int *out)
{
    if (!ini_table_get_entry_as_int(table, section, key, out)) {
        if (ini_table_check_entry(table, section, key)) cfg_note_bad(section, key);
        return false;
    }
    if (*out < lo || *out > hi) { cfg_note_bad(section, key); return false; }
    return true;
}

// Same sceKernelOpen-not-fopen gotcha plugin_loader's own code comment flags ("Does not work, may not...
// More: docs/code-comments.md (plugin/source/settings.c:119)
static void ambient_create_default_config(void)
{
    int32_t f = sceKernelOpen(AMBIENT_CONFIG_PATH, 0x200 | 0x001, 0777);
    if (f < 0) return; // no write access or path issue -- defaults above still apply in memory
    sceKernelWrite(f, AMBIENT_DEFAULT_INI, strlen(AMBIENT_DEFAULT_INI));
    sceKernelClose(f);
}

static ColorOrder parse_color_order(const char *s, ColorOrder fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "RGB")) return ORDER_RGB;
    if (!strcmp(s, "RBG")) return ORDER_RBG;
    if (!strcmp(s, "GRB")) return ORDER_GRB;
    if (!strcmp(s, "GBR")) return ORDER_GBR;
    if (!strcmp(s, "BRG")) return ORDER_BRG;
    if (!strcmp(s, "BGR")) return ORDER_BGR;
    cfg_note_bad("color", "color_order");
    return fallback; // unrecognized -- keep default rather than guess
}

static StartCorner parse_start_corner(const char *s, StartCorner fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "bottom_left"))  return CORNER_BOTTOM_LEFT;
    if (!strcmp(s, "bottom_right")) return CORNER_BOTTOM_RIGHT;
    if (!strcmp(s, "top_left"))     return CORNER_TOP_LEFT;
    if (!strcmp(s, "top_right"))    return CORNER_TOP_RIGHT;
    cfg_note_bad("layout", "led_start_corner");
    return fallback;
}

static LedDirection parse_direction(const char *s, LedDirection fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "clockwise")) return DIR_CLOCKWISE;
    if (!strcmp(s, "counterclockwise")) return DIR_COUNTERCLOCKWISE;
    cfg_note_bad("layout", "led_direction");
    return fallback;
}

// Matched by exact string, not parsed as a float -- deliberately, to avoid any float-parsing code path...
// More: docs/code-comments.md (plugin/source/settings.c:163)
static uint32_t parse_gamma_index(const char *section, const char *s, uint32_t fallback)
{
    static const char *kGammaStrings[NUM_GAMMA_LUTS] = {"1.0","1.4","1.8","2.0","2.2","2.4","2.6","2.8"};
    if (!s) return fallback;
    for (uint32_t i = 0; i < NUM_GAMMA_LUTS; i++) {
        if (!strcmp(s, kGammaStrings[i])) return i;
    }
    cfg_note_bad(section, "gamma");
    return fallback; // unrecognized string -- keep default rather than guess
}

// True if `section` holds at least one preset-owned key, i.e. the ini really has that preset.
// More: docs/code-comments.md (plugin/source/settings.c:179)
const char *g_presetOverride = NULL; // see ambient_internal.h; this plugin never sets it

static bool preset_section_has_keys(ini_table_s *table, const char *section)
{
    static const char *const kPresetKeys[] = {
        "scan_depth", "auto_letterbox_enabled", "auto_letterbox_threshold",
        "brightness", "saturation", "gamma", "black_level", "white_level", "contrast",
        "smoothing_enabled", "settling_time_ms", "dark_threshold",
        "brightness_r", "brightness_g", "brightness_b", "gamma_r", "gamma_g", "gamma_b",
    };
    for (unsigned i = 0; i < sizeof(kPresetKeys) / sizeof(kPresetKeys[0]); i++)
        if (ini_table_check_entry(table, section, kPresetKeys[i])) return true;
    return false;
}

void ambient_load_config(void)
{
    g_configDefaultCreated = 0;
    g_configParseFailed = 0;
    g_configBadCount = 0;
    g_configBadKey[0] = '\0';
    if (g_cfgSnapData == NULL && !ambient_file_exists(AMBIENT_CONFIG_PATH)) {
        ambient_create_default_config();
        g_configDefaultCreated = ambient_file_exists(AMBIENT_CONFIG_PATH) ? 1 : 0;
        ambient_rebuild_perchannel_gamma_luts(); // g_gammaLut{R,G,B} are static, zero-init by
                                                  // default -- MUST build the identity LUTs here too, or defaults (gammaR/G/B=100, meant to be a no-op)...
                                                  // More: docs/code-comments.md (plugin/source/settings.c:207)
        g_configLoadCount++;
        return; // g_config's compile-time defaults (== v1.3 behavior) stand otherwise
    }

    ini_table_s *table = ini_table_create();
    if (table == NULL || !ambient_config_read_table(table)) {
        if (table) ini_table_destroy(table);
        g_configParseFailed = 1;
        g_configLoadCount++;
        return; // parse failure -- keep defaults rather than run with a half-read config
    }

    const char *v;
    int iv; bool bv;

    // v3.8: the settings a preset owns (sampling depth, letterbox, colour, smoothing, RGB balance) are...
    // More: docs/code-comments.md (plugin/source/settings.c:226)
    const char *ps;
    if (g_presetOverride != NULL && preset_section_has_keys(table, g_presetOverride)) {
        // g_presetOverride wins over active=/the title guess, but only if its section actually has keys -- an...
        // More: docs/code-comments.md (plugin/source/settings.c:236)
        ps = g_presetOverride;
    } else {
        ps = preset_section_pick(ini_table_get_entry(table, "presets", "active"),
                                  media_title_is_movie(g_titleId),
                                  preset_section_has_keys(table, PRESET_SECTION_GAME),
                                  preset_section_has_keys(table, PRESET_SECTION_MOVIE));
    }
    const char *sLayout = ps ? ps : "layout";
    const char *sColor  = ps ? ps : "color";
    const char *sTiming = ps ? ps : "timing";

    // v3.9: [compat] gpu_only_remap.
    // More: docs/code-comments.md (plugin/source/settings.c:251)
    if (cfg_int_in(table, "compat", "gpu_only_remap", 0, 3, &iv))
        g_gpuOnlyRemap = (uint32_t)iv;

    // v3.9.1: [compat] report_file.
    // More: docs/code-comments.md (plugin/source/settings.c:259)
    if (ini_table_get_entry_as_int(table, "compat", "report_file", &iv) && (iv == 0 || iv == 1))
        g_reportEnabled = (uint32_t)iv;

    if ((v = ini_table_get_entry(table, "network", "wled_host")) != NULL) {
        strncpy(g_config.wledHost, v, sizeof(g_config.wledHost) - 1);
        g_config.wledHost[sizeof(g_config.wledHost) - 1] = '\0';
    }
    if (cfg_int_in(table, "network", "wled_port", 1, 65535, &iv))
        g_config.wledPort = (uint16_t)iv;

    if ((v = ini_table_get_entry(table, "network", "relay_host")) != NULL) {
        strncpy(g_config.relayHost, v, sizeof(g_config.relayHost) - 1);
        g_config.relayHost[sizeof(g_config.relayHost) - 1] = '\0';
    }
    if (cfg_int_in(table, "network", "relay_port", 1, 65535, &iv))
        g_config.relayPort = (uint16_t)iv;
    if (ini_table_get_entry_as_bool(table, "network", "relay_signal_enabled", &bv))
        g_config.relaySignalEnabled = bv;

    if (cfg_int_in(table, "layout", "led_count_top", 1, 2147483647, &iv))
        g_config.ledCountTop = (uint32_t)iv;
    if (cfg_int_in(table, "layout", "led_count_right", 1, 2147483647, &iv))
        g_config.ledCountRight = (uint32_t)iv;
    if (cfg_int_in(table, "layout", "led_count_bottom", 1, 2147483647, &iv))
        g_config.ledCountBottom = (uint32_t)iv;
    if (cfg_int_in(table, "layout", "led_count_left", 1, 2147483647, &iv))
        g_config.ledCountLeft = (uint32_t)iv;
    g_config.startCorner = parse_start_corner(ini_table_get_entry(table, "layout", "led_start_corner"), g_config.startCorner);
    g_config.direction = parse_direction(ini_table_get_entry(table, "layout", "led_direction"), g_config.direction);
    if (ini_table_get_entry_as_int(table, "layout", "led_offset", &iv))
        g_config.ledOffset = iv;
    if (cfg_int_in(table, sLayout, "scan_depth", 0, SCAN_DEPTH_MAX, &iv))
        g_config.scanDepth = (uint32_t)iv;

    if (ini_table_get_entry_as_bool(table, sLayout, "auto_letterbox_enabled", &bv))
        g_config.autoLetterboxEnabled = bv;
    if (cfg_int_in(table, sLayout, "auto_letterbox_threshold", 0, 255, &iv))
        g_config.autoLetterboxThreshold = (uint32_t)iv;
    if (cfg_int_in(table, "layout", "auto_letterbox_stability_frames", 1, 2147483647, &iv))
        g_config.autoLetterboxStabilityFrames = (uint32_t)iv;
    if (cfg_int_in(table, "layout", "auto_letterbox_check_interval_frames", 1, 2147483647, &iv))
        g_config.autoLetterboxCheckIntervalFrames = (uint32_t)iv;

    if (cfg_int_in(table, sColor, "brightness", 0, 255, &iv))
        g_config.brightness = (uint32_t)iv;
    g_config.gammaLutIndex = parse_gamma_index(sColor, ini_table_get_entry(table, sColor, "gamma"), g_config.gammaLutIndex);
    if (ini_table_get_entry_as_int(table, sColor, "saturation", &iv))
        g_config.saturation = iv < -100 ? -100 : (iv > SATURATION_MAX ? SATURATION_MAX : iv);
    g_config.colorOrder = parse_color_order(ini_table_get_entry(table, "color", "color_order"), g_config.colorOrder);
    if (cfg_int_in(table, sColor, "black_level", 0, 100, &iv))
        g_config.blackLevel = (uint32_t)iv;
    if (cfg_int_in(table, sColor, "white_level", 0, 100, &iv))
        g_config.whiteLevel = (uint32_t)iv;
    if (cfg_int_in(table, sColor, "dark_threshold", 0, 255, &iv))
        g_config.darkThreshold = (uint32_t)iv;

    // v2.2: Android-ported per-channel color knobs (see AmbientConfig struct comment for why these use a...
    // More: docs/code-comments.md (plugin/source/settings.c:318)
    if (ini_table_get_entry_as_int(table, sColor, "contrast", &iv))
        g_config.contrast = iv < -100 ? -100 : (iv > CONTRAST_MAX ? CONTRAST_MAX : iv);
    if (cfg_int_in(table, sColor, "brightness_r", 0, 500, &iv))
        g_config.brightnessR = (uint32_t)iv;
    if (cfg_int_in(table, sColor, "brightness_g", 0, 500, &iv))
        g_config.brightnessG = (uint32_t)iv;
    if (cfg_int_in(table, sColor, "brightness_b", 0, 500, &iv))
        g_config.brightnessB = (uint32_t)iv;
    if (cfg_int_in(table, sColor, "gamma_r", 10, 500, &iv))
        g_config.gammaR = (uint32_t)iv;
    if (cfg_int_in(table, sColor, "gamma_g", 10, 500, &iv))
        g_config.gammaG = (uint32_t)iv;
    if (cfg_int_in(table, sColor, "gamma_b", 10, 500, &iv))
        g_config.gammaB = (uint32_t)iv;
    ambient_rebuild_perchannel_gamma_luts();

    // v2.2.5: [dev] is intentionally undocumented in the generated ini template (AMBIENT_DEFAULT_INI) and...
    // More: docs/code-comments.md (plugin/source/settings.c:337)
    if ((v = ini_table_get_entry(table, "dev", "dev_ip")) != NULL) {
        strncpy(g_config.devIp, v, sizeof(g_config.devIp) - 1);
        g_config.devIp[sizeof(g_config.devIp) - 1] = '\0';
    }
    if (ini_table_get_entry_as_bool(table, "dev", "dev_logging", &bv))
        g_config.devLoggingEnabled = bv;

    // [notifications] enabled: optional, default on, not in the shipped ini. Read like every other switch
    // in this ini: on/true/1 is on, any other word is off.
    if (ini_table_get_entry_as_bool(table, "notifications", "enabled", &bv))
        g_config.notificationsEnabled = bv;

    if (cfg_int_in(table, "timing", "update_frequency_hz", 1, 240, &iv))
        g_config.updateFrequencyHz = (uint32_t)iv;
    if (ini_table_get_entry_as_bool(table, sTiming, "smoothing_enabled", &bv))
        g_config.smoothingEnabled = bv ? 1 : 0;
    if (cfg_int_in(table, sTiming, "settling_time_ms", 0, 2147483647, &iv))
        g_config.settlingTimeMs = (uint32_t)iv;
    if (cfg_int_in(table, "timing", "config_reload_check_seconds", 0, 2147483647, &iv))
        g_config.configReloadCheckSeconds = (uint32_t)iv;

    ini_table_destroy(table);
    g_configLoadCount++;
}

// v2.1 live config reload: re-reads the ini file if its mtime changed, checked at most once every...
// More: docs/code-comments.md (plugin/source/settings.c:372)
#define AMBIENT_FNV1A_OFFSET_BASIS 0x811c9dc5u
#define AMBIENT_FNV1A_PRIME        0x01000193u
static uint32_t ambient_fnv1a32(const uint8_t *data, size_t len)
{
    uint32_t hash = AMBIENT_FNV1A_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= AMBIENT_FNV1A_PRIME;
    }
    return hash;
}

// Sanity cap on the hash read, independent of the earlier no-libm constraint -- this is about not...
// More: docs/code-comments.md (plugin/source/settings.c:433)
#define AMBIENT_CONFIG_MAX_HASH_READ (1024 * 1024)

// Replaces v2.1.4's ambient_get_real_config_size: still gets size the same proven way (sceKernelOpen +...
// More: docs/code-comments.md (plugin/source/settings.c:440)
bool ambient_read_config_snapshot(int64_t *outSize, uint32_t *outHash, uint8_t **outData, size_t *outLen)
{
    int32_t fd = sceKernelOpen(AMBIENT_CONFIG_PATH, 0 /* O_RDONLY */, 0777);
    if (fd < 0) {
        *outSize = fd; // negative orbis error code, reused as the packet's "errno" field below
        return false;
    }
    int64_t sz = sceKernelLseek(fd, 0, SEEK_END);
    if (sz < 0) {
        sceKernelClose(fd);
        *outSize = sz;
        return false;
    }
    if (sz > AMBIENT_CONFIG_MAX_HASH_READ) {
        sceKernelClose(fd);
        *outSize = -1; // sentinel, not a real orbis error code -- "file unexpectedly huge", not an open/lseek failure
        return false;
    }

    int64_t seekBack = sceKernelLseek(fd, 0, SEEK_SET);
    if (seekBack < 0) {
        sceKernelClose(fd);
        *outSize = seekBack;
        return false;
    }

    // sz fits in AMBIENT_CONFIG_MAX_HASH_READ (checked above), so this is a small, bounded allocation, not...
    // More: docs/code-comments.md (plugin/source/settings.c:479)
    uint8_t *buf = (sz > 0) ? (uint8_t *)malloc((size_t)sz) : NULL;
    if (sz > 0 && buf == NULL) {
        sceKernelClose(fd);
        *outSize = -1; // sentinel -- malloc failure, not an orbis error code
        return false;
    }

    ssize_t nread = (sz > 0) ? sceKernelRead(fd, buf, (size_t)sz) : 0;
    sceKernelClose(fd);
    if (nread < 0) {
        free(buf);
        *outSize = nread;
        return false;
    }

    *outSize = sz;
    *outHash = ambient_fnv1a32(buf, (size_t)nread);
    if (outData != NULL) { *outData = buf; *outLen = (size_t)nread; } // the caller frees it; buf is NULL for an empty file
    else free(buf);
    return true;
}

bool ambient_get_real_config_size_and_hash(int64_t *outSize, uint32_t *outHash)
{
    return ambient_read_config_snapshot(outSize, outHash, NULL, NULL);
}

// mtime is kept in the struct/packet purely for backward-compatible layout with the v2.1.2/v2.1.3...
// More: docs/code-comments.md (plugin/source/settings.c:503)
static bool g_haveConfigBaseline = false;
static off_t g_configLastSize = 0;
// v2.1.5: content hash (FNV-1a/32) of the whole file, alongside size.
// More: docs/code-comments.md (plugin/source/settings.c:513)
static uint32_t g_configLastHash = 0;
// Increments on every call, independent of what the call finds -- see
// send_config_reload_debug_packet's check_count field above for why.
static uint32_t g_configCheckCount = 0;

// Slow file operations are logged (HOF1 FS_SLOW) when they finish, so a capture names the call that was stuck and for how long.
#define FS_SLOW_MS 100ULL
extern uint64_t sceKernelGetProcessTimeCounter(void);          // same declarations further down; harmless if the SDK header already has them
extern uint64_t sceKernelGetProcessTimeCounterFrequency(void);
static void fs_slow_note(const char *op, uint64_t startTsc)
{
    uint64_t hz = sceKernelGetProcessTimeCounterFrequency(), now = sceKernelGetProcessTimeCounter();
    if (hz == 0) return;
    uint64_t ms = (now - startTsc) * 1000ULL / hz;
    if (ms >= FS_SLOW_MS)
        ambient_handoff_diag("HANDOFF_DIAG FS_SLOW op=%s ms=%llu tsc=%llu hz=%llu", op,
                             (unsigned long long)ms, (unsigned long long)now, (unsigned long long)hz);
}

// Live reload is split in two. The watcher thread does ALL the file I/O (read, hash, diagnostic) and parks a copy of a
// changed ini here; the sampler only applies it from memory, so a stuck filesystem cannot stop frames. [docs/debugging/filesystem-stalls.md]
typedef struct { uint8_t *data; size_t len; } CfgSnap;
static CfgSnap *volatile g_cfgPending = NULL;
volatile int g_cfgWatcherRunning = 0; // set by main.c once the watcher thread exists; 0 = the sampler checks inline, as before

// The file half: one read per check (hash, preview and the copy all come from it). Watcher thread only.
void ambient_config_watch_tick(void)
{
    g_configCheckCount++;

    if (g_config.configReloadCheckSeconds == 0) {
        uint8_t preview[CONFIG_DEBUG_PREVIEW_LEN];
        ambient_read_content_preview(preview, sizeof(preview));
        send_config_reload_debug_packet(0, 0, 0, 0,
                                         0, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         0, g_configLastHash);
        return;
    }

    int64_t realSize;
    uint32_t realHash;
    uint8_t *data = NULL;
    size_t dataLen = 0;
    uint64_t ioStart = sceKernelGetProcessTimeCounter();
    bool readOk = ambient_read_config_snapshot(&realSize, &realHash, &data, &dataLen);
    fs_slow_note("cfg_read", ioStart);
    if (!readOk) {
        uint8_t preview[CONFIG_DEBUG_PREVIEW_LEN];
        ambient_read_content_preview(preview, sizeof(preview));
        send_config_reload_debug_packet(1, (uint32_t)(-realSize), 0, 0,
                                         0, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         0, g_configLastHash);
        return; // file gone/unreadable/unexpectedly huge -- keep running on current config
    }

    uint8_t preview[CONFIG_DEBUG_PREVIEW_LEN];
    memset(preview, 0, sizeof(preview));
    if (data != NULL) memcpy(preview, data, dataLen < sizeof(preview) ? dataLen : sizeof(preview));

    if (!g_haveConfigBaseline) {
        send_config_reload_debug_packet(2, 0, 0, 0,
                                         (uint64_t)realSize, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         realHash, g_configLastHash);
        g_configLastSize = (off_t)realSize;
        g_configLastHash = realHash;
        g_haveConfigBaseline = true;
        free(data);
        return; // first check just establishes a baseline, nothing to compare against yet
    }
    if ((off_t)realSize == g_configLastSize && realHash == g_configLastHash) {
        send_config_reload_debug_packet(3, 0, 0, 0,
                                         (uint64_t)realSize, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         realHash, g_configLastHash);
        free(data);
        return; // unchanged -- same size AND same content hash
    }

    send_config_reload_debug_packet(4, 0, 0, 0,
                                     (uint64_t)realSize, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                     realHash, g_configLastHash);

    CfgSnap *snap = (CfgSnap *)malloc(sizeof(*snap));
    if (snap == NULL) { free(data); return; } // baseline not advanced: the next check sees the change again
    snap->data = data;
    snap->len = dataLen;
    g_configLastSize = (off_t)realSize;
    g_configLastHash = realHash;
    CfgSnap *older = __atomic_exchange_n(&g_cfgPending, snap, __ATOMIC_ACQ_REL); // a newer edit replaces one nobody applied yet
    if (older != NULL) { free(older->data); free(older); }
}

// The memory half, on the sampler: applies a copy the watcher parked. Returns true when a reload happened.
bool ambient_apply_pending_config(void)
{
    CfgSnap *snap = __atomic_exchange_n(&g_cfgPending, NULL, __ATOMIC_ACQ_REL);
    if (snap == NULL) return false;

    // On a live relay_signal_enabled edit, send "on" or release the relay once (not while backgrounded), so the relay
    // is never left waiting on a flag this plugin no longer touches. [docs/code-comments.md]
    bool wasRelaySignalEnabled = g_config.relaySignalEnabled;
    static const char kEmpty[1] = "";
    ambient_config_snapshot_set(snap->data != NULL ? (const char *)snap->data : kEmpty, snap->len);
    ambient_load_config();  // parses the copy; unset/removed keys keep their CURRENT g_config value, not the compiled default
    ambient_config_snapshot_set(NULL, 0);
    free(snap->data);
    free(snap);
    if (g_config.relaySignalEnabled != wasRelaySignalEnabled && !g_isBackgrounded) {
        if (g_config.relaySignalEnabled) relay_send_external_source(true);
        else                             relay_release_external_source(); // not while the shell feed holds the strip
    }
    buildZoneGeometry();    // layout settings may have changed -- rebuild g_zoneX/g_zoneY/g_numZones
    g_smoothedRgbValid = false; // avoid smoothing a hard cut between old and new zone counts/positions
    return true;
}

// Inline check, the old behaviour: only used while no watcher thread is running (main.c could not start one).
void ambient_check_config_reload(void)
{
    ambient_config_watch_tick();
    ambient_apply_pending_config();
}

// The watcher thread: every configReloadCheckSeconds it reads and hashes the ini, off the sampler.
void *ambient_config_thread(void *args)
{
    (void)args;
    for (;;) {
        uint32_t secs = (uint32_t)g_config.configReloadCheckSeconds;
        if (secs == 0) { usleep(500000); continue; } // reload off; look again in case it is switched on
        ambient_config_watch_tick();
        // Sleep in 100 ms steps so a live edit of the interval takes effect within a tenth of a second.
        for (uint32_t i = 0; i < secs * 10u && (uint32_t)g_config.configReloadCheckSeconds == secs; i++) usleep(100000);
    }
    return NULL;
}


// Is the shell feed really running?
// More: docs/code-comments.md (plugin/source/settings.c:607)
extern uint64_t sceKernelGetProcessTimeCounter(void);          // same declarations state.c carries; harmless if the SDK header already has them
extern uint64_t sceKernelGetProcessTimeCounterFrequency(void);
#define SHELL_ALIVE_PATH     AMBILIGHT_DATA_DIR "/ps4_shell_alive"
#define SHELL_ALIVE_STALE_MS 3000ULL
static char     lastText[32];
static uint64_t firstCheckTsc = 0, lastChangeTsc = 0;
void ambient_shell_feed_rearm(void) { lastText[0] = '\0'; firstCheckTsc = 0; lastChangeTsc = 0; }
bool ambient_shell_feed_expected(void)
{
    uint64_t now = sceKernelGetProcessTimeCounter();
    uint64_t hz  = sceKernelGetProcessTimeCounterFrequency();
    if (hz == 0) return true;
    if (firstCheckTsc == 0) firstCheckTsc = now;

    uint64_t ioT = sceKernelGetProcessTimeCounter();
    int32_t dfd = sceKernelOpen(AMBILIGHT_DATA_DIR "/ps4_shell_feed_off", 0 /* O_RDONLY */, 0);
    if (dfd >= 0) { sceKernelClose(dfd); return false; }

    ini_table_s *table = ini_table_create();
    if (table != NULL) {
        if (ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) {
            bool bv = true;
            bool enabled = ini_table_get_entry_as_bool(table, "shell", "enabled", &bv) ? bv : true;
            if (!enabled) { ini_table_destroy(table); return false; }
        }
        ini_table_destroy(table);
    }

    char cur[32]; memset(cur, 0, sizeof(cur));
    int32_t fd = sceKernelOpen(SHELL_ALIVE_PATH, 0 /* O_RDONLY */, 0);
    if (fd >= 0) {
        ssize_t r = sceKernelRead(fd, cur, sizeof(cur) - 1);
        sceKernelClose(fd);
        if (r <= 0) cur[0] = '\0';
    }
    fs_slow_note("shell_alive_checks", ioT); // the disable file, the ini's [shell] enabled and the heartbeat read together
    if (cur[0] != '\0' && strcmp(cur, lastText) != 0) {
        memcpy(lastText, cur, sizeof(lastText));
        // The first read after load is only a baseline, so it does not count as proof of life by itself.
        if (lastChangeTsc != 0 || firstCheckTsc != now) lastChangeTsc = now;
        else lastChangeTsc = 0;
    }
    uint64_t ref = (lastChangeTsc != 0) ? lastChangeTsc : firstCheckTsc;
    return ((now - ref) * 1000ULL / hz) < SHELL_ALIVE_STALE_MS;
}
