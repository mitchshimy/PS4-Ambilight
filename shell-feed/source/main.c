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
// HOW THE SISTER'S CODE GOT IN. It is pasted, not rewritten: every region between a "sister-begin" and a
// "sister-end" marker was cut out of the sister's own sources by function name (PS4-Ambilight 89199fe)
// by tools/make_probe_v22.py, and tools/host_ini_check/run.sh cuts the same regions out of THIS file and
// compares them to the sister's files byte for byte, then runs both against the same ini and the same
// synthetic frames and compares the output. The one-file shape is deliberate: nothing verifies that extra
// .c files in source/ would be compiled or left alone by the test repo's build.bat.
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
// [v22] PS4-Ambilight's settings, zone geometry and colour chain, PASTED (sister 89199fe).
// Everything between "sister-begin" and "sister-end" below is a verbatim cut from the sister's own source,
// made by tools/make_probe_v22.py and checked by tools/host_ini_check/run.sh. The shims around those
// regions are this project's, and are small on purpose.
// =====================================================================

// What the sister's files get from headers and files this probe does not have:
#undef  debug_printf                  // plugin_common.h (pulled in by the build) already defines it as a klog call
#define debug_printf(...) ((void)0)       // silence it here: the ini reader calls it on malformed lines
#define _atoi atoi                        // config.c's own line 8
#define NUM_GAMMA_LUTS  8                 // gamma.c
#define MAX_TOTAL_ZONES 512               // ambient_internal.h
#define SCAN_DEPTH_MAX  4
#define SATURATION_MAX  100
#define CONTRAST_MAX    100
#define AMBIENT_CONFIG_PATH "/data/ps4_ambient_light.ini"
#define SCREEN_WIDTH    1920              // zones.c
#define SCREEN_HEIGHT   1080

// The ini reader mallocs a lot of tiny strings and the sister's code never checks for NULL. Inside the
// compositor that is a risk this probe does not need to take, so those calls go to a fixed 128 KB arena
// that is emptied before every ini read (the real ini is a few KB). free() is a no-op, realloc() copies.
static uint8_t g_arena[128 * 1024] __attribute__((aligned(16)));
static size_t  g_arenaUsed = 0;
static void arena_reset(void) { g_arenaUsed = 0; }
static void *arena_malloc(size_t n)
{
    size_t need = (n + 15) & ~(size_t)15;
    if (g_arenaUsed + 16 + need > sizeof(g_arena)) return NULL;
    uint8_t *p = g_arena + g_arenaUsed;
    *(size_t *)p = n;
    g_arenaUsed += 16 + need;
    return p + 16;
}
static void *arena_realloc(void *old, size_t n)
{
    if (old == NULL) return arena_malloc(n);
    size_t on = *(size_t *)((uint8_t *)old - 16);
    if (n <= on) return old;
    void *np = arena_malloc(n);
    if (np != NULL) memcpy(np, old, on);
    return np;
}
static void arena_free(void *p) { (void)p; }

// ---- sister-begin: config.h ini structs ----
typedef struct ini_entry_s {
    char *key;
    char *value;
} ini_entry_s;

typedef struct ini_section_s {
    char *name;
    ini_entry_s *entry;
    int size;
} ini_section_s;

typedef struct ini_table_s {
    ini_section_s *section;
    int size;
} ini_table_s;
// ---- sister-end ----
// ---- sister-begin: ambient_internal.h enums ----
typedef enum { CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT, CORNER_TOP_LEFT, CORNER_TOP_RIGHT } StartCorner;
typedef enum { DIR_CLOCKWISE, DIR_COUNTERCLOCKWISE } LedDirection;
typedef enum { ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR } ColorOrder;
// ---- sister-end ----
// ---- sister-begin: ambient_internal.h AmbientConfig ----
typedef struct {
    // [network]
    char wledHost[64];
    uint16_t wledPort;
    // v2.3: separate destination for the external-source on/off signal
    // (see relay_send_external_source below) -- the relay that owns
    // this (wled-relay's tv_external_source.py) runs on its own host,
    // NOT the WLED controller itself, so this can't just reuse
    // wledHost/wledPort.
    char relayHost[64];
    uint16_t relayPort;
    // v2.3.1: this whole external-source signal is a personal-setup
    // integration with one specific wled-relay project, not something
    // every user of this plugin has (or should have to configure a
    // dead host for) -- OFF by default, only sends anything if a user
    // explicitly opts in via the ini.
    bool relaySignalEnabled;
    // [layout]
    uint32_t ledCountTop, ledCountRight, ledCountBottom, ledCountLeft;
    StartCorner startCorner;
    LedDirection direction;
    int32_t ledOffset;          // rotates which physical LED index 0 lands on
    uint32_t scanDepth;         // sample radius: (2*scanDepth+1)^2 pixels averaged per zone
    // v3.0: auto letterbox/black-bar detection is now the ONLY inset
    // applied before sampling -- the old manual capture_margin_{top,
    // right,bottom,left} fields were removed (see CHANGELOG). Almost
    // everything plays fullscreen with no letterboxing at all, in
    // which case this correctly detects and applies zero margin; the
    // cases it exists for -- cutscenes, aspect-ratio-locked menus, a
    // status bar along one edge -- are exactly the cases a fixed
    // manual number couldn't handle anyway (it's either wrong during
    // the bar, or wrong for the other 99% of gameplay that has none).
    // On by default for that reason: disabling it now means sampling
    // starts at the true pixel 0/edge on every side, unconditionally.
    bool autoLetterboxEnabled;
    uint32_t autoLetterboxThreshold;         // 0-255: a probed pixel counts as "black" if every channel is below this
    uint32_t autoLetterboxStabilityFrames;   // consecutive matching detections required before a new border is committed -- kills flicker across a scene cut
    uint32_t autoLetterboxCheckIntervalFrames; // how many sample-thread iterations to wait between re-detections (each check is a bounded per-edge probe, up to MAX_BAR_DEPTH_V/H deep in letterbox.c, and every sample is a tiled read -- not free)
    // [color]
    uint32_t brightness;        // 0-255 global scale, applied after gamma
    uint32_t gammaLutIndex;     // index into kGammaLuts -- see NUM_GAMMA_LUTS below
    int32_t saturation;         // -100 (grayscale) .. 0 (unchanged) .. SATURATION_MAX (2x boost)
    ColorOrder colorOrder;
    uint32_t blackLevel;        // v2.1: 0-100, percent of 255 below which output clips to 0
    uint32_t whiteLevel;        // v2.1: 0-100, percent of 255 at/above which output clips to 255 (100 = no change)
    uint32_t darkThreshold;     // v2.1: 0-255, max(R,G,B) below this forces a zone fully black (0 = disabled)
    // v2.2: ported from the Android "inspiration" project's own
    // ColorProcessor.kt. These are ADDITIONS on top
    // of the existing brightness/gamma fields above, not replacements
    // -- brightness/gamma keep their original 0-255 / fixed-LUT-string
    // meaning so nobody's existing ini silently changes behavior.
    // Percentages below use Android's own convention (100 = neutral),
    // NOT this file's usual "0 = neutral" convention -- documented
    // per-field in the generated ini template.
    int32_t contrast;           // -100..CONTRAST_MAX, 0 = unchanged (100=neutral Android pct minus 100, same convention as saturation above)
    uint32_t brightnessR, brightnessG, brightnessB; // 0-500, 100 = unchanged. Multiplies with the global brightness above.
    uint32_t gammaR, gammaG, gammaB;                // 10-500, 100 = unchanged. Independent per-channel curves, applied PER SAMPLE
                                                     // before zone-averaging (see sampleZoneAverage) -- unlike every other
                                                     // knob here, which is applied once to the already-averaged zone color.
                                                     // This matters because gamma is non-linear: correcting-then-averaging
                                                     // and averaging-then-correcting are NOT the same operation, and the
                                                     // Android source applies its gamma per-pixel, before any downsampling.
    // v2.2.5: opt-in developer telemetry destination. Deliberately NOT
    // part of ambient_create_default_config()'s generated template
    // (see AMBIENT_DEFAULT_INI below) -- a person has to type a [dev]
    // section into their own ini by hand for this to ever do anything.
    // Empty devIp / false devLoggingEnabled (the zero-init default,
    // same as every other field in this struct before a real ini is
    // read) means debug_send_raw() sends nothing at all, full stop --
    // see its own comment for why this check lives there and not
    // scattered across each call site.
    char devIp[16];          // dotted-quad only, e.g. "192.168.2.117"; empty = disabled regardless of devLoggingEnabled
    bool devLoggingEnabled;  // both this AND a non-empty devIp are required -- neither alone is enough
    // [timing]
    uint32_t updateFrequencyHz;
    int smoothingEnabled;
    uint32_t settlingTimeMs;
    uint32_t configReloadCheckSeconds; // v2.1: 0 = load once at start only (v2.0 behavior), like before
} AmbientConfig;
// ---- sister-end ----

