// text_render.h -- minimal bitmap-font text renderer built directly on
// SceFreeType (real FreeType API: ft2build.h / FT_FREETYPE_H), not
// SDL2_ttf.
//
// Why this exists: SDL2_ttf was never actually linkable in this SDK
// snapshot (header only, no compiled lib -- see build.bat's original
// comment). SceFreeType *is* confirmed present (both freetype.h and
// libSceFreeType.so), and it's already linked via -lSceFreeType in
// build.bat. So instead of depending on a library that doesn't exist
// in this SDK, this renders text straight from FreeType:
//
//   1. At startup, rasterize the printable ASCII range (32..126) once
//      into a single texture atlas (each glyph's real FreeType
//      bitmap, packed into a grid).
//   2. Every draw_text() call after that is just SDL_RenderCopy() of
//      sub-rects from that atlas -- cheap enough for a 30fps UI with
//      ~10-20 lines of text per frame, and it works fine against the
//      software renderer this app already uses (SDL_CreateTextureFromSurface
//      + SDL_RenderCopy both work on a software-backed renderer; only
//      SDL_RenderPresent is the no-op here, and text draws never call it).
//
// Color is applied via SDL_SetTextureColorMod/AlphaMod on the shared
// atlas texture per draw call, not baked into the atlas -- so the same
// atlas serves the white/yellow/gray labels already used throughout
// main.c.

#ifndef TEXT_RENDER_H
#define TEXT_RENDER_H

#include <stdbool.h>
#include <SDL2/SDL.h>

typedef struct TextRenderer TextRenderer;

// Loads fontPath (expects the app0-relative path used everywhere else
// in this codebase, e.g. "/app0/assets/fonts/font.ttf"), rasterizes
// the atlas at pixelHeight, and returns a renderer ready for
// text_renderer_draw(). Returns NULL on any failure (missing font,
// FreeType init failure, etc.) -- callers should treat that the same
// way the old always-NULL TTF_Font* was treated (draw calls become
// silent no-ops), not crash.
TextRenderer *text_renderer_create(SDL_Renderer *renderer, const char *fontPath, int pixelHeight);

// Draws text with (x, y) as the top-left corner of the line, matching
// every existing draw_text() call site in main.c. Safe to call with
// tr == NULL (no-op), so callers don't need to null-check before every
// draw.
void text_renderer_draw(SDL_Renderer *renderer, TextRenderer *tr, int x, int y, const char *text, SDL_Color color);

// Width in pixels text would occupy if drawn -- useful for right-aligning
// or centering. Returns 0 if tr is NULL.
int text_renderer_measure(TextRenderer *tr, const char *text);

void text_renderer_destroy(TextRenderer *tr);

#endif // TEXT_RENDER_H
