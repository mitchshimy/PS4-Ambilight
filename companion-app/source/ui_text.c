// ui_text.c -- see ui_text.h.
//
// FreeType usage is deliberately kept to the same tiny API surface the
// previous text_render.c proved linkable against this SDK's
// libSceFreeType (FT_Init_FreeType / FT_New_Memory_Face /
// FT_Set_Pixel_Sizes / FT_Load_Char / FT_Done_Face /
// FT_Done_FreeType), plus FT_Get_Char_Index, which is needed to tell
// "this face has no glyph for U+25CB" from "this face rendered a
// blank" so the fallback chain can work at all.

#include "ui_text.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef UI_TEXT_DESKTOP_HARNESS
// This must be unconditional in the real build. An earlier pass gated
// it behind `#ifdef __ORBIS__` to let the desktop verification harness
// (which has no orbis/* headers) compile -- but __ORBIS__ is not a
// macro this toolchain defines anywhere (grep the whole codebase and
// build.bat: it appears nowhere else), so on the real OpenOrbis clang
// invocation that guard was always false. That silently compiled out
// both this include and the sceSysmoduleLoadModule() call below on
// every real build, while main.c's own unconditional orbis/* includes
// were unaffected -- which is exactly why this went undetected until a
// real crash log: the file still compiled and linked cleanly with the
// FreeType sysmodule never actually loaded, so the first real
// FT_Init_FreeType call (confirmed by reversing its PS4 NID against
// the crash log's "Required Function NID") faulted as
// PRX_NOT_RESOLVED_FUNCTION -- the same failure mode, for the same
// reason, as the ImeDialog bug this project already solved once.
// UI_TEXT_DESKTOP_HARNESS is instead defined explicitly, only by the
// desktop harness's own build command, so the default here -- what a
// plain build.bat invocation sees -- is the real, production include.
#include <orbis/Sysmodule.h>
#endif

// Desktop-harness stand-in for the one orbis call this file makes, so
// harness builds (which link no orbis libraries at all) still resolve
// the symbol; real builds never see this, since UI_TEXT_DESKTOP_HARNESS
// is never defined by build.bat.
#ifdef UI_TEXT_DESKTOP_HARNESS
static int sceSysmoduleLoadModule(unsigned int id) { (void)id; return 0; }
#define ORBIS_SYSMODULE_FREETYPE_OL 0
#endif

// --- Workaround for this SDK snapshot's freetype2 headers ---
// Carried over verbatim in spirit from the old text_render.c: the
// headers expose the types and constants but leave these base-API
// functions undeclared, even though libSceFreeType exports them under
// exactly these names. Redeclaring with FreeType's stable public
// signatures is harmless on a desktop build too (a duplicate extern
// declaration is not an error in C).
//
// FT_Get_Name_Index is deliberately NOT in this list. A real
// OpenOrbis build (clang, not this file's desktop gcc build) failed
// with "conflicting types for 'FT_Get_Name_Index'" -- this SDK's
// freetype.h, unlike the functions below, already declares it via
// FT_EXPORT(), whose visibility attribute a plain extern redeclaration
// doesn't match. Call it through the real declaration that
// FT_FREETYPE_H already brings in instead of redeclaring it here.
FT_Error FT_Init_FreeType(FT_Library *alibrary);
FT_Error FT_Done_FreeType(FT_Library library);
FT_Error FT_New_Memory_Face(FT_Library library, const FT_Byte *file_base, FT_Long file_size, FT_Long face_index, FT_Face *aface);
FT_Error FT_Done_Face(FT_Face face);
FT_Error FT_Load_Char(FT_Face face, FT_ULong char_code, FT_Int32 load_flags);
FT_UInt  FT_Get_Char_Index(FT_Face face, FT_ULong charcode);
FT_Error FT_Load_Glyph(FT_Face face, FT_UInt glyph_index, FT_Int32 load_flags);
FT_Error FT_Get_Kerning(FT_Face face, FT_UInt left, FT_UInt right, FT_UInt kern_mode, FT_Vector *akerning);

// Standard Latin f-ligatures. Chrome applies the `liga` feature by
// default, so "offset" is drawn with a single f_f glyph that is ~3px
// narrower at 16px than two f's. Without this, every word after an
// "ff" on a line sits a few pixels right of where the blueprint puts
// it. Only these five are needed for the design's copy; anything not
// present in the face falls back to the plain glyph sequence.
typedef struct { const char *seq; const char *glyphName; } Ligature;
static const Ligature kLigatures[] = {
    { "ffi", "f_f_i" },
    { "ffl", "f_f_l" },
    { "ff",  "f_f"   },
    { "fi",  "fi"    },
    { "fl",  "fl"    },
};
#define LIGATURE_COUNT ((int)(sizeof(kLigatures) / sizeof(kLigatures[0])))
// Ligature results are cached under synthetic codepoints so they can
// live in the same glyph cache as everything else.
#define LIGATURE_CP_BASE 0x110000u

