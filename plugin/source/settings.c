// settings.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. AmbientConfig defaults, ini load, and live-reload change detection.
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
#include <sys/stat.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "config.h"


// Defaults match v1.3's hardcoded behavior exactly -- upgrading from a
// build with no ini file present should look identical to before,
// not silently change anything.
AmbientConfig g_config = {
    // wledHost intentionally blank, not pre-filled with this project's
    // own controller's IP -- a build anyone else uses shouldn't ship
    // with someone else's network address. Blank just leaves DDP send
    // targeting nothing until a real ini (or the companion app) sets
    // wled_host; see AMBIENT_DEFAULT_INI below for the generated file.
    .wledHost = "",
    .wledPort = 4048,
    // relayHost blank for the same reason as wledHost above -- also
    // moot either way, since relaySignalEnabled defaults to false
    // below, so this address is never dialed unless a user explicitly
    // opts in AND sets their own relay_host in the ini.
    .relayHost = "",
    .relayPort = 24689, // must match wled-relay's tv_external_source.EXTERNAL_SOURCE_PORT
    .relaySignalEnabled = false, // opt-in only -- see the field comment above
    .ledCountTop = 73, .ledCountRight = 41, .ledCountBottom = 73, .ledCountLeft = 42,
    .startCorner = CORNER_BOTTOM_LEFT,
    .direction = DIR_CLOCKWISE,
    .ledOffset = 0,
    .marginTop = 0, .marginRight = 0, .marginBottom = 0, .marginLeft = 0,
    .scanDepth = 1,
    .brightness = 255,
    .gammaLutIndex = 0, // gamma 1.0 == passthrough, matches v1.3 (no color processing existed)
    .saturation = 0,
    .colorOrder = ORDER_RGB,
    .blackLevel = 0,     // no-op -- matches "no black_level existed before v2.1" exactly
    .whiteLevel = 100,   // no-op
    .darkThreshold = 0,  // disabled -- matches "no dark_threshold existed before v2.1" exactly
    .contrast = 0,       // no-op -- new in v2.2, matches "didn't exist before" like the rest of this block
    .brightnessR = 100, .brightnessG = 100, .brightnessB = 100, // no-op (100 = unchanged, Android convention)
    .gammaR = 100, .gammaG = 100, .gammaB = 100,                 // no-op (100 = unchanged, Android convention)
    .devIp = "", .devLoggingEnabled = false, // v2.2.5: disabled unless a real ini opts in -- see struct comment
    .updateFrequencyHz = 30,
    .smoothingEnabled = 0, // off by default -- v1.3 had no smoothing, don't change behavior silently
    .settlingTimeMs = 200,
    .configReloadCheckSeconds = 2, // matches the auto-generated template's default (2s) -- this is a
                                    // brand-new capability with no prior behavior to preserve, so unlike every
                                    // other default above it does NOT need to match "what v2.0 did" (v2.0 simply
                                    // couldn't do this at all). Only matters if ambient_create_default_config()
                                    // ever stops matching this value -- keep the two in sync by hand.
};

static bool ambient_file_exists(const char *filename)
{
    struct stat buff;
    return stat(filename, &buff) == 0;
}

