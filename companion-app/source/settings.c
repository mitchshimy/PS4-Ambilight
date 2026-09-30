// settings.c -- ini schema and defaults copied verbatim from
// ps4_ambient_light v2.2's own g_config initializer and
// ambient_load_config's ini_table_get_entry* calls. Every (section,
// key) pair below was checked against that file directly, not
// recalled from memory. If the plugin's schema changes, re-sync this
// table from its main.c.

#include "settings.h"
#include "config.h"
#include "presets.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *kStartCornerNames[] = { "bottom_left", "bottom_right", "top_left", "top_right" };
static const char *kDirectionNames[]   = { "clockwise", "counterclockwise" };
static const char *kColorOrderNames[]  = { "RGB", "RBG", "GRB", "GBR", "BRG", "BGR" };
const char *kGammaNames[8] = { "1.0", "1.4", "1.8", "2.0", "2.2", "2.4", "2.6", "2.8" };
// Lives here, not in presets.c, because the Preset row in kMenuItems points at it:
// anything that links settings.c (the isolation tests, mostly) must not also need presets.c.
const char *kPresetNames[PRESET_COUNT] = { "Game", "Movie" };

// v2.9: smoothing presets, ported from the Android "inspiration"
// project's own ColorSmoothing.kt (applyPreset) -- the ms values are
// copied verbatim from its "off"/"responsive"/"balanced"/"smooth"
// cases (that project's other two per-preset knobs, output delay and a
// forced update-frequency override, don't have an equivalent here:
// this plugin has no output-delay concept, and update_frequency_hz is
// this project's own independent CPU-budget setting that a smoothing
// preset has no business silently overriding). This is a UI
// convenience over the two REAL fields below, not a third setting of
// its own -- nothing here is a new ini key, so kMenuItems' own
// "every (section,key) pair is exactly what the real plugin reads"
// invariant for its actual entries is untouched. See its own kMenuItems
// row (section "ui", not a real ini section) and smoothing_preset_index/
// smoothing_apply_preset below.
const char *kSmoothingPresetNames[SMOOTHING_PRESET_COUNT] = { "Off", "Responsive", "Balanced", "Smooth" };
static const uint32_t kSmoothingPresetMs[SMOOTHING_PRESET_COUNT] = { 50, 50, 200, 500 };

// Bucketed rather than an exact-value match against kSmoothingPresetMs:
// this always has a defined answer (no "Custom" case to render/cycle
// out of), including for a settling_time_ms a user typed by hand via
// the raw field rather than picking a preset. Off (index 0) is decided
// by smoothingEnabled alone -- matches the Android original, where
// "off" and "responsive" share the exact same 50ms settling time and
// are distinguished only by whether smoothing is enabled at all.
int smoothing_preset_index(const AmbientConfig *cfg)
{
    if (!cfg->smoothingEnabled) return 0;
    if (cfg->settlingTimeMs <= kSmoothingPresetMs[1]) return 1;
    if (cfg->settlingTimeMs <= kSmoothingPresetMs[2]) return 2;
    return 3;
}

void smoothing_apply_preset(AmbientConfig *cfg, int presetIndex)
{
    if (presetIndex < 0) presetIndex = 0;
    if (presetIndex >= SMOOTHING_PRESET_COUNT) presetIndex = SMOOTHING_PRESET_COUNT - 1;
    cfg->smoothingEnabled = (presetIndex != 0) ? 1 : 0;
    cfg->settlingTimeMs = kSmoothingPresetMs[presetIndex];
}

#define OFF(field) offsetof(AmbientConfig, field)
// For FIELD_STRING items only -- see settings.h's comment on MenuItem's
// max field for why this reuses it instead of adding a new one.
#define STRBUF(field) (int32_t)sizeof(((AmbientConfig *)0)->field)

