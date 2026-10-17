// preset_select.h -- which ini section supplies the preset-owned settings.
// More: docs/code-comments.md (plugin/include/preset_select.h:1)
#ifndef PRESET_SELECT_H
#define PRESET_SELECT_H

#include <stdbool.h>
#include <string.h>

#define PRESET_SECTION_GAME  "preset_game"
#define PRESET_SECTION_MOVIE "preset_movie"

static inline const char *preset_section_pick(const char *active,   // [presets] active, or NULL
                                              bool titleIsMovie,
                                              bool hasGame, bool hasMovie)
{
    bool wantMovie = titleIsMovie || (active != NULL && strcmp(active, "movie") == 0);
    if (wantMovie) {
        if (hasMovie) return PRESET_SECTION_MOVIE;
        if (hasGame)  return PRESET_SECTION_GAME;
    } else {
        if (hasGame)  return PRESET_SECTION_GAME;
        if (hasMovie) return PRESET_SECTION_MOVIE;
    }
    return NULL;
}

#endif // PRESET_SELECT_H
