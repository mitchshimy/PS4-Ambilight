// module_census.c -- see module_census.h. Walks the module handles the way the shell feed does
// (sceKernelGetModuleInfo per handle, bounded), writes one load-log line per module, then checks
// each module with "gnmdriver" in its name for the two Gnm flip functions the plugin hooks.
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <orbis/libkernel.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "load_trace.h"
#include "module_census.h"

#define CENSUS_HANDLE_CAP   511   // the range the shell feed scans without a break (SCAN_START_CAP)
#define CENSUS_MAX_LINES    160   // the load log is capped at 48 KB for every title together
#define CENSUS_YIELD_USEC   200

volatile int32_t g_censusHookedVideoOut = 0;   // set by plugin_load once the hook targets are chosen
volatile int32_t g_censusHookedGnm      = 0;

static char lower_c(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

static int name_has_ci(const char *name, const char *needle)
{
    size_t n = strlen(needle);
    for (const char *p = name; *p; p++) {
        size_t i = 0;
        while (i < n && p[i] != '\0' && lower_c(p[i]) == lower_c(needle[i])) i++;
        if (i == n) return 1;
    }
    return 0;
}

int32_t module_census_log(void)
{
    if (!g_reportEnabled) return 0;   // the caller is gated too

    int found = 0, logged = 0, gnmCount = 0;
    int gnmHandles[8];
    for (int h = 0; h <= CENSUS_HANDLE_CAP; h++) {
        usleep(CENSUS_YIELD_USEC);
        struct OrbisKernelModuleInfo info;
        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        if (sceKernelGetModuleInfo((OrbisKernelModule)h, &info) != 0) continue;
        found++;
        int isGnm = name_has_ci(info.name, "gnm");
        int isVid = name_has_ci(info.name, "videoout");
        if (name_has_ci(info.name, "gnmdriver") && gnmCount < 8) gnmHandles[gnmCount++] = h;
        // Near the cap only graphics-looking names are kept, so the end of a long list is not lost.
        if (logged < CENSUS_MAX_LINES && (logged < CENSUS_MAX_LINES - 20 || isGnm || isVid)) {
            load_trace_note("module: handle %d name %s%s", h, info.name, (isGnm || isVid) ? "  <-- video" : "");
            logged++;
        }
    }
    load_trace_note("module census: %d modules answered in handles 0..%d, %d logged, %d with \"gnmdriver\" in the name",
                    found, CENSUS_HANDLE_CAP, logged, gnmCount);

    int32_t usable = 0;
    for (int i = 0; i < gnmCount; i++) {
        void *submit = NULL, *workload = NULL;
        sys_dynlib_dlsym(gnmHandles[i], "sceGnmSubmitAndFlipCommandBuffers", &submit);
        sys_dynlib_dlsym(gnmHandles[i], "sceGnmSubmitAndFlipCommandBuffersForWorkload", &workload);
        load_trace_note("gnmdriver candidate at handle %d: sceGnmSubmitAndFlipCommandBuffers %s (0x%llx), ForWorkload %s (0x%llx)",
                        gnmHandles[i], submit != NULL ? "FOUND" : "missing", (unsigned long long)(uintptr_t)submit,
                        workload != NULL ? "FOUND" : "missing", (unsigned long long)(uintptr_t)workload);
        if (usable == 0 && submit != NULL) usable = (int32_t)gnmHandles[i];
    }
    if (gnmCount == 0) load_trace_note("no module with \"gnmdriver\" in its name is loaded in this title");
    return usable;
}

static int name_equals_ci(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (lower_c(*a) != lower_c(*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

void module_census_snapshot(const char *tag, int32_t *hVideoOut, int32_t *hNormal, int32_t *hNeo)
{
    int32_t vOut = 0, normal = 0, neo = 0;
    if (hVideoOut) *hVideoOut = 0;
    if (hNormal)   *hNormal = 0;
    if (hNeo)      *hNeo = 0;
    if (!g_reportEnabled) return;

    int found = 0, maxHandle = -1;
    for (int h = 0; h <= CENSUS_HANDLE_CAP; h++) {
        usleep(CENSUS_YIELD_USEC);
        struct OrbisKernelModuleInfo info;
        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        if (sceKernelGetModuleInfo((OrbisKernelModule)h, &info) != 0) continue;
        found++;
        maxHandle = h;
        if (name_equals_ci(info.name, "libSceVideoOut.sprx") && vOut == 0)            vOut   = h;
        if (name_equals_ci(info.name, "libSceGnmDriver.sprx") && normal == 0)         normal = h;
        if (name_equals_ci(info.name, "libSceGnmDriverForNeoMode.sprx") && neo == 0)  neo    = h;
        if (name_has_ci(info.name, "gnm") || name_has_ci(info.name, "videoout"))
            load_trace_note("snapshot (%s): handle %d name %s", tag, h, info.name);
    }
    load_trace_note("snapshot (%s): %d modules, highest handle %d; loaded by the title: VideoOut %d, GnmDriver %d, GnmDriverForNeoMode %d (0 = not loaded)",
                    tag, found, maxHandle, (int)vOut, (int)normal, (int)neo);
    if (hVideoOut) *hVideoOut = vOut;
    if (hNormal)   *hNormal = normal;
    if (hNeo)      *hNeo = neo;
}
