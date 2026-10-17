#ifndef PLUGIN_COMMON_H
#define PLUGIN_COMMON_H

// config.c (copied verbatim from ps4_ambient_light's plugin source) expects this header to provide...
// More: docs/code-comments.md (companion-app/include/plugin_common.h:4)
#include <stdio.h>
#ifndef __FINAL__
#define debug_printf(a, args...) printf("[config] " a, ##args)
#else
#define debug_printf(a, args...)
#endif

// Orbis open(2) flags are BSD, not Linux: O_WRONLY=0x1, O_CREAT=0x200, O_TRUNC=0x400.
// Create-or-overwrite must include 0x400, or a shorter rewrite keeps the old tail bytes. [docs/code-comments.md]
#define ORBIS_O_WRONLY 0x0001
#define ORBIS_O_CREAT  0x0200
#define ORBIS_O_TRUNC  0x0400
#define ORBIS_O_CREAT_TRUNC_WRONLY (ORBIS_O_CREAT | ORBIS_O_TRUNC | ORBIS_O_WRONLY)

#endif // PLUGIN_COMMON_H
