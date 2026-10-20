// The shell feed's file work, split out of main.c: install and repair of ps4_shell_feed.prx and the...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:1)

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <orbis/libkernel.h>

#include "app_paths.h"
#include "app_state.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "file_util.h"
#include "net_http.h"
#include "sha256.h"
#include "update_check.h"
#include "plugin_install.h"
#include "plugin_common.h"
#include "install_plan.h"
#include "claim_file.h"
#include "lease.h"
#include "payload_ini.h"
#include "settings.h"
#include "ui_screens.h"
#include "shell_feed.h"

// shell feed: install, repair, autoload, claim Everything below runs only while "Shell feed" is on in...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:32)

// Re-reads which of the two shell files are on disk. A zero-byte file counts as missing: a
// download cut off before its first byte must not look like an install.
void rescan_shell_files(void)
{
    g_assetPresent[ASSET_SHELL_PRX] = file_size_of(SHELL_PRX_PATH) > 0;
    g_assetPresent[ASSET_PAYLOAD]   = file_size_of(PAYLOAD_PATH) > 0;
    if (!g_assetPresent[ASSET_SHELL_PRX]) g_assetUpdate[ASSET_SHELL_PRX] = false;
    if (!g_assetPresent[ASSET_PAYLOAD])   g_assetUpdate[ASSET_PAYLOAD]   = false;
}

// Recomputes the shell feed's summary for Home from the facts above.
// More: docs/code-comments.md (companion-app/source/shell_feed.c:49)
void sync_shell_summary(void)
{
    AssetStatus prx = !g_assetPresent[ASSET_SHELL_PRX] ? ASSET_MISSING
                    : g_assetUpdate[ASSET_SHELL_PRX]   ? ASSET_UPDATE : ASSET_PRESENT;
    AssetStatus bin = !g_assetPresent[ASSET_PAYLOAD]   ? ASSET_MISSING
                    : g_assetUpdate[ASSET_PAYLOAD]     ? ASSET_UPDATE : ASSET_PRESENT;
    ShellSummary now = shell_summarize(g_cfg.shellEnabled != 0, prx, bin);
    if (!shell_summary_equal(&now, &g_state.shell)) {
        g_state.shell = now;
        g_dirty = true;
    }
}

// ---- the loopback lease (lease.h): the app's claim on the strip, and proof the payload runs (connected = running) ----
// One connection to 127.0.0.1:LEASE_PORT while "Shell feed" is on; a claim is one message, not a file write; a dead app's socket is closed by the kernel.
LeaseClient g_appLease = { .fd = -1 };

static int app_lease_open(void *ctx)
{
    (void)ctx;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int on = 1;
    setsockopt(fd, SOL_SOCKET, LEASE_SO_NOSIGPIPE, &on, sizeof(on));
    struct timeval to = { 0, 100000 };
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    LEASE_SET_SIN_LEN(a);
    a.sin_family = AF_INET;
    a.sin_port = htons(LEASE_PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) { close(fd); return -1; }
    return fd;
}
static int app_lease_send(void *ctx, int fd, const char *msg, size_t n)
{
    (void)ctx;
    return send(fd, msg, n, LEASE_MSG_NOSIGNAL) == (ssize_t)n ? 0 : -1;
}
static int app_lease_peek(void *ctx, int fd)
{
    (void)ctx;
    if (fd < 0 || fd >= FD_SETSIZE) return -1;
    fd_set rs; FD_ZERO(&rs); FD_SET(fd, &rs);
    struct timeval tv = { 0, 0 };
    int r = select(fd + 1, &rs, NULL, NULL, &tv);
    if (r < 0) return -1;
    if (r == 0 || !FD_ISSET(fd, &rs)) return 1;
    char sink[16];
    ssize_t n = recv(fd, sink, sizeof(sink), LEASE_MSG_DONTWAIT);
    return n > 0 ? 1 : (n == 0 ? 0 : -1);
}
static void app_lease_close(void *ctx, int fd) { (void)ctx; close(fd); }
static const LeaseOps g_appLeaseOps = { app_lease_open, app_lease_send, app_lease_peek, app_lease_close, NULL };

