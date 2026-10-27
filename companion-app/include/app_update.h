// app_update.h -- is the newest published companion app newer than this one?
// Header-only and free of console calls so tests/test_app_update.c can check it on a PC.
#ifndef APP_UPDATE_H
#define APP_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// The release asset that holds the app's version, a bare "N.NN" line made from app_version.h by CI.
#define APP_VERSION_ASSET_NAME "ps4-ambilight-app.version"
// The package and its checksum as the release publishes them. CI gives the .pkg this fixed name so the app can ask for it.
#define APP_PKG_ASSET_NAME          "ps4-ambilight-app.pkg"
#define APP_PKG_CHECKSUM_ASSET_NAME APP_PKG_ASSET_NAME ".sha256"
// Where the download goes. The console's package installer only lists packages in /data/pkg.
#define APP_PKG_DIR          "/data/pkg"
// The file as saved carries its version, so the installer's list says which one it is: ps4-ambilight-app-1.03.pkg.
#define APP_PKG_SAVED_PREFIX "ps4-ambilight-app-"
// Where it is written first, always the same name whatever the version. The unversioned name is what earlier builds saved.
#define APP_PKG_PART_PATH    APP_PKG_DIR "/" APP_PKG_ASSET_NAME ".part"
#define APP_PKG_LEGACY_PATH  APP_PKG_DIR "/" APP_PKG_ASSET_NAME

// "/data/pkg/ps4-ambilight-app-1.03.pkg" for 1.3. false for a negative number or when it doesn't fit `out`.
static inline bool app_pkg_saved_path(int major, int minor, char *out, size_t outSize)
{
    if (major < 0 || minor < 0) return false;
    int n = snprintf(out, outSize, "%s/%s%d.%02d.pkg", APP_PKG_DIR, APP_PKG_SAVED_PREFIX, major, minor);
    return n > 0 && (size_t)n < outSize;
}

// "1.02", " v1.2\n" -> major 1, minor 2. One dot, 1 to 4 digits each side, nothing after but white space.
static inline bool app_version_parse(const char *text, int *major, int *minor)
{
    if (!text) return false;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
    if (*text == 'v' || *text == 'V') text++;
    int part[2] = { 0, 0 };
    for (int p = 0; p < 2; p++) {
        int digits = 0;
        while (*text >= '0' && *text <= '9') {
            if (++digits > 4) return false;
            part[p] = part[p] * 10 + (*text - '0');
            text++;
        }
        if (digits == 0) return false;
        if (p == 0) { if (*text != '.') return false; text++; }
    }
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
    if (*text != '\0') return false;
    *major = part[0];
    *minor = part[1];
    return true;
}

// -1: the published text isn't a version (or ours isn't), so nothing is said. 0: not newer. 1: newer.
// On 1 or 0 `shown` gets the published version as "M.NN" for the screen.
static inline int app_update_compare(const char *installed, const char *published, char *shown, size_t shownSize)
{
    if (shown && shownSize) shown[0] = '\0';
    int im, in, pm, pn;
    if (!app_version_parse(installed, &im, &in) || !app_version_parse(published, &pm, &pn)) return -1;
    if (shown && shownSize) snprintf(shown, shownSize, "%d.%02d", pm, pn);
    return (pm > im || (pm == im && pn > in)) ? 1 : 0;
}

#endif // APP_UPDATE_H
