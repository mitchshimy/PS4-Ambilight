// ui_theme.c -- loads the two families the blueprint imports.
//
// style.css pulls Space Grotesk 400/500/600/700 and JetBrains Mono
// 400/500/600/700 from Google Fonts. Those exact families are bundled
// under assets/fonts (Space Grotesk instanced from the upstream
// variable font at each weight), so the app renders in the same
// typefaces the design was drawn in rather than substituting one
// bundled serif for all of them.

#include "ui_theme.h"

#include <stdio.h>
#include <string.h>

static UiFont *load(const char *dir, const char *file, int px)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    UiFont *f = ui_font_load(path, px);
    if (!f) printf("[theme] failed to load %s @%dpx\n", path, px);
    return f;
}

#define SG_R "SpaceGrotesk-Regular.ttf"
#define SG_M "SpaceGrotesk-Medium.ttf"
#define SG_S "SpaceGrotesk-SemiBold.ttf"
#define SG_B "SpaceGrotesk-Bold.ttf"
#define JB_R "JetBrainsMono-Regular.ttf"
#define JB_M "JetBrainsMono-Medium.ttf"
#define JB_S "JetBrainsMono-SemiBold.ttf"

bool ui_fonts_load(UiFonts *o, const char *dir)
{
    memset(o, 0, sizeof(*o));

    o->h1         = load(dir, SG_B, 50);
    o->pageSub    = load(dir, SG_R, 19);
    o->cardTitle  = load(dir, SG_B, 24);
    o->cardDesc   = load(dir, SG_R, 16);
    o->fieldLabel = load(dir, SG_S, 14);
    o->navPill    = load(dir, SG_S, 20);
    o->cta        = load(dir, SG_S, 22);
    o->btn        = load(dir, SG_S, 19);
    o->hint       = load(dir, SG_M, 14);
    o->note       = load(dir, SG_R, 14);

    o->monoInput  = load(dir, JB_M, 20);
    o->monoPill   = load(dir, JB_M, 18);
    o->monoToggle = load(dir, JB_S, 18);
    o->monoTotal  = load(dir, JB_S, 15);
    o->monoBanner = load(dir, JB_R, 15);
    o->monoStatus = load(dir, JB_R, 14);
    o->monoKeycap = load(dir, JB_M, 12);

    // Space Grotesk has no glyph for the circle/cross used in the
    // controller hints ("Press ○ to go back", the ✕ keycap). The
    // browser silently falls back for those two characters; do the
    // same rather than dropping them.
    ui_font_set_fallback(o->note, o->monoStatus);
    ui_font_set_fallback(o->hint, o->monoStatus);
    ui_font_set_fallback(o->cta, o->monoInput);
    ui_font_set_fallback(o->btn, o->monoInput);
    ui_font_set_fallback(o->cardDesc, o->monoStatus);
    ui_font_set_fallback(o->fieldLabel, o->monoKeycap);
    ui_font_set_fallback(o->pageSub, o->monoInput);

    return o->h1 && o->cardDesc && o->monoInput;
}

void ui_fonts_destroy(UiFonts *f)
{
    UiFont **all = (UiFont **)f;
    int n = (int)(sizeof(UiFonts) / sizeof(UiFont *));
    for (int i = 0; i < n; i++) {
        if (all[i]) { ui_font_destroy(all[i]); all[i] = NULL; }
    }
}
