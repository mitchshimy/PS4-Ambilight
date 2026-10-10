// Installing, enabling and checking the ambient light plugin, and reading GoldHEN's plugin loader
// switch, split out of main.c. The code is as it ran on hardware.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <orbis/libkernel.h>

#include "app_paths.h"
#include "app_state.h"
#include "file_util.h"
#include "net_http.h"
#include "sha256.h"
#include "update_check.h"
#include "plugin_common.h"
#include "plugin_install.h"
#include "settings.h"
#include "ui_screens.h"
#include "goldhen_config.h"

static bool plugin_exists(void); // defined below; forward-declared for plugin_registration_state()

// Read-only scan of GoldHEN's plugins.ini for an exact, active (i.e.
// not commented-out) line matching PLUGIN_PRX_PATH -- same trimmed
// exact-match rule ensure_plugin_registered_in_goldhen() uses to avoid
// duplicate lines, factored out so the home screen's disabled-state
// check (plugin_registration_state() below) can reuse it without
// opening the file for write. A commented line (";path" or "#path",
// after trim) does NOT match the bare path, so a commented entry and
// a missing one both correctly read as "not registered" here -- the
// caller doesn't need to tell those two cases apart any further than
// that.
static bool plugin_registered_in_goldhen_ini(void)
{
    const char *path = PLUGIN_PRX_PATH;
    size_t pathLen = strlen(path);
    bool found = false;

    int32_t fd = sceKernelOpen(GOLDHEN_PLUGINS_INI, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return false;
    long size = sceKernelLseek(fd, 0, SEEK_END);
    sceKernelLseek(fd, 0, SEEK_SET);
    if (size > 0) {
        char *buf = (char *)malloc((size_t)size);
        if (buf) {
            long nread = sceKernelRead(fd, buf, (size_t)size);
            long len = (nread > 0) ? nread : 0;
            const char *p = buf, *end = buf + len;
            while (p < end) {
                const char *lineEnd = memchr(p, '\n', (size_t)(end - p));
                if (!lineEnd) lineEnd = end;
                const char *ls = p, *le = lineEnd;
                while (ls < le && (*ls == ' ' || *ls == '\t' || *ls == '\r')) ls++;
                while (le > ls && (le[-1] == ' ' || le[-1] == '\t' || le[-1] == '\r')) le--;
                if ((size_t)(le - ls) == pathLen && memcmp(ls, path, pathLen) == 0) {
                    found = true;
                    break;
                }
                p = (lineEnd < end) ? lineEnd + 1 : end;
            }
            free(buf);
        }
    }
    sceKernelClose(fd);
    return found;
}

// Three-way install state for the home screen: not installed (no
// .prx on disk at all), installed but disabled (.prx present, but
// plugins.ini's entry for it is commented out or missing), or
// installed and enabled. Doesn't distinguish "commented" from
// "missing" any further -- see plugin_registered_in_goldhen_ini()'s
// comment for why that's fine.
UiInstallState plugin_registration_state(void)
{
    if (!plugin_exists()) return UI_INSTALL_NONE;
    return plugin_registered_in_goldhen_ini() ? UI_INSTALL_OK : UI_INSTALL_DISABLED;
}

// Registers the plugin in GoldHEN's plugins.ini under [default]
// (loads for every title).
//
// This is deliberately NOT built on this file's own ini_table_s
// (settings_load/save's machinery) -- that's a generic parser meant
// for OUR OWN ini, where this app owns every key. plugins.ini is
// GoldHEN's file, shared with every other installed plugin, and uses
// a different dialect: bare-path lines with no "key = value" at all,
// plus ";"-prefixed comment lines GoldHEN itself ships as inert
// examples. Routing this through ini_table_read_from_file +
// ini_table_write_to_file was tried and empirically checked against a
// file shaped exactly like the one this app will actually find: it
// silently drops every comment line (ini_table_read_from_file's
// Comment state is dead code -- nothing ever transitions into it) and
// rewrites every bare-path entry as "path = ", corrupting even a
// pre-existing, unrelated, already-working plugin registration
// ([CUSA00411]'s entry in that test) into a form GoldHEN's own loader
// likely won't recognize. None of that is acceptable for a file this
// app doesn't own.
//
// So this instead only ever inserts one line, entirely by byte
// position -- find "[default]", find where its body ends (the next
// "[...]" header or EOF), insert our path there if it isn't already
// present anywhere under that section, and copy every other byte of
// the file through completely unchanged. If the file doesn't exist
// yet, it's created with just [default] and our own entry -- nothing
// else, since this app has no business guessing what other plugins a
// given user does or doesn't have installed. Verified against all of:
// no file yet, a file shaped like the one this app will actually find
// (comments + our section + an unrelated section with its own real
// entry), running it twice in a row (must not duplicate the line),
// and a file that has no [default] section at all yet.
static bool ensure_plugin_registered_in_goldhen(void)
{
    const char *path = PLUGIN_PRX_PATH;
    size_t pathLen = strlen(path);

    if (plugin_registered_in_goldhen_ini()) return true; // already there -- nothing to do

    char *buf = NULL;
    long len = 0;
    int32_t fd = sceKernelOpen(GOLDHEN_PLUGINS_INI, 0 /* O_RDONLY */, 0777);
    if (fd >= 0) {
        long size = sceKernelLseek(fd, 0, SEEK_END);
        sceKernelLseek(fd, 0, SEEK_SET);
        if (size > 0) {
            buf = (char *)malloc((size_t)size);
            if (!buf) { sceKernelClose(fd); return false; }
            long nread = sceKernelRead(fd, buf, (size_t)size);
            len = (nread > 0) ? nread : 0;
        }
        sceKernelClose(fd);
    }

    size_t cap = (size_t)len + 4096, outLen = 0;
    char *out = (char *)malloc(cap);
    if (!out) { free(buf); return false; }

    #define GH_APPEND(s) do { \
        size_t _n = strlen(s); \
        if (outLen + _n + 1 > cap) { \
            cap = (cap + _n + 1) * 2; \
            char *_g = (char *)realloc(out, cap); \
            if (!_g) { free(buf); free(out); return false; } \
            out = _g; \
        } \
        memcpy(out + outLen, (s), _n); outLen += _n; \
    } while (0)
    #define GH_APPEND_N(s, n) do { \
        size_t _n = (size_t)(n); \
        if (outLen + _n + 1 > cap) { \
            cap = (cap + _n + 1) * 2; \
            char *_g = (char *)realloc(out, cap); \
            if (!_g) { free(buf); free(out); return false; } \
            out = _g; \
        } \
        memcpy(out + outLen, (s), _n); outLen += _n; \
    } while (0)

    if (!buf || len == 0) {
        // No existing file (or an empty one): create it with just
        // [default] and our own entry. Earlier drafts of this
        // function also wrote out several other plugins' filenames as
        // commented-out examples -- those came from one particular
        // user's own existing plugins.ini, not from anything GoldHEN
        // itself ships or every install has, and had no business being
        // hardcoded into a file this app creates for anyone else.
        GH_APPEND("[default]\n");
        GH_APPEND(path);
        GH_APPEND("\n");
    } else {
        // Find the "[default]" header -- must start a line (either the
        // very first byte of the file, or right after a '\n').
        const char *defaultHdr = NULL;
        for (const char *s = buf; s + 9 <= buf + len; s++) {
            if (memcmp(s, "[default]", 9) == 0 && (s == buf || s[-1] == '\n')) { defaultHdr = s; break; }
        }

        if (!defaultHdr) {
            // File has content, but no [default] section yet -- append
            // a new one at the end, everything else untouched.
            GH_APPEND_N(buf, (size_t)len);
            if (len > 0 && buf[len - 1] != '\n') GH_APPEND("\n");
            GH_APPEND("\n[default]\n");
            GH_APPEND(path);
            GH_APPEND("\n");
        } else {
            const char *hdrLineEnd = memchr(defaultHdr, '\n', (size_t)(buf + len - defaultHdr));
            const char *afterHdr = hdrLineEnd ? hdrLineEnd + 1 : buf + len;

            // End of [default]'s body: the next line that starts a new
            // section, or EOF.
            const char *bodyEnd = buf + len;
            for (const char *s = afterHdr; s < buf + len; s++) {
                if (*s == '[' && (s == buf || s[-1] == '\n')) { bodyEnd = s; break; }
            }

            // Trim trailing blank lines from the body so the new line
            // lands right after the last real line, not after a run of
            // blank ones.
            const char *insertAt = bodyEnd;
            while (insertAt > afterHdr && (insertAt[-1] == '\n' || insertAt[-1] == '\r')) insertAt--;

            GH_APPEND_N(buf, (size_t)(insertAt - buf));
            if (insertAt > afterHdr) GH_APPEND("\n");
            GH_APPEND(path);
            GH_APPEND("\n");
            // Keep a blank-line separator before the next section, if
            // there is one -- matches the convention already visible
            // in a real plugins.ini (a blank line between [default]
            // and the next title's section).
            if (bodyEnd < buf + len) GH_APPEND("\n");
            GH_APPEND_N(bodyEnd, (size_t)(buf + len - bodyEnd));
        }
    }
    #undef GH_APPEND
    #undef GH_APPEND_N

    free(buf);

    int32_t wfd = sceKernelOpen(GOLDHEN_PLUGINS_INI, ORBIS_O_CREAT_TRUNC_WRONLY /* see plugin_common.h's BUG FIX comment */, 0777);
    if (wfd < 0) { free(out); return false; }
    bool ok = (outLen == 0) || (sceKernelWrite(wfd, out, outLen) == (long)outLen);
    sceKernelClose(wfd);
    free(out);
    return ok;
}

