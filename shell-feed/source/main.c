// shell_probe v24: v23 plus the four Phase 3 leftovers (docs/integration_plan_v2.md): report file, [dev] UDP
// mirror, self-heal, one log line per gate. Nothing about v23's behaviour changes; see this overlay's README.
//  1. REPORT FILE. [compat] report_file=1 -> /data/ps4_ambient_report_SHELL.txt, written by its own thread on
//     start, on a change, and every 60 s. No WLED address, no ini values. Last line "end of report".
//  2. [dev] MIRROR. [dev] dev_ip + dev_logging=true (both, like the sister) -> the 5 s status row and the stop,
//     standby, resume, claim and self-heal events go to dev_ip:4048 as DDP-wrapped text starting "SHL1".
//  3. SELF-HEAL. 30 bad buffer checks in a row (~1 s) -> map_scan(0); find_bufs();. At most one re-scan per
//     10 s, doubling to 60 s while re-scans do not help.
//  4. GATES. One "GATE:" line per compile-time gate at start (an undefined gate macro is now a compile error)
//     and one "GATE (runtime ...)" line whenever the ini changes report_file / [dev].
//
// ---- v23 header, unchanged ----
// shell_probe v23: runs until reboot; disable file and [shell] enabled= switch; no crash-loop guard.
//
// Basis (v22, hardware-verified, docs/2026-10-06_probe-v22_A.md + v22_B.md): ini read at start, live reload
// confirmed (two edits, order 0->1->0, strip visibly rotated). What v22 did not do: run past 300 s, or
// offer a way to silence the feed without a reboot.
//
// WHAT'S NEW (v23)
//  1. RUN UNTIL REBOOT. The 300-second LIVE_SECONDS limit is gone. The send loop runs until the process
//     exits (reboot), or until a clean stop fires (items 2-3 below). The LIVE: header says "until reboot"
//     and the LIVE done line reports elapsed seconds.
//  2. DISABLE FILE. If /data/ps4_shell_feed_off exists (create over FTP or the app) the send loop exits
//     cleanly with "DISABLED: disable file found". Delete the file to silence the feed until next reboot.
//  3. [shell] enabled=false in the ini. Same effect as the disable file, picked up by the live-reload poll
//     with no reboot. Default: true (absent key = enabled). "DISABLED: shell disabled in ini" is logged.
//
// NO CRASH-LOOP GUARD. v21 and v22 wrote /data/shell_probe_vXX_ran on first run and refused to re-run if
// it existed; v23 drops this entirely. The guard never actually triggered during testing (v1-v22) because
// the person always rebooted between sessions -- it only would have fired on a second injection in the same
// boot without a reboot, which never happened. Recovery from a crash loop is the same as any other plugin:
// remove the SPRX or disable AutoRun, exactly as a person would do for any bad GoldHEN plugin. The manual
// delete-to-override step was complexity that worked against the automation goal.
//
// v22 note (kept for context): the shell feed runs on YOUR ini -- the same settings, the same zone geometry,
// the same colour chain and the same [preset_game] the PS4-Ambilight plugin uses for a game.
//
// Basis (v21-A, hardware-verified, docs/2026-10-06_probe-v21_A.md): the claim gate, the 42-LED order fix and
// the fast handoff work. What v21 did not do was follow the ini: its order was a hard-coded rotation, its
// sampling was its own (four points per zone in a band), and its colours were raw. v22 replaces that.
//
// WHAT'S NEW
//  1. INI. /data/ps4_ambient_light.ini is read at start and re-checked every [timing]
//     config_reload_check_seconds (the sister's own 2 s), by size AND content hash read through raw
//     sceKernelOpen/Read (stat() is garbage on this filesystem, the sister's hardware-proven lesson). A change
//     reloads the settings, rebuilds the zone geometry and restarts smoothing, with no reboot.
//  2. PRESET. Settings that a preset owns come from [preset_game], whatever [presets] active= says, because
//     active=movie would otherwise turn the shell into "movie" too. If the ini has no game preset the same
//     fallbacks as the sister apply (the movie preset, then the old flat [layout]/[color]/[timing] keys).
//  3. GEOMETRY. buildZoneGeometry() from the sister: led_count_*, led_start_corner, led_direction and
//     led_offset decide where zone i sits and which LED it is. v21's hard-coded LED_ROT is no longer used by
//     the live send (it only remains for the old scene previews).
//  4. SAMPLING AND COLOUR. sampleZoneAverage() with scan_depth, the per-channel gamma, then
//     applyColorProcessing() (gamma -> brightness -> contrast -> saturation -> levels -> colour order),
//     applyDarkThreshold() and the sister's smoothing (settling_time_ms). update_frequency_hz sets the
//     pace, wled_host/wled_port the destination (the compile-time host is only a fallback).
//
// HOW THE SISTER'S CODE GOT IN. Through v24 (shell_probe's own history, as a research-repo probe) it was
// pasted, not rewritten: every region between a "sister-begin" and a "sister-end" marker was cut out of
// the sister's own sources by function name (PS4-Ambilight 89199fe) by tools/make_probe_v22.py, and
// tools/host_ini_check/run.sh cut the same regions out of that file and compared them to the sister's
// files byte for byte. As of the PS4-Ambilight/ps4-shell-capture-research merge's Stage C, this plugin
// (now living in the sister's own repo, as shell-feed/) instead compiles tiling.c, pixel_formats.c,
// zones.c, gamma.c, color_processing.c, settings.c and config.c straight from ../plugin/source/ -- see
// the comment above the real #include block further down for the rest of that story, and
// shell-feed/README.md for the provenance of the one piece that is still local to this file (the
// probe's own v17 tiling math). Stage D then moved the per-frame smoothing out of both this file and
// sample_thread.c into plugin/include/smoothing.h, so there is no "sister-begin" paste left here.
//
// NOT PORTED, on purpose (these exist for a game's process or a game's pixels, not the shell's):
//  - auto letterbox (the shell has no black bars; its margins stay 0), HDR/PQ format detection,
//    the sample-lag buffer choice and read-verify (v21 samples the buffer the compositor just flipped),
//  - [network] relay, [dev] UDP logging, [compat], the report file,
//  - writing a default ini: if the file is missing v22 keeps the sister's compiled defaults and says so.
//  The values in an INI line in the log are therefore the sister's own, with the shell's choice of preset.
//
// Claim reader, flip tracking, the 50 ms poll and the CLAIM log lines are v21's, unchanged.
//
// ENTRY POINT: module_start.
// CRASH-LOOP GUARD: marker /data/shell_probe_v22_ran (delete over FTP to rerun).
// HOOKS STAY INSTALLED after the window and keep counting; never unhooked. Reboot to clear.

#define DO_WLED_SEND             1   // [v18] ON: this is the live-send run
#define DO_LIVE_DDP              1   // [v19] FIX: v18 used this in two #if lines but never defined it, so the whole send loop compiled out
#define DO_WLED_SEND_INCLUDES    DO_WLED_SEND
#define WLED_HOST                "192.168.2.110"  // [v18] person's own WLED device
#define WLED_PORT                4048
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>    // [v22] atoi (the sister's ini reader)
#include <strings.h>   // [v22] strcasecmp (same)
#include <limits.h>    // [v22] INT_MAX (same)
#include <stdbool.h>   // [v22] the sister's settings code uses bool
#include <stdarg.h>    // [v24] shell_event()
// [v8] no ctype.h dependency -- ascii_lower() below is self-contained,
// so this doesn't rely on the toolchain's locale/ctype support existing.

#include <orbis/libkernel.h>
#include <orbis/_types/video.h>   // OrbisVideoOutBufferAttribute only

#include "plugin_common.h"
#if DO_WLED_SEND_INCLUDES
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#define DISABLE_FILE  "/data/ps4_shell_feed_off"  // [v23] create over FTP to silence the feed without a reboot
#define TAG           "[SHELLPROBE24]"
#define LIVE_HZ         30     // [v18] matches the sister project's own rate
// LIVE_SECONDS and REPORT_SECONDS removed in v23: the send loop runs until the process exits.

// [v20] the sister plugin's claim file. Format and contract: PS4-Ambilight v3.9.2+ (v3.9.3 writes fg=1 sooner),
// docs/development/shell-feed-coordination.md. This probe only reads it.
#define CLAIM_PATH        "/data/ps4_ambient_state"
#define CLAIM_RECORD_LEN  35
#define CLAIM_POLL_MS      50   // [v21] how often to re-read the file (v20: 250)
#define CLAIM_STALE_MS    2500  // a seen-but-unchanging seq older than this is not a claim
#define REPORT_PERIOD   5
#define RING_SIZE       8
#define MAX_ADDRS       4
#define MAX_GNM_MODS    4
#define DO_CONTROL_CALLS    1
#define ALLOW_ANY_PROCESS   0   // 1 = install hooks even if this is not GnmCompositor (do not)
#define DO_FLIPSTATUS_CALL       0   // [v13] back OFF; answered in v12-A. See header.
#define WATCH_SECONDS            60 // [v13] keep printing REGISTER events this long after the window
#define DO_DUMP_BUFFERS          0   // [v18] off: this run live-sends, does not dump (v17 machinery kept, unused)
#define DO_SAMPLER_SCENES        0   // [v18] off: v17's fixed-scene schedule is not used this run
#define SHELL_REPORT_FILE        1   // [v24] report thread + /data/ps4_ambient_report_SHELL.txt (still needs [compat] report_file=1)
#define SHELL_DEV_MIRROR         1   // [v24] [dev] UDP mirror (still needs dev_ip AND dev_logging=true)
#define SHELL_SELF_HEAL          1   // [v24] re-run map_scan()+find_bufs() after a run of bad buffer checks
#define ALLOW_KERNEL_PRX_LOAD    1   // [v15] see header RISK; 0 = resolve-only
#define DO_MPROTECT_READ         0   // [v14] see header RISK; 0 = query-only run
#define DO_RESOLUTIONSTATUS_CALL 0   // [v11] OFF by default -- same caution
#define STATUS_BUF_WORDS         32  // [v11] 32 x 8 bytes = 256 bytes; far larger than any known PS4 status struct

#define SCAN_START_CAP  511     // unchanged from v4-v9
#define SCAN_HARD_CAP   4095
#define SCAN_MISS_RUN   64
#define SCAN_YIELD_USEC 500     // per-handle yield, HANDOFF 7.12; unchanged
#define MAX_MONO_LOG_LINES 16

// ---------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------
static int first_byte(void *p)
{
    if (p == NULL) return -1;
    return (int)(*((volatile uint8_t *)p));
}

static char ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static int name_has_ci(const char *name, const char *needle)
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

// Copy 8 bytes of code at p into out (reads are plain loads; code pages are
// readable, as first_byte() already relies on).
static void read8(void *p, uint8_t out[8])
{
    for (int i = 0; i < 8; i++) out[i] = ((volatile uint8_t *)p)[i];
}

