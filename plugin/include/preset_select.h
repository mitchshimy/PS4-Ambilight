// preset_select.h -- which ini section supplies the preset-owned settings.
// Own header, string logic only, so tools/test_preset_select.c can build it on
// a PC.
//
// The ini holds two complete presets, [preset_game] and [preset_movie], and
// [presets] active= says which one runs. A title in media_titles.h runs movie
// whatever active says, because that is the whole point of having a preset
// per kind of content: Netflix gets its own smoothing without anyone opening
// the app.
//
// If the wanted preset has no section the other one is used, and if neither
// exists the answer is NULL, meaning "an ini from before v3.8, read the old
// flat [layout]/[color]/[timing] keys". So old inis keep working untouched.
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
