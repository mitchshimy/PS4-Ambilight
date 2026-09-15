#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <orbis/libkernel.h>

#include "plugin_common.h"
#define _atoi atoi

/// https://github.com/Teklad/tconfig > https://github.com/gimli2/tconfig
#include "config.h"

static ini_entry_s *_ini_entry_create(ini_section_s *section, const char *key, const char *value) {
    if ((section->size % 10) == 0) {
        section->entry = (ini_entry_s *)realloc(section->entry, (10 + section->size) * sizeof(ini_entry_s));
    }
    ini_entry_s *entry = &section->entry[section->size++];
    entry->key = (char *)malloc((strlen(key) + 1) * sizeof(char));
    entry->value = (char *)malloc((strlen(value) + 1) * sizeof(char));
    debug_printf("key: %s = value: %s\n", key, value);
    strcpy(entry->key, key);
    strcpy(entry->value, value);
    return entry;
}

static ini_section_s *_ini_section_create(ini_table_s *table, const char *section_name) {
    if ((table->size % 10) == 0) {
        table->section = (ini_section_s *)realloc(table->section, (10 + table->size) * sizeof(ini_section_s));
    }
    ini_section_s *section = &table->section[table->size++];
    section->size = 0;
    section->name = (char *)malloc((strlen(section_name) + 1) * sizeof(char));
    strcpy(section->name, section_name);
    section->entry = (ini_entry_s *)malloc(10 * sizeof(ini_entry_s));
    return section;
}

// Ctn: make this non-static
ini_section_s *_ini_section_find(ini_table_s *table, const char *name) {
    for (int i = 0; i < table->size; i++) {
        if (strcmp(table->section[i].name, name) == 0) {
            return &table->section[i];
        }
    }
    return NULL;
}

static ini_entry_s *_ini_entry_find(ini_section_s *section, const char *key) {
    for (int i = 0; i < section->size; i++) {
        if (strcmp(section->entry[i].key, key) == 0) {
            return &section->entry[i];
        }
    }
    return NULL;
}

static ini_entry_s *_ini_entry_get(ini_table_s *table, const char *section_name, const char *key) {
    ini_section_s *section = _ini_section_find(table, section_name);
    if (section == NULL) {
        return NULL;
    }

    ini_entry_s *entry = _ini_entry_find(section, key);
    if (entry == NULL) {
        return NULL;
    }
    return entry;
}

ini_table_s *ini_table_create() {
    ini_table_s *table = (ini_table_s *)malloc(sizeof(ini_table_s));
    table->size = 0;
    table->section = (ini_section_s *)malloc(10 * sizeof(ini_section_s));
    return table;
}

void ini_table_destroy(ini_table_s *table) {
    for (int i = 0; i < table->size; i++) {
        ini_section_s *section = &table->section[i];
        for (int q = 0; q < section->size; q++) {
            ini_entry_s *entry = &section->entry[q];
            free(entry->key);
            free(entry->value);
        }
        free(section->entry);
        free(section->name);
    }
    free(table->section);
    free(table);
}

int eof_hack(int c) {
    static bool first_time = true;

    if (first_time && c == EOF) {
        first_time = false;
        return INT_MAX;
    }

    return EOF;
}