static void print8(const char *label, const uint8_t b[8])
{
    printf(TAG "   %s %02x %02x %02x %02x %02x %02x %02x %02x\n", label,
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
}

// Step 1a: the v2 list call, kept for comparison (it returned count=1 in v2).
static void list_modules_api(void)
{
    OrbisKernelModule handles[256] = {0};
    size_t numModules = 0;
    s32 ret = sceKernelGetModuleList(handles, sizeof(handles), &numModules);
    printf(TAG " sceKernelGetModuleList ret=0x%x count=%d (v2 returned 1)\n",
           (unsigned)ret, (int)numModules);
}

// [v8] ONE consolidated scan: walks the handle range ONCE per phase and
// answers every question this probe used to run a SEPARATE full table scan
// for (Piglet, ScreenShot, VideoRecording, Mono discovery, four guessed
// Mono filenames -- up to ten full passes total in v7, HANDOFF 7.15's
// "repeated-full-scan design issue"). Still scans 0..SCAN_START_CAP
// unconditionally, then keeps going until SCAN_MISS_RUN consecutive misses
// or SCAN_HARD_CAP, exactly as v5-v7 (HANDOFF 7.9's fix, unchanged). All
// name matching is case-insensitive (fixes v7-A's missed Mono modules).
struct ScanResult {
    int found;              // handles that answered
    int highestValid;
    int hitCap;             // 1 if SCAN_HARD_CAP was hit before a miss-run completed
    int flags;              // bit0 Gnm, bit1 VideoOut, bit2 Piglet (ci substring, for the summary line)
    int hVideoOut;          // [v9] handle whose name is exactly "libscevideoout.sprx" (ci), or -1
    int hGnm;               // [v10] FIRST handle whose name CONTAINS "gnmdriver" (ci), or -1. v9 matched the exact name
                            // "libscegnmdriver.sprx" and missed libSceGnmDriver_sys.sprx (capture v9-A, HANDOFF 7.18).
    int gnmCount;           // [v10] how many modules contain "gnmdriver"
    int gnmHandles[MAX_GNM_MODS]; // [v10] their handles (first MAX_GNM_MODS)
    int hPiglet;            // first handle whose name contains "piglet" (ci), or -1
    int hScreenShot;        // handle whose name is exactly "libscescreenshot.sprx" (ci), or -1
    int hVideoRecording;    // handle whose name is exactly "libscevideorecording.sprx" (ci), or -1
    int hMono;              // best Mono match found (see preference rule below), or -1
    int monoMatchCount;     // how many modules matched "mono" (ci substring)
    int monoPrimaryIsSgen;  // 1 if hMono's name contains "monosgen" (the confirmed real runtime -- 7.15)
};

static struct ScanResult scan_and_discover(const char *phase)
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
            // Prefer a "monosgen" match (the confirmed real runtime, 7.15)
            // over any earlier non-sgen match (e.g. libmono-btls-shared.sprx,
            // a TLS/crypto helper, not the VM itself); otherwise keep the
            // first match found.
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

static void *resolve(int32_t handle, const char *lib, const char *sym)
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



// ---------------------------------------------------------------------
// Shared state, written by hooks, read by the reporter thread. Plain
// volatile counters: diagnostics only, torn reads are acceptable.
// ---------------------------------------------------------------------
static volatile uint32_t g_regCount  = 0;
static volatile uint32_t g_flipCount = 0;

static volatile int32_t  g_lastFlipHandle = -1;
static void             *g_pFlipStatusFn = NULL;   // [v11] resolved in module_start, used by probe_thread
static void             *g_pResStatusFn  = NULL;   // [v11] resolved in module_start, used by probe_thread
static volatile int      g_statusCallDone = 0;     // [v11] one-shot latch
static volatile int32_t  g_lastFlipIndex  = -1;
static volatile int32_t  g_lastFlipMode   = -1;

struct RegEvent {
    int32_t  handle, startIndex, bufferNum;
    uint32_t format, tmode, width, height, pixelPitch;
    uint64_t addr[MAX_ADDRS];
};
static struct RegEvent   g_ring[RING_SIZE];
static volatile uint32_t g_ringHead = 0;   // total events recorded

int32_t (*sceVideoOutRegisterBuffersPtr)(int32_t handle, int32_t startIndex,
                                          void *const *addresses, int32_t bufferNum,
                                          const OrbisVideoOutBufferAttribute *attribute);
int32_t (*sceVideoOutSubmitFlipPtr)(int32_t handle, int32_t bufferIndex,
                                     int32_t flipMode, int64_t flipArg);

HOOK_INIT(sceVideoOutRegisterBuffersPtr);
HOOK_INIT(sceVideoOutSubmitFlipPtr);

int32_t sceVideoOutRegisterBuffersPtr_hook(int32_t handle, int32_t startIndex,
                                            void *const *addresses, int32_t bufferNum,
                                            const OrbisVideoOutBufferAttribute *attribute)
{
    uint32_t slot = __sync_fetch_and_add(&g_ringHead, 1) % RING_SIZE;
    struct RegEvent *e = &g_ring[slot];
    e->handle = handle; e->startIndex = startIndex; e->bufferNum = bufferNum;
    if (attribute != NULL) {
        e->format = (uint32_t)attribute->format;
        e->tmode = (uint32_t)attribute->tmode;
        e->width = (uint32_t)attribute->width;
        e->height = (uint32_t)attribute->height;
        e->pixelPitch = (uint32_t)attribute->pixelPitch;
    } else {
        e->format = e->tmode = e->width = e->height = e->pixelPitch = 0;
    }
    for (int i = 0; i < MAX_ADDRS; i++) {
        e->addr[i] = (addresses != NULL && i < bufferNum) ? (uint64_t)addresses[i] : 0;
    }
    __sync_fetch_and_add(&g_regCount, 1);

    return HOOK_CONTINUE(sceVideoOutRegisterBuffersPtr,
                          int32_t(*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *),
                          handle, startIndex, addresses, bufferNum, attribute);
}

int32_t sceVideoOutSubmitFlipPtr_hook(int32_t handle, int32_t bufferIndex,
                                       int32_t flipMode, int64_t flipArg)
{
    g_lastFlipHandle = handle;
    g_lastFlipIndex = bufferIndex;
    g_lastFlipMode = flipMode;
    __sync_fetch_and_add(&g_flipCount, 1);

    return HOOK_CONTINUE(sceVideoOutSubmitFlipPtr,
                          int32_t(*)(int32_t, int32_t, int32_t, int64_t),
                          handle, bufferIndex, flipMode, flipArg);
}

// Log what a hook did to its target. Returns 1 if the target bytes changed.
static int report_patch(const char *what, void *target, void *hookfn,
                        const uint8_t before[8])
{
    uint8_t after[8];
    read8(target, after);
    int changed = memcmp(before, after, 8) != 0;
    int64_t delta = (int64_t)((uintptr_t)hookfn - (uintptr_t)target);
    printf(TAG " patch %s: target=%p hookfn=%p delta=%lld bytes_changed=%s%s\n", what,
           target, hookfn, (long long)delta, changed ? "YES" : "NO",
           (delta > 0x7fffffffLL || delta < -0x80000000LL) ? " (hook farther than 2GB)" : "");
    print8("before:", before);
    print8("after: ", after);
    return changed;
}

// [v10] HOOK32 patches a 32-bit relative jump, which cannot reach a hook function
// more than 2 GB from its target. v3-v8 only LOGGED that condition (it never occurred
// in the shell). In the compositor an unreachable hook would crash it on the first
// call, so v10 refuses to patch instead. Returns 1 if the hook is reachable.
static int hook_reachable(const char *what, void *target, void *hookfn)
{
    int64_t delta = (int64_t)((uintptr_t)hookfn - (uintptr_t)target);
    if (delta > 0x7fffffffLL || delta < -0x80000000LL) {
        printf(TAG " SKIP patch %s: hook function %p is %lld bytes from target %p (beyond 32-bit jump range); NOT patching\n",
               what, hookfn, (long long)delta, target);
        return 0;
    }
    return 1;
}

// ---------------------------------------------------------------------
// Reporter thread (probe_thread)
// [v23] The CUE table and the timed reporter loop have been removed: live_send() now runs until the
// process exits and never returns, so the post-send loop was dead code. Status rows (LIVE t=Xs ...) are
// printed by the send loop itself every 5 s and carry all the progress information that was in the CUEs.
// ---------------------------------------------------------------------


// =====================================================================
// [v14] Memory-map scan + optional CPU_READ probe (see header)
// =====================================================================
typedef struct {
    void    *start;
    void    *end;
    int64_t  offset;
    int32_t  protection;   // bit0 CPU_READ, bit1 CPU_WRITE, bit4 GPU_READ, bit5 GPU_WRITE
    int32_t  memoryType;
    uint8_t  flags;        // bit1 = direct (sister project, confirmed on MK11)
    char     name[31];
    uint8_t  pad[64];      // generous: the kernel is told 72 bytes, as in the sister project
} VqInfo;

#define VQ_FIND_NEXT      1            // Inferred (see header)
#define VQ_PROT_CPU_READ  0x1
#define VQ_FLAG_DIRECT    0x2
#define MAP_MAX_ITER      4096
#define MAP_PRINT_CAP     80
#define CAND_MIN          (6ULL  << 20)
#define CAND_MAX          (20ULL << 20)
#define CAND_CAP          8
#define PROT_TRY_CAP      4
#define SAMPLE_WORDS      4096

static int32_t (*g_pVq)(const void *, int32_t, void *, uint64_t) = NULL;
static int32_t (*g_pMprot)(const void *, uint64_t, int32_t) = NULL;

struct Cand { uint64_t start, end; int64_t off; int32_t prot, mtype; uint8_t flags; };
static struct Cand g_cand[CAND_CAP];
static int g_nCand = 0;

static void *find_kernel_sym(const char *sym, int32_t *hOut)
{
    // Step 1: resolve-only over known handle ranges.
    for (int32_t h = 0; h <= 0x2010; h++) {
        if (h == 65) h = 0x2000;          // 0..64, then 0x2000..0x2010
        void *a = NULL;
        int r = sys_dynlib_dlsym(h, sym, &a);
        if (r == 0 && a != NULL) { printf(TAG " MAP: %s found via dlsym on handle 0x%x\n", sym, (unsigned)h); *hOut = h; return a; }
    }
    printf(TAG " MAP: %s not found by dlsym over handles 0..64 and 0x2000..0x2010\n", sym);
#if ALLOW_KERNEL_PRX_LOAD
    // Step 2: ask the loader for libkernel by name (returns the existing handle if resident).
    static const char *const names[] = { "libkernel_sys.sprx", "libkernel.sprx" };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        int32_t h = 0;
        int lr = sys_dynlib_load_prx(names[i], &h);
        void *a = NULL;
        int r = (h != 0) ? sys_dynlib_dlsym(h, sym, &a) : -1;
        printf(TAG " MAP: load_prx(\"%s\") ret=0x%x handle=0x%x -> dlsym(%s) ret=0x%x addr=%p\n",
               names[i], (unsigned)lr, (unsigned)h, sym, (unsigned)r, a);
        if (h != 0 && r == 0 && a != NULL) { *hOut = h; return a; }
    }
#endif
    return NULL;
}

static int vq_at(uint64_t addr, int32_t flags, VqInfo *vi)
{
    memset(vi, 0, sizeof(*vi));
    return g_pVq((const void *)addr, flags, vi, 72);
}

static void map_scan(int verbose)
{
    g_nCand = 0;                                   // [v17] re-entrant: scenes 2-4 rescan
    if (g_pVq == NULL) {
        int32_t hq = -1, hm = -1;
        g_pVq = (int32_t (*)(const void *, int32_t, void *, uint64_t))find_kernel_sym("sceKernelVirtualQuery", &hq);
        g_pMprot = (int32_t (*)(const void *, uint64_t, int32_t))find_kernel_sym("sceKernelMprotect", &hm);
        printf(TAG " MAP: sceKernelVirtualQuery -> %p (handle %d, firstbyte=%d) | sceKernelMprotect -> %p (handle %d)\n",
               (void *)g_pVq, (int)hq, g_pVq ? first_byte((void *)g_pVq) : -1, (void *)g_pMprot, (int)hm);
    }
    if (g_pVq == NULL) { printf(TAG " MAP: VirtualQuery did not resolve; nothing to scan.\n"); return; }

    uint64_t cur = 0;
    int total = 0, printed = 0;
    for (int i = 0; i < MAP_MAX_ITER; i++) {
        VqInfo vi;
        int32_t r = vq_at(cur, VQ_FIND_NEXT, &vi);
        if (r != 0) {
            if (i == 0) printf(TAG " MAP: first FIND_NEXT query returned 0x%x (flag value may be wrong); stopping.\n", (unsigned)r);
            break;
        }
        uint64_t s = (uint64_t)vi.start, e = (uint64_t)vi.end;
        if (e <= s || s < cur) break;               // no forward progress
        total++;
        uint64_t sz = e - s;
        if (sz >= (4ULL << 20) && printed < MAP_PRINT_CAP && (verbose || ((vi.flags & VQ_FLAG_DIRECT) && sz >= CAND_MIN && sz <= CAND_MAX))) {
            printed++;
            printf(TAG " MAP: [%d] %llx-%llx size=%llu (%.2f MB) off=%llx prot=0x%x mtype=%d flags=0x%x name=%.31s%s\n",
                   total, (unsigned long long)s, (unsigned long long)e, (unsigned long long)sz, (double)sz / 1048576.0,
                   (unsigned long long)vi.offset, (unsigned)vi.protection, (int)vi.memoryType, (unsigned)vi.flags, vi.name,
                   ((vi.flags & VQ_FLAG_DIRECT) && sz >= CAND_MIN && sz <= CAND_MAX) ? "  <== CANDIDATE" : "");
        }
        if ((vi.flags & VQ_FLAG_DIRECT) && sz >= CAND_MIN && sz <= CAND_MAX && g_nCand < CAND_CAP) {
            struct Cand *c = &g_cand[g_nCand++];
            c->start = s; c->end = e; c->off = vi.offset; c->prot = vi.protection; c->mtype = vi.memoryType; c->flags = vi.flags;
        }
        cur = e;
    }
    printf(TAG " MAP: scan done. regions=%d, printed(>=4MB)=%d, candidates(direct 6-20MB)=%d\n", total, printed, g_nCand);
}

#if DO_MPROTECT_READ
static void cand_probe(void)
{
    int tried = 0;
    for (int k = 0; k < g_nCand; k++) {
        struct Cand *c = &g_cand[k];
        uint64_t len = c->end - c->start;
        VqInfo vi;
        if (vq_at(c->start, 0, &vi) != 0 || (uint64_t)vi.start != c->start || (uint64_t)vi.end != c->end) {
            printf(TAG " CAND#%d %llx: no longer matches its region, skipped\n", k, (unsigned long long)c->start);
            continue;
        }
        if (!(vi.protection & VQ_PROT_CPU_READ)) {
            if (g_pMprot == NULL || tried >= PROT_TRY_CAP) {
                printf(TAG " CAND#%d %llx: not CPU-readable (prot=0x%x) and no mprotect attempt (%s)\n", k,
                       (unsigned long long)c->start, (unsigned)vi.protection, g_pMprot ? "try cap" : "unresolved");
                continue;
            }
            tried++;
            printf(TAG " CAND#%d about to sceKernelMprotect(%llx, %llu, 0x%x) (last line before a crash = this call)\n", k,
                   (unsigned long long)c->start, (unsigned long long)len, (unsigned)(vi.protection | VQ_PROT_CPU_READ));
            int32_t pr = g_pMprot((const void *)c->start, len, vi.protection | VQ_PROT_CPU_READ);
            VqInfo chk;
            int cr = vq_at(c->start, 0, &chk);
            printf(TAG " CAND#%d mprotect returned 0x%x; re-query ret=%d prot=0x%x\n", k, (unsigned)pr, cr, (unsigned)chk.protection);
            if (pr != 0 || cr != 0 || !(chk.protection & VQ_PROT_CPU_READ)) continue;
        }

        // Bounded read-only sample. Re-verify readability immediately before reading.
        for (int pass = 0; pass < 2; pass++) {
            static uint32_t snap[2][SAMPLE_WORDS];
            if (vq_at(c->start, 0, &vi) != 0 || !(vi.protection & VQ_PROT_CPU_READ) || (uint64_t)vi.end != c->end) {
                printf(TAG " CAND#%d became unreadable before pass %d, stopping its sample\n", k, pass);
                break;
            }
            int32_t idx0 = g_lastFlipIndex;
            uint64_t stride = (len / SAMPLE_WORDS) & ~3ULL;
            if (stride == 0) stride = 4;
            uint32_t nz = 0, aff = 0;
            for (int w = 0; w < SAMPLE_WORDS; w++) {
                uint64_t off = (uint64_t)w * stride;
                if (off + 4 > len) { snap[pass][w] = 0; continue; }
                uint32_t v = *(volatile uint32_t *)(c->start + off);
                snap[pass][w] = v;
                if (v) nz++;
                if ((v >> 24) == 0xFF) aff++;
            }
            int32_t idx1 = g_lastFlipIndex;
            printf(TAG " CAND#%d pass%d size=%llu prot=0x%x mtype=%d | sampled %d words: nonzero=%u top-byte-FF=%u | flip idx %d->%d\n",
                   k, pass, (unsigned long long)len, (unsigned)vi.protection, (int)vi.memoryType, SAMPLE_WORDS,
                   (unsigned)nz, (unsigned)aff, (int)idx0, (int)idx1);
            if (pass == 0) {
                for (int q = 0; q < 8; q++) {
                    uint64_t off = ((len / 8) * (uint64_t)q) & ~0xFULL;
                    volatile uint32_t *p = (volatile uint32_t *)(c->start + off);
                    printf(TAG " CAND#%d   +0x%llx: %08x %08x %08x %08x\n", k, (unsigned long long)off,
                           (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);
                }
                usleep(16000);
            } else {
                uint32_t diff = 0;
                for (int w = 0; w < SAMPLE_WORDS; w++) if (snap[0][w] != snap[1][w]) diff++;
                printf(TAG " CAND#%d changed words between passes (~16 ms apart): %u / %d\n", k, (unsigned)diff, SAMPLE_WORDS);
            }
        }
    }
    printf(TAG " CAND: probe done.\n");
}
#endif


// =====================================================================
// [v17] Sampler core. The block between the markers is plain C with no SDK
// dependency: it is compiled and tested on a PC against the real v16-A dumps.
// Tiling formulas: verbatim from the sister project's plugin/source/tiling.c
// (BASE params only, paddedHeight 1088), which decoded all four v16-A buffers.
// =====================================================================
// ---- v17-math-begin ----
typedef struct {
    uint32_t pipeConfig, numPipes, bankWidth, bankHeight, numBanks;
    uint32_t macroTileWidth, macroTileHeight, paddedWidth, tileSplitBytes;
    uint32_t pipeInterleaveBits, pipeBits, bankBits;
} TileParams17;
static const TileParams17 kParamsBase17 = { 0xc, 8, 1, 1, 16, 128, 64, 1920, 512, 8, 3, 4 };
#define BUF_BYTES17 ((uint64_t)1920 * 1088 * 4)       // 8,355,840

static uint32_t t17_elem(uint32_t x, uint32_t y)
{
    uint32_t e = 0;
    e |= ((x >> 0) & 1) << 0; e |= ((x >> 1) & 1) << 1; e |= ((y >> 0) & 1) << 2;
    e |= ((x >> 2) & 1) << 3; e |= ((y >> 1) & 1) << 4; e |= ((y >> 2) & 1) << 5;
    return e;
}
static uint32_t t17_pipe(uint32_t x, uint32_t y)
{
    uint32_t p = 0;
    p |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 1) << 0;
    p |= (((x >> 4) ^ (y >> 4)) & 1) << 1;
    p |= (((x >> 5) ^ (y >> 5)) & 1) << 2;
    return p;
}
static uint32_t t17_log2(uint32_t i) { uint32_t l = 0; while ((i >>= 1) != 0) l++; return l; }
static uint32_t t17_bank(uint32_t x, uint32_t y, const TileParams17 *p)
{
    const uint32_t xs = x >> t17_log2(p->bankWidth * p->numPipes);
    const uint32_t ys = y >> t17_log2(p->bankHeight);
    uint32_t b = 0;
    b |= (((xs >> 3) ^ (ys >> 6)) & 1) << 0;
    b |= (((xs >> 4) ^ (ys >> 5) ^ (ys >> 6)) & 1) << 1;
    b |= (((xs >> 5) ^ (ys >> 4)) & 1) << 2;
    b |= (((xs >> 6) ^ (ys >> 3)) & 1) << 3;
    return b;
}
static uint64_t t17_offset(const TileParams17 *p, uint32_t x, uint32_t y)
{
    uint64_t element_index = t17_elem(x, y);
    uint64_t pipe = t17_pipe(x, y);
    uint64_t bank = t17_bank(x, y, p);
    uint32_t tile_bytes = (8 * 8 * 1 * 32 * 1 + 7) / 8;   // 256
    uint64_t element_offset = element_index * 32;
    uint64_t macro_tile_bytes = (p->macroTileWidth / 8) * (p->macroTileHeight / 8) * tile_bytes / (p->numPipes * p->numBanks);
    uint64_t macro_tiles_per_row = p->paddedWidth / p->macroTileWidth;
    uint64_t macro_tile_index = ((uint64_t)(y / p->macroTileHeight) * macro_tiles_per_row) + (x / p->macroTileWidth);
    uint64_t macro_tile_offset = macro_tile_index * macro_tile_bytes;
    uint64_t tile_row_index = (y / 8) % p->bankHeight;
    uint64_t tile_column_index = ((x / 8) / p->numPipes) % p->bankWidth;
    uint64_t tile_offset = ((tile_row_index * p->bankWidth) + tile_column_index) * tile_bytes;
    uint64_t total_offset = (macro_tile_offset + tile_offset) * 8 + element_offset;
    uint64_t bitOffset = total_offset & 0x7;
    total_offset /= 8;
    uint32_t pipeInterleaveMask = (1u << p->pipeInterleaveBits) - 1;
    uint64_t pipe_interleave_offset = total_offset & pipeInterleaveMask;
    uint64_t offset = total_offset >> p->pipeInterleaveBits;
    uint64_t finalByteOffset = pipe_interleave_offset
        | (pipe << (p->pipeInterleaveBits))
        | (bank << (p->pipeInterleaveBits + p->pipeBits))
        | (offset << (p->pipeInterleaveBits + p->pipeBits + p->bankBits));
    return ((finalByteOffset << 3) | bitOffset) / 8;
}

