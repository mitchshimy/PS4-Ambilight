// goldhen_config.h -- READ one setting from GoldHEN's own config file: [PluginLoader]...
// More: docs/code-comments.md (companion-app/include/goldhen_config.h:1)
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
