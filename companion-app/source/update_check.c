// The background update check, split out of main.c: the worker thread that asks the update source...
// More: docs/code-comments.md (companion-app/source/update_check.c:1)

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <orbis/libkernel.h>

#include "app_paths.h"
#include "app_state.h"
#include <SDL2/SDL.h>
#include "file_util.h"
#include "net_http.h"
#include "sha256.h"
#include "install_plan.h"
#include "update_source.h"
#include "app_update.h"
#include "app_version.h"
#include "settings.h"
#include "ui_screens.h"
#include "plugin_common.h"
#include "update_check.h"
#include "notify.h"

// update source: GitHub, or a local test server Normally every download resolves through GitHub's...
// More: docs/code-comments.md (companion-app/source/update_check.c:26)
static bool dev_update_base_url(char *out, size_t outSize)
{
    out[0] = '\0';
    ini_table_s *table = ini_table_create();
    if (!table) return false;
    bool ok = false;
    if (ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) {
        ok = update_source_normalize(ini_table_get_entry(table, "dev", "update_base_url"), out, outSize);
    }
    ini_table_destroy(table);
    return ok;
}

// Compares what's currently installed against the checksum published for the latest release, reusing...
// More: docs/code-comments.md (companion-app/source/update_check.c:55)
static bool check_for_plugin_update(void)
{
    // Hash the real .prx on disk first, not the cached sidecar -- the sidecar only gets written by...
    // More: docs/code-comments.md (companion-app/source/update_check.c:69)
    char installedHex[SHA256_HEX_SIZE];
    bool hashedRealFile = sha256_hex_of_file(PLUGIN_PRX_PATH, installedHex, sizeof(installedHex));
    if (!hashedRealFile && !read_installed_checksum(installedHex, sizeof(installedHex))) {
        printf("[update-check] FAILED: couldn't hash %s and no readable sidecar\n", PLUGIN_PRX_PATH);
        return false;
    }

    char shaAssetUrl[256];
    const char *acceptHdr = NULL;
    if (!resolve_asset_url(PLUGIN_CHECKSUM_ASSET_NAME, shaAssetUrl, sizeof(shaAssetUrl), &acceptHdr)) {
        printf("[update-check] FAILED: resolve_asset_url(%s)\n", PLUGIN_CHECKSUM_ASSET_NAME);
        return false;
    }

    char remoteHex[512]; // checksum sidecar is tiny; generous vs. a bare 65
    if (!http_download_text(shaAssetUrl, remoteHex, sizeof(remoteHex), acceptHdr)) {
        printf("[update-check] FAILED: http_download_text(%s)\n", shaAssetUrl);
        return false;
    }

    bool matches = sha256_hex_matches(installedHex, remoteHex);
    printf("[update-check] source=%s\n  installed=%s\n  remote_url=%s\n  remote=%.64s\n  result=%s\n",
           hashedRealFile ? "hashed real .prx" : "fallback sidecar", installedHex, shaAssetUrl, remoteHex,
           matches ? "up to date" : "UPDATE AVAILABLE");

    return !matches;
}

// Same question as check_for_plugin_update(), for one of the two shell feed files: is what is on disk...
// More: docs/code-comments.md (companion-app/source/update_check.c:108)
static int check_shell_asset_update(const char *assetName, const char *path)
{
    char installedHex[SHA256_HEX_SIZE];
    if (!sha256_hex_of_file(path, installedHex, sizeof(installedHex))) {
        printf("[update-check] %s: couldn't hash %s\n", assetName, path);
        return -1;
    }
    char sidecarName[96];
    snprintf(sidecarName, sizeof(sidecarName), "%s.sha256", assetName);

    char shaUrl[256];
    const char *acceptHdr = NULL;
    if (!resolve_asset_url(sidecarName, shaUrl, sizeof(shaUrl), &acceptHdr)) {
        printf("[update-check] %s: couldn't resolve %s\n", assetName, sidecarName);
        return -1;
    }
    char remoteHex[512];
    if (!http_download_text(shaUrl, remoteHex, sizeof(remoteHex), acceptHdr)) {
        printf("[update-check] %s: couldn't fetch %s\n", assetName, shaUrl);
        return -1;
    }
    bool matches = sha256_hex_matches(installedHex, remoteHex);
    printf("[update-check] %s\n  installed=%s\n  remote_url=%s\n  remote=%.64s\n  result=%s\n",
           assetName, installedHex, shaUrl, remoteHex, matches ? "up to date" : "UPDATE AVAILABLE");
    return matches ? 0 : 1;
}