// =====================================================================
// [v3.9.4 / Stage C merge] PS4-Ambilight's settings, zone geometry and colour chain -- shared code,
// not pasted. Through v24 (the shell-capture-research project's own history) this section was a
// byte-for-byte paste of the sister's source (PS4-Ambilight 89199fe), kept honest by
// tools/host_ini_check/run.sh diffing the two byte for byte. That stopped being possible the moment
// the sister itself changed underneath the paste (v3.9.4 added g_presetOverride to settings.c,
// specifically so this file could stop re-implementing preset selection by hand) -- a paste cannot
// both match its source and diverge from it to add new behaviour. So this merge's Stage C replaces
// the paste with the real thing: tiling.c, pixel_formats.c, zones.c, gamma.c, color_processing.c,
// settings.c and config.c are compiled from ../plugin/source/ (see shell-feed/Makefile's second
// source list) and declared through the sister's own real headers, included below. What stays
// local to this file: the probe's own v17 tiling math just above this comment (kept for the
// buffer-discovery/self-heal code that has always used it, unrelated to the zone sampler), and the
// small shim just below (11 externs the shared files need from sister files this project
// does not link -- network.c, buffer_guard.c, report.c -- each a safe default or a no-op here; see
// each one's own comment for why).
// =====================================================================

#include "ambient_internal.h"
#include "config.h"
#include "preset_select.h"
#include "smoothing.h"

// ---- shim: 11 symbols the 7 shared files above reference from sister files this project doesn't
// link (network.c, buffer_guard.c, report.c). Confirmed to be exactly these 11 by grepping all 7
// shared .c files for every extern ambient_internal.h declares; see shell-feed/docs/findings.md.
uint32_t g_autoLetterboxTop = 0, g_autoLetterboxRight = 0, g_autoLetterboxBottom = 0, g_autoLetterboxLeft = 0;
                                   // real, not a stub: the shell has no letterbox detection, so these
                                   // stay 0 (zones.c's effMargin*() helpers), same as before this refactor --
                                   // just no longer `static`, since zones.c now reads them as its own extern.
volatile uint64_t g_readableLimit = BUF_BYTES17;
                                   // real, not a stub: sampleZoneAverage()'s bounds check (no read past
                                   // one display buffer), same BUF_BYTES17 this probe's own buffer
                                   // discovery already uses and already confirmed on real captures --
                                   // just no longer `static`, since zones.c now reads it as its own extern.
volatile uint32_t g_gpuOnlyRemap = 0;       // GPU-only buffer remap is a game-process concern (buffer_guard.c); unused here
volatile uint32_t g_reportEnabled = 0;      // the sister's own [compat] report_file flag (report.c); this probe tracks its
                                             // OWN report_file setting separately as g_shReportOn (see sh_parse_aux below)
bool g_isBackgrounded = false;              // set by the sister's sample_thread.c/network.c, read inside
                                             // ambient_check_config_reload() (settings.c), which this probe never calls --
                                             // shell_cfg_poll() below is its own, separate poll function
void relay_send_external_source(bool active) { (void)active; } // [network] relay: not part of this design -- see shell-feed/docs/architecture.md "Decisions in force"
void send_config_reload_debug_packet(uint32_t event, uint32_t statErrno, uint64_t curMtime, uint64_t lastMtime,
                                      uint64_t curSize, uint64_t lastSize, uint32_t checkCount,
                                      const uint8_t *contentPreview, uint32_t curHash, uint32_t lastHash)
{ (void)event; (void)statErrno; (void)curMtime; (void)lastMtime; (void)curSize; (void)lastSize;
  (void)checkCount; (void)contentPreview; (void)curHash; (void)lastHash; } // diagnostic-only, part of
                                             // ambient_check_config_reload(), which this probe never calls
void ambient_read_content_preview(uint8_t *out, size_t previewLen) { memset(out, 0, previewLen); } // feeds the no-op packet above
void debug_send_raw(const uint8_t *data, int len) { (void)data; (void)len; } // zones.c's own [dev]-gated diagnostic
                                             // packets (HDR2200 detection etc.), separate from this probe's [dev] mirror

// ---------------------------------------------------------------------
// [v22] Glue: this project's own code from here to the end of the block.
// ---------------------------------------------------------------------
static uint8_t g_smoothedRgb[MAX_TOTAL_ZONES][3];  // this file's own smoothing STATE (the maths is smoothing.h's) -- private, per the
                                                     // comment above ambient_internal.h's g_smoothedRgbValid extern
bool g_smoothedRgbValid = false; // ambient_internal.h declares this extern (real definition's normally sample_thread.c's,
                                  // which this project doesn't link); false until the first real frame, and again after a reload

// The compositor's display buffers are A8R8G8B8 words (0xAARRGGBB), the same layout v17's sampler reads.
static void unpack_argb(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((px >> 16) & 255); *g = (uint8_t)((px >> 8) & 255); *b = (uint8_t)(px & 255);
}

static const char *g_cfgPreset = "defaults"; // which ini section supplied the preset-owned settings, for the log
static int      g_cfgHaveBaseline = 0;
static int64_t  g_cfgLastSize = 0;
static uint32_t g_cfgLastHash = 0;
static int      g_cfgLastReadOk = 1;
static uint64_t g_cfgNextPollTsc = 0;
static uint32_t g_cfgReloads = 0;
static volatile int g_shReportOn = 0; // [v24] [compat] report_file=1 (sister's rule: 0/1 only, else the value stands)
static int      g_shellEnabled = 1;  // [v23] [shell] enabled=; default true; updated on every reload
static int      g_liveSendEverStarted = 0; // [v23-fix v44] set after live_send()'s first-ever config
                                            // read; makes every later entry (i.e. every resume out of
                                            // wait_for_reenable()) log as INI RELOAD and count toward
                                            // g_cfgReloads/ini_reloads, instead of logging as a fresh
                                            // LOAD that silently didn't increment the counter. Found in
                                            // capture v23-B (2026-10-06): the resume's own config read
                                            // printed "INI LOAD", not "INI RELOAD", so ini_reloads under-
                                            // counted every resume.

// [v24] The three keys the shell reads that are not in the sister's config struct: [compat] report_file and
// [dev] dev_ip / dev_logging, with the sister's own rules (a missing or out-of-range key leaves the value as it was).
// g_shDevLog / g_shDevIp are defined with the v24 portable block further down; declared here for the loader.
static int  g_shDevLog;
static char g_shDevIp[16];
static void sh_parse_aux(ini_table_s *table)
{
    const char *v; int iv; bool bv;
    if (ini_table_get_entry_as_int(table, "compat", "report_file", &iv) && (iv == 0 || iv == 1)) g_shReportOn = iv;
    if ((v = ini_table_get_entry(table, "dev", "dev_ip")) != NULL) {
        strncpy(g_shDevIp, v, sizeof(g_shDevIp) - 1);
        g_shDevIp[sizeof(g_shDevIp) - 1] = '\0';
    }
    if (ini_table_get_entry_as_bool(table, "dev", "dev_logging", &bv)) g_shDevLog = bv ? 1 : 0;
}

// Reads the ini into g_config. Stage C of the PS4-Ambilight/ps4-shell-capture-research merge: this now
// calls the sister's real ambient_load_config() (plugin/source/settings.c, linked, not pasted) for
// everything [compat] gpu_only_remap/report_file included, instead of re-walking the ini key by key
// by hand. The one deliberate difference from a game's own load -- [preset_game] whatever [presets]
// active= says, no media title can turn it into "movie" (the shell is not a title) -- is now expressed
// through g_presetOverride (ambient_internal.h/settings.c, added alongside this refactor) rather than
// by duplicating preset_section_pick()'s own call here.
// Returns 1 if the file was read and parsed; on 0 g_config is untouched and the caller keeps running on
// it. Like the sister: a key that is missing or out of range keeps the value g_config already has.
static int shell_cfg_load(void)
{
    g_presetOverride = PRESET_SECTION_GAME; // wins over active=/media title inside ambient_load_config(); see its comment
    ambient_load_config();
    g_cfgPreset = PRESET_SECTION_GAME; // ambient_load_config() doesn't report back which section it used; this is what we forced

    // [compat] report_file and [dev] dev_ip/dev_logging: shell-only copies (g_shReportOn/g_shDevIp/g_shDevLog),
    // separate from the fields ambient_load_config() just set on g_config itself. [shell] enabled= is a
    // probe-only key the sister's struct has no field for at all. None of these three are in the sister's
    // ambient_load_config(), so they still need their own, separate ini read -- the same pattern
    // shell_enabled_poll() below already uses on its own cadence.
    ini_table_s *table = ini_table_create();
    if (table == NULL || !ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) return 0; // config read via ambient_load_config() above still stands; only these three fall back
    sh_parse_aux(table); // [v24]
    bool bv;
    g_shellEnabled = ini_table_get_entry_as_bool(table, "shell", "enabled", &bv) ? (bv ? 1 : 0) : 1; // absent = enabled
    ini_table_destroy(table);
    return 1;
}

// One line with every value the shell is running on, printed after each (re)load and geometry rebuild, so a
// log shows exactly which ini the strip followed.
static void shell_cfg_log(const char *why, int64_t bytes, uint32_t hash)
{
    printf(TAG " INI %s: bytes=%lld hash=%08x preset=%s | host=%s:%u zones=%u (T%u R%u B%u L%u) corner=%d dir=%d offset=%d order=%d |"
           " scan_depth=%u brightness=%u saturation=%d gamma_idx=%u contrast=%d black=%u white=%u dark=%u smoothing=%d settle_ms=%u hz=%u |"
           " bright_rgb=%u/%u/%u gamma_rgb=%u/%u/%u\n",
           why, (long long)bytes, (unsigned)hash, g_cfgPreset, g_config.wledHost, (unsigned)g_config.wledPort, (unsigned)g_numZones,
           (unsigned)g_config.ledCountTop, (unsigned)g_config.ledCountRight, (unsigned)g_config.ledCountBottom, (unsigned)g_config.ledCountLeft,
           (int)g_config.startCorner, (int)g_config.direction, (int)g_config.ledOffset, (int)g_config.colorOrder,
           (unsigned)g_config.scanDepth, (unsigned)g_config.brightness, (int)g_config.saturation, (unsigned)g_config.gammaLutIndex,
           (int)g_config.contrast, (unsigned)g_config.blackLevel, (unsigned)g_config.whiteLevel, (unsigned)g_config.darkThreshold,
           (int)g_config.smoothingEnabled, (unsigned)g_config.settlingTimeMs, (unsigned)g_config.updateFrequencyHz,
           (unsigned)g_config.brightnessR, (unsigned)g_config.brightnessG, (unsigned)g_config.brightnessB,
           (unsigned)g_config.gammaR, (unsigned)g_config.gammaG, (unsigned)g_config.gammaB);
}

// Live reload, the sister's rule: every configReloadCheckSeconds, read the file through the raw syscalls,
// and reload when its size OR its content hash changed. The first check only records a baseline.
// Returns 1 when settings were reloaded (the caller then recomputes its pace and destination).
static int shell_cfg_poll(uint64_t nowTsc, uint64_t tscHz)
{
    if (g_config.configReloadCheckSeconds == 0 || tscHz == 0) return 0;
    if (nowTsc < g_cfgNextPollTsc) return 0;
    g_cfgNextPollTsc = nowTsc + tscHz * (uint64_t)g_config.configReloadCheckSeconds;

    int64_t sz = 0; uint32_t h = 0;
    if (!ambient_get_real_config_size_and_hash(&sz, &h)) {
        if (g_cfgLastReadOk) printf(TAG " INI unreadable (%lld): keeping the current settings\n", (long long)sz);
        g_cfgLastReadOk = 0;
        return 0;
    }
    if (!g_cfgLastReadOk) printf(TAG " INI readable again\n");
    g_cfgLastReadOk = 1;
    if (!g_cfgHaveBaseline) { g_cfgLastSize = sz; g_cfgLastHash = h; g_cfgHaveBaseline = 1; return 0; }
    if (sz == g_cfgLastSize && h == g_cfgLastHash) return 0;
    g_cfgLastSize = sz; g_cfgLastHash = h;

    if (!shell_cfg_load()) {
        printf(TAG " INI RELOAD: file changed (bytes=%lld hash=%08x) but could not be parsed; keeping the current settings\n",
               (long long)sz, (unsigned)h);
        return 0;
    }
    buildZoneGeometry();            // layout settings may have changed
    g_smoothedRgbValid = false;     // a hard cut between the old and new settings, not a blend
    g_cfgReloads++;
    shell_cfg_log("RELOAD", sz, h);
    return 1;
}

