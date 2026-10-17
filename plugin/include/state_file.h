// state_file.h -- the text of the cross-process state file (v3.9.2)....
// More: docs/code-comments.md (plugin/include/state_file.h:1)
#ifndef STATE_FILE_H
#define STATE_FILE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ambilight_paths.h"

#define STATE_PATH        AMBILIGHT_DATA_DIR "/ps4_ambient_state"
#define STATE_TITLE_LEN    9    // PS4 title IDs are always exactly this (e.g. "CUSA00000")

// "v1 fg=0 seq=000000 title=CUSA00000\n" -- 6+1+5+6+7+9+1 = 35 bytes, constant for every fg (0/1, one...
// More: docs/code-comments.md (plugin/include/state_file.h:29)
#define STATE_RECORD_LEN  35

typedef struct {
    int      foreground;    // 1 while !g_isBackgrounded AND the first DDP send has happened (launch latch, state.c); otherwise independent of output activity
    uint32_t seq;            // increments on each transition into foreground; wraps mod 1,000,000
    char     titleId[16];    // "" when sys_sdk_proc_info hasn't set it yet
} StateSnapshot;

// Right-pads/truncates to exactly STATE_TITLE_LEN so the record's width never depends on the title...
// More: docs/code-comments.md (plugin/include/state_file.h:40)
static inline void state_title_fixed(char out[STATE_TITLE_LEN + 1], const char *titleId)
{
    const char *src = (titleId && titleId[0]) ? titleId : "-";
    size_t n = strlen(src);
    if (n > STATE_TITLE_LEN) n = STATE_TITLE_LEN;
    memset(out, ' ', STATE_TITLE_LEN);
    memcpy(out, src, n);
    out[STATE_TITLE_LEN] = '\0';
}

// Returns STATE_RECORD_LEN on success, 0 if something made the formatted length come out different...
// More: docs/code-comments.md (plugin/include/state_file.h:53)
static inline size_t state_format(char *out, size_t cap, int foreground, uint32_t seq, const char *titleId)
{
    char title[STATE_TITLE_LEN + 1];
    state_title_fixed(title, titleId);
    int n = snprintf(out, cap, "v1 fg=%d seq=%06u title=%s\n",
                      foreground ? 1 : 0, (unsigned)(seq % 1000000u), title);
    return (n == STATE_RECORD_LEN) ? (size_t)n : 0;
}

#endif
