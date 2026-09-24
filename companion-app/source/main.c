// main.c -- ps4_ambient_light companion app.
//
// This build wires the visual layer (ui_*.c) up to real, working
// logic: every field on Set up/Customization reads and writes the
// live AmbientConfig through settings.c's generic get/set-by-offset,
// numeric/string fields open the real on-screen keyboard, Save writes
// the ini the plugin itself reads, the CTA installs or updates the
// real plugin .prx over HTTP, and Set up/Customization/Test strip all
// drive the real WLED strip over DDP using the same wire-order layout
// math the Android app uses.
//
// Almost none of this logic is new. It's a port of a previously
// hardened implementation (see the comments throughout this file that
// say what was actually confirmed, and the couple of real bugs that
// were found and fixed there -- the IME-dialog sysmodule load and the
// DDP offset-0 bug are both explained where they're fixed below) onto
// the new visual layer, not a rewrite from scratch. What changed is
// how a field gets drawn and where its on-screen position comes from
// (ui_screens.c, driven by settings.c's kMenuItems); what a field
// means and how editing it is saved has not.
//
// SDL's role stays deliberately small: create the window, hand over
// its surface, and present it. All drawing goes through UiCanvas.
//
// NOT YET VERIFIED: this file has not been compiled with the real
// OpenOrbis toolchain or run on a PS4. Two interaction choices below
// are new conventions introduced here (not carried over from prior
// art) and are flagged where they're implemented: L1/R1 as a quick
// +-step nudge on the focused field, and Cross as the single
// "activate this field" button for every field type. Both are
// plausible, standard console-UI choices, but neither was exercised
// by the version of this app that was actually run on hardware.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <wchar.h>

#include <SDL2/SDL.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>
#include <orbis/ImeDialog.h>
#include <orbis/CommonDialog.h>

#include "settings.h"
#include "color_pipeline.h"
#include "ddp.h"
#include "relay_signal.h"
#include "layout.h"
#include "ui_canvas.h"
#include "ui_theme.h"
#include "ui_screens.h"
#include "led_frame.h"
#include "ui_widgets.h"   // CONTENT_TOP / CONTENT_H for scroll clamping

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FRAME_WIDTH  1920
#define FRAME_HEIGHT 1080

// Same path the plugin itself reads -- this app edits that exact
// file, not a copy, so there's only ever one source of truth.
#define AMBIENT_CONFIG_PATH "/data/ps4_ambient_light.ini"
#define FONT_DIR "/app0/assets/fonts"

// Where the plugin binary and GoldHEN's own plugin registry live.
// Confirmed via GOLDHEN_PATH in the plugin repo's own common/
// plugin_common.h and multiple independent install-guide sources.
#define GOLDHEN_PLUGINS_DIR "/data/GoldHEN/plugins"
#define GOLDHEN_PLUGINS_INI "/data/GoldHEN/plugins.ini"
#define PLUGIN_PRX_PATH GOLDHEN_PLUGINS_DIR "/ps4_ambient_light.prx"

#define PLUGIN_UPDATE_URL "https://github.com/mitchshimy/PS4-Ambilight/releases/latest/download/ps4_ambient_light.prx"

// ---------------- global state ----------------

static AmbientConfig g_cfg;
static UiState g_state;
static bool g_dirty = true;

// Held so do_plugin_update() can force an interim "Downloading..."
// frame before its blocking network/file I/O -- same reasoning as the
// version of this that ran on hardware: without it, a slow server
// makes the app look completely frozen with zero feedback.
static UiCanvas *g_canvas = NULL;
static const UiFonts *g_fonts = NULL;
static SDL_Surface *g_windowSurface = NULL;
static SDL_Surface *g_canvasSurface = NULL;
static SDL_Window *g_window = NULL;

static void set_status(const char *msg, bool isError)
{
    snprintf(g_state.statusLine, sizeof(g_state.statusLine), "%s", msg);
    g_state.statusIsSaved = false;
    g_state.statusIsError = isError;
    g_dirty = true;
}

// Cached snapshot of ui_render_home_static()'s output -- everything on
// Home except the LED edge frame. Rebuilt every time render_and_present()
// actually runs the full Home draw; reused by render_home_fast() on
// every animation-only frame in between, which is what turns "redraw
// all of Home's text/icons/background at 30fps" into "redraw ~200
// small squares at 30fps". Allocated once in main() once the frame
// size is known.
static uint32_t *g_homeStaticCache = NULL;
static bool g_homeStaticCacheValid = false;

static void render_and_present(void)
{
    if (g_state.screen == UI_SCREEN_HOME) {
        // Drawn in two pieces (not ui_render()) specifically so the
        // static part can be snapshotted before the LED frame goes on
        // top of it -- see ui_render_home_static()'s own comment.
        ui_render_home_static(g_canvas, g_fonts, &g_cfg, &g_state);
        if (g_homeStaticCache) {
            memcpy(g_homeStaticCache, g_canvas->px, (size_t)FRAME_WIDTH * FRAME_HEIGHT * sizeof(uint32_t));
            g_homeStaticCacheValid = true;
        }
        if (g_state.testRunning) {
            led_frame_draw(g_canvas, g_state.ledSlots, g_state.ledSlotCount,
                            g_state.ledColors, g_state.ledColorCount);
        }
    } else {
        ui_render(g_canvas, g_fonts, &g_cfg, &g_state);
        // Left Home -- rebuild the cache from scratch next time we're
        // back rather than trust a snapshot from a potentially
        // different install/toast/focus state.
        g_homeStaticCacheValid = false;
    }
    SDL_BlitSurface(g_canvasSurface, NULL, g_windowSurface, NULL);
    SDL_UpdateWindowSurface(g_window);
}

// The actual fast path: reuse the cached static layer, redraw only the
// (now cheap -- see led_frame.c) LED frame on top of it. Only valid to
// call when g_homeStaticCacheValid and we're still on Home; the main
// loop below is the only caller and it checks both.
static void render_home_fast(void)
{
    if (!g_homeStaticCache) return; // shouldn't happen (see g_homeStaticCacheValid's invariant), but cheap to guard
    memcpy(g_canvas->px, g_homeStaticCache, (size_t)FRAME_WIDTH * FRAME_HEIGHT * sizeof(uint32_t));
    led_frame_draw(g_canvas, g_state.ledSlots, g_state.ledSlotCount,
                    g_state.ledColors, g_state.ledColorCount);
    SDL_BlitSurface(g_canvasSurface, NULL, g_windowSurface, NULL);
    SDL_UpdateWindowSurface(g_window);
}