// Same sceKernelOpen-not-fopen gotcha plugin_loader's own code comment
// flags ("Does not work, may not have write access") -- not
// rediscovering that the hard way here.
static void ambient_create_default_config(void)
{
    #define AMBIENT_DEFAULT_INI \
        "[network]\n" \
        "; The real WLED controller's IP -- NOT this PC's own IP.\n" \
        "wled_host=\n" \
        "wled_port=4048\n" \
        "; Advanced/optional: only relevant if you're also running the\n" \
        "; wled-relay companion project and want this plugin to tell it\n" \
        "; when it's driving the TV backlight WLED controller above\n" \
        "; directly, so that project's own audio-reactive effects don't\n" \
        "; fight this plugin for the same strip during a real game. OFF\n" \
        "; by default -- most users don't run that project and don't\n" \
        "; need this. Not part of this default file at all -- add\n" \
        "; relay_signal_enabled/relay_host/relay_port yourself, here in\n" \
        "; [network], only if you're actually running wled-relay.\n" \
        "\n" \
        "[layout]\n" \
        "; Physical LED counts per screen edge. Defaults match this\n" \
        "; project's own measured strip (73/41/73/42 = 229 total).\n" \
        "led_count_top=73\n" \
        "led_count_right=41\n" \
        "led_count_bottom=73\n" \
        "led_count_left=42\n" \
        "; Which corner physical LED index 0 sits at, and which way the\n" \
        "; strip runs from there. Valid led_start_corner: bottom_left,\n" \
        "; bottom_right, top_left, top_right. Valid led_direction:\n" \
        "; clockwise, counterclockwise.\n" \
        "led_start_corner=bottom_left\n" \
        "led_direction=clockwise\n" \
        "; If the light show is correct but rotated around the border\n" \
        "; (e.g. everything is one LED off from where it should be),\n" \
        "; adjust this instead of led_start_corner/led_direction.\n" \
        "led_offset=0\n" \
        "; Pixels to inset sampling from the true screen edge, per side.\n" \
        "; Raise these if overscan/black bars are being sampled instead\n" \
        "; of real picture content.\n" \
        "capture_margin_top=0\n" \
        "capture_margin_right=0\n" \
        "capture_margin_bottom=0\n" \
        "capture_margin_left=0\n" \
        "; Sample radius per zone: (2*scan_depth+1)^2 pixels averaged.\n" \
        "; Higher = smoother/less noisy but more CPU per frame.\n" \
        "scan_depth=1\n" \
        "\n" \
        "[color]\n" \
        "; Global brightness scale, 0-255. 255 = no change.\n" \
        "brightness=255\n" \
        "; Must be exactly one of: 1.0 1.4 1.8 2.0 2.2 2.4 2.6 2.8\n" \
        "; (precomputed lookup tables -- no other value is accepted).\n" \
        "gamma=1.0\n" \
        "; -100 (grayscale) to 300 (4x color boost, mostly clipped by\n" \
        "; then -- verified overflow-safe well beyond this, the cap is\n" \
        "; just where it stops looking meaningfully different).\n" \
        "; 0 = unchanged.\n" \
        "saturation=0\n" \
        "; Match your strip's actual wiring. Valid values: RGB, RBG,\n" \
        "; GRB, GBR, BRG, BGR. Most WS2812B/NeoPixel strips are GRB.\n" \
        "color_order=RGB\n" \
        "; Levels adjustment (0-100, percent of the 0-255 range).\n" \
        "; Anything at/below black_level becomes 0; anything at/above\n" \
        "; white_level becomes 255; the rest stretches to fill the gap.\n" \
        "; Defaults (0, 100) are a no-op.\n" \
        "black_level=0\n" \
        "white_level=100\n" \
        "; If a zone's brightest channel drops below this (0-255), that\n" \
        "; zone is forced fully black instead of showing a faint/noisy\n" \
        "; near-black color. Has built-in hysteresis (must rise 10 above\n" \
        "; this value again before turning back on) so it won't flicker\n" \
        "; on scenes hovering right at the threshold. 0 = disabled.\n" \
        "dark_threshold=0\n" \
        "; --- Below this line: ported from the Android version's own\n" \
        "; per-channel color engine. NOTE THE DIFFERENT CONVENTION: these\n" \
        "; use Android's \"100 = unchanged\" percent scale, NOT this file's\n" \
        "; usual \"0 = unchanged\" scale used by saturation/brightness above.\n" \
        "; Contrast: -100..300, 0 = unchanged (stretches/shrinks around\n" \
        "; mid-grey 128, same math as saturation but around brightness\n" \
        "; instead of hue).\n" \
        "contrast=0\n" \
        "; Per-channel brightness, 0-500, 100 = unchanged. Multiplies with\n" \
        "; the single [color] brightness above rather than replacing it.\n" \
        "brightness_r=100\n" \
        "brightness_g=100\n" \
        "brightness_b=100\n" \
        "; Per-channel gamma, 10-500, 100 = unchanged. Unlike the fixed\n" \
        "; gamma= list above (8 preset curves, shared across all 3\n" \
        "; channels), these accept ANY value in range and are independent\n" \
        "; per channel. Applied PER SAMPLE PIXEL before zone-averaging,\n" \
        "; not to the already-averaged zone color -- matches how the\n" \
        "; Android version does it, and avoids a single stray bright\n" \
        "; pixel in an otherwise-dark zone getting averaged in BEFORE\n" \
        "; being gamma-crushed.\n" \
        "gamma_r=100\n" \
        "gamma_g=100\n" \
        "gamma_b=100\n" \
        "\n" \
        "[timing]\n" \
        "; How many times per second to sample and send color.\n" \
        "update_frequency_hz=30\n" \
        "; Blend each new sample with the previous one over roughly\n" \
        "; settling_time_ms, instead of snapping instantly -- reduces\n" \
        "; flicker on fast scene cuts. false = send raw samples as-is.\n" \
        "smoothing_enabled=false\n" \
        "settling_time_ms=200\n" \
        "; How often (seconds) to check this file for changes WHILE\n" \
        "; RUNNING and apply them live -- no need to close/reopen the\n" \
        "; game. 0 = only read this file once, at plugin load (the\n" \
        "; original v2.0 behavior).\n" \
        "config_reload_check_seconds=2\n"

    int32_t f = sceKernelOpen(AMBIENT_CONFIG_PATH, 0x200 | 0x001, 0777);
    if (f < 0) return; // no write access or path issue -- defaults above still apply in memory
    sceKernelWrite(f, AMBIENT_DEFAULT_INI, strlen(AMBIENT_DEFAULT_INI));
    sceKernelClose(f);
    #undef AMBIENT_DEFAULT_INI
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
    return fallback; // unrecognized -- keep default rather than guess
}

