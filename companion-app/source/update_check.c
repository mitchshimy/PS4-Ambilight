// The background update check, split out of main.c: the worker thread that asks the update source
// whether the plugin or a shell feed file has a newer build, the flags it publishes through, and
// the once-a-frame poll that turns the answer into Home's state. The code is as it ran on hardware.

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
#include "settings.h"
#include "ui_screens.h"
#include "plugin_common.h"
#include "update_check.h"

// ---------------- update source: GitHub, or a local test server ----------------
//
// Normally every download resolves through GitHub's release API (net_http.c). For testing builds
// without publishing a release, put this in the ini the plugin already reads:
//
//   [dev]
//   update_base_url=http://192.168.2.117:8000
//
// and every download (the plugin, the shell feed .prx, the Ambilight payload) and its ".sha256" sidecar is
// fetched from <base>/<file name> instead -- e.g. `python tools/serve_local_release.py`, which
// serves a folder with python's http.server. Delete the line to go back to GitHub. Read fresh each
// time, so editing the ini needs no restart. [dev] already exists for dev_ip/dev_logging, and the
// app's own save leaves unknown keys alone, so this survives a Save.
//
// Plain http:// through sceHttpCreateConnectionWithURL works on a console (checked with the update
// check, installs and the checksum failure paths).
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

// Compares what's currently installed against the checksum published
// for the latest release, reusing the exact same asset-resolution
// path do_plugin_update() uses to fetch it. Only meaningful once
// plugin_registration_state() has already returned UI_INSTALL_OK --
// NONE/DISABLED take priority in the UI over this, so callers should
// only invoke this when the plugin is both present and registered.
//
// Best-effort/fail-quiet: any network or I/O failure here (no
// connection, GitHub unreachable, asset renamed, ...) just means "no
// update reported this session" rather than an error shown to the
// user -- this runs unprompted on every launch, and a routine offline
// PS4 shouldn't see a scary status message for it.
static bool check_for_plugin_update(void)
{
    // Hash the real .prx on disk first, not the cached sidecar --
    // the sidecar only gets written by do_plugin_update() after an
    // in-app install, so it goes stale (and silently wrong) the
    // moment someone drops a different build in by hand, e.g. FTPing
    // an older .prx over it. Falling back to the sidecar only when
    // hashing the real file fails means a manual replacement always
    // gets compared honestly against the real bytes now sitting at
    // PLUGIN_PRX_PATH, instead of whatever this app last installed.
    // Re-hashing on every check used to matter when this ran inline
    // on the main thread at startup; now that it's a background-
    // thread check, that cost isn't worth trading correctness for.
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

// Same question as check_for_plugin_update(), for one of the two shell feed files: is what is on
// disk what the update source publishes? Returns 1 = newer build available, 0 = up to date,
// -1 = couldn't tell (not installed, no network, source unreachable) -- which callers treat as
// "nothing to report", same fail-quiet rule as the plugin check.
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

// ---------------- background update check ----------------
//
// check_for_plugin_update() above does two sequential blocking HTTPS
// round trips to GitHub. Running it inline on the main thread at
// startup -- as this used to do -- held the whole UI hostage behind
// the network on every single launch, which is what actually produced
// the "app feels slow to open" latency: even on a fast connection
// that's a DNS+TLS+request/response pair twice in a row before the
// home screen becomes interactive, and on a bad connection it's the
// full 10s connect/resolve/send timeout, more than once.
//
// Fixed by kicking the check off on its own thread right after the
// home screen is up, and polling a plain atomic flag once a frame
// (poll_background_update_check(), called from the main loop below)
// instead of blocking anything. SDL_atomic_t is used rather than a
// plain bool specifically so the main thread's read and the
// background thread's write can't tear -- g_state itself is still
// only ever touched from the main thread, in the polling function.
//
// CRASH FIX: the first version of this used SDL_CreateThread(), which
// on a stack size of 0 means "whatever this platform's default is" --
// SDL's own docs warn that default can be "wildly different between
// platforms... a few kilobytes" on some. On real hardware this
// crashed on startup every time: a SIGSEGV write fault at exactly
// rsp-8, with a huge rbp/rsp gap for only a few stack frames rooted at
// a fresh thread-entry trampoline -- the textbook shape of overrunning
// a too-small thread stack, not a bad pointer. github_resolve_asset_
// api_url() above already treats a 64KB buffer as "too large to
// comfortably put on this thread's stack", so a platform default small
// enough to blow past on this call chain isn't a stretch. Switched to
// the plugin side's own scePthreadCreate pattern (see plugin/source/
// main.c's ambient_sample_thread call, confirmed against real
// hardware there) with an explicit stack size instead of trusting an
// unknown SDL-port default. UPDATE_CHECK_THREAD_STACK_SIZE reuses the
// same 256KB this file already trusts sceSslInit() with, rather than
// inventing a new number.
//
// Deliberately defined after check_for_plugin_update() rather than
// forward-declared -- this file has no function forward declarations
// anywhere else and relies entirely on top-to-bottom definition
// order, so this follows that same convention instead of introducing
// a new one.
#define UPDATE_CHECK_THREAD_STACK_SIZE (256 * 1024)

static SDL_atomic_t g_updateCheckDone;      // 0 = not finished yet, 1 = finished
static SDL_atomic_t g_updateCheckAvailable; // valid only once g_updateCheckDone == 1: bitmask of AssetId (1 << id) with a newer build
static SDL_atomic_t g_updateCheckKnown;     // valid only once g_updateCheckDone == 1: bitmask of AssetId whose check actually got an answer
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

// scePthreadCreate's expected entry-point shape (void *(*)(void *)),
// same as plugin/source/sample_thread.c's ambient_sample_thread --
// not SDL_CreateThread's int(*)(void*), since this no longer goes
// through SDL to create the thread.
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
    SDL_AtomicSet(&g_updateCheckAvailable, available);
    SDL_AtomicSet(&g_updateCheckKnown, known);
    SDL_AtomicSet(&g_updateCheckDone, 1); // set last -- this publishes the results above
    return NULL;
}

