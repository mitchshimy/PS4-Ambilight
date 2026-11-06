// module_census.h -- the load log's list of the modules a title has loaded (v4.0.1).
//
// Only with [compat] report_file=1. Called from plugin_load when a video library handle is 0, to say
// which modules that process does have and whether any "gnmdriver" module carries the Gnm flip
// functions. It reads only; plugin_load uses the handle it returns as a backup way to find the Neo driver.
#ifndef MODULE_CENSUS_H
#define MODULE_CENSUS_H

#include <stdint.h>

// Returns the handle of the first "gnmdriver" module that has sceGnmSubmitAndFlipCommandBuffers, or 0.
// Each "gnmdriver candidate" line also prints the address of the two functions.
int32_t module_census_log(void);

// A short look at the module list taken BEFORE the plugin loads anything by name, so it shows what the title
// itself loaded: only gnm/videoout names are logged. Gives the handle of libSceVideoOut.sprx, libSceGnmDriver.sprx
// and libSceGnmDriverForNeoMode.sprx (exact names, 0 = not loaded); any out pointer may be NULL. All 0 without report_file=1.
void module_census_snapshot(const char *tag, int32_t *hVideoOut, int32_t *hNormal, int32_t *hNeo);

// The handles plugin_load finally hooked, kept so the census at a "WAITING: no flips" event can say what was hooked.
extern volatile int32_t g_censusHookedVideoOut;
extern volatile int32_t g_censusHookedGnm;

#endif // MODULE_CENSUS_H