// Is the newest release's app newer than this one? The release carries APP_VERSION_ASSET_NAME, a bare "N.NN" line made
// from app_version.h by CI. -1 couldn't tell (offline, no such file on that release, not a version), 0 not newer, 1 newer.
static int check_app_update(char *shown, size_t shownSize)
{
    char url[256];
    const char *acceptHdr = NULL;
    if (!resolve_asset_url(APP_VERSION_ASSET_NAME, url, sizeof(url), &acceptHdr)) {
        printf("[update-check] app: couldn't resolve %s\n", APP_VERSION_ASSET_NAME);
        return -1;
    }
    char remote[64];
    if (!http_download_text(url, remote, sizeof(remote), acceptHdr)) {
        printf("[update-check] app: couldn't fetch %s\n", url);
        return -1;
    }
    int r = app_update_compare(APP_VERSION, remote, shown, shownSize);
    printf("[update-check] app\n  installed=%s\n  remote_url=%s\n  result=%s\n", APP_VERSION, url,
           r < 0 ? "not a version, ignored" : r ? "NEWER APP AVAILABLE" : "up to date");
    return r;
}

// The update check runs on its own thread with an explicit 256 KB stack, polled once a frame via an atomic flag.
// SDL_CreateThread's default stack overflowed on console at startup; do not go back to it. [docs/code-comments.md]
#define UPDATE_CHECK_THREAD_STACK_SIZE (256 * 1024)

static SDL_atomic_t g_updateCheckDone;      // 0 = not finished yet, 1 = finished
static SDL_atomic_t g_updateCheckAvailable; // valid only once g_updateCheckDone == 1: bitmask of AssetId (1 << id) with a newer build
static SDL_atomic_t g_updateCheckKnown;     // valid only once g_updateCheckDone == 1: bitmask of AssetId whose check actually got an answer
static SDL_atomic_t g_updateCheckApp;       // valid only once g_updateCheckDone == 1: -2 not asked, -1 couldn't tell, 0 not newer, 1 newer
static char g_appRemoteShown[16];           // the thread's answer for the above; written before g_updateCheckDone, read after
static bool g_checkWantApp = false;         // whether this run asks about the app; written before the thread starts
static bool g_appChecked = false;           // the app question got an answer once this run, so it is not asked again
static char g_appNotified[16];              // the version already announced in the console's notification bar
static char g_appUpdateVersion[16];         // "" or the newer version, for the main thread
static bool g_updateCheckStarted = false;
static int  g_checkWantMask = 0;            // which assets this run checks; written before the thread starts, read only by it
static bool g_recheckWanted = false;        // something asked for a check while one was already running

// Re-reads [dev] update_base_url. Main thread only. Skipped while a check thread is running so the
// string it reads can't change under it; the next call after it finishes picks up an edit.
void refresh_dev_base_url(void)
{
    if (g_updateCheckStarted) return;
    char fresh[200];
    dev_update_base_url(fresh, sizeof(fresh));
    if (strcmp(fresh, net_dev_base_url()) != 0) {
        net_set_dev_base_url(fresh);
        if (fresh[0]) printf("[update-source] using the local test server: %s\n", fresh);
        else          printf("[update-source] back to GitHub releases\n");
    }
}

// Shell feed bookkeeping shared with the Home plan (see "shell feed" section below).
bool g_assetPresent[ASSET_COUNT];    // only the two shell assets are used
bool g_assetUpdate[ASSET_COUNT];     // last answer from the update source, for all three

// scePthreadCreate's expected entry-point shape (void *(*)(void *)), same as...
// More: docs/code-comments.md (companion-app/source/update_check.c:208)
static void *update_check_thread_fn(void *unused)
{
    (void)unused;
    int want = g_checkWantMask;
    int available = 0, known = 0;
    if (want & (1 << ASSET_PLUGIN)) {
        // Unchanged semantics: false covers both "up to date" and "couldn't check".
        if (check_for_plugin_update()) available |= 1 << ASSET_PLUGIN;
        known |= 1 << ASSET_PLUGIN;
    }
    if (want & (1 << ASSET_SHELL_PRX)) {
        int r = check_shell_asset_update(SHELL_PRX_ASSET_NAME, SHELL_PRX_PATH);
        if (r >= 0) known |= 1 << ASSET_SHELL_PRX;
        if (r == 1) available |= 1 << ASSET_SHELL_PRX;
    }
    if (want & (1 << ASSET_PAYLOAD)) {
        int r = check_shell_asset_update(PAYLOAD_ASSET_NAME, PAYLOAD_PATH);
        if (r >= 0) known |= 1 << ASSET_PAYLOAD;
        if (r == 1) available |= 1 << ASSET_PAYLOAD;
    }
    int appResult = -2;
    if (g_checkWantApp) appResult = check_app_update(g_appRemoteShown, sizeof(g_appRemoteShown));
    SDL_AtomicSet(&g_updateCheckApp, appResult);
    SDL_AtomicSet(&g_updateCheckAvailable, available);
    SDL_AtomicSet(&g_updateCheckKnown, known);
    SDL_AtomicSet(&g_updateCheckDone, 1); // set last -- this publishes the results above
    return NULL;
}