// Fires the check off in the background. Safe to call even when
// g_state.install isn't UI_INSTALL_OK -- callers still gate on that
// the same way the old inline call did, this just doesn't block them
// while it runs.
void start_background_update_check(void)
{
    // What there is to check right now: the plugin when it is installed and enabled (NONE/DISABLED
    // have their own button and outrank "is there something newer"), and each shell feed file that
    // is on disk, but only with the Shell feed switch on -- off means nothing shell-related runs.
    int want = 0;
    if (g_state.install == UI_INSTALL_OK) want |= 1 << ASSET_PLUGIN;
    if (g_cfg.shellEnabled) {
        if (g_assetPresent[ASSET_SHELL_PRX]) want |= 1 << ASSET_SHELL_PRX;
        if (g_assetPresent[ASSET_PAYLOAD])   want |= 1 << ASSET_PAYLOAD;
    }
    if (want == 0) return;

    // One check at a time: a request that arrives mid-run (the Shell feed switch was just turned
    // on, say) is remembered and run when the current one finishes.
    if (g_updateCheckStarted) { g_recheckWanted = true; return; }

    refresh_dev_base_url();
    g_checkWantMask = want;
    SDL_AtomicSet(&g_updateCheckDone, 0);

    OrbisPthreadAttr attr;
    if (scePthreadAttrInit(&attr) != 0) return;
    scePthreadAttrSetstacksize(&attr, UPDATE_CHECK_THREAD_STACK_SIZE);

    OrbisPthread thread;
    if (scePthreadCreate(&thread, &attr, update_check_thread_fn, NULL, "plugin_update_check") == 0) {
        scePthreadDetach(thread); // fire-and-forget: poll_background_update_check() reads the result via atomics
        g_updateCheckStarted = true;
    }
    // scePthreadCreate copies whatever it needs out of attr (same as
    // POSIX pthread_create) -- safe to destroy right after, whether or
    // not the create call above actually succeeded.
    scePthreadAttrDestroy(&attr);

    // If thread creation itself failed, g_updateCheckStarted stays
    // false and poll_background_update_check() below just never finds
    // anything to report -- same "fail quiet" behavior
    // check_for_plugin_update() already documents for network/GitHub
    // failures.
}

// Called once a frame from the main loop. Cheap when nothing's ready
// yet (one atomic read), and only ever touches g_state/set_status
// from the main thread once the background thread has finished.
void poll_background_update_check(void)
{
    if (!g_updateCheckStarted) return;
    if (!SDL_AtomicGet(&g_updateCheckDone)) return;

    g_updateCheckStarted = false;
    int available = SDL_AtomicGet(&g_updateCheckAvailable);
    int known = SDL_AtomicGet(&g_updateCheckKnown);

    if (available & (1 << ASSET_PLUGIN)) g_state.install = UI_INSTALL_UPDATE;
    // Shell files: an answer replaces what we knew; "couldn't tell" leaves it alone.
    for (int id = ASSET_SHELL_PRX; id <= ASSET_PAYLOAD; id++) {
        if (known & (1 << id)) g_assetUpdate[id] = (available & (1 << id)) != 0;
    }

    if (available) {
        set_status("Update available.", false);
    } else if (!g_state.statusIsError) {
        // Only overwrite a still-neutral "Checking for updates..." --
        // if something else already changed the status line in the
        // meantime (a save, an error, the user starting an install),
        // leave it alone rather than stomping on it late.
        set_status("Ready.", false);
    }

    if (g_recheckWanted) { g_recheckWanted = false; start_background_update_check(); }
}
