// load_trace_text.h -- the pure part of the load log (v4.0.1): text, buffer and progress rules,
// with no console calls, so a host test can drive it (same split as report_file.h / report.c).
//
// The load log is ONE file, /data/ps4-ambilight/ps4_ambient_loadlog.txt, that every title the plugin is
// loaded into appends to, each line tagged with the title ID. A tester sends that one file instead of
// working out which per-title report to look for. It is written only with [compat] report_file=1.
//
// It exists because "no report file" could not be told apart from "the plugin was never loaded" or "the
// plugin stopped early in plugin_load". Every step of plugin_load leaves a line, so the last line says
// where it stopped, and no line for a title at all means GoldHEN never loaded the plugin into it.
#ifndef LOAD_TRACE_TEXT_H
#define LOAD_TRACE_TEXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ambilight_paths.h"

#define LOADTRACE_PATH      AMBILIGHT_DATA_DIR "/ps4_ambient_loadlog.txt"
#define LOADTRACE_BUF_MAX   6144     // lines wait here until report_file=1 is known; plugin_load writes well under 3 KB
#define LOADTRACE_LINE_MAX  256
#define LOADTRACE_FILE_CAP  49152    // a launch that finds the file bigger than this starts it over (once per launch)

// ---- the line buffer ---------------------------------------------------------------------------

typedef struct {
    char   text[LOADTRACE_BUF_MAX];
    size_t len;        // bytes held
    size_t flushed;    // of those, bytes already on disk
    int    full;       // a line did not fit: the marker was added and nothing more is kept
} LoadTraceBuf;

// The title ID reduced to letters, digits and underscore (nothing odd can reach the file); "?" when empty.
static inline void lt_clean_id(char *out, size_t cap, const char *id)
{
    size_t n = 0;
    if (cap == 0) return;
    for (const char *p = id; p != NULL && *p != '\0' && n < cap - 1 && n < 15; p++) {
        char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
            out[n++] = c;
    }
    if (n == 0) out[n++] = '?';
    out[n] = '\0';
}

// Adds "[TITLE] msg\n". A message with a newline in it, or one too long, is cut: one line stays one line.
static inline void ltb_add_line(LoadTraceBuf *b, const char *titleId, const char *msg)
{
    static const char marker[] = "(load log buffer full, later lines dropped)\n";
    if (b->full) return;
    char tag[16];
    lt_clean_id(tag, sizeof(tag), titleId);
    char line[LOADTRACE_LINE_MAX];
    int n = snprintf(line, sizeof(line), "[%s] %s", tag, msg != NULL ? msg : "");
    if (n < 0) return;
    size_t len = (size_t)n >= sizeof(line) ? sizeof(line) - 1 : (size_t)n;
    for (size_t i = 0; i < len; i++) if (line[i] == '\n' || line[i] == '\r') line[i] = ' ';
    // Keep room for the marker so a full buffer still ends with a line saying so.
    if (b->len + len + 1 + sizeof(marker) > sizeof(b->text)) {
        if (b->len + sizeof(marker) <= sizeof(b->text)) {
            memcpy(b->text + b->len, marker, sizeof(marker) - 1);
            b->len += sizeof(marker) - 1;
        }
        b->full = 1;
        return;
    }
    memcpy(b->text + b->len, line, len);
    b->text[b->len + len] = '\n';
    b->len += len + 1;
}

static inline const char *ltb_pending(const LoadTraceBuf *b, size_t *n)
{
    *n = b->len - b->flushed;
    return b->text + b->flushed;
}

static inline void ltb_mark_flushed(LoadTraceBuf *b, size_t n)
{
    b->flushed += n;
    if (b->flushed > b->len) b->flushed = b->len;
}

// ---- progress after load: what the title does once the hooks are in -----------------------------
// The report thread's one-second turn calls lt_progress_step. Counters in, at most one event out per call; each
// event is reported once. Nothing here reads the console.

#define LT_WAIT_SECS 20   // how long a title may take before "nothing yet" is said

typedef struct {
    uint32_t registerCalls;
    uint32_t flipGnm, flipVideoOut, flipWorkload;
    uint32_t loopPasses;
    int32_t  bufferCount;
    int      haveValidFormat;
    uint32_t activeFormat;
    int      backgrounded;
    uint32_t guardRejects;
} LtCounters;

typedef enum {
    LT_EV_NONE = 0,
    LT_EV_REGISTERED,        // first sceVideoOutRegisterBuffers call seen
    LT_EV_FLIPPING,          // first flip seen
    LT_EV_RUNNING,           // the sampler finished its first pass
    LT_EV_FORMAT_UNKNOWN,    // buffers registered in a format the plugin doesn't decode
    LT_EV_NO_REGISTER,       // LT_WAIT_SECS in, no buffers registered and no flips
    LT_EV_NO_FLIPS,          // buffers registered, no flips
    LT_EV_NO_SAMPLER,        // flips arriving, the sampler never finished a pass (and the title is in the foreground)
    LT_EV_COUNT
} LtEvent;

typedef struct {
    uint32_t secs;           // calls so far, one per second
    unsigned said;           // bit per LtEvent already reported
} LtProgress;

static inline int lt_flip_total(const LtCounters *c) { return (int)(c->flipGnm + c->flipVideoOut + c->flipWorkload > 0); }

