// state_file.h -- the text of the cross-process state file (v3.9.2).
//
// /data/ps4-ambilight/ps4_ambient_state tells a second, independent process -- a compositor-level feed
// reading the shell's own screens (Home, Settings, the quick menu) -- whether the game title is foregrounded, so the two never fight over the strip. This
// plugin has no code that reads the other side; it only writes its own state,
// unconditionally, the same way it already exposes report.c's counters. Holds no personal data:
// a flag, a counter and a title ID, the same title ID already shown in the report file.
//
// Deliberately NOT like report.c's file: that one is rewritten with O_TRUNC, fine for an
// occasional diagnostic nobody is polling. This file is read by another process several times
// a second, so every record is a FIXED WIDTH and every write lands at offset 0 WITHOUT
// truncating -- a reader can only ever see the old complete record or the new one, never a torn
// write. state.c fills a StateSnapshot from the live globals and does the file I/O; this header
// is string logic only, so tools/test_state.c can build and check it on a PC (same split as
// report_file.h / report.c).
#ifndef STATE_FILE_H
#define STATE_FILE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ambilight_paths.h"

#define STATE_PATH        AMBILIGHT_DATA_DIR "/ps4_ambient_state"
#define STATE_TITLE_LEN    9    // PS4 title IDs are always exactly this (e.g. "CUSA00000")

// "v1 fg=0 seq=000000 title=CUSA00000\n" -- 6+1+5+6+7+9+1 = 35 bytes, constant for every
// fg (0/1, one digit) and every seq (wrapped mod 1,000,000, always 6 digits). Checked equal
// to this constant by tools/test_state.c, not just hand-counted.
#define STATE_RECORD_LEN  35

typedef struct {
    int      foreground;    // 1 while !g_isBackgrounded; independent of output/transmission activity
    uint32_t seq;            // increments on each transition into foreground; wraps mod 1,000,000
    char     titleId[16];    // "" when sys_sdk_proc_info hasn't set it yet
} StateSnapshot;

// Right-pads/truncates to exactly STATE_TITLE_LEN so the record's width never depends on the
// title ID's own length. Every real PS4 title ID is already this length; this is defense in
// depth for a malformed or empty one, not expected to trigger in practice.
static inline void state_title_fixed(char out[STATE_TITLE_LEN + 1], const char *titleId)
{
    const char *src = (titleId && titleId[0]) ? titleId : "-";
    size_t n = strlen(src);
    if (n > STATE_TITLE_LEN) n = STATE_TITLE_LEN;
    memset(out, ' ', STATE_TITLE_LEN);
    memcpy(out, src, n);
    out[STATE_TITLE_LEN] = '\0';
}

// Returns STATE_RECORD_LEN on success, 0 if something made the formatted length come out
// different (should not happen; a caller treats 0 as "don't write this turn, retry next tick").
// `cap` must be at least STATE_RECORD_LEN + 1.
static inline size_t state_format(char *out, size_t cap, int foreground, uint32_t seq, const char *titleId)
{
    char title[STATE_TITLE_LEN + 1];
    state_title_fixed(title, titleId);
    int n = snprintf(out, cap, "v1 fg=%d seq=%06u title=%s\n",
                      foreground ? 1 : 0, (unsigned)(seq % 1000000u), title);
    return (n == STATE_RECORD_LEN) ? (size_t)n : 0;
}

#endif
