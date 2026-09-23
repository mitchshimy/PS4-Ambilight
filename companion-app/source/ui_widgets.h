// ui_widgets.h -- the blueprint's components, as components.
//
// Rather than hard-coding pixel positions per screen, this reproduces
// the small slice of CSS box model the design actually uses: border-box
// sizing, padding, margins, a flex row with `gap` and `flex: 1 1 240px`
// growth, and CSS line boxes for vertical text placement. Card heights
// then fall out of their content the same way they do in the browser.
//
// This was checked against the blueprint screenshots rather than
// assumed. On Set up, the four LED-count inputs land at x = 191, 581,
// 971, 1361 with width 370, the Colour row's six fields come out 240
// wide, and Screen sampling's five come out 292 -- all matching the
// captures pixel for pixel.

#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "ui_canvas.h"
#include "ui_text.h"
#include "ui_theme.h"
#include "ui_icons.h"

// ---- page metrics (style.css) ----
#define PAGE_W        1920.0f
#define PAGE_H        1080.0f
#define PAGE_MARGIN_X 144.0f
#define CONTENT_W     1632.0f     // 1920 - 2*144
#define CONTENT_TOP   236.0f      // .content { top: 236px }
#define CONTENT_H     844.0f
#define CARD_GAP      20.0f       // .content-inner { gap: 20px }

typedef enum {
    UI_FIELD_INPUT,   // .input   -- dark box, mono value
    UI_FIELD_PILL,    // .pill    -- raised box, leading icon, mono value
    UI_FIELD_TOGGLE   // the inline toggle-row used by Smoothing
} UiFieldKind;

typedef struct {
    UiIconId labelIcon;
    const char *label;
    UiColor labelColor;    // a == 0 -> --text-label

    UiFieldKind kind;
    const char *value;
    UiIconId valueIcon;    // .pill only
    bool toggleOn;         // UI_FIELD_TOGGLE only

    bool focused;          // draws the bracket focus marks

    // Flex sizing. basis 0 means the default `flex: 1 1 240px`.
    // fixed makes it `flex: 0 0 <basis>px`.
    float basis;
    bool fixed;
} UiField;

typedef struct {
    const UiField *fields;
    int count;
    float marginTop;       // extra space above this row (RGB balance's second row uses 22px)
} UiFieldRow;

typedef struct {
    UiColor accent;        // --panel-accent, the 3px left border
    UiIconId icon;
    UiColor iconColor;
    const char *title;
    const char *desc;

    const UiFieldRow *rows;
    int rowCount;

    // Optional dashed "Physical strip total: N LEDs" divider, drawn
    // after the given row index (-1 for none).
    int totalAfterRow;
    const char *totalLabel;   // text before the highlighted count
    const char *totalValue;   // the amber part
} UiCard;

float ui_card_height(const UiFonts *f, const UiCard *card, float width);

// Vertical offset (from the card's top edge) and height of one field
// row, so a caller can scroll a focused control into view without
// duplicating the card's layout maths.
bool ui_card_row_offset(const UiFonts *f, const UiCard *card, int row, float *outY, float *outH);
void  ui_card_draw(UiCanvas *c, const UiFonts *f, const UiCard *card, float x, float y, float width);

// ---- shared pieces used directly by the Home screen ----
void ui_focus_brackets(UiCanvas *c, UiRect r, UiColor color);

// A full outline ring drawn just outside a control's own bounding box
// -- for controls where ui_focus_brackets' corner marks would sit on
// top of a solid fill colour (a big amber button, say) and be hard to
// see there regardless of the bracket colour. Sitting outside the
// control means the ring is always against the page background
// instead, so contrast never depends on the control's own fill.
void ui_focus_ring(UiCanvas *c, UiRect r, UiRadius radius, float gap, float thickness, UiColor color);
void ui_draw_keycap(UiCanvas *c, const UiFonts *f, float x, float baselineY, const char *label, float *outAdvance);
// A keycap chip followed by a short description, e.g. "L1/R1" +
// "nudge value" -- the convention Home's own footer already uses.
typedef struct { const char *cap; const char *word; } UiHintItem;

// Line-box height of a hint row (keycap chips are taller than the
// surrounding text, so CSS-style line-box math is needed the same way
// ui_text_baseline_for_box() needs it elsewhere) -- callers use this
// to place whatever comes after the row.
float ui_hint_row_line_height(const UiFonts *f);
float ui_hint_row_baseline_above(const UiFonts *f);

// Draws items left to right starting at x, separated the same way
// Home's footer already separates its three hints.
void ui_draw_hint_row(UiCanvas *c, const UiFonts *f, float x, float baselineY,
                       const UiHintItem *items, int count);

// Word-wraps `text` to maxWidth, returning the line count and writing
// up to maxLines NUL-terminated lines into out[].
int ui_wrap_text(UiFont *font, const char *text, float maxWidth, char out[][256], int maxLines);

// Draws the page header (h1 + subtitle) at the blueprint's fixed
// position, shared by Set up and Customization.
void ui_page_header(UiCanvas *c, const UiFonts *f, const char *title, const char *subtitle);

#endif // UI_WIDGETS_H