// ---------------- button auto-repeat ----------------
//
// Holding a D-Pad direction or L1/R1 now fires repeatedly instead of
// once per press -- moving focus, or nudging a numeric field, used to
// need one separate press per step, which made anything more than a
// couple of steps tedious (walking Brightness from 0 to 255 five at a
// time is 51 individual presses). Fires immediately on the initial
// press, waits REPEAT_INITIAL_DELAY_MS before the first repeat (so a
// quick tap never double-fires), then repeats every
// REPEAT_INTERVAL_MS for as long as the button stays held.
//
// Only applied to the 6 buttons where repeated firing is actually
// useful (4 D-Pad directions + L1/R1). Cross, Circle and Options stay
// edge-only (PRESSED()/crossUp below) -- those open a dialog, navigate
// once, or quit, and firing those repeatedly while held would be
// actively wrong (e.g. re-opening the keyboard every 90ms).

typedef struct { bool wasDown; uint32_t pressedAtMs; uint32_t lastRepeatMs; } RepeatState;

#define REPEAT_INITIAL_DELAY_MS 350u
#define REPEAT_INTERVAL_MS      90u

static bool repeat_fire(RepeatState *rs, bool down, uint32_t nowMs)
{
    if (!down) { rs->wasDown = false; return false; }
    if (!rs->wasDown) {
        rs->wasDown = true;
        rs->pressedAtMs = nowMs;
        rs->lastRepeatMs = nowMs;
        return true; // fires immediately on the initial press
    }
    if (nowMs - rs->pressedAtMs < REPEAT_INITIAL_DELAY_MS) return false;
    if (nowMs - rs->lastRepeatMs >= REPEAT_INTERVAL_MS) {
        rs->lastRepeatMs = nowMs;
        return true;
    }
    return false;
}

// ---------------- field navigation ----------------
//
// Row structure comes from ui_screen_field_rows() (ui_screens.c),
// which is the exact same source the card renderer uses -- navigation
// and drawing can't disagree about where a field is, because they're
// built from the same table.
// built from the same table.

static void move_focus_vertical(UiScreenId screen, int fieldCount, int dir)
{
    int rowStart[16], rowCount[16];
    int n = ui_screen_field_rows(screen, rowStart, rowCount, 16);

    int cur = g_state.focusField;
    if (cur >= fieldCount) {
        // On the Save button -- Up goes to the last real row's first
        // column, Down does nothing (nowhere further to go).
        if (dir < 0 && n > 0) g_state.focusField = rowStart[n - 1];
        return;
    }

    for (int r = 0; r < n; r++) {
        if (cur >= rowStart[r] && cur < rowStart[r] + rowCount[r]) {
            int col = cur - rowStart[r];
            int nr = r + dir;
            if (nr < 0) return;
            if (nr >= n) { g_state.focusField = fieldCount; return; } // -> Save button
            if (col >= rowCount[nr]) col = rowCount[nr] - 1;
            g_state.focusField = rowStart[nr] + col;
            return;
        }
    }
}

// Keeps the focused row inside the .content viewport, the way the
// browser's scroll-into-view would. Reaching the Save button scrolls
// all the way to the bottom, since that button lives in the fixed
// save bar below the scrolling content, not in a row within it.
static void scroll_to_focus(const UiFonts *fonts, int fieldCount)
{
    float maxScroll = ui_screen_content_height(fonts, &g_cfg, g_state.screen) - CONTENT_H;
    if (maxScroll < 0.0f) maxScroll = 0.0f;

    if (g_state.focusField >= fieldCount) {
        g_state.scrollY = maxScroll;
        return;
    }

    float top, bottom;
    if (!ui_screen_focus_bounds(fonts, &g_cfg, g_state.screen, g_state.focusField, &top, &bottom))
        return;

    float viewTop = CONTENT_TOP + g_state.scrollY;
    float viewBottom = viewTop + CONTENT_H;
    const float pad = 24.0f;

    if (top - pad < viewTop) g_state.scrollY -= (viewTop - (top - pad));
    else if (bottom + pad > viewBottom) g_state.scrollY += ((bottom + pad) - viewBottom);

    if (g_state.scrollY > maxScroll) g_state.scrollY = maxScroll;
    if (g_state.scrollY < 0.0f) g_state.scrollY = 0.0f;
}

// ---------------- controller ----------------

static int g_padHandle = -1;
static int32_t g_userId = -1; // set once in pad_init(), reused by the IME dialog

static bool pad_init(void)
{
    if (sceUserServiceInitialize(NULL) < 0) { printf("[pad] sceUserServiceInitialize failed\n"); return false; }
    if (sceUserServiceGetInitialUser(&g_userId) < 0) { printf("[pad] sceUserServiceGetInitialUser failed\n"); return false; }
    if (scePadInit() < 0) { printf("[pad] scePadInit failed\n"); return false; }
    g_padHandle = scePadOpen(g_userId, 0, 0, NULL);
    if (g_padHandle < 0) { printf("[pad] scePadOpen failed (%d)\n", g_padHandle); return false; }
    return true;
}

// ---------------- HTTP / plugin install+update ----------------
//
// Verified real APIs, not guessed: SDK's samples/net_http/main.c is a
// real, working https://www.google.com download demo this sequence is
// copied from -- sceSysmoduleLoadModuleInternal for NET/HTTP/SSL,
// sceNetInit/PoolCreate, sceSslInit, sceHttpInit, then the
// template/connection/request chain below.

static int g_libnetMemId = 0, g_libhttpCtxId = 0, g_libsslCtxId = 0;
static bool g_httpReady = false;

static int skip_ssl_callback(int libsslId, unsigned int verifyErr, void *const sslCert[], int certNum, void *userArg)
{
    (void)libsslId; (void)verifyErr; (void)sslCert; (void)certNum; (void)userArg;
    return 1; // accept -- matches the sample; tighten this if certificate pinning matters later
}

