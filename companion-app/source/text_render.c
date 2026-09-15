// text_render.c -- see text_render.h for why this exists (SDL2_ttf was
// never confirmed to link in this SDK; SceFreeType is).
//
// Real FreeType API used here, matching the upstream FreeType 2 docs:
// FT_Init_FreeType / FT_New_Face / FT_Set_Pixel_Sizes / FT_Load_Char
// (with FT_LOAD_RENDER, which rasterizes in the same call) /
// FT_Done_Face / FT_Done_FreeType. Nothing PS4-specific here --
// SceFreeType is Sony's packaged build of stock FreeType2, same API.

#include "text_render.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <orbis/Sysmodule.h>

// --- Workaround for this SDK snapshot's freetype2 headers ---
// The headers here expose the FreeType typedefs/constants (FT_Library,
// FT_Face, FT_GlyphSlot, FT_LOAD_RENDER, ...) and even declare
// FT_Set_Pixel_Sizes, but leave the five base-API functions below
// undeclared -- even though the compiled libSceFreeType.so we already
// link (-lSceFreeType) exports the real, standard FreeType symbols
// under these exact names. Redeclaring them here with FreeType's
// stable public signatures is harmless (a duplicate extern decl isn't
// an error in C) and lets the linker resolve them as normal.
FT_Error FT_Init_FreeType(FT_Library *alibrary);
FT_Error FT_Done_FreeType(FT_Library library);
FT_Error FT_New_Memory_Face(FT_Library library, const FT_Byte *file_base, FT_Long file_size, FT_Long face_index, FT_Face *aface);
FT_Error FT_Done_Face(FT_Face face);
FT_Error FT_Load_Char(FT_Face face, FT_ULong char_code, FT_Int32 load_flags);

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GLYPH_FIRST 32   // ' '
#define GLYPH_LAST  126  // '~'
#define GLYPH_COUNT (GLYPH_LAST - GLYPH_FIRST + 1)
#define ATLAS_COLUMNS 16
#define GLYPH_PADDING 2  // px of empty border kept around each packed glyph, avoids bleed between cells when the renderer samples at cell edges

typedef struct {
    SDL_Rect atlasRect; // where this glyph's pixels live in the atlas texture
    int bearingX;       // left offset from the pen position to the glyph's left edge
    int bearingY;       // distance from the text baseline up to the glyph's top edge
    int advance;         // how far to move the pen after drawing this glyph
} Glyph;

struct TextRenderer {
    SDL_Texture *atlas;
    Glyph glyphs[GLYPH_COUNT];
    int lineHeight;
};

// Reads the whole font file into memory so FT_New_Memory_Face can load
// it without needing stdio path semantics to match FreeType's
// expectations exactly -- matches the pattern the rest of this
// codebase already uses for reading /app0-relative files in config.c.
static uint8_t *read_whole_file(const char *path, long *outSize)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc((size_t)size);
    if (!buf) { fclose(f); return NULL; }
    size_t readBytes = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (readBytes != (size_t)size) { free(buf); return NULL; }
    *outSize = size;
    return buf;
}