// v2.2: reads via sceKernelOpen/sceKernelRead/sceKernelClose into a
// memory buffer instead of fopen()/fgetc(). This function is reachable
// from ambient_check_config_reload() on ambient_sample_thread (a worker
// thread spawned separately from plugin load), and libc stdio (FILE*,
// buffering, locks) isn't safe to touch off the main plugin-load thread
// on this toolchain -- that's what was crashing. Same raw-syscall
// approach game_patch/utils.cpp already uses successfully elsewhere in
// this repo. The parsing state machine below is unchanged; only the
// byte source changed, from fgetc(f) to indexing into filebuf.
bool ini_table_read_from_file(ini_table_s* table, const char* file)
{
    int32_t fd = sceKernelOpen(file, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return false;

    int64_t filesize = sceKernelLseek(fd, 0, SEEK_END);
    if (filesize < 0) {
        sceKernelClose(fd);
        return false;
    }
    sceKernelLseek(fd, 0, SEEK_SET);

    // malloc(0) is legal-but-implementation-defined (may return NULL even
    // on success), so a genuinely empty file gets a 1-byte buffer that
    // nothing ever reads into rather than being misread as a malloc failure.
    char *filebuf = (char *)malloc((size_t)(filesize > 0 ? filesize : 1));
    if (filebuf == NULL) {
        sceKernelClose(fd);
        return false;
    }

    ssize_t nread = filesize > 0 ? sceKernelRead(fd, filebuf, (size_t)filesize) : 0;
    sceKernelClose(fd);
    if (nread < 0) {
        free(filebuf);
        return false;
    }

    size_t filepos = 0;
    size_t filelen = (size_t)nread;
    #define fgetc(unused) (filepos < filelen ? (int)(unsigned char)filebuf[filepos++] : EOF)

    enum {Section, Key, Value, Comment} state = Section;
    int   c;
    int   position = 0;
    int   spaces   = 0;
    int   line     = 0;
    int   buffer_size = 128 * sizeof(char);
    char* buf   = (char*)malloc(buffer_size);
    char* value = NULL;

    ini_section_s* current_section = NULL;
    memset(buf, '\0', buffer_size);

    bool first_eol = false;
    while(1) {
        c = fgetc(f);
        if (c == eof_hack(c))
            break;

        if (c == '\r')
            continue;
        if (position > buffer_size-2) {
            buffer_size += 128 * sizeof(char);
            size_t value_offset = value == NULL ? 0 : value - buf;
            buf = (char*)realloc(buf, buffer_size);
            memset(buf+position, '\0', buffer_size-position);

            if (value != NULL)
                value = buf + value_offset;
        }
        switch(c) {
            case ' ':
                switch(state) {
                    case Value: if (value[0] != '\0') spaces++; break;
                    default: if (buf[0] != '\0') spaces++; break;
                }
                break;
            case ';':
                while (c != eof_hack(c) && c != '\n')
                {
                    c = fgetc(f);
                }
            // fallthrough
            case '\n':
            // fallthrough
            case EOF:
                if (first_eol) {
                    continue;
                    first_eol = true;
                }
                line++;
                if (state == Value) {
                    if (current_section == NULL) {
                        current_section = _ini_section_create(table, "");
                    }
                    _ini_entry_create(current_section, buf, value);
                    value = NULL;
                } else if (strlen(buf) > 1 && position && state == Key) {
                    if (current_section == NULL) {
                        current_section = _ini_section_create(table, "");
                    }
                    _ini_entry_create(current_section, buf, "");
                } else if (state == Comment) {
                    if (current_section == NULL) {
                        current_section = _ini_section_create(table, "");
                    }
                    _ini_entry_create(current_section, buf, "");
                } else if (state == Section) {
                    debug_printf("Section `%s' missing `]' operator.", buf);
                } else if(state == Key && position) {
                    debug_printf("Key `%s' missing `=' operator.", buf);
                }
                memset(buf, '\0', buffer_size);
                state = Key;
                position = 0;
                spaces = 0;
                break;
            case '[':
                state = Section;
                break;
            case ']':
                current_section = _ini_section_create(table, buf);
                memset(buf, '\0', buffer_size);
                position = 0;
                spaces = 0;
                state = Key;
                break;
            case '=':
                if (state == Key) {
                    state = Value;
                    buf[position++] = '\0';
                    value = buf + position;
                    spaces = 0;
                    continue;
                }
            default:
                for(;spaces > 0; spaces--) buf[position++] = ' ';
                buf[position++] = c;
                break;
        }
    }
    #undef fgetc
    free(buf);
    free(filebuf);
    return true;
}

bool ini_table_write_to_file(ini_table_s *table, const char *file) {
    FILE *f = fopen(file, "w+");
    if (f == NULL)
        return false;
    for (int i = 0; i < table->size; i++) {
        ini_section_s *section = &table->section[i];
        fprintf(f, i > 0 ? "\n[%s]\n" : "[%s]\n", section->name);
        for (int q = 0; q < section->size; q++) {
            ini_entry_s *entry = &section->entry[q];
            if (entry->key[0] == ';') {
                fprintf(f, "%s\n", entry->key);
            } else {
                fprintf(f, "%s = %s\n", entry->key, entry->value);
            }
        }
    }
    if (fflush(f) == 0)
        fsync(fileno(f));
    fclose(f);
    return true;
}

void ini_table_create_entry(ini_table_s *table, const char *section_name, const char *key, const char *value) {
    ini_section_s *section = _ini_section_find(table, section_name);
    if (section == NULL) {
        section = _ini_section_create(table, section_name);
    }
    ini_entry_s *entry = _ini_entry_find(section, key);
    if (entry == NULL) {
        entry = _ini_entry_create(section, key, value);
    } else {
        free(entry->value);
        entry->value = (char *)malloc((strlen(value) + 1) * sizeof(char));
        strcpy(entry->value, value);
    }
}

bool ini_table_check_entry(ini_table_s *table, const char *section_name, const char *key) {
    return (_ini_entry_get(table, section_name, key) != NULL);
}

const char *ini_table_get_entry(ini_table_s *table, const char *section_name, const char *key) {
    ini_entry_s *entry = _ini_entry_get(table, section_name, key);
    if (entry == NULL) {
        return NULL;
    }
    return entry->value;
}

bool ini_table_get_entry_as_int(ini_table_s *table, const char *section_name, const char *key, int *value) {
    const char *val = ini_table_get_entry(table, section_name, key);
    if (val == NULL) {
        return false;
    }
    *value = _atoi(val);
    return true;
}

bool ini_table_get_entry_as_bool(ini_table_s *table, const char *section_name, const char *key, bool *value) {
    const char *val = ini_table_get_entry(table, section_name, key);
    if (val == NULL) {
        return false;
    }
    if (strcasecmp(val, "on") == 0 || strcasecmp(val, "true") == 0 || strcasecmp(val, "1") == 0) {
        *value = true;
    } else {
        *value = false;
    }
    return true;
}
