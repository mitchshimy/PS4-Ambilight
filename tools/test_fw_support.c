// Run from the repo root:
//   gcc -Wall -Wextra -Icommon -o /tmp/test_fw_support tools/test_fw_support.c && /tmp/test_fw_support
#include "fw_support.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    int maj = -1, min = -1;

    // BCD decode: the low 16 bits (build/patch) are ignored.
    assert(fw_decode(0x09000000u, &maj, &min) && maj == 9 && min == 0);
    assert(fw_decode(0x12500031u, &maj, &min) && maj == 12 && min == 50);
    assert(fw_decode(0x07550001u, &maj, &min) && maj == 7 && min == 55);
    assert(fw_decode(0x14000000u, &maj, &min) && maj == 14 && min == 0);
    // Not BCD / not a version: refused, never guessed at.
    assert(!fw_decode(0, &maj, &min));
    assert(!fw_decode(0x0A000000u, &maj, &min));   // 0xA nibble
    assert(!fw_decode(0x090F0000u, &maj, &min));
    assert(!fw_decode(0xFFFFFFFFu, &maj, &min));

    // Support table, one from each group plus the gaps between them.
    assert(fw_classify(5, 5) == FW_SUPPORTED);
    assert(fw_classify(9, 0) == FW_SUPPORTED);
    assert(fw_classify(12, 2) == FW_SUPPORTED);
    assert(fw_classify(12, 50) == FW_PARTIAL);
    assert(fw_classify(13, 4) == FW_PARTIAL);
    assert(fw_classify(14, 0) == FW_PARTIAL);
    assert(fw_classify(6, 0) == FW_UNSUPPORTED);    // between supported versions
    assert(fw_classify(9, 20) == FW_UNSUPPORTED);
    assert(fw_classify(4, 74) == FW_UNSUPPORTED);
    assert(fw_classify(14, 2) == FW_UNSUPPORTED);
    assert(fw_classify(15, 0) == FW_UNSUPPORTED);

    // Notices: only partial/unsupported speak.
    char t[96], b[128];
    assert(!fw_notice(FW_SUPPORTED, 9, 0, t, sizeof t, b, sizeof b) && !t[0] && !b[0]);
    assert(!fw_notice(FW_UNKNOWN, 0, 0, t, sizeof t, b, sizeof b));
    assert(fw_notice(FW_PARTIAL, 12, 50, t, sizeof t, b, sizeof b));
    assert(strstr(t, "12.50") && strstr(t, "incomplete") && strstr(b, "refuse"));
    assert(fw_notice(FW_UNSUPPORTED, 13, 4, t, sizeof t, b, sizeof b) && strstr(t, "13.04"));
    // Minor is zero-padded, and a tiny buffer truncates instead of overflowing.
    char tiny[8];
    fw_notice(FW_PARTIAL, 12, 50, tiny, sizeof tiny, b, sizeof b);
    assert(strlen(tiny) == 7);

    printf("fw_support: ok\n");
    return 0;
}
