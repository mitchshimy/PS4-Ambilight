// Run from the repo root:
//   gcc -Wall -Wextra -Icommon -o /tmp/test_fw_support tools/test_fw_support.c && /tmp/test_fw_support
#include "fw_support.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    int maj = -1, min = -1;

    // BCD decode: the low 16 bits (build/patch) are ignored.
    assert(fw_decode(0x09000000u, &maj, &min) && maj == 9 && min == 0);
    assert(fw_decode(0x12520031u, &maj, &min) && maj == 12 && min == 52);
    assert(fw_decode(0x07550001u, &maj, &min) && maj == 7 && min == 55);
    assert(fw_decode(0x14000000u, &maj, &min) && maj == 14 && min == 0);
    // Not BCD / not a version: refused, never guessed at.
    assert(!fw_decode(0, &maj, &min));
    assert(!fw_decode(0x0A000000u, &maj, &min));
    assert(!fw_decode(0x090F0000u, &maj, &min));
    assert(!fw_decode(0xFFFFFFFFu, &maj, &min));

    // Only end-to-end hardware-tested versions are called confirmed.
    assert(fw_classify(9, 0) == FW_SUPPORTED);
    assert(fw_classify(12, 52) == FW_SUPPORTED);
    // In the loader's table but never run: untested.
    assert(fw_classify(5, 5) == FW_UNTESTED);
    assert(fw_classify(9, 4) == FW_UNTESTED);
    assert(fw_classify(12, 2) == FW_UNTESTED);
    assert(fw_classify(13, 52) == FW_UNTESTED);
    // No table entry: not listed (14.00, and versions between table entries).
    assert(fw_classify(14, 0) == FW_NOT_LISTED);
    assert(fw_classify(12, 50) == FW_NOT_LISTED);
    assert(fw_classify(9, 20) == FW_NOT_LISTED);
    assert(fw_classify(4, 74) == FW_NOT_LISTED);
    assert(fw_classify(15, 0) == FW_NOT_LISTED);

    // Notices: confirmed/unknown say nothing; untested and not-listed each get their own wording.
    char t[96], b[128];
    assert(!fw_notice(FW_SUPPORTED, 9, 0, t, sizeof t, b, sizeof b) && !t[0] && !b[0]);
    assert(!fw_notice(FW_SUPPORTED, 12, 52, t, sizeof t, b, sizeof b) && !t[0] && !b[0]);
    assert(!fw_notice(FW_UNKNOWN, 0, 0, t, sizeof t, b, sizeof b));
    assert(fw_notice(FW_UNTESTED, 9, 4, t, sizeof t, b, sizeof b));
    assert(strstr(t, "9.04") && strstr(t, "not yet verified") && strstr(b, "has an entry") && strstr(b, "9.00 and 12.52"));
    assert(fw_notice(FW_NOT_LISTED, 14, 0, t, sizeof t, b, sizeof b));
    assert(strstr(t, "14.00") && strstr(t, "not in the shell loader") && strstr(b, "unsupported firmware"));
    // Minor is zero-padded, and a tiny buffer truncates instead of overflowing.
    char tiny[8];
    fw_notice(FW_NOT_LISTED, 12, 50, tiny, sizeof tiny, b, sizeof b);
    assert(strlen(tiny) == 7);

    puts("fw_support: ok");
    return 0;
}