// Once per frame. driving = the app is sending to the strip right now. Returns true while the claim is carried by a lease connection
// (the caller then keeps the claim FILE at fg=0, as the plugin does).
bool lease_app_tick(uint32_t nowMs, bool driving)
{
    static bool inited = false;
    if (!inited) { lease_client_init(&g_appLease); inited = true; }
    lease_client_set(&g_appLease, driving ? 1 : 0, CLAIM_TITLE);
    lease_client_tick(&g_appLease, nowMs, &g_appLeaseOps, g_cfg.shellEnabled ? 1 : 0);
    if (!g_cfg.shellEnabled && !driving && g_appLease.fd >= 0) lease_client_shutdown(&g_appLease, &g_appLeaseOps);
    return g_appLease.fd >= 0;
}

void lease_app_shutdown(void) { lease_client_shutdown(&g_appLease, &g_appLeaseOps); }

// When the payload was last sent to BinLoader, so the UI can say "starting" until the heartbeat shows up.
static uint32_t g_payloadSentMs = 0;
static bool     g_payloadSentOnce = false;
static bool     g_payloadSentPending = false;

// is the payload already running?
// More: docs/code-comments.md (companion-app/source/shell_feed.c:70)
#define SHELL_ALIVE_FILE        "/data/ps4_shell_alive"
#define SHELL_ALIVE_POLL_MS     500u
#define SHELL_ALIVE_STALE_MS    3500u
#define PAYLOAD_STARTING_MS     12000u   // how long after a send we say "starting" before giving up

void shell_alive_poll(uint32_t nowMs)
{
    static uint32_t lastPollMs = 0, lastChangeMs = 0;
    static char lastText[32];
    static bool haveBaseline = false;

    if (g_payloadSentPending) { g_payloadSentPending = false; g_payloadSentOnce = true; g_payloadSentMs = nowMs; }
    if (nowMs - lastPollMs < SHELL_ALIVE_POLL_MS) return;
    lastPollMs = nowMs;

    if (g_cfg.shellEnabled && g_appLease.fd < 0) { // connected: no file read at all
        char cur[32]; memset(cur, 0, sizeof(cur));
        int32_t fd = sceKernelOpen(SHELL_ALIVE_FILE, 0 /* O_RDONLY */, 0);
        if (fd >= 0) {
            long r = sceKernelRead(fd, cur, sizeof(cur) - 1);
            sceKernelClose(fd);
            if (r <= 0) cur[0] = '\0';
        }
        if (cur[0] != '\0' && strcmp(cur, lastText) != 0) {
            memcpy(lastText, cur, sizeof(lastText));
            // Only a change after a baseline read proves it is alive right now (the value on disk
            // may be left over from before the app started).
            if (haveBaseline) lastChangeMs = nowMs;
            haveBaseline = true;
        }
    }

    // A held lease connection is proof the payload is listening right now (see lease_app_tick); the heartbeat file is the older proof
    // and the fallback when the payload could not open its listener.
    bool running = g_cfg.shellEnabled && (g_appLease.fd >= 0 || (lastChangeMs != 0 && (nowMs - lastChangeMs) < SHELL_ALIVE_STALE_MS));
    if (running) g_payloadSentOnce = false;   // it came up; a later stop is not "starting"
    bool starting = g_cfg.shellEnabled && !running && g_payloadSentOnce && (nowMs - g_payloadSentMs) < PAYLOAD_STARTING_MS;

    if (running != g_state.shellFeedRunning || starting != g_state.shellFeedStarting) {
        g_state.shellFeedRunning  = running;
        g_state.shellFeedStarting = starting;
        g_dirty = true;
    }
}

