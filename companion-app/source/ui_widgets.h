// ui_widgets.h -- the blueprint's components, as components.
// More: docs/code-comments.md (companion-app/source/ui_widgets.h:1)

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
    UI_FIELD_TOGGLE,  // the inline toggle-row used by Smoothing
    UI_FIELD_BUTTON   // an action, drawn like a pill; `armed` swaps it to the amber "press again" look
} UiFieldKind;

typedef struct {
    UiIconId labelIcon;
    const char *label;
    UiColor labelColor;    // a == 0 -> --text-label

    UiFieldKind kind;
    const char *value;
    UiIconId valueIcon;    // .pill only
    bool toggleOn;         // UI_FIELD_TOGGLE only
    bool armed;            // UI_FIELD_BUTTON only -- waiting on a confirming second press

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

// Hash of everything that decides how a card looks (text, values, icons, colours, focus, armed...)...
// two cards with the same signature draw identically at the same width. Never 0.
uint32_t ui_card_signature(const UiCard *card);

// Vertical offset (from the card's top edge) and height of one field row, so a caller can scroll a...
// More: docs/code-comments.md (companion-app/source/ui_widgets.h:83)
bool ui_card_row_offset(const UiFonts *f, const UiCard *card, int row, float *outY, float *outH);
void  ui_card_draw(UiCanvas *c, const UiFonts *f, const UiCard *card, float x, float y, float width);

// ---- shared pieces used directly by the Home screen ----
void ui_focus_brackets(UiCanvas *c, UiRect r, UiColor color);

// A full outline ring drawn just outside a control's own bounding box -- for controls where...
// More: docs/code-comments.md (companion-app/source/ui_widgets.h:92)
void ui_focus_ring(UiCanvas *c, UiRect r, UiRadius radius, float gap, float thickness, UiColor color);
void ui_draw_keycap(UiCanvas *c, const UiFonts *f, float x, float baselineY, const char *label, float *outAdvance);
// A keycap chip followed by a short description, e.g. "L1/R1" +
// "nudge value" -- the convention Home's own footer already uses.
typedef struct { const char *cap; const char *word; } UiHintItem;

// Line-box height of a hint row (keycap chips are taller than the surrounding text, so CSS-style...
// More: docs/code-comments.md (companion-app/source/ui_widgets.h:104)
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