#define malloc(n)      arena_malloc(n)
#define realloc(p, n)  arena_realloc((p), (n))
#define free(p)        arena_free(p)

// Detile math: the sister's getTiledElementByteOffset(&kParamsBase, x, y) IS v17's t17_offset(&kParamsBase17, x, y)
// (same BASE parameters, verbatim formulas), so the sister's sampler runs on the probe's own tiling.
typedef TileParams17 TileParams;
#define kParamsBase kParamsBase17
static uint64_t getTiledElementByteOffset(const TileParams *p, uint32_t x, uint32_t y) { return t17_offset(p, x, y); }
typedef void (*PixelUnpackFn)(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b);
// sampleZoneAverage()'s bounds check: no read past one display buffer (find_bufs() verifies each buffer is this long).
static volatile uint64_t g_readableLimit = BUF_BYTES17;
// The shell has no letterbox detection, so the zone geometry's inset is always 0 (zones.c's effMargin*()).
static uint32_t g_autoLetterboxTop = 0, g_autoLetterboxRight = 0, g_autoLetterboxBottom = 0, g_autoLetterboxLeft = 0;

// ---- sister-begin: config.c ini reader ----
static ini_entry_s *_ini_entry_create(ini_section_s *section, const char *key, const char *value) {
    if ((section->size % 10) == 0) {
        section->entry = (ini_entry_s *)realloc(section->entry, (10 + section->size) * sizeof(ini_entry_s));
    }
    ini_entry_s *entry = &section->entry[section->size++];
    entry->key = (char *)malloc((strlen(key) + 1) * sizeof(char));
    entry->value = (char *)malloc((strlen(value) + 1) * sizeof(char));
    debug_printf("key: %s = value: %s\n", key, value);
    strcpy(entry->key, key);
    strcpy(entry->value, value);
    return entry;
}

static ini_section_s *_ini_section_create(ini_table_s *table, const char *section_name) {
    if ((table->size % 10) == 0) {
        table->section = (ini_section_s *)realloc(table->section, (10 + table->size) * sizeof(ini_section_s));
    }
    ini_section_s *section = &table->section[table->size++];
    section->size = 0;
    section->name = (char *)malloc((strlen(section_name) + 1) * sizeof(char));
    strcpy(section->name, section_name);
    section->entry = (ini_entry_s *)malloc(10 * sizeof(ini_entry_s));
    return section;
}

// Ctn: make this non-static
ini_section_s *_ini_section_find(ini_table_s *table, const char *name) {
    for (int i = 0; i < table->size; i++) {
        if (strcmp(table->section[i].name, name) == 0) {
            return &table->section[i];
        }
    }
    return NULL;
}

static ini_entry_s *_ini_entry_find(ini_section_s *section, const char *key) {
    for (int i = 0; i < section->size; i++) {
        if (strcmp(section->entry[i].key, key) == 0) {
            return &section->entry[i];
        }
    }
    return NULL;
}

static ini_entry_s *_ini_entry_get(ini_table_s *table, const char *section_name, const char *key) {
    ini_section_s *section = _ini_section_find(table, section_name);
    if (section == NULL) {
        return NULL;
    }

    ini_entry_s *entry = _ini_entry_find(section, key);
    if (entry == NULL) {
        return NULL;
    }
    return entry;
}

ini_table_s *ini_table_create() {
    ini_table_s *table = (ini_table_s *)malloc(sizeof(ini_table_s));
    table->size = 0;
    table->section = (ini_section_s *)malloc(10 * sizeof(ini_section_s));
    return table;
}

void ini_table_destroy(ini_table_s *table) {
    for (int i = 0; i < table->size; i++) {
        ini_section_s *section = &table->section[i];
        for (int q = 0; q < section->size; q++) {
            ini_entry_s *entry = &section->entry[q];
            free(entry->key);
            free(entry->value);
        }
        free(section->entry);
        free(section->name);
    }
    free(table->section);
    free(table);
}

int eof_hack(int c) {
    static bool first_time = true;

    if (first_time && c == EOF) {
        first_time = false;
        return INT_MAX;
    }

    return EOF;
}

