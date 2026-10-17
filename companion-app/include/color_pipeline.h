#ifndef COLOR_PIPELINE_H
#define COLOR_PIPELINE_H

#include <stdint.h>

// AmbientConfig, StartCorner, LedDirection, ColorOrder below are copied VERBATIM from...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:6)

typedef enum { CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT, CORNER_TOP_LEFT, CORNER_TOP_RIGHT } StartCorner;
typedef enum { DIR_CLOCKWISE, DIR_COUNTERCLOCKWISE } LedDirection;
typedef enum { ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR } ColorOrder;

typedef struct {
    // [network]
    char wledHost[64];
    uint16_t wledPort;
    // wled-relay hand-off signal (ps4_ambient_light's own v2.3+ feature, ported to this app at v17, then...
    // More: docs/code-comments.md (companion-app/include/color_pipeline.h:21)
    char relayHost[64];
    uint16_t relayPort;
    int relaySignalEnabled;
    // [layout]
    uint32_t ledCountTop, ledCountRight, ledCountBottom, ledCountLeft;
    StartCorner startCorner;
    LedDirection direction;
    int32_t ledOffset;
    uint32_t scanDepth;
    int autoLetterboxEnabled;
    uint32_t autoLetterboxThreshold;
    uint32_t autoLetterboxStabilityFrames;
    uint32_t autoLetterboxCheckIntervalFrames;
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
    // [shell] -- the shell feed (shell-feed/), the second plugin that lights the strip from the console's...
    // More: docs/code-comments.md (companion-app/include/color_pipeline.h:57)
    int shellEnabled;    // ini [shell] enabled -- the same key the shell feed itself reads
    int notificationsEnabled; // ini [notifications] enabled: the console's notification bar, for the plugin, the
                              // shell feed and this app. Default ON (absent = on, like the plugin reads it).
    int shellAutoload;   // NOT an ini key: mirrors GoldHEN's payload AutoRun list (main.c reads
                         // and writes that file, see payload_ini.h). Never saved with the ini.
} AmbientConfig;

// Must be called once whenever gammaR/gammaG/gammaB change (config load, or a live edit in the...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:67)
void colorpipeline_rebuild_perchannel_gamma_luts(const AmbientConfig *cfg);

// Runs the exact same processing chain as the real plugin's applyColorProcessing on a single input...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:72)
void colorpipeline_process(const AmbientConfig *cfg, uint8_t r8, uint8_t g8, uint8_t b8, uint8_t *out3);

// Exposed separately in case the app needs the wire-order remap on its own (e.g. showing "what the...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:77)
void colorpipeline_write_color_ordered(uint8_t r, uint8_t g, uint8_t b, ColorOrder order, uint8_t *out3);

#endif // COLOR_PIPELINE_H
