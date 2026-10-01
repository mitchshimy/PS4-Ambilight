// Run from companion-app/. Build: gcc -I. -Iinclude -I../plugin/include -o /tmp/test_presets tests/test_presets.c source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c
#include "presets.h"
#include "config.h"
#include "default_ini.h"   // the ini the plugin writes on first run, from plugin/include
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define P "/tmp/test_presets.ini"

static const MenuItem *item(const char *key)
{
    for (int i = 0; i < kMenuItemCount; i++) if (!strcmp(kMenuItems[i].key, key)) return &kMenuItems[i];
    assert(!"item not found"); return NULL;
}

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc((size_t)n + 1);
    size_t got = fread(b, 1, (size_t)n, f); b[got] = 0; fclose(f);
    return b;
}
static void spit(const char *path, const char *text) { FILE *f = fopen(path, "w"); assert(f); fputs(text, f); fclose(f); }
// True if `[section]` in `text` contains the line `key = ...` (the app's writer format).
static bool has_key(const char *text, const char *section, const char *key)
{
    char hdr[64]; snprintf(hdr, sizeof(hdr), "[%s]\n", section);
    const char *s = strstr(text, hdr);
    if (!s) return false;
    s += strlen(hdr);
    const char *end = strstr(s, "\n[");
    char pat[64]; snprintf(pat, sizeof(pat), "\n%s = ", key);
    char *sec = strndup(s, end ? (size_t)(end - s) : strlen(s));
    char *withNl = (char *)malloc(strlen(sec) + 3); sprintf(withNl, "\n%s", sec);
    bool found = strstr(withNl, pat) != NULL;
    free(sec); free(withNl);
    return found;
}

