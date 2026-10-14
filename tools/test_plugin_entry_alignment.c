// Regression test for the GoldHEN plugin entry stack-alignment thunk.
// A naked caller deliberately calls the exported thunk with RSP%16 == 8
// immediately before CALL (the bad case seen in the PS4 crash context).
// The thunk must realign before it calls the implementation, whose entry
// therefore observes the ABI-required RSP%16 == 8.
#include <stdint.h>
#include <stdio.h>
#include "plugin_entry_alignment.h"

static __attribute__((naked, noinline, used)) int32_t observe_entry_rsp_alignment(
    int32_t argc __attribute__((unused)), const char *argv[] __attribute__((unused)))
{
    __asm__ volatile("movq %rsp, %rax\n\t"
                     "andq $15, %rax\n\t"
                     "retq\n\t");
}

AMBIENT_DEFINE_ALIGNED_ENTRY(test_aligned_entry, observe_entry_rsp_alignment)

__attribute__((naked, noinline)) static int32_t call_with_bad_alignment(void)
{
    __asm__ volatile("callq test_aligned_entry\n\t"
                     "retq\n\t");
}

int main(void)
{
    int32_t observed = call_with_bad_alignment();
    if (observed != 8) {
        fprintf(stderr, "stack alignment regression: implementation entered with RSP%%16=%d, expected 8\n", observed);
        return 1;
    }
    puts("plugin entry stack alignment: PASS (misaligned caller repaired)");
    return 0;
}