static StartCorner parse_start_corner(const char *s, StartCorner fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "bottom_left"))  return CORNER_BOTTOM_LEFT;
    if (!strcmp(s, "bottom_right")) return CORNER_BOTTOM_RIGHT;
    if (!strcmp(s, "top_left"))     return CORNER_TOP_LEFT;
    if (!strcmp(s, "top_right"))    return CORNER_TOP_RIGHT;
    return fallback;
}

static LedDirection parse_direction(const char *s, LedDirection fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "clockwise")) return DIR_CLOCKWISE;
    if (!strcmp(s, "counterclockwise")) return DIR_COUNTERCLOCKWISE;
    return fallback;
}

// Matched by exact string, not parsed as a float -- deliberately, to
// avoid any float-parsing code path at runtime in a build with no
// libm linked. Only these 8 values exist as precomputed LUTs anyway
// (see kGammaLuts below), so a free-form value couldn't be honored
// correctly regardless.
static uint32_t parse_gamma_index(const char *s, uint32_t fallback)
{
    static const char *kGammaStrings[NUM_GAMMA_LUTS] = {"1.0","1.4","1.8","2.0","2.2","2.4","2.6","2.8"};
    if (!s) return fallback;
    for (uint32_t i = 0; i < NUM_GAMMA_LUTS; i++) {
        if (!strcmp(s, kGammaStrings[i])) return i;
    }
    return fallback; // unrecognized string -- keep default rather than guess
}