#define ASCII_FIRST 32
#define ASCII_LAST  126
#define ASCII_COUNT (ASCII_LAST - ASCII_FIRST + 1)
#define EXTRA_MAX   48   // non-ASCII codepoints cached on demand

typedef struct {
    uint8_t *bitmap;   // 8-bit coverage, w*h, may be NULL for blanks
    int w, h;
    int left, top;      // FreeType bearings
    float advance;
    FT_UInt index;      // kept so FT_Get_Kerning can be applied between glyphs
    bool loaded;
    bool present;       // face actually has a glyph for this codepoint
} UiGlyph;

struct UiFont {
    FT_Library ft;
    FT_Face face;
    uint8_t *fileData;   // FT_New_Memory_Face keeps this alive for the face's lifetime

    int pixelSize;
    float ascent, descent, lineNormal;

    UiGlyph ascii[ASCII_COUNT];
    uint32_t extraCp[EXTRA_MAX];
    UiGlyph  extra[EXTRA_MAX];
    int extraCount;

    UiFont *fallback;
    bool ligatures;       // proportional faces only; a monospace face must not re-width
    FT_UInt ligIndex[LIGATURE_COUNT];
};

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
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) { free(buf); return NULL; }
    *outSize = size;
    return buf;
}

UiFont *ui_font_load(const char *path, int pixelSize)
{
    // FreeType is a separate runtime sysmodule on PS4 -- linking
    // -lSceFreeType is not enough. Confirmed against OpenOrbis's own
    // samples/_common/graphics.cpp, which loads this before its first
    // FT_Init_FreeType call. Loading twice (once per ui_font_load
    // call, one per bundled font) is harmless -- it's the documented
    // idempotent pattern for this API.
    //
    // This call, and the check on its result, must never be skipped:
    // a version of this file that compiled it out on the real target
    // (via an unverified __ORBIS__ guard meant only for the desktop
    // harness) shipped once already and crashed on console with
    // PRX_NOT_RESOLVED_FUNCTION on the very first FT_Init_FreeType
    // call below, because that call's import was never resolved
    // against a loaded module. If this load ever genuinely fails,
    // bail out here rather than let that happen again -- draw calls
    // against a NULL UiFont* are already safe no-ops everywhere else
    // in this file, so returning NULL degrades to "no text", not a
    // crash.
    int sysmoduleRet = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);
    if (sysmoduleRet < 0) {
        printf("[text] sceSysmoduleLoadModule(FREETYPE_OL) failed: 0x%08x\n", sysmoduleRet);
        return NULL;
    }

    long size = 0;
    uint8_t *data = read_whole_file(path, &size);
    if (!data) { printf("[text] cannot read font: %s\n", path); return NULL; }

    UiFont *f = (UiFont *)calloc(1, sizeof(UiFont));
    if (!f) { free(data); return NULL; }

    int ftRet = FT_Init_FreeType(&f->ft);
    if (ftRet != 0) {
        printf("[text] FT_Init_FreeType failed: %d\n", ftRet);
        free(data); free(f); return NULL;
    }
    int faceRet = FT_New_Memory_Face(f->ft, data, (FT_Long)size, 0, &f->face);
    if (faceRet != 0) {
        printf("[text] FT_New_Memory_Face failed (%d): %s\n", faceRet, path);
        FT_Done_FreeType(f->ft); free(data); free(f); return NULL;
    }
    FT_Set_Pixel_Sizes(f->face, 0, (FT_UInt)pixelSize);

    f->fileData = data;
    f->pixelSize = pixelSize;

    // Metrics are computed from the face's design values rather than
    // face->size->metrics, which FreeType rounds to whole pixels. That
    // rounding is enough to move a baseline by a pixel at these sizes,
    // and every vertical position in the screens is derived from these
    // numbers.
    float upem = (float)f->face->units_per_EM;
    if (upem <= 0.0f) upem = 1000.0f;
    float scale = (float)pixelSize / upem;
    f->ascent = (float)f->face->ascender * scale;
    f->descent = -(float)f->face->descender * scale;
    f->lineNormal = (float)(f->face->ascender - f->face->descender) * scale;

    // Monospace faces (JetBrains Mono here) must keep every advance
    // identical, so shaping is left off for them.
    f->ligatures = (f->face->face_flags & FT_FACE_FLAG_FIXED_WIDTH) == 0;
    if (f->ligatures) {
        for (int i = 0; i < LIGATURE_COUNT; i++)
            f->ligIndex[i] = FT_Get_Name_Index(f->face, (FT_String *)kLigatures[i].glyphName);
    }

    return f;
}