// Grouped and ordered to match the blueprint's own card layout
// (setup.html / customisation.html), not the plugin's ini section
// order -- MENU_SCREEN_SETUP items first, then MENU_SCREEN_CUSTOMIZE,
// each screen's items one contiguous run so the UI can group by a run
// of matching `group` strings into one card. `label` is now the exact
// on-screen text, since ui_screens.c builds every settings card
// directly from this table -- there is no second, hand-written copy of
// these strings to keep in sync. Percent-style units are not part of
// the label: they come from the trailing DisplayUnit, so the number
// shown can differ from the stored one (see settings.h).
//
// This reshuffles TWO fields from where the plugin's own ini
// sections would naturally put them, because the blueprint's cards
// don't follow section boundaries:
//  - color_order sits on Set Up's "LED strip layout" card (it's a
//    pill next to Direction there), not on Customize's "Colour" card.
//  - config_reload_check_seconds gets its own "Live reload" card on
//    Set Up, not folded into Customize's "Motion and darkness".
// Every (section, key, type, min, max) tuple is still exactly what the
// real plugin reads -- only screen/group/label/order changed. `step`
// is too, except on UNIT_PCT_OF_MAX rows (brightness and the two
// thresholds), where it's in the percent the person sees.
//
// A few ini keys have no row here on purpose (the two auto letterbox
// debounce timings). settings_load/settings_save still handle them
// directly; see the note where they used to sit.
const MenuItem kMenuItems[] = {
    // =========================== Set Up ===========================
    { "WLED IPv4",         "network", "wled_host",                  FIELD_STRING, OFF(wledHost),        0, STRBUF(wledHost), 0, NULL, 0,
      MENU_SCREEN_SETUP, "WLED connection", "Enter your WLED controller's IPv4 address. DDP uses UDP port 4048 by default." },
    { "UDP port",          "network", "wled_port",                  FIELD_U16,    OFF(wledPort),        1, 65535,   1, NULL, 0,
      MENU_SCREEN_SETUP, "WLED connection", NULL },
    { "Output FPS",        "timing",  "update_frequency_hz",        FIELD_U32,    OFF(updateFrequencyHz), 1,  240,   1, NULL, 0,
      MENU_SCREEN_SETUP, "WLED connection", NULL },

    { "Left LEDs",         "layout",  "led_count_left",             FIELD_U32,    OFF(ledCountLeft),    1,   500,   1, NULL, 0,
      MENU_SCREEN_SETUP, "LED strip layout", "Enter the physical LED count on each edge, then match the strip start and direction. If the strip starts between corners, a positive offset moves colours toward higher LED numbers and a negative offset moves them lower." },
    { "Top LEDs",          "layout",  "led_count_top",              FIELD_U32,    OFF(ledCountTop),     1,   500,   1, NULL, 0,
      MENU_SCREEN_SETUP, "LED strip layout", NULL },
    { "Right LEDs",        "layout",  "led_count_right",            FIELD_U32,    OFF(ledCountRight),   1,   500,   1, NULL, 0,
      MENU_SCREEN_SETUP, "LED strip layout", NULL },
    { "Bottom LEDs",       "layout",  "led_count_bottom",           FIELD_U32,    OFF(ledCountBottom),  1,   500,   1, NULL, 0,
      MENU_SCREEN_SETUP, "LED strip layout", NULL },
    { "LED offset",        "layout",  "led_offset",                 FIELD_I32,    OFF(ledOffset),   -1000,  1000,   1, NULL, 0,
      MENU_SCREEN_SETUP, "LED strip layout", NULL },
    { "Start corner",      "layout",  "led_start_corner",           FIELD_ENUM,   OFF(startCorner),     0,     3,   1, kStartCornerNames, 4,
      MENU_SCREEN_SETUP, "LED strip layout", NULL },
    { "Direction",         "layout",  "led_direction",              FIELD_ENUM,   OFF(direction),       0,     1,   1, kDirectionNames, 2,
      MENU_SCREEN_SETUP, "LED strip layout", NULL },
    { "Colour order",      "color",   "color_order",                FIELD_ENUM,   OFF(colorOrder),      0,     5,   1, kColorOrderNames, 6,
      MENU_SCREEN_SETUP, "LED strip layout", NULL },

    { "Reload check (seconds)", "timing", "config_reload_check_seconds", FIELD_U32, OFF(configReloadCheckSeconds), 0, 60, 1, NULL, 0,
      MENU_SCREEN_SETUP, "Live reload", "How often the plugin checks this file for changes while it's running, so updates apply without closing the game. Set to 0 to only read the file once, at load." },

    // ======================== Customize ========================
    // Capped at 4, not the plugin's old ceiling of 10 -- each step here
    // is (2*scan_depth+1)^2 real tiled-memory reads PER ZONE, PER
    // Preset selector + reset button. Both are section "ui": neither is an ini key,
    // and their offsets are dummies (same trick as smoothing_preset below) -- presets.c
    // owns the real state, main.c's nudge_field / input code dispatch on `key`, and
    // presets.c's own field walk skips every "ui" row.
    { "Preset",   "ui", "preset",       FIELD_ENUM,   OFF(smoothingEnabled), 0, PRESET_COUNT - 1, 1, kPresetNames, PRESET_COUNT,
      MENU_SCREEN_CUSTOMIZE, "Preset", "Game and Movie each keep their own settings. Everything below edits the preset shown here, so switching never overwrites the other one." },
    { "Defaults", "ui", "preset_reset", FIELD_ACTION, OFF(smoothingEnabled), 0, 0, 1, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Preset", NULL },

    // SAMPLE-THREAD PASS (see letterbox.c's own probeNonBlack comment
    // on why a tiled read isn't a free array index). 4 still gives a
    // clearly smoother average than the default of 1 without the
    // worst-case cost climbing anywhere near what 10 allowed. The
    // plugin's own ini loader enforces this same ceiling independently
    // (SCAN_DEPTH_MAX in ambient_internal.h), so a hand-edited ini
    // can't bypass it even if this slider's max ever drifts from that.
    { "Edge depth",        "layout",  "scan_depth",                 FIELD_U32,    OFF(scanDepth),       0,     4,   1, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Screen sampling", "Each edge zone averages a square block of pixels around its sample point." },

    { "Auto letterbox",       "layout", "auto_letterbox_enabled",   FIELD_BOOL,   OFF(autoLetterboxEnabled), 0, 1, 1, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Auto letterbox", "Detects black bars on each edge independently and insets sampling to stay off them. This is the only capture inset -- turn it off to sample every pixel unconditionally." },
    { "Bar threshold", "layout", "auto_letterbox_threshold", FIELD_U32, OFF(autoLetterboxThreshold), 0, 255, 1, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Auto letterbox", NULL, UNIT_PCT_OF_MAX },
    // "Stability (frames)" and "Recheck every (frames)" (auto_letterbox_
    // stability_frames / auto_letterbox_check_interval_frames) are also
    // dropped from the UI -- these are the debounce timing constants
    // tuned alongside the depth-bounded, majority-vote detection scan
    // (see the plugin's own letterbox.c). A user turning "Stability"
    // down to react faster is exactly how the flicker that setting
    // exists to prevent comes back. The "Auto letterbox" card's row
    // layout ({4} -> {2}) is updated to match in kGroupLayouts.

    { "Brightness",        "color", "brightness",  FIELD_U32,  OFF(brightness),      0,   255,   5, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Colour", "Shape overall brightness, colour intensity, gamma response and the black/white levels used for LED output.", UNIT_PCT_OF_MAX },
    { "Saturation", "color", "saturation", FIELD_I32, OFF(saturation), -100, 100, 5, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Colour", NULL, UNIT_PCT_SIGNED },
    { "Gamma",              "color",   "gamma",                     FIELD_U32,    OFF(gammaLutIndex),   0,     7,   1, NULL, 0, // display maps 0-7 -> "1.0".."2.8", see format_item_value()
      MENU_SCREEN_CUSTOMIZE, "Colour", NULL },
    { "Black level",      "color",   "black_level",               FIELD_U32,    OFF(blackLevel),      0,   100,   1, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Colour", NULL, UNIT_PCT_DIRECT },
    { "White level",      "color",   "white_level",               FIELD_U32,    OFF(whiteLevel),      0,   100,   1, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Colour", NULL, UNIT_PCT_DIRECT },
    { "Contrast",   "color", "contrast", FIELD_I32, OFF(contrast), -100, 100, 5, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Colour", NULL, UNIT_PCT_SIGNED },

    { "Smoothing",          "timing",  "smoothing_enabled",         FIELD_BOOL,   OFF(smoothingEnabled),  0,    1,   1, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Motion and darkness", "Control transition smoothing and how the strip reacts to very dark scenes." },
    { "Smoothing preset",   "ui",      "smoothing_preset",          FIELD_ENUM,   OFF(smoothingEnabled), 0, SMOOTHING_PRESET_COUNT - 1, 1, kSmoothingPresetNames, SMOOTHING_PRESET_COUNT,
      MENU_SCREEN_CUSTOMIZE, "Motion and darkness", NULL }, // "ui" section/not-a-real-offset are deliberate -- see settings.h; nudge_field/format_value special-case this key instead of using it generically
    { "Settling time (ms)", "timing",  "settling_time_ms",          FIELD_U32,    OFF(settlingTimeMs),    0, 5000,  50, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Motion and darkness", NULL },
    { "Black threshold", "color", "dark_threshold", FIELD_U32, OFF(darkThreshold), 0, 255, 1, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "Motion and darkness", NULL, UNIT_PCT_OF_MAX },

    { "Red balance",      "color",   "brightness_r",              FIELD_U32,    OFF(brightnessR),     0,   500,   5, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "RGB balance", "Calibrate per-channel brightness and gamma to match your television and wall colour.", UNIT_PCT_DIRECT },
    { "Green balance",    "color",   "brightness_g",              FIELD_U32,    OFF(brightnessG),     0,   500,   5, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "RGB balance", NULL, UNIT_PCT_DIRECT },
    { "Blue balance",     "color",   "brightness_b",              FIELD_U32,    OFF(brightnessB),     0,   500,   5, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "RGB balance", NULL, UNIT_PCT_DIRECT },
    { "Gamma R",            "color",   "gamma_r",                   FIELD_U32,    OFF(gammaR),         10,   500,   5, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "RGB balance", NULL, UNIT_PCT_DIRECT },
    { "Gamma G",            "color",   "gamma_g",                   FIELD_U32,    OFF(gammaG),         10,   500,   5, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "RGB balance", NULL, UNIT_PCT_DIRECT },
    { "Gamma B",            "color",   "gamma_b",                   FIELD_U32,    OFF(gammaB),         10,   500,   5, NULL, 0,
      MENU_SCREEN_CUSTOMIZE, "RGB balance", NULL, UNIT_PCT_DIRECT },
};
const int kMenuItemCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);

// Copied verbatim from ps4_ambient_light's own g_config initializer
// (main.c) -- see that file if this ever needs re-syncing.
void settings_set_defaults(AmbientConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    // wledHost intentionally left blank (not pre-filled with a real
    // address) -- matches ps4_ambient_light's own compiled default.
    // Leaves the Setup screen's WLED Host field empty until a user
    // fills in their own controller's IP.
    cfg->wledPort = 4048;
    cfg->ledCountTop = 73; cfg->ledCountRight = 41; cfg->ledCountBottom = 73; cfg->ledCountLeft = 42;
    cfg->startCorner = CORNER_BOTTOM_LEFT;
    cfg->direction = DIR_CLOCKWISE;
    cfg->ledOffset = 0;
    cfg->scanDepth = 1;
    cfg->autoLetterboxEnabled = 1;
    cfg->autoLetterboxThreshold = 18;
    cfg->autoLetterboxStabilityFrames = 3;
    cfg->autoLetterboxCheckIntervalFrames = 15;
    cfg->brightness = 255;
    cfg->gammaLutIndex = 0;
    cfg->saturation = 0;
    cfg->colorOrder = ORDER_RGB;
    cfg->blackLevel = 0;
    cfg->whiteLevel = 100;
    cfg->darkThreshold = 0;
    cfg->contrast = 0;
    cfg->brightnessR = 100; cfg->brightnessG = 100; cfg->brightnessB = 100;
    cfg->gammaR = 100; cfg->gammaG = 100; cfg->gammaB = 100;
    cfg->updateFrequencyHz = 30;
    cfg->smoothingEnabled = 0;
    cfg->settlingTimeMs = 200;
    cfg->configReloadCheckSeconds = 2;
    // relayHost intentionally left blank (memset above) and
    // relaySignalEnabled defaults off, same reasoning as wledHost
    // above -- this app shouldn't ship pointed at this project's own
    // relay setup. relayPort's default matches ps4_ambient_light's
    // own compiled default (24689) since, unlike a host address,
    // there's nothing personal about a port number.
    cfg->relayPort = 24689;
}

static StartCorner parse_start_corner(const char *s, StartCorner fallback)
{
    if (!s) return fallback;
    for (int i = 0; i < 4; i++) if (!strcmp(s, kStartCornerNames[i])) return (StartCorner)i;
    return fallback;
}
static LedDirection parse_direction(const char *s, LedDirection fallback)
{
    if (!s) return fallback;
    for (int i = 0; i < 2; i++) if (!strcmp(s, kDirectionNames[i])) return (LedDirection)i;
    return fallback;
}
static ColorOrder parse_color_order(const char *s, ColorOrder fallback)
{
    if (!s) return fallback;
    for (int i = 0; i < 6; i++) if (!strcmp(s, kColorOrderNames[i])) return (ColorOrder)i;
    return fallback;
}

// Clamps value into the [min,max] that kMenuItems declares for
// (section,key) -- the exact same bounds settings_set_i32() already
// enforces for every D-Pad-driven edit. settings_load() previously
// re-implemented range checks by hand per field, and several fields
// only got a partial check (e.g. LED counts/margins accepted any
// value >= their lower bound with no upper cap at all, led_offset had
// no bound check whatsoever) -- a hand-edited, corrupted, or future-
// schema ini could load values the UI itself would never let you set.
// Routing every numeric field through the schema's own min/max here
// means there's exactly one place bounds are defined, and it can't
// silently drift out of sync the way hand-copied literals could.
static int32_t clamp_to_schema(const char *section, const char *key, int32_t value)
{
    for (int i = 0; i < kMenuItemCount; i++) {
        if (strcmp(kMenuItems[i].section, section) == 0 && strcmp(kMenuItems[i].key, key) == 0) {
            if (value < kMenuItems[i].min) value = kMenuItems[i].min;
            if (value > kMenuItems[i].max) value = kMenuItems[i].max;
            break;
        }
    }
    return value;
}

// These two letterbox debounce keys have no row in kMenuItems any more (they're
// hidden from the UI), so clamp_to_schema() can't find bounds for them and
// would pass any value straight through. Same limits the rows used to carry.
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

bool settings_load(AmbientConfig *cfg, const char *path)
{
    settings_set_defaults(cfg);

    ini_table_s *table = ini_table_create();
    if (table == NULL) return false;
    if (!ini_table_read_from_file(table, path)) {
        ini_table_destroy(table);
        return false;
    }

    const char *v; int iv; bool bv;

    if ((v = ini_table_get_entry(table, "network", "wled_host")) != NULL) {
        strncpy(cfg->wledHost, v, sizeof(cfg->wledHost) - 1);
        cfg->wledHost[sizeof(cfg->wledHost) - 1] = '\0';
    }
    if (ini_table_get_entry_as_int(table, "network", "wled_port", &iv))
        cfg->wledPort = (uint16_t)clamp_to_schema("network", "wled_port", iv);

    // Reintroduced (was lost between v17 and v18 -- see color_pipeline.h's
    // struct comment). Deliberately NOT in kMenuItems, so no
    // clamp_to_schema() call here -- there's no schema entry for these
    // three to look up, same as v17 had it.
    if ((v = ini_table_get_entry(table, "network", "relay_host")) != NULL) {
        strncpy(cfg->relayHost, v, sizeof(cfg->relayHost) - 1);
        cfg->relayHost[sizeof(cfg->relayHost) - 1] = '\0';
    }
    if (ini_table_get_entry_as_int(table, "network", "relay_port", &iv))
        cfg->relayPort = (uint16_t)iv;
    if (ini_table_get_entry_as_bool(table, "network", "relay_signal_enabled", &bv))
        cfg->relaySignalEnabled = bv ? 1 : 0;

    if (ini_table_get_entry_as_int(table, "layout", "led_count_top", &iv)) cfg->ledCountTop = (uint32_t)clamp_to_schema("layout", "led_count_top", iv);
    if (ini_table_get_entry_as_int(table, "layout", "led_count_right", &iv)) cfg->ledCountRight = (uint32_t)clamp_to_schema("layout", "led_count_right", iv);
    if (ini_table_get_entry_as_int(table, "layout", "led_count_bottom", &iv)) cfg->ledCountBottom = (uint32_t)clamp_to_schema("layout", "led_count_bottom", iv);
    if (ini_table_get_entry_as_int(table, "layout", "led_count_left", &iv)) cfg->ledCountLeft = (uint32_t)clamp_to_schema("layout", "led_count_left", iv);
    cfg->startCorner = parse_start_corner(ini_table_get_entry(table, "layout", "led_start_corner"), cfg->startCorner);
    cfg->direction = parse_direction(ini_table_get_entry(table, "layout", "led_direction"), cfg->direction);
    if (ini_table_get_entry_as_int(table, "layout", "led_offset", &iv)) cfg->ledOffset = clamp_to_schema("layout", "led_offset", iv);
    if (ini_table_get_entry_as_int(table, "layout", "scan_depth", &iv)) cfg->scanDepth = (uint32_t)clamp_to_schema("layout", "scan_depth", iv);
    if (ini_table_get_entry_as_bool(table, "layout", "auto_letterbox_enabled", &bv)) cfg->autoLetterboxEnabled = bv ? 1 : 0;
    if (ini_table_get_entry_as_int(table, "layout", "auto_letterbox_threshold", &iv)) cfg->autoLetterboxThreshold = (uint32_t)clamp_to_schema("layout", "auto_letterbox_threshold", iv);
    if (ini_table_get_entry_as_int(table, "layout", "auto_letterbox_stability_frames", &iv)) cfg->autoLetterboxStabilityFrames = (uint32_t)clamp_i32(iv, 1, 30);
    if (ini_table_get_entry_as_int(table, "layout", "auto_letterbox_check_interval_frames", &iv)) cfg->autoLetterboxCheckIntervalFrames = (uint32_t)clamp_i32(iv, 1, 300);

    if (ini_table_get_entry_as_int(table, "color", "brightness", &iv)) cfg->brightness = (uint32_t)clamp_to_schema("color", "brightness", iv);
    // gamma preset is stored as a string ("1.0".."2.8") in the real ini,
    // not an index -- match that exactly rather than inventing our own
    // representation, so a file this app writes still loads correctly
    // in the real plugin.
    if ((v = ini_table_get_entry(table, "color", "gamma")) != NULL) {
        for (int i = 0; i < 8; i++) if (!strcmp(v, kGammaNames[i])) { cfg->gammaLutIndex = (uint32_t)i; break; }
    }
    if (ini_table_get_entry_as_int(table, "color", "saturation", &iv)) cfg->saturation = clamp_to_schema("color", "saturation", iv);
    cfg->colorOrder = parse_color_order(ini_table_get_entry(table, "color", "color_order"), cfg->colorOrder);
    if (ini_table_get_entry_as_int(table, "color", "black_level", &iv)) cfg->blackLevel = (uint32_t)clamp_to_schema("color", "black_level", iv);
    if (ini_table_get_entry_as_int(table, "color", "white_level", &iv)) cfg->whiteLevel = (uint32_t)clamp_to_schema("color", "white_level", iv);
    if (ini_table_get_entry_as_int(table, "color", "dark_threshold", &iv)) cfg->darkThreshold = (uint32_t)clamp_to_schema("color", "dark_threshold", iv);
    if (ini_table_get_entry_as_int(table, "color", "contrast", &iv)) cfg->contrast = clamp_to_schema("color", "contrast", iv);
    if (ini_table_get_entry_as_int(table, "color", "brightness_r", &iv)) cfg->brightnessR = (uint32_t)clamp_to_schema("color", "brightness_r", iv);
    if (ini_table_get_entry_as_int(table, "color", "brightness_g", &iv)) cfg->brightnessG = (uint32_t)clamp_to_schema("color", "brightness_g", iv);
    if (ini_table_get_entry_as_int(table, "color", "brightness_b", &iv)) cfg->brightnessB = (uint32_t)clamp_to_schema("color", "brightness_b", iv);
    if (ini_table_get_entry_as_int(table, "color", "gamma_r", &iv)) cfg->gammaR = (uint32_t)clamp_to_schema("color", "gamma_r", iv);
    if (ini_table_get_entry_as_int(table, "color", "gamma_g", &iv)) cfg->gammaG = (uint32_t)clamp_to_schema("color", "gamma_g", iv);
    if (ini_table_get_entry_as_int(table, "color", "gamma_b", &iv)) cfg->gammaB = (uint32_t)clamp_to_schema("color", "gamma_b", iv);

    if (ini_table_get_entry_as_int(table, "timing", "update_frequency_hz", &iv)) cfg->updateFrequencyHz = (uint32_t)clamp_to_schema("timing", "update_frequency_hz", iv);
    if (ini_table_get_entry_as_bool(table, "timing", "smoothing_enabled", &bv)) cfg->smoothingEnabled = bv ? 1 : 0;
    if (ini_table_get_entry_as_int(table, "timing", "settling_time_ms", &iv)) cfg->settlingTimeMs = (uint32_t)clamp_to_schema("timing", "settling_time_ms", iv);
    if (ini_table_get_entry_as_int(table, "timing", "config_reload_check_seconds", &iv)) cfg->configReloadCheckSeconds = (uint32_t)clamp_to_schema("timing", "config_reload_check_seconds", iv);

    ini_table_destroy(table);
    return true;
}

bool settings_item_is_preset_owned(const MenuItem *item)
{
    return item->screen == MENU_SCREEN_CUSTOMIZE && strcmp(item->section, "ui") != 0
        && item->type != FIELD_STRING && item->type != FIELD_ACTION;
}

// Removes `key` from `section`. ini_table_s is a plain struct (see config.h), so this
// edits it directly. (Comment lines never get here: ini_table_read_from_file drops
// them, which is why an app save has always stripped the plugin's generated comments.)
static void ini_remove_entry(ini_table_s *table, const char *section, const char *key)
{
    for (int i = 0; i < table->size; i++) {
        ini_section_s *sec = &table->section[i];
        if (strcmp(sec->name, section) != 0) continue;
        for (int q = 0; q < sec->size; q++) {
            if (strcmp(sec->entry[q].key, key) != 0) continue;
            free(sec->entry[q].key);
            free(sec->entry[q].value);
            memmove(&sec->entry[q], &sec->entry[q + 1], (size_t)(sec->size - q - 1) * sizeof(ini_entry_s));
            sec->size--;
            return;
        }
    }
}

void settings_fill_table(ini_table_s *table, const AmbientConfig *cfg, bool presetKeys)
{
    // MERGE FIX (reapplied -- this line of work still hasn't picked it
    // up as of v16): this used to start from a blank table containing
    // ONLY the fields this app knows about, then write that out --
    // silently destroying anything else already in the file, including
    // a hand-added [dev] section (dev_ip/dev_logging -- ps4_ambient_
    // light v2.2.5, confirmed working on real hardware). Loading the
    // existing file into the same table FIRST, then upserting just the
    // known fields on top of it via the same ini_table_create_entry
    // calls already below, preserves everything else untouched. (The
    // read happens in the callers now: settings_save and presets_save
    // load the file into `table` before calling this.)

    // BUG FIX: relay_host/relay_port/relay_signal_enabled have no
    // settings-UI row (by design -- this app doesn't manage them), so
    // cfg's copies of these three are only ever whatever settings_load
    // happened to read in, or the struct defaults (blank/24689/false)
    // if the file never had them. Writing them out unconditionally on
    // every save -- as this used to -- meant a completely fresh ini
    // (generated by the plugin itself, which never writes these keys
    // at all unless a user hand-adds them) grew all three keys the
    // moment it was ever saved from this app, even though nothing
    // opted in. Capture whether each key already existed in the file
    // BEFORE this save touches anything, and only write it back if it
    // did -- so this app can preserve a hand-added relay setup across
    // a save without ever being the thing that first introduces one.
    bool hadRelayHost   = ini_table_get_entry(table, "network", "relay_host") != NULL;
    bool hadRelayPort   = ini_table_get_entry(table, "network", "relay_port") != NULL;
    bool hadRelaySignal = ini_table_get_entry(table, "network", "relay_signal_enabled") != NULL;

    char buf[64];
    #define SET_INT(section, key, val) do { snprintf(buf, sizeof(buf), "%d", (int)(val)); ini_table_create_entry(table, section, key, buf); } while (0)

    ini_table_create_entry(table, "network", "wled_host", cfg->wledHost);
    SET_INT("network", "wled_port", cfg->wledPort);
    // Only re-written if already present -- see the BUG FIX comment above.
    if (hadRelayHost)   ini_table_create_entry(table, "network", "relay_host", cfg->relayHost);
    if (hadRelayPort)   SET_INT("network", "relay_port", cfg->relayPort);
    if (hadRelaySignal) ini_table_create_entry(table, "network", "relay_signal_enabled", cfg->relaySignalEnabled ? "true" : "false");

    SET_INT("layout", "led_count_top", cfg->ledCountTop);
    SET_INT("layout", "led_count_right", cfg->ledCountRight);
    SET_INT("layout", "led_count_bottom", cfg->ledCountBottom);
    SET_INT("layout", "led_count_left", cfg->ledCountLeft);
    ini_table_create_entry(table, "layout", "led_start_corner", kStartCornerNames[cfg->startCorner]);
    ini_table_create_entry(table, "layout", "led_direction", kDirectionNames[cfg->direction]);
    SET_INT("layout", "led_offset", cfg->ledOffset);
    SET_INT("layout", "scan_depth", cfg->scanDepth);
    ini_table_create_entry(table, "layout", "auto_letterbox_enabled", cfg->autoLetterboxEnabled ? "true" : "false");
    SET_INT("layout", "auto_letterbox_threshold", cfg->autoLetterboxThreshold);
    SET_INT("layout", "auto_letterbox_stability_frames", cfg->autoLetterboxStabilityFrames);
    SET_INT("layout", "auto_letterbox_check_interval_frames", cfg->autoLetterboxCheckIntervalFrames);

    SET_INT("color", "brightness", cfg->brightness);
    {
        uint32_t gi = cfg->gammaLutIndex < 8 ? cfg->gammaLutIndex : 0;
        ini_table_create_entry(table, "color", "gamma", kGammaNames[gi]);
    }
    SET_INT("color", "saturation", cfg->saturation);
    ini_table_create_entry(table, "color", "color_order", kColorOrderNames[cfg->colorOrder]);
    SET_INT("color", "black_level", cfg->blackLevel);
    SET_INT("color", "white_level", cfg->whiteLevel);
    SET_INT("color", "dark_threshold", cfg->darkThreshold);
    SET_INT("color", "contrast", cfg->contrast);
    SET_INT("color", "brightness_r", cfg->brightnessR);
    SET_INT("color", "brightness_g", cfg->brightnessG);
    SET_INT("color", "brightness_b", cfg->brightnessB);
    SET_INT("color", "gamma_r", cfg->gammaR);
    SET_INT("color", "gamma_g", cfg->gammaG);
    SET_INT("color", "gamma_b", cfg->gammaB);

    SET_INT("timing", "update_frequency_hz", cfg->updateFrequencyHz);
    ini_table_create_entry(table, "timing", "smoothing_enabled", cfg->smoothingEnabled ? "true" : "false");
    SET_INT("timing", "settling_time_ms", cfg->settlingTimeMs);
    SET_INT("timing", "config_reload_check_seconds", cfg->configReloadCheckSeconds);

    #undef SET_INT

    if (!presetKeys) {
        // Concise layout: what a preset owns lives in its [preset_*] section only.
        for (int i = 0; i < kMenuItemCount; i++)
            if (settings_item_is_preset_owned(&kMenuItems[i]))
                ini_remove_entry(table, kMenuItems[i].section, kMenuItems[i].key);

        // The two letterbox timings have no row in the app and are tuned together with
        // the plugin's detection scan, so a file only carries them if someone changed them.
        AmbientConfig d;
        settings_set_defaults(&d);
        if (cfg->autoLetterboxStabilityFrames == d.autoLetterboxStabilityFrames)
            ini_remove_entry(table, "layout", "auto_letterbox_stability_frames");
        if (cfg->autoLetterboxCheckIntervalFrames == d.autoLetterboxCheckIntervalFrames)
            ini_remove_entry(table, "layout", "auto_letterbox_check_interval_frames");
    }
}

bool settings_save(const AmbientConfig *cfg, const char *path)
{
    ini_table_s *table = ini_table_create();
    if (table == NULL) return false;
    ini_table_read_from_file(table, path);   // merge into the existing file, see settings_fill_table
    settings_fill_table(table, cfg, true);
    bool ok = ini_table_write_to_file(table, path);
    ini_table_destroy(table);
    return ok;
}

// Generic get/set through a MenuItem's byte offset into AmbientConfig.
// Only used for the numeric/bool/enum types (U32/I32/U16/BOOL/ENUM) --
// FIELD_STRING (wledHost) is handled separately by the UI via the
// on-screen keyboard, not through these.
// ---- display-unit helpers (see DisplayUnit in settings.h) -------------
// Presentation only: stored values stay raw 0-255 / 0-500 / etc. so the
// ini format and the plugin never see percentages. Only what is shown and
// typed changes -- "255" means nothing to most people, "100%" does.
static int32_t div_round(int32_t num, int32_t den)
{
    if (den <= 0) return 0;
    return (num >= 0) ? (num + den / 2) / den : -((-num + den / 2) / den);
}

int32_t settings_to_display(const MenuItem *item, int32_t stored)
{
    if (item->unit == UNIT_PCT_OF_MAX) return div_round(stored * 100, item->max);
    return stored;
}

int32_t settings_from_display(const MenuItem *item, int32_t display)
{
    int32_t v = display;
    if (item->unit == UNIT_PCT_OF_MAX) {
        if (display < 0) display = 0;
        if (display > 100) display = 100;
        v = div_round(display * item->max, 100);
    }
    if (v < item->min) v = item->min;
    if (v > item->max) v = item->max;
    return v;
}

int32_t settings_display_min(const MenuItem *item)
{
    return settings_to_display(item, item->min);
}

int32_t settings_display_max(const MenuItem *item)
{
    return settings_to_display(item, item->max);
}

// For UNIT_PCT_OF_MAX rows `step` in the table is ALREADY in percent (so
// brightness nudges by a clean 5%, thresholds by 1%); every other unit
// keeps stepping in stored units, which for the "direct" percents are
// percent anyway.
int32_t settings_display_step(const MenuItem *item)
{
    return item->step > 0 ? item->step : 1;
}

char *settings_format_display(const MenuItem *item, int32_t display, char *dst, size_t dstSize)
{
    switch (item->unit) {
        case UNIT_PCT_OF_MAX:
        case UNIT_PCT_DIRECT: snprintf(dst, dstSize, "%d%%", (int)display); break;
        case UNIT_PCT_SIGNED:
            if (display == 0) snprintf(dst, dstSize, "0%%");
            else snprintf(dst, dstSize, "%+d%%", (int)display);
            break;
        default: snprintf(dst, dstSize, "%d", (int)display); break;
    }
    return dst;
}


int32_t settings_get_i32(const AmbientConfig *cfg, const MenuItem *item)
{
    const uint8_t *base = (const uint8_t *)cfg + item->offset;
    switch (item->type) {
        case FIELD_U32:  return (int32_t)(*(const uint32_t *)base);
        case FIELD_I32:  return *(const int32_t *)base;
        case FIELD_U16:  return (int32_t)(*(const uint16_t *)base);
        case FIELD_BOOL: return *(const int *)base;
        case FIELD_ENUM: return *(const int *)base; // enums are plain int-backed in AmbientConfig
        default:         return 0; // FIELD_STRING -- not applicable
    }
}

void settings_set_i32(AmbientConfig *cfg, const MenuItem *item, int32_t value)
{
    if (value < item->min) value = item->min;
    if (value > item->max) value = item->max;
    uint8_t *base = (uint8_t *)cfg + item->offset;
    switch (item->type) {
        case FIELD_U32:  *(uint32_t *)base = (uint32_t)value; break;
        case FIELD_I32:  *(int32_t *)base = value; break;
        case FIELD_U16:  *(uint16_t *)base = (uint16_t)value; break;
        case FIELD_BOOL: *(int *)base = value ? 1 : 0; break;
        case FIELD_ENUM: *(int *)base = value; break;
        default: break; // FIELD_STRING -- not applicable
    }
}