// Fires the check off in the background.
// More: docs/code-comments.md (companion-app/source/update_check.c:238)
void start_background_update_check(void)
{
    // What there is to check right now: the plugin when it is installed and enabled (NONE/DISABLED have...
    // More: docs/code-comments.md (companion-app/source/update_check.c:244)
    int want = 0;
    if (g_state.install == UI_INSTALL_OK) want |= 1 << ASSET_PLUGIN;
    if (g_cfg.shellEnabled) {
        if (g_assetPresent[ASSET_SHELL_PRX]) want |= 1 << ASSET_SHELL_PRX;
        if (g_assetPresent[ASSET_PAYLOAD])   want |= 1 << ASSET_PAYLOAD;
    }
    // The app's own version is asked about once per run whatever else is installed: someone with no plugin yet,
    // or with the plugin off, still needs to hear that a newer app exists. A failed attempt is retried next time.
    bool wantApp = !g_appChecked;
    if (want == 0 && !wantApp) return;

    // One check at a time: a request that arrives mid-run (the Shell feed switch was just turned
    // on, say) is remembered and run when the current one finishes.
    if (g_updateCheckStarted) { g_recheckWanted = true; return; }

    refresh_dev_base_url();
    g_checkWantMask = want;
    g_checkWantApp = wantApp;
    SDL_AtomicSet(&g_updateCheckDone, 0);

    OrbisPthreadAttr attr;
    if (scePthreadAttrInit(&attr) != 0) return;
    scePthreadAttrSetstacksize(&attr, UPDATE_CHECK_THREAD_STACK_SIZE);

    OrbisPthread thread;
    if (scePthreadCreate(&thread, &attr, update_check_thread_fn, NULL, "plugin_update_check") == 0) {
        scePthreadDetach(thread); // fire-and-forget: poll_background_update_check() reads the result via atomics
        g_updateCheckStarted = true;
    }
    // scePthreadCreate copies whatever it needs out of attr (same as POSIX pthread_create) -- safe to...
    // More: docs/code-comments.md (companion-app/source/update_check.c:272)
    scePthreadAttrDestroy(&attr);

    // If thread creation itself failed, g_updateCheckStarted stays false and...
    // More: docs/code-comments.md (companion-app/source/update_check.c:277)
}

// Called once a frame from the main loop.
// More: docs/code-comments.md (companion-app/source/update_check.c:284)
void poll_background_update_check(void)
{
    if (!g_updateCheckStarted) return;
    if (!SDL_AtomicGet(&g_updateCheckDone)) return;

    g_updateCheckStarted = false;
    int available = SDL_AtomicGet(&g_updateCheckAvailable);
    int known = SDL_AtomicGet(&g_updateCheckKnown);

    int appResult = SDL_AtomicGet(&g_updateCheckApp);
    if (appResult >= 0) {            // an answer replaces what we knew; "couldn't tell" leaves it alone
        g_appChecked = true;
        snprintf(g_appUpdateVersion, sizeof(g_appUpdateVersion), "%s", appResult == 1 ? g_appRemoteShown : "");
        snprintf(g_state.appUpdateVersion, sizeof(g_state.appUpdateVersion), "%s", g_appUpdateVersion);
        g_dirty = true;              // Home draws the notice from this
        // Once per version per launch, in the console's notification bar (it follows the Notifications switch).
        if (appResult == 1 && strcmp(g_appNotified, g_appUpdateVersion) != 0) {
            snprintf(g_appNotified, sizeof(g_appNotified), "%s", g_appUpdateVersion);
            app_notify(false, "App update v%s available.", g_appUpdateVersion);
        }
    }
    bool appNewer = g_appUpdateVersion[0] != '\0';

    if (available & (1 << ASSET_PLUGIN)) g_state.install = UI_INSTALL_UPDATE;
    // Shell files: an answer replaces what we knew; "couldn't tell" leaves it alone.
    for (int id = ASSET_SHELL_PRX; id <= ASSET_PAYLOAD; id++) {
        if (known & (1 << id)) g_assetUpdate[id] = (available & (1 << id)) != 0;
    }

    if (available && appNewer) {
        char line[160];
        snprintf(line, sizeof(line), "Update available. A newer app (%s) is out too: press Triangle to download it.",
                 g_appUpdateVersion);
        set_status(line, false);
    } else if (available) {
        set_status("Update available.", false);
    } else if (appNewer) {
        char line[200];
        snprintf(line, sizeof(line), "App update %s available (this is " APP_VERSION "). Press Triangle to download it.",
                 g_appUpdateVersion);
        set_status(line, false);
    } else if (!g_state.statusIsError && strcmp(g_state.statusLine, "Checking for updates...") == 0) {
        // Only a still-neutral "Checking for updates..." becomes "Ready.". The check now runs with nothing installed
        // too, and must not wipe "Plugin not installed yet." or a line something else set meanwhile.
        set_status("Ready.", false);
    }

    if (g_recheckWanted) { g_recheckWanted = false; start_background_update_check(); }
}

const char *app_update_version(void) { return g_appUpdateVersion; }