// Downloads one file from the update source into its own staging path, checks it against the published...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:120)
static bool install_shell_asset(const char *assetName, const char *dstPath, const char *stagingPath,
                                char *err, size_t errSize)
{
    char sidecarName[96];
    snprintf(sidecarName, sizeof(sidecarName), "%s.sha256", assetName);

    char binUrl[256], shaUrl[256];
    const char *acceptHdr = NULL;
    if (!resolve_asset_url(assetName, binUrl, sizeof(binUrl), &acceptHdr)) {
        snprintf(err, errSize, "couldn't find %s at the update source.", assetName);
        return false;
    }
    if (!resolve_asset_url(sidecarName, shaUrl, sizeof(shaUrl), &acceptHdr)) {
        snprintf(err, errSize, "couldn't find %s at the update source.", sidecarName);
        return false;
    }

    uint8_t digest[SHA256_DIGEST_SIZE];
    if (!http_download(binUrl, stagingPath, digest, acceptHdr)) {
        sceKernelUnlink(stagingPath);
        snprintf(err, errSize, "download failed -- check your network and the update source.");
        return false;
    }

    char expectedHex[512];
    if (!http_download_text(shaUrl, expectedHex, sizeof(expectedHex), acceptHdr)) {
        sceKernelUnlink(stagingPath);
        snprintf(err, errSize, "couldn't fetch the checksum for verification.");
        return false;
    }
    char actualHex[SHA256_HEX_SIZE];
    sha256_to_hex(digest, actualHex);
    if (!sha256_hex_matches(actualHex, expectedHex)) {
        sceKernelUnlink(stagingPath);
        snprintf(err, errSize, "checksum mismatch -- downloaded file rejected.");
        return false;
    }

    // Durability, as in do_plugin_update(): get the verified bytes onto storage before the rename.
    int32_t fd = sceKernelOpen(stagingPath, 0 /* O_RDONLY */, 0777);
    if (fd < 0) { snprintf(err, errSize, "couldn't reopen the downloaded file."); return false; }
    bool synced = (fsync(fd) >= 0);
    sceKernelClose(fd);
    if (!synced) {
        sceKernelUnlink(stagingPath);
        snprintf(err, errSize, "couldn't flush the downloaded file to storage.");
        return false;
    }

    if (sceKernelRename(stagingPath, dstPath) < 0) {
        sceKernelUnlink(stagingPath);
        snprintf(err, errSize, "couldn't move it into place.");
        return false;
    }
    int32_t verifyFd = sceKernelOpen(dstPath, 0, 0777);
    if (verifyFd >= 0) { fsync(verifyFd); sceKernelClose(verifyFd); }
    return true;
}

// Sends the Ambilight payload (.bin) to GoldHEN's BinLoader on port 9090 -- the same protocol every...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:183)
bool launch_payload_now(void)
{
    int32_t fd = sceKernelOpen(PAYLOAD_PATH, 0 /* O_RDONLY */, 0777);
    if (fd < 0) {
        set_status("Ambilight payload: " PAYLOAD_ASSET_NAME " not found -- install it from Home first.", true);
        return false;
    }
    long size = sceKernelLseek(fd, 0, SEEK_END);
    sceKernelLseek(fd, 0, SEEK_SET);
    if (size <= 0) {
        sceKernelClose(fd);
        set_status("Ambilight payload: " PAYLOAD_ASSET_NAME " is empty -- reinstall from Home.", true);
        return false;
    }

    // Read entire payload into a heap buffer (payloads are small, typically < 1 MB).
    char *buf = (char *)malloc((size_t)size);
    if (!buf) {
        sceKernelClose(fd);
        set_status("Ambilight payload: out of memory reading the payload.", true);
        return false;
    }
    long nread = sceKernelRead(fd, buf, (size_t)size);
    sceKernelClose(fd);
    if (nread != size) {
        free(buf);
        set_status("Ambilight payload: couldn't read the full payload from disk.", true);
        return false;
    }

    // GoldHEN's BinLoader: connect to 127.0.0.1:9090, write raw bytes, close.
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        free(buf);
        set_status("Ambilight payload: socket() failed -- BinLoader may not be running.", true);
        return false;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(9090);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 3-second connect timeout via SO_RCVTIMEO/SO_SNDTIMEO; if BinLoader isn't listening
    // this returns quickly rather than blocking for the system default.
    struct timeval tv = { 3, 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        free(buf);
        set_status("Ambilight payload: couldn't connect to BinLoader on port 9090 -- is GoldHEN loaded?", true);
        return false;
    }

    // Stream all bytes. send() may write fewer than requested on each call; loop until done.
    long sent = 0;
    bool ok = true;
    while (sent < size) {
        long n = send(sock, buf + sent, (size_t)(size - sent), 0);
        if (n <= 0) { ok = false; break; }
        sent += n;
    }
    close(sock);
    free(buf);

    if (!ok) {
        set_status("Ambilight payload: send to BinLoader failed partway through.", true);
        return false;
    }

    g_payloadSentPending = true;   // shell_alive_poll() stamps the time on its next call

    if (g_state.fwTitle[0]) {
        // Sent, but the loader is untested on this firmware or has no entry for it: the text says which.
        char msg[256];
        snprintf(msg, sizeof(msg), "Ambilight payload sent. %s %s", g_state.fwTitle, g_state.fwBody);
        set_status(msg, false);
        return true;
    }
    set_status("Ambilight payload sent to GoldHEN -- shell feed should be active now.", false);
    return true;
}