// [v46] Found in capture v23-C (2026-10-07, person-reported): with config_reload_check_seconds=0,
// shell_cfg_poll() above never runs while live_send() is sending, so [shell] enabled= could only be
// noticed at the very start of a live_send() session -- a real gap, not fixed by v45 (which only
// covers standby, where nothing is being sent and a full reload is free). The person asked for the
// kill switch to work the same way the disable file already does: checked on its own cadence,
// independent of config_reload_check_seconds, while everything else in the ini (brightness, zones,
// host, ...) keeps following config_reload_check_seconds exactly as before -- this deliberately does
// NOT call shell_cfg_load() (which would reload everything) or touch g_config, buildZoneGeometry(),
// or the gamma LUTs. Logged separately from "INI RELOAD" and does not increment g_cfgReloads, since
// nothing but the one key changed.
#define ENABLED_POLL_MS 1000u
static void shell_enabled_poll(uint64_t nowTsc, uint64_t tscHz, uint64_t *nextPollTsc)
{
    if (tscHz == 0 || nowTsc < *nextPollTsc) return;
    *nextPollTsc = nowTsc + tscHz * ENABLED_POLL_MS / 1000u;

    ini_table_s *table = ini_table_create();
    if (table == NULL || !ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) return; // unreadable: leave g_shellEnabled as it was, same rule as shell_cfg_load()

    int was = g_shellEnabled;
    bool bv;
    g_shellEnabled = ini_table_get_entry_as_bool(table, "shell", "enabled", &bv) ? (bv ? 1 : 0) : 1; // absent = enabled
    ini_table_destroy(table);

    if (g_shellEnabled != was)
        printf(TAG " SHELL ENABLED POLL: [shell] enabled= now %s (checked every %us, independent of config_reload_check_seconds)\n",
               g_shellEnabled ? "true" : "false", (unsigned)(ENABLED_POLL_MS / 1000u));
}

// One frame: every zone sampled with scan_depth, run through the sister's colour chain, dark threshold and
// smoothing, written to rgbTriplets in LED order (zone i IS LED i: buildZoneGeometry() already applied the
// start corner, direction and offset). Returns the zone count. This is the sister's sample_thread.c pass 3
// without the game-process-only parts (see the header comment).
static uint32_t shell_process_frame(uint64_t bufferAddr, uint32_t sampleIntervalUs, uint8_t *rgbTriplets)
{
    uint32_t smoothingAlpha = smoothing_alpha(sampleIntervalUs, g_config.settlingTimeMs);
    for (uint32_t i = 0; i < g_numZones; i++) {
        uint8_t r, g, b;
        sampleZoneAverage(&kParamsBase, bufferAddr, unpack_argb, g_zoneX[i], g_zoneY[i], &r, &g, &b);
        uint8_t verified[3];
        applyColorProcessing(r, g, b, verified);
        applyDarkThreshold(i, &verified[0], &verified[1], &verified[2]);
                    if (g_config.smoothingEnabled) {
                        smoothing_step(g_smoothedRgb[i], verified, smoothingAlpha, g_smoothedRgbValid);
                        rgbTriplets[i*3+0] = g_smoothedRgb[i][0];
                        rgbTriplets[i*3+1] = g_smoothedRgb[i][1];
                        rgbTriplets[i*3+2] = g_smoothedRgb[i][2];
                    } else {
                        rgbTriplets[i*3+0] = verified[0];
                        rgbTriplets[i*3+1] = verified[1];
                        rgbTriplets[i*3+2] = verified[2];
                    }
    }
    g_smoothedRgbValid = true;
    return g_numZones;
}

#define SCR_W      1920
#define SCR_H      1080
#define LED_TOP    73
#define LED_RIGHT  41
#define LED_BOTTOM 73
#define LED_LEFT   42
#define NZ         (LED_TOP + LED_RIGHT + LED_BOTTOM + LED_LEFT)   // 229
#define ZPTS       4            // 2x2 points per zone
#define BAND_TB    120          // top/bottom zone depth, px
#define BAND_LR    240          // left/right zone depth, px
#define SC_PTS     128          // scattered points for the bar analysis

// [v21] This probe's zone order is top L->R (73), right T->B (41), bottom R->L (73), left B->T (42),
// zone 0 at the top-left corner. The sister plugin's order for the person's ini (bottom_left,
// clockwise, offset 0) starts at the bottom-left, 42 LEDs (one left edge) earlier, so zone z
// belongs at LED (z + LED_ROT) mod NZ. Verified on a PC: docs/2026-10-05_zone_order_check.md.
// Hard-coded for that ini; an ini-driven layout comes later.
#define LED_ROT    LED_LEFT

// Copies NZ zone colours into a DDP payload in LED order.
static void zones_to_leds(uint8_t *dst, const uint8_t (*rgb)[3])
{
    for (int z = 0; z < NZ; z++) {
        int led = (z + LED_ROT) % NZ;
        dst[3 * led + 0] = rgb[z][0];
        dst[3 * led + 1] = rgb[z][1];
        dst[3 * led + 2] = rgb[z][2];
    }
}

static uint32_t g_ptOff[NZ * ZPTS];
static uint16_t g_ptX[NZ * ZPTS], g_ptY[NZ * ZPTS];
static uint32_t g_scOff[SC_PTS];
static uint16_t g_scX[SC_PTS], g_scY[SC_PTS];
static int      g_badPts = 0;

// Zone order: 0..72 top (left->right), 73..113 right (top->bottom),
// 114..186 bottom (right->left), 187..228 left (bottom->top). Physical LED
// mapping (start corner / direction / offset) is the sister project's config job.
static void zone_rect(int z, int *x0, int *y0, int *x1, int *y1)
{
    if (z < LED_TOP) {
        int k = z;
        *x0 = k * SCR_W / LED_TOP; *x1 = (k + 1) * SCR_W / LED_TOP; *y0 = 0; *y1 = BAND_TB;
    } else if (z < LED_TOP + LED_RIGHT) {
        int k = z - LED_TOP;
        *y0 = k * SCR_H / LED_RIGHT; *y1 = (k + 1) * SCR_H / LED_RIGHT; *x0 = SCR_W - BAND_LR; *x1 = SCR_W;
    } else if (z < LED_TOP + LED_RIGHT + LED_BOTTOM) {
        int c = LED_BOTTOM - 1 - (z - LED_TOP - LED_RIGHT);
        *x0 = c * SCR_W / LED_BOTTOM; *x1 = (c + 1) * SCR_W / LED_BOTTOM; *y0 = SCR_H - BAND_TB; *y1 = SCR_H;
    } else {
        int c = LED_LEFT - 1 - (z - LED_TOP - LED_RIGHT - LED_BOTTOM);
        *y0 = c * SCR_H / LED_LEFT; *y1 = (c + 1) * SCR_H / LED_LEFT; *x0 = 0; *x1 = BAND_LR;
    }
}

static void build_points(void)
{
    for (int z = 0; z < NZ; z++) {
        int x0, y0, x1, y1;
        zone_rect(z, &x0, &y0, &x1, &y1);
        for (int j = 0; j < ZPTS; j++) {
            int fx = (j & 1) ? 3 : 1, fy = (j >> 1) ? 3 : 1;
            int x = x0 + (x1 - x0) * fx / 4, y = y0 + (y1 - y0) * fy / 4;
            if (x > SCR_W - 1) x = SCR_W - 1;
            if (y > SCR_H - 1) y = SCR_H - 1;
            uint64_t off = t17_offset(&kParamsBase17, (uint32_t)x, (uint32_t)y);
            if (off + 4 > BUF_BYTES17) { off = 0; g_badPts++; }
            g_ptX[z * ZPTS + j] = (uint16_t)x; g_ptY[z * ZPTS + j] = (uint16_t)y;
            g_ptOff[z * ZPTS + j] = (uint32_t)off;
        }
    }
    uint32_t s = 0x1234567u;
    for (int i = 0; i < SC_PTS; i++) {
        s = s * 1664525u + 1013904223u; int x = (int)((s >> 8) % SCR_W);
        s = s * 1664525u + 1013904223u; int y = (int)((s >> 8) % SCR_H);
        uint64_t off = t17_offset(&kParamsBase17, (uint32_t)x, (uint32_t)y);
        if (off + 4 > BUF_BYTES17) { off = 0; g_badPts++; }
        g_scX[i] = (uint16_t)x; g_scY[i] = (uint16_t)y; g_scOff[i] = (uint32_t)off;
    }
}

static inline uint32_t rd32_17(uint64_t a) { return *(volatile uint32_t *)a; }

// Word layout A8R8G8B8: 0xAARRGGBB. Average of ZPTS points per zone.
static void sample_zones(uint64_t base, uint8_t rgb[][3])
{
    for (int z = 0; z < NZ; z++) {
        uint32_t r = 0, g = 0, b = 0;
        for (int j = 0; j < ZPTS; j++) {
            uint32_t w = rd32_17(base + g_ptOff[z * ZPTS + j]);
            r += (w >> 16) & 255; g += (w >> 8) & 255; b += w & 255;
        }
        rgb[z][0] = (uint8_t)(r / ZPTS); rgb[z][1] = (uint8_t)(g / ZPTS); rgb[z][2] = (uint8_t)(b / ZPTS);
    }
}

// The offline-measured bar colour class (v16-A): R ~= G, B a little higher, mid brightness.
static int is_lav17(uint32_t w)
{
    int r = (w >> 16) & 255, g = (w >> 8) & 255, b = w & 255;
    int dg = r > g ? r - g : g - r;
    return r >= 0x60 && dg <= 4 && (b - r) >= 6 && (b - r) <= 40;
}
// ---- v17-math-end ----

static inline uint64_t rdtsc17(void)
{
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

struct Buf { uint64_t start, len; };
static struct Buf g_buf[2];
static int        g_nBuf = 0;
static uint64_t   g_tscHz = 0;

static int buf_ok(const struct Buf *b)
{
    VqInfo vi;
    return vq_at(b->start, 0, &vi) == 0 && (uint64_t)vi.start == b->start && (uint64_t)vi.end == b->start + b->len
        && (vi.protection & VQ_PROT_CPU_READ) && (vi.flags & VQ_FLAG_DIRECT);
}

// Display buffers = direct, already CPU-readable, named *compositor*, exactly 8,355,840 bytes.
static void find_bufs(void)
{
    g_nBuf = 0;
    int others = 0;
    for (int k = 0; k < g_nCand; k++) {
        VqInfo vi;
        uint64_t len = g_cand[k].end - g_cand[k].start;
        if (vq_at(g_cand[k].start, 0, &vi) != 0 || !(vi.flags & VQ_FLAG_DIRECT) || !(vi.protection & VQ_PROT_CPU_READ)
            || !name_has_ci(vi.name, "compositor")) continue;
        if (len == BUF_BYTES17 && g_nBuf < 2) { g_buf[g_nBuf].start = g_cand[k].start; g_buf[g_nBuf].len = len; g_nBuf++; }
        else others++;
    }
    if (g_nBuf == 2 && g_buf[1].start < g_buf[0].start) { struct Buf t = g_buf[0]; g_buf[0] = g_buf[1]; g_buf[1] = t; }
    printf(TAG " BUFS: %d display buffer(s) matched (compositor, direct, CPU-readable, %llu B); other compositor-named 6-20MB regions: %d\n",
           g_nBuf, (unsigned long long)BUF_BYTES17, others);
    for (int b = 0; b < g_nBuf; b++)
        printf(TAG " BUFS: buf%d start=%llx len=%llu\n", b, (unsigned long long)g_buf[b].start, (unsigned long long)g_buf[b].len);
}

static void calibrate_tsc(void)
{
    uint32_t f0 = g_flipCount; uint64_t a = rdtsc17();
    usleep(1000000);
    uint64_t b = rdtsc17(); uint32_t f1 = g_flipCount;
    uint32_t df = f1 - f0;
    if (df >= 30) g_tscHz = (uint64_t)((double)(b - a) * 59.94 / (double)df);   // output is 59.94 Hz per VideoOutStatus
    else          g_tscHz = (b - a);                                            // fallback: assumes usleep(1 s) was accurate
    printf(TAG " TSC: %llu ticks in ~1 s over %u compositor flips -> %llu Hz (%s)\n",
           (unsigned long long)(b - a), (unsigned)df, (unsigned long long)g_tscHz, df >= 30 ? "flip-rate based" : "usleep based, flips<30");
}

#define BURST_TICKS 300
struct TickRec { uint64_t t0, t1; int32_t idx0, idx1; uint32_t zt[2]; uint32_t sc[2][SC_PTS]; };
static struct TickRec g_rec[BURST_TICKS];
struct BurstHdr { uint32_t magic, ver, nBuf, ticks, scPts, recSize, nz, zpts; uint64_t tscHz; uint64_t bufStart[2]; };

static void run_burst(int scene)
{
    if (g_nBuf == 0 || g_tscHz == 0) { printf(TAG " BURST s%d: skipped (no buffers or no TSC calibration)\n", scene); return; }
    static uint8_t zc[2][NZ][3];
    int done = 0;
    printf(TAG " BURST s%d: about to run %d ticks (reads only; both regions re-verified every tick)\n", scene, BURST_TICKS);
    for (int k = 0; k < BURST_TICKS; k++) {
        int ok = buf_ok(&g_buf[0]);
        for (int b = 1; b < g_nBuf; b++) ok = ok && buf_ok(&g_buf[b]);
        if (!ok) { printf(TAG " BURST s%d: a display buffer changed or vanished at tick %d; stopped\n", scene, k); break; }
        struct TickRec *r = &g_rec[k];
        memset(r->sc, 0, sizeof(r->sc)); r->zt[0] = r->zt[1] = 0;
        r->t0 = rdtsc17(); r->idx0 = g_lastFlipIndex;
        for (int b = 0; b < g_nBuf; b++) {
            uint64_t za = rdtsc17();
            for (int i = 0; i < SC_PTS; i++) r->sc[b][i] = rd32_17(g_buf[b].start + g_scOff[i]);
            sample_zones(g_buf[b].start, zc[b]);
            r->zt[b] = (uint32_t)(rdtsc17() - za);
        }
        r->idx1 = g_lastFlipIndex; r->t1 = rdtsc17();
        done++;
        usleep(1000);
    }
    if (done == 0) return;

    // ---- summary for the log ----
    uint64_t sumTick = 0, maxTick = 0, minTick = ~0ULL, sumZt[2] = {0, 0};
    uint32_t stable = 0, idxMoved = 0, lav[2][2] = {{0, 0}, {0, 0}}, lavTot[2][2] = {{0, 0}, {0, 0}}, chg[2] = {0, 0};
    for (int k = 0; k < done; k++) {
        struct TickRec *r = &g_rec[k];
        uint64_t d = r->t1 - r->t0; sumTick += d; if (d > maxTick) maxTick = d; if (d < minTick) minTick = d;
        sumZt[0] += r->zt[0]; sumZt[1] += r->zt[1];
        if (r->idx0 != r->idx1) idxMoved++;
        if (r->idx0 == r->idx1 && (r->idx0 == 0 || r->idx0 == 1)) {
            stable++;
            for (int b = 0; b < g_nBuf; b++) {
                int m = (r->idx0 == b) ? 1 : 0;        // m=1: this buffer's index is the one just flipped to
                for (int i = 0; i < SC_PTS; i++) { lavTot[b][m]++; if (is_lav17(r->sc[b][i])) lav[b][m]++; }
            }
        }
        if (k > 0) for (int b = 0; b < g_nBuf; b++)
            for (int i = 0; i < SC_PTS; i++) if (r->sc[b][i] != g_rec[k - 1].sc[b][i]) chg[b]++;
    }
    double us = 1e6 / (double)g_tscHz;
    printf(TAG " BURST s%d: %d ticks, tick us avg/min/max = %.0f/%.0f/%.0f; full 229-zone+128-point pass us per buffer avg = %.0f / %.0f; flip idx moved inside %u ticks; %u ticks had a steady idx 0/1\n",
           scene, done, (double)sumTick / done * us, (double)minTick * us, (double)maxTick * us,
           (double)sumZt[0] / done * us, g_nBuf > 1 ? (double)sumZt[1] / done * us : 0.0, (unsigned)idxMoved, (unsigned)stable);
    for (int b = 0; b < g_nBuf; b++)
        printf(TAG " BURST s%d: buf%d grey-lavender scatter hits: when idx==%d: %u/%u | when idx!=%d: %u/%u | scatter words changed tick-to-tick: %u of %d\n",
               scene, b, b, (unsigned)lav[b][1], (unsigned)lavTot[b][1], b, (unsigned)lav[b][0], (unsigned)lavTot[b][0],
               (unsigned)chg[b], (done > 1 ? (done - 1) * SC_PTS : 0));

    // ---- raw record file for offline analysis ----
    char path[64];
    snprintf(path, sizeof(path), "/data/shell_probe_v17_s%d_burst.bin", scene);
    int32_t fd = sceKernelOpen(path, 0x200 | 0x400 | 0x001, 0777);
    if (fd < 0) { printf(TAG " BURST s%d: cannot create %s (%d)\n", scene, path, (int)fd); return; }
    struct BurstHdr h; memset(&h, 0, sizeof(h));
    h.magic = 0x42373156u; h.ver = 1; h.nBuf = (uint32_t)g_nBuf; h.ticks = (uint32_t)done; h.scPts = SC_PTS;
    h.recSize = sizeof(struct TickRec); h.nz = NZ; h.zpts = ZPTS; h.tscHz = g_tscHz;
    for (int b = 0; b < g_nBuf; b++) h.bufStart[b] = g_buf[b].start;
    int64_t w1 = sceKernelWrite(fd, &h, sizeof(h));
    int64_t w2 = sceKernelWrite(fd, g_scX, sizeof(g_scX));
    int64_t w3 = sceKernelWrite(fd, g_scY, sizeof(g_scY));
    int64_t w4 = sceKernelWrite(fd, g_rec, (uint64_t)done * sizeof(struct TickRec));
    sceKernelClose(fd);
    printf(TAG " BURST s%d: wrote %s (hdr %lld, x %lld, y %lld, recs %lld of %llu bytes)\n", scene, path,
           (long long)w1, (long long)w2, (long long)w3, (long long)w4, (unsigned long long)done * sizeof(struct TickRec));
}

static void ddp_preview(int scene, int c, uint8_t rgb[][3])
{
    uint8_t pk[10 + 3 * NZ];
    int dataSize = 3 * NZ;
    pk[0] = 0x41; pk[1] = 0; pk[2] = 0x0B; pk[3] = 0x01; pk[4] = pk[5] = pk[6] = pk[7] = 0;
    pk[8] = (uint8_t)((dataSize >> 8) & 255); pk[9] = (uint8_t)(dataSize & 255);
    zones_to_leds(pk + 10, (const uint8_t (*)[3])rgb);   // [v21] LED order, not zone order
    printf(TAG " DDP s%d c%d: %d-byte packet built (header %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x, %d zones); first zones rgb: %02x%02x%02x %02x%02x%02x %02x%02x%02x [send %s]\n",
           scene, c, 10 + dataSize, pk[0], pk[1], pk[2], pk[3], pk[4], pk[5], pk[6], pk[7], pk[8], pk[9], NZ,
           rgb[0][0], rgb[0][1], rgb[0][2], rgb[1][0], rgb[1][1], rgb[1][2], rgb[2][0], rgb[2][1], rgb[2][2],
           DO_WLED_SEND ? "ON" : "compiled out");
#if DO_WLED_SEND
    // Mirrors the sister project's network.c (one UDP packet, 5 ms send timeout). Off by default.
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct timeval to = { 0, 5000 };
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
        struct sockaddr_in d; memset(&d, 0, sizeof(d));
        d.sin_family = AF_INET; d.sin_port = htons(WLED_PORT);
        if (inet_pton(AF_INET, WLED_HOST, &d.sin_addr) == 1)
            sendto(sock, pk, 10 + dataSize, 0, (struct sockaddr *)&d, sizeof(d));
        close(sock);
    }
#endif
}