// v2.2: reads via sceKernelOpen/sceKernelRead/sceKernelClose into a
// memory buffer instead of fopen()/fgetc(). This function is reachable
// from ambient_check_config_reload() on ambient_sample_thread (a worker
// thread spawned separately from plugin load), and libc stdio (FILE*,
// buffering, locks) isn't safe to touch off the main plugin-load thread
// on this toolchain -- that's what was crashing. Same raw-syscall
// approach game_patch/utils.cpp already uses successfully elsewhere in
// this repo. The parsing state machine below is unchanged; only the
// byte source changed, from fgetc(f) to indexing into filebuf.
bool ini_table_read_from_file(ini_table_s* table, const char* file)
{
    int32_t fd = sceKernelOpen(file, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return false;

    int64_t filesize = sceKernelLseek(fd, 0, SEEK_END);
    if (filesize < 0) {
        sceKernelClose(fd);
        return false;
    }
    sceKernelLseek(fd, 0, SEEK_SET);

    // malloc(0) is legal-but-implementation-defined (may return NULL even
    // on success), so a genuinely empty file gets a 1-byte buffer that
    // nothing ever reads into rather than being misread as a malloc failure.
    char *filebuf = (char *)malloc((size_t)(filesize > 0 ? filesize : 1));
    if (filebuf == NULL) {
        sceKernelClose(fd);
        return false;
    }

    ssize_t nread = filesize > 0 ? sceKernelRead(fd, filebuf, (size_t)filesize) : 0;
    sceKernelClose(fd);
    if (nread < 0) {
        free(filebuf);
        return false;
    }

    size_t filepos = 0;
    size_t filelen = (size_t)nread;
    #define fgetc(unused) (filepos < filelen ? (int)(unsigned char)filebuf[filepos++] : EOF)

    enum {Section, Key, Value, Comment} state = Section;
    int   c;
    int   position = 0;
    int   spaces   = 0;
    int   line     = 0;
    int   buffer_size = 128 * sizeof(char);
    char* buf   = (char*)malloc(buffer_size);
    char* value = NULL;

    ini_section_s* current_section = NULL;
    memset(buf, '\0', buffer_size);

    bool first_eol = false;
    while(1) {
        c = fgetc(f);
        if (c == eof_hack(c))
            break;

        if (c == '\r')
            continue;
        if (position > buffer_size-2) {
            buffer_size += 128 * sizeof(char);
            size_t value_offset = value == NULL ? 0 : value - buf;
            buf = (char*)realloc(buf, buffer_size);
            memset(buf+position, '\0', buffer_size-position);

            if (value != NULL)
                value = buf + value_offset;
        }
        switch(c) {
            case ' ':
                switch(state) {
                    case Value: if (value[0] != '\0') spaces++; break;
                    default: if (buf[0] != '\0') spaces++; break;
                }
                break;
            case ';':
                while (c != eof_hack(c) && c != '\n')
                {
                    c = fgetc(f);
                }
            // fallthrough
            case '\n':
            // fallthrough
            case EOF:
                if (first_eol) {
                    continue;
                    first_eol = true;
                }
                line++;
                if (state == Value) {
                    if (current_section == NULL) {
                        current_section = _ini_section_create(table, "");
                    }
                    _ini_entry_create(current_section, buf, value);
                    value = NULL;
                } else if (strlen(buf) > 1 && position && state == Key) {
                    if (current_section == NULL) {
                        current_section = _ini_section_create(table, "");
                    }
                    _ini_entry_create(current_section, buf, "");
                } else if (state == Comment) {
                    if (current_section == NULL) {
                        current_section = _ini_section_create(table, "");
                    }
                    _ini_entry_create(current_section, buf, "");
                } else if (state == Section) {
                    debug_printf("Section `%s' missing `]' operator.", buf);
                } else if(state == Key && position) {
                    debug_printf("Key `%s' missing `=' operator.", buf);
                }
                memset(buf, '\0', buffer_size);
                state = Key;
                position = 0;
                spaces = 0;
                break;
            case '[':
                state = Section;
                break;
            case ']':
                current_section = _ini_section_create(table, buf);
                memset(buf, '\0', buffer_size);
                position = 0;
                spaces = 0;
                state = Key;
                break;
            case '=':
                if (state == Key) {
                    state = Value;
                    buf[position++] = '\0';
                    value = buf + position;
                    spaces = 0;
                    continue;
                }
            default:
                for(;spaces > 0; spaces--) buf[position++] = ' ';
                buf[position++] = c;
                break;
        }
    }
    #undef fgetc
    free(buf);
    free(filebuf);
    return true;
}
// ---- sister-end ----
// ---- sister-begin: config.c ini getters ----
void ini_table_create_entry(ini_table_s *table, const char *section_name, const char *key, const char *value) {
    ini_section_s *section = _ini_section_find(table, section_name);
    if (section == NULL) {
        section = _ini_section_create(table, section_name);
    }
    ini_entry_s *entry = _ini_entry_find(section, key);
    if (entry == NULL) {
        entry = _ini_entry_create(section, key, value);
    } else {
        free(entry->value);
        entry->value = (char *)malloc((strlen(value) + 1) * sizeof(char));
        strcpy(entry->value, value);
    }
}

bool ini_table_check_entry(ini_table_s *table, const char *section_name, const char *key) {
    return (_ini_entry_get(table, section_name, key) != NULL);
}

const char *ini_table_get_entry(ini_table_s *table, const char *section_name, const char *key) {
    ini_entry_s *entry = _ini_entry_get(table, section_name, key);
    if (entry == NULL) {
        return NULL;
    }
    return entry->value;
}

bool ini_table_get_entry_as_int(ini_table_s *table, const char *section_name, const char *key, int *value) {
    const char *val = ini_table_get_entry(table, section_name, key);
    if (val == NULL) {
        return false;
    }
    *value = _atoi(val);
    return true;
}

bool ini_table_get_entry_as_bool(ini_table_s *table, const char *section_name, const char *key, bool *value) {
    const char *val = ini_table_get_entry(table, section_name, key);
    if (val == NULL) {
        return false;
    }
    if (strcasecmp(val, "on") == 0 || strcasecmp(val, "true") == 0 || strcasecmp(val, "1") == 0) {
        *value = true;
    } else {
        *value = false;
    }
    return true;
}
// ---- sister-end ----
// ---- sister-begin: settings.c g_config defaults ----
AmbientConfig g_config = {
    // wledHost intentionally blank, not pre-filled with this project's
    // own controller's IP -- a build anyone else uses shouldn't ship
    // with someone else's network address. Blank just leaves DDP send
    // targeting nothing until a real ini (or the companion app) sets
    // wled_host; see AMBIENT_DEFAULT_INI below for the generated file.
    .wledHost = "",
    .wledPort = 4048,
    // relayHost blank for the same reason as wledHost above -- also
    // moot either way, since relaySignalEnabled defaults to false
    // below, so this address is never dialed unless a user explicitly
    // opts in AND sets their own relay_host in the ini.
    .relayHost = "",
    .relayPort = 24689, // must match wled-relay's tv_external_source.EXTERNAL_SOURCE_PORT
    .relaySignalEnabled = false, // opt-in only -- see the field comment above
    .ledCountTop = 73, .ledCountRight = 41, .ledCountBottom = 73, .ledCountLeft = 42,
    .startCorner = CORNER_BOTTOM_LEFT,
    .direction = DIR_CLOCKWISE,
    .ledOffset = 0,
    .scanDepth = 1,
    // v3.0: on by default -- this is now the only inset applied before
    // sampling (see ambient_internal.h). threshold/stability default
    // values are unchanged from when this was introduced in v2.9 as an
    // addition on top of manual margins.
    .autoLetterboxEnabled = true,
    .autoLetterboxThreshold = 18,
    .autoLetterboxStabilityFrames = 3,
    .autoLetterboxCheckIntervalFrames = 15, // ~2x/sec at the 30Hz default update_frequency_hz
    .brightness = 255,
    .gammaLutIndex = 0, // gamma 1.0 == passthrough, matches v1.3 (no color processing existed)
    .saturation = 0,
    .colorOrder = ORDER_RGB,
    .blackLevel = 0,     // no-op -- matches "no black_level existed before v2.1" exactly
    .whiteLevel = 100,   // no-op
    .darkThreshold = 0,  // v2.1's original "0 = disabled" default -- reverted back
                         // from 10 (which had been kept in sync with the companion
                         // app's own settings_set_defaults()); both now default to 0
    .contrast = 0,       // no-op -- new in v2.2, matches "didn't exist before" like the rest of this block
    .brightnessR = 100, .brightnessG = 100, .brightnessB = 100, // no-op (100 = unchanged, Android convention)
    .gammaR = 100, .gammaG = 100, .gammaB = 100,                 // no-op (100 = unchanged, Android convention)
    .devIp = "", .devLoggingEnabled = false, // v2.2.5: disabled unless a real ini opts in -- see struct comment
    .updateFrequencyHz = 30,
    .smoothingEnabled = 0, // off by default -- v1.3 had no smoothing, don't change behavior silently
    .settlingTimeMs = 200,
    .configReloadCheckSeconds = 2, // matches the auto-generated template's default (2s) -- this is a
                                    // brand-new capability with no prior behavior to preserve, so unlike every
                                    // other default above it does NOT need to match "what v2.0 did" (v2.0 simply
                                    // couldn't do this at all). Only matters if ambient_create_default_config()
                                    // ever stops matching this value -- keep the two in sync by hand.
};
// ---- sister-end ----
// ---- sister-begin: settings.c parse helpers ----
static ColorOrder parse_color_order(const char *s, ColorOrder fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "RGB")) return ORDER_RGB;
    if (!strcmp(s, "RBG")) return ORDER_RBG;
    if (!strcmp(s, "GRB")) return ORDER_GRB;
    if (!strcmp(s, "GBR")) return ORDER_GBR;
    if (!strcmp(s, "BRG")) return ORDER_BRG;
    if (!strcmp(s, "BGR")) return ORDER_BGR;
    return fallback; // unrecognized -- keep default rather than guess
}

static StartCorner parse_start_corner(const char *s, StartCorner fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "bottom_left"))  return CORNER_BOTTOM_LEFT;
    if (!strcmp(s, "bottom_right")) return CORNER_BOTTOM_RIGHT;
    if (!strcmp(s, "top_left"))     return CORNER_TOP_LEFT;
    if (!strcmp(s, "top_right"))    return CORNER_TOP_RIGHT;
    return fallback;
}

static LedDirection parse_direction(const char *s, LedDirection fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "clockwise")) return DIR_CLOCKWISE;
    if (!strcmp(s, "counterclockwise")) return DIR_COUNTERCLOCKWISE;
    return fallback;
}

// Matched by exact string, not parsed as a float -- deliberately, to
// avoid any float-parsing code path at runtime in a build with no
// libm linked. Only these 8 values exist as precomputed LUTs anyway
// (see kGammaLuts below), so a free-form value couldn't be honored
// correctly regardless.
static uint32_t parse_gamma_index(const char *s, uint32_t fallback)
{
    static const char *kGammaStrings[NUM_GAMMA_LUTS] = {"1.0","1.4","1.8","2.0","2.2","2.4","2.6","2.8"};
    if (!s) return fallback;
    for (uint32_t i = 0; i < NUM_GAMMA_LUTS; i++) {
        if (!strcmp(s, kGammaStrings[i])) return i;
    }
    return fallback; // unrecognized string -- keep default rather than guess
}
// ---- sister-end ----
// ---- sister-begin: settings.c preset_section_has_keys ----
static bool preset_section_has_keys(ini_table_s *table, const char *section)
{
    static const char *const kPresetKeys[] = {
        "scan_depth", "auto_letterbox_enabled", "auto_letterbox_threshold",
        "brightness", "saturation", "gamma", "black_level", "white_level", "contrast",
        "smoothing_enabled", "settling_time_ms", "dark_threshold",
        "brightness_r", "brightness_g", "brightness_b", "gamma_r", "gamma_g", "gamma_b",
    };
    for (unsigned i = 0; i < sizeof(kPresetKeys) / sizeof(kPresetKeys[0]); i++)
        if (ini_table_check_entry(table, section, kPresetKeys[i])) return true;
    return false;
}
// ---- sister-end ----
// ---- sister-begin: preset_select.h ----
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
// ---- sister-end ----
// ---- sister-begin: settings.c config hash ----
#define AMBIENT_FNV1A_OFFSET_BASIS 0x811c9dc5u
#define AMBIENT_FNV1A_PRIME        0x01000193u
static uint32_t ambient_fnv1a32(const uint8_t *data, size_t len)
{
    uint32_t hash = AMBIENT_FNV1A_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= AMBIENT_FNV1A_PRIME;
    }
    return hash;
}

