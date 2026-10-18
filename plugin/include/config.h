#pragma once

#ifndef GH_LIB
#include <stdbool.h>
#include <stddef.h>
#endif

typedef struct ini_entry_s {
    char *key;
    char *value;
} ini_entry_s;

typedef struct ini_section_s {
    char *name;
    ini_entry_s *entry;
    int size;
} ini_section_s;

typedef struct ini_table_s {
    ini_section_s *section;
    int size;
} ini_table_s;

// @brief Creates an empty ini_table_s struct for writing new entries to. @return ini_table_s*
// More: docs/code-comments.md (plugin/include/config.h:23)
ini_table_s *ini_table_create();

// @brief Free up all the allocated resources in the ini_table_s struct. @param table
// More: docs/code-comments.md (plugin/include/config.h:29)
void ini_table_destroy(ini_table_s *table);

// @brief Creates an ini_table_s struct filled with data from the specified `file'.
// More: docs/code-comments.md (plugin/include/config.h:35)
bool ini_table_read_from_file(ini_table_s *table, const char *file);
bool ini_table_read_from_buffer(ini_table_s *table, const char *data, size_t len); // same parser, ini already in memory
bool ambient_config_read_table(ini_table_s *table); // settings.c: the plugin's ini as a table -- the copy being applied during a reload, else the file
void ambient_config_snapshot_set(const char *data, size_t len); // settings.c: while set, the loaders parse this copy instead of opening the ini; NULL clears

// @brief Writes the specified ini_table_s struct to the specified `file'.
// More: docs/code-comments.md (plugin/include/config.h:44)
bool ini_table_write_to_file(ini_table_s *table, const char *file);

// @brief Creates a new entry in the `table' containing the `key' and `value' provided if it does not...
// More: docs/code-comments.md (plugin/include/config.h:54)
void ini_table_create_entry(ini_table_s *table, const char *section_name, const char *key, const char *value);

// @brief Checks for the existance of an entry in the specified `table'.
// More: docs/code-comments.md (plugin/include/config.h:65)
bool ini_table_check_entry(ini_table_s *table, const char *section_name, const char *key);

// @brief Retrieves the unmodified value of the specified `key' in `section_name'.
// More: docs/code-comments.md (plugin/include/config.h:75)
const char *ini_table_get_entry(ini_table_s *table, const char *section_name, const char *key);

// @brief Retrieves the value of the specified `key' in `section_name', converted to int.
// More: docs/code-comments.md (plugin/include/config.h:86)
bool ini_table_get_entry_as_int(ini_table_s *table, const char *section_name, const char *key, int *value);

// @brief Retrieves the value of the specified `key' in `section_name', converted to bool.
// More: docs/code-comments.md (plugin/include/config.h:97)
bool ini_table_get_entry_as_bool(ini_table_s *table, const char *section_name, const char *key, bool *value);

// Ctn: make this non-static
ini_section_s *_ini_section_find(ini_table_s *table, const char *name);
