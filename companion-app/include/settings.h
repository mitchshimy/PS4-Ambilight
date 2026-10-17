#ifndef SETTINGS_H
#define SETTINGS_H

#include "color_pipeline.h"
#include "config.h"
#include <stdbool.h>
#include <stddef.h>

// Field types for the generic, data-driven settings menu.
// More: docs/code-comments.md (companion-app/include/settings.h:9)
typedef enum {
    FIELD_U32,     // plain uint32_t, adjustable by step within [min,max]
    FIELD_I32,     // plain int32_t, same
    FIELD_U16,     // wledPort specifically
    FIELD_BOOL,    // smoothingEnabled
    FIELD_STRING,  // wledHost -- edited via on-screen keyboard (see ime.h)
    FIELD_ENUM,    // startCorner / direction / colorOrder -- cycled via a string list
    FIELD_ACTION,  // a button, not a value (Customization's "reset preset") -- has no
                   // storage; get/set are no-ops and main.c's input code dispatches on `key`
} FieldType;

// Which of the two settings screens (Set Up / Customisation) an item belongs to -- see the comment...
// More: docs/code-comments.md (companion-app/include/settings.h:24)
typedef enum {
    MENU_SCREEN_SETUP,
    MENU_SCREEN_CUSTOMIZE,
} MenuScreen;

// How a numeric field is SHOWN and TYPED in the UI.
// More: docs/code-comments.md (companion-app/include/settings.h:32)
typedef enum {
    UNIT_RAW = 0,        // shown exactly as stored (counts, ms, Hz, ports...)
    UNIT_PCT_OF_MAX,     // stored 0..max shown as 0..100% (e.g. brightness 255 -> "100%").
                         // For these rows `step` is in PERCENT, not stored units.
    UNIT_PCT_DIRECT,     // stored value already IS a percent (100 = unchanged) -- just gets a "%"
    UNIT_PCT_SIGNED,     // stored value is a percent change around 0 -- shown "+20%" / "-10%" / "0%"
} DisplayUnit;

typedef struct {
    const char *label;        // shown in the UI
    const char *section;      // ini [section]
    const char *key;          // ini key
    FieldType type;
    size_t offset;             // offsetof(AmbientConfig, field)
    int32_t min, max, step;    // min/max are stored values; `step` is in display units (percent for UNIT_PCT_OF_MAX rows). ignored for BOOL/ENUM. For STRING, max is
                                // repurposed as the destination buffer's size in bytes (e.g. sizeof(cfg.wledHost)) -- see settings.c's...
                                // More: docs/code-comments.md (companion-app/include/settings.h:51)
    const char **enumNames;    // FIELD_ENUM only -- display strings, index-matched to the enum's own values
    int enumCount;
    MenuScreen screen;         // which settings screen shows this item
    const char *group;         // card heading on this screen -- items are
                                // expected to sit contiguously per group (see kMenuItems' own ordering); a run of items sharing the...
                                // More: docs/code-comments.md (companion-app/include/settings.h:61)
    const char *groupDesc;     // one-line description shown once, under
                                // the heading, on the first item of a new group -- NULL on every other item in that same group
                                // More: docs/code-comments.md (companion-app/include/settings.h:66)
    DisplayUnit unit;          // trailing + optional: rows that omit it are UNIT_RAW
} MenuItem;

// The full settings menu, data-driven so the UI doesn't need one
// hand-written render/input case per field. Defined in settings.c.
extern const MenuItem kMenuItems[];
extern const int kMenuItemCount;

// Fills cfg with the plugin's own compile-time defaults (copied verbatim from AmbientConfig's real...
// More: docs/code-comments.md (companion-app/include/settings.h:77)
void settings_set_defaults(AmbientConfig *cfg);

// Reads path into cfg, overlaying settings_set_defaults().
// More: docs/code-comments.md (companion-app/include/settings.h:85)
bool settings_load(AmbientConfig *cfg, const char *path);

// Writes cfg out to path using config.h's ini_table_write_to_file, preserving the exact section/key...
// More: docs/code-comments.md (companion-app/include/settings.h:91)
bool settings_save(const AmbientConfig *cfg, const char *path);

// True for the values a preset owns: every real row on the Customization screen (not the "ui" rows...
// More: docs/code-comments.md (companion-app/include/settings.h:98)
bool settings_item_is_preset_owned(const MenuItem *item);

// The text on a FIELD_ACTION button (what the UI draws inside it).
// More: docs/code-comments.md (companion-app/include/settings.h:103)
bool settings_action_text(const MenuItem *item, bool resetArmed, int presetIndex,
                          char *out, size_t outSize);

// Puts the setup keys (and, if `presetKeys`, the flat preset-owned keys too) into `table`, which...
// More: docs/code-comments.md (companion-app/include/settings.h:112)
void settings_fill_table(ini_table_s *table, const AmbientConfig *cfg, bool presetKeys);

// Generic get/set through a MenuItem's offset -- used by the UI so navigation/adjustment code is...
// More: docs/code-comments.md (companion-app/include/settings.h:121)
int32_t settings_to_display(const MenuItem *item, int32_t stored);
int32_t settings_from_display(const MenuItem *item, int32_t display); // rounds, then clamps into [min,max]
int32_t settings_display_min(const MenuItem *item);
int32_t settings_display_max(const MenuItem *item);
int32_t settings_display_step(const MenuItem *item); // one D-Pad/L1/R1 nudge, in display units
// Formats a display value ("100%", "+20%", "1234"). Returns dst.
char   *settings_format_display(const MenuItem *item, int32_t display, char *dst, size_t dstSize);
int32_t settings_get_i32(const AmbientConfig *cfg, const MenuItem *item);
void settings_set_i32(AmbientConfig *cfg, const MenuItem *item, int32_t value);

// v2.9: smoothing preset convenience (settings.c) -- see that file's comment above...
// More: docs/code-comments.md (companion-app/include/settings.h:136)
extern const char *kGammaNames[8];

#define SMOOTHING_PRESET_COUNT 4
extern const char *kSmoothingPresetNames[SMOOTHING_PRESET_COUNT]; // "Off","Responsive","Balanced","Smooth"
int smoothing_preset_index(const AmbientConfig *cfg);             // derives the current bucket from the two real fields
void smoothing_apply_preset(AmbientConfig *cfg, int presetIndex); // writes both real fields for the given bucket

#endif // SETTINGS_H
