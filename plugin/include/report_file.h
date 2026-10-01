// report_file.h -- the text of the per-title report file (v3.9.1).
//
// A tester who reports "the strip stays dark in game X" has so far had to run a debug build
// and capture UDP packets. The release build now writes a short text file instead,
// /data/ps4_ambient_report_<TITLEID>.txt, and the tester sends that. Everything in it is
// already kept in plain globals in every build; only the packets that send them are
// debug-only. It holds no personal data: no WLED address, no ini values, nothing typed.
//
// This header is string logic only, with no console calls, so tools/test_report.c can build
// it on a PC (same idea as preset_select.h and pq8bit_vote.h). report.c fills a
// ReportSnapshot from the live globals and does the file I/O.
#ifndef REPORT_FILE_H
#define REPORT_FILE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define REPORT_PATH_PREFIX   "/data/ps4_ambient_report_"
#define REPORT_PATH_SUFFIX   ".txt"
#define REPORT_PATH_MAX      96
#define REPORT_TEXT_MAX      2048
#define REPORT_REFRESH_SECS  60   // rewrite this often even when nothing changed (timing, counters)

typedef struct {
    char     version[16];          // "3.9.1"
    char     build[12];            // first 7 characters of the commit the plugin was built from
    char     titleId[16];          // "" when sys_sdk_proc_info failed
    uint32_t uptimeSecs;           // seconds since the report thread started, approximately

    uint32_t registerCalls;        // sceVideoOutRegisterBuffers hook calls
    uint32_t flipGnm, flipVideoOut, flipWorkload;   // the three flip entry points, calls each
    int32_t  bufferCount;
    int      haveValidFormat;
    uint32_t activeFormat;
    uint32_t loopPasses;           // sampler loop iterations that got to the bottom, foreground only
    int      backgrounded;

    uint32_t timingWindows;        // completed 30-pass timing windows, 0 = no timing data yet
    uint32_t passUsAvg, passUsMax; // last window

    uint32_t guardAvailable, guardRejects;
    int32_t  guardLastRet;
    uint64_t guardLastBadAddr;

    uint32_t remapSetting, remapCreated, remapFailed, remapPasses, remapLastMethod;
    int32_t  remapLastRet;
} ReportSnapshot;

// Where the title has got to, from the counters alone. "running" means the sampler loop has
// completed at least one pass. A game that dies on its first pass is left at "flipping",
// which is why the file is written on every change and not only at the end.
static inline const char *report_stage(const ReportSnapshot *s)
{
    uint32_t flips = s->flipGnm + s->flipVideoOut + s->flipWorkload;
    if (s->loopPasses > 0) return "running";
    if (flips > 0)         return "flipping";
    if (s->registerCalls > 0) return "registered";
    return "hooked";
}

static inline const char *report_remap_method(uint32_t m)
{
    switch (m) {
        case 0:  return "none";
        case 1:  return "typed alias";
        case 2:  return "plain alias";
        case 3:  return "mprotect";
        default: return "unknown";
    }
}

// /data/ps4_ambient_report_<TITLEID>.txt. The title ID is reduced to letters, digits and
// underscore so nothing odd can reach the path; an empty or all-odd one becomes "unknown".
static inline void report_build_path(char *out, size_t cap, const char *titleId)
{
    char id[16];
    size_t n = 0;
    for (const char *p = titleId; p != NULL && *p != '\0' && n < sizeof(id) - 1; p++) {
        char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
            id[n++] = c;
    }
    if (n == 0) { memcpy(id, "unknown", 8); }
    else        { id[n] = '\0'; }
    snprintf(out, cap, "%s%s%s", REPORT_PATH_PREFIX, id, REPORT_PATH_SUFFIX);
}

// True when something a person reading the file cares about moved: the stage, the format,
// or the guard/remap outcome. The per-frame counters (flips, passes, timing) are left out on
// purpose, or this would rewrite the file every second.
static inline int report_changed(const ReportSnapshot *a, const ReportSnapshot *b)
{
    return strcmp(report_stage(a), report_stage(b)) != 0
        || a->haveValidFormat != b->haveValidFormat
        || a->activeFormat    != b->activeFormat
        || a->bufferCount     != b->bufferCount
        || a->guardAvailable  != b->guardAvailable
        || (a->guardRejects  > 0) != (b->guardRejects  > 0)
        || a->guardLastRet    != b->guardLastRet
        || a->remapCreated    != b->remapCreated
        || a->remapFailed     != b->remapFailed
        || a->remapLastMethod != b->remapLastMethod
        || a->remapLastRet    != b->remapLastRet
        || (a->remapPasses   > 0) != (b->remapPasses   > 0)
        || a->backgrounded    != b->backgrounded;
}

// Fills `out` (NUL terminated, never longer than cap - 1) and returns its length. The last
// line is always "end of report", so a cut-off or half-written file is easy to tell.
static inline size_t report_format(char *out, size_t cap, const ReportSnapshot *s)
{
    if (cap == 0) return 0;
    int n = snprintf(out, cap,
        "PS4 Ambilight report\n"
        "Send this file when you report a game. It has no personal data in it.\n"
        "\n"
        "plugin:         %s\n"
        "build:          %s\n"
        "title:          %s\n"
        "stage:          %s\n"
        "uptime_s:       %u\n"
        "\n"
        "format:         0x%08x (%s)\n"
        "buffers:        %d\n"
        "register_calls: %u\n"
        "flip_calls:     gnm %u, videoout %u, workload %u\n"
        "loop_passes:    %u\n"
        "backgrounded:   %s\n"
        "pass_us:        avg %u, max %u (last 30 passes; %u windows)\n"
        "\n"
        "guard:          available %s, rejects %u, last_ret %d, last_bad_addr 0x%llx\n"
        "remap:          setting %u, created %u, failed %u, passes %u, last_method %s, last_ret %d\n"
        "\n"
        "end of report\n",
        s->version, s->build, s->titleId[0] ? s->titleId : "unknown", report_stage(s),
        (unsigned)s->uptimeSecs,
        (unsigned)s->activeFormat, s->haveValidFormat ? "recognized" : "not recognized",
        (int)s->bufferCount,
        (unsigned)s->registerCalls,
        (unsigned)s->flipGnm, (unsigned)s->flipVideoOut, (unsigned)s->flipWorkload,
        (unsigned)s->loopPasses,
        s->backgrounded ? "yes" : "no",
        (unsigned)s->passUsAvg, (unsigned)s->passUsMax, (unsigned)s->timingWindows,
        s->guardAvailable ? "yes" : "no", (unsigned)s->guardRejects, (int)s->guardLastRet,
        (unsigned long long)s->guardLastBadAddr,
        (unsigned)s->remapSetting, (unsigned)s->remapCreated, (unsigned)s->remapFailed,
        (unsigned)s->remapPasses, report_remap_method(s->remapLastMethod), (int)s->remapLastRet);
    if (n < 0) { out[0] = '\0'; return 0; }
    if ((size_t)n >= cap) {
        // Never expected at REPORT_TEXT_MAX (the test checks), but if a future field makes it
        // overflow, keep the terminator line so the file still reads as complete.
        static const char tail[] = "\n(cut off)\nend of report\n";
        size_t t = sizeof(tail) - 1;
        if (cap > t) memcpy(out + cap - 1 - t, tail, t);
        out[cap - 1] = '\0';
        return cap - 1;
    }
    return (size_t)n;
}

#endif // REPORT_FILE_H