static bool http_init(void)
{
    if (g_httpReady) return true;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0) return false;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0) return false;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0) return false;

    sceNetInit();
    int ret = sceNetPoolCreate("ambientAppNetPool", 4 * 1024, 0);
    if (ret < 0) return false;
    g_libnetMemId = ret;

    ret = sceSslInit(256 * 1024);
    if (ret < 0) return false;
    g_libsslCtxId = ret;

    ret = sceHttpInit(g_libnetMemId, g_libsslCtxId, 4 * 1024 * 1024);
    if (ret < 0) return false;
    g_libhttpCtxId = ret;

    g_httpReady = true;
    return true;
}

// Downloads full_url to local_dst. Returns true on a clean 200 +
// complete read.
static bool http_download(const char *full_url, const char *local_dst)
{
    if (!http_init()) return false;

    int tpl = sceHttpCreateTemplate(g_libhttpCtxId, "Mozilla/5.0 (PLAYSTATION 4; 1.00)", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return false;
    sceHttpsSetSslCallback(tpl, skip_ssl_callback, NULL);

    // Bounds how long a hung/unreachable PLUGIN_UPDATE_URL can freeze
    // the app. Confirmed real 2-arg signatures; NOT confirmed against
    // a working call site in this SDK (no sample calls these), so
    // treat as a real, compilable improvement over no timeout at all,
    // not a confirmed-correct one.
    sceHttpSetConnectTimeOut(tpl, 10 * 1000 * 1000);  // 10s
    sceHttpSetResolveTimeOut(tpl, 10 * 1000 * 1000);  // 10s
    sceHttpSetSendTimeOut(tpl, 10 * 1000 * 1000);     // 10s

    bool ok = false;
    int conn = sceHttpCreateConnectionWithURL(tpl, full_url, 1);
    if (conn >= 0) {
        int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, full_url, 0);
        if (req >= 0) {
            if (sceHttpSendRequest(req, NULL, 0) >= 0) {
                int32_t statusCode = 0;
                sceHttpGetStatusCode(req, &statusCode);
                if (statusCode == 200) {
                    // sceKernelOpen/Write/Close, not fopen -- fopen
                    // from an unusual thread context is not reliably
                    // safe in this environment (same fix the plugin
                    // itself needed).
                    int32_t fd = sceKernelOpen(local_dst, 0x200 | 0x001 /* O_TRUNC|O_CREAT */, 0777);
                    if (fd >= 0) {
                        uint8_t buf[64 * 1024];
                        ok = true;
                        for (;;) {
                            int n = sceHttpReadData(req, buf, sizeof(buf));
                            if (n < 0) { ok = false; break; }
                            if (n == 0) break;
                            if (sceKernelWrite(fd, buf, n) != n) { ok = false; break; }
                        }
                        sceKernelClose(fd);
                    }
                }
            }
        }
        if (req >= 0) sceHttpDeleteRequest(req);
    }
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return ok;
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
static UiInstallState plugin_registration_state(void)
{
    if (!plugin_exists()) return UI_INSTALL_NONE;
    return plugin_registered_in_goldhen_ini() ? UI_INSTALL_OK : UI_INSTALL_DISABLED;
}

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

    int32_t wfd = sceKernelOpen(GOLDHEN_PLUGINS_INI, 0x200 | 0x001 /* O_TRUNC|O_CREAT */, 0777);
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
static void do_plugin_update(void)
{
    set_status("Downloading plugin...", false);
    render_and_present();   // one frame of feedback before the blocking I/O below

    const char *tmpPath = "/data/ps4_ambient_light_update.tmp";
    if (!http_download(PLUGIN_UPDATE_URL, tmpPath)) {
        set_status("Download FAILED -- check PLUGIN_UPDATE_URL and network.", true);
        return;
    }

    // Stage the new build alongside the live one, and only ever touch
    // PLUGIN_PRX_PATH itself via sceKernelRename() once the staged
    // copy is fully verified good -- a copy failure partway through
    // never touches the live, working file.
    const char *stagingPath = PLUGIN_PRX_PATH ".new";
    int32_t src = sceKernelOpen(tmpPath, 0, 0777);
    if (src < 0) { set_status("Update failed: couldn't reopen downloaded file.", true); return; }
    int32_t dst = sceKernelOpen(stagingPath, 0x200 | 0x001, 0777);
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
    sceKernelClose(dst);
    if (!copyOk) {
        set_status("Update failed: staging copy incomplete. Your existing plugin is untouched.", true);
        return;
    }

    if (sceKernelRename(stagingPath, PLUGIN_PRX_PATH) < 0) {
        set_status("Update failed: couldn't install staged build. Your existing plugin is untouched.", true);
        return;
    }

    if (!ensure_plugin_registered_in_goldhen()) {
        set_status("Plugin installed, but plugins.ini update failed -- add it manually.", true);
        return;
    }

    g_state.install = UI_INSTALL_OK;
    set_status("Installed. Relaunch your game to load it.", false);
}

// Re-enables a .prx that's already on disk but whose plugins.ini entry
// is commented out or missing (UI_INSTALL_DISABLED) -- just rewrites
// the ini line locally via the same ensure_plugin_registered_in_goldhen()
// used after a fresh install/update, no network download needed.
static void do_plugin_enable(void)
{
    if (!ensure_plugin_registered_in_goldhen()) {
        set_status("Couldn't update plugins.ini -- enable it manually.", true);
        return;
    }
    g_state.install = UI_INSTALL_OK;
    set_status("Plugin enabled. Relaunch your game to load it.", false);
}

// ---------------- live preview / test strip ----------------
//
// Builds one color per *configured* LED in real physical wire order
// (layout_build -- layout.c, a C port of the Android app's
// LedLayoutGeometry.kt) and sends the whole strip over DDP, rather
// than a handful of fixed swatches at DDP offset 0 -- offset 0 is
// always the physical start of the strip, so a fixed-swatch version
// only ever lit whichever few pixels happen to sit right at the
// start, no matter how the LED counts were configured.

static LedSlot g_layoutSlots[LAYOUT_MAX_LEDS];
static int g_layoutCount = 0;
// Must outlive update_live_preview() -- UiState.ledColors (set below)
// borrows this pointer through to render_and_present(), which runs
// later in the same loop iteration; a stack-local buffer here would be
// a dangling pointer by the time ui_render() actually reads it.
static uint8_t g_wireBytes[LAYOUT_MAX_LEDS * 3];

// Advanced by real elapsed time (not an assumed frame duration) once
// per loop iteration in main(). Only actually used to offset colour
// while Test Strip is running (see update_live_preview()) -- letting
// it run continuously the rest of the time is harmless and simpler
// than gating the accumulation itself.
static float g_animSeconds = 0.0f;

// Test Strip cycles between two visually distinct animation modes
// rather than running one pattern forever -- aesthetic choices, not
// something the blueprint specifies (it has no concept of a running
// preview at all).
//
// Phase 1, "rotating rainbow": the same per-position rainbow
// Set up/Customization show at rest, slowly rotated -- the whole
// strip is lit at once, in a fixed spatial arrangement that just
// turns. One revolution every 10 seconds, four revolutions per phase.
//
// Phase 2, "flowing wave": a single saturated band of colour travels
// around the joined perimeter with a fading trail behind it (the rest
// of the strip stays dim), and the band's own hue slowly cycles as it
// travels -- motion you can track with your eye, not just a static
// rainbow shape turning, which is the actual "variety" this is for:
// the two modes don't just differ in speed or colour, they differ in
// *shape* (everywhere-at-once vs. one moving point).
#define TEST_PHASE1_REV_SEC     10.0f                          // seconds per rainbow revolution
#define TEST_PHASE1_ROUNDS      4
#define TEST_PHASE1_DURATION    (TEST_PHASE1_REV_SEC * TEST_PHASE1_ROUNDS)   // 40s
#define TEST_PHASE1_PULSE_HZ    (1.0f / 2.0f)                   // one brightness breath every 2s

#define TEST_PHASE2_LAP_SEC     10.0f                           // seconds per trip around the loop
#define TEST_PHASE2_LAPS        2
#define TEST_PHASE2_DURATION    (TEST_PHASE2_LAP_SEC * TEST_PHASE2_LAPS)     // 20s
#define TEST_PHASE2_HUE_CYCLE_SEC TEST_PHASE2_DURATION          // one full colour cycle per phase
#define TEST_PHASE2_TRAIL_FRAC  0.22f                           // trailing comet covers 22% of the loop
#define TEST_PHASE2_AMBIENT_V   0.05f                           // dim level for the rest of the strip

#define TEST_CYCLE_DURATION (TEST_PHASE1_DURATION + TEST_PHASE2_DURATION)    // 60s, then repeats

static void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf, gf, bf;
    if      (h < 60)  { rf = c; gf = x; bf = 0; }
    else if (h < 120) { rf = x; gf = c; bf = 0; }
    else if (h < 180) { rf = 0; gf = c; bf = x; }
    else if (h < 240) { rf = 0; gf = x; bf = c; }
    else if (h < 300) { rf = x; gf = 0; bf = c; }
    else              { rf = c; gf = 0; bf = x; }
    *r = (uint8_t)((rf + m) * 255.0f);
    *g = (uint8_t)((gf + m) * 255.0f);
    *b = (uint8_t)((bf + m) * 255.0f);
}

