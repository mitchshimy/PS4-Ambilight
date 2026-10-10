// payload_ini.h -- read and flip ONE entry in GoldHEN's payload AutoRun list, touching nothing else.
//
//   [AutoRun]
//   /user/data/payloads/ps4-ambilight-900.bin = 1
//
// "= 1" runs the payload when GoldHEN loads, "= 0" keeps the entry but doesn't run it
// (confirmed on a console). The file belongs to GoldHEN and is shared with every other
// payload the person has, so this is text surgery, not the app's ini_table: that generic
// parser drops comments and would rewrite lines it doesn't understand (see the long comment
// above ensure_plugin_registered_in_goldhen() in main.c, which hit exactly that on plugins.ini).
// Everything outside the one line this changes is copied through byte for byte.
//
// Entries are matched by FILE NAME, not by the whole path, because the same file shows up as
// /data/payloads/x.bin or /user/data/payloads/x.bin depending on who added the line.
//
// Header-only and free of console calls so tests/test_payload_ini.c can run it on a PC.
#ifndef PAYLOAD_INI_H
#define PAYLOAD_INI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    AUTOLOAD_ABSENT = 0,  // no active [AutoRun] entry for the file (a commented-out one doesn't count)
    AUTOLOAD_ON,          // entry present, value 1
    AUTOLOAD_OFF          // entry present, any other value (0 is what we write)
} AutoloadState;

static inline bool pi_is_blank(char c) { return c == ' ' || c == '\t' || c == '\r'; }

// Trims blanks off both ends of [*s, *e).
static inline void pi_trim(const char **s, const char **e)
{
    while (*s < *e && pi_is_blank(**s)) (*s)++;
    while (*e > *s && pi_is_blank((*e)[-1])) (*e)--;
}

static inline bool pi_ieq(const char *a, size_t alen, const char *lit)
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

// If [ls, le) (one line, already without '\n') is a "[Section]" header, sets *isAuto and
// returns true.
static inline bool pi_section_header(const char *ls, const char *le, bool *isAuto)
{
    pi_trim(&ls, &le);
    if (le - ls < 2 || *ls != '[' || le[-1] != ']') return false;
    const char *ns = ls + 1, *ne = le - 1;
    pi_trim(&ns, &ne);
    *isAuto = pi_ieq(ns, (size_t)(ne - ns), "AutoRun");
    return true;
}

// If the line is an active "path = value" entry whose file name is `fileName`, returns true
// and gives the value token's span. `lineStart`/`lineEnd` are the raw line (no '\n').
static inline bool pi_entry_matches(const char *lineStart, const char *lineEnd, const char *fileName,
                                    const char **valStart, const char **valEnd)
{
    const char *ls = lineStart, *le = lineEnd;
    pi_trim(&ls, &le);
    if (ls >= le || *ls == ';' || *ls == '#') return false;       // blank or commented out

    const char *eq = (const char *)memchr(ls, '=', (size_t)(le - ls));
    if (!eq) return false;                                          // no value: not an entry we understand

    const char *ps = ls, *pe = eq;
    pi_trim(&ps, &pe);
    const char *base = ps;
    for (const char *q = ps; q < pe; q++) if (*q == '/') base = q + 1;
    size_t nlen = strlen(fileName);
    if ((size_t)(pe - base) != nlen || memcmp(base, fileName, nlen) != 0) return false;

    // value token: from after '=' (skipping blanks) up to the next blank or ';'
    const char *vs = eq + 1;
    while (vs < lineEnd && pi_is_blank(*vs)) vs++;
    const char *ve = vs;
    while (ve < lineEnd && !pi_is_blank(*ve) && *ve != ';') ve++;
    *valStart = vs; *valEnd = ve;
    return true;
}

static inline AutoloadState payload_autoload_state(const char *buf, size_t len, const char *fileName)
{
    bool inAuto = false;
    const char *p = buf, *end = buf + len;
    while (p < end) {
        const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
        const char *le = nl ? nl : end;
        bool isAuto;
        if (pi_section_header(p, le, &isAuto)) {
            inAuto = isAuto;
        } else if (inAuto) {
            const char *vs, *ve;
            if (pi_entry_matches(p, le, fileName, &vs, &ve))
                return (ve > vs && *vs == '1' && ve - vs == 1) ? AUTOLOAD_ON : AUTOLOAD_OFF;
        }
        p = nl ? nl + 1 : end;
    }
    return AUTOLOAD_ABSENT;
}

// ---- writer ----

typedef struct { char *p; size_t n, cap; bool oom; } PiBuf;

