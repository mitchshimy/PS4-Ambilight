#ifndef COLOR_PIPELINE_H
#define COLOR_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>

// AmbientConfig, StartCorner, LedDirection, ColorOrder below are
// copied VERBATIM from ps4_ambient_light's own main.c -- originally
// synced against v2.2, re-synced against v2.6 as of this app's v17
// (added relayHost/relayPort/relaySignalEnabled -- see the struct's
// own comment on those fields below; the real plugin's separate
// devIp/devLoggingEnabled fields are NOT mirrored here since nothing
// in this app reads or writes them -- settings_save's existing
// merge-preserve behavior already protects a hand-added [dev] section
// without this app needing to understand it, same as before this
// sync). This must stay byte-for-byte in sync with the real plugin
// struct -- the settings UI and ini reader/writer in this app depend
// on field names matching exactly. Re-sync from the plugin's main.c
// whenever it changes, don't hand-edit independently.

typedef enum { CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT, CORNER_TOP_LEFT, CORNER_TOP_RIGHT } StartCorner;
typedef enum { DIR_CLOCKWISE, DIR_COUNTERCLOCKWISE } LedDirection;
typedef enum { ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR } ColorOrder;

typedef struct {
    // [network]
    char wledHost[64];
    uint16_t wledPort;
    // v17: matches the real plugin's own [network] relayHost/relayPort
    // -- a separate destination from the WLED controller above,
    // wled-relay's own tv_external_source.py listener runs on its own
    // host, not on the WLED device itself.
    char relayHost[64];
    uint16_t relayPort;
    // Personal-setup integration with one specific wled-relay project,
    // not something every user of this app has -- OFF by default in
    // the real plugin, same here.
    bool relaySignalEnabled;
    // [layout]
    uint32_t ledCountTop, ledCountRight, ledCountBottom, ledCountLeft;
    StartCorner startCorner;
    LedDirection direction;
    int32_t ledOffset;
    uint32_t marginTop, marginRight, marginBottom, marginLeft;
    uint32_t scanDepth;
    // [color]
    uint32_t brightness;
    uint32_t gammaLutIndex;
    int32_t saturation;
    ColorOrder colorOrder;
    uint32_t blackLevel;
    uint32_t whiteLevel;
    uint32_t darkThreshold;
    int32_t contrast;
    uint32_t brightnessR, brightnessG, brightnessB;
    uint32_t gammaR, gammaG, gammaB;
    // [timing]
    uint32_t updateFrequencyHz;
    int smoothingEnabled;
    uint32_t settlingTimeMs;
    uint32_t configReloadCheckSeconds;
} AmbientConfig;

// Must be called once whenever gammaR/gammaG/gammaB change (config
// load, or a live edit in the settings UI) before colorpipeline_process
// is called -- mirrors the real plugin calling this at config load.
void colorpipeline_rebuild_perchannel_gamma_luts(const AmbientConfig *cfg);

// Runs the exact same processing chain as the real plugin's
// applyColorProcessing on a single input color. out3 receives the
// final 3 bytes already in the configured wire order (colorOrder).
void colorpipeline_process(const AmbientConfig *cfg, uint8_t r8, uint8_t g8, uint8_t b8, uint8_t *out3);

// Exposed separately in case the app needs the wire-order remap on its
// own (e.g. showing "what the strip receives" vs "the true RGB color"
// as two separate preview swatches).
void colorpipeline_write_color_ordered(uint8_t r, uint8_t g, uint8_t b, ColorOrder order, uint8_t *out3);

#endif // COLOR_PIPELINE_H