// ---------------------------------------------------------------------
// [v24] Phase 3 leftovers (docs/integration_plan_v2.md, Phase 3): report file, [dev] UDP mirror,
// self-heal, one log line per gate. Probe-only: nothing shared with the sister is touched.
// The region below is host-tested by tools/host_v24_check/run.sh, which cuts it out of THIS file
// between the portable-begin/end markers and builds it on a PC (real sockets, real heal timeline).
// ---------------------------------------------------------------------
// ---- v24-portable-begin ----

// --- self-heal: when does a run of bad buffer checks justify re-running map_scan()+find_bufs()? ---
#define HEAL_FAIL_THRESHOLD   30u     // consecutive ticks that needed a buffer and found it bad (~1 s at 30 Hz)
#define HEAL_COOLDOWN_MIN_MS  10000u  // never re-scan more often than this ...
#define HEAL_COOLDOWN_MAX_MS  60000u  // ... doubling after each re-scan that did not help, up to this

typedef struct {
    uint32_t consecFail;     // bad ticks in a row; any good tick resets it
    uint32_t heals;          // re-scans started
    uint32_t recovered;      // re-scans followed by a good tick
    uint32_t failed;         // re-scans that found no display buffer at all
    uint32_t unrecovered;    // re-scans in a row with no good tick since; drives the back-off
    uint64_t lastHealMs;
    int      awaiting;       // a re-scan happened and no good tick has followed yet
} HealState;

static uint32_t heal_cooldown_ms(const HealState *h)
{
    uint32_t c = HEAL_COOLDOWN_MIN_MS;
    for (uint32_t i = 1; i < h->unrecovered && c < HEAL_COOLDOWN_MAX_MS; i++) c *= 2u;
    return c > HEAL_COOLDOWN_MAX_MS ? HEAL_COOLDOWN_MAX_MS : c;
}

// One call per tick that needed a buffer (ticks the sister's claim skipped never get here).
// Returns 1 when the caller should now re-scan.
static int heal_tick(HealState *h, int bufOk, uint64_t nowMs)
{
    if (bufOk) {
        if (h->awaiting) { h->recovered++; h->awaiting = 0; }
        h->consecFail = 0; h->unrecovered = 0;
        return 0;
    }
    if (h->consecFail < 0xFFFFFFFFu) h->consecFail++;
    if (h->consecFail < HEAL_FAIL_THRESHOLD) return 0;
    if (h->heals > 0 && nowMs >= h->lastHealMs && nowMs - h->lastHealMs < heal_cooldown_ms(h)) return 0;
    h->heals++; h->unrecovered++; h->lastHealMs = nowMs; h->awaiting = 1;
    return 1;
}

static void heal_done(HealState *h, int buffersFound) { if (buffersFound <= 0) h->failed++; }

// --- gates: one table, one log line each (the v18 lesson). A gate macro that is not defined is a
// compile ERROR here (not a silent #if -> 0), because the table uses the macro's value directly. ---
typedef struct { const char *name; int on; int critical; const char *whenOff; } ShGate;
static const ShGate kGates[] = {
    { "DO_WLED_SEND",             DO_WLED_SEND,             1, "no socket headers, no DDP send: the strip is never driven" },
    { "DO_LIVE_DDP",              DO_LIVE_DDP,              1, "live_send() is not compiled: the strip is never driven (v18's failure)" },
    { "DO_CONTROL_CALLS",         DO_CONTROL_CALLS,         0, "the two hook positive-control calls are skipped" },
    { "ALLOW_ANY_PROCESS",        ALLOW_ANY_PROCESS,        0, "hooks go in GnmCompositor only (intended)" },
    { "ALLOW_KERNEL_PRX_LOAD",    ALLOW_KERNEL_PRX_LOAD,    0, "resolve-only: no kernel PRX is loaded to find symbols" },
    { "DO_MPROTECT_READ",         DO_MPROTECT_READ,         0, "no mprotect/readability probe of candidate regions (intended)" },
    { "DO_FLIPSTATUS_CALL",       DO_FLIPSTATUS_CALL,       0, "sceVideoOutGetFlipStatus is resolved but never called (intended)" },
    { "DO_RESOLUTIONSTATUS_CALL", DO_RESOLUTIONSTATUS_CALL, 0, "sceVideoOutGetResolutionStatus is resolved but never called (intended)" },
    { "DO_DUMP_BUFFERS",          DO_DUMP_BUFFERS,          0, "v17 buffer dumps not compiled in (intended)" },
    { "DO_SAMPLER_SCENES",        DO_SAMPLER_SCENES,        0, "v17 fixed-scene schedule not used (intended; no #if depends on it in v24)" },
    { "SHELL_REPORT_FILE",        SHELL_REPORT_FILE,        0, "no report thread: [compat] report_file=1 does nothing" },
    { "SHELL_DEV_MIRROR",         SHELL_DEV_MIRROR,         0, "no [dev] UDP mirror: dev_logging=true does nothing" },
    { "SHELL_SELF_HEAL",          SHELL_SELF_HEAL,          0, "a bad display buffer is skipped forever, never re-scanned" },
};
#define SH_GATE_COUNT ((int)(sizeof(kGates) / sizeof(kGates[0])))

// Writes the line for gate i into out. ON gates are one short line; OFF gates say what is missing.
static void sh_gate_line(char *out, size_t cap, int i)
{
    const ShGate *g = &kGates[i];
    if (g->on) snprintf(out, cap, "GATE: %-24s = 1 (ON)", g->name);
    else       snprintf(out, cap, "GATE: %-24s = 0 (OFF%s) -- %s", g->name, g->critical ? ", CRITICAL" : "", g->whenOff);
}

// --- the report file: /data/ps4_ambient_report_SHELL.txt, same opt-in and layout idea as the sister's report ---
#define SH_REPORT_PATH          "/data/ps4_ambient_report_SHELL.txt"
#define SH_REPORT_TEXT_MAX      2048
#define SH_REPORT_REFRESH_SECS  60    // rewrite this often even when nothing changed (counters)
#define SH_REPORT_MAX_FAILURES  3     // then stop trying for the session, like the sister

enum { SH_STARTING = 0, SH_LIVE = 1, SH_STANDBY = 2, SH_STOPPED = 3 };

typedef struct {
    int      state;             // SH_*
    int      stopReason;        // live_send()'s last return: 0 never started, 1 disable file, 2 [shell] enabled=false
    char     notStartedWhy[40]; // why live_send() returned 0, "" otherwise
    uint32_t uptimeSecs;
    uint32_t liveSessions;      // times live_send() reached its send loop
    uint64_t sent, skipped, gameOwned;
    uint32_t flips;             // compositor flips counted by the hook
    int      bufCount;
    uint32_t consecFail, heals, healsRecovered, healsFailed;
    int      claimEverSeen, claimActive;
    uint32_t iniReloads;
    int      iniReadOk, shellEnabled, devMirror;
} ShReport;

static const char *sh_state_name(int s)
{
    switch (s) { case SH_STARTING: return "starting"; case SH_LIVE: return "live";
                 case SH_STANDBY: return "standby";   case SH_STOPPED: return "stopped"; default: return "unknown"; }
}
static const char *sh_stop_name(int r)
{
    switch (r) { case 0: return "none"; case 1: return "disable file present"; case 2: return "[shell] enabled=false"; default: return "unknown"; }
}

// True when something a person reading the file cares about moved. Per-frame counters are left out on
// purpose (the refresh timer covers them), or this would rewrite the file every second.
static int sh_report_changed(const ShReport *a, const ShReport *b)
{
    return a->state != b->state || a->stopReason != b->stopReason
        || strcmp(a->notStartedWhy, b->notStartedWhy) != 0
        || a->liveSessions != b->liveSessions || a->bufCount != b->bufCount
        || a->heals != b->heals || a->healsFailed != b->healsFailed
        || a->claimEverSeen != b->claimEverSeen || a->iniReadOk != b->iniReadOk
        || a->shellEnabled != b->shellEnabled || a->devMirror != b->devMirror
        || (a->sent > 0) != (b->sent > 0);
}

// Same rule as the sister's tick: write the first time, on a change, and every REFRESH seconds.
// sinceWrite must already count this turn.
static int sh_report_due(const ShReport *last, const ShReport *cur, int wroteOnce, uint32_t sinceWrite)
{
    if (!wroteOnce) return 1;
    return sh_report_changed(last, cur) || sinceWrite >= SH_REPORT_REFRESH_SECS;
}

// No WLED address, no ini values, nothing typed -- same promise as the sister's file. Last line is always "end of report".
static size_t sh_report_format(char *out, size_t cap, const ShReport *s)
{
    if (cap == 0) return 0;
    size_t n = 0;
    int w = snprintf(out, cap,
        "PS4 Ambilight shell feed report\n"
        "Send this file when you report a problem with the shell feed. It has no personal data in it.\n"
        "\n"
        "module:         shell_probe v24\n"
        "state:          %s\n"
        "last_stop:      %s\n"
        "not_started:    %s\n"
        "uptime_s:       %u\n"
        "live_sessions:  %u\n"
        "\n"
        "ticks:          sent %llu, skipped %llu, game_owned %llu\n"
        "compositor_flips: %u\n"
        "buffers:        %d\n"
        "self_heal:      bad_in_a_row %u, rescans %u, recovered %u, found_nothing %u\n"
        "claim:          ever_seen %s, active %s\n"
        "ini:            reloads %u, last_read %s, shell_enabled %s\n"
        "dev_mirror:     %s\n"
        "\n",
        sh_state_name(s->state), sh_stop_name(s->stopReason), s->notStartedWhy[0] ? s->notStartedWhy : "-",
        (unsigned)s->uptimeSecs, (unsigned)s->liveSessions,
        (unsigned long long)s->sent, (unsigned long long)s->skipped, (unsigned long long)s->gameOwned,
        (unsigned)s->flips, s->bufCount,
        (unsigned)s->consecFail, (unsigned)s->heals, (unsigned)s->healsRecovered, (unsigned)s->healsFailed,
        s->claimEverSeen ? "yes" : "no", s->claimActive ? "yes" : "no",
        (unsigned)s->iniReloads, s->iniReadOk ? "ok" : "FAILED", s->shellEnabled ? "yes" : "no",
        s->devMirror ? "on" : "off");
    if (w < 0) { out[0] = '\0'; return 0; }
    n = (size_t)w;
    for (int i = 0; i < SH_GATE_COUNT && n < cap; i++) {
        char gl[160]; sh_gate_line(gl, sizeof(gl), i);
        w = snprintf(out + n, cap - n, "%s\n", gl);
        if (w < 0) break;
        n += (size_t)w;
    }
    if (n < cap) { w = snprintf(out + n, cap - n, "\nend of report\n"); if (w > 0) n += (size_t)w; }
    if (n >= cap) {
        static const char tail[] = "\n(cut off)\nend of report\n";
        size_t t = sizeof(tail) - 1;
        if (cap > t) memcpy(out + cap - 1 - t, tail, t);
        out[cap - 1] = '\0';
        return cap - 1;
    }
    return n;
}

// --- [dev] UDP mirror: the sister's telemetry idiom (network.c debug_send_raw): DDP header, port 4048,
// payload capped at 192 bytes, own short-lived socket, and BOTH dev_ip and dev_logging=true required. The
// payload starts with the 4-byte magic "SHL1" so a listener can tell it from the plugin's binary packets. ---
#define SH_DEV_PORT         4048
#define SH_DEV_PAYLOAD_MAX  192
#define SH_DEV_MAGIC        "SHL1"

