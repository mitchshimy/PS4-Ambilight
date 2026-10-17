// text_render.h -- minimal bitmap-font text renderer built directly on SceFreeType (real FreeType API...
// More: docs/code-comments.md (companion-app/source/text_render.h:1)

#ifndef TEXT_RENDER_H
#define TEXT_RENDER_H

#include <stdbool.h>
#include <SDL2/SDL.h>

typedef struct TextRenderer TextRenderer;

// Loads fontPath (expects the app0-relative path used everywhere else in this codebase, e.g....
// More: docs/code-comments.md (companion-app/source/text_render.h:35)
TextRenderer *text_renderer_create(SDL_Renderer *renderer, const char *fontPath, int pixelHeight);

// Draws text with (x, y) as the top-left corner of the line, matching every existing draw_text() call...
// More: docs/code-comments.md (companion-app/source/text_render.h:44)
void text_renderer_draw(SDL_Renderer *renderer, TextRenderer *tr, int x, int y, const char *text, SDL_Color color);

// Width in pixels text would occupy if drawn -- useful for right-aligning
// or centering. Returns 0 if tr is NULL.
int text_renderer_measure(TextRenderer *tr, const char *text);

void text_renderer_destroy(TextRenderer *tr);

#endif // TEXT_RENDER_H