int main(void)
{
    assert(preset_field_count() <= PRESET_MAX_FIELDS);

    // ---- shipped values, checked as the numbers a person sees on screen ----
    PresetValues g, m;
    preset_factory(PRESET_GAME, &g);
    preset_factory(PRESET_MOVIE, &m);
    AmbientConfig cg, cm;
    settings_set_defaults(&cg); settings_set_defaults(&cm);
    preset_apply(&cg, &g); preset_apply(&cm, &m);

    assert(cg.scanDepth == 2 && cm.scanDepth == 4);
    assert(cg.brightness == 255 && cm.brightness == 204);
    assert(settings_to_display(item("brightness"), (int32_t)cm.brightness) == 80);      // shows "80%"
    assert(cg.saturation == 35 && cm.saturation == 25);
    assert(!strcmp(kGammaNames[cg.gammaLutIndex], "2.2") && !strcmp(kGammaNames[cm.gammaLutIndex], "2.4"));
    assert(cg.blackLevel == 0 && cm.blackLevel == 1);
    assert(cg.smoothingEnabled == 1 && cg.settlingTimeMs == 50);
    assert(cm.smoothingEnabled == 1 && cm.settlingTimeMs == 200);
    // Left at the plugin default in both, on purpose.
    assert(cg.autoLetterboxEnabled == 1 && cm.autoLetterboxEnabled == 1);
    assert(cg.autoLetterboxThreshold == 18 && cm.autoLetterboxThreshold == 18);
    assert(cg.whiteLevel == 100 && cm.whiteLevel == 100);
    assert(cg.contrast == 0 && cm.contrast == 0 && cg.darkThreshold == 0 && cm.darkThreshold == 0);
    assert(cg.brightnessR == 100 && cg.brightnessG == 100 && cg.brightnessB == 100);
    assert(cm.gammaR == 100 && cm.gammaG == 100 && cm.gammaB == 100);
    printf("shipped preset values: PASSED\n");

    // ---- fresh install (no ini): Game is active with responsive smoothing ----
    remove(P);
    AmbientConfig cfg;
    assert(!settings_load(&cfg, P));
    settings_set_defaults(&cfg);
    presets_init(&cfg, P);
    assert(preset_active() == PRESET_GAME);
    assert(cfg.smoothingEnabled == 1 && cfg.settlingTimeMs == 50 && cfg.scanDepth == 2 && cfg.saturation == 35);
    printf("fresh install seeds Game: PASSED\n");

    // ---- editing then switching keeps each preset's own values ----
    cfg.brightness = 200;                       // edit while in Game
    preset_select(&cfg, PRESET_MOVIE);
    assert(cfg.settlingTimeMs == 200 && cfg.brightness == 204 && cfg.scanDepth == 4);   // Movie untouched by the Game edit
    cfg.settlingTimeMs = 250;                   // edit while in Movie
    preset_select(&cfg, PRESET_GAME);
    assert(cfg.settlingTimeMs == 50 && cfg.brightness == 200);    // Game kept its own
    preset_select(&cfg, PRESET_MOVIE);
    assert(cfg.settlingTimeMs == 250);                            // Movie's unsaved edit survived
    printf("per-preset values survive switching: PASSED\n");

    // ---- save, reload: presets only in their sections, both restored ----
    cfg.wledPort = 4321; strcpy(cfg.wledHost, "10.0.0.5");
    assert(presets_save(&cfg, P));
    {
        char *t = slurp(P);
        assert(has_key(t, "presets", "active") && has_key(t, "preset_game", "settling_time_ms") && has_key(t, "preset_movie", "scan_depth"));
        assert(strstr(t, "active = movie"));
        // No second copy of any preset value in the old sections.
        assert(!has_key(t, "layout", "scan_depth") && !has_key(t, "color", "brightness") && !has_key(t, "timing", "settling_time_ms"));
        assert(has_key(t, "color", "color_order") && has_key(t, "timing", "update_frequency_hz") && has_key(t, "layout", "led_count_top"));
        free(t);
    }
    AmbientConfig r;
    assert(settings_load(&r, P));
    presets_init(&r, P);
    assert(preset_active() == PRESET_MOVIE && r.settlingTimeMs == 250);
    preset_select(&r, PRESET_GAME);
    assert(r.settlingTimeMs == 50 && r.brightness == 200 && r.smoothingEnabled == 1);
    assert(r.wledPort == 4321 && strcmp(r.wledHost, "10.0.0.5") == 0);   // setup never touched by presets
    printf("save/load round trip, no duplicated keys: PASSED\n");

    // ---- selecting a preset must not alter Setup values ----
    AmbientConfig before = r;
    preset_select(&r, PRESET_MOVIE);
    assert(r.ledCountTop == before.ledCountTop && r.colorOrder == before.colorOrder &&
           r.updateFrequencyHz == before.updateFrequencyHz && r.wledPort == before.wledPort);
    printf("presets leave Setup values alone: PASSED\n");

    // ---- reset only resets the preset you're in ----
    preset_select(&r, PRESET_GAME);
    r.brightness = 100;
    preset_reset_arm(); assert(preset_reset_is_armed());
    preset_reset_active(&r);
    assert(!preset_reset_is_armed());
    assert(r.brightness == 255 && r.settlingTimeMs == 50 && r.saturation == 35);
    preset_select(&r, PRESET_MOVIE);
    assert(r.settlingTimeMs == 250);            // Movie's saved tweak still there
    preset_reset_active(&r);
    assert(r.settlingTimeMs == 200 && r.brightness == 204 && r.blackLevel == 1);
    printf("reset is per-preset: PASSED\n");

    // ---- ini written before presets existed: tuned values become Game, Movie inherits them ----
    FILE *f = fopen(P, "w");
    fprintf(f, "[color]\nbrightness=180\nsaturation=20\ngamma=2.2\n[timing]\nsmoothing_enabled=false\nsettling_time_ms=100\n[dev]\ndev_ip=1.2.3.4\n");
    fclose(f);
    assert(settings_load(&r, P));
    presets_init(&r, P);
    assert(preset_active() == PRESET_GAME);
    assert(r.brightness == 180 && r.smoothingEnabled == 0 && r.settlingTimeMs == 100);   // nothing changed for them
    preset_select(&r, PRESET_MOVIE);
    assert(r.brightness == 180 && r.saturation == 20 && r.gammaLutIndex == 4);           // calibration carried over
    assert(r.smoothingEnabled == 1 && r.settlingTimeMs == 200);                          // Movie's shipped smoothing, nothing else
    printf("migration keeps existing tuning: PASSED\n");

    // ---- untouched pre-preset ini seeds the shipped presets ----
    f = fopen(P, "w"); fprintf(f, "[network]\nwled_host=1.2.3.4\n"); fclose(f);
    assert(settings_load(&r, P));
    presets_init(&r, P);
    assert(r.smoothingEnabled == 1 && r.settlingTimeMs == 50 && r.gammaLutIndex == 4);
    printf("untouched ini seeds shipped presets: PASSED\n");

    // ---- once preset sections exist, leftover flat values are ignored (as the plugin does) ----
    spit(P, "[timing]\nsettling_time_ms = 999\nsmoothing_enabled = false\n[color]\nbrightness = 10\n"
            "[presets]\nactive = game\n[preset_game]\nsettling_time_ms = 60\nbrightness = 250\nsmoothing_enabled = true\n");
    assert(settings_load(&r, P)); presets_init(&r, P);
    assert(r.settlingTimeMs == 60 && r.brightness == 250 && r.smoothingEnabled == 1);
    printf("flat leftovers ignored once presets exist: PASSED\n");

    // ---- other sections survive a preset save ----
    f = fopen(P, "a"); fprintf(f, "[dev]\ndev_ip=9.9.9.9\n"); fclose(f);
    assert(settings_load(&r, P)); presets_init(&r, P);
    assert(presets_save(&r, P));
    ini_table_s *t = ini_table_create(); assert(ini_table_read_from_file(t, P));
    assert(ini_table_get_entry(t, "dev", "dev_ip") && !strcmp(ini_table_get_entry(t, "dev", "dev_ip"), "9.9.9.9"));
    assert(ini_table_check_entry(t, "preset_movie", "gamma") && ini_table_check_entry(t, "presets", "active"));
    ini_table_destroy(t);
    printf("unrelated sections preserved: PASSED\n");

    // ---- out-of-range values in a preset section are clamped ----
    f = fopen(P, "w"); fprintf(f, "[presets]\nactive=movie\n[preset_movie]\nbrightness=9999\nsettling_time_ms=-5\ngamma=9.9\n"); fclose(f);
    assert(settings_load(&r, P)); presets_init(&r, P);
    assert(preset_active() == PRESET_MOVIE);
    preset_select(&r, PRESET_GAME); preset_select(&r, PRESET_MOVIE);
    assert(r.brightness <= 255 && r.settlingTimeMs <= 5000);
    (void)item;
    printf("out-of-range values clamped: PASSED\n");

    // ---- first save from an ini that predates presets: the flat copies go ----
    spit(P, "[network]\nwled_host = 1.2.3.4\n[layout]\n; Sample radius per zone.\nscan_depth = 3\nled_count_top = 60\n"
            "; How many consecutive detections must agree.\nauto_letterbox_stability_frames = 3\n"
            "auto_letterbox_check_interval_frames = 15\n"
            "[color]\n; Global brightness scale.\nbrightness = 180\n; Match your strip's wiring.\ncolor_order = GRB\n"
            "[timing]\nupdate_frequency_hz = 60\nsmoothing_enabled = false\nsettling_time_ms = 100\n");
    assert(settings_load(&r, P)); presets_init(&r, P);
    assert(r.scanDepth == 3 && r.brightness == 180 && r.settlingTimeMs == 100);      // their tuning is Game
    assert(presets_save(&r, P));
    {
        char *t = slurp(P);
        assert(!has_key(t, "layout", "scan_depth") && !has_key(t, "color", "brightness") && !has_key(t, "timing", "settling_time_ms"));
        assert(!strstr(t, ";"));                                                                  // the app's reader drops comments, so a save never leaves stray ones
        assert(has_key(t, "color", "color_order") && strstr(t, "color_order = GRB"));
        assert(has_key(t, "layout", "led_count_top") && strstr(t, "led_count_top = 60"));
        assert(!has_key(t, "layout", "auto_letterbox_stability_frames") && !has_key(t, "layout", "auto_letterbox_check_interval_frames"));
        assert(strstr(t, "scan_depth = 3") && strstr(t, "brightness = 180"));                     // ...and they live in [preset_game]
        free(t);
    }
    printf("first save migrates an old ini to the concise layout: PASSED\n");

    // ---- the hidden letterbox timings are kept when someone tuned them ----
    spit(P, "[layout]\nauto_letterbox_stability_frames = 9\n");
    assert(settings_load(&r, P)); presets_init(&r, P);
    assert(r.autoLetterboxStabilityFrames == 9);
    assert(presets_save(&r, P));
    {
        char *t = slurp(P);
        assert(strstr(t, "auto_letterbox_stability_frames = 9"));
        assert(!has_key(t, "layout", "auto_letterbox_check_interval_frames"));
        free(t);
    }
    printf("tuned hidden keys survive, defaults are omitted: PASSED\n");

    // ---- the ini the plugin ships/generates and the app's shipped presets say the same thing ----
    {
        char *file = slurp("../plugin/config/ps4_ambient_light.ini");
        assert(file && "run from companion-app/ so ../plugin/config/ is reachable");
        // Same text, ignoring line endings a Windows checkout may have changed.
        char *a = strdup(file), *w = a; for (char *q = file; *q; q++) if (*q != '\r') *w++ = *q; *w = 0;
        assert(strcmp(a, AMBIENT_DEFAULT_INI) == 0);
        free(a); free(file);

        spit(P, AMBIENT_DEFAULT_INI);
        assert(settings_load(&r, P)); presets_init(&r, P);
        assert(preset_active() == PRESET_GAME);
        PresetValues shown, fg, fm;
        preset_capture(&r, &shown); preset_factory(PRESET_GAME, &fg); preset_factory(PRESET_MOVIE, &fm);
        assert(memcmp(&shown, &fg, sizeof(shown)) == 0);            // plugin's [preset_game] == app's Game
        preset_select(&r, PRESET_MOVIE); preset_capture(&r, &shown);
        assert(memcmp(&shown, &fm, sizeof(shown)) == 0);            // plugin's [preset_movie] == app's Movie
        assert(r.colorOrder == ORDER_RGB && r.updateFrequencyHz == 30 && r.configReloadCheckSeconds == 2);
    }
    printf("plugin's default ini matches the app's shipped presets: PASSED\n");

    // ---- golden file: what the app writes for a fresh setup, byte for byte ----
    // tools/test_plugin_config.c loads the same file through the plugin's real
    // loader, so the app's writer and the plugin's reader can't drift apart.
    {
        remove(P);
        AmbientConfig g;
        settings_set_defaults(&g);
        strcpy(g.wledHost, "192.168.2.110");
        presets_init(&g, P);
        assert(presets_save(&g, P));
        char *got = slurp(P), *want = slurp("../tools/data/preset_ini_golden.ini");
        if (!want) { spit("../tools/data/preset_ini_golden.ini", got); printf("(wrote missing golden file)\n"); want = strdup(got); }
        if (strcmp(got, want) != 0) { spit("/tmp/preset_ini_actual.ini", got); printf("golden mismatch, actual saved to /tmp/preset_ini_actual.ini\n"); }
        assert(strcmp(got, want) == 0);
        free(got); free(want);
    }
    printf("app output matches the golden ini: PASSED\n");

    // ---- hand-added [compat] keys (v3.9 gpu_only_remap, v3.9.1 report_file) survive the app saving ----
    // A tester adds report_file=1 by hand; opening the app and saving must not drop it.
    {
        FILE *cf = fopen(P, "w");
        assert(cf);
        fputs("[network]\nwled_host=10.0.0.5\n\n[compat]\ngpu_only_remap=1\nreport_file=1\n", cf);
        fclose(cf);
        AmbientConfig c;
        assert(settings_load(&c, P));
        presets_init(&c, P);
        assert(presets_save(&c, P));
        char *t = slurp(P);
        assert(has_key(t, "compat", "report_file") && strstr(t, "report_file = 1"));
        assert(has_key(t, "compat", "gpu_only_remap") && strstr(t, "gpu_only_remap = 1"));
        free(t);
    }
    printf("hand-added [compat] keys survive a save: PASSED\n");

    printf("ALL PRESET CHECKS PASSED\n");
    return 0;
}
