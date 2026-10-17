// claim_file.h -- the companion app's half of /data/ps4-ambilight/ps4_ambient_state.
// More: docs/code-comments.md (companion-app/include/claim_file.h:1)
#ifndef CLAIM_FILE_H
#define CLAIM_FILE_H

#include "data_dir.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CLAIM_PATH          APP_DATA_DIR "/ps4_ambient_state"
#define CLAIM_TITLE         "COMPANION"   // exactly 9 characters, like a CUSA title ID
#define CLAIM_RECORD_LEN    35
#define CLAIM_HEARTBEAT_MS  1000u
#define CLAIM_RETRY_MS      250u          // after a failed write, wait this long (not every frame)

// "v1 fg=1 seq=000123 title=COMPANION\n" -- returns CLAIM_RECORD_LEN, or 0 if it came out
// another length (caller then skips the write and retries later). cap >= CLAIM_RECORD_LEN+1.
static inline size_t claim_format(char *out, size_t cap, int foreground, uint32_t seq)
{
    int n = snprintf(out, cap, "v1 fg=%d seq=%06u title=%s\n",
                     foreground ? 1 : 0, (unsigned)(seq % 1000000u), CLAIM_TITLE);
    return (n == CLAIM_RECORD_LEN) ? (size_t)n : 0;
}

// Reads seq out of an existing record (written by anyone), so ours continues from it.
// Returns false for a short, empty or unrecognised record; the caller then starts at 0.
static inline bool claim_parse_seq(const char *buf, size_t len, uint32_t *outSeq)
{
    if (len < 17 || memcmp(buf, "v1 fg=", 6) != 0) return false;
    if ((buf[6] != '0' && buf[6] != '1') || memcmp(buf + 7, " seq=", 5) != 0) return false;
    uint32_t v = 0;
    for (int i = 0; i < 6; i++) {
        char c = buf[12 + i];
        if (c < '0' || c > '9') return false;
        v = v * 10u + (uint32_t)(c - '0');
    }
    *outSeq = v;
    return true;
}

// The caller's file write: returns 0 on success. Real build: sceKernelOpen(CLAIM_PATH,
// O_WRONLY|O_CREAT -- NOT O_TRUNC), write, close. Injected so tests can drive the logic.
typedef int (*ClaimWriteFn)(void *ctx, const char *text, size_t len);

typedef struct {
    uint32_t seq;          // seq of the last record on disk (or of the file we continued from)
    bool     wasFg;        // does the record on disk say fg=1 (as far as we know)?
    uint32_t lastWriteMs;  // when that record was written
    uint32_t retryAtMs;    // earliest time to try again after a failed write
    bool     retryArmed;
} ClaimRun;

static inline void claim_init(ClaimRun *rs, uint32_t startSeq)
{
    memset(rs, 0, sizeof(*rs));
    rs->seq = startSeq % 1000000u;
}

// One look per frame. `driving` is "the app is sending to the strip right now (and Shell
// feed is on)". Returns 1 if it wrote the file this call. Cheap when there's nothing to do.
static inline int claim_tick(ClaimRun *rs, uint32_t nowMs, bool driving, ClaimWriteFn write, void *ctx)
{
    char text[CLAIM_RECORD_LEN + 1];

    if (rs->retryArmed && (int32_t)(nowMs - rs->retryAtMs) < 0) return 0;

    if (driving) {
        if (rs->wasFg && (uint32_t)(nowMs - rs->lastWriteMs) < CLAIM_HEARTBEAT_MS) return 0;
        uint32_t next = (rs->seq + 1u) % 1000000u;
        size_t len = claim_format(text, sizeof(text), 1, next);
        if (len == 0) return 0;
        if (write(ctx, text, len) != 0) { rs->retryArmed = true; rs->retryAtMs = nowMs + CLAIM_RETRY_MS; return 0; }
        rs->retryArmed = false;
        rs->seq = next;               // only consumed once the write really happened
        rs->wasFg = true;
        rs->lastWriteMs = nowMs;
        return 1;
    }

    if (rs->wasFg) {
        size_t len = claim_format(text, sizeof(text), 0, rs->seq);
        if (len == 0) return 0;
        if (write(ctx, text, len) != 0) { rs->retryArmed = true; rs->retryAtMs = nowMs + CLAIM_RETRY_MS; return 0; }
        rs->retryArmed = false;
        rs->wasFg = false;
        rs->lastWriteMs = nowMs;
        return 1;
    }
    return 0;
}

#endif // CLAIM_FILE_H