static inline LtEvent lt_progress_step(LtProgress *p, const LtCounters *c)
{
    p->secs++;
    int flips = lt_flip_total(c);
    LtEvent cand[LT_EV_COUNT - 1];
    int nc = 0;
    if (c->registerCalls > 0)                                            cand[nc++] = LT_EV_REGISTERED;
    if (flips)                                                           cand[nc++] = LT_EV_FLIPPING;
    // The sampler counts passes before a title has registered any buffer (a base PS4 log showed 30 at T+1s), so
    // "running" waits for the registration: otherwise a title that never gets going would be reported as captured.
    if (c->loopPasses > 0 && c->registerCalls > 0)                       cand[nc++] = LT_EV_RUNNING;
    if (c->registerCalls > 0 && !c->haveValidFormat)                     cand[nc++] = LT_EV_FORMAT_UNKNOWN;
    if (p->secs >= LT_WAIT_SECS) {
        if (c->registerCalls == 0 && !flips)                             cand[nc++] = LT_EV_NO_REGISTER;
        if (c->registerCalls > 0 && !flips)                              cand[nc++] = LT_EV_NO_FLIPS;
        if (flips && c->loopPasses == 0 && !c->backgrounded)             cand[nc++] = LT_EV_NO_SAMPLER;
    }
    for (int i = 0; i < nc; i++) {
        unsigned bit = 1u << (unsigned)cand[i];
        if (!(p->said & bit)) { p->said |= bit; return cand[i]; }
    }
    return LT_EV_NONE;
}

// True for events worth an on-screen notice. The others go to the file only: the existing notices already
// cover an unknown format, and "registered" / "flipping" are routine.
static inline int lt_event_notifies(LtEvent ev)
{
    return ev == LT_EV_RUNNING || ev == LT_EV_NO_REGISTER || ev == LT_EV_NO_FLIPS || ev == LT_EV_NO_SAMPLER;
}

static inline void lt_event_text(char *out, size_t cap, LtEvent ev, const LtCounters *c, uint32_t secs)
{
    switch (ev) {
    case LT_EV_REGISTERED:
        snprintf(out, cap, "T+%us register: buffers %d, format 0x%08x (%s)", (unsigned)secs, (int)c->bufferCount,
                 (unsigned)c->activeFormat, c->haveValidFormat ? "recognized" : "NOT recognized");
        break;
    case LT_EV_FLIPPING:
        snprintf(out, cap, "T+%us flips seen: gnm %u, videoout %u, workload %u", (unsigned)secs,
                 (unsigned)c->flipGnm, (unsigned)c->flipVideoOut, (unsigned)c->flipWorkload);
        break;
    case LT_EV_RUNNING:
        snprintf(out, cap, "T+%us sampler running: %u passes, format 0x%08x, guard rejects %u", (unsigned)secs,
                 (unsigned)c->loopPasses, (unsigned)c->activeFormat, (unsigned)c->guardRejects);
        break;
    case LT_EV_FORMAT_UNKNOWN:
        snprintf(out, cap, "T+%us format 0x%08x is not recognized, so no colours will be sent", (unsigned)secs,
                 (unsigned)c->activeFormat);
        break;
    case LT_EV_NO_REGISTER:
        snprintf(out, cap, "T+%us WAITING: hooked, but no buffer was registered and no flip came (register 0, flips 0)",
                 (unsigned)secs);
        break;
    case LT_EV_NO_FLIPS:
        snprintf(out, cap, "T+%us WAITING: %u buffer registration(s) but no flips (gnm 0, videoout 0, workload 0)",
                 (unsigned)secs, (unsigned)c->registerCalls);
        break;
    case LT_EV_NO_SAMPLER:
        snprintf(out, cap, "T+%us WAITING: flips arrive (gnm %u, videoout %u, workload %u) but the sampler has 0 passes",
                 (unsigned)secs, (unsigned)c->flipGnm, (unsigned)c->flipVideoOut, (unsigned)c->flipWorkload);
        break;
    default:
        if (cap > 0) out[0] = '\0';
        break;
    }
}

// The on-screen version: short, the title first. Only for the events lt_event_notifies accepts.
static inline void lt_event_notice_text(char *out, size_t cap, LtEvent ev, const LtCounters *c, const char *titleId)
{
    char tag[16];
    lt_clean_id(tag, sizeof(tag), titleId);
    switch (ev) {
    case LT_EV_RUNNING:
        snprintf(out, cap, "diag: %s is being captured (format 0x%08x).", tag, (unsigned)c->activeFormat);
        break;
    case LT_EV_NO_REGISTER:
        snprintf(out, cap, "diag: %s is hooked but never registered a screen buffer in %d s.", tag, LT_WAIT_SECS);
        break;
    case LT_EV_NO_FLIPS:
        snprintf(out, cap, "diag: %s registered buffers but no frames are flipping.", tag);
        break;
    case LT_EV_NO_SAMPLER:
        snprintf(out, cap, "diag: %s is flipping frames but the sampler has not started.", tag);
        break;
    default:
        if (cap > 0) out[0] = '\0';
        break;
    }
}

// A one-line picture of all the counters, written at 10, 30 and 60 seconds so the file ends with a summary
// even when no event fired.
static inline int lt_summary_due(uint32_t secs) { return secs == 10 || secs == 30 || secs == 60; }

static inline void lt_summary_text(char *out, size_t cap, const LtCounters *c, uint32_t secs)
{
    snprintf(out, cap,
             "T+%us summary: register %u, flips gnm %u / videoout %u / workload %u, loop_passes %u, buffers %d, "
             "format 0x%08x (%s), guard_rejects %u, backgrounded %s",
             (unsigned)secs, (unsigned)c->registerCalls, (unsigned)c->flipGnm, (unsigned)c->flipVideoOut,
             (unsigned)c->flipWorkload, (unsigned)c->loopPasses, (int)c->bufferCount, (unsigned)c->activeFormat,
             c->haveValidFormat ? "recognized" : "not recognized", (unsigned)c->guardRejects,
             c->backgrounded ? "yes" : "no");
}

#endif // LOAD_TRACE_TEXT_H