static bool plugin_exists(void)
{
    int32_t fd = sceKernelOpen(PLUGIN_PRX_PATH, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return false;
    sceKernelClose(fd);
    return true;
}

// Downloads the latest plugin build and installs it. Used for both
// "Install plugin" (first run) and re-fetching a newer build once
// already installed.
void do_plugin_update(void)
{
    refresh_dev_base_url();
    set_status(net_dev_base_url()[0] ? "Looking up the release on the local test server..." : "Looking up latest release...", false);
    render_and_present();   // one frame of feedback before the blocking I/O below

    // First hop: resolve both assets' own small api.github.com URLs
    // from the release metadata, up front, before downloading
    // anything -- so a stale/renamed asset fails fast with a clear
    // status message instead of after the (much larger) .prx download.
    // See the BUG FIX comment on GITHUB_RELEASES_LATEST_API_URL in net_http.h
    // for why this replaces a direct GET of PLUGIN_UPDATE_URL.
    char prxAssetUrl[256];
    const char *acceptHdr = NULL;   // NULL when the source is a plain local file server
    if (!resolve_asset_url(PLUGIN_ASSET_NAME, prxAssetUrl, sizeof(prxAssetUrl), &acceptHdr)) {
        set_status("Update failed: couldn't find " PLUGIN_ASSET_NAME " in the latest release.", true);
        return;
    }
    char shaAssetUrl[256];
    if (!resolve_asset_url(PLUGIN_CHECKSUM_ASSET_NAME, shaAssetUrl, sizeof(shaAssetUrl), &acceptHdr)) {
        set_status("Update failed: couldn't find " PLUGIN_CHECKSUM_ASSET_NAME " in the latest release.", true);
        return;
    }

    set_status("Downloading plugin...", false);
    render_and_present();

    // Second hop: Accept: application/octet-stream is what makes the
    // GitHub asset API respond with a small-header redirect straight
    // to the asset bytes instead of its default JSON metadata body.
    const char *tmpPath = "/data/ps4_ambient_light_update.tmp";
    uint8_t digest[SHA256_DIGEST_SIZE];
    if (!http_download(prxAssetUrl, tmpPath, digest, acceptHdr)) {
        set_status("Download FAILED -- check your network and try again.", true);
        return;
    }

    set_status("Verifying checksum...", false);
    render_and_present();

    char expectedHex[512]; // checksum sidecar is tiny; generous vs. a bare 65
    if (!http_download_text(shaAssetUrl, expectedHex, sizeof(expectedHex), acceptHdr)) {
        // NOT YET VERIFIED (same status as the IME-dialog and L1/R1
        // conventions flagged at the top of this file): sceKernelUnlink
        // matches libkernel's usual POSIX-mirroring naming
        // (sceKernelOpen/Read/Write/Close/Rename are all real and
        // already used above/below), but no sample in this SDK
        // snapshot was found that actually calls it. If it's missing
        // at link time, the fallback is to just leave the .tmp file on
        // disk -- harmless (it's overwritten by O_TRUNC next attempt)
        // -- rather than something this function must have.
        sceKernelUnlink(tmpPath);
        set_status("Update failed: couldn't fetch checksum for verification.", true);
        return;
    }

    char actualHex[SHA256_HEX_SIZE];
    sha256_to_hex(digest, actualHex);
    if (!sha256_hex_matches(actualHex, expectedHex)) {
        sceKernelUnlink(tmpPath);
        set_status("Update failed: checksum mismatch -- downloaded file rejected.", true);
        return;
    }

    // Stage the new build alongside the live one, and only ever touch
    // PLUGIN_PRX_PATH itself via sceKernelRename() once the staged
    // copy is fully verified good -- a copy failure partway through
    // never touches the live, working file.
    const char *stagingPath = PLUGIN_PRX_PATH ".new";
    int32_t src = sceKernelOpen(tmpPath, 0, 0777);
    if (src < 0) { set_status("Update failed: couldn't reopen downloaded file.", true); return; }
    // BUG FIX: this used to be the bare literal "0x200 | 0x001" with
    // no O_TRUNC at all (see plugin_common.h's BUG FIX comment) -- if
    // stagingPath already had leftover bytes from an earlier update
    // attempt and the new download was the same size or smaller, this
    // open() didn't truncate them, so the copy loop below (which reads
    // src until ITS eof, not stagingPath's old length) could carry
    // that leftover tail through the rename into the live
    // PLUGIN_PRX_PATH -- corrupting the install with extra trailing
    // bytes that were never part of the SHA-256-verified download,
    // which is exactly what made check_for_plugin_update() see a
    // mismatch against GitHub's checksum on every subsequent launch.
    int32_t dst = sceKernelOpen(stagingPath, ORBIS_O_CREAT_TRUNC_WRONLY, 0777);
    if (dst < 0) { sceKernelClose(src); set_status("Update failed: can't write staging file.", true); return; }
    uint8_t buf[64 * 1024];
    bool copyOk = true;
    for (;;) {
        long n = sceKernelRead(src, buf, sizeof(buf));
        if (n < 0) { copyOk = false; break; }
        if (n == 0) break;
        if (sceKernelWrite(dst, buf, n) != n) { copyOk = false; break; }
    }
    sceKernelClose(src);
    // DURABILITY FIX: force the staged copy's data out of the OS
    // write-back cache and onto physical storage BEFORE it's renamed
    // into place. Without this, sceKernelWrite() above only guarantees
    // the bytes are visible to reads within this same running OS
    // instance -- nothing stopped a reboot (or GoldHEN/the console
    // dropping power) between "Installed." and the plugin actually
    // being reloaded from disk from silently reverting stagingPath's
    // dirty pages, so the rename below could complete against data
    // that was never actually committed. fsync() operates on the
    // fd, same POSIX call already used (in a different file) on the
    // libc FILE*-based path in config.c -- sceKernelOpen()'s fds are
    // the same POSIX-compatible fds, so this works on the raw fd here
    // too, no sceKernel-prefixed equivalent needed. Checked: a failed
    // fsync means the data isn't safely on disk, so this must abort
    // the install rather than rename an unverified-durable file into
    // the live path.
    if (copyOk && fsync(dst) < 0) copyOk = false;
    sceKernelClose(dst);
    if (!copyOk) {
        set_status("Update failed: staging copy incomplete. Your existing plugin is untouched.", true);
        return;
    }

    if (sceKernelRename(stagingPath, PLUGIN_PRX_PATH) < 0) {
        set_status("Update failed: couldn't install staged build. Your existing plugin is untouched.", true);
        return;
    }

    // Same reasoning as above, applied to the rename itself: a rename
    // is a directory-metadata change, which the OS can also hold in
    // write-back cache rather than committing immediately. Re-opening
    // PLUGIN_PRX_PATH here and fsyncing that fd (rather than trying to
    // fsync stagingPath's now-stale fd, or a directory fd -- open()
    // on a directory isn't reliably supported by this SDK's libkernel)
    // pushes the renamed directory entry and its data out to disk
    // before this function reports success.
    {
        int32_t verifyFd = sceKernelOpen(PLUGIN_PRX_PATH, 0, 0777);
        if (verifyFd >= 0) {
            fsync(verifyFd);
            sceKernelClose(verifyFd);
        }
    }

    if (!ensure_plugin_registered_in_goldhen()) {
        set_status("Plugin installed, but plugins.ini update failed -- add it manually.", true);
        return;
    }

    // Best-effort -- see write_installed_checksum()'s comment. A
    // failed write here doesn't affect the install itself.
    write_installed_checksum(actualHex);

    g_state.install = UI_INSTALL_OK;
    set_status("Installed. Relaunch your game.", false);
}

// Re-enables a .prx that's already on disk but whose plugins.ini entry
// is commented out or missing (UI_INSTALL_DISABLED) -- just rewrites
// the ini line locally via the same ensure_plugin_registered_in_goldhen()
// used after a fresh install/update, no network download needed.
void do_plugin_enable(void)
{
    if (!ensure_plugin_registered_in_goldhen()) {
        set_status("Couldn't update plugins.ini -- enable it manually.", true);
        return;
    }
    g_state.install = UI_INSTALL_OK;
    set_status("Plugin enabled. Relaunch your game to load it.", false);
}

// ---------------- GoldHEN's plugin loader switch ----------------
//
// Settings > GoldHEN > Plugin Settings > Enable Plugins Loader is saved in /data/GoldHEN/config.ini
// ([PluginLoader] PluginLoader_Enabled, goldhen_config.h). With it off GoldHEN loads no plugin for
// any game, and for a long time Home couldn't tell. Now Home reads it and shows it ahead of every
// other check. The app only READS it: GoldHEN keeps the live value in memory and doesn't re-read
// the file (writing 1 left its menu showing the switch off), and it won't change the switch at all
// while a game or app is running, which includes this one. So the button just closes the app.
// Only a definite 0 counts as off: a missing file or key (an older GoldHEN) is UNKNOWN.

PluginLoaderState read_plugin_loader_state(void)
{
    size_t len = 0;
    char *buf = read_small_file(GOLDHEN_CONFIG_INI, &len);
    if (!buf) return LOADER_UNKNOWN;
    PluginLoaderState s = goldhen_plugin_loader_state(buf, len);
    free(buf);
    return s;
}

// Re-reads the switch and updates Home when it changed. Cheap (the file is about 1 KB), and the
// person can flip it in GoldHEN's own menu while this app sits in the background, so Home calls
// this every couple of seconds and right after coming back to the foreground.
void refresh_plugin_loader_state(void)
{
    PluginLoaderState s = read_plugin_loader_state();
    if (s == g_state.loader) return;
    PluginLoaderState was = g_state.loader;
    g_state.loader = s;
    g_dirty = true;
    if (s == LOADER_OFF)
        set_status("GoldHEN's plugin loader is off -- close this app and tick it in Settings > GoldHEN > Plugin Settings.", true);
    else if (was == LOADER_OFF && s == LOADER_ON)
        set_status("GoldHEN's plugin loader is on.", false);
}
