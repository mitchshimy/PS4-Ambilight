// load_trace.h -- the load log (v4.0.1). See load_trace_text.h for what it is and why.
//
// Everything here is a no-op unless the ini has [compat] report_file=1: lines are only kept in memory
// until that is known, nothing is written or shown without it, and the progress thread is only started
// when it is on. An ordinary install behaves exactly as before.
#ifndef LOAD_TRACE_H
#define LOAD_TRACE_H

#include <stdint.h>

#include "load_trace_text.h"

// One line in /data/ps4-ambilight/ps4_ambient_loadlog.txt, tagged with the title ID. Before the config
// is read the line waits in memory; once report_file=1 is known it is written at once (and each later
// line is written as it is made), so a crash leaves the last step reached on disk.
void load_trace_note(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// An on-screen "PS4 Ambilight: ..." notice, only with report_file=1 and whatever [notifications] says,
// because it is a debugging aid. The text is also written to the load log.
void load_trace_notice(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Logs the first byte at each hook target (0xE9 is the forwarding stub the plugin refuses). Reads the same
// bytes plugin_load's own guard reads, only with report_file=1. NULL for an optional target that did not resolve.
void load_trace_hook_targets(const void *registerBuffers, const void *gnmSubmitAndFlip,
                             const void *submitFlip, const void *gnmWorkload);

// One second of watching what the title does after the hooks are in (registered, flipping, sampling, or stuck).
// The state is the caller's; with report_file off it only counts the second.
void load_trace_progress_once(LtProgress *prog);

// The thread that calls it once a second. Started by plugin_load only when report_file=1 at load.
void *load_trace_thread(void *args);

#endif // LOAD_TRACE_H
