// settings.c -- ini schema and defaults copied verbatim from
// ps4_ambient_light v2.2's own g_config initializer and
// ambient_load_config's ini_table_get_entry* calls. Every (section,
// key) pair below was checked against that file directly, not
// recalled from memory. If the plugin's schema changes, re-sync this
// table from its main.c.

#include "settings.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

static const char *kStartCornerNames[] = { "bottom_left", "bottom_right", "top_left", "top_right" };
static const char *kDirectionNames[]   = { "clockwise", "counterclockwise" };
static const char *kColorOrderNames[]  = { "RGB", "RBG", "GRB", "GBR", "BRG", "BGR" };

#define OFF(field) offsetof(AmbientConfig, field)

const MenuItem kMenuItems[] = {
    // [network] -- wledHost handled separately (FIELD_STRING, needs the
    // on-screen keyboard, not a +/- adjuster like everything else here)
    { "WLED Host",         "network", "wled_host",                  FIELD_STRING, OFF(wledHost),        0,     0,   0, NULL, 0 },
    { "WLED Port",         "network", "wled_port",                  FIELD_U16,    OFF(wledPort),        1, 65535,   1, NULL, 0 },

    // [layout]
    { "LEDs: Top",         "layout",  "led_count_top",              FIELD_U32,    OFF(ledCountTop),     1,   500,   1, NULL, 0 },
    { "LEDs: Right",       "layout",  "led_count_right",            FIELD_U32,    OFF(ledCountRight),   1,   500,   1, NULL, 0 },
    { "LEDs: Bottom",      "layout",  "led_count_bottom",           FIELD_U32,    OFF(ledCountBottom),  1,   500,   1, NULL, 0 },
    { "LEDs: Left",        "layout",  "led_count_left",             FIELD_U32,    OFF(ledCountLeft),    1,   500,   1, NULL, 0 },
    { "Start Corner",      "layout",  "led_start_corner",           FIELD_ENUM,   OFF(startCorner),     0,     3,   1, kStartCornerNames, 4 },
    { "Direction",         "layout",  "led_direction",              FIELD_ENUM,   OFF(direction),       0,     1,   1, kDirectionNames, 2 },
    { "LED Offset",        "layout",  "led_offset",                 FIELD_I32,    OFF(ledOffset),   -1000,  1000,   1, NULL, 0 },
    { "Margin: Top",       "layout",  "capture_margin_top",         FIELD_U32,    OFF(marginTop),       0,   500,   1, NULL, 0 },
    { "Margin: Right",     "layout",  "capture_margin_right",       FIELD_U32,    OFF(marginRight),     0,   500,   1, NULL, 0 },
    { "Margin: Bottom",    "layout",  "capture_margin_bottom",      FIELD_U32,    OFF(marginBottom),    0,   500,   1, NULL, 0 },
    { "Margin: Left",      "layout",  "capture_margin_left",        FIELD_U32,    OFF(marginLeft),      0,   500,   1, NULL, 0 },
    { "Scan Depth",        "layout",  "scan_depth",                 FIELD_U32,    OFF(scanDepth),       0,    10,   1, NULL, 0 },

    // [color]
    { "Brightness",        "color",   "brightness",                 FIELD_U32,    OFF(brightness),      0,   255,   5, NULL, 0 },
    { "Gamma (preset)",    "color",   "gamma",                      FIELD_U32,    OFF(gammaLutIndex),   0,     7,   1, NULL, 0 }, // display maps 0-7 -> "1.0".."2.8", see ui.c
    { "Saturation",        "color",   "saturation",                 FIELD_I32,    OFF(saturation),   -100,   300,   5, NULL, 0 },
    { "Color Order",       "color",   "color_order",                FIELD_ENUM,   OFF(colorOrder),      0,     5,   1, kColorOrderNames, 6 },
    { "Black Level",       "color",   "black_level",                FIELD_U32,    OFF(blackLevel),      0,   100,   1, NULL, 0 },
    { "White Level",       "color",   "white_level",                FIELD_U32,    OFF(whiteLevel),      0,   100,   1, NULL, 0 },
    { "Dark Threshold",    "color",   "dark_threshold",             FIELD_U32,    OFF(darkThreshold),   0,   255,   1, NULL, 0 },
    { "Contrast",          "color",   "contrast",                   FIELD_I32,    OFF(contrast),     -100,   300,   5, NULL, 0 },
    { "Brightness R",      "color",   "brightness_r",               FIELD_U32,    OFF(brightnessR),     0,   500,   5, NULL, 0 },
    { "Brightness G",      "color",   "brightness_g",               FIELD_U32,    OFF(brightnessG),     0,   500,   5, NULL, 0 },
    { "Brightness B",      "color",   "brightness_b",               FIELD_U32,    OFF(brightnessB),     0,   500,   5, NULL, 0 },
    { "Gamma R",           "color",   "gamma_r",                    FIELD_U32,    OFF(gammaR),         10,   500,   5, NULL, 0 },
    { "Gamma G",           "color",   "gamma_g",                    FIELD_U32,    OFF(gammaG),         10,   500,   5, NULL, 0 },
    { "Gamma B",           "color",   "gamma_b",                    FIELD_U32,    OFF(gammaB),         10,   500,   5, NULL, 0 },

    // [timing]
    { "Update Rate (Hz)",  "timing",  "update_frequency_hz",        FIELD_U32,    OFF(updateFrequencyHz), 1,  240,   1, NULL, 0 },
    { "Smoothing",         "timing",  "smoothing_enabled",          FIELD_BOOL,   OFF(smoothingEnabled),  0,    1,   1, NULL, 0 },
    { "Settling Time (ms)","timing",  "settling_time_ms",           FIELD_U32,    OFF(settlingTimeMs),    0, 5000,  50, NULL, 0 },
    { "Reload Check (s)",  "timing",  "config_reload_check_seconds",FIELD_U32,    OFF(configReloadCheckSeconds), 0, 60, 1, NULL, 0 },
};
const int kMenuItemCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);

