// ui_screens.h -- the three blueprint screens.
// More: docs/code-comments.md (companion-app/source/ui_screens.h:1)

#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include "ui_canvas.h"
#include <stdint.h>
#include "ui_theme.h"
#include "color_pipeline.h"
#include "settings.h"
#include "layout.h"
#include "install_plan.h"   // UiInstallState (the plugin's states), ShellSummary, HomePlan

typedef enum {
    UI_SCREEN_HOME,
    UI_SCREEN_SETUP,
    UI_SCREEN_CUSTOMIZATION,
    UI_SCREEN_HELP
} UiScreenId;

#define HOME_FOCUS_CTA   0
#define HOME_FOCUS_HELP  1
#define HOME_FOCUS_SETUP 2
#define HOME_FOCUS_CUST  3

// Field counts per screen are derived from kMenuItems via ui_screen_item_range() -- don't reintroduce...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:30)

typedef struct {
    UiScreenId screen;

    // Home
    UiInstallState install;
    // GoldHEN's "Enable Plugins Loader", read from its config. Zero is LOADER_UNKNOWN, which Home
    // treats as on: only a definite OFF changes what Home shows or what its button does.
    PluginLoaderState loader;
    // The shell feed's side of Home: zero-initialised means SHELL_OFF, which draws Home exactly as it was...
    // More: docs/code-comments.md (companion-app/source/ui_screens.h:42)
    ShellSummary shell;
    // Whether the Ambilight payload is actually running, read from the shell feed's heartbeat file...
    // More: docs/code-comments.md (companion-app/source/ui_screens.h:47)
    bool shellFeedRunning;
    bool shellFeedStarting;
    bool setupComplete;
    bool testRunning;
    bool toastVisible;
    // Firmware warning on Home (fw_check.c), also repeated when the payload is launched. Both empty =
    // no warning. Drawn only while the Shell feed switch is on, and never blocks anything.
    char fwTitle[96];
    char fwBody[128];
    int homeFocus;

    // Help screen only: the Triangle-button QR popup (help_qr.c) pointing at this project's README.
    // More: docs/code-comments.md (companion-app/source/ui_screens.h:61)
    bool qrVisible;

    // Set up / Customization
    float scrollY;
    int focusField;           // index into that screen's field list; == field count means the Save button

    // Footer/status line, shown on every screen. statusIsSaved gives the blueprint's two-tone "saved ->...
    // More: docs/code-comments.md (companion-app/source/ui_screens.h:70)
    char statusLine[256];
    // Set up / Customization: a one-line hint for the row just pressed (empty = none). Those screens
    // draw no status line, so the save bar shows this in place of its usual note.
    char settingsHint[160];
    bool statusIsSaved;
    bool statusIsError;

    // The LED edge frame (led_frame.c) draws exactly these bytes, in wire order -- the literal RGB...
    // More: docs/code-comments.md (companion-app/source/ui_screens.h:81)
    const uint8_t *ledColors;
    int ledColorCount;

    // Screen positions for those same LEDs, in the same order -- main.c's own layout_build() output...
    // More: docs/code-comments.md (companion-app/source/ui_screens.h:91)
    const LedSlot *ledSlots;
    int ledSlotCount;

    // True for a short window right after settings_save() succeeds -- the Save button shows a distinct...
    // More: docs/code-comments.md (companion-app/source/ui_screens.h:98)
    bool saveJustConfirmed;
} UiState;

void ui_render(UiCanvas *c, const UiFonts *f, const AmbientConfig *cfg, const UiState *st);

// Same picture as ui_render(), but Set up / Customization / Help composite cached card layers (scroll rounds to whole pixels).
// Home ignores the cache.
typedef struct UiScreenCache UiScreenCache;
UiScreenCache *ui_screen_cache_create(void);
void ui_screen_cache_destroy(UiScreenCache *cache);
void ui_screen_cache_invalidate(UiScreenCache *cache);   // after anything the signatures can't see (fonts, config reload)
void ui_render_cached(UiCanvas *c, const UiFonts *f, const AmbientConfig *cfg, const UiState *st,
                      UiScreenCache *cache);

// Draws every part of Home except the LED edge frame (led_frame.c's job, driven by...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:108)
void ui_render_home_static(UiCanvas *c, const UiFonts *f, const AmbientConfig *cfg, const UiState *st);

// Total content height of the current settings screen, so the caller
// can clamp scrolling.
float ui_screen_content_height(const UiFonts *f, const AmbientConfig *cfg, UiScreenId screen);

// Page-space top/bottom of the focused field's row at scroll 0, so the
// caller can keep it inside the .content viewport.
bool ui_screen_focus_bounds(const UiFonts *f, const AmbientConfig *cfg, UiScreenId screen,
                             int fieldIndex, float *outTop, float *outBottom);

// The [start, start+count) span of kMenuItems belonging to one MenuScreen -- shared by the card...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:128)
void ui_screen_item_range(MenuScreen screen, int *outStart, int *outCount);

// Row layout for D-Pad grid navigation: fills rowStart[]/rowCount[] with each row's starting field...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:135)
int ui_screen_field_rows(UiScreenId screen, int rowStart[], int rowCount[], int maxRows);

#endif // UI_SCREENS_H
