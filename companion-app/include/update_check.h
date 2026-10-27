// The background update check, split out of main.c: the worker thread that asks the update source...
// More: docs/code-comments.md (companion-app/include/update_check.h:1)

#ifndef UPDATE_CHECK_H
#define UPDATE_CHECK_H

#include "install_plan.h"   // ASSET_COUNT
#include <stdbool.h>
#include <stddef.h>

extern bool g_assetPresent[ASSET_COUNT];   // only the two shell assets are used
extern bool g_assetUpdate[ASSET_COUNT];    // last answer from the update source, for all three

void refresh_dev_base_url(void);
void start_background_update_check(void);
void poll_background_update_check(void);

// The newest published app version when it is newer than this build ("1.03"), "" when it isn't or nothing is known.
// Main thread only; fills in once poll_background_update_check() has seen the check finish.
const char *app_update_version(void);

#endif