// Copied verbatim from ps4_ambient_light's own g_config initializer
// (main.c) -- see that file if this ever needs re-syncing.
void settings_set_defaults(AmbientConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->wledHost, "192.168.2.110", sizeof(cfg->wledHost) - 1);
    cfg->wledPort = 4048;
    cfg->ledCountTop = 73; cfg->ledCountRight = 41; cfg->ledCountBottom = 73; cfg->ledCountLeft = 42;
    cfg->startCorner = CORNER_BOTTOM_LEFT;
    cfg->direction = DIR_CLOCKWISE;
    cfg->ledOffset = 0;
    cfg->marginTop = 0; cfg->marginRight = 0; cfg->marginBottom = 0; cfg->marginLeft = 0;
    cfg->scanDepth = 1;
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
    if (ini_table_get_entry_as_int(table, "network", "wled_port", &iv) && iv > 0 && iv <= 65535)
        cfg->wledPort = (uint16_t)iv;

    if (ini_table_get_entry_as_int(table, "layout", "led_count_top", &iv) && iv > 0) cfg->ledCountTop = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_right", &iv) && iv > 0) cfg->ledCountRight = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_bottom", &iv) && iv > 0) cfg->ledCountBottom = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_left", &iv) && iv > 0) cfg->ledCountLeft = (uint32_t)iv;
    cfg->startCorner = parse_start_corner(ini_table_get_entry(table, "layout", "led_start_corner"), cfg->startCorner);
    cfg->direction = parse_direction(ini_table_get_entry(table, "layout", "led_direction"), cfg->direction);
    if (ini_table_get_entry_as_int(table, "layout", "led_offset", &iv)) cfg->ledOffset = iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_top", &iv) && iv >= 0) cfg->marginTop = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_right", &iv) && iv >= 0) cfg->marginRight = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_bottom", &iv) && iv >= 0) cfg->marginBottom = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_left", &iv) && iv >= 0) cfg->marginLeft = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "scan_depth", &iv) && iv >= 0) cfg->scanDepth = (uint32_t)iv;

    if (ini_table_get_entry_as_int(table, "color", "brightness", &iv) && iv >= 0 && iv <= 255) cfg->brightness = (uint32_t)iv;
    // gamma preset is stored as a string ("1.0".."2.8") in the real ini,
    // not an index -- match that exactly rather than inventing our own
    // representation, so a file this app writes still loads correctly
    // in the real plugin.
    if ((v = ini_table_get_entry(table, "color", "gamma")) != NULL) {
        static const char *kGammaStrings[8] = {"1.0","1.4","1.8","2.0","2.2","2.4","2.6","2.8"};
        for (int i = 0; i < 8; i++) if (!strcmp(v, kGammaStrings[i])) { cfg->gammaLutIndex = (uint32_t)i; break; }
    }
    if (ini_table_get_entry_as_int(table, "color", "saturation", &iv) && iv >= -100 && iv <= 300) cfg->saturation = iv;
    cfg->colorOrder = parse_color_order(ini_table_get_entry(table, "color", "color_order"), cfg->colorOrder);
    if (ini_table_get_entry_as_int(table, "color", "black_level", &iv) && iv >= 0 && iv <= 100) cfg->blackLevel = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "white_level", &iv) && iv >= 0 && iv <= 100) cfg->whiteLevel = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "dark_threshold", &iv) && iv >= 0 && iv <= 255) cfg->darkThreshold = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "contrast", &iv) && iv >= -100 && iv <= 300) cfg->contrast = iv;
    if (ini_table_get_entry_as_int(table, "color", "brightness_r", &iv) && iv >= 0 && iv <= 500) cfg->brightnessR = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "brightness_g", &iv) && iv >= 0 && iv <= 500) cfg->brightnessG = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "brightness_b", &iv) && iv >= 0 && iv <= 500) cfg->brightnessB = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "gamma_r", &iv) && iv >= 10 && iv <= 500) cfg->gammaR = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "gamma_g", &iv) && iv >= 10 && iv <= 500) cfg->gammaG = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "gamma_b", &iv) && iv >= 10 && iv <= 500) cfg->gammaB = (uint32_t)iv;

    if (ini_table_get_entry_as_int(table, "timing", "update_frequency_hz", &iv) && iv > 0 && iv <= 240) cfg->updateFrequencyHz = (uint32_t)iv;
    if (ini_table_get_entry_as_bool(table, "timing", "smoothing_enabled", &bv)) cfg->smoothingEnabled = bv ? 1 : 0;
    if (ini_table_get_entry_as_int(table, "timing", "settling_time_ms", &iv) && iv >= 0) cfg->settlingTimeMs = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "timing", "config_reload_check_seconds", &iv) && iv >= 0) cfg->configReloadCheckSeconds = (uint32_t)iv;

    ini_table_destroy(table);
    return true;
}

