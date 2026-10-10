// goldhen_config.h -- READ one setting from GoldHEN's own config file:
//
//   [PluginLoader]
//   PluginLoader_Enabled = 1        <- "Enable Plugins Loader" in Settings > GoldHEN > Plugin Settings
//
// The file name, section and key come from two real files off a console, one with the switch
// off and one with it on; the value was the only difference between them. GoldHEN writes the
// file when the switch is changed in its menu.
//
// This is READ-ONLY on purpose. An earlier version also wrote the value, and on a console that
// did not work: the file said 1 but GoldHEN's menu still showed the switch off, because GoldHEN
// keeps the live value in memory and never re-reads the file. Worse, the file then disagreed
// with the real state, so Home would have reported the loader on while it was off. GoldHEN also
// refuses to change the switch while a game or app is running, and this app counts as one. So
// the only fix is the person's: close the app, tick the switch in GoldHEN's menu.
//
// Anything unexpected is LOADER_UNKNOWN, which means "don't claim anything": a missing file, a
// GoldHEN version without the key, a value that isn't exactly 0 or 1. The app only ever acts
// on a definite OFF, so an unfamiliar config can never block the plugin's own buttons.
//
// Header-only and free of console calls so tests/test_goldhen_config.c can run it on a PC.
#ifndef GOLDHEN_CONFIG_H
#define GOLDHEN_CONFIG_H

#include "install_plan.h"   // PluginLoaderState
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define GOLDHEN_CFG_SECTION "PluginLoader"
#define GOLDHEN_CFG_KEY     "PluginLoader_Enabled"

static inline bool gc_blank(char c) { return c == ' ' || c == '\t' || c == '\r'; }

static inline void gc_trim(const char **s, const char **e)
{
    while (*s < *e && gc_blank(**s)) (*s)++;
    while (*e > *s && gc_blank((*e)[-1])) (*e)--;
}

static inline bool gc_ieq(const char *a, size_t alen, const char *lit)
{
    size_t n = strlen(lit);
    if (alen != n) return false;
    for (size_t i = 0; i < n; i++) {
        char x = a[i], y = lit[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return false;
    }
    return true;
}

// Finds the value of PluginLoader_Enabled inside [PluginLoader]: its trimmed span [*vs, *ve) as
// byte offsets into buf. False when the section or the key isn't there. The first match wins.
static inline bool gc_find_loader_value(const char *buf, size_t len, size_t *vs, size_t *ve)
{
    bool inSection = false;
    size_t i = 0;
    while (i < len) {
        size_t ls = i;
        while (i < len && buf[i] != '\n') i++;
        size_t le = i;
        if (i < len) i++;                                   // step over the '\n'

        const char *s = buf + ls, *e = buf + le;
        if (ls == 0 && le >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB &&
            (unsigned char)s[2] == 0xBF) s += 3;            // UTF-8 byte-order mark
        gc_trim(&s, &e);
        if (s == e || *s == ';' || *s == '#') continue;

        if (*s == '[' && e[-1] == ']') {                    // a section header
            const char *ns = s + 1, *ne = e - 1;
            gc_trim(&ns, &ne);
            inSection = gc_ieq(ns, (size_t)(ne - ns), GOLDHEN_CFG_SECTION);
            continue;
        }
        if (!inSection) continue;

        const char *eq = (const char *)memchr(s, '=', (size_t)(e - s));
        if (!eq) continue;
        const char *ks = s, *ke = eq;
        gc_trim(&ks, &ke);
        if (!gc_ieq(ks, (size_t)(ke - ks), GOLDHEN_CFG_KEY)) continue;

        const char *v = eq + 1, *ve2 = e;
        gc_trim(&v, &ve2);
        *vs = (size_t)(v - buf);
        *ve = (size_t)(ve2 - buf);
        return true;
    }
    return false;
}

// ON for exactly "1", OFF for exactly "0", UNKNOWN for anything else (including no key at all).
static inline PluginLoaderState goldhen_plugin_loader_state(const char *buf, size_t len)
{
    size_t vs, ve;
    if (!buf || !gc_find_loader_value(buf, len, &vs, &ve)) return LOADER_UNKNOWN;
    if (ve - vs == 1 && buf[vs] == '1') return LOADER_ON;
    if (ve - vs == 1 && buf[vs] == '0') return LOADER_OFF;
    return LOADER_UNKNOWN;
}

#endif // GOLDHEN_CONFIG_H