// Wraps x into [0, 1) -- used for comparing positions around the
// joined perimeter, which is circular.
static float wrap01(float x)
{
    x = fmodf(x, 1.0f);
    if (x < 0.0f) x += 1.0f;
    return x;
}

static void update_live_preview(void)
{
    colorpipeline_rebuild_perchannel_gamma_luts(&g_cfg);

    g_layoutCount = layout_build(&g_cfg, g_layoutSlots, LAYOUT_MAX_LEDS,
                                  (float)FRAME_WIDTH, (float)FRAME_HEIGHT, 0.0f);
    if (g_layoutCount <= 0) {
        // Nothing configured -- nothing to send, and nothing real to
        // show on the edge frame either. Leaving a stale g_wireBytes
        // pointer in g_state here would have the frame keep showing
        // whatever was last computed under a *previous* config, which
        // is exactly the "doesn't represent it perfectly" complaint
        // this was fixed for -- so explicitly clear it.
        g_state.ledColors = NULL;
        g_state.ledColorCount = 0;
        g_state.ledSlots = NULL;
        g_state.ledSlotCount = 0;
        return;
    }

    // Set up/Customization show a *stable* per-position colour on
    // purpose -- the point there is seeing which physical LED
    // corresponds to which part of the screen, and a colour that kept
    // moving would work against that. Test Strip on Home has the
    // opposite goal (feel like the strip is alive, the way it will be
    // once a game is actually running), so only that state gets any
    // animation at all.
    bool animateForTest = (g_state.screen == UI_SCREEN_HOME && g_state.testRunning);

    float tInCycle = animateForTest ? fmodf(g_animSeconds, TEST_CYCLE_DURATION) : 0.0f;
    bool phase2 = animateForTest && (tInCycle >= TEST_PHASE1_DURATION);
    float tInPhase = phase2 ? (tInCycle - TEST_PHASE1_DURATION) : tInCycle;

    // Phase 2's moving parameters are the same for every LED this
    // frame, so compute them once rather than per-LED.
    float waveHue = fmodf((tInPhase / TEST_PHASE2_HUE_CYCLE_SEC) * 360.0f, 360.0f);
    float wavePos = wrap01(tInPhase / TEST_PHASE2_LAP_SEC);   // 0..1 fraction around the loop

    for (int i = 0; i < g_layoutCount; i++) {
        uint8_t rIn, gIn, bIn;
        float baseHue = layout_hue_for_slot(&g_layoutSlots[i], (float)FRAME_WIDTH, (float)FRAME_HEIGHT, 0.0f);
        float hue = baseHue, v = 1.0f;

        if (animateForTest && !phase2) {
            // Phase 1: rotate the whole rainbow, breathe the brightness.
            hue = fmodf(baseHue + tInPhase * (360.0f / TEST_PHASE1_REV_SEC), 360.0f);
            if (hue < 0.0f) hue += 360.0f;
            v = 0.82f + 0.18f * sinf(tInPhase * 2.0f * (float)M_PI * TEST_PHASE1_PULSE_HZ);
        } else if (animateForTest && phase2) {
            // Phase 2: a single travelling band with a fading trail.
            // basePos reuses baseHue/360 -- layout_hue_for_slot()
            // already maps position around the joined perimeter to
            // 0-360, so dividing back by 360 recovers that same 0..1
            // position without a second implementation of the same
            // perimeter math.
            float basePos = baseHue / 360.0f;
            float behind = wrap01(wavePos - basePos);   // 0 at the wave's head, grows behind it
            if (behind <= TEST_PHASE2_TRAIL_FRAC) {
                v = 1.0f - (behind / TEST_PHASE2_TRAIL_FRAC);
                v = TEST_PHASE2_AMBIENT_V + (1.0f - TEST_PHASE2_AMBIENT_V) * v;
            } else {
                v = TEST_PHASE2_AMBIENT_V;
            }
            hue = waveHue;
        }

        hsv_to_rgb(hue, 1.0f, v, &rIn, &gIn, &bIn);
        colorpipeline_process(&g_cfg, rIn, gIn, bIn, &g_wireBytes[i * 3]);
    }

    // The edge frame shows these bytes regardless of whether the DDP
    // send below actually reaches the strip -- they're an accurate
    // preview of what this configuration *produces*, which stays
    // meaningful even while WLED Host is unreachable (e.g. while
    // tuning Customization settings offline).
    g_state.ledColors = g_wireBytes;
    g_state.ledColorCount = g_layoutCount;
    g_state.ledSlots = g_layoutSlots;
    g_state.ledSlotCount = g_layoutCount;

    bool sent = ddp_send_rgb_zones(g_cfg.wledHost, g_cfg.wledPort, g_wireBytes, g_layoutCount);

    // Only touch the status line on the transition into/out of
    // failure -- this runs every frame, so without throttling, a bad
    // WLED Host would permanently overwrite every other status
    // message (save confirmations, update results) the instant it
    // happened.
    static bool s_wasSending = true; // mismatched on purpose so a bad host present at startup is reported too
    if (sent != s_wasSending) {
        if (!sent) {
            char msg[256];
            snprintf(msg, sizeof(msg), "WLED Host \"%s\" isn't a valid IPv4 address -- live output paused.", g_cfg.wledHost);
            set_status(msg, true);
        }
        s_wasSending = sent;
    }
}

