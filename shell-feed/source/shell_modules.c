// shell_modules.c -- which modules are loaded in this process, and where their symbols are: the one
// consolidated handle scan, case-insensitive name matching, and symbol resolution.
#include "shell_internal.h"

// Helpers
// More: docs/code-comments.md (shell-feed/source/shell_modules.c:5)

int first_byte(void *p)
{
    if (p == NULL) return -1;
    return (int)(*((volatile uint8_t *)p));
}

static char ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

int name_has_ci(const char *name, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen == 0) return 1;
    for (const char *p = name; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] != '\0' && ascii_lower(p[i]) == ascii_lower(needle[i])) i++;
        if (i == nlen) return 1;
    }
    return 0;
}

static int name_exact_ci(const char *name, const char *exact)
{
    size_t i = 0;
    for (;; i++) {
        char a = ascii_lower(name[i]);
        char b = ascii_lower(exact[i]);
        if (a != b) return 0;
        if (a == '\0') return 1;
    }
}

static int looks_graphics(const char *name)
{
    static const char *const keys[] = {
        "gnm", "videoout", "piglet", "gles", "egl",
        "agc", "gpu", "disp", "screen", "compos", "vsh", "mono"
    };
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (name_has_ci(name, keys[i])) return 1;
    return 0;
}

// Step 1a: the v2 list call, kept for comparison (it returned count=1 in v2).
void list_modules_api(void)
{
    OrbisKernelModule handles[256] = {0};
    size_t numModules = 0;
    s32 ret = sceKernelGetModuleList(handles, sizeof(handles), &numModules);
    printf(TAG " sceKernelGetModuleList ret=0x%x count=%d (v2 returned 1)\n",
           (unsigned)ret, (int)numModules);
}

// [v8] ONE consolidated scan: walks the handle range ONCE per phase and answers every question this...
// More: docs/code-comments.md (shell-feed/source/shell_modules.c:65)

struct ScanResult scan_and_discover(const char *phase)
{
    struct ScanResult r = {0};
    r.hPiglet = r.hScreenShot = r.hVideoRecording = r.hMono = r.hVideoOut = r.hGnm = -1;
    int missRun = 0, h;
    printf(TAG " scan %s: handles 0..%d unconditionally, then until %d consecutive misses (cap %d), single consolidated pass\n",
           phase, SCAN_START_CAP, SCAN_MISS_RUN, SCAN_HARD_CAP);
    for (h = 0; h <= SCAN_HARD_CAP; h++) {
        usleep(SCAN_YIELD_USEC); // yield once per handle -- HANDOFF 7.12, unchanged in v8
        struct OrbisKernelModuleInfo info = {0};
        info.size = sizeof(info);
        s32 rr = sceKernelGetModuleInfo((OrbisKernelModule)h, &info);
        if (rr != 0) {
            missRun++;
            if (h > SCAN_START_CAP && missRun >= SCAN_MISS_RUN) break;
            continue;
        }
        missRun = 0;
        r.highestValid = h;
        r.found++;

        int g = looks_graphics(info.name);
        if (name_has_ci(info.name, "gnm")) r.flags |= 1;
        if (name_has_ci(info.name, "videoout")) r.flags |= 2;
        if (name_has_ci(info.name, "piglet")) {
            r.flags |= 4;
            if (r.hPiglet < 0) r.hPiglet = h;
        }
        if (name_exact_ci(info.name, "libscevideoout.sprx")) r.hVideoOut = h;
        if (name_has_ci(info.name, "gnmdriver")) {
            if (r.gnmCount < MAX_GNM_MODS) r.gnmHandles[r.gnmCount] = h;
            r.gnmCount++;
            if (r.hGnm < 0) r.hGnm = h;
            printf(TAG "   [%s] gnmdriver candidate: handle=%d name=%s\n", phase, h, info.name);
        }
        if (name_exact_ci(info.name, "libscescreenshot.sprx")) r.hScreenShot = h;
        if (name_exact_ci(info.name, "libscevideorecording.sprx")) r.hVideoRecording = h;
        if (name_has_ci(info.name, "mono")) {
            if (r.monoMatchCount < MAX_MONO_LOG_LINES)
                printf(TAG "   [%s] mono match: handle=%d name=%s\n", phase, h, info.name);
            r.monoMatchCount++;
            int isSgen = name_has_ci(info.name, "monosgen");
            // Prefer a "monosgen" match (the confirmed real runtime, 7.15) over any earlier non-sgen match (e.g....
            // More: docs/code-comments.md (shell-feed/source/shell_modules.c:116)
            if (r.hMono < 0 || (isSgen && !r.monoPrimaryIsSgen)) {
                r.hMono = h;
                r.monoPrimaryIsSgen = isSgen;
            }
        }

        printf(TAG "   [%s] handle=%d name=%s base=%p%s\n", phase, h, info.name,
               (void *)info.segmentInfo[0].address, g ? "  <-- graphics?" : "");
    }
    if (h > SCAN_HARD_CAP) r.hitCap = 1;
    printf(TAG " scan %s: stopped at h=%d (%s), %d handles answered, highest valid=%d\n",
           phase, h, r.hitCap ? "HIT HARD CAP, list may extend further" : "miss-run complete, list likely ended",
           r.found, r.highestValid);
    printf(TAG " scan %s summary: VideoOut.handle=%d Gnm.handle=%d | Gnm=%d VideoOut=%d Piglet=%d(handle=%d) ScreenShot=handle=%d VideoRecording=handle=%d Mono=handle=%d(sgen=%d, %d total match%s)\n",
           phase, r.hVideoOut, r.hGnm, (r.flags & 1) ? 1 : 0, (r.flags & 2) ? 1 : 0, (r.flags & 4) ? 1 : 0, r.hPiglet,
           r.hScreenShot, r.hVideoRecording, r.hMono, r.monoPrimaryIsSgen, r.monoMatchCount,
           r.monoMatchCount == 1 ? "" : "es");
    return r;
}

void *resolve(int32_t handle, const char *lib, const char *sym)
{
    void *addr = NULL;
    if (handle == 0) {
        printf(TAG " %s: handle 0, skipping %s\n", lib, sym);
        return NULL;
    }
    int ret = sys_dynlib_dlsym(handle, sym, &addr);
    printf(TAG " %s : %s -> %p ret=0x%x firstbyte=%d%s\n", lib, sym, addr,
           (unsigned)ret, first_byte(addr), (first_byte(addr) == 0xE9) ? " (jmp stub)" : "");
    return addr;
}
