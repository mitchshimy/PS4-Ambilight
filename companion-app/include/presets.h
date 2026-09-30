// presets.h -- Game / Movie presets for the Customization screen.
//
// A preset is a snapshot of every Customization-screen value (colour,
// smoothing, letterbox, sampling, RGB balance...). Setup values (WLED
// address, LED layout, colour order, update rate) are never part of a
// preset.
//
// HOW IT STAYS COMPATIBLE WITH THE PLUGIN: the flat ini keys the plugin
// already reads ([color], [timing], [layout]) always hold the ACTIVE
// preset's values, so an unmodified plugin just runs whichever preset
// was last saved. The full set of both presets is also written to
// [preset_game] / [preset_movie] (same key names, same value formats as
// the flat keys) plus [presets] active=game|movie, ready for a plugin
// that wants to pick a preset per title.
//
// Editing a field edits the active preset -- there is no Custom mode.
// Switching presets stashes the current values into their slot first,
// so unsaved edits to one preset survive a trip to the other.

#ifndef PRESETS_H
#define PRESETS_H

#include "settings.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { PRESET_GAME = 0, PRESET_MOVIE = 1 } PresetId;
#define PRESET_COUNT 2
extern const char *kPresetNames[PRESET_COUNT];   // "Game", "Movie" (defined in settings.c, see the note there)

#define PRESET_MAX_FIELDS 32
typedef struct { int32_t v[PRESET_MAX_FIELDS]; } PresetValues;

// ---- pure helpers (no global state) ----
int  preset_field_count(void);                                   // how many Customization values a preset holds
void preset_capture(const AmbientConfig *cfg, PresetValues *out);
void preset_apply(AmbientConfig *cfg, const PresetValues *in);   // clamps to each field's range
void preset_factory(PresetId id, PresetValues *out);             // the shipped values (see presets.c for the table and why)

// ---- the live preset set (module state) ----
// Call once after settings_load(). Reads the preset sections from `path`
// (missing file / sections are fine), migrates an ini written before
// presets existed, and leaves `cfg` showing the active preset.
void presets_init(AmbientConfig *cfg, const char *path);
int  preset_active(void);
void preset_select(AmbientConfig *cfg, int id);
void preset_reset_active(AmbientConfig *cfg);                    // back to shipped values for the preset you're in

// Two-press confirm for reset, so a stray X press can't wipe a tuned
// preset. Any other input should call preset_reset_disarm().
bool preset_reset_is_armed(void);
void preset_reset_arm(void);
void preset_reset_disarm(void);

// Stashes cfg into the active slot, then merges [presets] and both
// preset sections into the ini at `path`. Call AFTER settings_save().
bool presets_save(const AmbientConfig *cfg, const char *path);

#endif // PRESETS_H
