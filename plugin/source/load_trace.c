// load_trace.c -- part of ps4_ambient_light (v4.0.1), plugin only (the shell feed does not build it).
// The console side of the load log: appends to /data/ps4-ambilight/ps4_ambient_loadlog.txt, shows the
// diagnostic notices, and follows a title's progress after load. The text and the rules are in
// load_trace_text.h so a host test can drive them. Nothing here does anything unless [compat] report_file=1.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <orbis/libkernel.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "load_trace_text.h"
#include "load_trace.h"

static LoadTraceBuf  s_buf;            // static so it stays off the stack
static int           s_trunc = -1;     // -1 not decided yet; 1 the next write starts the file over; 0 append
static volatile int  s_lock;           // plugin_load and the progress thread can both write a line

static void lt_lock(void)   { while (__sync_lock_test_and_set(&s_lock, 1)) usleep(200); }
static void lt_unlock(void) { __sync_lock_release(&s_lock); }

// Same raw-kernel-call route report.c uses (sceKernelOpen, not fopen), with the BSD flag values:
// O_WRONLY 0x0001, O_APPEND 0x0008, O_CREAT 0x0200, O_TRUNC 0x0400.
static void lt_flush_locked(void)
{
    if (!g_reportEnabled) return;                 // nothing leaves memory without report_file=1
    size_t n = 0;
    const char *p = ltb_pending(&s_buf, &n);
    if (n == 0) return;

    if (s_trunc < 0) {                            // first write of this launch: is the file already big?
        s_trunc = 0;
        int32_t rfd = sceKernelOpen(LOADTRACE_PATH, 0 /* O_RDONLY */, 0);
        if (rfd >= 0) {
            int64_t sz = sceKernelLseek(rfd, 0, SEEK_END);
            sceKernelClose(rfd);
            if (sz > LOADTRACE_FILE_CAP) s_trunc = 1;
        }
    }
    int32_t flags = 0x0001 | 0x0008 | 0x0200;
    if (s_trunc == 1) flags |= 0x0400;
    int32_t fd = sceKernelOpen(LOADTRACE_PATH, flags, 0777);
    if (fd < 0) return;                           // the lines stay in memory and go out with the next one
    ssize_t w = sceKernelWrite(fd, p, n);
    sceKernelClose(fd);
    if (w > 0) {
        ltb_mark_flushed(&s_buf, (size_t)w);
        s_trunc = 0;                              // only the first write of a launch may start the file over
    }
}

void load_trace_note(const char *fmt, ...)
{
    char msg[LOADTRACE_LINE_MAX];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    lt_lock();
    ltb_add_line(&s_buf, g_titleId, msg);
    lt_flush_locked();
    lt_unlock();
}

void load_trace_notice(const char *fmt, ...)
{
    if (!g_reportEnabled) return;
    char msg[176];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    Notify(TEX_ICON_SYSTEM, "PS4 Ambilight: %s", msg);
    load_trace_note("notice sent: %s", msg);
}

static void byte_text(char *out, size_t cap, const void *p)
{
    if (p == NULL) snprintf(out, cap, "n/a");
    else           snprintf(out, cap, "0x%02x", (unsigned)*(const volatile uint8_t *)p);
}

void load_trace_hook_targets(const void *registerBuffers, const void *gnmSubmitAndFlip,
                             const void *submitFlip, const void *gnmWorkload)
{
    if (!g_reportEnabled) return;
    char a[8], b[8], c[8], d[8];
    byte_text(a, sizeof(a), registerBuffers);
    byte_text(b, sizeof(b), gnmSubmitAndFlip);
    byte_text(c, sizeof(c), submitFlip);
    byte_text(d, sizeof(d), gnmWorkload);
    load_trace_note("first byte at hook targets (0xE9 = forwarding stub, refused): RegisterBuffers %s, "
                    "GnmSubmitAndFlip %s, SubmitFlip %s, GnmWorkload %s", a, b, c, d);
}

// One second of progress watching. The state is the caller's, so a test can run several scenarios in one process.
void load_trace_progress_once(LtProgress *prog)
{
    if (!g_reportEnabled) { prog->secs++; return; }   // switched off by a config reload: keep time, say nothing

    LtCounters c;
    memset(&c, 0, sizeof(c));
    c.registerCalls   = g_registerHookCallCount;
    c.flipGnm         = g_flipHookCallCount;
    c.flipVideoOut    = g_videoOutSubmitFlipHookCallCount;
    c.flipWorkload    = g_gnmForWorkloadHookCallCount;
    c.loopPasses      = g_reportLoopPasses;
    c.bufferCount     = g_bufferCount;
    c.haveValidFormat = g_haveValidFormat;
    c.activeFormat    = g_activeFormat;
    c.backgrounded    = g_isBackgrounded ? 1 : 0;
    c.guardRejects    = g_guardRejectCount;

    char text[LOADTRACE_LINE_MAX];
    LtEvent ev = lt_progress_step(prog, &c);
    if (ev != LT_EV_NONE) {
        lt_event_text(text, sizeof(text), ev, &c, prog->secs);
        load_trace_note("%s", text);
        if (lt_event_notifies(ev)) {
            char notice[176];
            lt_event_notice_text(notice, sizeof(notice), ev, &c, g_titleId);
            load_trace_notice("%s", notice);
        }
    }
    if (lt_summary_due(prog->secs)) {
        lt_summary_text(text, sizeof(text), &c, prog->secs);
        load_trace_note("%s", text);
    }
}

void *load_trace_thread(void *args)
{
    (void)args;
    static LtProgress prog;                       // static: zero-initialized, off the stack
    for (;;) {
        usleep(1000000);
        load_trace_progress_once(&prog);
    }
    return NULL;
}