// ---------------- on-screen keyboard ----------------
//
// Real API surface pulled from orbis/ImeDialog.h, orbis/_types/
// ime_dialog.h and orbis/CommonDialog.h -- see the extensive
// verification notes this was ported from for exactly what's
// confirmed vs. assumed (posx/posy units, whether NULL is a valid
// second sceImeDialogInit argument, whether ORBIS_TYPE_NUMBER's layout
// includes '-' for negative fields). None of that re-verified this
// session; ported as-is.

static bool g_imeDialogOpen = false;
static wchar_t g_imeBuffer[64];
static int g_imeItemIndex = -1; // which kMenuItems[] entry this dialog is editing

static void widen_ascii(wchar_t *dst, const char *src, size_t dstCount)
{
    size_t i = 0;
    for (; i < dstCount - 1 && src[i] != '\0'; i++) dst[i] = (wchar_t)(unsigned char)src[i];
    dst[i] = L'\0';
}

// itemIndex must refer to a FIELD_STRING, FIELD_U32, FIELD_I32, or
// FIELD_U16 item -- FIELD_ENUM/FIELD_BOOL are cycled/toggled directly
// by Cross instead (see handle_settings_input()).
static void open_field_ime_dialog(int itemIndex)
{
    if (g_imeDialogOpen || sceCommonDialogIsUsed()) return;
    const MenuItem *item = &kMenuItems[itemIndex];

    char placeholderAscii[80], titleAscii[80], currentAscii[64];
    if (item->type == FIELD_STRING) {
        snprintf(placeholderAscii, sizeof(placeholderAscii), "192.168.x.x");
        snprintf(currentAscii, sizeof(currentAscii), "%s", g_cfg.wledHost);
    } else {
        snprintf(placeholderAscii, sizeof(placeholderAscii), "%d to %d", item->min, item->max);
        snprintf(currentAscii, sizeof(currentAscii), "%d", settings_get_i32(&g_cfg, item));
    }
    snprintf(titleAscii, sizeof(titleAscii), "%s", item->label);

    static wchar_t placeholderBuf[80], titleBuf[80];
    widen_ascii(placeholderBuf, placeholderAscii, sizeof(placeholderBuf) / sizeof(placeholderBuf[0]));
    widen_ascii(titleBuf, titleAscii, sizeof(titleBuf) / sizeof(titleBuf[0]));

    memset(g_imeBuffer, 0, sizeof(g_imeBuffer));
    widen_ascii(g_imeBuffer, currentAscii, sizeof(g_imeBuffer) / sizeof(g_imeBuffer[0]));

    OrbisImeDialogSetting setting;
    memset(&setting, 0, sizeof(setting));
    setting.userId = g_userId;
    setting.type = (item->type == FIELD_STRING) ? ORBIS_TYPE_BASIC_LATIN : ORBIS_TYPE_NUMBER;
    setting.supportedLanguages = 0;
    setting.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT;
    setting.inputMethod = ORBIS__DEFAULT;
    setting.filter = NULL;
    setting.option = 0;
    setting.maxTextLength = (uint32_t)(sizeof(g_imeBuffer) / sizeof(g_imeBuffer[0]) - 1);
    setting.inputTextBuffer = g_imeBuffer;
    setting.posx = FRAME_WIDTH / 2.0f;
    setting.posy = FRAME_HEIGHT / 2.0f;
    setting.horizontalAlignment = ORBIS_H_CENTER;
    setting.verticalAlignment = ORBIS_V_CENTER;
    setting.placeholder = placeholderBuf;
    setting.title = titleBuf;

    int32_t ret = sceImeDialogInit(&setting, NULL);
    printf("[ime] sceImeDialogInit() = %d (field \"%s\")\n", ret, item->label);
    if (ret == 0) {
        g_imeDialogOpen = true;
        g_imeItemIndex = itemIndex;
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Couldn't open the keyboard (sceImeDialogInit = %d).", ret);
        set_status(msg, true);
    }
}

