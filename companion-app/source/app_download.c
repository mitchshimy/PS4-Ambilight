// Downloads the newest companion app .pkg into /data/pkg for the console's own package installer, and checks it.
// The app can't install a package itself, so this is where it stops.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <orbis/libkernel.h>

#include "app_download.h"
#include "app_paths.h"
#include "app_state.h"
#include "app_update.h"
#include "net_http.h"
#include "plugin_common.h"
#include "sha256.h"
#include "update_check.h"

// Removes our own earlier packages, by name (nothing else in the folder is touched): every version up to this one,
// the unversioned name earlier builds used, and a part file left by an interrupted download.
static void remove_old_app_packages(int major, int minor)
{
    char p[96];
    for (int ma = 1; ma <= major && ma <= 20; ma++)
        for (int mi = 0; mi <= (ma == major ? minor : 99); mi++)
            if (app_pkg_saved_path(ma, mi, p, sizeof(p))) sceKernelUnlink(p);
    sceKernelUnlink(APP_PKG_LEGACY_PATH);
    sceKernelUnlink(APP_PKG_PART_PATH);
}

void do_app_update_download(void)
{
    if (!app_update_version()[0]) return;
    int major, minor;
    char target[96];
    if (!app_version_parse(app_update_version(), &major, &minor) || !app_pkg_saved_path(major, minor, target, sizeof(target))) {
        set_status("App download failed: the new version number isn't usable.", true);
        return;
    }
    refresh_dev_base_url();
    set_status("Looking up the app package...", false);
    render_and_present();

    char pkgUrl[256], shaUrl[256];
    const char *accept = NULL;
    if (!resolve_asset_url(APP_PKG_ASSET_NAME, pkgUrl, sizeof(pkgUrl), &accept)) {
        set_status("App download failed: couldn't find " APP_PKG_ASSET_NAME " in the latest release.", true);
        return;
    }
    if (!resolve_asset_url(APP_PKG_CHECKSUM_ASSET_NAME, shaUrl, sizeof(shaUrl), &accept)) {
        set_status("App download failed: couldn't find " APP_PKG_CHECKSUM_ASSET_NAME " in the latest release.", true);
        return;
    }

    // The installer lists only /data/pkg, so make it when it is missing (this fails harmlessly when it exists).
    APP_MKDIR(APP_PKG_DIR);
    // One of our packages at a time: clear the older ones so the installer's list shows only the new one.
    remove_old_app_packages(major, minor);
    g_state.appPkgPath[0] = '\0';

    const char *part = APP_PKG_PART_PATH;
    int32_t probe = sceKernelOpen(part, ORBIS_O_CREAT_TRUNC_WRONLY, 0777);
    if (probe < 0) {
        set_status("App download failed: couldn't create " APP_PKG_DIR " to save it in.", true);
        return;
    }
    sceKernelClose(probe);

    set_status("Downloading the app package...", false);
    render_and_present();
    uint8_t digest[SHA256_DIGEST_SIZE];
    if (!http_download(pkgUrl, part, digest, accept)) {
        sceKernelUnlink(part);
        set_status("App download failed -- check your network and try again.", true);
        return;
    }

    set_status("Verifying checksum...", false);
    render_and_present();
    char expectedHex[512];
    if (!http_download_text(shaUrl, expectedHex, sizeof(expectedHex), accept)) {
        sceKernelUnlink(part);
        set_status("App download failed: couldn't fetch the checksum for verification.", true);
        return;
    }
    char actualHex[SHA256_HEX_SIZE];
    sha256_to_hex(digest, actualHex);
    if (!sha256_hex_matches(actualHex, expectedHex)) {
        sceKernelUnlink(part);
        set_status("App download failed: checksum mismatch -- the file was deleted.", true);
        return;
    }

    // Flush the file, then rename it into place, so the installer never sees a half-written package.
    int32_t fd = sceKernelOpen(part, 0, 0777);
    if (fd >= 0) { fsync(fd); sceKernelClose(fd); }
    if (sceKernelRename(part, target) < 0) {
        sceKernelUnlink(part);
        set_status("App download failed: couldn't save the package.", true);
        return;
    }

    snprintf(g_state.appPkgPath, sizeof(g_state.appPkgPath), "%s", target);
    set_status("App update downloaded. Install it from GoldHEN > Debug Settings > Package Installer.", false);
    g_dirty = true;
}
