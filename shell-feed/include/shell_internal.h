// shell_internal.h -- what the shell feed's source files share.
// More: docs/code-comments.md (shell-feed/include/shell_internal.h:1)
#ifndef SHELL_INTERNAL_H
#define SHELL_INTERNAL_H

#define DO_WLED_SEND             1   // [v18] ON: this is the live-send run
#define DO_LIVE_DDP              1   // [v19] FIX: v18 used this in two #if lines but never defined it, so the whole send loop compiled out
#define DO_WLED_SEND_INCLUDES    DO_WLED_SEND
#define WLED_HOST                ""  // no compiled-in fallback: with no wled_host in the ini the shell feed does not start (same as the plugin). Was the maintainer's own WLED, which sent UDP to that address on other users' networks when wled_host was unset.
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
#include "ambilight_paths.h"   // AMBILIGHT_DATA_DIR: every file this feed reads or writes is in that one folder

#if DO_WLED_SEND_INCLUDES
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#define DISABLE_FILE  AMBILIGHT_DATA_DIR "/ps4_shell_feed_off"  // [v23] create over FTP to silence the feed without a reboot
#define TAG           "[SHELLPROBE24]"
#define LIVE_HZ         30     // [v18] matches the sister project's own rate
// LIVE_SECONDS and REPORT_SECONDS removed in v23: the send loop runs until the process exits.

// [v20] the sister plugin's claim file. Format and contract: PS4-Ambilight v3.9.2+ (v3.9.3 writes fg=1 sooner),
// docs/development/shell-feed-coordination.md. This probe only reads it.
#define STATE_PATH        AMBILIGHT_DATA_DIR "/ps4_ambient_state"
#define STATE_RECORD_LEN  35
#define STATE_POLL_MS     100  // poll the explicit foreground bit at 10 Hz
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
#define SHELL_REPORT_FILE        1   // [v24] report thread + /data/ps4-ambilight/ps4_ambient_report_SHELL.txt (still needs [compat] report_file=1)
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

// The plugin's own headers, compiled-in code for the 7 shared files (see the Makefile's SHAREDFILES).

#include "ambient_internal.h"
#include "config.h"
#include "preset_select.h"
#include "smoothing.h"

#define BUF_BYTES17 ((uint64_t)1920 * 1088 * 4)       // 8,355,840

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

// [v21] This probe's zone order is top L->R (73), right T->B (41), bottom R->L (73), left B->T (42)...
// More: docs/code-comments.md (shell-feed/include/shell_internal.h:113)
#define LED_ROT    LED_LEFT

static inline uint64_t rdtsc17(void)
{
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

struct Buf { uint64_t start, len; };

typedef struct {
    uint32_t consecFail;     // bad ticks in a row; any good tick resets it
    uint32_t heals;          // re-scans started
    uint32_t recovered;      // re-scans followed by a good tick
    uint32_t failed;         // re-scans that found no display buffer at all
    uint32_t unrecovered;    // re-scans in a row with no good tick since; drives the back-off
    uint64_t lastHealMs;
    int      awaiting;       // a re-scan happened and no good tick has followed yet
} HealState;

enum { SH_STARTING = 0, SH_LIVE = 1, SH_STANDBY = 2, SH_STOPPED = 3 };

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

// Shared between the shell feed's own files.
// More: docs/code-comments.md (shell-feed/include/shell_internal.h:159)
#pragma GCC visibility push(hidden)

// shell_modules.c
int   first_byte(void *p);
int   name_has_ci(const char *name, const char *needle);
void  list_modules_api(void);
struct ScanResult scan_and_discover(const char *phase);
void *resolve(int32_t handle, const char *lib, const char *sym);

// shell_hooks.c  (the counters are written by the hooks and read by the sampler and the report)
extern volatile uint32_t g_regCount;
extern volatile uint32_t g_flipCount;
extern volatile int32_t  g_lastFlipIndex;
void shell_hooks_install(void *pReg, void *pFlip, int *patchedReg, int *patchedFlip);
#if DO_CONTROL_CALLS
void shell_hooks_control_calls(void *pReg, void *pFlip, int patchedReg, int patchedFlip);
#endif

// shell_buffers.c
extern struct Buf g_buf[2];
extern int        g_nBuf;
extern uint64_t   g_tscHz;
extern HealState  g_heal;
void map_scan(int verbose);
int  buf_ok(const struct Buf *b);
void find_bufs(void);
void calibrate_tsc(void);
int  heal_tick(HealState *h, int bufOk, uint64_t nowMs);
#if DO_LIVE_DDP
void sh_do_heal(void);
#endif

// shell_probe17.c
extern int g_badPts;
void build_points(void);

// shell_config.c
extern int          g_cfgHaveBaseline;
extern int64_t      g_cfgLastSize;
extern uint32_t     g_cfgLastHash;
extern int          g_cfgLastReadOk;
extern uint32_t     g_cfgReloads;
extern volatile int g_shReportOn;
extern int          g_shellEnabled;
void sh_parse_aux(ini_table_s *table);
int  shell_cfg_load(void);
void shell_cfg_log(const char *why, int64_t bytes, uint32_t hash);
int  shell_cfg_poll(uint64_t nowTsc, uint64_t tscHz);
void shell_enabled_poll(uint64_t nowTsc, uint64_t tscHz, uint64_t *nextPollTsc);

// shell_frame.c
uint32_t shell_process_frame(uint64_t bufferAddr, uint32_t sampleIntervalUs, uint8_t *rgbTriplets);

// shell_state.c
extern int      g_claimEverSeen;
extern int      g_claimActive;
extern uint64_t g_liveStartTsc;
int  claim_poll(uint64_t nowTsc, uint64_t tscHz);
void shell_heartbeat(uint64_t nowTsc, uint64_t tscHz, uint64_t *nextBeatTsc, uint64_t counter);

// shell_relay.c
void shell_relay_update(int shellActive);
void shell_relay_tick(uint64_t nowTsc, uint64_t tscHz);

// shell_log.c
extern int  g_shDevLog;
extern char g_shDevIp[16];
int  sh_dev_active(int devLogging, const char *devIp);
void sh_dev_send(const char *text);
void shell_event(const char *fmt, ...);
void sh_notify(bool essential, const char *fmt, ...);

// shell_report.c
extern volatile int      g_shState;
extern volatile int      g_shStopReason;
extern volatile uint32_t g_shLiveSessions;
extern char              g_shNotStartedWhy[40];
void sh_publish(uint64_t sent, uint64_t skipped, uint64_t gameOwned);
void shell_gates_log(void);
#if SHELL_REPORT_FILE
void *shell_report_thread(void *args);
#endif

// shell_live.c
#if DO_LIVE_DDP
int live_send(void);
#endif

// shell_thread.c
void *probe_thread(void *args);
#if DO_LIVE_DDP
void sh_runtime_gates_note(void);
#endif

#pragma GCC visibility pop

#endif // SHELL_INTERNAL_H
