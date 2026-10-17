// ui_text.h -- FreeType text for the canvas.
// More: docs/code-comments.md (companion-app/source/ui_text.h:1)

#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "ui_canvas.h"

typedef struct UiFont UiFont;

// pixelSize is the CSS font-size in px (the em size), not a cell height.
UiFont *ui_font_load(const char *path, int pixelSize);
void ui_font_destroy(UiFont *f);

// Any codepoint missing from `f` is looked up in `fallback` instead.
// More: docs/code-comments.md (companion-app/source/ui_text.h:33)
void ui_font_set_fallback(UiFont *f, UiFont *fallback);

float ui_font_ascent(const UiFont *f);
float ui_font_descent(const UiFont *f);      // positive number of px below the baseline
float ui_font_line_normal(const UiFont *f);  // line-height: normal, in px

// Baseline for a line box whose top edge is at boxTop.
// More: docs/code-comments.md (companion-app/source/ui_text.h:42)
float ui_text_baseline_for_box(const UiFont *f, float boxTop, float lineHeight);

float ui_text_width(UiFont *f, const char *utf8);

// x is the pen start; baselineY is the baseline.
void ui_text_draw(UiCanvas *c, UiFont *f, float x, float baselineY, const char *utf8, UiColor color);

// Convenience: draws with the line-box semantics above, returning the advance width.
// More: docs/code-comments.md (companion-app/source/ui_text.h:52)
float ui_text_draw_box(UiCanvas *c, UiFont *f, float x, float boxTop, float lineHeight,
                        const char *utf8, UiColor color);

#endif // UI_TEXT_H