void ui_font_set_fallback(UiFont *f, UiFont *fb) { if (f) f->fallback = fb; }

void ui_font_destroy(UiFont *f)
{
    if (!f) return;
    for (int i = 0; i < ASCII_COUNT; i++) free(f->ascii[i].bitmap);
    for (int i = 0; i < f->extraCount; i++) free(f->extra[i].bitmap);
    FT_Done_Face(f->face);
    FT_Done_FreeType(f->ft);
    free(f->fileData);
    free(f);
}

float ui_font_ascent(const UiFont *f) { return f ? f->ascent : 0.0f; }
float ui_font_descent(const UiFont *f) { return f ? f->descent : 0.0f; }
float ui_font_line_normal(const UiFont *f) { return f ? f->lineNormal : 0.0f; }

float ui_text_baseline_for_box(const UiFont *f, float boxTop, float lineHeight)
{
    if (!f) return boxTop;
    float used = (lineHeight > 0.0f) ? lineHeight : f->lineNormal;
    // CSS half-leading: the content area (ascent+descent) is centred
    // inside the used line-height.
    float halfLeading = (used - (f->ascent + f->descent)) * 0.5f;
    return boxTop + halfLeading + f->ascent;
}

static void rasterise_into(UiFont *f, UiGlyph *g, uint32_t cp)
{
    g->loaded = true;

    if (cp >= LIGATURE_CP_BASE) {
        int li = (int)(cp - LIGATURE_CP_BASE);
        FT_UInt gi = (li >= 0 && li < LIGATURE_COUNT) ? f->ligIndex[li] : 0;
        if (gi == 0) { g->present = false; return; }
        if (FT_Load_Glyph(f->face, gi, FT_LOAD_RENDER | FT_LOAD_NO_HINTING) != 0) { g->present = false; return; }
        g->present = true;
        g->index = gi;
        goto copy_bitmap;
    }

    g->index = FT_Get_Char_Index(f->face, (FT_ULong)cp);
    g->present = (g->index != 0);
    if (!g->present) return;

    // FT_LOAD_NO_HINTING matters here. With hinting on, FreeType
    // snaps stems to the pixel grid and rounds each advance, which
    // makes a long line of text drift several pixels away from where
    // the browser puts it by the end of the line. Chrome lays this
    // design out with unhinted, linearly-scaled advances; matching
    // that keeps wrap points and line widths identical.
    if (FT_Load_Char(f->face, (FT_ULong)cp, FT_LOAD_RENDER | FT_LOAD_NO_HINTING) != 0) { g->present = false; return; }

copy_bitmap:;
    FT_GlyphSlot slot = f->face->glyph;
    int w = (int)slot->bitmap.width, h = (int)slot->bitmap.rows;
    g->w = w; g->h = h;
    g->left = slot->bitmap_left;
    g->top = slot->bitmap_top;
    // linearHoriAdvance is the unrounded 16.16 advance; advance.x has
    // already been rounded to 1/64 px.
    g->advance = (slot->linearHoriAdvance != 0)
               ? (float)slot->linearHoriAdvance / 65536.0f
               : (float)slot->advance.x / 64.0f;
    if (w > 0 && h > 0) {
        g->bitmap = (uint8_t *)malloc((size_t)w * h);
        if (g->bitmap) {
            for (int y = 0; y < h; y++)
                memcpy(&g->bitmap[(size_t)y * w], &slot->bitmap.buffer[y * slot->bitmap.pitch], (size_t)w);
        }
    }
}

// Returns the glyph and, via outOwner, the font whose metrics it came
// from (which may be a fallback).
static const UiGlyph *glyph_for(UiFont *f, uint32_t cp, UiFont **outOwner)
{
    if (!f) return NULL;

    UiGlyph *g = NULL;
    if (cp >= ASCII_FIRST && cp <= ASCII_LAST) {
        g = &f->ascii[cp - ASCII_FIRST];
    } else {
        for (int i = 0; i < f->extraCount; i++) {
            if (f->extraCp[i] == cp) { g = &f->extra[i]; break; }
        }
        if (!g) {
            if (f->extraCount >= EXTRA_MAX) return NULL;
            g = &f->extra[f->extraCount];
            f->extraCp[f->extraCount] = cp;
            f->extraCount++;
        }
    }
    if (!g->loaded) rasterise_into(f, g, cp);
    if (g->present) { if (outOwner) *outOwner = f; return g; }

    if (f->fallback) return glyph_for(f->fallback, cp, outOwner);
    return NULL;
}