// Call once per frame from the main loop while g_imeDialogOpen.
static void update_ime_dialog(void)
{
    OrbisDialogStatus status = sceImeDialogGetStatus();
    if (status != ORBIS_DIALOG_STATUS_STOPPED) return;

    OrbisDialogResult result;
    memset(&result, 0, sizeof(result));
    sceImeDialogGetResult(&result);

    const MenuItem *item = (g_imeItemIndex >= 0 && g_imeItemIndex < kMenuItemCount) ? &kMenuItems[g_imeItemIndex] : NULL;

    if (result.endstatus == ORBIS_DIALOG_OK && item != NULL) {
        char typed[64];
        size_t i = 0;
        for (; i < sizeof(typed) - 1 && g_imeBuffer[i] != L'\0'; i++) typed[i] = (char)g_imeBuffer[i];
        typed[i] = '\0';

        char msg[160];
        if (item->type == FIELD_STRING) {
            char *dst = (char *)&g_cfg + item->offset;
            snprintf(dst, (size_t)item->max /* buffer size, see STRBUF() in settings.c */, "%s", typed);
            snprintf(msg, sizeof(msg), "%s set to %s.", item->label, dst);
            set_status(msg, false);
        } else {
            // strtol, not atoi: atoi can't distinguish "not a number"
            // from a real 0, and silently writing 0 for garbage input
            // is exactly the kind of accepted-but-wrong save
            // settings_load()'s own hardening was written to prevent
            // on the load side.
            char *end = NULL;
            long v = strtol(typed, &end, 10);
            if (end == typed || *end != '\0') {
                snprintf(msg, sizeof(msg), "\"%s\" isn't a number -- %s unchanged.", typed, item->label);
                set_status(msg, true);
            } else {
                settings_set_i32(&g_cfg, item, (int32_t)v); // clamps into [item->min, item->max]
                snprintf(msg, sizeof(msg), "%s set to %d.", item->label, settings_get_i32(&g_cfg, item));
                set_status(msg, false);
            }
        }
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Keyboard cancelled -- %s unchanged.", item ? item->label : "field");
        set_status(msg, false);
    }

    sceImeDialogTerm();
    g_imeDialogOpen = false;
    g_imeItemIndex = -1;
    g_dirty = true;
}

// ---------------- Home input ----------------

static void handle_home_input(bool up, bool down, bool left, bool right, bool cross)
{
    if (up && g_state.homeFocus != HOME_FOCUS_CTA) { g_state.homeFocus = HOME_FOCUS_CTA; g_dirty = true; }
    else if (down && g_state.homeFocus == HOME_FOCUS_CTA) { g_state.homeFocus = HOME_FOCUS_HELP; g_dirty = true; }
    else if (left && g_state.homeFocus > HOME_FOCUS_HELP) { g_state.homeFocus--; g_dirty = true; }
    else if (right && g_state.homeFocus >= HOME_FOCUS_HELP && g_state.homeFocus < HOME_FOCUS_CUST) { g_state.homeFocus++; g_dirty = true; }

    if (up || down || left || right) g_state.toastVisible = false;

    if (!cross) return;

    switch (g_state.homeFocus) {
        case HOME_FOCUS_CTA:
            if (g_state.install == UI_INSTALL_DISABLED) {
                do_plugin_enable();
            } else if (g_state.install != UI_INSTALL_OK) {
                do_plugin_update();
            } else if (g_state.testRunning) {
                g_state.testRunning = false;
                set_status("Test strip stopped.", false);
            } else if (!g_state.setupComplete) {
                g_state.toastVisible = true;
            } else {
                g_state.testRunning = true;
                char msg[128];
                snprintf(msg, sizeof(msg), "Test strip running -- sending live colours to %s.", g_cfg.wledHost);
                set_status(msg, false);
            }
            break;
        case HOME_FOCUS_SETUP:
            g_state.screen = UI_SCREEN_SETUP;
            g_state.focusField = 0;
            g_state.scrollY = 0.0f;
            break;
        case HOME_FOCUS_CUST:
            g_state.screen = UI_SCREEN_CUSTOMIZATION;
            g_state.focusField = 0;
            g_state.scrollY = 0.0f;
            break;
        default:
            break;
    }
    g_dirty = true;
}

// ---------------- Settings screen input ----------------

// How long the Save button shows its "Saved!" confirmation state
// (ui_screens.c: UiState.saveJustConfirmed) before reverting to
// normal. Long enough to register, short enough not to feel stuck.
#define SAVE_CONFIRM_DURATION_MS 1600u
static uint32_t g_saveConfirmUntilMs = 0;

static void do_save(void)
{
    if (settings_save(&g_cfg, AMBIENT_CONFIG_PATH)) {
        snprintf(g_state.statusLine, sizeof(g_state.statusLine),
                 "%s -- the plugin picks this up on its own reload check.", AMBIENT_CONFIG_PATH);
        g_state.statusIsSaved = true;
        g_state.statusIsError = false;
        g_saveConfirmUntilMs = SDL_GetTicks() + SAVE_CONFIRM_DURATION_MS;
    } else {
        set_status("Save FAILED -- check the config path is writable.", true);
    }
    g_dirty = true;
}

// Nudges/cycles/toggles the focused field by one step without opening
// the keyboard. New in this pass (see the file header) -- L1/R1 were
// never used by the version of this app that ran on hardware.
static void nudge_field(const MenuItem *item, int dir)
{
    if (item->type == FIELD_STRING) return; // no numeric nudge for an IP
    int32_t cur = settings_get_i32(&g_cfg, item);
    int32_t next;
    if (item->type == FIELD_BOOL) {
        next = cur ? 0 : 1;
    } else if (item->type == FIELD_ENUM) {
        next = cur + dir;
        if (next < item->min) next = item->max;
        if (next > item->max) next = item->min;
    } else {
        next = cur + dir * item->step;
    }
    settings_set_i32(&g_cfg, item, next);

    char msg[128];
    if (item->type == FIELD_BOOL) snprintf(msg, sizeof(msg), "%s: %s.", item->label, next ? "On" : "Off");
    else snprintf(msg, sizeof(msg), "%s: %d.", item->label, settings_get_i32(&g_cfg, item));
    set_status(msg, false);
}

