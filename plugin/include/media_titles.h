// media_titles.h -- title IDs that always run the Movie preset.
// More: docs/code-comments.md (plugin/include/media_titles.h:1)
#ifndef MEDIA_TITLES_H
#define MEDIA_TITLES_H

#include <stdbool.h>
#include <string.h>

static const char *const kMovieTitleIds[] = {
    // Netflix
    "CUSA00129",   // US / Canada
    "CUSA00127",   // Europe / Australia
    "CUSA02988",   // Japan
    // YouTube
    "CUSA01015",   // US (AR BR CA CL CO MX PE US)
    "CUSA01116",   // Europe and the rest of the world
    "CUSA01065",   // Japan
    "CUSA01034",   // Asia (HK ID KR MY SG TW TH)
    NULL // keep last
};

// The list is a parameter so a PC test can hand it a fake one.
static inline bool media_title_in_list(const char *titleId, const char *const *list)
{
    if (titleId == NULL || titleId[0] == '\0') return false;
    for (int i = 0; list[i] != NULL; i++)
        if (strcmp(titleId, list[i]) == 0) return true;
    return false;
}

static inline bool media_title_is_movie(const char *titleId)
{
    return media_title_in_list(titleId, kMovieTitleIds);
}

#endif // MEDIA_TITLES_H
