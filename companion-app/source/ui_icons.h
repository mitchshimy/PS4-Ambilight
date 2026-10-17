// ui_icons.h -- the blueprint's line icons.
// More: docs/code-comments.md (companion-app/source/ui_icons.h:1)

#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "ui_canvas.h"

typedef enum {
    ICON_NONE = 0,

    // Set up
    ICON_WIFI,          // WLED connection + WLED IPv4 + UDP port
    ICON_CLOCK,         // Output FPS, Settling time
    ICON_LED_STRIP,     // LED strip layout, per-edge counts, strip total
    ICON_LOOP,          // LED offset, Direction
    ICON_CORNER,        // Start corner
    ICON_GLOBE,         // Colour order
    ICON_REFRESH,       // Live reload, Update plugin

    // Customization
    ICON_FRAME,         // Screen sampling, Edge depth, Auto letterbox
    ICON_LEVELS,        // Colour card, Customization nav tab
    ICON_SUN,           // Brightness
    ICON_DROPLET,       // Saturation
    ICON_GAMMA,         // Gamma
    ICON_MOON,          // Black level, Black threshold
    ICON_SUN_FILLED,    // White level
    ICON_SPLIT,         // Contrast
    ICON_WAVES,         // Motion and darkness, Smoothing
    ICON_SLIDERS_RGB,   // RGB balance card title (three channel colours)
    ICON_SLIDER_R,      // Red balance
    ICON_SLIDER_G,      // Green balance
    ICON_SLIDER_B,      // Blue balance
    ICON_RING,          // Gamma R/G/B

    // Home
    ICON_HELP,
    ICON_QR,            // Help screen: open QR code popup
    ICON_CALENDAR,      // Set up nav tab
    ICON_ACTIVITY,      // Test strip
    ICON_DOWNLOAD,      // Install plugin
    ICON_STOP,          // Stop test
    ICON_SAVE,          // Save customization
    ICON_WARNING,       // toast
    ICON_CHECK,         // save confirmation

    ICON_COUNT
} UiIconId;

// Draws the icon scaled from its 24x24 viewBox into a size x size box with its top-left at (x, y).
// More: docs/code-comments.md (companion-app/source/ui_icons.h:59)
void ui_icon_draw(UiCanvas *c, UiIconId id, float x, float y, float size, UiColor color);

#endif // UI_ICONS_H