// The Home button's shell feed half: creates the folders if they aren't there, then fetches whatever...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:283)
void do_shell_repair(void)
{
    // Both folders may not exist yet (first install) -- or may exist and be empty. Either way, make
    // them; an already-existing folder is not an error.
    refresh_dev_base_url();
    APP_MKDIR(SHELL_DIR);
    APP_MKDIR(PAYLOADS_DIR);
    rescan_shell_files();

    struct { AssetId id; const char *name; const char *dst; const char *stage; const char *label; } jobs[2] = {
        { ASSET_SHELL_PRX, SHELL_PRX_ASSET_NAME, SHELL_PRX_PATH, SHELL_PRX_STAGING_PATH, "shell feed" },
        { ASSET_PAYLOAD,   PAYLOAD_ASSET_NAME,   PAYLOAD_PATH,   PAYLOAD_STAGING_PATH,   "Ambilight payload" },
    };

    int installed = 0, updated = 0;
    for (int i = 0; i < 2; i++) {
        bool wasThere = g_assetPresent[jobs[i].id];
        if (wasThere && !g_assetUpdate[jobs[i].id]) continue;   // fine as it is

        char msg[200];
        snprintf(msg, sizeof(msg), "Downloading %s%s...", jobs[i].label, net_dev_base_url()[0] ? " from the local server" : "");
        set_status(msg, false);
        render_and_present();

        char err[128];
        if (!install_shell_asset(jobs[i].name, jobs[i].dst, jobs[i].stage, err, sizeof(err))) {
            snprintf(msg, sizeof(msg), "Shell feed: %s %s", jobs[i].label, err);
            set_status(msg, true);
            rescan_shell_files();
            return;
        }
        g_assetPresent[jobs[i].id] = true;
        g_assetUpdate[jobs[i].id] = false;
        if (wasThere) updated++; else installed++;
    }
    rescan_shell_files();

    if (installed > 0) {
        // Install succeeded: run the launcher immediately via BinLoader so the shell feed is live right now...
        // More: docs/code-comments.md (companion-app/source/shell_feed.c:324)
        bool launched = launch_payload_now();
        if (!launched) {
            set_status(g_cfg.shellAutoload
                ? "Shell feed installed. It also loads next time GoldHEN starts (Autoload is on)."
                : "Shell feed installed. Run the Ambilight payload via Launch Ambilight payload in Set up, or turn on Autoload.", false);
        } else if (g_cfg.shellAutoload) {
            set_status("Ambilight payload sent to GoldHEN -- shell feed active. Autoload at boot is also on.", false);
        }
    } else if (updated > 0) {
        // (commented-out code removed)
        // More: docs/code-comments.md (companion-app/source/shell_feed.c:337)
        set_status("Shell feed updated. Restart the console (or re-launch the Ambilight payload in Set up) to apply it.", false);
    }
}

