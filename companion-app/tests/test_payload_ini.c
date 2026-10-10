// Run from companion-app/: gcc -Wall -Wextra -Iinclude -o /tmp/test_payload_ini tests/test_payload_ini.c && /tmp/test_payload_ini
// The only thing that edits GoldHEN's payload AutoRun list: other entries, comments and
// line endings must come through byte for byte.
#include "payload_ini.h"
#include <assert.h>
#include <stdio.h>

#define FN    "ps4-ambilight-900.bin"
#define ENTRY "/user/data/payloads/" FN

static AutoloadState st(const char *t) { return payload_autoload_state(t, strlen(t), FN); }

// Applies and returns a malloc'd string; checks `changed` agrees with a plain compare.
static char *ap(const char *t, bool on, bool *changedOut)
{
    size_t n; bool ch;
    char *r = payload_autoload_apply(t, strlen(t), ENTRY, FN, on, &n, &ch);
    assert(r && n == strlen(r));
    assert(ch == (strcmp(r, t) != 0));
    if (changedOut) *changedOut = ch;
    return r;
}
#define EXPECT(got, want) do { if (strcmp((got), (want)) != 0) { printf("--- got ---\n%s\n--- want ---\n%s\n", (got), (want)); assert(!"mismatch"); } } while (0)

