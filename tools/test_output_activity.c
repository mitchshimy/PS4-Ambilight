// Host test for the exact output-counter helper used by network.c.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "output_activity.h"

int main(void)
{
    volatile uint32_t count = 0;

    assert(wled_output_note_send(&count, 0, 10) == 0);
    assert(wled_output_note_send(&count, -1, 10) == 0);
    assert(wled_output_note_send(&count, 9, 10) == 0);
    assert(count == 0); // failed/short sends must not keep the game claim alive

    assert(wled_output_note_send(&count, 10, 10) == 1);
    assert(count == 1);
    assert(wled_output_note_send(&count, 10, 10) == 1);
    assert(count == 2);

    puts("only complete successful sendto results advance the output counter: PASSED");
    return 0;
}
