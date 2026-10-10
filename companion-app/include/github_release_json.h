// The scan that finds one asset's API URL in GitHub's "latest release" JSON, split out of main.c so
// a PC test can run it (tests/test_github_release_json.c). Used by net_http.c. The code is as it
// ran on hardware; only "static" became "static inline".
#ifndef GITHUB_RELEASE_JSON_H
#define GITHUB_RELEASE_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Hand-rolled scan for one field, NOT a general JSON parser -- there's
// no JSON library linked into this app, and GitHub's release-asset
// response shape (each asset object listing "url" as its first field,
// well before "name") is stable enough that this is a reasonable,
// low-risk way to pull one string out of it rather than pulling in a
// dependency for one lookup. It only has to survive being handed
// GitHub's own JSON, not arbitrary/hostile input.
//
// Finds the asset object in releaseJson whose "name" field equals
// assetName exactly (not e.g. a prefix match against
// "ps4_ambient_light.prx.sha256" when looking for
// "ps4_ambient_light.prx"), and copies that object's "url" field
// (the asset's own small api.github.com URL -- NOT
// "browser_download_url", which just redirects through github.com's
// web app again, the exact thing this whole fix avoids) into outUrl.
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

    // needle/needleSpaced both end in the closing '"' right after
    // assetName, so a match against either already rules out matching
    // "ps4_ambient_light.prx" as a prefix inside
    // "ps4_ambient_light.prx.sha256" -- no separate boundary check
    // needed.
    const char *m1 = strstr(releaseJson, needle);
    const char *m2 = strstr(releaseJson, needleSpaced);
    namePos = m1 && (!m2 || m1 < m2) ? m1 : m2;
    if (!namePos) return false;

    // The asset object's "url" field comes before "name" in GitHub's
    // schema, so walk backward from namePos to this asset's opening
    // '{' (the nearest unmatched '{' scanning left), then search
    // forward from there (not from the start of the whole response)
    // so we can't accidentally grab a different asset's "url".
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