void ambient_load_config(void)
{
    if (!ambient_file_exists(AMBIENT_CONFIG_PATH)) {
        ambient_create_default_config();
        ambient_rebuild_perchannel_gamma_luts(); // g_gammaLut{R,G,B} are static, zero-init by
                                                  // default -- MUST build the identity LUTs here
                                                  // too, or defaults (gammaR/G/B=100, meant to be
                                                  // a no-op) would silently crush every sample to
                                                  // black instead.
        return; // g_config's compile-time defaults (== v1.3 behavior) stand otherwise
    }

    ini_table_s *table = ini_table_create();
    if (table == NULL || !ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) {
        if (table) ini_table_destroy(table);
        return; // parse failure -- keep defaults rather than run with a half-read config
    }

    const char *v;
    int iv; bool bv;

    if ((v = ini_table_get_entry(table, "network", "wled_host")) != NULL) {
        strncpy(g_config.wledHost, v, sizeof(g_config.wledHost) - 1);
        g_config.wledHost[sizeof(g_config.wledHost) - 1] = '\0';
    }
    if (ini_table_get_entry_as_int(table, "network", "wled_port", &iv) && iv > 0 && iv <= 65535)
        g_config.wledPort = (uint16_t)iv;

    if ((v = ini_table_get_entry(table, "network", "relay_host")) != NULL) {
        strncpy(g_config.relayHost, v, sizeof(g_config.relayHost) - 1);
        g_config.relayHost[sizeof(g_config.relayHost) - 1] = '\0';
    }
    if (ini_table_get_entry_as_int(table, "network", "relay_port", &iv) && iv > 0 && iv <= 65535)
        g_config.relayPort = (uint16_t)iv;
    if (ini_table_get_entry_as_bool(table, "network", "relay_signal_enabled", &bv))
        g_config.relaySignalEnabled = bv;

    if (ini_table_get_entry_as_int(table, "layout", "led_count_top", &iv) && iv > 0)
        g_config.ledCountTop = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_right", &iv) && iv > 0)
        g_config.ledCountRight = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_bottom", &iv) && iv > 0)
        g_config.ledCountBottom = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_left", &iv) && iv > 0)
        g_config.ledCountLeft = (uint32_t)iv;
    g_config.startCorner = parse_start_corner(ini_table_get_entry(table, "layout", "led_start_corner"), g_config.startCorner);
    g_config.direction = parse_direction(ini_table_get_entry(table, "layout", "led_direction"), g_config.direction);
    if (ini_table_get_entry_as_int(table, "layout", "led_offset", &iv))
        g_config.ledOffset = iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_top", &iv) && iv >= 0)
        g_config.marginTop = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_right", &iv) && iv >= 0)
        g_config.marginRight = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_bottom", &iv) && iv >= 0)
        g_config.marginBottom = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_left", &iv) && iv >= 0)
        g_config.marginLeft = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "scan_depth", &iv) && iv >= 0)
        g_config.scanDepth = (uint32_t)iv;

    if (ini_table_get_entry_as_int(table, "color", "brightness", &iv) && iv >= 0 && iv <= 255)
        g_config.brightness = (uint32_t)iv;
    g_config.gammaLutIndex = parse_gamma_index(ini_table_get_entry(table, "color", "gamma"), g_config.gammaLutIndex);
    if (ini_table_get_entry_as_int(table, "color", "saturation", &iv) && iv >= -100 && iv <= 300)
        g_config.saturation = iv;
    g_config.colorOrder = parse_color_order(ini_table_get_entry(table, "color", "color_order"), g_config.colorOrder);
    if (ini_table_get_entry_as_int(table, "color", "black_level", &iv) && iv >= 0 && iv <= 100)
        g_config.blackLevel = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "white_level", &iv) && iv >= 0 && iv <= 100)
        g_config.whiteLevel = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "dark_threshold", &iv) && iv >= 0 && iv <= 255)
        g_config.darkThreshold = (uint32_t)iv;

    // v2.2: Android-ported per-channel color knobs (see AmbientConfig
    // struct comment for why these use a different 0-neutral-point
    // convention than the fields just above).
    if (ini_table_get_entry_as_int(table, "color", "contrast", &iv) && iv >= -100 && iv <= 300)
        g_config.contrast = iv;
    if (ini_table_get_entry_as_int(table, "color", "brightness_r", &iv) && iv >= 0 && iv <= 500)
        g_config.brightnessR = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "brightness_g", &iv) && iv >= 0 && iv <= 500)
        g_config.brightnessG = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "brightness_b", &iv) && iv >= 0 && iv <= 500)
        g_config.brightnessB = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "gamma_r", &iv) && iv >= 10 && iv <= 500)
        g_config.gammaR = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "gamma_g", &iv) && iv >= 10 && iv <= 500)
        g_config.gammaG = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "gamma_b", &iv) && iv >= 10 && iv <= 500)
        g_config.gammaB = (uint32_t)iv;
    ambient_rebuild_perchannel_gamma_luts();

    // v2.2.5: [dev] is intentionally undocumented in the generated ini
    // template (AMBIENT_DEFAULT_INI) and absent from both reference
    // configs in this folder -- someone has to already know to type
    // "[dev]" / "dev_ip=..." / "dev_logging=true" into their own ini
    // by hand. That's deliberate: see the struct comment on why this
    // shouldn't be one flag away from accidentally-on for a normal
    // user. Both keys are required together -- a stray dev_ip with no
    // dev_logging=true does nothing, and vice versa (checked again,
    // redundantly, right at the one place that actually sends
    // anything -- see debug_send_raw).
    if ((v = ini_table_get_entry(table, "dev", "dev_ip")) != NULL) {
        strncpy(g_config.devIp, v, sizeof(g_config.devIp) - 1);
        g_config.devIp[sizeof(g_config.devIp) - 1] = '\0';
    }
    if (ini_table_get_entry_as_bool(table, "dev", "dev_logging", &bv))
        g_config.devLoggingEnabled = bv;

    if (ini_table_get_entry_as_int(table, "timing", "update_frequency_hz", &iv) && iv > 0 && iv <= 240)
        g_config.updateFrequencyHz = (uint32_t)iv;
    if (ini_table_get_entry_as_bool(table, "timing", "smoothing_enabled", &bv))
        g_config.smoothingEnabled = bv ? 1 : 0;
    if (ini_table_get_entry_as_int(table, "timing", "settling_time_ms", &iv) && iv >= 0)
        g_config.settlingTimeMs = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "timing", "config_reload_check_seconds", &iv) && iv >= 0)
        g_config.configReloadCheckSeconds = (uint32_t)iv;

    ini_table_destroy(table);
}

