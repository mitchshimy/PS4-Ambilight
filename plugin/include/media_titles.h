// media_titles.h -- title IDs that always run the Movie preset.
//
// Anything not listed runs whichever preset [presets] active= names (game by
// default). Add an ID here, rebuild, and that app switches to Movie by itself
// with no trip to the companion app. The ID is the one GoldHEN reports for
// the running process (sys_sdk_proc_info's titleid), CUSA plus five digits.
//
// Only IDs confirmed against independent sources belong here. A listed ID
// forces Movie (sampling depth 4, 200 ms smoothing) on whatever runs under
// it, so one that turns out to be a game is worse than a missing one --
// CUSA05682, for example, looks like a YouTube ID in some lists and is
// Horizon Zero Dawn. tools/test_plugin_config.c guards that one by name.
//
// Known but NOT listed, unconfirmed: YouTube TV (one catalogue gives CUSA18680).
// A YouTube beta build, CUSA06021, also exists and is left out.
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
