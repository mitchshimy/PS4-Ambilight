// ui_text.h -- FreeType text for the canvas.
//
// Differences from the old text_render.h this replaces, all of them
// forced by the blueprint:
//
//  * Many faces, not one. style.css uses Space Grotesk at 400/500/600/
//    700 and JetBrains Mono at 400/500/600. A single bold serif atlas
//    cannot express that, and the mismatch was the most visible reason
//    the old UI didn't read as the same product.
//  * Many sizes. The blueprint spans 12px keycaps to 50px page
//    headings; one atlas per (face, size) is created up front.
//  * UTF-8 with a fallback face, because real strings in the design
//    contain -, →, ○ and ✕, none of which are ASCII.
//  * CSS line-box positioning. ui_text_baseline_for_box() reproduces
//    the browser's half-leading maths so a value like "top: 64px" in
//    the blueprint lands on the same scanline here. Verified against
//    the blueprint screenshots: Set up's h1 baseline falls at y=113 in
//    both.
//  * Draws into UiCanvas with real coverage blending instead of
//    SDL_RenderCopy of a tinted atlas.

#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "ui_canvas.h"

typedef struct UiFont UiFont;

// pixelSize is the CSS font-size in px (the em size), not a cell height.
UiFont *ui_font_load(const char *path, int pixelSize);
void ui_font_destroy(UiFont *f);

// Any codepoint missing from `f` is looked up in `fallback` instead.
// Used to give the Space Grotesk faces access to JetBrains Mono's ○
// and ✕, exactly the way the browser falls back for those two.
void ui_font_set_fallback(UiFont *f, UiFont *fallback);

float ui_font_ascent(const UiFont *f);
float ui_font_descent(const UiFont *f);      // positive number of px below the baseline
float ui_font_line_normal(const UiFont *f);  // line-height: normal, in px

// Baseline for a line box whose top edge is at boxTop. Pass
// lineHeight <= 0 for `line-height: normal`; otherwise pass the used
// line-height in px (e.g. 1.6 * 16 for .card-desc).
float ui_text_baseline_for_box(const UiFont *f, float boxTop, float lineHeight);

float ui_text_width(UiFont *f, const char *utf8);

// x is the pen start; baselineY is the baseline.
void ui_text_draw(UiCanvas *c, UiFont *f, float x, float baselineY, const char *utf8, UiColor color);

// Convenience: draws with the line-box semantics above, returning the
// advance width. Most call sites in the screens use this so the
// numbers in the code are the same numbers that appear in style.css.
float ui_text_draw_box(UiCanvas *c, UiFont *f, float x, float boxTop, float lineHeight,
                        const char *utf8, UiColor color);

#endif // UI_TEXT_H