// v2.1 live config reload: re-reads the ini file if its mtime changed,
// checked at most once every configReloadCheckSeconds (0 = never,
// matches v2.0's load-once behavior). Safe without any locking because
// g_config/g_zoneX/g_zoneY/g_numZones are read AND written exclusively
// by this one thread -- plugin_load's initial ambient_load_config()/
// buildZoneGeometry() call happens before this thread is even spawned,
// and the flip hook only ever touches g_currentDisplayBufferIndex, not
// any of this state (handoff §45).
// v2.1.1 BUGFIX (real hardware confirmed live reload never fired --
// see handoff §46): the original version used g_configLastMtime==0 as
// its own "not yet initialized" sentinel. That's a real, fatal design
// flaw independent of any PS4-specific quirk: 0 is also a value
// st_mtime could legitimately have (unpopulated/unsupported on this
// filesystem, or just a coincidence). If st_mtime is ever actually 0,
// this check would conclude "still establishing baseline" FOREVER,
// on every single call, and reload would silently never fire again --
// exactly the symptom reported. Fixed with a separate boolean flag so
// the sentinel state can never collide with a real mtime value.
//
// Also now tracks file SIZE alongside mtime and reloads if EITHER
// changed -- not because mtime is confirmed broken on this platform
// (not verified from here), but because this is cheap insurance
// against exactly that class of platform-level uncertainty, and this
// project has already spent one whole session finding out an
// assumption about low-level platform behavior was wrong (§17-22).
// v2.1.4: real hardware (see handoff §47/§48) showed st_size is just as
// broken as st_mtime on this filesystem -- stuck reporting a fixed
// wrong value (8) for the entire life of the process, never once
// reflecting the real file's actual size even while it was being
// actively edited. Both fields come off the same stat() call, so
// "st_mtime is broken, but surely st_size is fine" was never a safe
// assumption -- it just hadn't been checked directly yet. What IS
// proven reliable on this platform is a raw sceKernelOpen +
// sceKernelLseek(SEEK_END) + sceKernelClose read: that's exactly how
// the initial plugin-load config read (and the v2.1.3 content preview)
// already get real, live data, and both are confirmed correct against
// this exact path by the fact edits DO take effect after a game
// restart. So the reload check now sources its size signal from that
// same syscall path instead of patching around stat() a third time.
// Existence is still checked via stat() in ambient_file_exists() --
// that's a boolean return code, not a numeric field, and nothing here
// suggests that part is unreliable.
// v2.1.5: content hash, added because size alone can't see a
// same-length edit (confirmed on real hardware -- an RGB->RBG swap
// left cur_size/last_size both correctly reading a stable, real 2809
// but never differing, so event stayed 3/"unchanged" forever). FNV-1a
// is used purely as a cheap, dependency-free change signal, not for
// any security property -- no libm, no external library, consistent
// with the existing no-libm constraint (§ PQ/sRGB LUT work).
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

// Sanity cap on the hash read, independent of the earlier no-libm
// constraint -- this is about not handing malloc() a garbage size if
// AMBIENT_CONFIG_PATH ever resolves to something unexpected. The real
// file is a few KB at most (even a heavily-commented ini); 1 MiB is
// generous headroom, not a real expected size.
#define AMBIENT_CONFIG_MAX_HASH_READ (1024 * 1024)

// Replaces v2.1.4's ambient_get_real_config_size: still gets size the
// same proven way (sceKernelOpen + sceKernelLseek(SEEK_END)), but now
// also seeks back to the start and reads the whole file through the
// same fd to compute a content hash in the same call, rather than
// opening the file twice per check. Both outputs share one success/
// failure result since they come from the same read; on any failure
// *outSize carries the negative orbis error code (or a sentinel below)
// same as v2.1.4's function did, and *outHash is left untouched.
static bool ambient_get_real_config_size_and_hash(int64_t *outSize, uint32_t *outHash)
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

    // sz fits in AMBIENT_CONFIG_MAX_HASH_READ (checked above), so this
    // is a small, bounded allocation, not proportional to whatever
    // AMBIENT_CONFIG_PATH happens to resolve to.
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
    free(buf);
    return true;
}

