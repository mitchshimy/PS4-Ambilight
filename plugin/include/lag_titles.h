// lag_titles.h -- title IDs that sample one flip back instead of two.
// More: docs/debugging/sample-lag-and-boot-flash.md (two-slot swap chains)
#ifndef LAG_TITLES_H
#define LAG_TITLES_H

#include <stdbool.h>
#include <string.h>

static const char *const kLagOneTitleIds[] = {
    "LAPY20002",   // Lapy's PS4 Player: two slots, the slot two flips back is the one still being drawn
    NULL // keep last
};

// The list is a parameter so a PC test can hand it a fake one.
static inline bool lag_title_in_list(const char *titleId, const char *const *list)
{
    if (titleId == NULL || titleId[0] == '\0') return false;
    for (int i = 0; list[i] != NULL; i++)
        if (strcmp(titleId, list[i]) == 0) return true;
    return false;
}

static inline bool lag_title_is_one_back(const char *titleId)
{
    return lag_title_in_list(titleId, kLagOneTitleIds);
}

#endif // LAG_TITLES_H