// ---- autoload: GoldHEN's payload list ----

bool read_autoload_on(void)
{
    size_t len;
    char *buf = read_small_file(GOLDHEN_PAYLOADS_INI, &len);
    if (!buf) return false;
    bool on = payload_autoload_state(buf, len, PAYLOAD_ASSET_NAME) == AUTOLOAD_ON;
    free(buf);
    return on;
}

// Sets this payload's entry in GoldHEN's list to 1 or 0 without touching any other line.
static bool write_autoload(bool on)
{
    size_t len = 0;
    char *old = read_small_file(GOLDHEN_PAYLOADS_INI, &len);   // NULL for a missing or empty file
    const char *src = old ? old : "";

    size_t newLen; bool changed;
    char *text = payload_autoload_apply(src, len, PAYLOAD_AUTORUN_ENTRY, PAYLOAD_ASSET_NAME, on, &newLen, &changed);
    free(old);
    if (!text) return false;

    bool ok = true;
    if (changed) {
        APP_MKDIR(GOLDHEN_DIR);
        ok = write_file_atomic(GOLDHEN_PAYLOADS_INI, text, newLen);
    }
    free(text);
    return ok;
}

// "Autoload at boot" was toggled in Set up. Unlike every other row it takes effect immediately
// (it edits GoldHEN's file, not this app's ini), so the status line says what happened.
void autoload_hint(const char *msg)
{
    snprintf(g_state.settingsHint, sizeof(g_state.settingsHint), "%s", msg);
    g_dirty = true;
}

void toggle_autoload(void)
{
    bool want = !g_cfg.shellAutoload;
    if (want) {
        // Tell the person what is missing, in the order they would fix it.
        if (!g_cfg.shellEnabled) {
            autoload_hint("Autoload needs the Shell feed: turn the Shell feed switch on first.");
            return;
        }
        if (g_state.install == UI_INSTALL_NONE) {
            autoload_hint("Autoload needs the plugin: install it from Home first.");
            return;
        }
        if (g_state.install == UI_INSTALL_DISABLED) {
            autoload_hint("Autoload needs the plugin enabled: enable it from Home first.");
            return;
        }
        if (!g_assetPresent[ASSET_PAYLOAD] || !g_assetPresent[ASSET_SHELL_PRX]) {
            autoload_hint("Autoload needs the Ambilight payload and shell feed files: install them from Home first.");
            return;
        }
    }
    if (!write_autoload(want)) {
        set_status("Couldn't update GoldHEN's payload list -- autoload unchanged.", true);
        return;
    }
    g_cfg.shellAutoload = want ? 1 : 0;
    set_status(want ? "Autoload at boot: On -- the Ambilight payload runs when GoldHEN starts."
                    : "Autoload at boot: Off.", false);
}

// "Shell feed" was toggled in Set up (the value is already flipped in g_cfg).
void on_shell_enabled_changed(void)
{
    rescan_shell_files();
    g_cfg.shellAutoload = read_autoload_on() ? 1 : 0;
    sync_shell_summary();
    g_dirty = true;
    if (g_cfg.shellEnabled) start_background_update_check();
}

// ---- claim: keep the shell feed off the strip while this app is driving it ----

ClaimRun g_claim;

int claim_write_cb(void *ctx, const char *text, size_t len)
{
    (void)ctx;
    // Same open flags as the plugin's writer (O_WRONLY | O_CREAT, deliberately NO O_TRUNC): the
    // record is overwritten in place at offset 0, so a reader never sees an empty file.
    int32_t fd = sceKernelOpen(CLAIM_PATH, 0x0001 | 0x0200, 0777);
    if (fd < 0) return -1;
    long n = sceKernelWrite(fd, text, len);
    sceKernelClose(fd);
    return (n == (long)len) ? 0 : -1;
}

void claim_start(void)
{
    uint32_t seq = 0;
    size_t len;
    char *buf = read_small_file(CLAIM_PATH, &len);
    if (buf) { claim_parse_seq(buf, len, &seq); free(buf); }
    claim_init(&g_claim, seq);
}
