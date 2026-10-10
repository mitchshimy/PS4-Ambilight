// update_source.h -- cleans up the [dev] update_base_url value (see main.c, dev_update_base_url).
// Header-only and free of console calls so tests/test_shell_settings.c can check it on a PC.
#ifndef UPDATE_SOURCE_H
#define UPDATE_SOURCE_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// "  http://192.168.2.117:8000/ \r" -> "http://192.168.2.117:8000". Accepts only http:// or https://
// URLs (anything else would turn a typo into a request to nowhere), trims blanks and trailing
// slashes, and fails if it doesn't fit. out is "" on failure.
static inline bool update_source_normalize(const char *raw, char *out, size_t outSize)
{
    if (outSize == 0) return false;
    out[0] = '\0';
    if (!raw) return false;
    while (*raw == ' ' || *raw == '\t') raw++;
    size_t n = strlen(raw);
    while (n > 0 && (raw[n - 1] == ' ' || raw[n - 1] == '\t' || raw[n - 1] == '\r' || raw[n - 1] == '\n' || raw[n - 1] == '/')) n--;
    if (n == 0 || n >= outSize) return false;
    bool http  = n > 7 && strncmp(raw, "http://", 7) == 0;
    bool https = n > 8 && strncmp(raw, "https://", 8) == 0;
    if (!http && !https) return false;
    memcpy(out, raw, n);
    out[n] = '\0';
    return true;
}

#endif // UPDATE_SOURCE_H
