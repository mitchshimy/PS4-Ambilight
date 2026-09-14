#ifndef SETTINGS_H
#define SETTINGS_H

#include "color_pipeline.h"
#include <stdbool.h>
#include <stddef.h>

// Field types for the generic, data-driven settings menu. One
// MenuItem per ini key the real plugin reads (verified against
// ps4_ambient_light's actual ini_table_get_entry* call sites, not
// recalled from memory -- see settings.c's kMenuItems table).
typedef enum {
    FIELD_U32,     // plain uint32_t, adjustable by step within [min,max]
    FIELD_I32,     // plain int32_t, same
    FIELD_U16,     // wledPort specifically
    FIELD_BOOL,    // smoothingEnabled
    FIELD_STRING,  // wledHost -- edited via on-screen keyboard (see ime.h)
    FIELD_ENUM,    // startCorner / direction / colorOrder -- cycled via a string list
} FieldType;

typedef struct {
    const char *label;        // shown in the UI
    const char *section;      // ini [section]
    const char *key;          // ini key
    FieldType type;
    size_t offset;             // offsetof(AmbientConfig, field)
    int32_t min, max, step;    // ignored for STRING/BOOL/ENUM
    const char **enumNames;    // FIELD_ENUM only -- display strings, index-matched to the enum's own values
    int enumCount;
} MenuItem;

// The full settings menu, data-driven so the UI doesn't need one
// hand-written render/input case per field. Defined in settings.c.
extern const MenuItem kMenuItems[];
extern const int kMenuItemCount;

// Fills cfg with the plugin's own compile-time defaults (copied
// verbatim from AmbientConfig's real initializer in main.c) BEFORE
// settings_load overlays whatever the ini file actually contains --
// same "defaults first, then overlay only what's present" behavior as
// the plugin's own ambient_load_config, so a field missing from an
// edited-down ini behaves identically here and in the real plugin.
void settings_set_defaults(AmbientConfig *cfg);

// Reads path into cfg, overlaying settings_set_defaults(). Returns
// false if the file doesn't exist or can't be parsed -- caller should
// still proceed with defaults in that case, matching the plugin's own
// "keep defaults on parse failure" behavior.
bool settings_load(AmbientConfig *cfg, const char *path);

// Writes cfg out to path using config.h's ini_table_write_to_file,
// preserving the exact section/key names the plugin reads. Does NOT
// attempt to preserve comments or key order from an existing file --
// this always produces a fresh, canonical layout. Returns false on
// write failure (path unwritable, etc.).
bool settings_save(const AmbientConfig *cfg, const char *path);

// Generic get/set through a MenuItem's offset -- used by the UI so
// navigation/adjustment code is written once, not per-field.
int32_t settings_get_i32(const AmbientConfig *cfg, const MenuItem *item);
void settings_set_i32(AmbientConfig *cfg, const MenuItem *item, int32_t value);

#endif // SETTINGS_H