static int sh_dev_active(int devLogging, const char *devIp) { return devLogging && devIp != NULL && devIp[0] != '\0'; }

static size_t sh_dev_pack(uint8_t *out, size_t cap, const char *text)
{
    size_t tl = strlen(text), room = SH_DEV_PAYLOAD_MAX - 4;
    if (tl > room) tl = room;
    while (tl > 0 && (text[tl - 1] == '\n' || text[tl - 1] == '\r')) tl--;
    size_t plen = 4 + tl;
    if (cap < 10 + plen) return 0;
    out[0] = 0x41; out[1] = 0; out[2] = 0x0B; out[3] = 0x01; out[4] = out[5] = out[6] = out[7] = 0;
    out[8] = (uint8_t)((plen >> 8) & 0xFF); out[9] = (uint8_t)(plen & 0xFF);
    memcpy(out + 10, SH_DEV_MAGIC, 4);
    memcpy(out + 14, text, tl);
    return 10 + plen;
}

static int  g_shDevLog = 0;     // [dev] dev_logging, set by sh_parse_aux() (defined with the ini globals)
static char g_shDevIp[16] = ""; // [dev] dev_ip

// Fire-and-forget: one socket per call (events and 5 s rows only, never per tick), 5 ms send timeout.
static void sh_dev_send(const char *text)
{
#if SHELL_DEV_MIRROR && DO_WLED_SEND_INCLUDES
    if (!sh_dev_active(g_shDevLog, g_shDevIp)) return;
    uint8_t pkt[10 + SH_DEV_PAYLOAD_MAX];
    size_t n = sh_dev_pack(pkt, sizeof(pkt), text);
    if (n == 0) return;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return;
    struct timeval to = { 0, 5000 };
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_port = htons(SH_DEV_PORT);
    if (inet_pton(AF_INET, g_shDevIp, &a.sin_addr) == 1)
        sendto(s, pkt, n, 0, (struct sockaddr *)&a, sizeof(a));
    close(s);
#else
    (void)text;
#endif
}

// A new log line: klog AND the [dev] mirror. No trailing newline in fmt.
static void shell_event(const char *fmt, ...)
{
    char line[SH_DEV_PAYLOAD_MAX + 64];
    va_list ap; va_start(ap, fmt); vsnprintf(line, sizeof(line), fmt, ap); va_end(ap);
    printf(TAG " %s\n", line);
    sh_dev_send(line);
}
// ---- v24-portable-end ----

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
// [v20] The sister plugin's claim file: read-only, polled, never written here.
// ---------------------------------------------------------------------
// Fixed layout of "v1 fg=%d seq=%06u title=%-9s\n" (35 bytes), parsed by byte offset rather
// than sscanf, consistent with how this probe parses everything else. Returns 0 and fills
// *fg/*seq on a well-formed record; returns -1 (meaning: treat as no claim) on anything else,
// including a short read, a missing file, or a record that doesn't start "v1 fg=".
static int claim_read(int *fgOut, uint32_t *seqOut)
{
    int32_t fd = sceKernelOpen(CLAIM_PATH, 0 /* O_RDONLY */, 0);
    if (fd < 0) return -1;
    uint8_t buf[CLAIM_RECORD_LEN + 4];
    int32_t n = sceKernelRead(fd, buf, sizeof(buf));
    sceKernelClose(fd);
    if (n < CLAIM_RECORD_LEN) return -1;
    if (buf[0] != 'v' || buf[1] != '1' || buf[2] != ' ' || buf[3] != 'f' || buf[4] != 'g' || buf[5] != '=')
        return -1;
    int fg = buf[6] - '0';
    if (fg != 0 && fg != 1) return -1;
    if (buf[7] != ' ' || buf[8] != 's' || buf[9] != 'e' || buf[10] != 'q' || buf[11] != '=')
        return -1;
    uint32_t seq = 0;
    for (int i = 0; i < 6; i++) {
        uint8_t c = buf[12 + i];
        if (c < '0' || c > '9') return -1;
        seq = seq * 10 + (uint32_t)(c - '0');
    }
    if (buf[18] != ' ' || buf[19] != 't' || buf[20] != 'i' || buf[21] != 't' || buf[22] != 'l' || buf[23] != 'e' || buf[24] != '=')
        return -1;
    *fgOut = fg; *seqOut = seq;
    return 0;
}

// Polled from live_send()'s loop every CLAIM_POLL_MS, never more often. State lives across
// calls in these statics (this probe has no other caller of claim_poll).
static uint64_t g_claimNextPollTsc = 0;
static uint64_t g_claimDeadlineTsc = 0;  // claim considered fresh until this TSC value
static uint32_t g_claimLastSeq = 0xFFFFFFFFu;
static int      g_claimEverSeen = 0;     // have we parsed a valid record at least once?
static int      g_claimActive = 0;       // the CURRENT gate state, for transition logging only
static int      g_claimReleaseExplicit = 0; // [v21] last release came from fg=0 (1) or the timeout (0)
static uint64_t g_liveStartTsc = 0;      // [v21] set by live_send(); CLAIM lines print ms since this

// Returns 1 if the strip is currently claimed by the sister plugin (skip this tick), 0 if not.
// Does the actual file read only when it's time to poll; every other call is just a TSC compare.
static int claim_poll(uint64_t nowTsc, uint64_t tscHz)
{
    if (nowTsc >= g_claimNextPollTsc) {
        g_claimNextPollTsc = nowTsc + (tscHz > 0 ? (tscHz * CLAIM_POLL_MS / 1000) : 0);
        int fg; uint32_t seq;
        if (claim_read(&fg, &seq) == 0) {
            if (!g_claimEverSeen) {
                // [v21] The first record ever read cannot arm a claim: it may be a frozen fg=1
                // left by a previous boot. Remember its seq; only a CHANGE arms (v20 armed it).
                g_claimEverSeen = 1;
                g_claimLastSeq = seq;
            } else if (fg == 0) {
                g_claimDeadlineTsc = 0;   // explicit "not driving": clears immediately
                g_claimLastSeq = 0xFFFFFFFFu; // [v21] a new game process restarts seq; always treat it as new
                g_claimReleaseExplicit = 1;
            } else if (seq != g_claimLastSeq) {
                g_claimLastSeq = seq;
                g_claimDeadlineTsc = nowTsc + (tscHz > 0 ? (tscHz * CLAIM_STALE_MS / 1000) : 0);
                g_claimReleaseExplicit = 0;
            }
            // fg==1 but seq unchanged: leave the existing deadline alone, let it run out.
        }
        // A read failure changes nothing: whatever deadline is already set keeps counting
        // down on its own, which is exactly the crash-recovery behaviour the design wants.
    }
    int active = (nowTsc < g_claimDeadlineTsc);
    if (active != g_claimActive) {
        unsigned long long ms = (tscHz > 0 && nowTsc >= g_liveStartTsc) ? (unsigned long long)((nowTsc - g_liveStartTsc) * 1000ULL / tscHz) : 0ULL;
        char mir[80];
        if (active) {
            printf(TAG " CLAIM ACQUIRED: sister plugin is driving the strip, this probe backs off (seq=%u, t=%llums)\n", g_claimLastSeq, ms);
            snprintf(mir, sizeof(mir), "CLAIM ACQUIRED seq=%u t=%llums", g_claimLastSeq, ms);
        } else {
            printf(TAG " CLAIM RELEASED: resuming shell-side sends (%s, t=%llums)\n",
                   g_claimReleaseExplicit ? "explicit fg=0" : "timeout", ms);
            snprintf(mir, sizeof(mir), "CLAIM RELEASED %s t=%llums", g_claimReleaseExplicit ? "fg=0" : "timeout", ms);
        }
        sh_dev_send(mir); // [v24]
        g_claimActive = active;
    }
    return active;
}

// ---------------------------------------------------------------------
// [v24] Report-file side: live_send() publishes its counters into these globals at each 5 s row and at
// every state change; the report thread (its own thread, like the sister's report.c, so a file write
// never lands inside a 33 ms tick) only ever reads them.
// ---------------------------------------------------------------------
// ---- v24-report-io-begin ----
static volatile int      g_shState = SH_STARTING;
static volatile int      g_shStopReason = 0;
static volatile uint32_t g_shLiveSessions = 0;
static volatile uint64_t g_shSent = 0, g_shSkipped = 0, g_shGameOwned = 0;
static char              g_shNotStartedWhy[40] = "";
static HealState         g_heal;

static void sh_publish(uint64_t sent, uint64_t skipped, uint64_t gameOwned)
{
    g_shSent = sent; g_shSkipped = skipped; g_shGameOwned = gameOwned;
}

static void sh_report_snapshot(ShReport *s, uint32_t uptimeSecs)
{
    memset(s, 0, sizeof(*s));
    s->state = g_shState; s->stopReason = g_shStopReason;
    snprintf(s->notStartedWhy, sizeof(s->notStartedWhy), "%s", g_shNotStartedWhy);
    s->uptimeSecs = uptimeSecs; s->liveSessions = g_shLiveSessions;
    s->sent = g_shSent; s->skipped = g_shSkipped; s->gameOwned = g_shGameOwned;
    s->flips = g_flipCount; s->bufCount = g_nBuf;
    s->consecFail = g_heal.consecFail; s->heals = g_heal.heals;
    s->healsRecovered = g_heal.recovered; s->healsFailed = g_heal.failed;
    s->claimEverSeen = g_claimEverSeen; s->claimActive = g_claimActive;
    s->iniReloads = g_cfgReloads; s->iniReadOk = g_cfgLastReadOk; s->shellEnabled = g_shellEnabled;
    s->devMirror = sh_dev_active(g_shDevLog, g_shDevIp) && SHELL_DEV_MIRROR;
}

#if SHELL_REPORT_FILE
// Same raw-kernel-call route the probe's dumps use. Flags are FreeBSD's: O_WRONLY 1, O_CREAT 0x200, O_TRUNC 0x400.
static int sh_report_write(const char *text, size_t len)
{
    int32_t fd = sceKernelOpen(SH_REPORT_PATH, 0x0001 | 0x0200 | 0x0400, 0777);
    if (fd < 0) return -1;
    int64_t w = sceKernelWrite(fd, text, len);
    sceKernelClose(fd);
    return (w == (int64_t)len) ? 0 : -1;
}

// Does nothing while [compat] report_file is not 1 (an ini reload can switch it on later).
static void *shell_report_thread(void *args)
{
    (void)args;
    static ShReport last, cur;
    static char text[SH_REPORT_TEXT_MAX];
    uint32_t uptime = 0, since = 0, failures = 0;
    int wrote = 0;
    for (;;) {
        if (g_shReportOn && failures < SH_REPORT_MAX_FAILURES) {
            sh_report_snapshot(&cur, uptime);
            since++;
            if (sh_report_due(&last, &cur, wrote, since)) {
                size_t len = sh_report_format(text, sizeof(text), &cur);
                if (sh_report_write(text, len) != 0) failures++;
                else { failures = 0; last = cur; wrote = 1; since = 0; }
            }
        }
        usleep(1000000);
        uptime++;
    }
    return NULL;
}
#endif
// ---- v24-report-io-end ----

#if DO_LIVE_DDP
// [v24] Early read of just the three aux keys, so the report file and the [dev] mirror work even when
// live_send() returns before its own ini read (no buffers, no destination, ...) -- exactly when a report is wanted.
static void sh_early_flags_load(void)
{
    ini_table_s *table = ini_table_create();
    if (table != NULL && ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) sh_parse_aux(table);
    if (table != NULL) ini_table_destroy(table);
}

static int g_shGateSig = -1;
// One line whenever the runtime gates (all ini-driven) change; the first call after each load always prints.
static void sh_runtime_gates_note(void)
{
    int rep = (SHELL_REPORT_FILE && g_shReportOn) ? 1 : 0;
    int dev = (SHELL_DEV_MIRROR && sh_dev_active(g_shDevLog, g_shDevIp)) ? 1 : 0;
    int heal = SHELL_SELF_HEAL ? 1 : 0;
    int sig = rep | (dev << 1) | (heal << 2);
    if (sig == g_shGateSig) return;
    g_shGateSig = sig;
    shell_event("GATE (runtime, from the ini): report_file=%s dev_mirror=%s self_heal=%s",
                SHELL_REPORT_FILE ? (g_shReportOn ? "ON" : "OFF ([compat] report_file is not 1)") : "OFF (compiled out)",
                SHELL_DEV_MIRROR ? (dev ? "ON" : "OFF ([dev] needs dev_ip AND dev_logging=true)") : "OFF (compiled out)",
                heal ? "ON" : "OFF (compiled out)");
}

// [v24] One re-scan, run from live_send()'s own thread. The caller skips the current tick and re-paces.
static void sh_do_heal(void)
{
    int before = g_nBuf;
    uint32_t n = g_heal.heals;
    map_scan(0);
    find_bufs();
    heal_done(&g_heal, g_nBuf);
    shell_event("SELFHEAL #%u: re-scan found %d display buffer(s) (was %d); next re-scan not before %us",
                (unsigned)n, g_nBuf, before, (unsigned)(heal_cooldown_ms(&g_heal) / 1000u));
}
#endif

