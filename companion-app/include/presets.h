// presets.h -- Game / Movie presets for the Customization screen.
// More: docs/code-comments.md (companion-app/include/presets.h:1)

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

// the live preset set (module state) Call once after settings_load().
// More: docs/code-comments.md (companion-app/include/presets.h:39)
void presets_init(AmbientConfig *cfg, const char *path);
int  preset_active(void);
void preset_select(AmbientConfig *cfg, int id);
void preset_reset_active(AmbientConfig *cfg);                    // back to shipped values for the preset you're in

// Two-press confirm for reset, so a stray X press can't wipe a tuned
// preset. Any other input should call preset_reset_disarm().
bool preset_reset_is_armed(void);
void preset_reset_arm(void);
void preset_reset_disarm(void);

// Stashes cfg into the active slot, then writes the whole ini in one pass: setup values in their own...
// More: docs/code-comments.md (companion-app/include/presets.h:56)
bool presets_save(const AmbientConfig *cfg, const char *path);

#endif // PRESETS_H