// Sanity cap on the hash read, independent of the earlier no-libm
// constraint -- this is about not handing malloc() a garbage size if
// AMBIENT_CONFIG_PATH ever resolves to something unexpected. The real
// file is a few KB at most (even a heavily-commented ini); 1 MiB is
// generous headroom, not a real expected size.
#define AMBIENT_CONFIG_MAX_HASH_READ (1024 * 1024)

// Replaces v2.1.4's ambient_get_real_config_size: still gets size the
// same proven way (sceKernelOpen + sceKernelLseek(SEEK_END)), but now
// also seeks back to the start and reads the whole file through the
// same fd to compute a content hash in the same call, rather than
// opening the file twice per check. Both outputs share one success/
// failure result since they come from the same read; on any failure
// *outSize carries the negative orbis error code (or a sentinel below)
// same as v2.1.4's function did, and *outHash is left untouched.
static bool ambient_get_real_config_size_and_hash(int64_t *outSize, uint32_t *outHash)
{
    int32_t fd = sceKernelOpen(AMBIENT_CONFIG_PATH, 0 /* O_RDONLY */, 0777);
    if (fd < 0) {
        *outSize = fd; // negative orbis error code, reused as the packet's "errno" field below
        return false;
    }
    int64_t sz = sceKernelLseek(fd, 0, SEEK_END);
    if (sz < 0) {
        sceKernelClose(fd);
        *outSize = sz;
        return false;
    }
    if (sz > AMBIENT_CONFIG_MAX_HASH_READ) {
        sceKernelClose(fd);
        *outSize = -1; // sentinel, not a real orbis error code -- "file unexpectedly huge", not an open/lseek failure
        return false;
    }

    int64_t seekBack = sceKernelLseek(fd, 0, SEEK_SET);
    if (seekBack < 0) {
        sceKernelClose(fd);
        *outSize = seekBack;
        return false;
    }

    // sz fits in AMBIENT_CONFIG_MAX_HASH_READ (checked above), so this
    // is a small, bounded allocation, not proportional to whatever
    // AMBIENT_CONFIG_PATH happens to resolve to.
    uint8_t *buf = (sz > 0) ? (uint8_t *)malloc((size_t)sz) : NULL;
    if (sz > 0 && buf == NULL) {
        sceKernelClose(fd);
        *outSize = -1; // sentinel -- malloc failure, not an orbis error code
        return false;
    }

    ssize_t nread = (sz > 0) ? sceKernelRead(fd, buf, (size_t)sz) : 0;
    sceKernelClose(fd);
    if (nread < 0) {
        free(buf);
        *outSize = nread;
        return false;
    }

    *outSize = sz;
    *outHash = ambient_fnv1a32(buf, (size_t)nread);
    free(buf);
    return true;
}
// ---- sister-end ----
// ---- sister-begin: gamma.c fixed LUTs ----
const uint8_t kGammaLuts[NUM_GAMMA_LUTS][256] = {
    { // gamma 1.0
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
        96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
    },
    { // gamma 1.4
        0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5,
        5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 12, 12, 13, 13,
        14, 15, 15, 16, 16, 17, 18, 18, 19, 20, 20, 21, 22, 22, 23, 24,
        25, 25, 26, 27, 28, 28, 29, 30, 31, 31, 32, 33, 34, 34, 35, 36,
        37, 38, 38, 39, 40, 41, 42, 43, 43, 44, 45, 46, 47, 48, 49, 49,
        50, 51, 52, 53, 54, 55, 56, 57, 57, 58, 59, 60, 61, 62, 63, 64,
        65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96,
        97, 98, 99, 100, 101, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
        115, 116, 117, 118, 119, 120, 121, 122, 124, 125, 126, 127, 128, 129, 130, 132,
        133, 134, 135, 136, 137, 139, 140, 141, 142, 143, 145, 146, 147, 148, 149, 151,
        152, 153, 154, 155, 157, 158, 159, 160, 161, 163, 164, 165, 166, 168, 169, 170,
        171, 173, 174, 175, 176, 178, 179, 180, 181, 183, 184, 185, 187, 188, 189, 190,
        192, 193, 194, 196, 197, 198, 200, 201, 202, 203, 205, 206, 207, 209, 210, 211,
        213, 214, 215, 217, 218, 219, 221, 222, 223, 225, 226, 227, 229, 230, 232, 233,
        234, 236, 237, 238, 240, 241, 242, 244, 245, 247, 248, 249, 251, 252, 254, 255,
    },
    { // gamma 1.8
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2,
        2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6,
        6, 6, 7, 7, 8, 8, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12,
        13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 21,
        21, 22, 22, 23, 24, 24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31,
        32, 32, 33, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 41, 42, 43,
        44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 53, 54, 55, 56, 57,
        58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
        74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 86, 87, 88, 89, 90,
        91, 92, 93, 95, 96, 97, 98, 99, 100, 102, 103, 104, 105, 107, 108, 109,
        110, 111, 113, 114, 115, 116, 118, 119, 120, 122, 123, 124, 126, 127, 128, 129,
        131, 132, 134, 135, 136, 138, 139, 140, 142, 143, 145, 146, 147, 149, 150, 152,
        153, 154, 156, 157, 159, 160, 162, 163, 165, 166, 168, 169, 171, 172, 174, 175,
        177, 178, 180, 181, 183, 184, 186, 188, 189, 191, 192, 194, 195, 197, 199, 200,
        202, 204, 205, 207, 208, 210, 212, 213, 215, 217, 218, 220, 222, 224, 225, 227,
        229, 230, 232, 234, 236, 237, 239, 241, 243, 244, 246, 248, 250, 251, 253, 255,
    },
    { // gamma 2.0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
        4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9,
        9, 9, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16,
        16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 23, 23, 24, 24,
        25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35, 35,
        36, 37, 38, 38, 39, 40, 41, 42, 42, 43, 44, 45, 46, 47, 47, 48,
        49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 84, 85, 86, 87, 88, 89, 91, 92, 93, 94, 95, 97, 98, 99,
        100, 102, 103, 104, 105, 107, 108, 109, 111, 112, 113, 115, 116, 117, 119, 120,
        121, 123, 124, 126, 127, 128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 143,
        145, 146, 148, 149, 151, 152, 154, 155, 157, 158, 160, 162, 163, 165, 166, 168,
        170, 171, 173, 175, 176, 178, 180, 181, 183, 185, 186, 188, 190, 192, 193, 195,
        197, 199, 200, 202, 204, 206, 207, 209, 211, 213, 215, 217, 218, 220, 222, 224,
        226, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255,
    },
    { // gamma 2.2
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6,
        6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12,
        12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
        20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29,
        30, 30, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 39, 39, 40, 41,
        42, 43, 43, 44, 45, 46, 47, 48, 49, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
        73, 74, 75, 76, 77, 78, 79, 81, 82, 83, 84, 85, 87, 88, 89, 90,
        91, 93, 94, 95, 97, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 111,
        113, 114, 116, 117, 119, 120, 121, 123, 124, 126, 127, 129, 130, 132, 133, 135,
        137, 138, 140, 141, 143, 145, 146, 148, 149, 151, 153, 154, 156, 158, 159, 161,
        163, 165, 166, 168, 170, 172, 173, 175, 177, 179, 181, 182, 184, 186, 188, 190,
        192, 194, 196, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
        223, 225, 227, 229, 231, 234, 236, 238, 240, 242, 244, 246, 248, 251, 253, 255,
    },
    { // gamma 2.4
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
        2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9,
        9, 10, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 14, 15, 15,
        16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24,
        24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35,
        35, 36, 37, 38, 39, 39, 40, 41, 42, 43, 43, 44, 45, 46, 47, 48,
        49, 50, 51, 52, 53, 53, 54, 55, 56, 57, 58, 59, 60, 62, 63, 64,
        65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 80, 81, 82,
        83, 85, 86, 87, 88, 90, 91, 92, 94, 95, 96, 98, 99, 100, 102, 103,
        105, 106, 108, 109, 111, 112, 114, 115, 117, 118, 120, 121, 123, 124, 126, 127,
        129, 131, 132, 134, 136, 137, 139, 141, 142, 144, 146, 148, 149, 151, 153, 155,
        156, 158, 160, 162, 164, 166, 167, 169, 171, 173, 175, 177, 179, 181, 183, 185,
        187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 210, 212, 214, 216, 218,
        220, 223, 225, 227, 229, 232, 234, 236, 239, 241, 243, 246, 248, 250, 253, 255,
    },
    { // gamma 2.6
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3,
        3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7,
        7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12,
        13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20,
        20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29,
        30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 38, 38, 39, 40, 41, 42,
        42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
        58, 59, 60, 61, 62, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73, 75,
        76, 77, 78, 80, 81, 82, 84, 85, 86, 88, 89, 90, 92, 93, 94, 96,
        97, 99, 100, 102, 103, 105, 106, 108, 109, 111, 112, 114, 115, 117, 119, 120,
        122, 124, 125, 127, 129, 130, 132, 134, 136, 137, 139, 141, 143, 145, 146, 148,
        150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180,
        182, 184, 186, 188, 191, 193, 195, 197, 199, 202, 204, 206, 209, 211, 213, 215,
        218, 220, 223, 225, 227, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252, 255,
    },
    { // gamma 2.8
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
        2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
        5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
        10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
        17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
        25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
        37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
        51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
        69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
        90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
        115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
        144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
        177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
        215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255,
    },
};
// ---- sister-end ----
// ---- sister-begin: gamma.c per-channel LUTs ----
static float ambient_fast_log2(float x)
{
    union { float f; uint32_t i; } vx = { x };
    union { uint32_t i; float f; } mx = { (vx.i & 0x007FFFFFu) | 0x3f000000u };
    float y = (float)vx.i;
    y *= 1.0f / (1 << 23);
    return y - 124.22551499f - 1.498030302f * mx.f - 1.72587999f / (0.3520887068f + mx.f);
}