// [v18] Continuous live send: one socket, LIVE_HZ, TSC-paced. [v23] runs until reboot or clean stop.
// [v23-fix] live_send() now returns why it stopped, so the caller can tell a user-triggered disable
// (resumable) apart from a configuration/environment problem (not retried automatically):
//   0 = never got to the loop at all (no buffers, no host configured, socket/address setup failed)
//   1 = stopped because the disable file was present
//   2 = stopped because [shell] enabled=false
// ---------------------------------------------------------------------
#if DO_LIVE_DDP
static int live_send(void)
{
    if (g_nBuf == 0) {
        printf(TAG " LIVE: no display buffers found, nothing to send\n");
        snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "no display buffers");
        return 0;
    }

    // [v22] The ini first: it decides the destination, the pace, the zone count and every colour setting.
    // A missing or unreadable file is not fatal: the sister's compiled defaults stand (and WLED_HOST is the fallback host).
    // [v23-fix v44] Only the very first call this process lifetime is a real LOAD; every later call is a
    // resume out of wait_for_reenable() re-reading the same ini, which is a RELOAD in every sense that
    // matters to a reader of the log (and should count toward g_cfgReloads, same as shell_cfg_poll()'s).
    int resuming = g_liveSendEverStarted;
    g_liveSendEverStarted = 1;
    int cfgOk = shell_cfg_load();
    snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "%s", ""); // [v24] we got past the buffer check
    buildZoneGeometry();
    g_smoothedRgbValid = false;
    {
        int64_t sz = 0; uint32_t h = 0;
        if (ambient_get_real_config_size_and_hash(&sz, &h)) { g_cfgLastSize = sz; g_cfgLastHash = h; g_cfgHaveBaseline = 1; }
        if (resuming && cfgOk) g_cfgReloads++;
        const char *why = cfgOk ? (resuming ? "RELOAD" : "LOAD")
                                 : (resuming ? "RELOAD FAILED (keeping prior settings)" : "LOAD FAILED (compiled defaults in use)");
        shell_cfg_log(why, cfgOk ? sz : 0, cfgOk ? h : 0);
    }
    sh_runtime_gates_note(); // [v24]

    char curHost[64]; int curPort;
    strncpy(curHost, g_config.wledHost[0] != '\0' ? g_config.wledHost : WLED_HOST, sizeof(curHost) - 1);
    curHost[sizeof(curHost) - 1] = '\0';
    curPort = g_config.wledPort != 0 ? (int)g_config.wledPort : (int)WLED_PORT;
    if (curHost[0] == '\0') {
        printf(TAG " LIVE: no wled_host in the ini and WLED_HOST is empty, nothing to send\n");
        snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "no destination host");
        return 0;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { printf(TAG " LIVE: socket() failed (%d)\n", sock); snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "socket() failed"); return 0; }
    struct timeval to = { 0, 5000 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
    struct sockaddr_in d; memset(&d, 0, sizeof(d));
    d.sin_family = AF_INET; d.sin_port = htons((uint16_t)curPort);
    if (inet_pton(AF_INET, curHost, &d.sin_addr) != 1) {
        printf(TAG " LIVE: inet_pton failed for host \"%s\"\n", curHost);
        snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "destination is not an address");
        close(sock);
        return 0;
    }
    printf(TAG " LIVE: sending to %s:%d, %u Hz, until reboot, buffers found=%d, zones=%u\n",
           curHost, curPort, (unsigned)g_config.updateFrequencyHz, g_nBuf, (unsigned)g_numZones);
    g_shLiveSessions++; g_shState = SH_LIVE; g_shStopReason = 0; // [v24]
    { char m[100]; snprintf(m, sizeof(m), "LIVE start zones=%u hz=%u buffers=%d session=%u",
                            (unsigned)g_numZones, (unsigned)g_config.updateFrequencyHz, g_nBuf, (unsigned)g_shLiveSessions); sh_dev_send(m); }

    static uint8_t pk[10 + 3 * MAX_TOTAL_ZONES];
    static uint8_t rgbTriplets[3 * MAX_TOTAL_ZONES];
    pk[0] = 0x41; pk[1] = 0; pk[2] = 0x0B; pk[3] = 0x01; pk[4] = pk[5] = pk[6] = pk[7] = 0;

    uint32_t hz = g_config.updateFrequencyHz;
    uint64_t periodTsc = (g_tscHz > 0) ? (g_tscHz / hz) : 0;
    uint64_t nextTick = rdtsc17();
    g_liveStartTsc = nextTick;   // [v21] CLAIM lines print ms since here
    // [v23] no endTsc: the loop runs until a clean stop (disable file, shell enabled=false) or process exit.
    uint64_t nextRowTsc = nextTick + 5 * g_tscHz, rows = 0;
    uint64_t nextEnabledPollTsc = nextTick; // [v46] due immediately on the first tick
    uint64_t k = 0, sent = 0, skipped = 0, buf0 = 0, buf1 = 0, gameOwned = 0;
    uint64_t sumUs = 0, maxUs = 0, minUs = 0xFFFFFFFFu, rowTicks = 0;
    int stopReason = 0; // [v23-fix] set just before each break below; 0 here only guards against a logic error

    for (;; k++) {

        if (periodTsc > 0) {
            uint64_t now = rdtsc17();
            if (now < nextTick) {
                uint64_t waitTsc = nextTick - now;
                uint32_t waitUs = (uint32_t)(waitTsc * 1000000ULL / g_tscHz);
                if (waitUs > 0 && waitUs < 1000000) usleep(waitUs);
            }
            nextTick += periodTsc;
        } else {
            usleep(1000000 / hz); // [v18] fallback if TSC calibration failed: fixed-rate, will drift
        }

        // [v22] The REST of the ini (brightness, zones, host, ...), re-checked on the sister's own schedule
        // (config_reload_check_seconds, 2 s) -- [shell] enabled= no longer follows this cadence; see
        // shell_enabled_poll() above, [v46]. Runs even
        // while the sister plugin holds the claim, so an edit made during a game is already in effect afterwards.
        // [v23] disable file: /data/ps4_shell_feed_off -- create over FTP to stop cleanly.
        {
            int32_t dfd = sceKernelOpen(DISABLE_FILE, 0 /* O_RDONLY */, 0);
            if (dfd >= 0) { sceKernelClose(dfd); printf(TAG " DISABLED: disable file found (" DISABLE_FILE ").\n"); sh_dev_send("DISABLED: disable file"); stopReason = 1; break; }
        }
        // [v46] [shell] enabled= checked on its own 1 s cadence, independent of config_reload_check_seconds
        // (see shell_enabled_poll()'s comment) -- was previously "picked up by the live-reload poll above",
        // which meant config_reload_check_seconds=0 silently made this unreachable until the next fresh start.
        shell_enabled_poll(rdtsc17(), g_tscHz, &nextEnabledPollTsc);
        if (!g_shellEnabled) { printf(TAG " DISABLED: shell disabled in ini ([shell] enabled=false).\n"); sh_dev_send("DISABLED: [shell] enabled=false"); stopReason = 2; break; }

        if (shell_cfg_poll(rdtsc17(), g_tscHz)) {
            sh_runtime_gates_note(); // [v24]
            hz = g_config.updateFrequencyHz;
            periodTsc = (g_tscHz > 0) ? (g_tscHz / hz) : 0;
            nextTick = rdtsc17() + periodTsc;
            const char *nh = g_config.wledHost[0] != '\0' ? g_config.wledHost : WLED_HOST;
            int np = g_config.wledPort != 0 ? (int)g_config.wledPort : (int)WLED_PORT;
            if (strcmp(nh, curHost) != 0 || np != curPort) {
                struct sockaddr_in nd; memset(&nd, 0, sizeof(nd));
                nd.sin_family = AF_INET; nd.sin_port = htons((uint16_t)np);
                if (inet_pton(AF_INET, nh, &nd.sin_addr) == 1) {
                    d = nd; strncpy(curHost, nh, sizeof(curHost) - 1); curHost[sizeof(curHost) - 1] = '\0'; curPort = np;
                    printf(TAG " LIVE: destination is now %s:%d\n", curHost, curPort);
                } else {
                    printf(TAG " LIVE: new host \"%s\" is not an address; still sending to %s:%d\n", nh, curHost, curPort);
                }
            }
        }

        int rowDue = (g_tscHz > 0 && rdtsc17() >= nextRowTsc);

        // [v20] The sister plugin's claim, checked every tick (cheap: a TSC compare unless it's
        // actually time to poll the file). While it's fresh, this tick is entirely skipped --
        // no buffer read, no send -- and counted separately from both sent and skipped (a claim
        // deferral is not a failure of anything on this side).
        if (claim_poll(rdtsc17(), g_tscHz)) {
            gameOwned++;
            if (rowDue) {
                rows++; nextRowTsc += 5 * g_tscHz;
                printf(TAG " LIVE t=%llus: sent=%llu skipped=%llu gameOwned=%llu buf0=%llu buf1=%llu (claim active)\n",
                       (unsigned long long)(rows * 5), (unsigned long long)sent, (unsigned long long)skipped,
                       (unsigned long long)gameOwned, (unsigned long long)buf0, (unsigned long long)buf1);
                sh_publish(sent, skipped, gameOwned); // [v24]
                { char m[110]; snprintf(m, sizeof(m), "LIVE t=%llus sent=%llu skipped=%llu gameOwned=%llu (claim active)",
                                        (unsigned long long)(rows * 5), (unsigned long long)sent, (unsigned long long)skipped, (unsigned long long)gameOwned); sh_dev_send(m); }
                sumUs = 0; maxUs = 0; minUs = 0xFFFFFFFFu; rowTicks = 0;
            }
            continue;
        }

        int32_t idx = g_lastFlipIndex;
        int b = (g_nBuf == 2 && idx == 1) ? 1 : 0; // default buf0 for idx==-1/0/anything else
        if (b == 0) buf0++; else buf1++;

        uint64_t t0 = rdtsc17();
        int bufReady = (g_nBuf > 0) && buf_ok(&g_buf[b]); // [v24] g_nBuf can be 0 after a failed re-scan
#if SHELL_SELF_HEAL
        {
            uint64_t nowMs = g_tscHz > 1000 ? rdtsc17() / (g_tscHz / 1000u) : (k * 1000u / (hz > 0 ? hz : 30u));
            if (heal_tick(&g_heal, bufReady, nowMs)) {
                sh_do_heal();
                nextTick = rdtsc17() + periodTsc; // the re-scan took a while: re-pace rather than burst
                skipped++; continue;
            }
        }
#endif
        if (!bufReady) { skipped++; continue; }
        uint32_t sampleIntervalUs = 1000000u / hz;
        uint32_t nz = shell_process_frame(g_buf[b].start, sampleIntervalUs, rgbTriplets);
        int dataSize = (int)(3 * nz);
        pk[8] = (uint8_t)((dataSize >> 8) & 255); pk[9] = (uint8_t)(dataSize & 255);
        memcpy(pk + 10, rgbTriplets, (size_t)dataSize);
        sendto(sock, pk, 10 + dataSize, 0, (struct sockaddr *)&d, sizeof(d));
        uint64_t elapsedTsc = rdtsc17() - t0;
        uint32_t elapsedUs = (g_tscHz > 0) ? (uint32_t)(elapsedTsc * 1000000ULL / g_tscHz) : 0;
        sumUs += elapsedUs; if (elapsedUs > maxUs) maxUs = elapsedUs; if (elapsedUs < minUs) minUs = elapsedUs;
        sent++; rowTicks++;

        if (rowDue) { // every 5 s
            rows++; nextRowTsc += 5 * g_tscHz;
            uint32_t za = 0, zb = nz / 3, zc = (2 * nz) / 3;
            printf(TAG " LIVE t=%llus: sent=%llu skipped=%llu gameOwned=%llu buf0=%llu buf1=%llu avg=%lluus min=%lluus max=%lluus"
                   " first3=%02x%02x%02x/%02x%02x%02x/%02x%02x%02x zones=%u hz=%u reloads=%u\n",
                   (unsigned long long)(rows * 5), (unsigned long long)sent, (unsigned long long)skipped,
                   (unsigned long long)gameOwned, (unsigned long long)buf0, (unsigned long long)buf1,
                   rowTicks ? (unsigned long long)(sumUs / rowTicks) : 0ULL, (unsigned long long)minUs, (unsigned long long)maxUs,
                   rgbTriplets[za*3], rgbTriplets[za*3+1], rgbTriplets[za*3+2],
                   rgbTriplets[zb*3], rgbTriplets[zb*3+1], rgbTriplets[zb*3+2],
                   rgbTriplets[zc*3], rgbTriplets[zc*3+1], rgbTriplets[zc*3+2],
                   (unsigned)nz, (unsigned)hz, (unsigned)g_cfgReloads);
            sh_publish(sent, skipped, gameOwned); // [v24]
            { char m[150]; snprintf(m, sizeof(m), "LIVE t=%llus sent=%llu skipped=%llu gameOwned=%llu avg=%lluus max=%lluus zones=%u hz=%u reloads=%u heals=%u",
                                    (unsigned long long)(rows * 5), (unsigned long long)sent, (unsigned long long)skipped, (unsigned long long)gameOwned,
                                    rowTicks ? (unsigned long long)(sumUs / rowTicks) : 0ULL, (unsigned long long)maxUs,
                                    (unsigned)nz, (unsigned)hz, (unsigned)g_cfgReloads, (unsigned)g_heal.heals); sh_dev_send(m); }
            sumUs = 0; maxUs = 0; minUs = 0xFFFFFFFFu; rowTicks = 0;
        }
    }

    close(sock);
    sh_publish(sent, skipped, gameOwned); // [v24]
    g_shStopReason = stopReason;
    {
        uint64_t elapsedSec = g_tscHz > 0 ? (rdtsc17() - g_liveStartTsc) / g_tscHz : k / (hz > 0 ? hz : 30);
        printf(TAG " LIVE done. elapsed=%llus ticks=%llu sent=%llu skipped=%llu (%.1f%%) gameOwned=%llu (%.1f%%) buf0=%llu buf1=%llu"
               " sister_ever_seen=%d ini_reloads=%u\n",
               (unsigned long long)elapsedSec,
               (unsigned long long)k, (unsigned long long)sent, (unsigned long long)skipped,
               k ? 100.0 * (double)skipped / (double)k : 0.0,
               (unsigned long long)gameOwned, k ? 100.0 * (double)gameOwned / (double)k : 0.0,
               (unsigned long long)buf0, (unsigned long long)buf1, g_claimEverSeen, (unsigned)g_cfgReloads);
        { char m[100]; snprintf(m, sizeof(m), "LIVE done elapsed=%llus sent=%llu skipped=%llu stop=%d",
                                (unsigned long long)elapsedSec, (unsigned long long)sent, (unsigned long long)skipped, stopReason); sh_dev_send(m); }
    }
    return stopReason; // [v23-fix] 1 (disable file) or 2 (enabled=false); 0 would mean the loop was left without
                        // taking either break above, which should not happen -- left as a visible 0 rather than
                        // asserting, so a future bug here shows as "stopped permanently" instead of a crash.
}

// [v23-fix] Entered only after a clean, user-triggered stop (live_send() returned 1 or 2), never after a
// configuration/environment failure (0). Polls for both kill-switches to clear -- the disable file gone AND
// [shell] enabled= back to true -- then returns so probe_thread() can call live_send() again. Does not touch
// hooks, map_scan() or find_bufs(): the buffers already found stay as they are, same as a running live_send()
// never re-scans them either. The disable file is checked every poll regardless of the ini's own cadence,
// since it carries no cadence of its own.
//
// [v45] The ini itself used to be re-read on the SAME cadence as live_send()'s own reload poll
// (config_reload_check_seconds) -- which meant a value of 0 (meaning "never poll" while actively
// sending, a deliberate way to skip the file-stat overhead at 30 Hz) also meant "never poll" here,
// where nothing is being sent and there is no overhead to avoid. The result, confirmed on hardware
// (HANDOFF v44): a permanent deadlock -- `[shell] enabled=false` with `config_reload_check_seconds=0`
// could not be cleared by `enabled=true`, nor by raising `config_reload_check_seconds` itself, since
// that value can only be learned by a read this never performed. Only a reboot cleared it.
//
// Fixed by giving standby its own cadence, independent of config_reload_check_seconds: the ini is now
// re-read every STANDBY_RELOAD_MS regardless of that setting. config_reload_check_seconds still governs
// how often live_send() re-reads the ini WHILE SENDING (unchanged, 0 still means "never poll" there,
// same deliberate opt-out as before) -- this only changes the no-cost, not-sending-anything wait here.
#define STANDBY_POLL_MS   1000u
#define STANDBY_RELOAD_MS 1000u // [v45] fixed cadence for standby's own ini re-read; does not read
                                 // config_reload_check_seconds, so a value of 0 there can no longer
                                 // block recovery. Deliberately == STANDBY_POLL_MS (checked every
                                 // wake): standby already wakes once a second to poll the disable
                                 // file, and sceKernelOpen+a config_size_and_hash stat/read is cheap
                                 // next to that, so there is no reason to wait any longer than this
                                 // loop already does.
static void wait_for_reenable(int stopReason)
{
    g_shState = SH_STANDBY; // [v24]
    sh_dev_send(stopReason == 1 ? "STANDBY: disable file present" : "STANDBY: [shell] enabled=false");
    printf(TAG " STANDBY: feed stopped (%s), waiting to resume (re-checking the ini every %us"
           " regardless of config_reload_check_seconds).\n",
           stopReason == 1 ? "disable file present" : "[shell] enabled=false",
           (unsigned)(STANDBY_RELOAD_MS / 1000u));

    uint32_t elapsedMs = 0;
    for (;;) {
        usleep(STANDBY_POLL_MS * 1000);
        elapsedMs += STANDBY_POLL_MS;

        int32_t dfd = sceKernelOpen(DISABLE_FILE, 0 /* O_RDONLY */, 0);
        if (dfd >= 0) { sceKernelClose(dfd); elapsedMs = 0; continue; } // still held off by the file

        if (elapsedMs >= STANDBY_RELOAD_MS) { // [v45] was gated on config_reload_check_seconds > 0; no longer
            elapsedMs = 0;
            shell_cfg_load(); // sets g_shellEnabled directly; a read failure leaves it as it was, same as shell_cfg_poll()
        }

        if (g_shellEnabled) { printf(TAG " RESUMING: re-enabled, restarting the send loop.\n"); sh_dev_send("RESUMING"); return; }
    }
}