TextRenderer *text_renderer_create(SDL_Renderer *renderer, const char *fontPath, int pixelHeight)
{
    // FreeType is a real sysmodule on PS4, separate from linking
    // -lSceFreeType at build time -- confirmed against OpenOrbis's own
    // samples/_common/graphics.cpp (Scene2D::Init), which loads this
    // exact module before its first FT_Init_FreeType call. Without
    // this, FT_Init_FreeType fails, text_renderer_create returns NULL,
    // and every draw_text() call becomes a silent no-op.
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0) {
        printf("[text] sceSysmoduleLoadModule(FREETYPE_OL) failed\n");
        return NULL;
    }

    long fontDataSize = 0;
    uint8_t *fontData = read_whole_file(fontPath, &fontDataSize);
    if (!fontData) {
        printf("[text] failed to read font file: %s\n", fontPath);
        return NULL;
    }

    FT_Library ft;
    if (FT_Init_FreeType(&ft) != 0) {
        printf("[text] FT_Init_FreeType failed\n");
        free(fontData);
        return NULL;
    }

    FT_Face face;
    // FT_New_Memory_Face keeps fontData alive for the lifetime of the
    // face -- freed further down once the atlas is baked and the face
    // is no longer needed.
    if (FT_New_Memory_Face(ft, fontData, (FT_Long)fontDataSize, 0, &face) != 0) {
        printf("[text] FT_New_Memory_Face failed for: %s\n", fontPath);
        FT_Done_FreeType(ft);
        free(fontData);
        return NULL;
    }
    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)pixelHeight);

    // First pass: rasterize every glyph and track the largest cell size
    // needed so the atlas grid can use a uniform cell.
    int cellW = 0, cellH = 0;
    for (int c = GLYPH_FIRST; c <= GLYPH_LAST; c++) {
        if (FT_Load_Char(face, (FT_ULong)c, FT_LOAD_RENDER) != 0) continue;
        FT_GlyphSlot g = face->glyph;
        if ((int)g->bitmap.width > cellW) cellW = (int)g->bitmap.width;
        if ((int)g->bitmap.rows > cellH) cellH = (int)g->bitmap.rows;
    }
    cellW += GLYPH_PADDING * 2;
    cellH += GLYPH_PADDING * 2;
    if (cellW <= 0 || cellH <= 0) {
        printf("[text] font produced no rasterizable glyphs: %s\n", fontPath);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(fontData);
        return NULL;
    }

    int atlasRows = (GLYPH_COUNT + ATLAS_COLUMNS - 1) / ATLAS_COLUMNS;
    int atlasW = ATLAS_COLUMNS * cellW;
    int atlasH = atlasRows * cellH;

    // ARGB8888 surface: RGB channels stay white, alpha carries the
    // glyph's actual AA coverage. Texture color-mod tints this per
    // draw call so one atlas serves every label color in main.c.
    SDL_Surface *atlasSurface = SDL_CreateRGBSurfaceWithFormat(0, atlasW, atlasH, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!atlasSurface) {
        printf("[text] SDL_CreateRGBSurfaceWithFormat failed: %s\n", SDL_GetError());
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(fontData);
        return NULL;
    }
    SDL_LockSurface(atlasSurface);
    memset(atlasSurface->pixels, 0, (size_t)atlasSurface->pitch * atlasSurface->h); // fully transparent to start

    TextRenderer *tr = (TextRenderer *)calloc(1, sizeof(TextRenderer));
    tr->lineHeight = (int)(face->size->metrics.height >> 6);
    int ascent = (int)(face->size->metrics.ascender >> 6);

    for (int c = GLYPH_FIRST; c <= GLYPH_LAST; c++) {
        int idx = c - GLYPH_FIRST;
        int col = idx % ATLAS_COLUMNS;
        int row = idx / ATLAS_COLUMNS;
        int cellX = col * cellW;
        int cellY = row * cellH;

        if (FT_Load_Char(face, (FT_ULong)c, FT_LOAD_RENDER) != 0) {
            tr->glyphs[idx].atlasRect = (SDL_Rect){0, 0, 0, 0};
            continue;
        }
        FT_GlyphSlot g = face->glyph;
        int gw = (int)g->bitmap.width;
        int gh = (int)g->bitmap.rows;

        int destX = cellX + GLYPH_PADDING;
        int destY = cellY + GLYPH_PADDING;

        // Copy FreeType's 8bpp grayscale bitmap into the ARGB atlas,
        // pixel row by pixel row -- FreeType's pitch can differ from
        // width (it's allowed to pad rows), so index with pitch, not width.
        uint32_t *atlasPixels = (uint32_t *)atlasSurface->pixels;
        int atlasPixelsPerRow = atlasSurface->pitch / 4;
        for (int row_ = 0; row_ < gh; row_++) {
            for (int col_ = 0; col_ < gw; col_++) {
                uint8_t alpha = g->bitmap.buffer[row_ * g->bitmap.pitch + col_];
                int px = destX + col_;
                int py = destY + row_;
                atlasPixels[py * atlasPixelsPerRow + px] = ((uint32_t)alpha << 24) | 0x00FFFFFFu; // white, alpha = coverage
            }
        }

        tr->glyphs[idx].atlasRect = (SDL_Rect){destX, destY, gw, gh};
        tr->glyphs[idx].bearingX = g->bitmap_left;
        tr->glyphs[idx].bearingY = g->bitmap_top;
        tr->glyphs[idx].advance = (int)(g->advance.x >> 6);
    }

    SDL_UnlockSurface(atlasSurface);

    tr->atlas = SDL_CreateTextureFromSurface(renderer, atlasSurface);
    SDL_FreeSurface(atlasSurface);
    if (!tr->atlas) {
        printf("[text] SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        free(tr);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(fontData);
        return NULL;
    }
    SDL_SetTextureBlendMode(tr->atlas, SDL_BLENDMODE_BLEND);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    free(fontData);

    // tr->lineHeight is repurposed here to hold ascent (top-of-line to
    // baseline distance in px), which is what text_renderer_draw and
    // text_renderer_measure actually need -- the font's full line
    // height (ascent+descent+gap) isn't used anywhere in this UI since
    // main.c positions each line manually.
    tr->lineHeight = ascent;
    return tr;
}

static const Glyph *glyph_for(TextRenderer *tr, char c)
{
    if (c < GLYPH_FIRST || c > GLYPH_LAST) return NULL;
    return &tr->glyphs[c - GLYPH_FIRST];
}

void text_renderer_draw(SDL_Renderer *renderer, TextRenderer *tr, int x, int y, const char *text, SDL_Color color)
{
    if (!tr || !text) return;

    SDL_SetTextureColorMod(tr->atlas, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(tr->atlas, color.a);

    int penX = x;
    int baselineY = y + tr->lineHeight; // tr->lineHeight holds ascent, i.e. top-of-line to baseline distance

    for (const char *p = text; *p; p++) {
        const Glyph *g = glyph_for(tr, *p);
        if (!g || g->atlasRect.w == 0) {
            // Unrenderable / space-like character: still advance the
            // pen using the font's average advance so layout doesn't
            // collapse. Fall back to a quarter of lineHeight if we
            // have nothing better.
            penX += tr->lineHeight / 3;
            continue;
        }
        SDL_Rect dst = {
            penX + g->bearingX,
            baselineY - g->bearingY,
            g->atlasRect.w,
            g->atlasRect.h
        };
        SDL_RenderCopy(renderer, tr->atlas, &g->atlasRect, &dst);
        penX += g->advance;
    }
}

int text_renderer_measure(TextRenderer *tr, const char *text)
{
    if (!tr || !text) return 0;
    int width = 0;
    for (const char *p = text; *p; p++) {
        const Glyph *g = glyph_for(tr, *p);
        width += g ? g->advance : (tr->lineHeight / 3);
    }
    return width;
}

void text_renderer_destroy(TextRenderer *tr)
{
    if (!tr) return;
    if (tr->atlas) SDL_DestroyTexture(tr->atlas);
    free(tr);
}