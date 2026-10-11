// Run from companion-app/:
//   gcc -Wall -Wextra -Iinclude -I../plugin/include -o /tmp/test_claim_file tests/test_claim_file.c && /tmp/test_claim_file
// The claim record against the plugin's REAL state_format(), and the heartbeat logic on a clock.
#include "claim_file.h"
#include "state_file.h"     // plugin/include -- the other writer of the same file
#include <assert.h>
#include <stdio.h>

typedef struct { int writes; char last[64]; size_t lastLen; int failNext; char all[32][64]; } Sink;

static int sink_write(void *ctx, const char *text, size_t len)
{
    Sink *s = (Sink *)ctx;
    if (s->failNext > 0) { s->failNext--; return -1; }
    assert(len == CLAIM_RECORD_LEN);
    memcpy(s->last, text, len); s->last[len] = 0; s->lastLen = len;
    if (s->writes < 32) memcpy(s->all[s->writes], s->last, len + 1);
    s->writes++;
    return 0;
}

int main(void)
{
    // ---- format: identical to the plugin's, apart from the title ----
    for (int fg = 0; fg <= 1; fg++)
        for (uint32_t seq = 0; seq < 1200000; seq += 7919) {
            char mine[64], theirs[64];
            assert(claim_format(mine, sizeof(mine), fg, seq) == CLAIM_RECORD_LEN);
            assert(state_format(theirs, sizeof(theirs), fg, seq, CLAIM_TITLE) == STATE_RECORD_LEN);
            assert(CLAIM_RECORD_LEN == STATE_RECORD_LEN);
            assert(strcmp(mine, theirs) == 0);
        }
    assert(strlen(CLAIM_TITLE) == STATE_TITLE_LEN);
    assert(strcmp(CLAIM_PATH, STATE_PATH) == 0);
    assert(strcmp(APP_DATA_DIR, AMBILIGHT_DATA_DIR) == 0);   // the app's copy of the folder matches the plugin's
    printf("format matches plugin state_format(): ok\n");

    // ---- parse: reads what the plugin writes, rejects junk ----
    char rec[64]; uint32_t v = 99;
    state_format(rec, sizeof(rec), 1, 123, "CUSA00000");
    assert(claim_parse_seq(rec, STATE_RECORD_LEN, &v) && v == 123);
    state_format(rec, sizeof(rec), 0, 999999, "CUSA00000");
    assert(claim_parse_seq(rec, STATE_RECORD_LEN, &v) && v == 999999);
    assert(!claim_parse_seq("", 0, &v));
    assert(!claim_parse_seq("v2 fg=1 seq=000123 title=CUSA00000\n", 35, &v));
    assert(!claim_parse_seq("v1 fg=1 seq=00x123 title=CUSA00000\n", 35, &v));
    assert(!claim_parse_seq("v1 fg=2 seq=000123 title=CUSA00000\n", 35, &v));
    assert(!claim_parse_seq("v1 fg=1 seq=0001", 16, &v));
    printf("parse: ok\n");

    // ---- timeline: start sending, heartbeat, stop ----
    ClaimRun rs; Sink sk; memset(&sk, 0, sizeof(sk));
    claim_init(&rs, 41);                                   // a file with seq=41 was already on disk
    uint32_t t = 10000;
    assert(claim_tick(&rs, t, false, sink_write, &sk) == 0 && sk.writes == 0);   // idle: silent, no file touched
    assert(claim_tick(&rs, t, true,  sink_write, &sk) == 1);                      // first frame sending: fg=1 NOW
    assert(strncmp(sk.last, "v1 fg=1 seq=000042 title=COMPANION", 33) == 0);      // seq changed vs what a reader saw (41)
    for (int i = 1; i <= 29; i++) assert(claim_tick(&rs, t + i * 33, true, sink_write, &sk) == 0);  // ~1 s of frames: no writes
    assert(sk.writes == 1);
    assert(claim_tick(&rs, t + 1000, true, sink_write, &sk) == 1);                // heartbeat
    assert(strncmp(sk.last, "v1 fg=1 seq=000043", 18) == 0);
    assert(claim_tick(&rs, t + 2000, true, sink_write, &sk) == 1);
    assert(strncmp(sk.last, "v1 fg=1 seq=000044", 18) == 0);
    assert(claim_tick(&rs, t + 2033, false, sink_write, &sk) == 1);               // stop: fg=0 once, same seq
    assert(strncmp(sk.last, "v1 fg=0 seq=000044", 18) == 0);
    assert(claim_tick(&rs, t + 2066, false, sink_write, &sk) == 0);               // ...and then quiet
    assert(claim_tick(&rs, t + 9000, false, sink_write, &sk) == 0);
    assert(sk.writes == 4);   // fg=1 (42), heartbeats (43, 44), fg=0
    // a second stint continues the numbering, never repeats a seq a reader could have seen
    assert(claim_tick(&rs, t + 9100, true, sink_write, &sk) == 1);
    assert(strncmp(sk.last, "v1 fg=1 seq=000045", 18) == 0);
    printf("timeline: ok\n");

    // ---- no file on disk yet: first record is seq 1, not 0 ----
    memset(&sk, 0, sizeof(sk)); claim_init(&rs, 0);
    assert(claim_tick(&rs, 0, true, sink_write, &sk) == 1);
    assert(strncmp(sk.last, "v1 fg=1 seq=000001", 18) == 0);

    // ---- seq wraps at 1,000,000 like the plugin's ----
    memset(&sk, 0, sizeof(sk)); claim_init(&rs, 999999);
    assert(claim_tick(&rs, 0, true, sink_write, &sk) == 1);
    assert(strncmp(sk.last, "v1 fg=1 seq=000000", 18) == 0);
    printf("start/wrap: ok\n");

    // ---- a failed write doesn't use up a seq, and retries are throttled, not every frame ----
    memset(&sk, 0, sizeof(sk)); claim_init(&rs, 10); sk.failNext = 2;
    assert(claim_tick(&rs, 1000, true, sink_write, &sk) == 0);                    // fails
    assert(claim_tick(&rs, 1033, true, sink_write, &sk) == 0 && sk.failNext == 1); // throttled: didn't even try
    assert(claim_tick(&rs, 1250, true, sink_write, &sk) == 0 && sk.failNext == 0); // retry, fails again
    assert(claim_tick(&rs, 1500, true, sink_write, &sk) == 1);                    // succeeds
    assert(strncmp(sk.last, "v1 fg=1 seq=000011", 18) == 0);                      // 10 -> 11, nothing skipped
    // a failed fg=0 is retried until it lands (a reader must not be left with a stale fg=1 if avoidable)
    sk.failNext = 1;
    assert(claim_tick(&rs, 1600, false, sink_write, &sk) == 0);
    assert(claim_tick(&rs, 1700, false, sink_write, &sk) == 0);                   // inside the throttle window
    assert(claim_tick(&rs, 1850, false, sink_write, &sk) == 1);
    assert(strncmp(sk.last, "v1 fg=0 seq=000011", 18) == 0);
    printf("write failures: ok\n");

    // ---- millisecond clock wrap doesn't break the heartbeat ----
    memset(&sk, 0, sizeof(sk)); claim_init(&rs, 0);
    uint32_t near = 0xFFFFFFFFu - 500;
    assert(claim_tick(&rs, near, true, sink_write, &sk) == 1);
    assert(claim_tick(&rs, near + 400, true, sink_write, &sk) == 0);
    assert(claim_tick(&rs, near + 1000, true, sink_write, &sk) == 1);             // crossed the wrap
    printf("clock wrap: ok\nALL PASSED\n");
    return 0;
}
