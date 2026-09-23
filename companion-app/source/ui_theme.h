// ui_theme.h -- style.css :root, transcribed.
//
// Every colour below is the literal token from the blueprint's
// stylesheet, including the ones written as rgba() -- those stay
// translucent here rather than being flattened to a "solid
// approximation", because a 9%-white hairline over a #15171b panel
// and a #2b2e34 opaque line are visibly different at 1px.

#ifndef UI_THEME_H
#define UI_THEME_H

#include "ui_canvas.h"
#include "ui_text.h"

// ---- surfaces ----
#define COL_BG0             ui_rgb(0x0b, 0x0c, 0x0e)
#define COL_PANEL_BG        ui_rgb(0x15, 0x17, 0x1b)
#define COL_PANEL_BG_RAISED ui_rgb(0x19, 0x1b, 0x20)
#define COL_PANEL_BORDER    ui_rgba(255, 255, 255, 0.09f)
#define COL_PANEL_BORDER_STRONG ui_rgba(255, 255, 255, 0.16f)
#define COL_INPUT_BG        ui_rgb(0x0f, 0x10, 0x13)
#define COL_INPUT_BORDER    ui_rgba(255, 255, 255, 0.12f)

#define RADIUS_PANEL 8.0f
#define RADIUS_INPUT 4.0f

// ---- type colour ----
#define COL_TEXT_HEADING ui_rgb(0xf1, 0xef, 0xe8)
#define COL_TEXT_BODY    ui_rgb(0x8d, 0x91, 0x99)
#define COL_TEXT_LABEL   ui_rgb(0xb9, 0xbd, 0xc4)
#define COL_TEXT_VALUE   ui_rgb(0xf1, 0xef, 0xe8)

// ---- accents ----
#define COL_AMBER        ui_rgb(0xff, 0xb4, 0x54)
// A visibly brighter amber for a solid-fill control's focused state
// (the Save button) -- not a blueprint token, since the blueprint has
// no console-focus concept at all; needed because corner brackets
// drawn on top of an already-amber fill (ui_focus_brackets' usual
// treatment) have too little contrast to see, which was the actual
// complaint this fixes: the save button looked identical focused or
// not.
#define COL_AMBER_BRIGHT ui_rgb(0xff, 0xcb, 0x87)
#define COL_AMBER_DIM    ui_rgba(255, 180, 84, 0.16f)
#define COL_CYAN         ui_rgb(0x5c, 0xd6, 0xdf)
#define COL_CHAN_RED     ui_rgb(0xff, 0x6b, 0x5b)
#define COL_CHAN_GREEN   ui_rgb(0x5f, 0xdc, 0x94)
#define COL_CHAN_BLUE    ui_rgb(0x5b, 0x9d, 0xff)
#define COL_WARN         ui_rgb(0xff, 0xb4, 0x54)
#define COL_OK           ui_rgb(0x5f, 0xdc, 0x94)
// Text drawn on top of a solid amber control (.cta / .pill-accent /
// .save-btn all use #231506).
#define COL_ON_AMBER     ui_rgb(0x23, 0x15, 0x06)

// ---- fonts ----
// Named for the rule they serve in style.css, so a call site reads
// like the stylesheet it came from.
typedef struct {
    UiFont *h1;             // Space Grotesk 700 / 50px
    UiFont *pageSub;        // 400 / 19px
    UiFont *cardTitle;      // 700 / 24px
    UiFont *cardDesc;       // 400 / 16px
    UiFont *fieldLabel;     // 600 / 14px
    UiFont *navPill;        // 600 / 20px
    UiFont *cta;            // 600 / 22px
    UiFont *btn;            // 600 / 19px
    UiFont *hint;           // 500 / 14px
    UiFont *note;           // 400 / 14px

    UiFont *monoInput;      // JetBrains Mono 500 / 20px
    UiFont *monoPill;       // 500 / 18px
    UiFont *monoToggle;     // 600 / 18px
    UiFont *monoTotal;      // 600 / 15px
    UiFont *monoBanner;     // 400 / 15px
    UiFont *monoStatus;     // 400 / 14px
    UiFont *monoKeycap;     // 500 / 12px
} UiFonts;

// fontDir is the directory holding the bundled .ttf files, with no
// trailing slash (e.g. "/app0/assets/fonts").
bool ui_fonts_load(UiFonts *out, const char *fontDir);
void ui_fonts_destroy(UiFonts *f);

#endif // UI_THEME_H