#endif

#if DO_DUMP_BUFFERS
static uint8_t g_dumpChunk[64 * 1024];

// v16's dump, extended: while copying, pick up the zone points that fall in each chunk so the
// zone colours are computed from the SAME copy that lands in the file.
static void dump_bufs(int scene)
{
    for (int b = 0; b < g_nBuf; b++) {
        const uint64_t start = g_buf[b].start, len = g_buf[b].len;
        if (!buf_ok(&g_buf[b])) { printf(TAG " DUMP s%d buf%d: skipped (region changed or not readable)\n", scene, b); continue; }
        char path[64];
        snprintf(path, sizeof(path), "/data/shell_probe_v17_s%d_c%d.raw", scene, b);
        int32_t fd = sceKernelOpen(path, 0x200 | 0x400 | 0x001, 0777);
        if (fd < 0) { printf(TAG " DUMP s%d: cannot create %s (%d)\n", scene, path, (int)fd); return; }
        static uint32_t zs[NZ][3];
        memset(zs, 0, sizeof(zs));
        int32_t idx0 = g_lastFlipIndex;
        printf(TAG " DUMP s%d: about to copy %llx len=%llu -> %s (flip idx %d)\n", scene,
               (unsigned long long)start, (unsigned long long)len, path, (int)idx0);
        uint64_t done = 0; int bad = 0;
        while (done < len) {
            uint64_t chunk = len - done;
            if (chunk > sizeof(g_dumpChunk)) chunk = sizeof(g_dumpChunk);
            volatile uint32_t *src = (volatile uint32_t *)(start + done);
            uint32_t *dst = (uint32_t *)g_dumpChunk;
            for (uint64_t w = 0; w < chunk / 4; w++) dst[w] = src[w];
            for (int i = 0; i < NZ * ZPTS; i++) {
                uint32_t off = g_ptOff[i];
                if (off >= done && off < done + chunk) {
                    uint32_t w = dst[(off - done) >> 2]; int z = i / ZPTS;
                    zs[z][0] += (w >> 16) & 255; zs[z][1] += (w >> 8) & 255; zs[z][2] += w & 255;
                }
            }
            int64_t wr = sceKernelWrite(fd, g_dumpChunk, chunk);
            if (wr != (int64_t)chunk) { bad = 1; break; }
            done += chunk;
        }
        sceKernelClose(fd);
        printf(TAG " DUMP s%d: %s %s, %llu of %llu bytes, flip idx %d->%d, region %llx\n", scene, path,
               bad ? "WRITE SHORT" : "ok", (unsigned long long)done, (unsigned long long)len,
               (int)idx0, (int)g_lastFlipIndex, (unsigned long long)start);
        if (bad) continue;
        static uint8_t rgb[NZ][3];
        for (int z = 0; z < NZ; z++) for (int c = 0; c < 3; c++) rgb[z][c] = (uint8_t)(zs[z][c] / ZPTS);
        for (int z0 = 0; z0 < NZ; z0 += 29) {
            char line[29 * 7 + 8]; int n = 0;
            for (int z = z0; z < z0 + 29 && z < NZ; z++) n += snprintf(line + n, sizeof(line) - (size_t)n, "%02x%02x%02x ", rgb[z][0], rgb[z][1], rgb[z][2]);
            printf(TAG " ZONES s%d c%d [%d..%d]: %s\n", scene, b, z0, (z0 + 28 < NZ ? z0 + 28 : NZ - 1), line);
        }
        ddp_preview(scene, b, rgb);
    }
}
#endif

static void scene_run(int scene, int do_dump)
{
    static uint32_t lastFlips = 0;
    uint32_t f = g_flipCount;
    printf(TAG " SCENE %d begin: compositor flips=%u (+%u since the previous scene), last idx=%d, register=%u\n",
           scene, (unsigned)f, (unsigned)(f - lastFlips), (int)g_lastFlipIndex, (unsigned)g_regCount);
    lastFlips = f;
    map_scan(scene == 1);
    find_bufs();
    if (g_nBuf == 0) { printf(TAG " SCENE %d: no display buffers matched -- nothing sampled (compare the candidates above with scene 1)\n", scene); return; }
    run_burst(scene);
#if DO_DUMP_BUFFERS
    if (do_dump) dump_bufs(scene);
#else
    (void)do_dump;
#endif
    printf(TAG " SCENE %d done: compositor flips=%u\n", scene, (unsigned)g_flipCount);
}

static void *probe_thread(void *args)
{
    // [v23] printedEvents, prevFlip, nextCue removed: the post-send reporter loop they served is gone.
    printf(TAG " probe_thread started (v24: runs until reboot or clean stop).\n");

    // [v17] sampler setup (unchanged); [v18] scene_run is not called this version.
    build_points();
    printf(TAG " SAMPLER: %d zones x %d points (+%d scatter points) built; points outside the buffer: %d\n", NZ, ZPTS, SC_PTS, g_badPts);
    calibrate_tsc();

#if DO_LIVE_DDP
    sh_early_flags_load();   // [v24] report_file / [dev] known before the first live_send(), even if it returns at once
    sh_runtime_gates_note(); // [v24]
    // [v19] FIX 2: g_nBuf is only ever set by find_bufs(), which v17 reached through scene_run(1,..).
    // v18 stopped calling scene_run, so g_nBuf stayed 0 and live_send() would have returned
    // "no display buffers found". Scan and match the buffers here, then send.
    map_scan(0);
    find_bufs();
    // [v23] live_send() runs until a clean stop fires (disable file, [shell] enabled=false) or a
    // configuration/environment problem stops it before it ever starts sending. The post-send reporter
    // and watch loops that followed here in v22 are removed: they were dead code from v18 onward
    // (live_send already ran for LIVE_SECONDS, leaving no time) and are doubly dead now.
    // [v23-fix] A clean stop used to end probe_thread() for good ("Reboot to clear"), which meant
    // [shell] enabled=true back in the ini, or removing the disable file, did not actually resume
    // anything -- found on hardware in capture v23-A. Loop here instead: wait in wait_for_reenable()
    // for both kill-switches to clear, then call live_send() again. Buffers found earlier are not
    // re-scanned; a stop with reason 0 (buffers/host/socket) is not retried automatically.
    for (;;) {
        int stopReason = live_send();
        if (stopReason == 0) {
            printf(TAG " probe_thread: send loop stopped and will not retry (no buffers, no destination, or"
                   " socket setup failed -- see the LIVE line above); hooks stay installed. Reboot to retry.\n");
            g_shState = SH_STOPPED; // [v24] the report file says why (not_started:)
            shell_event("STOPPED for good: %s", g_shNotStartedWhy[0] ? g_shNotStartedWhy : "see the LIVE line");
            break;
        }
        wait_for_reenable(stopReason);
    }
#else
    printf(TAG " LIVE: compiled out (DO_LIVE_DDP=0): this build has no send loop and will never drive the strip.\n");
    g_shState = SH_STOPPED;
#endif

    return NULL;
}

// ---------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------
s32 attr_module_hidden module_start(s64 argc, const void *args)
{
    printf(TAG " ---- begin v24 compositor hooks + sampler (pid %d) ----\n", (int)getpid());
    {   // [v24] one line per compile-time gate: a gate that is off is stated, never silent (the v18 lesson)
        int off = 0, crit = 0;
        for (int i = 0; i < SH_GATE_COUNT; i++) {
            char gl[160]; sh_gate_line(gl, sizeof(gl), i);
            printf(TAG " %s\n", gl);
            if (!kGates[i].on) { off++; if (kGates[i].critical) crit++; }
        }
        printf(TAG " GATES: %d total, %d off, %d critical off%s\n", SH_GATE_COUNT, off, crit,
               crit ? " -- THIS BUILD CANNOT DRIVE THE STRIP" : "");
    }

    // 1. Who are we? Hooks are installed only in the compositor.
    struct proc_info procInfo = {0};
    int rc = sys_sdk_proc_info(&procInfo);
    printf(TAG " sys_sdk_proc_info rc=%d pid=%d name=%s titleid=%s\n",
           rc, (int)procInfo.pid, procInfo.name, procInfo.titleid);
    int isCompositor = name_has_ci(procInfo.name, "gnmcompositor") && name_has_ci(procInfo.titleid, "npxs20976");
    printf(TAG " expected target: name containing \"GnmCompositor\", titleid NPXS20976%s\n",
           isCompositor ? " -- MATCHES" : " -- DOES NOT MATCH, we are somewhere else");
    int mayHook = isCompositor || ALLOW_ANY_PROCESS;
    if (!mayHook)
        printf(TAG " not the compositor: this run is passive (census + resolve only); NO hooks will be installed.\n");

    // 2. ONE scan; nothing is loaded.
    list_modules_api();
    struct ScanResult s = scan_and_discover("census");
    printf(TAG " census: %d modules answered%s; gnmdriver modules: %d\n",
           s.found, s.hitCap ? ", HIT HARD CAP" : "", s.gnmCount);

    // 3. Resolve. VideoOut for the hooks; Gnm candidates for the record only.
    void *pReg  = resolve(s.hVideoOut, "libSceVideoOut.sprx", "sceVideoOutRegisterBuffers");
    void *pFlip = resolve(s.hVideoOut, "libSceVideoOut.sprx", "sceVideoOutSubmitFlip");
    // [v11] resolve-only, always -- no risk, confirms the symbols exist and
    // where, before anything decides whether to call them.
    g_pFlipStatusFn = resolve(s.hVideoOut, "libSceVideoOut.sprx", "sceVideoOutGetFlipStatus");
    g_pResStatusFn  = resolve(s.hVideoOut, "libSceVideoOut.sprx", "sceVideoOutGetResolutionStatus");
    int gnmAnyFlip = 0;
    for (int i = 0; i < s.gnmCount && i < MAX_GNM_MODS; i++) {
        int gh = s.gnmHandles[i];
        void *g1 = resolve(gh, "gnmdriver(substring match)", "sceGnmSubmitAndFlipCommandBuffers");
        void *g2 = resolve(gh, "gnmdriver(substring match)", "sceGnmSubmitAndFlipCommandBuffersForWorkload");
        void *g3 = resolve(gh, "gnmdriver(substring match)", "sceGnmSubmitCommandBuffers");
        void *g4 = resolve(gh, "gnmdriver(substring match)", "sceGnmSubmitDone");
        if (g1 != NULL || g2 != NULL) gnmAnyFlip = 1;
        (void)g3; (void)g4;
    }
    printf(TAG " VERDICT-INPUTS: videoout_resident=%d gnm_modules=%d reg=%d flip=%d gnm_public_flip_names_resolve=%d\n",
           s.hVideoOut >= 0, s.gnmCount, pReg != NULL, pFlip != NULL, gnmAnyFlip);

    if (!mayHook) {
        printf(TAG " module_start done (passive).\n");
        return 0;
    }

    // 4. Hooks, each with a before/after byte check. Skip forwarding stubs (0xE9).
    sceVideoOutRegisterBuffersPtr = (int32_t (*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *))pReg;
    sceVideoOutSubmitFlipPtr = (int32_t (*)(int32_t, int32_t, int32_t, int64_t))pFlip;

    int patchedReg = 0, patchedFlip = 0;
    uint8_t b[8];

    if (pReg != NULL && first_byte(pReg) != 0xE9 &&
        hook_reachable("RegisterBuffers", pReg, (void *)sceVideoOutRegisterBuffersPtr_hook)) {
        read8(pReg, b);
        printf(TAG " about to patch sceVideoOutRegisterBuffers at %p (a hang or black screen right after this line = the patch itself faulted)\n", pReg);
        HOOK32(sceVideoOutRegisterBuffersPtr);
        patchedReg = report_patch("RegisterBuffers", pReg, (void *)sceVideoOutRegisterBuffersPtr_hook, b);
    }
    if (pFlip != NULL && first_byte(pFlip) != 0xE9 &&
        hook_reachable("SubmitFlip", pFlip, (void *)sceVideoOutSubmitFlipPtr_hook)) {
        read8(pFlip, b);
        printf(TAG " about to patch sceVideoOutSubmitFlip at %p (a hang or black screen right after this line = the patch itself faulted)\n", pFlip);
        HOOK32(sceVideoOutSubmitFlipPtr);
        patchedFlip = report_patch("SubmitFlip", pFlip, (void *)sceVideoOutSubmitFlipPtr_hook, b);
    }
    printf(TAG " patch summary: register=%d flip=%d\n", patchedReg, patchedFlip);
    if (!patchedReg && !patchedFlip) {
        printf(TAG " no hook was installed: nothing to count, no reporter started. Zero counts would mean nothing here.\n");
        return 0;
    }

#if DO_CONTROL_CALLS
    // 5. Positive control: call the PATCHED entry points (raw resolved addresses,
    // not the trampoline pointers) with an invalid handle (-1). Assumed, not
    // verified in the compositor: rejected at argument validation, as in the shell.
    if (patchedFlip && pFlip != NULL) {
        uint32_t c0 = g_flipCount;
        printf(TAG " control: about to call SubmitFlip(handle=-1) via %p\n", pFlip);
        int32_t r = ((int32_t (*)(int32_t, int32_t, int32_t, int64_t))pFlip)(-1, 0, 0, 0);
        printf(TAG " control: SubmitFlip returned 0x%x, flip count %u -> %u => hook %s\n",
               (unsigned)r, (unsigned)c0, (unsigned)g_flipCount,
               (g_flipCount > c0) ? "FIRES" : "DID NOT FIRE");
    }
    if (patchedReg && pReg != NULL) {
        uint32_t c0 = g_regCount;
        printf(TAG " control: about to call RegisterBuffers(handle=-1) via %p\n", pReg);
        int32_t r = ((int32_t (*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *))pReg)(-1, 0, NULL, 0, NULL);
        printf(TAG " control: RegisterBuffers returned 0x%x, register count %u -> %u => hook %s\n",
               (unsigned)r, (unsigned)c0, (unsigned)g_regCount,
               (g_regCount > c0) ? "FIRES" : "DID NOT FIRE");
    }
    // Remove the control calls from the counts and the ring.
    g_flipCount = 0; g_regCount = 0; g_ringHead = 0;
    g_lastFlipHandle = -1; g_lastFlipIndex = -1; g_lastFlipMode = -1;
    printf(TAG " control done, counters reset to 0\n");
#endif

    OrbisPthread thread;
    scePthreadCreate(&thread, NULL, probe_thread, NULL, "shell_probe_thr");
#if SHELL_REPORT_FILE
    {   // [v24] the report thread: idle until [compat] report_file=1
        OrbisPthread rthread;
        scePthreadCreate(&rthread, NULL, shell_report_thread, NULL, "shell_report_thr");
    }
#else
    printf(TAG " report thread: compiled out (SHELL_REPORT_FILE=0)\n");
#endif

    printf(TAG " module_start done, reporter thread running\n");
    return 0;
}

s32 attr_module_hidden module_stop(s64 argc, const void *args) { return 0; }
