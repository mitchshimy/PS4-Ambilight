// The scan that finds one asset's API URL in GitHub's "latest release" JSON, split out of main.c so a...
// More: docs/code-comments.md (companion-app/include/github_release_json.h:1)
#ifndef GITHUB_RELEASE_JSON_H
#define GITHUB_RELEASE_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Hand-rolled scan for one field, NOT a general JSON parser -- there's no JSON library linked into...
// More: docs/code-comments.md (companion-app/include/github_release_json.h:12)
static inline bool github_json_find_asset_url(const char *releaseJson, const char *assetName,
                                        char *outUrl, size_t outUrlSize)
{
    char needle[160];
    int nlen = snprintf(needle, sizeof(needle), "\"name\":\"%s\"", assetName);
    if (nlen <= 0 || (size_t)nlen >= sizeof(needle)) return false;

    const char *namePos = NULL;
    // GitHub's JSON isn't guaranteed to omit the space after ':' --
    // try both the compact and the spaced form at each candidate spot.
    char needleSpaced[160];
    snprintf(needleSpaced, sizeof(needleSpaced), "\"name\": \"%s\"", assetName);

    // needle/needleSpaced both end in the closing '"' right after assetName, so a match against either...
    // More: docs/code-comments.md (companion-app/include/github_release_json.h:40)
    const char *m1 = strstr(releaseJson, needle);
    const char *m2 = strstr(releaseJson, needleSpaced);
    namePos = m1 && (!m2 || m1 < m2) ? m1 : m2;
    if (!namePos) return false;

    // The asset object's "url" field comes before "name" in GitHub's schema, so walk backward from namePos...
    // More: docs/code-comments.md (companion-app/include/github_release_json.h:50)
    const char *objStart = namePos;
    int depth = 0;
    while (objStart > releaseJson) {
        objStart--;
        if (*objStart == '}') depth++;
        else if (*objStart == '{') {
            if (depth == 0) break;
            depth--;
        }
    }
    if (*objStart != '{') return false;

    const char *urlKey = strstr(objStart, "\"url\":\"");
    size_t urlKeyLen = 7;
    if (!urlKey || urlKey > namePos) {
        urlKey = strstr(objStart, "\"url\": \"");
        urlKeyLen = 8;
        if (!urlKey || urlKey > namePos) return false;
    }
    const char *valueStart = urlKey + urlKeyLen;
    const char *valueEnd = strchr(valueStart, '"');
    if (!valueEnd) return false;
    size_t valueLen = (size_t)(valueEnd - valueStart);
    if (valueLen == 0 || valueLen >= outUrlSize) return false;
    memcpy(outUrl, valueStart, valueLen);
    outUrl[valueLen] = '\0';
    return true;
}

#endif