static void handle_settings_input(bool up, bool down, bool left, bool right,
                                   bool cross, bool circle, bool l1, bool r1)
{
    MenuScreen menuScreen = (g_state.screen == UI_SCREEN_SETUP) ? MENU_SCREEN_SETUP : MENU_SCREEN_CUSTOMIZE;
    int start, fieldCount;
    ui_screen_item_range(menuScreen, &start, &fieldCount);

    bool moved = false;
    if (left && g_state.focusField > 0 && g_state.focusField < fieldCount) { g_state.focusField--; moved = true; }
    else if (right && g_state.focusField < fieldCount - 1) { g_state.focusField++; moved = true; }
    else if (up) { move_focus_vertical(g_state.screen, fieldCount, -1); moved = true; }
    else if (down) { move_focus_vertical(g_state.screen, fieldCount, +1); moved = true; }

    if (moved) { scroll_to_focus(g_fonts, fieldCount); g_dirty = true; }

    bool onSaveButton = (g_state.focusField >= fieldCount);

    if (cross) {
        if (onSaveButton) {
            do_save();
        } else {
            const MenuItem *item = &kMenuItems[start + g_state.focusField];
            if (item->type == FIELD_BOOL) nudge_field(item, +1);
            else if (item->type == FIELD_ENUM) nudge_field(item, +1);
            else open_field_ime_dialog(start + g_state.focusField);
        }
        g_dirty = true;
    } else if ((l1 || r1) && !onSaveButton) {
        const MenuItem *item = &kMenuItems[start + g_state.focusField];
        nudge_field(item, r1 ? +1 : -1);
        g_dirty = true;
    }

    if (circle) {
        g_state.screen = UI_SCREEN_HOME;
        g_state.scrollY = 0.0f;
        g_dirty = true;
    }
}

