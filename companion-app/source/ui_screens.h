// ui_screens.h -- the three blueprint screens.
//
// Home is laid out at home.html's own absolute coordinates; Set up and
// Customization are built from ui_widgets' card/field model, which
// reproduces setup.html and customization.html's flow layout.

#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include "ui_canvas.h"
#include <stdint.h>
#include "ui_theme.h"
#include "color_pipeline.h"
#include "settings.h"
#include "layout.h"

typedef enum {
    UI_SCREEN_HOME,
    UI_SCREEN_SETUP,
    UI_SCREEN_CUSTOMIZATION
} UiScreenId;

typedef enum {
    UI_INSTALL_NONE,     // "Plugin not installed"
    UI_INSTALL_UPDATE,   // "Update available"
    UI_INSTALL_OK        // "Plugin installed"
} UiInstallState;

#define HOME_FOCUS_CTA   0
#define HOME_FOCUS_HELP  1
#define HOME_FOCUS_SETUP 2
#define HOME_FOCUS_CUST  3

#define SETUP_FIELD_COUNT 12
#define CUST_FIELD_COUNT  20

typedef struct {
    UiScreenId screen;

    // Home
    UiInstallState install;
    bool setupComplete;
    bool testRunning;
    bool toastVisible;
    int homeFocus;

    // Set up / Customization
    float scrollY;
    int focusField;           // index into that screen's field list; == field count means the Save button

    // Footer/status line, shown on every screen. statusIsSaved gives
    // the blueprint's two-tone "saved -> path -- ..." treatment;
    // otherwise the whole line renders in one colour (warn-amber for
    // statusIsError, muted grey otherwise).
    char statusLine[256];
    bool statusIsSaved;
    bool statusIsError;

    // The LED edge frame (led_frame.c) draws exactly these bytes, in
    // wire order -- the literal RGB triplets main.c last sent over
    // DDP, not an approximation of them. NULL/0 means nothing has
    // been sent yet (or the last send failed), which the frame shows
    // as a dim, unmistakably-placeholder colour rather than a fake
    // gradient. Owned by the caller (main.c); this struct only borrows
    // the pointer for the duration of one ui_render() call.
    const uint8_t *ledColors;
    int ledColorCount;

    // Screen positions for those same LEDs, in the same order --
    // main.c's own layout_build() output, reused here rather than
    // led_frame.c calling layout_build() a second time every frame
    // purely to re-derive positions main.c already computed.
    const LedSlot *ledSlots;
    int ledSlotCount;

    // True for a short window right after settings_save() succeeds --
    // the Save button shows a distinct "Saved!" state while this is
    // set, rather than nothing visibly happening. main.c owns the
    // timing (how long the window lasts); this struct just carries
    // the current yes/no for one frame.
    bool saveJustConfirmed;
} UiState;

void ui_render(UiCanvas *c, const UiFonts *f, const AmbientConfig *cfg, const UiState *st);

// Draws every part of Home except the LED edge frame (led_frame.c's
// job, driven by UiState.ledSlots/ledColors). Exposed separately so
// main.c can cache the result and, while Test Strip's animation keeps
// the LED frame the only thing actually changing, redraw just that
// instead of repainting all of Home's text, icons and background at
// 30fps for content that's identical frame to frame. ui_render()
// itself still draws Home in one combined call for callers (like the
// desktop verification harness) that have no reason to know this
// split exists.
void ui_render_home_static(UiCanvas *c, const UiFonts *f, const AmbientConfig *cfg, const UiState *st);

// Total content height of the current settings screen, so the caller
// can clamp scrolling.
float ui_screen_content_height(const UiFonts *f, const AmbientConfig *cfg, UiScreenId screen);

// Page-space top/bottom of the focused field's row at scroll 0, so the
// caller can keep it inside the .content viewport.
bool ui_screen_focus_bounds(const UiFonts *f, const AmbientConfig *cfg, UiScreenId screen,
                             int fieldIndex, float *outTop, float *outBottom);

// The [start, start+count) span of kMenuItems belonging to one
// MenuScreen -- shared by the card builder (ui_screens.c) and main.c's
// D-Pad navigation/editing, so both ever agree on which kMenuItems
// index a given on-screen field is. A screen's items are one
// contiguous run (see settings.c's own comment above kMenuItems).
void ui_screen_item_range(MenuScreen screen, int *outStart, int *outCount);

// Row layout for D-Pad grid navigation: fills rowStart[]/rowCount[]
// with each row's starting field index (0-based within the screen,
// matching UiState.focusField) and field count, in the same row
// splits the card renderer uses (kGroupLayouts in ui_screens.c) --
// main.c's Up/Down/Left/Right navigation is built on this so it can
// never disagree with what's actually drawn. Returns the number of
// rows written (capped at maxRows).
int ui_screen_field_rows(UiScreenId screen, int rowStart[], int rowCount[], int maxRows);

#endif // UI_SCREENS_H