int main(void)
{
    // ---- state ----
    assert(st("") == AUTOLOAD_ABSENT);
    assert(st("[AutoRun]\n") == AUTOLOAD_ABSENT);
    assert(st("[AutoRun]\n" ENTRY " = 1\n") == AUTOLOAD_ON);
    assert(st("[AutoRun]\n" ENTRY " = 0\n") == AUTOLOAD_OFF);
    assert(st("[AutoRun]\n" ENTRY "=1\n") == AUTOLOAD_ON);                         // no spaces
    assert(st("[AutoRun]\n" ENTRY "   =   1   \n") == AUTOLOAD_ON);
    assert(st("[AutoRun]\r\n" ENTRY " = 1\r\n") == AUTOLOAD_ON);                   // CRLF
    assert(st("[AutoRun]\n" ENTRY " = 1") == AUTOLOAD_ON);                         // no trailing newline
    assert(st("[autorun]\n" ENTRY " = 1\n") == AUTOLOAD_ON);                       // section name case
    assert(st("[AutoRun]\n/data/payloads/" FN " = 1\n") == AUTOLOAD_ON);           // other spelling of the path
    assert(st("[AutoRun]\n" ENTRY " = 1 ; note\n") == AUTOLOAD_ON);
    assert(st("[AutoRun]\n;" ENTRY " = 1\n") == AUTOLOAD_ABSENT);                  // commented out is not an entry
    assert(st("[AutoRun]\n#" ENTRY " = 1\n") == AUTOLOAD_ABSENT);
    assert(st("[AutoRun]\n/user/data/payloads/other.bin = 1\n") == AUTOLOAD_ABSENT);
    assert(st("[AutoRun]\n/user/data/payloads/x" FN " = 1\n") == AUTOLOAD_ABSENT); // a longer name that merely ends the same
    assert(st("[AutoRun]\n/user/data/payloads/" FN ".bak = 1\n") == AUTOLOAD_ABSENT);
    assert(st("[Other]\n" ENTRY " = 1\n") == AUTOLOAD_ABSENT);                     // right line, wrong section
    assert(st("[AutoRun]\n" ENTRY " = 2\n") == AUTOLOAD_OFF);                      // only exactly 1 means on
    assert(st("[AutoRun]\n" ENTRY " = true\n") == AUTOLOAD_OFF);
    assert(st("[AutoRun]\n" ENTRY "\n") == AUTOLOAD_ABSENT);                       // no value: not an entry we understand
    assert(st("[AutoRun]\na.bin = 1\n" ENTRY " = 0\nb.bin = 1\n") == AUTOLOAD_OFF);
    printf("state: ok\n");

    // ---- turning ON when it isn't there ----
    bool ch; char *r;
    r = ap("", true, &ch);                      assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY " = 1\n"); free(r);
    r = ap("[AutoRun]\n", true, &ch);           assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY " = 1\n"); free(r);
    r = ap("[AutoRun]", true, &ch);             assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY " = 1\n"); free(r);  // header is the last line, no newline
    // other people's entries and comments are kept, ours goes after the last one
    r = ap("; my payloads\n[AutoRun]\n/user/data/payloads/a.bin = 1\n/user/data/payloads/b.bin = 0\n", true, &ch);
    assert(ch); EXPECT(r, "; my payloads\n[AutoRun]\n/user/data/payloads/a.bin = 1\n/user/data/payloads/b.bin = 0\n" ENTRY " = 1\n"); free(r);
    // another section after AutoRun stays after it, and blank lines between stay where they were
    r = ap("[AutoRun]\n/user/data/payloads/a.bin = 1\n\n[Other]\nx = y\n", true, &ch);
    assert(ch); EXPECT(r, "[AutoRun]\n/user/data/payloads/a.bin = 1\n" ENTRY " = 1\n\n[Other]\nx = y\n"); free(r);
    // AutoRun with no entries, then another section
    r = ap("[AutoRun]\n\n[Other]\nx = y\n", true, &ch);
    assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY " = 1\n\n[Other]\nx = y\n"); free(r);
    // AutoRun is the last section and the file has no trailing newline
    r = ap("[AutoRun]\n/user/data/payloads/a.bin = 1", true, &ch);
    assert(ch); EXPECT(r, "[AutoRun]\n/user/data/payloads/a.bin = 1\n" ENTRY " = 1\n"); free(r);
    // no AutoRun section at all: it is created at the end, everything else untouched
    r = ap("[Settings]\nfoo = bar\n", true, &ch);
    assert(ch); EXPECT(r, "[Settings]\nfoo = bar\n\n[AutoRun]\n" ENTRY " = 1\n"); free(r);
    r = ap("[Settings]\nfoo = bar", true, &ch);
    assert(ch); EXPECT(r, "[Settings]\nfoo = bar\n\n[AutoRun]\n" ENTRY " = 1\n"); free(r);
    // a commented-out copy of our line is left alone and a real one is added
    r = ap("[AutoRun]\n;" ENTRY " = 1\n", true, &ch);
    assert(ch); EXPECT(r, "[AutoRun]\n;" ENTRY " = 1\n" ENTRY " = 1\n"); free(r);
    // CRLF files get CRLF lines
    r = ap("[AutoRun]\r\n/user/data/payloads/a.bin = 1\r\n", true, &ch);
    assert(ch); EXPECT(r, "[AutoRun]\r\n/user/data/payloads/a.bin = 1\r\n" ENTRY " = 1\r\n"); free(r);
    printf("add: ok\n");

    // ---- flipping an existing entry changes the value and nothing else ----
    r = ap("[AutoRun]\n" ENTRY " = 1\n", false, &ch);   assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY " = 0\n"); free(r);
    r = ap("[AutoRun]\n" ENTRY " = 0\n", true, &ch);    assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY " = 1\n"); free(r);
    r = ap("[AutoRun]\n" ENTRY "=1\n", false, &ch);     assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY "=0\n"); free(r);
    r = ap("[AutoRun]\n" ENTRY "   =   1   \n", false, &ch); assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY "   =   0   \n"); free(r);
    r = ap("[AutoRun]\r\n" ENTRY " = 1\r\n", false, &ch); assert(ch); EXPECT(r, "[AutoRun]\r\n" ENTRY " = 0\r\n"); free(r);
    r = ap("[AutoRun]\n" ENTRY " = 1 ; keep me\n", false, &ch); assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY " = 0 ; keep me\n"); free(r);
    r = ap("[AutoRun]\n" ENTRY " = 1", false, &ch);     assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY " = 0"); free(r);   // EOF without newline
    // the neighbours are untouched
    r = ap("; top\n[AutoRun]\n/user/data/payloads/a.bin = 1\n" ENTRY " = 1\n/user/data/payloads/b.bin = 1\n\n[Z]\nq = 1\n", false, &ch);
    assert(ch); EXPECT(r, "; top\n[AutoRun]\n/user/data/payloads/a.bin = 1\n" ENTRY " = 0\n/user/data/payloads/b.bin = 1\n\n[Z]\nq = 1\n"); free(r);
    // the other spelling of the path is edited in place, NOT duplicated
    r = ap("[AutoRun]\n/data/payloads/" FN " = 1\n", false, &ch);
    assert(ch); EXPECT(r, "[AutoRun]\n/data/payloads/" FN " = 0\n"); free(r);
    r = ap("[AutoRun]\n/data/payloads/" FN " = 0\n", true, &ch);
    assert(ch); EXPECT(r, "[AutoRun]\n/data/payloads/" FN " = 1\n"); free(r);
    // duplicates are all kept in step
    r = ap("[AutoRun]\n" ENTRY " = 1\n/data/payloads/" FN " = 1\n", false, &ch);
    assert(ch); EXPECT(r, "[AutoRun]\n" ENTRY " = 0\n/data/payloads/" FN " = 0\n"); free(r);
    printf("flip: ok\n");

    // ---- no-ops report no change ----
    const char *same[] = { "[AutoRun]\n" ENTRY " = 1\n", "[AutoRun]\n" ENTRY " = 0\n" };
    r = ap(same[0], true, &ch);  assert(!ch); free(r);
    r = ap(same[1], false, &ch); assert(!ch); free(r);
    r = ap("", false, &ch);      assert(!ch); EXPECT(r, ""); free(r);                         // nothing to turn off
    r = ap("[AutoRun]\n/user/data/payloads/a.bin = 1\n", false, &ch); assert(!ch); free(r);   // absent + off: leave the file alone
    r = ap("[Settings]\nfoo = bar\n", false, &ch); assert(!ch); free(r);
    printf("no-ops: ok\n");

    // ---- round trips: on then off then on, and state agrees with what was applied ----
    const char *starts[] = { "", "[AutoRun]\n", "[AutoRun]\n/user/data/payloads/a.bin = 1\n", "[Settings]\nfoo = bar\n",
                              "[AutoRun]\r\n/user/data/payloads/a.bin = 1\r\n\r\n[Z]\r\nq = 1\r\n" };
    for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); i++) {
        char *a = ap(starts[i], true, NULL);   assert(st(a) == AUTOLOAD_ON);
        char *b = ap(a, false, NULL);          assert(st(b) == AUTOLOAD_OFF);
        char *c = ap(b, true, NULL);           assert(st(c) == AUTOLOAD_ON);
        assert(strcmp(a, c) == 0);                                                          // flipping back restores the same text
        char *d = ap(c, true, &ch);            assert(!ch);                                 // idempotent
        // every other line of the original survives, in order
        const char *p = starts[i];
        while (*p) {
            const char *e = strchr(p, '\n'); size_t n = e ? (size_t)(e - p) + 1 : strlen(p);
            char line[256]; memcpy(line, p, n); line[n] = 0;
            char *pos = strstr(c, line); (void)pos;
            assert(pos);
            p += n;
        }
        free(a); free(b); free(c); free(d);
    }
    printf("round trips: ok\nALL PASSED\n");
    return 0;
}