// ---------------- Entry point ----------------

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (!settings_load(&g_cfg, AMBIENT_CONFIG_PATH)) {
        settings_set_defaults(&g_cfg);
        printf("[main] no config at %s -- using defaults\n", AMBIENT_CONFIG_PATH);
    }
    colorpipeline_rebuild_perchannel_gamma_luts(&g_cfg);

    // Not SDL_INIT_JOYSTICK: input comes from the native scePad API
    // below, and letting SDL's joystick layer claim the same HID
    // device was the cause of a real "highlight never moves" bug on
    // hardware.
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("[main] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("PS4 Ambilight", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                           FRAME_WIDTH, FRAME_HEIGHT, 0);
    if (!window) { printf("[main] SDL_CreateWindow failed: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    g_window = window;

    // This SDL port needs the window surface, not an accelerated
    // renderer -- confirmed against OpenOrbis's own samples/SDL2. The
    // UI is composited into our own ARGB buffer and blitted here, so
    // no SDL_Renderer is created at all.
    SDL_Surface *windowSurface = SDL_GetWindowSurface(window);
    g_windowSurface = windowSurface;

    UiCanvas *canvas = ui_canvas_create(FRAME_WIDTH, FRAME_HEIGHT);
    if (!canvas) { printf("[main] canvas allocation failed\n"); SDL_Quit(); return 1; }
    g_canvas = canvas;

    g_homeStaticCache = (uint32_t *)malloc((size_t)FRAME_WIDTH * FRAME_HEIGHT * sizeof(uint32_t));
    if (!g_homeStaticCache) {
        // Not fatal -- render_and_present() and render_home_fast()
        // both already check for NULL and simply skip the caching
        // optimization if it's missing, falling back to a full redraw
        // every frame (correct, just not fast). Worth knowing about if
        // it happens, since it means Test Strip will be as sluggish as
        // before this fix.
        printf("[main] home static cache allocation failed -- Test Strip will redraw fully every frame\n");
    }

    SDL_Surface *canvasSurface = SDL_CreateRGBSurfaceWithFormatFrom(
        canvas->px, FRAME_WIDTH, FRAME_HEIGHT, 32, FRAME_WIDTH * 4, SDL_PIXELFORMAT_ARGB8888);
    g_canvasSurface = canvasSurface;

    UiFonts fonts;
    if (!ui_fonts_load(&fonts, FONT_DIR)) {
        printf("[main] font load failed from %s -- text will be missing\n", FONT_DIR);
    }
    g_fonts = &fonts;

    if (!pad_init()) printf("[main] controller not available\n");

    // Required before any sce*Dialog call. BUGFIX carried over from
    // real hardware testing of the version this was ported from: the
    // crash was PRX_NOT_RESOLVED_FUNCTION, Required Module Name
    // libSceImeDialog, happening the instant Cross was pressed on a
    // string field. Linking -lSceImeDialog resolves the symbol at
    // link time; the real system .sprx module still has to be loaded
    // into the running process at runtime before calls into it work --
    // the same class of bug the earlier putty.log fix (the
    // __ORBIS__ guard that silently dropped the FreeType sysmodule
    // load) turned out to be. ORBIS_SYSMODULE_IME_DIALOG loads via the
    // plain sceSysmoduleLoadModule (not the _Internal variant --
    // that's a different enum family than Net/Http/Ssl/CommonDialog
    // use); ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG loads via
    // sceSysmoduleLoadModuleInternal, same as Net/Http/Ssl above.
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG) < 0) {
        printf("[main] sceSysmoduleLoadModuleInternal(COMMON_DIALOG) failed\n");
    }
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG) < 0) {
        printf("[main] sceSysmoduleLoadModule(IME_DIALOG) failed\n");
    }
    int commonDialogRet = sceCommonDialogInitialize();
    printf("[main] sceCommonDialogInitialize() = %d\n", commonDialogRet);

    memset(&g_state, 0, sizeof(g_state));
    g_state.screen = UI_SCREEN_HOME;
    g_state.install = plugin_registration_state();
    g_state.setupComplete = g_cfg.wledHost[0] != '\0' &&
        (g_cfg.ledCountTop + g_cfg.ledCountRight + g_cfg.ledCountBottom + g_cfg.ledCountLeft) > 0;
    g_state.homeFocus = HOME_FOCUS_CTA;
    g_state.focusField = 0;
    set_status(g_state.install == UI_INSTALL_OK ? "Ready." :
               g_state.install == UI_INSTALL_DISABLED ? "Plugin installed but disabled -- enable it below." :
               "Plugin not installed yet.", false);

    OrbisPadData pad, prevPad;
    memset(&pad, 0, sizeof(pad));
    memset(&prevPad, 0, sizeof(prevPad));

    RepeatState rsUp, rsDown, rsLeft, rsRight, rsL1, rsR1;
    memset(&rsUp, 0, sizeof(rsUp));
    memset(&rsDown, 0, sizeof(rsDown));
    memset(&rsLeft, 0, sizeof(rsLeft));
    memset(&rsRight, 0, sizeof(rsRight));
    memset(&rsL1, 0, sizeof(rsL1));
    memset(&rsR1, 0, sizeof(rsR1));

    uint32_t lastTicksMs = SDL_GetTicks();

    bool running = true;
    while (running) {
        uint32_t nowMs = SDL_GetTicks();
        g_animSeconds += (nowMs - lastTicksMs) / 1000.0f;
        lastTicksMs = nowMs;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
        }

        if (g_padHandle >= 0) {
            prevPad = pad;
            scePadReadState(g_padHandle, &pad);

            // While the on-screen keyboard is up, this app's own
            // D-Pad-driven input has to sit out that frame so it's
            // not fighting the system dialog for the same presses
            // underneath it.
            bool commonDialogUsed = sceCommonDialogIsUsed();

            // D-Pad + L1/R1: held-repeat (see repeat_fire() above).
            bool up    = repeat_fire(&rsUp,    (pad.buttons & ORBIS_PAD_BUTTON_UP) != 0,    nowMs);
            bool down  = repeat_fire(&rsDown,  (pad.buttons & ORBIS_PAD_BUTTON_DOWN) != 0,  nowMs);
            bool left  = repeat_fire(&rsLeft,  (pad.buttons & ORBIS_PAD_BUTTON_LEFT) != 0,  nowMs);
            bool right = repeat_fire(&rsRight, (pad.buttons & ORBIS_PAD_BUTTON_RIGHT) != 0, nowMs);
            bool l1    = repeat_fire(&rsL1,    (pad.buttons & ORBIS_PAD_BUTTON_L1) != 0,    nowMs);
            bool r1    = repeat_fire(&rsR1,    (pad.buttons & ORBIS_PAD_BUTTON_R1) != 0,    nowMs);

            // Cross, Circle, Options: edge-only, on purpose (see the
            // comment above repeat_fire()).
            #define PRESSED(b) ((pad.buttons & (b)) && !(prevPad.buttons & (b)))
            bool circle  = PRESSED(ORBIS_PAD_BUTTON_CIRCLE);
            bool options = PRESSED(ORBIS_PAD_BUTTON_OPTIONS);
            // Cross fires on release, not press: opening the keyboard
            // on the press edge left a stale press for the IME dialog
            // to consume as an immediate Cancel before any real input
            // could register -- a real, previously-hit bug, not a
            // hypothetical one.
            bool crossUp = !(pad.buttons & ORBIS_PAD_BUTTON_CROSS) && (prevPad.buttons & ORBIS_PAD_BUTTON_CROSS);
            #undef PRESSED

            if (options) running = false;

            if (!commonDialogUsed) {
                if (g_state.screen == UI_SCREEN_HOME)
                    handle_home_input(up, down, left, right, crossUp);
                else
                    handle_settings_input(up, down, left, right, crossUp, circle, l1, r1);
            }
        }

        if (g_imeDialogOpen) update_ime_dialog();

        // wled-relay hand-off signal: this app has no idle/Home-only
        // mode for this purpose -- "driving the strip" here just means
        // "this app is running with Relay Signal enabled", independent
        // of which screen is up or whether Test Strip is running. Fire
        // only on a real transition (comparing against last frame's
        // value), same as v17 had it -- covers app startup (already-
        // enabled config fires "on" on the very first frame, since the
        // sentinel below starts false), a live toggle, and nothing
        // else. No internal opt-in gate inside
        // relay_send_external_source() itself on purpose: the call
        // site only ever fires on a real transition, so a user who
        // never enables it can never produce one, and a gate that
        // re-checked the same flag that just changed would silently
        // swallow the "off" send on a true->false transition -- the
        // exact bug this avoids by construction, same as the real
        // plugin's own fix.
        static bool s_relayWasActive = false;
        if (g_cfg.relaySignalEnabled != s_relayWasActive) {
            relay_send_external_source(g_cfg.relayHost, g_cfg.relayPort, g_cfg.relaySignalEnabled != 0);
            s_relayWasActive = g_cfg.relaySignalEnabled != 0;
        }

        // Set up/Customization always keep the live preview running,
        // since their own LED edge frame is always on screen there.
        // Home only runs it -- and therefore only actually sends
        // anything over DDP -- while Test Strip is active, so sitting
        // on Home doesn't silently drive the real strip.
        if (g_state.screen != UI_SCREEN_HOME || g_state.testRunning) {
            update_live_preview();
        }

        g_state.saveJustConfirmed = (nowMs < g_saveConfirmUntilMs);

        // Normally the app only redraws when something actually
        // changed (g_dirty), which is what lets it sit idle between
        // presses instead of burning CPU on an unchanging screen. Two
        // things need a continuously *moving* picture with no input at
        // all, though: the Test Strip animation, and the Save
        // button's confirmation window counting down -- both need to
        // keep redrawing every frame for their own duration, not just
        // on the frame that started them.
        bool onHomeTest = (g_state.screen == UI_SCREEN_HOME && g_state.testRunning);
        bool needsContinuousRedraw = onHomeTest || g_state.saveJustConfirmed;

        if (g_dirty) {
            // Something actually changed (focus moved, install state
            // flipped, a field was edited, ...) -- a full redraw,
            // which also refreshes the Home cache below reuses.
            render_and_present();
            g_dirty = false;
        } else if (onHomeTest && g_homeStaticCacheValid) {
            // The only thing that changed this frame is the
            // animation -- reuse the cached static layer and redraw
            // just the LED frame on top of it, instead of repainting
            // all of Home again for content that's identical to last
            // frame. This is the actual fix for Test Strip feeling
            // sluggish: previously every one of these ~30-times-a-
            // second frames repainted the background glow, the
            // header, the banner, the CTA, the nav tiles and the
            // footer, none of which had changed at all.
            render_home_fast();
        } else if (needsContinuousRedraw) {
            // Continuous redraw is needed but the fast path isn't
            // available yet (e.g. the very first animation frame,
            // before the cache exists) -- fall back to a full render,
            // which populates the cache for every subsequent frame.
            render_and_present();
        }

        usleep(1000000 / 30);
    }

    if (g_imeDialogOpen) sceImeDialogTerm();
    // Defensive final "off" if the signal was last left on -- this app
    // has no plugin_unload-style guaranteed teardown hook the way the
    // real plugin does, so this is the closest equivalent (same as v17).
    if (g_cfg.relaySignalEnabled) {
        relay_send_external_source(g_cfg.relayHost, g_cfg.relayPort, false);
    }
    ui_fonts_destroy(&fonts);
    free(g_homeStaticCache);
    SDL_FreeSurface(canvasSurface);
    ui_canvas_destroy(canvas);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