static float ambient_fast_exp2(float p)
{
    float offset = (p < 0.0f) ? 1.0f : 0.0f;
    float clipp = (p < -126.0f) ? -126.0f : p;
    int32_t w = (int32_t)clipp;
    float z = clipp - (float)w + offset;
    union { uint32_t i; float f; } v = {
        (uint32_t)((1 << 23) * (clipp + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z))
    };
    return v.f;
}

// x assumed in [0,1] here (normalized 0-255 sample); 0 is handled as
// a special case since log2(0) is undefined and every real gamma
// curve maps black to black anyway.
static float ambient_fast_pow01(float x, float p)
{
    if (x <= 0.0f) return 0.0f;
    return ambient_fast_exp2(p * ambient_fast_log2(x));
}

// Per-channel gamma LUTs for the v2.2 gamma_r/gamma_g/gamma_b knobs.
// Rebuilt on config load/reload (ambient_rebuild_perchannel_gamma_luts
// below), NOT per pixel -- same pattern as everything else in this
// file that could otherwise involve float math on a hot path.
//
// Stored as FLOAT, not uint8_t, deliberately: Android's own gamma LUT
// (buildGammaLut in ColorProcessor.kt) is a FloatArray too, and the
// whole rest of its pipeline stays in Float with no intermediate
// clamp/round until the single final clamp+round back to a byte. An
// 8-bit LUT here would force an early, lossy round-to-byte right
// after gamma, before brightness/contrast/saturation/levels even run
// -- verified in Python before porting that this isn't just
// theoretical: chaining early uint8_t clamps between stages produced
// answers up to ~13/255 off from the real float pipeline on ordinary
// inputs, not merely off-by-one rounding noise.
float g_gammaLutR[256], g_gammaLutG[256], g_gammaLutB[256];

static void ambient_build_one_gamma_lut(uint32_t pct, float *outLut)
{
    if (pct == 100) { // passthrough -- skip the float math entirely
        for (uint32_t i = 0; i < 256; i++) outLut[i] = (float)i;
        return;
    }
    float invGamma = 100.0f / (float)pct;
    for (uint32_t i = 0; i < 256; i++) {
        float norm = (float)i / 255.0f;
        outLut[i] = ambient_fast_pow01(norm, invGamma) * 255.0f; // left unclamped/unrounded on purpose
    }
}

void ambient_rebuild_perchannel_gamma_luts(void)
{
    ambient_build_one_gamma_lut(g_config.gammaR, g_gammaLutR);
    ambient_build_one_gamma_lut(g_config.gammaG, g_gammaLutG);
    ambient_build_one_gamma_lut(g_config.gammaB, g_gammaLutB);
}
// ---- sister-end ----
// ---- sister-begin: color_processing.c ----
static int32_t applyBrightness(int32_t c, uint32_t brightness)
{
    return (c * (int32_t)brightness) / 255;
}

// v2.2: per-channel brightness multiplier, ported from Android's
// (brightness/100) * (brightnessChannel/100) combined factor. Kept as
// its own integer scale (0-500, 100=neutral) separate from the 0-255
// global `applyBrightness` above rather than trying to unify the two
// scales -- see the AmbientConfig struct comment for why.
static int32_t applyChannelBrightnessPct(int32_t c, uint32_t pct)
{
    if (pct == 100) return c; // no-op fast path
    return (c * (int32_t)pct) / 100;
}

// v2.2: contrast, ported from Android's `128 + (c-128)*cf` with
// cf = 1 + contrast/100. Uses this file's usual "0 = unchanged"
// convention (same as saturation), NOT Android's raw "100 = unchanged"
// percentage -- see the AmbientConfig struct comment.
static void applyContrast(int32_t r, int32_t g, int32_t b, int32_t contrast,
                           int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (contrast == 0) { *outR = r; *outG = g; *outB = b; return; }
    *outR = 128 + ((r - 128) * (100 + contrast)) / 100;
    *outG = 128 + ((g - 128) * (100 + contrast)) / 100;
    *outB = 128 + ((b - 128) * (100 + contrast)) / 100;
}

// v2.2: input/output are unclamped int32_t now (see the big comment
// above applyBrightness) -- a channel that arrived negative (from
// contrast) or >255 (from a >100% per-channel brightness) still needs
// to pull the luma calculation in that same, un-clamped direction, or
// this stage disagrees with what Android's real float math does.
static void applySaturation(int32_t r, int32_t g, int32_t b, int32_t sat,
                             int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (sat == 0) { *outR = r; *outG = g; *outB = b; return; }
    // Integer luma using the real BT.601 weights (Android uses
    // 0.299/0.587/0.114 as floats; /1000 here instead of the old
    // >>8-and-round-to-256ths approximation, now that this whole path
    // carries enough precision to make the more exact weights worth
    // using.
    int32_t luma = (r * 299 + g * 587 + b * 114) / 1000;
    *outR = luma + ((r - luma) * (100 + sat)) / 100;
    *outG = luma + ((g - luma) * (100 + sat)) / 100;
    *outB = luma + ((b - luma) * (100 + sat)) / 100;
}

// Writes the 3 output bytes in the configured wire order (most
// WS2812B/NeoPixel strips are GRB, not RGB
// Android inspiration project's own color_order setting).
static void writeColorOrdered(uint8_t r, uint8_t g, uint8_t b, ColorOrder order, uint8_t *out3)
{
    switch (order) {
        case ORDER_RGB: out3[0]=r; out3[1]=g; out3[2]=b; break;
        case ORDER_RBG: out3[0]=r; out3[1]=b; out3[2]=g; break;
        case ORDER_GRB: out3[0]=g; out3[1]=r; out3[2]=b; break;
        case ORDER_GBR: out3[0]=g; out3[1]=b; out3[2]=r; break;
        case ORDER_BRG: out3[0]=b; out3[1]=r; out3[2]=g; break;
        default: /* ORDER_BGR */ out3[0]=b; out3[1]=g; out3[2]=r; break;
    }
}

// v2.1/v2.2: levels adjustment (black_level/white_level) -- a
// "stretch" of the 0-255 range, verified in Python before porting.
// Applied last in the color pipeline, matching the
// real Android ColorProcessor order (gamma -> brightness -> contrast
// -> saturation -> levels).
//
// This is now ALSO the single final clamp-to-byte for the entire
// v2.2 pipeline (see the big comment above applyBrightness) -- input
// here may be negative or >255 coming out of contrast/saturation, so
// even the "no-op" (blackLevel=0, whiteLevel=100) fast path below
// must still clamp, not just pass through, or an out-of-range
// unclamped value would wrap/truncate incorrectly when cast to
// uint8_t by the caller.
static void applyLevels(int32_t r, int32_t g, int32_t b, uint32_t blackLevel, uint32_t whiteLevel,
                         uint8_t *outR, uint8_t *outG, uint8_t *outB)
{
    #define CLAMP255(c) (uint8_t)((c) < 0 ? 0 : ((c) > 255 ? 255 : (c)))
    if (blackLevel == 0 && whiteLevel == 100) {
        *outR = CLAMP255(r); *outG = CLAMP255(g); *outB = CLAMP255(b);
        return;
    }
    int32_t blackThresh = (int32_t)(blackLevel * 255 / 100);
    int32_t whiteThresh = (int32_t)(whiteLevel * 255 / 100);
    int32_t range = whiteThresh - blackThresh;
    if (range <= 0) { *outR = 0; *outG = 0; *outB = 0; return; } // degenerate config -- fail to black, not undefined
    #define STRETCH(c) CLAMP255(((c) < blackThresh) ? 0 : ((c) >= whiteThresh) ? 255 : \
                                  (((c) - blackThresh) * 255) / range)
    *outR = STRETCH(r);
    *outG = STRETCH(g);
    *outB = STRETCH(b);
    #undef STRETCH
    #undef CLAMP255
}

