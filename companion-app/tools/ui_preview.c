// ui_preview.c -- render the companion's real UI code to PNGs on a PC, so layout changes can be
// looked at without a console build. Same sources the PS4 build compiles; only main.c (SDL, pads,
// sockets) and the Orbis FreeType module load are left out.
//
//   tools/ui_preview.sh /tmp/ui     # builds this and writes /tmp/ui/*.png
//
// It does NOT prove the console looks identical (fonts are the same files, but this is a desktop
// FreeType), it proves the layout: what fits, what wraps, whether two banners match, where the
// button sits.
#include "ui_screens.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>

static void dump(UiCanvas *c, const char *dir, const char *name)
{
    char path[512]; snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", c->w, c->h);
    for (int i = 0; i < c->w * c->h; i++) {
        uint32_t p = c->px[i];
        fputc((p >> 16) & 255, f); fputc((p >> 8) & 255, f); fputc(p & 255, f);
    }
    fclose(f);
}

static void render(UiCanvas *c, UiFonts *fonts, AmbientConfig *cfg, UiState *st, const char *dir, const char *name)
{
    ui_canvas_clear(c, ui_rgb(0, 0, 0));
    ui_render(c, fonts, cfg, st);
    dump(c, dir, name);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    const char *fontDir = argc > 2 ? argv[2] : "assets/fonts";

    UiFonts fonts;
    if (!ui_fonts_load(&fonts, fontDir)) { fprintf(stderr, "fonts failed to load from %s\n", fontDir); return 1; }
    UiCanvas *c = ui_canvas_create(1920, 1080);

    AmbientConfig cfg; settings_set_defaults(&cfg);
    UiState st; memset(&st, 0, sizeof(st));
    st.screen = UI_SCREEN_HOME; st.setupComplete = true;
    snprintf(st.statusLine, sizeof(st.statusLine), "Ready.");

    // ---- Home: shell feed OFF (must look exactly like it did before) ----
    st.install = UI_INSTALL_OK;                                   render(c, &fonts, &cfg, &st, dir, "home_off_ok");
    st.install = UI_INSTALL_NONE;                                 render(c, &fonts, &cfg, &st, dir, "home_off_noplugin");

    // ---- Home: shell feed ON ----
    cfg.shellEnabled = 1;
    st.install = UI_INSTALL_OK;
    st.shell = shell_summarize(true, ASSET_PRESENT, ASSET_PRESENT);   cfg.shellAutoload = 1;  render(c, &fonts, &cfg, &st, dir, "home_on_all_ok");
    cfg.shellAutoload = 0;                                                                     render(c, &fonts, &cfg, &st, dir, "home_on_all_ok_noautoload");
    st.shell = shell_summarize(true, ASSET_MISSING, ASSET_MISSING);                            render(c, &fonts, &cfg, &st, dir, "home_on_none");
    st.shell = shell_summarize(true, ASSET_PRESENT, ASSET_MISSING);                            render(c, &fonts, &cfg, &st, dir, "home_on_loader_missing");
    st.shell = shell_summarize(true, ASSET_MISSING, ASSET_PRESENT);                            render(c, &fonts, &cfg, &st, dir, "home_on_prx_missing");
    st.shell = shell_summarize(true, ASSET_UPDATE, ASSET_UPDATE);                              render(c, &fonts, &cfg, &st, dir, "home_on_update_both");
    st.install = UI_INSTALL_NONE; st.shell = shell_summarize(true, ASSET_MISSING, ASSET_MISSING); render(c, &fonts, &cfg, &st, dir, "home_on_everything_missing");
    st.install = UI_INSTALL_UPDATE; st.shell = shell_summarize(true, ASSET_UPDATE, ASSET_PRESENT); render(c, &fonts, &cfg, &st, dir, "home_on_update_all");
    st.install = UI_INSTALL_DISABLED; st.shell = shell_summarize(true, ASSET_PRESENT, ASSET_PRESENT); render(c, &fonts, &cfg, &st, dir, "home_on_plugin_disabled");
    st.install = UI_INSTALL_OK; st.testRunning = true; st.homeFocus = HOME_FOCUS_CTA;         render(c, &fonts, &cfg, &st, dir, "home_on_testing");
    st.testRunning = false; st.homeFocus = 0;

    // ---- GoldHEN's plugin loader off: outranks every other state ----
    st.install = UI_INSTALL_OK; st.loader = LOADER_OFF;
    cfg.shellEnabled = 0; st.shell = shell_summarize(false, ASSET_PRESENT, ASSET_PRESENT);     render(c, &fonts, &cfg, &st, dir, "home_loader_off");
    cfg.shellEnabled = 1; st.shell = shell_summarize(true, ASSET_PRESENT, ASSET_PRESENT);      render(c, &fonts, &cfg, &st, dir, "home_loader_off_shell_on");
    st.install = UI_INSTALL_NONE;                                                              render(c, &fonts, &cfg, &st, dir, "home_loader_off_plugin_missing");
    st.loader = LOADER_UNKNOWN; st.install = UI_INSTALL_OK;

    // ---- Set up ----
    st.screen = UI_SCREEN_SETUP; st.scrollY = 0;
    render(c, &fonts, &cfg, &st, dir, "setup_top");
    int h = (int)ui_screen_content_height(&fonts, &cfg, UI_SCREEN_SETUP);
    printf("setup content height: %d\n", h);
    st.scrollY = 330.0f;                                                                       render(c, &fonts, &cfg, &st, dir, "setup_scrolled");

    // ---- Set up with a row hint, and Help ----
    st.scrollY = 330.0f;
    snprintf(st.settingsHint, sizeof(st.settingsHint), "Autoload needs the Shell feed: turn the Shell feed switch on first.");
    render(c, &fonts, &cfg, &st, dir, "setup_hint");
    st.settingsHint[0] = '\0';
    st.screen = UI_SCREEN_HELP; st.scrollY = 0;
    render(c, &fonts, &cfg, &st, dir, "help_top");
    st.scrollY = 700.0f;                                                                       render(c, &fonts, &cfg, &st, dir, "help_scrolled");

    ui_canvas_destroy(c); ui_fonts_destroy(&fonts);
    return 0;
}
