// presets.c -- see presets.h for the model.

#include "presets.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *kPresetSection[PRESET_COUNT] = { "preset_game", "preset_movie" };
static const char *kPresetIniName[PRESET_COUNT] = { "game", "movie" };

static PresetValues s_slot[PRESET_COUNT];
static int  s_active = PRESET_GAME;
static bool s_resetArmed = false;

// A preset holds every real Customization row -- see settings_item_is_preset_owned.
static bool is_preset_item(const MenuItem *it) { return settings_item_is_preset_owned(it); }

int preset_field_count(void)
{
    int n = 0;
    for (int i = 0; i < kMenuItemCount; i++) if (is_preset_item(&kMenuItems[i])) n++;
    return n;
}

void preset_capture(const AmbientConfig *cfg, PresetValues *out)
{
    memset(out, 0, sizeof(*out));
    int n = 0;
    for (int i = 0; i < kMenuItemCount && n < PRESET_MAX_FIELDS; i++)
        if (is_preset_item(&kMenuItems[i])) out->v[n++] = settings_get_i32(cfg, &kMenuItems[i]);
}

void preset_apply(AmbientConfig *cfg, const PresetValues *in)
{
    int n = 0;
    for (int i = 0; i < kMenuItemCount && n < PRESET_MAX_FIELDS; i++)
        if (is_preset_item(&kMenuItems[i])) settings_set_i32(cfg, &kMenuItems[i], in->v[n++]);
}

// Shipped values. Anything not listed keeps the plugin default, which is
// deliberate for: letterbox (on, 7% bar threshold -- already tuned), white
// level 100%, contrast 0 (its pivot sits on the post-gamma value, so any
// non-zero setting crushes darks), black threshold 0 (its fixed +10 LED-unit
// hysteresis blacks out dim scenes once gamma is on), and RGB balance /
// per-channel gamma 100% (they depend on the individual strip's white point,
// so they're set by eye).
//
//                       Game      Movie
//   edge depth           2 (5x5)   4 (9x9)   -- Movie has CPU to spare; Game runs inside the game's process
//   brightness           100%      80%       -- stored 255 / 204
//   saturation           +35%      +25%      -- Movie lower: skin tones dominate
//   gamma                2.2       2.4       -- game-mode vs cinema-mode display
//   black level          0%        1%        -- Movie: stops the strip toggling on grainy dark scenes
//   smoothing            on, 50ms  on, 200ms
//
// Gamma here stacks with WLED's own gamma if that's enabled for realtime
// data, so WLED's colour gamma should be off.
typedef struct { const char *key; int32_t value; } PresetDefault;

#define GAMMA_2_2 4   // indexes into kGammaNames
#define GAMMA_2_4 5

static const PresetDefault kGameDefaults[] = {
    { "scan_depth",        2 },
    { "brightness",        255 },
    { "saturation",        35 },
    { "gamma",             GAMMA_2_2 },
    { "black_level",       0 },
    { "smoothing_enabled", 1 },
    { "settling_time_ms",  50 },
};
static const PresetDefault kMovieDefaults[] = {
    { "scan_depth",        4 },
    { "brightness",        204 },
    { "saturation",        25 },
    { "gamma",             GAMMA_2_4 },
    { "black_level",       1 },
    { "smoothing_enabled", 1 },
    { "settling_time_ms",  200 },
};

static void apply_defaults(AmbientConfig *cfg, const PresetDefault *d, int n)
{
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < kMenuItemCount; k++) {
            if (!strcmp(kMenuItems[k].key, d[i].key)) { settings_set_i32(cfg, &kMenuItems[k], d[i].value); break; }
        }
    }
}

void preset_factory(PresetId id, PresetValues *out)
{
    AmbientConfig tmp;
    settings_set_defaults(&tmp);
    if (id == PRESET_MOVIE) apply_defaults(&tmp, kMovieDefaults, (int)(sizeof(kMovieDefaults) / sizeof(kMovieDefaults[0])));
    else                    apply_defaults(&tmp, kGameDefaults,  (int)(sizeof(kGameDefaults)  / sizeof(kGameDefaults[0])));
    preset_capture(&tmp, out);
}

// ---- ini side ----

static void format_value(const MenuItem *it, int32_t v, char *dst, size_t n)
{
    if (it->type == FIELD_BOOL)              snprintf(dst, n, "%s", v ? "true" : "false");
    else if (!strcmp(it->key, "gamma"))      snprintf(dst, n, "%s", kGammaNames[(v >= 0 && v < 8) ? v : 0]);
    else                                     snprintf(dst, n, "%d", (int)v);
}

static bool section_has_any(ini_table_s *t, const char *section)
{
    for (int i = 0; i < kMenuItemCount; i++)
        if (is_preset_item(&kMenuItems[i]) && ini_table_check_entry(t, section, kMenuItems[i].key)) return true;
    return false;
}