// Full per-zone pipeline, v2.2 order -- now matches the Android source
// exactly: gamma -> brightness -> contrast -> saturation -> levels
// (black/white point) -> wire-order bytes. (v2.0/v2.1 had saturation
// BEFORE brightness, contradicting this file's own comment about
// matching Android. Harmless while brightness was
// only ever a single uniform scalar, since a uniform scale commutes
// with saturation's luma-relative math either way, but it stops being
// harmless the moment per-channel brightness enters the picture below,
// so it's fixed here rather than left as a latent trap.)
//
// Note the input (r,g,b) here has ALREADY had the new per-channel
// gamma_r/gamma_g/gamma_b applied, per-sample, before zone-averaging
// (see sampleZoneAverage) -- that part deliberately happens BEFORE
// this function, not in it, because gamma is non-linear and Android
// applies it before any downsampling too. The legacy fixed-LUT
// `gamma=` global below is a SEPARATE, ADDITIONAL curve applied here,
// zone-wide, after averaging -- kept for backward compatibility with
// existing configs. Both default to a no-op, so an unmodified ini
// still behaves exactly like before.
//
// dark_threshold is deliberately NOT applied here -- it needs
// per-zone hysteresis STATE across calls, which lives with the
// smoothing state in the thread loop instead.
void applyColorProcessing(uint8_t r8, uint8_t g8, uint8_t b8, uint8_t *out3)
{
    // --- gamma (legacy global fixed-LUT stage; per-channel gamma_r/g/b
    // already happened per-sample before this call). This is the one
    // remaining byte-rounding step before the pipeline goes fully
    // unclamped int32_t below -- see the struct comment on why the
    // legacy uint8_t LUT itself wasn't also upgraded to float. ---
    uint32_t gi = g_config.gammaLutIndex < NUM_GAMMA_LUTS ? g_config.gammaLutIndex : 0;
    int32_t r = kGammaLuts[gi][r8];
    int32_t g = kGammaLuts[gi][g8];
    int32_t b = kGammaLuts[gi][b8];

    // --- brightness: global (0-255 scale) then per-channel (Android's
    // 0-500pct scale) on top of it. Unclamped from here on. ---
    r = applyBrightness(r, g_config.brightness);
    g = applyBrightness(g, g_config.brightness);
    b = applyBrightness(b, g_config.brightness);
    r = applyChannelBrightnessPct(r, g_config.brightnessR);
    g = applyChannelBrightnessPct(g, g_config.brightnessG);
    b = applyChannelBrightnessPct(b, g_config.brightnessB);

    // --- contrast ---
    int32_t cr, cg, cb;
    applyContrast(r, g, b, g_config.contrast, &cr, &cg, &cb);

    // --- saturation ---
    int32_t sr, sg, sb;
    applySaturation(cr, cg, cb, g_config.saturation, &sr, &sg, &sb);

    // --- levels (black/white point) -- the ONE clamp+round back to a
    // byte for this whole unclamped chain, matching Android's own
    // single final clamp+round. ---
    uint8_t lr, lg, lb;
    applyLevels(sr, sg, sb, g_config.blackLevel, g_config.whiteLevel, &lr, &lg, &lb);

    writeColorOrdered(lr, lg, lb, g_config.colorOrder, out3);
}

// v2.1 dark_threshold state: per-zone hysteresis flag (see
// applyDarkThreshold below) -- separate from g_smoothedRgb since it's
// a decision (am I "dark" right now), not a color value.
static bool g_zoneIsDark[MAX_TOTAL_ZONES];

// Forces (r,g,b) to black if the zone should currently be considered
// "dark", with hysteresis to avoid flicker on scenes hovering right at
// the threshold: must drop BELOW darkThreshold to go dark, but must
// rise darkThreshold+10 to come back -- verified in Python against a
// deliberately noisy sequence straddling the threshold before porting.
// Applied to the pre-smoothing target, so smoothing (if
// enabled) naturally fades in/out of black instead of snapping.
#define DARK_HYSTERESIS 10
void applyDarkThreshold(uint32_t zoneIdx, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (g_config.darkThreshold == 0) return; // disabled
    uint8_t luma = *r > *g ? (*r > *b ? *r : *b) : (*g > *b ? *g : *b);
    if (g_zoneIsDark[zoneIdx]) {
        if (luma >= g_config.darkThreshold + DARK_HYSTERESIS) g_zoneIsDark[zoneIdx] = false;
    } else if (luma < g_config.darkThreshold) {
        g_zoneIsDark[zoneIdx] = true;
    }
    if (g_zoneIsDark[zoneIdx]) { *r = 0; *g = 0; *b = 0; }
}
// ---- sister-end ----
// ---- sister-begin: zones.c geometry ----
uint32_t g_zoneX[MAX_TOTAL_ZONES];
uint32_t g_zoneY[MAX_TOTAL_ZONES];
uint32_t g_numZones = 0; // set by buildZoneGeometry(), 0 until then

typedef enum { EDGE_LEFT, EDGE_TOP, EDGE_RIGHT, EDGE_BOTTOM } Edge;

// v3.0: effective margin per edge = whatever letterbox.c's auto-
// detection currently has committed for that edge (0 when the feature
// is off or hasn't found a bar there -- see that file). This used to
// be a manual g_config.margin* PLUS the auto value; the manual fields
// were removed once auto letterbox could fully cover what they were
// for (see CHANGELOG) -- these are kept as their own named functions,
// rather than reading g_autoLetterboxTop etc. directly below, purely
// for the safety clamp: no edge's margin can reach past the screen's
// own midpoint. That's the same limit BorderProcessor.applyKnownBorderCrop
// itself applies ("never crop more than half of either axis") -- here
// it also protects the unsigned subtraction below (SCREEN_HEIGHT - 1 -
// marginTop - marginBottom, etc.) from underflowing, which in
// principle a bad detection on two opposite edges could still cause
// without it. Detection itself is now bounded far tighter than this
// (MAX_BAR_DEPTH_V/H in letterbox.c, screen/6), so in practice this
// clamp is only a backstop against a bad value, not a limit that
// detection ever gets close to.
static inline uint32_t effMarginTop(void)
{
    return g_autoLetterboxTop > (SCREEN_HEIGHT / 2 - 1) ? (SCREEN_HEIGHT / 2 - 1) : g_autoLetterboxTop;
}
static inline uint32_t effMarginBottom(void)
{
    return g_autoLetterboxBottom > (SCREEN_HEIGHT / 2 - 1) ? (SCREEN_HEIGHT / 2 - 1) : g_autoLetterboxBottom;
}
static inline uint32_t effMarginLeft(void)
{
    return g_autoLetterboxLeft > (SCREEN_WIDTH / 2 - 1) ? (SCREEN_WIDTH / 2 - 1) : g_autoLetterboxLeft;
}
static inline uint32_t effMarginRight(void)
{
    return g_autoLetterboxRight > (SCREEN_WIDTH / 2 - 1) ? (SCREEN_WIDTH / 2 - 1) : g_autoLetterboxRight;
}