// Minimal UTF-8 decoder. Returns the codepoint and advances *p.
static uint32_t utf8_next(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    uint32_t cp;
    if (s[0] < 0x80) { cp = s[0]; *p += 1; }
    else if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F); *p += 2;
    } else if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F); *p += 3;
    } else if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12)
           | ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F); *p += 4;
    } else { cp = s[0]; *p += 1; }
    return cp;
}

// Consumes a ligature at *p if one applies, returning its synthetic
// codepoint, or 0 if the next character should be handled normally.
// Kerning. The bundled Space Grotesk faces carry a `kern` table
// generated from their own GPOS pair positioning, restricted to the
// glyphs this UI can draw, because FreeType only reads the legacy
// table and the upstream font ships kerning in GPOS only. It is not
// cosmetic: unkerned, the Screen sampling description measures
// 1301.6px against a 1300px max-width and wraps a word earlier than
// the blueprint does.
static float kern_between(UiFont *f, FT_UInt prev, FT_UInt cur)
{
    if (!prev || !cur) return 0.0f;
    FT_Vector d;
    if (FT_Get_Kerning(f->face, prev, cur, 1 /* FT_KERNING_UNFITTED */, &d) != 0) return 0.0f;
    return (float)d.x / 64.0f;
}

static uint32_t take_ligature(UiFont *f, const char **p)
{
    if (!f->ligatures || (unsigned char)**p >= 0x80) return 0;
    for (int i = 0; i < LIGATURE_COUNT; i++) {
        if (f->ligIndex[i] == 0) continue;
        size_t n = strlen(kLigatures[i].seq);
        if (strncmp(*p, kLigatures[i].seq, n) == 0) {
            UiFont *owner = f;
            uint32_t cp = LIGATURE_CP_BASE + (uint32_t)i;
            if (glyph_for(f, cp, &owner)) { *p += n; return cp; }
        }
    }
    return 0;
}

float ui_text_width(UiFont *f, const char *utf8)
{
    if (!f || !utf8) return 0.0f;
    float w = 0.0f;
    FT_UInt prev = 0;
    UiFont *prevOwner = NULL;
    const char *p = utf8;
    while (*p) {
        uint32_t cp = take_ligature(f, &p);
        if (!cp) cp = utf8_next(&p);
        UiFont *owner = f;
        const UiGlyph *g = glyph_for(f, cp, &owner);
        if (g) {
            if (prevOwner == owner) w += kern_between(owner, prev, g->index);
            w += g->advance;
            prev = g->index; prevOwner = owner;
        } else {
            w += f->pixelSize * 0.33f;
            prev = 0; prevOwner = NULL;
        }
    }
    return w;
}

void ui_text_draw(UiCanvas *c, UiFont *f, float x, float baselineY, const char *utf8, UiColor color)
{
    if (!c || !f || !utf8 || color.a == 0) return;

    float pen = x;
    FT_UInt prev = 0;
    UiFont *prevOwner = NULL;
    const char *p = utf8;
    while (*p) {
        uint32_t cp = take_ligature(f, &p);
        if (!cp) cp = utf8_next(&p);
        UiFont *owner = f;
        const UiGlyph *g = glyph_for(f, cp, &owner);
        if (!g) { pen += f->pixelSize * 0.33f; prev = 0; prevOwner = NULL; continue; }
        if (prevOwner == owner) pen += kern_between(owner, prev, g->index);
        prev = g->index; prevOwner = owner;

        if (g->bitmap) {
            int ox = (int)floorf(pen + (float)g->left + 0.5f);
            int oy = (int)floorf(baselineY + 0.5f) - g->top;
            for (int gy = 0; gy < g->h; gy++) {
                int py = oy + gy;
                if (py < c->clipY0 || py >= c->clipY1) continue;
                const uint8_t *row = &g->bitmap[(size_t)gy * g->w];
                for (int gx = 0; gx < g->w; gx++) {
                    uint8_t a = row[gx];
                    if (!a) continue;
                    ui_blend_pixel(c, ox + gx, py, color, a / 255.0f);
                }
            }
        }
        pen += g->advance;
    }
}

float ui_text_draw_box(UiCanvas *c, UiFont *f, float x, float boxTop, float lineHeight,
                        const char *utf8, UiColor color)
{
    float baseline = ui_text_baseline_for_box(f, boxTop, lineHeight);
    ui_text_draw(c, f, x, baseline, utf8, color);
    return ui_text_width(f, utf8);
}