static inline void pi_put(PiBuf *b, const char *s, size_t n)
{
    if (b->oom || n == 0) return;
    if (b->n + n + 1 > b->cap) {
        size_t nc = (b->cap ? b->cap : 256);
        while (nc < b->n + n + 1) nc *= 2;
        char *g = (char *)realloc(b->p, nc);
        if (!g) { b->oom = true; return; }
        b->p = g; b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

// Returns a malloc'd, NUL-terminated copy of the file with the entry for `fileName` set to
// on (1) or off (0); the caller frees it. *outLen is its length, *changed whether it differs
// from the input.
//  * An existing entry (every one, if there are duplicates) has only its value rewritten --
//    path text, spacing and any trailing comment stay as they were.
//  * No entry and on=true: `entryPath = 1` is added at the end of [AutoRun] (the section is
//    created at the end of the file if there is none).
//  * No entry and on=false: nothing to turn off, the text is returned unchanged.
//  * Line endings of the file (LF or CRLF) are kept for inserted lines.
// Returns NULL only if out of memory.
static inline char *payload_autoload_apply(const char *buf, size_t len, const char *entryPath,
                                           const char *fileName, bool on, size_t *outLen, bool *changed)
{
    const char *eol = "\n";
    {
        const char *nl = (const char *)memchr(buf, '\n', len);
        if (nl && nl > buf && nl[-1] == '\r') eol = "\r\n";
    }
    const char digit = on ? '1' : '0';

    // Pass 1: is there a matching entry, and where would a new line go?
    bool anyMatch = false, sawAuto = false;
    size_t insertAt = len;          // input offset to insert before; len means "at the very end"
    {
        bool inAuto = false;
        const char *p = buf, *end = buf + len;
        size_t lastBodyEnd = 0; bool haveBody = false;
        while (p < end) {
            const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
            const char *le = nl ? nl : end;
            const char *next = nl ? nl + 1 : end;
            bool isAuto;
            if (pi_section_header(p, le, &isAuto)) {
                if (inAuto && !isAuto) { /* leaving AutoRun: keep lastBodyEnd as is */ }
                inAuto = isAuto;
                if (isAuto) { sawAuto = true; lastBodyEnd = (size_t)(next - buf); haveBody = true; }
            } else if (inAuto) {
                const char *s = p, *e = le;
                pi_trim(&s, &e);
                if (s < e) lastBodyEnd = (size_t)(next - buf);        // last non-blank line of the section
                const char *vs, *ve;
                if (pi_entry_matches(p, le, fileName, &vs, &ve)) anyMatch = true;
            }
            p = next;
        }
        if (sawAuto && haveBody) insertAt = lastBodyEnd;
    }

    PiBuf out = { NULL, 0, 0, false };
    pi_put(&out, "", 0);
    if (!out.p && !out.oom) { out.p = (char *)malloc(1); if (out.p) { out.p[0] = '\0'; out.cap = 1; } else out.oom = true; }

    bool needInsert = (!anyMatch && on);
    bool inserted = false;
    {
        bool inAuto = false;
        const char *p = buf, *end = buf + len;
        while (p < end) {
            if (needInsert && !inserted && sawAuto && (size_t)(p - buf) == insertAt) {
                pi_put(&out, entryPath, strlen(entryPath));
                pi_put(&out, " = 1", 4);
                pi_put(&out, eol, strlen(eol));
                inserted = true;
            }
            const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
            const char *le = nl ? nl : end;
            const char *next = nl ? nl + 1 : end;
            bool isAuto;
            if (pi_section_header(p, le, &isAuto)) {
                inAuto = isAuto;
                pi_put(&out, p, (size_t)(next - p));
            } else if (inAuto) {
                const char *vs, *ve;
                if (pi_entry_matches(p, le, fileName, &vs, &ve)) {
                    pi_put(&out, p, (size_t)(vs - p));
                    pi_put(&out, &digit, 1);
                    pi_put(&out, ve, (size_t)(next - ve));
                } else {
                    pi_put(&out, p, (size_t)(next - p));
                }
            } else {
                pi_put(&out, p, (size_t)(next - p));
            }
            p = next;
        }

        if (needInsert && !inserted) {
            // Insert point is the end of the file: either an [AutoRun] section whose body runs
            // to EOF, or no [AutoRun] section at all.
            if (len > 0 && buf[len - 1] != '\n') pi_put(&out, eol, strlen(eol));
            if (!sawAuto) {
                if (len > 0) pi_put(&out, eol, strlen(eol));            // blank line before a new section
                pi_put(&out, "[AutoRun]", 9);
                pi_put(&out, eol, strlen(eol));
            }
            pi_put(&out, entryPath, strlen(entryPath));
            pi_put(&out, " = 1", 4);
            pi_put(&out, eol, strlen(eol));
        }
    }

    if (out.oom) { free(out.p); return NULL; }
    *outLen = out.n;
    *changed = !(out.n == len && (len == 0 || memcmp(out.p, buf, len) == 0));
    return out.p;
}

#endif // PAYLOAD_INI_H
