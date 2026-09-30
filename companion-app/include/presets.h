// presets.h -- Game / Movie presets for the Customization screen.
//
// A preset is a snapshot of every Customization-screen value (colour,
// smoothing, letterbox, sampling, RGB balance...). Setup values (WLED
// address, LED layout, colour order, update rate) are never part of a
// preset.
//
// HOW IT'S STORED: the ini holds [presets] active=game|movie and the full set of
// each preset's values in [preset_game] / [preset_movie] (same key names and value
// formats the plugin has always used). The plugin (v3.8+) reads the active preset's
// section itself, so those keys are NOT also written to [layout] / [color] / [timing]
// -- those sections keep only setup values (LED layout, colour order, update rate...).
// Loading an ini from before presets still works: its flat values become Game.
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
// presets existed, and leaves `cfg` showing the active preset. Once an ini has
// preset sections they are the only source: leftover flat values are ignored,
// exactly as the plugin ignores them.
void presets_init(AmbientConfig *cfg, const char *path);
int  preset_active(void);
void preset_select(AmbientConfig *cfg, int id);
void preset_reset_active(AmbientConfig *cfg);                    // back to shipped values for the preset you're in

// Two-press confirm for reset, so a stray X press can't wipe a tuned
// preset. Any other input should call preset_reset_disarm().
bool preset_reset_is_armed(void);
void preset_reset_arm(void);
void preset_reset_disarm(void);

// Stashes cfg into the active slot, then writes the whole ini in one pass: setup
// values in their own sections, [presets], and both preset sections, merged into
// what's already at `path` so anything else in the file survives. This replaces
// settings_save() for the app; it removes the old flat copies of preset values.
bool presets_save(const AmbientConfig *cfg, const char *path);

#endif // PRESETS_H
