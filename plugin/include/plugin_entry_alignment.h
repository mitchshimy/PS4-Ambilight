// GoldHEN plugin callbacks can be entered through a loader call path whose
// x86-64 stack alignment is not guaranteed. The PS4 libc's vectorized
// printf implementation uses MOVAPS and faults if a nested call inherits
// that misalignment. Keep these exported entry shims tiny and assembly-only:
// save the incoming frame, align RSP to 16 bytes, call the C implementation,
// then restore the exact incoming stack before returning to the loader.
#ifndef AMBIENT_PLUGIN_ENTRY_ALIGNMENT_H
#define AMBIENT_PLUGIN_ENTRY_ALIGNMENT_H

#include <stdint.h>

#define AMBIENT_DEFINE_ALIGNED_ENTRY(name, implementation)                         \
    __attribute__((naked, visibility("default")))                                 \
    int32_t name(int32_t argc __attribute__((unused)),                             \
                 const char *argv[] __attribute__((unused)))                       \
    {                                                                               \
        __asm__ volatile(                                                           \
            "pushq %rbp\n\t"                                                      \
            ".cfi_adjust_cfa_offset 8\n\t"                                       \
            ".cfi_offset %rbp, -16\n\t"                                           \
            "movq %rsp, %rbp\n\t"                                                 \
            ".cfi_def_cfa_register %rbp\n\t"                                     \
            "andq $-16, %rsp\n\t"                                                 \
            "callq " #implementation "\n\t"                                     \
            "movq %rbp, %rsp\n\t"                                                 \
            "popq %rbp\n\t"                                                      \
            ".cfi_def_cfa %rsp, 8\n\t"                                            \
            ".cfi_restore %rbp\n\t"                                              \
            "retq\n\t");                                                        \
    }

#endif