// Generates `count` points along one screen edge, in either its
// canonical clockwise-from-bottom-left direction (reversed=false) or
// the opposite direction (reversed=true), honoring capture margins.
// Canonical directions: LEFT bottom->top, TOP left->right,
// RIGHT top->bottom, BOTTOM right->left -- matches v1.0-v1.3's
// original (now-default) bottom-left/clockwise behavior exactly.
//
// v2.7.4: denominator changed from (count-1) to count -- the old
// inclusive-both-endpoints spacing put a point at t=0 (this edge's
// start corner) AND a point at t=count-1 landing exactly on the FAR
// corner, which the next edge's own t=0 point also lands on (its own
// start corner is the same physical point). Every one of the 4 screen
// corners therefore got two zones reading the identical pixel --
// confirmed on real hardware: zone 0 and zone (numZones-1) sent
// bit-identical raw pixel values on every single sampled frame,
// exactly matching this math for a bottom-left-start clockwise loop
// (zone 0 and the last zone both land on the bottom-left corner).
// Dividing by `count` instead spaces `count` points evenly across
// [0, edge_length) -- t=0 still lands exactly on the start corner,
// but the last point (t=count-1) now falls one step short of the far
// corner, leaving that corner to be owned solely by the next edge's
// own t=0 point. No more double-sampled corners, and no more (count >
// 1 ? ... : 0) special case needed -- count is always >= 1 here (the
// loop above never runs this function body for count == 0), so
// dividing by count alone is always safe.
static void generateEdgePoints(Edge edge, bool reversed, uint32_t count, uint32_t *outIdx)
{
    for (uint32_t i = 0; i < count && *outIdx < MAX_TOTAL_ZONES; i++) {
        uint32_t t = reversed ? (count - 1 - i) : i;
        uint32_t x, y;
        uint32_t mTop = effMarginTop(), mRight = effMarginRight(), mBottom = effMarginBottom(), mLeft = effMarginLeft();
        switch (edge) {
        case EDGE_LEFT:
            x = mLeft;
            y = (SCREEN_HEIGHT - 1 - mBottom) -
                (t * (SCREEN_HEIGHT - 1 - mTop - mBottom)) / count;
            break;
        case EDGE_TOP:
            x = mLeft +
                (t * (SCREEN_WIDTH - 1 - mLeft - mRight)) / count;
            y = mTop;
            break;
        case EDGE_RIGHT:
            x = SCREEN_WIDTH - 1 - mRight;
            y = mTop +
                (t * (SCREEN_HEIGHT - 1 - mTop - mBottom)) / count;
            break;
        default: // EDGE_BOTTOM
            x = (SCREEN_WIDTH - 1 - mRight) -
                (t * (SCREEN_WIDTH - 1 - mLeft - mRight)) / count;
            y = SCREEN_HEIGHT - 1 - mBottom;
            break;
        }
        g_zoneX[*outIdx] = x;
        g_zoneY[*outIdx] = y;
        (*outIdx)++;
    }
}

// Fills g_zoneX/g_zoneY (and g_numZones) at load time using only
// integer math (no libm linked in this build, see Makefile LIBS).
void buildZoneGeometry(void)
{
    static const Edge kCwFromBL[4] = { EDGE_LEFT, EDGE_TOP, EDGE_RIGHT, EDGE_BOTTOM };
    uint32_t counts[4] = {
        [EDGE_LEFT] = g_config.ledCountLeft, [EDGE_TOP] = g_config.ledCountTop,
        [EDGE_RIGHT] = g_config.ledCountRight, [EDGE_BOTTOM] = g_config.ledCountBottom,
    };

    // Rotate the canonical CW-from-bottom-left order to start at the
    // edge that begins at the configured start corner.
    int startEdgeIdx;
    switch (g_config.startCorner) {
        case CORNER_BOTTOM_LEFT:  startEdgeIdx = 0; break; // LEFT
        case CORNER_TOP_LEFT:     startEdgeIdx = 1; break; // TOP
        case CORNER_TOP_RIGHT:    startEdgeIdx = 2; break; // RIGHT
        default: /* BOTTOM_RIGHT */ startEdgeIdx = 3; break; // BOTTOM
    }

    Edge order[4];
    bool edgeReversed[4];
    if (g_config.direction == DIR_CLOCKWISE) {
        for (int i = 0; i < 4; i++) { order[i] = kCwFromBL[(startEdgeIdx + i) % 4]; edgeReversed[i] = false; }
    } else {
        // CCW = reverse of the CW-for-this-corner visit order, with
        // every edge's own direction also reversed. Verified in
        // Python against all 4 corners before porting.
        for (int i = 0; i < 4; i++) { order[i] = kCwFromBL[(startEdgeIdx + (3 - i)) % 4]; edgeReversed[i] = true; }
    }

    uint32_t idx = 0;
    for (int i = 0; i < 4; i++) {
        generateEdgePoints(order[i], edgeReversed[i], counts[order[i]], &idx);
    }
    g_numZones = idx;

    // led_offset: rotate which physical LED index 0 lands on, without
    // changing the shape/order of the sequence itself. Done as a
    // second pass (rotate the filled arrays) rather than folded into
    // the generation loop above, so this stays a single, independently
    // understandable step.
    int32_t offset = g_config.ledOffset % (int32_t)g_numZones;
    if (offset < 0) offset += (int32_t)g_numZones; // proper modulo for negative offsets
    if (offset != 0) {
        uint32_t tmpX[MAX_TOTAL_ZONES], tmpY[MAX_TOTAL_ZONES];
        memcpy(tmpX, g_zoneX, g_numZones * sizeof(uint32_t));
        memcpy(tmpY, g_zoneY, g_numZones * sizeof(uint32_t));
        for (uint32_t i = 0; i < g_numZones; i++) {
            uint32_t src = (i + (uint32_t)offset) % g_numZones;
            g_zoneX[i] = tmpX[src];
            g_zoneY[i] = tmpY[src];
        }
    }
}
// ---- sister-end ----
// ---- sister-begin: zones.c sampleZoneAverage ----
void sampleZoneAverage(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                               uint32_t cx, uint32_t cy, uint8_t *outR, uint8_t *outG, uint8_t *outB)
{
    // v2.0: sample radius is g_config.scanDepth (was the fixed
    // ZONE_SAMPLE_RADIUS #define). int32_t cast since scanDepth is
    // unsigned but used in a signed loop range below.
    int32_t radius = (int32_t)g_config.scanDepth;
    // v2.2: float accumulators, not uint32_t -- the per-sample gamma
    // LUTs (g_gammaLutR/G/B) now return unclamped/unrounded float, so
    // rounding each sample to a byte before summing would reintroduce
    // exactly the precision loss this change is meant to avoid. This
    // only touches (2*radius+1)^2 samples per zone -- basic float
    // add/multiply, no libm -- so it's the same cost class as the
    // uint32_t version it replaces.
    float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
    uint32_t n = 0;
    for (int32_t dy = -radius; dy <= radius; dy++) {
        for (int32_t dx = -radius; dx <= radius; dx++) {
            int32_t sx = (int32_t)cx + dx;
            int32_t sy = (int32_t)cy + dy;
            if (sx < 0 || sy < 0 || sx >= SCREEN_WIDTH || sy >= SCREEN_HEIGHT) continue;
            uint64_t off = getTiledElementByteOffset(p, (uint32_t)sx, (uint32_t)sy);
            // v1.1: bounds-check before touching real memory. A correct
            // offset formula should never produce something out of
            // range for a valid (sx,sy), but this project has already
            // paid for bad-memory-access crashes once --
            // cheap insurance against a future edge case or a param
            // typo, not a sign anything is currently wrong.
            if (off + 4 > g_readableLimit) continue;
            uint32_t px;
            memcpy(&px, (const void*)(bufferAddr + off), 4);
            uint8_t r, g, b;
            unpack(px, &r, &g, &b);
            // v2.2: per-channel gamma (gamma_r/g/b) applied to THIS
            // sample now, before it's folded into the zone sum --
            // see the big comment above applyColorProcessing for why
            // this one specific step moved here instead of staying in
            // the post-average pipeline like everything else.
            sumR += g_gammaLutR[r];
            sumG += g_gammaLutG[g];
            sumB += g_gammaLutB[b];
            n++;
        }
    }
    if (n == 0) n = 1;
    float avgR = sumR / (float)n, avgG = sumG / (float)n, avgB = sumB / (float)n;
    // This IS the one deliberate round-to-byte boundary in the whole
    // v2.2 pipeline before the final output: the legacy post-average
    // stages (applyColorProcessing) take uint8_t in/out to stay a
    // drop-in match for the pre-v2.2 function signature and call
    // site. Everything from here through applyColorProcessing's
    // internals is unclamped int32_t, with only ONE more round+clamp
    // at the very end -- see that function.
    #define CLAMPF(v) ((v) < 0.0f ? 0.0f : ((v) > 255.0f ? 255.0f : (v)))
    *outR = (uint8_t)(CLAMPF(avgR) + 0.5f);
    *outG = (uint8_t)(CLAMPF(avgG) + 0.5f);
    *outB = (uint8_t)(CLAMPF(avgB) + 0.5f);
    #undef CLAMPF
}
// ---- sister-end ----

#undef malloc
#undef realloc
#undef free

// ---------------------------------------------------------------------
// [v22] Glue: this project's own code from here to the end of the block.
// ---------------------------------------------------------------------
static uint8_t g_smoothedRgb[MAX_TOTAL_ZONES][3];  // sample_thread.c's smoothing state
static bool    g_smoothedRgbValid = false;         // false until the first real frame, and again after a reload

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