bool settings_save(const AmbientConfig *cfg, const char *path)
{
    ini_table_s *table = ini_table_create();
    if (table == NULL) return false;

    // MERGE FIX (found while reconciling this app into the main
    // project tree, reapplied here because this v0-v9 upload branched
    // from before the fix and never picked it up): this used to start
    // from a blank table containing ONLY the fields this app knows
    // about, then write that out -- silently destroying anything else
    // already in the file. That's a real problem, not theoretical: it
    // would delete a user's hand-added [dev] section (dev_ip/
    // dev_logging -- ps4_ambient_light v2.2.5, confirmed working on
    // real hardware) the first time they saved from this app, along
    // with any future ini field the plugin gains that this app doesn't
    // know about yet. Loading the existing file into the same table
    // FIRST, then upserting just the known fields on top of it via the
    // same ini_table_create_entry calls already below, preserves
    // everything else untouched. A failed read here just means
    // "nothing to preserve yet" (e.g. first save ever) -- not fatal,
    // the known fields below still populate the table either way.
    ini_table_read_from_file(table, path);

    char buf[64];
    #define SET_INT(section, key, val) do { snprintf(buf, sizeof(buf), "%d", (int)(val)); ini_table_create_entry(table, section, key, buf); } while (0)

    ini_table_create_entry(table, "network", "wled_host", cfg->wledHost);
    SET_INT("network", "wled_port", cfg->wledPort);

    SET_INT("layout", "led_count_top", cfg->ledCountTop);
    SET_INT("layout", "led_count_right", cfg->ledCountRight);
    SET_INT("layout", "led_count_bottom", cfg->ledCountBottom);
    SET_INT("layout", "led_count_left", cfg->ledCountLeft);
    ini_table_create_entry(table, "layout", "led_start_corner", kStartCornerNames[cfg->startCorner]);
    ini_table_create_entry(table, "layout", "led_direction", kDirectionNames[cfg->direction]);
    SET_INT("layout", "led_offset", cfg->ledOffset);
    SET_INT("layout", "capture_margin_top", cfg->marginTop);
    SET_INT("layout", "capture_margin_right", cfg->marginRight);
    SET_INT("layout", "capture_margin_bottom", cfg->marginBottom);
    SET_INT("layout", "capture_margin_left", cfg->marginLeft);
    SET_INT("layout", "scan_depth", cfg->scanDepth);

    SET_INT("color", "brightness", cfg->brightness);
    {
        static const char *kGammaStrings[8] = {"1.0","1.4","1.8","2.0","2.2","2.4","2.6","2.8"};
        uint32_t gi = cfg->gammaLutIndex < 8 ? cfg->gammaLutIndex : 0;
        ini_table_create_entry(table, "color", "gamma", kGammaStrings[gi]);
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

    bool ok = ini_table_write_to_file(table, path);
    ini_table_destroy(table);
    return ok;
}

// Generic get/set through a MenuItem's byte offset into AmbientConfig.
// Only used for the numeric/bool/enum types (U32/I32/U16/BOOL/ENUM) --
// FIELD_STRING (wledHost) is handled separately by the UI via the
// on-screen keyboard, not through these.
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
