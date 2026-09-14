#ifndef PLUGIN_COMMON_H
#define PLUGIN_COMMON_H

// config.c (copied verbatim from ps4_ambient_light's plugin source)
// expects this header to provide debug_printf. This app is a
// standalone homebrew application, not a GoldHEN plugin, so none of
// the real plugin_common.h's hook/attr_public/GOLDHEN_PATH machinery
// applies here -- only config.c's one dependency is stubbed.
#include <stdio.h>
#ifndef __FINAL__
#define debug_printf(a, args...) printf("[config] " a, ##args)
#else
#define debug_printf(a, args...)
#endif

#endif // PLUGIN_COMMON_H