// mtime is kept in the struct/packet purely for backward-compatible
// layout with the v2.1.2/v2.1.3 captures already on file -- it is
// HARDCODED to 0 as of v2.1.4 and no longer read from stat() or used
// for change-detection at all (confirmed non-functional on this
// filesystem, see handoff §46/§47). Size, sourced from
// ambient_get_real_config_size_and_hash above, is one of two change
// signals as of v2.1.5 -- see g_configLastHash below for the other,
// added because size alone missed a real same-length edit on hardware.
static bool g_haveConfigBaseline = false;
static off_t g_configLastSize = 0;
// v2.1.5: content hash (FNV-1a/32) of the whole file, alongside size.
// Needed because a same-length edit (confirmed on hardware: RGB->RBG)
// changes the file's content without changing its byte count, which
// size alone has no way to detect. Reload now fires if EITHER size or
// hash differs from last check.
static uint32_t g_configLastHash = 0;
// Increments on every call, independent of what the call finds -- see
// send_config_reload_debug_packet's check_count field above for why.
static uint32_t g_configCheckCount = 0;

void ambient_check_config_reload(void)
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
    if (!ambient_get_real_config_size_and_hash(&realSize, &realHash)) {
        uint8_t preview[CONFIG_DEBUG_PREVIEW_LEN];
        ambient_read_content_preview(preview, sizeof(preview));
        send_config_reload_debug_packet(1, (uint32_t)(-realSize), 0, 0,
                                         0, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         0, g_configLastHash);
        return; // file gone/unreadable/unexpectedly huge -- keep running on current config
    }

    uint8_t preview[CONFIG_DEBUG_PREVIEW_LEN];
    ambient_read_content_preview(preview, sizeof(preview));

    if (!g_haveConfigBaseline) {
        send_config_reload_debug_packet(2, 0, 0, 0,
                                         (uint64_t)realSize, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         realHash, g_configLastHash);
        g_configLastSize = (off_t)realSize;
        g_configLastHash = realHash;
        g_haveConfigBaseline = true;
        return; // first check just establishes a baseline, nothing to compare against yet
    }
    if ((off_t)realSize == g_configLastSize && realHash == g_configLastHash) {
        send_config_reload_debug_packet(3, 0, 0, 0,
                                         (uint64_t)realSize, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         realHash, g_configLastHash);
        return; // unchanged -- same size AND same content hash
    }

    send_config_reload_debug_packet(4, 0, 0, 0,
                                     (uint64_t)realSize, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                     realHash, g_configLastHash);

    g_configLastSize = (off_t)realSize;
    g_configLastHash = realHash;
    // v2.4.1: relay_send_external_source(true) was previously only
    // ever called once, at the very end of a successful plugin_load --
    // if relay_signal_enabled was still false at that moment (e.g. the
    // game was already running when the ini was edited to turn it on),
    // nothing would ever re-send "on" for the rest of that session,
    // even though g_config.relaySignalEnabled below picks up the new
    // value correctly. Capturing the value BEFORE reload and comparing
    // after closes that gap: a live false->true edit now sends "on"
    // immediately (matching plugin_load's own real transition), and a
    // live true->false edit sends "off" once, so wled-relay isn't left
    // waiting on a flag this plugin will now never touch again.
    // Skipped while backgrounded -- matches ambient_sample_thread's own
    // condition for what "on" means, so this can't contradict whatever
    // that loop decides once it resumes.
    // v2.6: the true->false direction described above didn't actually
    // work until this version -- relay_send_external_source() used to
    // gate itself on g_config.relaySignalEnabled internally, which by
    // this point is already false (that's what triggered this call),
    // so the "off" send this call exists to make was being silently
    // swallowed. That internal gate is gone now (see that function's
    // own comment) -- this call site's own "only fires on a real
    // transition" condition is what makes it safe for a
    // never-opted-in user, not a second gate inside the function.
    bool wasRelaySignalEnabled = g_config.relaySignalEnabled;
    ambient_load_config();  // re-reads the file; unset/removed keys keep their CURRENT g_config value, not the compiled default (see note below)
    if (g_config.relaySignalEnabled != wasRelaySignalEnabled && !g_isBackgrounded) {
        relay_send_external_source(g_config.relaySignalEnabled);
    }
    buildZoneGeometry();    // layout settings may have changed -- rebuild g_zoneX/g_zoneY/g_numZones
    g_smoothedRgbValid = false; // avoid smoothing a hard cut between old and new zone counts/positions
}