// Overlays whatever keys `section` has onto *v; keys it lacks keep v's value.
static void parse_section(ini_table_s *t, const char *section, PresetValues *v)
{
    int n = 0;
    for (int i = 0; i < kMenuItemCount && n < PRESET_MAX_FIELDS; i++) {
        const MenuItem *it = &kMenuItems[i];
        if (!is_preset_item(it)) continue;
        int idx = n++;
        int iv; bool bv; const char *s;
        int32_t val = v->v[idx];
        bool got = false;
        if (it->type == FIELD_BOOL) {
            if (ini_table_get_entry_as_bool(t, section, it->key, &bv)) { val = bv ? 1 : 0; got = true; }
        } else if (!strcmp(it->key, "gamma")) {
            if ((s = ini_table_get_entry(t, section, it->key)) != NULL)
                for (int g = 0; g < 8; g++) if (!strcmp(s, kGammaNames[g])) { val = g; got = true; break; }
        } else if (ini_table_get_entry_as_int(t, section, it->key, &iv)) {
            val = iv; got = true;
        }
        if (got) {
            if (val < it->min) val = it->min;
            if (val > it->max) val = it->max;
            v->v[idx] = val;
        }
    }
}

void presets_init(AmbientConfig *cfg, const char *path)
{
    memset(s_slot, 0, sizeof(s_slot));
    s_active = PRESET_GAME;
    s_resetArmed = false;

    // What settings_load left in cfg for the preset-owned fields: the old flat
    // keys from an ini that predates presets, or the plugin defaults if the
    // file has none. Only used to migrate an old ini (below).
    PresetValues flat;
    preset_capture(cfg, &flat);

    ini_table_s *t = ini_table_create();
    bool haveFile = (t != NULL) && ini_table_read_from_file(t, path);

    bool have[PRESET_COUNT] = { false, false };
    if (haveFile) {
        for (int p = 0; p < PRESET_COUNT; p++) have[p] = section_has_any(t, kPresetSection[p]);
        const char *a = ini_table_get_entry(t, "presets", "active");
        if (a && !strcmp(a, kPresetIniName[PRESET_MOVIE])) s_active = PRESET_MOVIE;
    }

    if (!have[PRESET_GAME] && !have[PRESET_MOVIE]) {
        // First run with presets (or no ini at all).
        AmbientConfig defs; PresetValues defVals;
        settings_set_defaults(&defs);
        preset_capture(&defs, &defVals);
        s_active = PRESET_GAME;

        if (memcmp(&flat, &defVals, sizeof(flat)) == 0) {
            // Nothing has ever been customised: start from the shipped presets.
            preset_factory(PRESET_GAME,  &s_slot[PRESET_GAME]);
            preset_factory(PRESET_MOVIE, &s_slot[PRESET_MOVIE]);
        } else {
            // An existing, tuned setup: it becomes Game exactly as it is, and
            // Movie starts as a copy of it with Movie's shipped smoothing -- so
            // nobody loses their colour calibration by updating.
            s_slot[PRESET_GAME] = flat;
            PresetValues movieFactory; AmbientConfig movieDefs, movie = *cfg;
            preset_factory(PRESET_MOVIE, &movieFactory);
            settings_set_defaults(&movieDefs);
            preset_apply(&movieDefs, &movieFactory);
            movie.smoothingEnabled = movieDefs.smoothingEnabled;
            movie.settlingTimeMs   = movieDefs.settlingTimeMs;
            preset_capture(&movie, &s_slot[PRESET_MOVIE]);
        }
    } else {
        // The ini has presets, and from here on the plugin reads ONLY those for
        // these values (any flat leftovers are ignored), so this must too. A
        // preset section that's missing entirely starts from its shipped values.
        for (int p = 0; p < PRESET_COUNT; p++) {
            preset_factory((PresetId)p, &s_slot[p]);
            if (have[p]) parse_section(t, kPresetSection[p], &s_slot[p]);
        }
    }
    if (t) ini_table_destroy(t);

    preset_apply(cfg, &s_slot[s_active]);   // cfg shows the active preset
}

int preset_active(void) { return s_active; }

void preset_select(AmbientConfig *cfg, int id)
{
    if (id < 0) id = 0;
    if (id >= PRESET_COUNT) id = PRESET_COUNT - 1;
    s_resetArmed = false;
    if (id == s_active) return;
    preset_capture(cfg, &s_slot[s_active]);   // keep unsaved edits to the one we're leaving
    s_active = id;
    preset_apply(cfg, &s_slot[s_active]);
}

void preset_reset_active(AmbientConfig *cfg)
{
    preset_factory((PresetId)s_active, &s_slot[s_active]);
    preset_apply(cfg, &s_slot[s_active]);
    s_resetArmed = false;
}

bool preset_reset_is_armed(void) { return s_resetArmed; }
void preset_reset_arm(void)      { s_resetArmed = true; }
void preset_reset_disarm(void)   { s_resetArmed = false; }

bool presets_save(const AmbientConfig *cfg, const char *path)
{
    preset_capture(cfg, &s_slot[s_active]);

    ini_table_s *t = ini_table_create();
    if (t == NULL) return false;
    ini_table_read_from_file(t, path);   // merge, so [dev], relay keys and comments survive

    // Setup values into their own sections; the preset-owned keys leave their old
    // flat sections (a file from before presets still has them there).
    settings_fill_table(t, cfg, false);

    ini_table_create_entry(t, "presets", "active", kPresetIniName[s_active]);
    for (int p = 0; p < PRESET_COUNT; p++) {
        int n = 0;
        for (int i = 0; i < kMenuItemCount && n < PRESET_MAX_FIELDS; i++) {
            const MenuItem *it = &kMenuItems[i];
            if (!is_preset_item(it)) continue;
            char buf[16];
            format_value(it, s_slot[p].v[n++], buf, sizeof(buf));
            ini_table_create_entry(t, kPresetSection[p], it->key, buf);
        }
    }
    bool ok = ini_table_write_to_file(t, path);
    ini_table_destroy(t);
    return ok;
}
