// The background update check, split out of main.c: the worker thread that asks the update source
// whether the plugin or a shell feed file has a newer build, the flags it publishes through, and
// the once-a-frame poll that turns the answer into Home's state. The code is as it ran on hardware.

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

#endif