// Reads the ini into g_config. This follows the sister's ambient_load_config() line for line, minus [compat],
// the relay and [dev] settings, and with ONE deliberate difference: the preset is [preset_game] whatever
// [presets] active= says, and no media title can turn it into "movie" (the shell is not a title).
// Returns 1 if the file was read and parsed; on 0 g_config is untouched and the caller keeps running on it.
// Like the sister: a key that is missing or out of range keeps the value g_config already has.
static int shell_cfg_load(void)
{
    arena_reset();
    ini_table_s *table = ini_table_create();
    if (table == NULL || !ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) return 0;

    const char *v;
    int iv; bool bv;
    const char *ps = preset_section_pick("game", false,
                                         preset_section_has_keys(table, PRESET_SECTION_GAME),
                                         preset_section_has_keys(table, PRESET_SECTION_MOVIE));
    g_cfgPreset = ps ? ps : "flat [layout]/[color]/[timing]";
    const char *sLayout = ps ? ps : "layout";
    const char *sColor  = ps ? ps : "color";
    const char *sTiming = ps ? ps : "timing";

    if ((v = ini_table_get_entry(table, "network", "wled_host")) != NULL) {
        strncpy(g_config.wledHost, v, sizeof(g_config.wledHost) - 1);
        g_config.wledHost[sizeof(g_config.wledHost) - 1] = '\0';
    }
    if (ini_table_get_entry_as_int(table, "network", "wled_port", &iv) && iv > 0 && iv <= 65535)
        g_config.wledPort = (uint16_t)iv;

    if (ini_table_get_entry_as_int(table, "layout", "led_count_top", &iv) && iv > 0)
        g_config.ledCountTop = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_right", &iv) && iv > 0)
        g_config.ledCountRight = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_bottom", &iv) && iv > 0)
        g_config.ledCountBottom = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_left", &iv) && iv > 0)
        g_config.ledCountLeft = (uint32_t)iv;
    g_config.startCorner = parse_start_corner(ini_table_get_entry(table, "layout", "led_start_corner"), g_config.startCorner);
    g_config.direction = parse_direction(ini_table_get_entry(table, "layout", "led_direction"), g_config.direction);
    if (ini_table_get_entry_as_int(table, "layout", "led_offset", &iv))
        g_config.ledOffset = iv;
    if (ini_table_get_entry_as_int(table, sLayout, "scan_depth", &iv) && iv >= 0 && iv <= SCAN_DEPTH_MAX)
        g_config.scanDepth = (uint32_t)iv;

    if (ini_table_get_entry_as_int(table, sColor, "brightness", &iv) && iv >= 0 && iv <= 255)
        g_config.brightness = (uint32_t)iv;
    g_config.gammaLutIndex = parse_gamma_index(ini_table_get_entry(table, sColor, "gamma"), g_config.gammaLutIndex);
    if (ini_table_get_entry_as_int(table, sColor, "saturation", &iv))
        g_config.saturation = iv < -100 ? -100 : (iv > SATURATION_MAX ? SATURATION_MAX : iv);
    g_config.colorOrder = parse_color_order(ini_table_get_entry(table, "color", "color_order"), g_config.colorOrder);
    if (ini_table_get_entry_as_int(table, sColor, "black_level", &iv) && iv >= 0 && iv <= 100)
        g_config.blackLevel = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, sColor, "white_level", &iv) && iv >= 0 && iv <= 100)
        g_config.whiteLevel = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, sColor, "dark_threshold", &iv) && iv >= 0 && iv <= 255)
        g_config.darkThreshold = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, sColor, "contrast", &iv))
        g_config.contrast = iv < -100 ? -100 : (iv > CONTRAST_MAX ? CONTRAST_MAX : iv);
    if (ini_table_get_entry_as_int(table, sColor, "brightness_r", &iv) && iv >= 0 && iv <= 500)
        g_config.brightnessR = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, sColor, "brightness_g", &iv) && iv >= 0 && iv <= 500)
        g_config.brightnessG = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, sColor, "brightness_b", &iv) && iv >= 0 && iv <= 500)
        g_config.brightnessB = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, sColor, "gamma_r", &iv) && iv >= 10 && iv <= 500)
        g_config.gammaR = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, sColor, "gamma_g", &iv) && iv >= 10 && iv <= 500)
        g_config.gammaG = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, sColor, "gamma_b", &iv) && iv >= 10 && iv <= 500)
        g_config.gammaB = (uint32_t)iv;
    ambient_rebuild_perchannel_gamma_luts();

    if (ini_table_get_entry_as_int(table, "timing", "update_frequency_hz", &iv) && iv > 0 && iv <= 240)
        g_config.updateFrequencyHz = (uint32_t)iv;
    if (ini_table_get_entry_as_bool(table, sTiming, "smoothing_enabled", &bv))
        g_config.smoothingEnabled = bv ? 1 : 0;
    if (ini_table_get_entry_as_int(table, sTiming, "settling_time_ms", &iv) && iv >= 0)
        g_config.settlingTimeMs = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "timing", "config_reload_check_seconds", &iv) && iv >= 0)
        g_config.configReloadCheckSeconds = (uint32_t)iv;

    sh_parse_aux(table); // [v24]
    // [v23] [shell] enabled=: probe-only key, not in the sister's struct.
    if (ini_table_get_entry_as_bool(table, "shell", "enabled", &bv))
        g_shellEnabled = bv ? 1 : 0;
    else
        g_shellEnabled = 1; // absent = enabled

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

    arena_reset();
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

    arena_reset();
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
    uint32_t smoothingAlpha = 256u;
// ---- sister-begin: sample_thread.c smoothing alpha ----
        uint32_t smoothingIntervalMs = sampleIntervalUs / 1000u;
        smoothingAlpha = (smoothingIntervalMs * 256u) / (g_config.settlingTimeMs + 1u);
        if (smoothingAlpha > 256u) smoothingAlpha = 256u;
// ---- sister-end ----
    for (uint32_t i = 0; i < g_numZones; i++) {
        uint8_t r, g, b;
        sampleZoneAverage(&kParamsBase, bufferAddr, unpack_argb, g_zoneX[i], g_zoneY[i], &r, &g, &b);
        uint8_t verified[3];
        applyColorProcessing(r, g, b, verified);
        applyDarkThreshold(i, &verified[0], &verified[1], &verified[2]);
// ---- sister-begin: sample_thread.c smoothing ----
                    if (g_config.smoothingEnabled) {
                        if (!g_smoothedRgbValid) {
                            g_smoothedRgb[i][0] = verified[0];
                            g_smoothedRgb[i][1] = verified[1];
                            g_smoothedRgb[i][2] = verified[2];
                        } else {
                            for (int c = 0; c < 3; c++) {
                                int32_t prev = g_smoothedRgb[i][c];
                                int32_t raw = verified[c];
                                int32_t delta = ((raw - prev) * (int32_t)smoothingAlpha) / 256;
                                // v2.7.5: C's / truncates toward zero, not
                                // toward -infinity -- so once |raw-prev| is
                                // small enough that |delta| rounds down to
                                // 0, prev stops moving AT ALL, forever,
                                // even though raw != prev. Confirmed by
                                // simulation with this project's own
                                // defaults (update_frequency_hz=30,
                                // settling_time_ms=100 -> smoothingAlpha=83):
                                // decaying toward a target of 0 from ANY
                                // starting brightness lands on exactly 3
                                // and sticks there permanently -- a dim
                                // residual glow that never reaches black on
                                // its own. This is what was reported as "a
                                // colorful scene's colors are retained
                                // dimly" after a cut to black. Forcing a
                                // minimum step of 1 toward raw whenever the
                                // proper proportional step would otherwise
                                // round to 0 guarantees this always
                                // converges to the exact target within a
                                // few extra frames, instead of stalling
                                // short of it indefinitely.
                                if (delta == 0 && raw != prev) delta = (raw > prev) ? 1 : -1;
                                g_smoothedRgb[i][c] = (uint8_t)(prev + delta);
                            }
                        }
                        rgbTriplets[i*3+0] = g_smoothedRgb[i][0];
                        rgbTriplets[i*3+1] = g_smoothedRgb[i][1];
                        rgbTriplets[i*3+2] = g_smoothedRgb[i][2];
                    } else {
                        rgbTriplets[i*3+0] = verified[0];
                        rgbTriplets[i*3+1] = verified[1];
                        rgbTriplets[i*3+2] = verified[2];
                    }
// ---- sister-end ----
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
// self-heal, one log line per gate. Probe-only: no sister-begin block is touched.
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
    arena_reset();
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
        arena_reset();
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
