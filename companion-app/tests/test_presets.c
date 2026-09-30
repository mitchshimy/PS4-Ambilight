// Build: gcc -I include -o /tmp/test_presets tests/test_presets.c source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c
#include "presets.h"
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define P "/tmp/test_presets.ini"

static const MenuItem *item(const char *key)
{
    for (int i = 0; i < kMenuItemCount; i++) if (!strcmp(kMenuItems[i].key, key)) return &kMenuItems[i];
    assert(!"item not found"); return NULL;
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
    assert(cm.smoothingEnabled == 1 && cm.settlingTimeMs == 300);
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
    assert(cfg.settlingTimeMs == 300 && cfg.brightness == 204 && cfg.scanDepth == 4);   // Movie untouched by the Game edit
    cfg.settlingTimeMs = 250;                   // edit while in Movie
    preset_select(&cfg, PRESET_GAME);
    assert(cfg.settlingTimeMs == 50 && cfg.brightness == 200);    // Game kept its own
    preset_select(&cfg, PRESET_MOVIE);
    assert(cfg.settlingTimeMs == 250);                            // Movie's unsaved edit survived
    printf("per-preset values survive switching: PASSED\n");

    // ---- save, reload: flat keys = active preset, both presets restored ----
    cfg.wledPort = 4321; strcpy(cfg.wledHost, "10.0.0.5");
    assert(settings_save(&cfg, P));
    assert(presets_save(&cfg, P));
    AmbientConfig r;
    assert(settings_load(&r, P));
    assert(r.settlingTimeMs == 250);            // plugin-visible flat key = active (Movie)
    presets_init(&r, P);
    assert(preset_active() == PRESET_MOVIE);
    preset_select(&r, PRESET_GAME);
    assert(r.settlingTimeMs == 50 && r.brightness == 200 && r.smoothingEnabled == 1);
    assert(r.wledPort == 4321 && strcmp(r.wledHost, "10.0.0.5") == 0);   // setup never touched by presets
    printf("save/load round trip: PASSED\n");

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
    assert(r.settlingTimeMs == 300 && r.brightness == 204 && r.blackLevel == 1);
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
    assert(r.smoothingEnabled == 1 && r.settlingTimeMs == 300);                          // Movie's shipped smoothing, nothing else
    printf("migration keeps existing tuning: PASSED\n");

    // ---- untouched pre-preset ini seeds the shipped presets ----
    f = fopen(P, "w"); fprintf(f, "[network]\nwled_host=1.2.3.4\n"); fclose(f);
    assert(settings_load(&r, P));
    presets_init(&r, P);
    assert(r.smoothingEnabled == 1 && r.settlingTimeMs == 50 && r.gammaLutIndex == 4);
    printf("untouched ini seeds shipped presets: PASSED\n");

    // ---- hand-edited flat key beats a stale preset section for the active preset ----
    assert(settings_load(&r, P)); presets_init(&r, P);
    assert(settings_save(&r, P) && presets_save(&r, P));
    f = fopen(P, "r"); char text[8192]; size_t n = fread(text, 1, sizeof(text) - 1, f); text[n] = 0; fclose(f);
    char *p = strstr(text, "settling_time_ms = 50");  // flat [timing] copy is first
    assert(p); p[strlen("settling_time_ms = ")] = '7'; p[strlen("settling_time_ms = ") + 1] = '5';   // -> 75
    f = fopen(P, "w"); fputs(text, f); fclose(f);
    assert(settings_load(&r, P)); presets_init(&r, P);
    assert(r.settlingTimeMs == 75);
    preset_select(&r, PRESET_MOVIE); preset_select(&r, PRESET_GAME);
    assert(r.settlingTimeMs == 75);
    printf("hand-edited flat keys win: PASSED\n");

    // ---- other sections survive a preset save ----
    f = fopen(P, "a"); fprintf(f, "[dev]\ndev_ip=9.9.9.9\n"); fclose(f);
    assert(settings_load(&r, P)); presets_init(&r, P);
    assert(settings_save(&r, P) && presets_save(&r, P));
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

    printf("ALL PRESET CHECKS PASSED\n");
    return 0;
}
