// main.c -- ps4_ambient_light companion app.
//
// Verified real APIs used throughout, not guessed: SDL2 init pattern
// (SDL_INIT_VIDEO|SDL_INIT_JOYSTICK, SDL_CreateWindow 1920x1080,
// SDL_JoystickOpen(0)) copied from OpenOrbis's own samples/SDL2
// example. HTTP/SSL/Net init and download sequence copied from
// samples/net_http/main.c, which is a real, working
// https://www.google.com download demo -- not assumed to work from
// header inspection alone. File I/O (config.c) already proven inside
// ps4_ambient_light itself.
//
// NOT verified by this session: this file has not been compiled with
// the real OpenOrbis toolchain or run on a PS4. Only settings.c,
// config.c, and color_pipeline.c (the pure-logic pieces) have been
// isolated and tested with plain gcc on a Linux sandbox. SDL2
// rendering, controller input, and the HTTP download's real behavior
// on this specific PS4/network path all still need that first real
// build+run to confirm. See ps4-ambient-light-handoff.md for this
// session's full account.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <SDL2/SDL.h>
// SDL2_ttf was never actually linkable in this SDK snapshot (header
// only, no compiled lib -- see build.bat's original comment for the
// full story). Text now renders through text_render.h/.c, a small
// glyph-atlas renderer built directly on SceFreeType (confirmed
// present, already linked via -lSceFreeType), not through SDL2_ttf.
#include "text_render.h"

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
// On-screen keyboard for the wledHost field. Real signatures pulled
// directly from these SDK headers, not written from memory -- see the
// comment above open_field_ime_dialog() for exactly what's
// confirmed vs. assumed (no working IME-dialog sample exists in this
// SDK to check against; samples/keyboard is a *physical* USB/BT
// keyboard API, orbis/Keyboard.h, a different thing entirely, despite
// this project's own earlier README implying otherwise).
#include <orbis/ImeDialog.h>
#include <orbis/CommonDialog.h>
#include <wchar.h>

#include <math.h>

#include "settings.h"
#include "color_pipeline.h"
#include "ddp.h"
#include "config.h"
#include "layout.h"

#define FRAME_WIDTH  1920
#define FRAME_HEIGHT 1080

// On-screen rectangle the full-strip layout preview is drawn into --
// shared between update_live_preview() (which needs it to compute LED
// positions via layout_build) and render_setup_screen() (which draws
// into the same rect), so the two never disagree about where things
// are.
#define PREVIEW_X 900
#define PREVIEW_Y 90
#define PREVIEW_W (FRAME_WIDTH - PREVIEW_X - 60)
#define PREVIEW_H 560
#define PREVIEW_PAD 26

// Same path the plugin itself reads -- this app edits that exact
// file, not a copy, so there's only ever one source of truth.
#define AMBIENT_CONFIG_PATH "/data/ps4_ambient_light.ini"

// Where the plugin binary and GoldHEN's own plugin registry live.
// Confirmed via GOLDHEN_PATH in the plugin repo's own common/
// plugin_common.h and multiple independent install-guide sources
// (handoff notes on GoldHEN's built-in FTP server / plugins.ini).
#define GOLDHEN_PLUGINS_DIR "/data/GoldHEN/plugins"
#define GOLDHEN_PLUGINS_INI "/data/GoldHEN/plugins.ini"
#define PLUGIN_PRX_PATH GOLDHEN_PLUGINS_DIR "/ps4_ambient_light.prx"

// CHANGE THIS to your actual repo's raw release asset URL before
// building. Left as a real-shaped placeholder rather than something
// obviously fake, so it's easy to spot and swap, not easy to miss.
#define PLUGIN_UPDATE_URL "https://github.com/YOUR_USERNAME/YOUR_REPO/releases/latest/download/ps4_ambient_light.prx"

// ---------------- Menu state ----------------

typedef enum { SCREEN_HOME, SCREEN_SETUP, SCREEN_CUSTOMIZE, SCREEN_UPDATE, SCREEN_QUIT } Screen;

// kMenuItems is ordered with every MENU_SCREEN_SETUP item first, then
// every MENU_SCREEN_CUSTOMIZE item (see settings.c's own comment above
// the table) -- so each screen's items are one contiguous run, and all
// the list/scroll code below only needs a [start,count) range per
// screen rather than filtering scattered indices every frame.
static void screen_item_range(MenuScreen screen, int *outStart, int *outCount)
{
    int start = -1, count = 0;
    for (int i = 0; i < kMenuItemCount; i++) {
        if (kMenuItems[i].screen == screen) {
            if (start < 0) start = i;
            count++;
        }
    }
    *outStart = (start < 0) ? 0 : start;
    *outCount = count;
}

static AmbientConfig g_cfg;
static int g_selectedIndex = 0;
// Index of the first menu item drawn -- lets the list scroll once there
// are more items than fit on screen. See
// settings_last_visible_index_for() and its call site in
// handle_grouped_settings_input() for how this stays in sync with
// g_selectedIndex, and render_grouped_settings_list() for where it's
// consumed. Shared across both Set Up and Customisation -- reset to
// that screen's first item whenever Home routes into one (see
// handle_home_input()).
static int g_scrollOffset = 0;
static Screen g_screen = SCREEN_HOME;
// Which control has focus on the Home screen: 0 = the big
// Install/Update CTA, 1 = Help, 2 = Set Up, 3 = Customisation --
// mirrors the reference screenshot's layout (one big button above a
// row of three smaller ones).
static int g_homeFocus = 0;
static char g_statusLine[256] = "";
// Set once in main() right after SDL_CreateWindow. Needed by
// do_plugin_update() so it can present one real frame showing
// "Downloading..." before the blocking network/file I/O that follows
// it -- see that function for why.
static SDL_Window *g_window = NULL;

// A second, larger text atlas used only for page titles ("PS4 Ambient
// Light", "Update Plugin") -- text_renderer_create() rasterizes one
// fixed pixelHeight per instance, so a visually distinct "big bold
// title" size (matching the reference screenshots' page headers) needs
// its own instance rather than reusing the 24px body-text atlas.
// Global rather than threaded through every render_*() signature,
// same pattern g_window already uses -- created once in main(),
// destroyed once at shutdown, read (never written) everywhere else.
static TextRenderer *g_titleFont = NULL;

// Visual design tokens -- ported from a reference Android TV UI
// (dark navy background, mint/teal accent, rounded card panels) the
// user shared as screenshots. Kept as named constants, and declared
// this early in the file (rather than down by draw_text/draw_card
// where they're mostly used) so functions like do_plugin_update()
// that render mid-operation feedback before the main loop starts can
// use them too.
static const SDL_Color COL_BG           = {10, 14, 22, 255};
static const SDL_Color COL_CARD_BG      = {19, 27, 40, 255};
static const SDL_Color COL_CARD_BORDER  = {40, 53, 76, 255};
static const SDL_Color COL_FIELD_BG     = {12, 18, 28, 255};
static const SDL_Color COL_FIELD_BORDER = {44, 57, 78, 255};
static const SDL_Color COL_TEXT_PRIMARY = {240, 244, 248, 255};
static const SDL_Color COL_TEXT_SECOND  = {148, 160, 180, 255};
static const SDL_Color COL_ACCENT       = {53, 221, 196, 255};   // primary teal
static const SDL_Color COL_ACCENT_TEXT  = {10, 22, 26, 255};     // text drawn ON the teal fill
static const SDL_Color COL_SELECT_WASH  = {24, 46, 46, 255};     // selected-row background

// Forward declaration: render_update_screen() is defined later, in the
// Rendering section below, but do_plugin_update() (HTTP self-update,
// right below) needs to call it before doing so.
static void render_update_screen(SDL_Renderer *renderer, TextRenderer *font);

// A handful of representative test colors, kept for the small
// pipeline-correctness swatches (gamma/contrast/saturation/levels/
// color order) that are still drawn on screen -- but these are no
// longer what's sent to the real strip. See "Live preview" below for
// the full-strip layout preview that replaced the old 5-swatches-at-
// DDP-offset-0 approach.
#define NUM_PREVIEW_SWATCHES 5
static const uint8_t kPreviewInputs[NUM_PREVIEW_SWATCHES][3] = {
    {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}, {128, 128, 128}
};

// Full-strip layout state, rebuilt every frame in update_live_preview()
// and reused by render_setup_screen() so the on-screen boxes and the
// buffer actually sent over DDP always agree with each other and with
// real physical wire order (see layout.c).
static LedSlot g_layoutSlots[LAYOUT_MAX_LEDS];
static uint8_t g_layoutRGB[LAYOUT_MAX_LEDS][3]; // true RGB (colorOrder-independent), for on-screen drawing only
static int g_layoutCount = 0;

// Simple HSV->RGB, used to give every configured LED in the layout
// preview a distinct color (a full hue sweep around the strip)
// instead of a handful of fixed swatches -- this is what actually
// lets you confirm startCorner/direction/ledOffset match your real
// physical install, since you can see the whole loop light up in
// order, not just whether pixel 0 is lit.
static void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    h = fmodf(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf, gf, bf;
    if      (h <  60.0f) { rf = c; gf = x; bf = 0; }
    else if (h < 120.0f) { rf = x; gf = c; bf = 0; }
    else if (h < 180.0f) { rf = 0; gf = c; bf = x; }
    else if (h < 240.0f) { rf = 0; gf = x; bf = c; }
    else if (h < 300.0f) { rf = x; gf = 0; bf = c; }
    else                  { rf = c; gf = 0; bf = x; }
    *r = (uint8_t)((rf + m) * 255.0f);
    *g = (uint8_t)((gf + m) * 255.0f);
    *b = (uint8_t)((bf + m) * 255.0f);
}

// ---------------- HTTP self-update (verified pattern from
// OpenOrbis samples/net_http/main.c) ----------------

static int g_libnetMemId = 0, g_libhttpCtxId = 0, g_libsslCtxId = 0;
static bool g_httpReady = false;

static int skip_ssl_callback(int libsslId, unsigned int verifyErr, void *const sslCert[], int certNum, void *userArg)
{
    (void)libsslId; (void)verifyErr; (void)sslCert; (void)certNum; (void)userArg;
    return 1; // accept -- matches the sample; tighten this if certificate pinning matters to you later
}

static bool http_init(void)
{
    if (g_httpReady) return true;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0) return false;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0) return false;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0) return false;

    int ret = sceNetInit();
    (void)ret;
    ret = sceNetPoolCreate("ambientAppNetPool", 4 * 1024, 0);
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
// complete read, matching net_http's own success criteria.
static bool http_download(const char *full_url, const char *local_dst)
{
    if (!http_init()) return false;

    int tpl = sceHttpCreateTemplate(g_libhttpCtxId, "Mozilla/5.0 (PLAYSTATION 4; 1.00)", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return false;
    sceHttpsSetSslCallback(tpl, skip_ssl_callback, NULL);

    // Bounds how long a hung/unreachable PLUGIN_UPDATE_URL can freeze
    // this app -- previously unbounded, so a bad URL or a dead network
    // path just hung the whole UI indefinitely with no way to cancel.
    // sceHttpSetConnectTimeOut/SetResolveTimeOut/SetSendTimeOut all have
    // real, confirmed 2-arg signatures in orbis/Http.h (unlike
    // sceHttpSetRecvTimeOut, see below), applied here at the template
    // level to match how sceHttpsSetSslCallback above already works.
    // NOT verified against a working call site in this SDK though --
    // no sample here (net_http included) actually calls any of these,
    // so treat this as a real, compilable improvement over no timeout
    // at all, not a confirmed-correct one; a real hardware run against
    // a genuinely unresponsive server is the only way to confirm it
    // actually bounds the hang.
    sceHttpSetConnectTimeOut(tpl, 10 * 1000 * 1000);  // 10s
    sceHttpSetResolveTimeOut(tpl, 10 * 1000 * 1000);  // 10s
    sceHttpSetSendTimeOut(tpl, 10 * 1000 * 1000);     // 10s
    // sceHttpSetRecvTimeOut deliberately NOT called: this header
    // declares it as a bare `void sceHttpSetRecvTimeOut();`, unlike its
    // three siblings above which all take (int32_t id, uint32_t usec).
    // That looks like an unfixed auto-generated stub (same pattern
    // found earlier in this SDK's freetype.h, where several functions
    // kept their doc comments but lost their real prototypes) rather
    // than a genuine 0-arg signature -- calling it with guessed
    // arguments risks a real ABI mismatch/crash, so the actual body-read
    // phase (sceHttpReadData below, the most likely place a slow-drip
    // response hangs) remains unbounded by a real per-call timeout.
    // Flagging this gap rather than guessing at a fix for it.

    bool ok = false;
    int conn = sceHttpCreateConnectionWithURL(tpl, full_url, 1);
    if (conn >= 0) {
        int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, full_url, 0);
        if (req >= 0) {
            if (sceHttpSendRequest(req, NULL, 0) >= 0) {
                int32_t statusCode = 0;
                sceHttpGetStatusCode(req, &statusCode);
                if (statusCode == 200) {
                    // sceKernelOpen/Write/Close, NOT fopen -- same fix
                    // ps4_ambient_light itself needed (handoff §46-49):
                    // fopen from an unusual thread context is not
                    // reliably safe in this environment.
                    int32_t fd = sceKernelOpen(local_dst, 0x200 | 0x001 /* O_TRUNC|O_CREAT, matches ambient_create_default_config's proven write flags */, 0777);
                    if (fd >= 0) {
                        uint8_t buf[64 * 1024];
                        ok = true;
                        for (;;) {
                            int n = sceHttpReadData(req, buf, sizeof(buf));
                            if (n < 0) { ok = false; break; }
                            if (n == 0) break; // clean EOF
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

// Registers the plugin in GoldHEN's own plugins.ini under [default]
// (loads for every title, appropriate here since this plugin hooks
// generic video-out/flip APIs present in every game, not a per-title
// patch) -- using the same config.h ini_table API as everything else
// in this project, not a hand-rolled text append that could corrupt
// an existing plugins.ini with entries for OTHER plugins already in
// it. Reads the real file first specifically to avoid clobbering
// those.
static bool register_plugin_in_goldhen(void)
{
    ini_table_s *table = ini_table_create();
    if (!table) return false;
    // Read the EXISTING plugins.ini if present -- do not assume this
    // app is the only thing that put entries there.
    ini_table_read_from_file(table, GOLDHEN_PLUGINS_INI); // ok if this returns false (file doesn't exist yet); table stays valid+empty
    ini_table_create_entry(table, "default", "ps4_ambient_light", PLUGIN_PRX_PATH);
    bool ok = ini_table_write_to_file(table, GOLDHEN_PLUGINS_INI);
    ini_table_destroy(table);
    return ok;
}

// Whether the plugin .prx is currently present at PLUGIN_PRX_PATH --
// drives the Home screen's single CTA between "Install Plugin" (first
// run, nothing there yet) and "Update Plugin" (already installed,
// fetch the latest build). Uses the same sceKernelOpen/Close pair
// already used elsewhere in this file for real file I/O rather than a
// stat call, since sceKernelOpen(..., O_RDONLY, ...) failing IS the
// existence check -- no separate stat API needed for a yes/no
// question like this one.
static bool plugin_exists(void)
{
    int32_t fd = sceKernelOpen(PLUGIN_PRX_PATH, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return false;
    sceKernelClose(fd);
    return true;
}

static void do_plugin_update(SDL_Renderer *renderer, TextRenderer *font)
{
    snprintf(g_statusLine, sizeof(g_statusLine), "Downloading plugin...");
    // Render+present this one line right now -- everything below this
    // point blocks the main loop (network I/O, then file I/O), so
    // without this the "Downloading..." message never actually gets a
    // frame to appear on screen before being overwritten by the final
    // success/failure line. A slow or hung server would otherwise make
    // the app look completely frozen with zero feedback.
    SDL_SetRenderDrawColor(renderer, COL_BG.r, COL_BG.g, COL_BG.b, 255);
    SDL_RenderClear(renderer);
    render_update_screen(renderer, font);
    SDL_UpdateWindowSurface(g_window);

    const char *tmpPath = "/data/ps4_ambient_light_update.tmp";
    if (!http_download(PLUGIN_UPDATE_URL, tmpPath)) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Download FAILED -- check PLUGIN_UPDATE_URL and network.");
        return;
    }

    // Stage the new build alongside the live one, and only ever touch
    // PLUGIN_PRX_PATH itself via sceKernelRename() once the staged copy
    // is fully verified good. sceKernelRename IS a real, confirmed
    // function here (int32_t sceKernelRename(const char*, const
    // char*), orbis/libkernel.h) -- contradicting this function's own
    // earlier comment assuming it "isn't assumed available" and using
    // manual copy+delete instead. That older approach opened
    // PLUGIN_PRX_PATH directly with O_TRUNC before a single byte of the
    // new build was copied in: a copy failure partway through (disk
    // full, I/O error) left the previously-working, live plugin
    // truncated and broken -- worse off than before the update was
    // attempted. Staging first means a failed copy never touches the
    // live file at all, and rename() replaces it as a single
    // filesystem operation instead of that truncate-then-hope-the-
    // write-completes window.
    const char *stagingPath = PLUGIN_PRX_PATH ".new";
    int32_t src = sceKernelOpen(tmpPath, 0, 0777);
    if (src < 0) { snprintf(g_statusLine, sizeof(g_statusLine), "Update failed: couldn't reopen downloaded file."); return; }
    int32_t dst = sceKernelOpen(stagingPath, 0x200 | 0x001, 0777);
    if (dst < 0) { sceKernelClose(src); snprintf(g_statusLine, sizeof(g_statusLine), "Update failed: can't write staging file %s", stagingPath); return; }
    uint8_t buf[64 * 1024];
    bool copyOk = true;
    for (;;) {
        ssize_t n = sceKernelRead(src, buf, sizeof(buf));
        if (n < 0) { copyOk = false; break; }
        if (n == 0) break;
        if (sceKernelWrite(dst, buf, n) != n) { copyOk = false; break; }
    }
    sceKernelClose(src);
    sceKernelClose(dst);
    if (!copyOk) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Update failed: staging copy was incomplete. Your existing plugin is untouched.");
        return;
    }

    if (sceKernelRename(stagingPath, PLUGIN_PRX_PATH) < 0) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Update failed: couldn't install staged build. Your existing plugin is untouched.");
        return;
    }

    if (!register_plugin_in_goldhen()) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Plugin installed, but plugins.ini update failed -- add it manually.");
        return;
    }
    snprintf(g_statusLine, sizeof(g_statusLine), "Updated. Relaunch your game to load the new build.");
}

// ---------------- Live preview ----------------

// Recomputed every frame from the current settings and both rendered
// on screen AND pushed to the real WLED light, so what the user sees
// in the app matches what their actual strip shows, not a simulation
// of it.
//
// This used to build exactly 5 fixed swatches and send them as
// numZones=5 at DDP pixel offset 0 (ddp_send_rgb_zones always writes
// offset 0 -- see ddp.c). That's the "5/6 boxes, only the first LED
// lighting up" bug: DDP offset 0 is always the physical start of the
// strip, so no matter how ledCountTop/Right/Bottom/Left were
// configured, only whichever few pixels happen to sit at the very
// start of the strip ever lit up -- the rest of a real install (which
// can be dozens of LEDs around all 4 edges) never received anything.
// This now builds one color per *configured* LED, in real physical
// wire order (layout_build -- see layout.c, a C port of the Android
// app's LedLayoutGeometry.kt), and sends the whole thing, so the
// preview and the real strip always show the same number of lit
// pixels in the same order.
// Tells wled-relay's tv_external_source.py whether THIS APP is
// currently the one driving the WLED controller directly -- same
// signal, same wire format (bare ASCII "on"/"off" over a plain UDP
// datagram, no MQTT client needed) as ps4_ambient_light's own
// relay_send_external_source() (main.c, v2.3+): this app's own
// update_live_preview() sends real DDP frames to the exact same
// physical WLED controller wled-relay's audio-reactive TV backlight
// system also targets, and the two would fight over the same LEDs
// while a user has this app open, just as they would during a real
// game.
//
// Unlike the real plugin (which only drives the strip while a game is
// actually running/foregrounded) this app has no idle mode --
// update_live_preview() runs every frame regardless of g_screen, for
// this app's entire runtime (see the main loop). So "driving" here is
// simply "this app is running with the feature enabled" -- there's no
// screen or mode to key off, just relaySignalEnabled itself. The
// caller (main loop, below) tracks that directly.
//
// One-shot, own socket per call, same as the plugin's version -- this
// only fires on a real relaySignalEnabled transition (see the caller),
// nowhere near update_live_preview()'s own 30fps DDP send, so there's
// no reason to hold a socket open for it.
//
// Deliberately has NO internal "if not enabled, return" gate -- same
// reasoning as the real plugin's own v2.6 fix: the caller only invokes
// this when relaySignalEnabled itself just changed, including
// true->false, and an internal gate re-checking that same
// already-changed flag would silently swallow the "off" send that
// call exists to make. A user who never enables the setting can never
// produce a transition, so this function is simply never called for
// them at all -- opt-in protection falls out of the call site, not a
// second gate here fighting with it.
static void relay_send_external_source(bool active)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return;

    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(g_cfg.relayPort);
    if (inet_pton(AF_INET, g_cfg.relayHost, &destAddr.sin_addr) != 1) {
        close(sockfd);
        return;
    }

    const char *payload = active ? "on" : "off";
    sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
    close(sockfd);
}

static void update_live_preview(void)
{
    colorpipeline_rebuild_perchannel_gamma_luts(&g_cfg);

    g_layoutCount = layout_build(&g_cfg, g_layoutSlots, LAYOUT_MAX_LEDS,
                                  (float)PREVIEW_W, (float)PREVIEW_H, (float)PREVIEW_PAD);

    if (g_layoutCount <= 0) {
        // Nothing configured (all 4 side counts are 0) -- nothing to
        // send or draw; leave the strip untouched rather than sending
        // an empty/garbage packet.
        return;
    }

    uint8_t wireBytes[LAYOUT_MAX_LEDS * 3];
    for (int i = 0; i < g_layoutCount; i++) {
        uint8_t rIn, gIn, bIn;
        // Hue derived from this slot's own fixed physical position
        // (layout_hue_for_slot(), see layout.c) rather than its index
        // i in g_layoutSlots[] -- see CHANGELOG-v11-to-v12.md: an
        // index-based hue meant Start Corner/Direction/LED Offset only
        // ever changed which screen position got drawn at wire-offset
        // i, never what color was actually sent to wire-offset i, so
        // those settings were invisible on the real strip even though
        // the on-screen rectangle plainly rotated.
        hsv_to_rgb(layout_hue_for_slot(&g_layoutSlots[i], (float)PREVIEW_W, (float)PREVIEW_H, (float)PREVIEW_PAD),
                   1.0f, 1.0f, &rIn, &gIn, &bIn);

        // Wire-order bytes -- what actually gets sent to the strip,
        // honoring the configured colorOrder.
        colorpipeline_process(&g_cfg, rIn, gIn, bIn, &wireBytes[i * 3]);

        // True-RGB bytes -- for the on-screen box, which should show
        // "what color is this really" rather than the wire byte
        // order, same reasoning the old pipeline swatches used.
        AmbientConfig rgbCfg = g_cfg;
        rgbCfg.colorOrder = ORDER_RGB;
        colorpipeline_process(&rgbCfg, rIn, gIn, bIn, g_layoutRGB[i]);
    }

    bool sent = ddp_send_rgb_zones(g_cfg.wledHost, g_cfg.wledPort, wireBytes, g_layoutCount);

    // update_live_preview() runs every frame at 30fps, so only touch
    // g_statusLine on the transition into/out of failure -- otherwise a
    // bad WLED Host would permanently overwrite every other status
    // message (save confirmations, update results, etc.) the instant it
    // happened, the same throttling reasoning already used for the pad
    // debug logging above.
    static bool s_wasSending = true; // mismatched on purpose so a bad host present at startup is reported too
    if (sent != s_wasSending) {
        if (!sent) {
            snprintf(g_statusLine, sizeof(g_statusLine),
                     "WLED Host \"%s\" isn't a valid IPv4 address -- live preview/output paused.", g_cfg.wledHost);
        }
        s_wasSending = sent;
    }
}

// ---------------- Rendering ----------------

static void draw_text(SDL_Renderer *renderer, TextRenderer *font, int x, int y, const char *text, SDL_Color color)
{
    text_renderer_draw(renderer, font, x, y, text, color);
}

// Fills a rounded rect using per-row horizontal spans (a filled center
// strip, plus `radius` single-pixel-tall rows top and bottom whose
// width follows the circle equation) -- there's no SDL2_gfx in this
// SDK to lean on for real arc rendering, and this stays cheap enough
// for the handful of panels this UI draws per frame (O(radius)
// SDL_RenderFillRect calls per rounded rect, not O(radius^2) points).
static void draw_rounded_rect_fill(SDL_Renderer *renderer, SDL_Rect rect, int radius, SDL_Color color)
{
    if (rect.w <= 0 || rect.h <= 0) return;
    if (radius > rect.w / 2) radius = rect.w / 2;
    if (radius > rect.h / 2) radius = rect.h / 2;
    if (radius < 0) radius = 0;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    if (radius == 0) { SDL_RenderFillRect(renderer, &rect); return; }

    SDL_Rect mid = { rect.x, rect.y + radius, rect.w, rect.h - 2 * radius };
    if (mid.h > 0) SDL_RenderFillRect(renderer, &mid);

    for (int i = 0; i < radius; i++) {
        int dy = radius - i;
        int dx = (int)sqrtf((float)(radius * radius - dy * dy));
        int w = rect.w - 2 * (radius - dx);
        if (w <= 0) continue;
        SDL_Rect topRow = { rect.x + (radius - dx), rect.y + i, w, 1 };
        SDL_Rect botRow = { rect.x + (radius - dx), rect.y + rect.h - 1 - i, w, 1 };
        SDL_RenderFillRect(renderer, &topRow);
        SDL_RenderFillRect(renderer, &botRow);
    }
}

// "Stroke via double-fill": draws a filled rounded rect in `border`,
// then a slightly smaller inset one in `fill` on top of it. Avoids
// needing real rounded-rect outline/arc rendering while still giving
// the thin-bordered card look the reference screenshots use
// throughout (Colour Adjustment / WLED / Customisation panels).
// borderWidth == 0 draws a plain filled rounded rect with no border.
static void draw_card(SDL_Renderer *renderer, SDL_Rect rect, int radius, SDL_Color fill, SDL_Color border, int borderWidth)
{
    if (borderWidth > 0) {
        draw_rounded_rect_fill(renderer, rect, radius, border);
        SDL_Rect inner = { rect.x + borderWidth, rect.y + borderWidth, rect.w - 2 * borderWidth, rect.h - 2 * borderWidth };
        int innerRadius = radius - borderWidth;
        if (innerRadius < 0) innerRadius = 0;
        draw_rounded_rect_fill(renderer, inner, innerRadius, fill);
    } else {
        draw_rounded_rect_fill(renderer, rect, radius, fill);
    }
}

// ---- Small hand-drawn vector icons ---------------------------------
// The bundled font atlas only covers printable ASCII (32-126, see
// text_render.h) -- there's no palette/wifi/slider glyph to draw, so
// these build simple icon-like shapes out of the same primitives
// draw_card/draw_rounded_rect_fill already use, sized to fit a
// `size`x`size` box at (x, y). draw_rounded_rect_fill with a square
// rect and radius == size/2 draws a true filled circle (the corner
// rows cover the whole shape, there's no straight-edge "mid" strip
// left), which is reused below for every dot/circle.

static void draw_icon_help(SDL_Renderer *renderer, TextRenderer *font, int x, int y, int size, SDL_Color color)
{
    SDL_Rect circle = { x, y, size, size };
    draw_rounded_rect_fill(renderer, circle, size / 2, color);
    // Dark glyph reads fine on the bright teal accent fill (focused
    // state); on the muted unfocused fill it needs to stay light
    // instead, or "?" all but disappears against it.
    bool brightFill = (color.r == COL_ACCENT.r && color.g == COL_ACCENT.g && color.b == COL_ACCENT.b);
    SDL_Color textColor = brightFill ? COL_ACCENT_TEXT : COL_TEXT_PRIMARY;
    int qw = text_renderer_measure(font, "?");
    draw_text(renderer, font, x + (size - qw) / 2, y + size / 2 - 10, "?", textColor);
}

static void draw_icon_setup(SDL_Renderer *renderer, int x, int y, int size, SDL_Color color)
{
    // Three bars of increasing height, like a slider/equalizer control
    // -- echoes the reference screenshot's "Set Up" icon without
    // needing an actual glyph for it.
    int barW = size / 5;
    int gap = barW;
    int heights[3] = { size / 2, (size * 3) / 4, size };
    for (int i = 0; i < 3; i++) {
        SDL_Rect bar = { x + i * (barW + gap), y + (size - heights[i]), barW, heights[i] };
        draw_rounded_rect_fill(renderer, bar, barW / 3, color);
    }
}

static void draw_icon_palette(SDL_Renderer *renderer, int x, int y, int size, SDL_Color ringColor)
{
    // A ring (drawn via the same border-then-inset-fill trick as
    // draw_card) with three small colored dots inside it, standing in
    // for a palette/colour-picker icon.
    SDL_Rect ring = { x, y, size, size };
    draw_card(renderer, ring, size / 2, COL_CARD_BG, ringColor, 3);

    int dot = size / 4;
    SDL_Color dots[3] = { {235, 90, 90, 255}, {90, 210, 130, 255}, {90, 150, 235, 255} };
    int cx = x + size / 2, cy = y + size / 2;
    int off[3][2] = { { -dot, -dot / 2 }, { dot / 3, -dot }, { dot / 4, dot } };
    for (int i = 0; i < 3; i++) {
        SDL_Rect d = { cx + off[i][0], cy + off[i][1], dot, dot };
        draw_rounded_rect_fill(renderer, d, dot / 2, dots[i]);
    }
}

static void format_item_value(const MenuItem *item, const AmbientConfig *cfg, char *out, size_t outSize)
{
    if (item->type == FIELD_STRING) {
        // Was hardcoded to cfg->wledHost, the only STRING field that
        // existed at the time. Now that relayHost is a second one
        // (see settings.c's kMenuItems), read through the item's own
        // offset instead -- same generic-by-offset approach
        // settings_get_i32/settings_set_i32 already use for every
        // other field type.
        const char *base = (const char *)cfg + item->offset;
        snprintf(out, outSize, "%s", base);
        return;
    }
    if (item->type == FIELD_BOOL) {
        snprintf(out, outSize, "%s", settings_get_i32(cfg, item) ? "ON" : "OFF");
        return;
    }
    if (item->type == FIELD_ENUM) {
        int v = settings_get_i32(cfg, item);
        if (v >= 0 && v < item->enumCount) snprintf(out, outSize, "%s", item->enumNames[v]);
        else snprintf(out, outSize, "?");
        return;
    }
    if (strcmp(item->key, "gamma") == 0) { // the one special-display numeric field (index -> "1.0".."2.8")
        static const char *kGammaStrings[8] = {"1.0","1.4","1.8","2.0","2.2","2.4","2.6","2.8"};
        int v = settings_get_i32(cfg, item);
        snprintf(out, outSize, "%s", (v >= 0 && v < 8) ? kGammaStrings[v] : "?");
        return;
    }
    snprintf(out, outSize, "%d", settings_get_i32(cfg, item));
}

// Layout constants for the settings-list card. y stepping inside the
// card (34px per group header, +LIST_GROUP_DESC_H when a description
// is present, 30px per row) is kept numerically identical to before
// this pass' screen split -- settings_last_visible_index_for() below
// has to stay in lockstep with whatever render_grouped_settings_list()
// draws, and re-skinning colors/chrome doesn't need to touch that
// rhythm.
#define LIST_CARD_X 40
#define LIST_CARD_Y 92
#define LIST_CARD_W 800
#define LIST_ROW_X_PAD 22
#define LIST_VALUE_CHIP_W 190
#define LIST_VALUE_CHIP_H 34

// Mirrors render_grouped_settings_list()'s row layout (group headers +
// optional one-line description + row height + the same bottom clamp)
// without drawing anything, so handle_grouped_settings_input() can
// tell whether a given item index is actually on screen for a given
// scroll offset. Kept deliberately in lockstep with that function --
// if its pixel layout changes, this has to change with it, or
// scrolling and rendering will disagree about what's visible. Only
// counts rows belonging to `screen` -- kMenuItems' own ordering
// guarantees those are one contiguous run (see screen_item_range()),
// so skipping non-matching rows here is just a cheap no-op past the
// end of that run, not a correctness issue.
#define LIST_GROUP_DESC_H 22
static int settings_last_visible_index_for(MenuScreen screen, int scrollOffset)
{
    int y = LIST_CARD_Y + 26;
    const char *currentGroup = "";
    int last = scrollOffset - 1; // nothing shown yet
    for (int i = scrollOffset; i < kMenuItemCount; i++) {
        const MenuItem *item = &kMenuItems[i];
        if (item->screen != screen) continue;
        if (strcmp(currentGroup, item->group) != 0) {
            currentGroup = item->group;
            y += 34;
            if (item->groupDesc) y += LIST_GROUP_DESC_H;
        }
        y += 30;
        if (y > FRAME_HEIGHT - 260) break;
        last = i;
    }
    return last;
}

// Draws the scrollable card of items belonging to `screen` (Set Up or
// Customisation), grouped into per-`group` headers with an optional
// one-line description under each new heading -- same visual language
// (rounded card, teal bullet + heading, boxed value chip, teal-
// bordered selection) the original single-list screen already used,
// just keyed off `group` instead of `section` so a screen's items can
// be organized into topic cards instead of raw ini-section order.
// Returns the index of the last item actually drawn, so the caller can
// render the "X-Y of N" scroll indicator without a second pass.
static int render_grouped_settings_list(SDL_Renderer *renderer, TextRenderer *font, MenuScreen screen)
{
    int listCardBottom = (FRAME_HEIGHT - 260) + 24;
    SDL_Rect listCard = { LIST_CARD_X, LIST_CARD_Y, LIST_CARD_W, listCardBottom - LIST_CARD_Y };
    draw_card(renderer, listCard, 22, COL_CARD_BG, COL_CARD_BORDER, 1);

    int y = LIST_CARD_Y + 26;
    const char *currentGroup = "";
    int lastShown = g_scrollOffset - 1; // nothing drawn yet
    for (int i = g_scrollOffset; i < kMenuItemCount; i++) {
        const MenuItem *item = &kMenuItems[i];
        if (item->screen != screen) continue;

        if (strcmp(currentGroup, item->group) != 0) {
            currentGroup = item->group;
            // Small teal bullet ahead of the group name, echoing the
            // icon-plus-heading pattern the reference screenshots use
            // for every card/section title -- this build has no icon
            // glyphs in its ASCII-only atlas, so a solid accent square
            // stands in.
            SDL_Rect bullet = { LIST_CARD_X + LIST_ROW_X_PAD, y + 6, 10, 10 };
            draw_rounded_rect_fill(renderer, bullet, 3, COL_ACCENT);
            draw_text(renderer, font, LIST_CARD_X + LIST_ROW_X_PAD + 20, y, currentGroup, COL_TEXT_PRIMARY);
            y += 34;
            if (item->groupDesc) {
                draw_text(renderer, font, LIST_CARD_X + LIST_ROW_X_PAD, y, item->groupDesc, COL_TEXT_SECOND);
                y += LIST_GROUP_DESC_H;
            }
        }

        char valueStr[80];
        format_item_value(item, &g_cfg, valueStr, sizeof(valueStr));

        bool selected = (i == g_selectedIndex);
        SDL_Rect row = { LIST_CARD_X + 10, y - 4, LIST_CARD_W - 20, 30 };
        if (selected) {
            draw_card(renderer, row, 10, COL_SELECT_WASH, COL_ACCENT, 2);
        }

        draw_text(renderer, font, LIST_CARD_X + LIST_ROW_X_PAD, y, item->label,
                  selected ? COL_ACCENT : COL_TEXT_PRIMARY);

        // Value drawn inside its own small field chip, right-aligned in
        // the row -- mirrors the boxed-input look every field uses in
        // the reference screenshots.
        SDL_Rect chip = {
            LIST_CARD_X + LIST_CARD_W - LIST_VALUE_CHIP_W - LIST_ROW_X_PAD, y - 6,
            LIST_VALUE_CHIP_W, LIST_VALUE_CHIP_H
        };
        draw_card(renderer, chip, 8, selected ? COL_ACCENT : COL_FIELD_BG,
                  selected ? COL_ACCENT : COL_FIELD_BORDER, selected ? 0 : 1);
        int valueW = text_renderer_measure(font, valueStr);
        draw_text(renderer, font, chip.x + chip.w - valueW - 14, y - 1, valueStr,
                  selected ? COL_ACCENT_TEXT : COL_TEXT_PRIMARY);

        y += 30;
        lastShown = i;
        if (y > FRAME_HEIGHT - 260) break;
    }

    int start, count;
    screen_item_range(screen, &start, &count);
    if (g_scrollOffset > start || lastShown < start + count - 1) {
        char scrollLine[64];
        snprintf(scrollLine, sizeof(scrollLine), "%d-%d of %d",
                 g_scrollOffset - start + 1, lastShown - start + 1, count);
        int w = text_renderer_measure(font, scrollLine);
        draw_text(renderer, font, LIST_CARD_X + LIST_CARD_W - w - 22, 24, scrollLine, COL_TEXT_SECOND);
    }
    return lastShown;
}

static void render_setup_screen(SDL_Renderer *renderer, TextRenderer *font)
{
    draw_text(renderer, g_titleFont, 44, 24, "Set Up", COL_TEXT_PRIMARY);
    draw_text(renderer, font, 44, 62,
              "D-Pad: navigate   Cross: edit   Triangle: Update Plugin   Circle: Home   Options: Save & Quit",
              COL_TEXT_SECOND);

    render_grouped_settings_list(renderer, font, MENU_SCREEN_SETUP);

    // ---- Full-strip layout preview -------------------------------
    // Every configured LED, in real physical wire order and roughly
    // its real position around a TV outline (see layout_build() in
    // layout.c, a C port of the Android app's LedLayoutGeometry.kt).
    // This is what's actually sent to the real WLED strip right now.
    // Lives on Set Up rather than Customisation since it's a direct
    // visualization of this screen's own LED-count/corner/direction
    // fields, not the color pipeline.
    SDL_Rect previewCard = { PREVIEW_X - 24, PREVIEW_Y - 68, PREVIEW_W + 48, PREVIEW_H + 68 + 90 };
    draw_card(renderer, previewCard, 22, COL_CARD_BG, COL_CARD_BORDER, 1);

    draw_text(renderer, font, PREVIEW_X, PREVIEW_Y - 44, "Live layout preview", COL_TEXT_PRIMARY);
    draw_text(renderer, font, PREVIEW_X, PREVIEW_Y - 20,
              "Every LED, real wire order -- also sent to your WLED strip right now.", COL_TEXT_SECOND);

    draw_card(renderer, (SDL_Rect){ PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H }, 16, COL_FIELD_BG, COL_FIELD_BORDER, 1);

    // Capture-margin overlay -- purely informational, mirrors the
    // percentages the plugin itself excludes from capture on each
    // edge (set on the Customisation screen), same as the Android
    // app's own layout screen shows.
    {
        float mL = (float)g_cfg.marginLeft, mR = (float)g_cfg.marginRight;
        float mT = (float)g_cfg.marginTop, mB = (float)g_cfg.marginBottom;
        if (mL > 0.0f || mR > 0.0f || mT > 0.0f || mB > 0.0f) {
            SDL_SetRenderDrawColor(renderer, 90, 140, 170, 200);
            SDL_Rect inner = {
                PREVIEW_X + (int)(PREVIEW_W * mL / 100.0f),
                PREVIEW_Y + (int)(PREVIEW_H * mT / 100.0f),
                (int)(PREVIEW_W * (1.0f - (mL + mR) / 100.0f)),
                (int)(PREVIEW_H * (1.0f - (mT + mB) / 100.0f))
            };
            SDL_RenderDrawRect(renderer, &inner);
        }
    }

    for (int i = 0; i < g_layoutCount; i++) {
        const LedSlot *s = &g_layoutSlots[i];
        SDL_Rect r = {
            PREVIEW_X + (int)(s->cx - s->w / 2.0f),
            PREVIEW_Y + (int)(s->cy - s->h / 2.0f),
            (int)(s->w > 1.0f ? s->w : 1.0f),
            (int)(s->h > 1.0f ? s->h : 1.0f)
        };
        SDL_SetRenderDrawColor(renderer, g_layoutRGB[i][0], g_layoutRGB[i][1], g_layoutRGB[i][2], 255);
        SDL_RenderFillRect(renderer, &r);
        // First physical pixel gets a bright outline so
        // startCorner/direction/ledOffset are visually confirmable at
        // a glance -- same idea as the Android preview highlighting
        // LED #1 in green.
        if (i == 0) SDL_SetRenderDrawColor(renderer, COL_TEXT_PRIMARY.r, COL_TEXT_PRIMARY.g, COL_TEXT_PRIMARY.b, 255);
        else        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 90);
        SDL_RenderDrawRect(renderer, &r);
    }

    if (g_layoutCount == 0) {
        draw_text(renderer, font, PREVIEW_X + 20, PREVIEW_Y + PREVIEW_H / 2,
                  "No LEDs configured -- set Top/Right/Bottom/Left LED counts above 0.", COL_TEXT_SECOND);
    } else {
        char countLine[128];
        snprintf(countLine, sizeof(countLine),
                 "%d LEDs total (Top %u / Right %u / Bottom %u / Left %u) -- highlighted box = physical pixel 0",
                 g_layoutCount, g_cfg.ledCountTop, g_cfg.ledCountRight, g_cfg.ledCountBottom, g_cfg.ledCountLeft);
        draw_text(renderer, font, PREVIEW_X, PREVIEW_Y + PREVIEW_H + 14, countLine, COL_TEXT_SECOND);
    }

    draw_text(renderer, font, 44, FRAME_HEIGHT - 44, g_statusLine, COL_ACCENT);
}

static void render_customize_screen(SDL_Renderer *renderer, TextRenderer *font)
{
    draw_text(renderer, g_titleFont, 44, 24, "Customisation", COL_TEXT_PRIMARY);
    draw_text(renderer, font, 44, 62,
              "D-Pad: navigate   Cross: edit   Triangle: Update Plugin   Circle: Home   Options: Save & Quit",
              COL_TEXT_SECOND);

    render_grouped_settings_list(renderer, font, MENU_SCREEN_CUSTOMIZE);

    // ---- Pipeline-correctness swatches -----------------------------
    // On-screen only, NOT sent to the strip. Lives on Customisation
    // since it's a direct visualization of this screen's own color
    // fields -- exercises gamma/contrast/saturation/levels on pure
    // primaries so tuning color science doesn't depend on the physical
    // layout (set on the Set Up screen) being right first.
    SDL_Rect swatchCard = { PREVIEW_X - 24, PREVIEW_Y - 68, PREVIEW_W + 48, 220 };
    draw_card(renderer, swatchCard, 22, COL_CARD_BG, COL_CARD_BORDER, 1);

    draw_text(renderer, font, PREVIEW_X, PREVIEW_Y - 44, "Pipeline test swatches", COL_TEXT_PRIMARY);
    draw_text(renderer, font, PREVIEW_X, PREVIEW_Y - 20,
              "On-screen only, not sent to the strip -- for judging gamma/saturation/contrast on pure primaries.",
              COL_TEXT_SECOND);

    int swatchX = PREVIEW_X, swatchY = PREVIEW_Y + 20, swatchSize = 90;
    const char *swatchLabels[NUM_PREVIEW_SWATCHES] = {"Red", "Green", "Blue", "White", "Gray"};
    for (int i = 0; i < NUM_PREVIEW_SWATCHES; i++) {
        uint8_t rgbOut[3];
        AmbientConfig previewCfg = g_cfg;
        previewCfg.colorOrder = ORDER_RGB;
        colorpipeline_process(&previewCfg, kPreviewInputs[i][0], kPreviewInputs[i][1], kPreviewInputs[i][2], rgbOut);

        SDL_Rect sw = { swatchX + i * (swatchSize + 20), swatchY, swatchSize, swatchSize };
        draw_rounded_rect_fill(renderer, sw, 14, (SDL_Color){ rgbOut[0], rgbOut[1], rgbOut[2], 255 });
        draw_text(renderer, font, swatchX + i * (swatchSize + 20), swatchY + swatchSize + 8, swatchLabels[i], COL_TEXT_SECOND);
    }

    draw_text(renderer, font, 44, FRAME_HEIGHT - 44, g_statusLine, COL_ACCENT);
}

static void render_update_screen(SDL_Renderer *renderer, TextRenderer *font)
{
    draw_text(renderer, g_titleFont, 44, 24, "Update Plugin", COL_TEXT_PRIMARY);
    draw_text(renderer, font, 44, 62, "Fetches the latest ps4_ambient_light.prx and installs it.", COL_TEXT_SECOND);

    SDL_Rect card = { 40, 110, 1200, 220 };
    draw_card(renderer, card, 22, COL_CARD_BG, COL_CARD_BORDER, 1);

    draw_text(renderer, font, card.x + 24, card.y + 24, "Source URL", COL_TEXT_SECOND);
    SDL_Rect urlChip = { card.x + 24, card.y + 50, card.w - 48, 40 };
    draw_card(renderer, urlChip, 10, COL_FIELD_BG, COL_FIELD_BORDER, 1);
    draw_text(renderer, font, urlChip.x + 14, urlChip.y + 8, PLUGIN_UPDATE_URL, COL_TEXT_PRIMARY);

    draw_text(renderer, font, card.x + 24, card.y + 108, g_statusLine, COL_TEXT_PRIMARY);

    // X-to-download affordance, styled the same teal filled pill as
    // the reference screenshots' primary CTA ("Save customisation" /
    // "Start SceneGlow") -- purely informational here (there's no
    // pointer input, only D-Pad/buttons), but it visually reinforces
    // which button actually does the thing, same role the pill plays
    // in the reference UI.
    SDL_Rect cta = { card.x + 24, card.y + card.h - 60, 420, 48 };
    draw_rounded_rect_fill(renderer, cta, 14, COL_ACCENT);
    const char *ctaLabel = "X: Download & Install";
    int ctaW = text_renderer_measure(font, ctaLabel);
    draw_text(renderer, font, cta.x + (cta.w - ctaW) / 2, cta.y + 12, ctaLabel, COL_ACCENT_TEXT);

    draw_text(renderer, font, 44, FRAME_HEIGHT - 44, "Circle: back to Home (no changes made until X is pressed)", COL_TEXT_SECOND);
}

// ---------------- Home screen ----------------
// Entry point matching the reference screenshot: title + subtitle,
// a status card, one big primary CTA (Install/Update Plugin --
// whichever applies), and a row of three smaller buttons (Help, Set
// Up, Customisation). Set Up and Customisation now route to their own
// grouped screens (SCREEN_SETUP / SCREEN_CUSTOMIZE, see
// render_setup_screen()/render_customize_screen() and
// handle_grouped_settings_input() below) instead of one flat list.
// Help still has no dedicated screen, and says so rather than
// pretending to do something.
#define HOME_FOCUS_CTA   0
#define HOME_FOCUS_HELP  1
#define HOME_FOCUS_SETUP 2
#define HOME_FOCUS_CUST  3

// Computed once per frame by render_home_screen() -- kept around
// (rather than being locals) in case a future pass adds pointer/touch
// input, so hit-testing would have real button bounds to check
// against instead of needing to duplicate this layout math. Not read
// anywhere yet; D-Pad-only navigation in handle_home_input() below
// only needs g_homeFocus's index, not actual screen coordinates.
static SDL_Rect g_homeCtaRect;
static SDL_Rect g_homeButtonRects[3]; // Help, Set Up, Customisation, in that order

static void render_home_screen(SDL_Renderer *renderer, TextRenderer *font)
{
    bool installed = plugin_exists();

    draw_text(renderer, g_titleFont, 60, 40, "PS4 Ambilight", COL_TEXT_PRIMARY);
    draw_text(renderer, font, 60, 92, "Screen-reactive ambient lighting for your PS4, powered by GoldHEN.", COL_TEXT_SECOND);

    // Status card -- mirrors the reference's "SceneGlow is ready" /
    // "Ready to request screen capture." pairing, but reporting the
    // thing that's actually relevant to a companion app for a
    // background plugin: whether the plugin itself is installed.
    SDL_Rect statusCard = { 60, 170, 1800, 190 };
    draw_card(renderer, statusCard, 24, COL_CARD_BG, COL_CARD_BORDER, 1);
    const char *statusTitle = installed ? "Plugin installed" : "Plugin not installed";
    const char *statusSub = installed
        ? "ps4_ambient_light.prx is registered with GoldHEN. Adjust settings below, or update anytime."
        : "Install the plugin to enable ambient lighting -- settings alone don't do anything until it's running.";
    int titleW = text_renderer_measure(g_titleFont, statusTitle);
    draw_text(renderer, g_titleFont, statusCard.x + (statusCard.w - titleW) / 2, statusCard.y + 44, statusTitle, COL_TEXT_PRIMARY);
    int subW = text_renderer_measure(font, statusSub);
    draw_text(renderer, font, statusCard.x + (statusCard.w - subW) / 2, statusCard.y + 108, statusSub, COL_TEXT_SECOND);

    // Primary CTA -- label follows plugin state directly from
    // plugin_exists(), same check driving the status card above, so
    // the two can never disagree about whether it's installed.
    g_homeCtaRect = (SDL_Rect){ 360, statusCard.y + statusCard.h + 40, 1200, 140 };
    bool ctaFocused = (g_homeFocus == HOME_FOCUS_CTA);
    if (ctaFocused) {
        // Focus ring: a slightly larger rounded rect in the outline
        // color with the actual button rect (in accent color) drawn
        // on top of it as the "fill" -- same border-via-double-fill
        // technique draw_card() uses internally, just done manually
        // here so the ring can be a couple pixels larger than the
        // button itself instead of sharing its exact bounds.
        SDL_Rect ring = { g_homeCtaRect.x - 5, g_homeCtaRect.y - 5, g_homeCtaRect.w + 10, g_homeCtaRect.h + 10 };
        draw_rounded_rect_fill(renderer, ring, 28, COL_TEXT_PRIMARY);
    }
    draw_rounded_rect_fill(renderer, g_homeCtaRect, 24, COL_ACCENT);
    const char *ctaLabel = installed ? "Update Plugin" : "Install Plugin";
    int ctaW = text_renderer_measure(font, ctaLabel);
    draw_text(renderer, font, g_homeCtaRect.x + (g_homeCtaRect.w - ctaW) / 2, g_homeCtaRect.y + (g_homeCtaRect.h - 24) / 2, ctaLabel, COL_ACCENT_TEXT);

    // Bottom row: Help / Set Up / Customisation, each a smaller card
    // with an icon + label, matching the reference's bottom nav row.
    int rowY = g_homeCtaRect.y + g_homeCtaRect.h + 50;
    int btnW = 380, btnH = 110, gap = 40;
    int rowW = btnW * 3 + gap * 2;
    int rowX = (FRAME_WIDTH - rowW) / 2;
    const char *labels[3] = { "Help", "Set Up", "Customisation" };

    for (int i = 0; i < 3; i++) {
        SDL_Rect r = { rowX + i * (btnW + gap), rowY, btnW, btnH };
        g_homeButtonRects[i] = r;
        bool focused = (g_homeFocus == HOME_FOCUS_HELP + i);
        draw_card(renderer, r, 18, COL_CARD_BG, focused ? COL_ACCENT : COL_CARD_BORDER, focused ? 2 : 1);

        int iconSize = 44;
        int iconX = r.x + 24, iconY = r.y + (r.h - iconSize) / 2;
        if (i == 0)      draw_icon_help(renderer, font, iconX, iconY, iconSize, focused ? COL_ACCENT : COL_FIELD_BORDER);
        else if (i == 1) draw_icon_setup(renderer, iconX, iconY, iconSize, focused ? COL_ACCENT : COL_TEXT_PRIMARY);
        else              draw_icon_palette(renderer, iconX, iconY, iconSize, focused ? COL_ACCENT : COL_FIELD_BORDER);

        draw_text(renderer, font, iconX + iconSize + 18, r.y + (r.h - 20) / 2, labels[i], COL_TEXT_PRIMARY);
    }

    draw_text(renderer, font, 60, FRAME_HEIGHT - 44, g_statusLine, COL_ACCENT);
    draw_text(renderer, font, 60, FRAME_HEIGHT - 76,
              "D-Pad: move   Cross: select   Options: Save & Quit", COL_TEXT_SECOND);
}

static void handle_home_input(const OrbisPadData *pad, const OrbisPadData *prevPad, SDL_Renderer *renderer, TextRenderer *font)
{
    bool up    = (pad->buttons & ORBIS_PAD_BUTTON_UP)    && !(prevPad->buttons & ORBIS_PAD_BUTTON_UP);
    bool down  = (pad->buttons & ORBIS_PAD_BUTTON_DOWN)  && !(prevPad->buttons & ORBIS_PAD_BUTTON_DOWN);
    bool left  = (pad->buttons & ORBIS_PAD_BUTTON_LEFT)  && !(prevPad->buttons & ORBIS_PAD_BUTTON_LEFT);
    bool right = (pad->buttons & ORBIS_PAD_BUTTON_RIGHT) && !(prevPad->buttons & ORBIS_PAD_BUTTON_RIGHT);
    bool crossUp = !(pad->buttons & ORBIS_PAD_BUTTON_CROSS) && (prevPad->buttons & ORBIS_PAD_BUTTON_CROSS);
    bool options = (pad->buttons & ORBIS_PAD_BUTTON_OPTIONS) && !(prevPad->buttons & ORBIS_PAD_BUTTON_OPTIONS);

    if (up)   g_homeFocus = HOME_FOCUS_CTA;
    if (down) g_homeFocus = (g_homeFocus == HOME_FOCUS_CTA) ? HOME_FOCUS_SETUP : g_homeFocus;
    if (g_homeFocus != HOME_FOCUS_CTA) {
        if (right) g_homeFocus = HOME_FOCUS_HELP + ((g_homeFocus - HOME_FOCUS_HELP + 1) % 3);
        if (left)  g_homeFocus = HOME_FOCUS_HELP + ((g_homeFocus - HOME_FOCUS_HELP + 2) % 3);
    }

    if (crossUp) {
        switch (g_homeFocus) {
            case HOME_FOCUS_CTA:
                // Same download+stage+rename+register flow the Update
                // screen's X press uses -- do_plugin_update() renders
                // its own one-frame "Downloading..." progress via
                // render_update_screen() internally regardless of
                // which screen is actually active; a brief flash of
                // that screen's chrome during the (blocking) download
                // is a known, accepted cosmetic quirk of reusing it
                // here rather than threading a "what to render mid-
                // download" callback through for this first pass.
                do_plugin_update(renderer, font);
                break;
            case HOME_FOCUS_HELP:
                snprintf(g_statusLine, sizeof(g_statusLine), "Help screen isn't built yet -- see README.md for setup docs.");
                break;
            case HOME_FOCUS_SETUP: {
                int start, count;
                screen_item_range(MENU_SCREEN_SETUP, &start, &count);
                g_selectedIndex = start;
                g_scrollOffset = start;
                g_screen = SCREEN_SETUP;
                break;
            }
            case HOME_FOCUS_CUST: {
                int start, count;
                screen_item_range(MENU_SCREEN_CUSTOMIZE, &start, &count);
                g_selectedIndex = start;
                g_scrollOffset = start;
                g_screen = SCREEN_CUSTOMIZE;
                break;
            }
        }
    }

    if (options) {
        if (settings_save(&g_cfg, AMBIENT_CONFIG_PATH))
            snprintf(g_statusLine, sizeof(g_statusLine), "Saved to %s -- the plugin will pick this up on its own reload check.", AMBIENT_CONFIG_PATH);
        else
            snprintf(g_statusLine, sizeof(g_statusLine), "Save FAILED -- check %s is writable.", AMBIENT_CONFIG_PATH);
    }
}

// ---------------- Input / navigation ----------------

// Raw scePad button bits -- ORBIS_PAD_BUTTON_* constants from
// orbis/Pad.h (not SDL's joystick button indices, which aren't
// guaranteed to map 1:1 to DualShock buttons the same way across
// OpenOrbis versions -- using the real Pad API directly avoids that
// ambiguity, same reasoning as the SDL2 sample using raw joystick
// axes/buttons rather than SDL_GameController's abstracted mapping).
static int g_padHandle = -1;
static int32_t g_userId = -1; // set once in pad_init(), reused by the IME dialog below

// Verified against OpenOrbis samples/input/input/controller.cpp's real
// init sequence -- there is no ORBIS_USER_SERVICE_USER_ID_SYSTEM
// constant (that was wrong in an earlier draft of this file); the
// user ID has to come from sceUserServiceGetInitialUser, and
// scePadOpen's 4th argument is NULL for standard use, not a params
// struct (OrbisPadExtParam is for scePadOpenExt, a different call).
static bool pad_init(void)
{
    int initRet = scePadInit();
    printf("[pad_init] scePadInit() = %d\n", initRet);
    if (initRet != 0) return false;

    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);

    int32_t userID = -1;
    int userRet = sceUserServiceGetInitialUser(&userID);
    printf("[pad_init] sceUserServiceGetInitialUser() = %d, userID = %d\n", userRet, userID);
    g_userId = userID;

    g_padHandle = scePadOpen(userID, 0, 0, NULL);
    printf("[pad_init] scePadOpen() = %d\n", g_padHandle);
    return g_padHandle >= 0;
}

// ---------------- On-screen keyboard (any numeric or string field) ----------------
//
// Real API surface, pulled directly from orbis/ImeDialog.h,
// orbis/_types/ime_dialog.h, and orbis/CommonDialog.h in this SDK --
// not from memory:
//   sceCommonDialogInitialize(void)               -- confirmed, call once at startup
//   sceCommonDialogIsUsed(void)                    -- confirmed, true while ANY system dialog is up
//   sceImeDialogInit(const OrbisImeDialogSetting*, OrbisImeSettingsExtended*)
//   sceImeDialogGetStatus(void) -> OrbisDialogStatus (NONE/RUNNING/STOPPED)
//   sceImeDialogGetResult(OrbisDialogResult*)      -- endstatus: OK/CANCEL/ABORD
//   sceImeDialogTerm(void)
// OrbisImeDialogSetting's fields (userId, type, inputTextBuffer,
// maxTextLength, posx/posy, alignment, placeholder, title, etc.) are
// all real, confirmed struct members -- this isn't a guessed layout.
// ORBIS_TYPE_NUMBER = 4 (orbis/_types/ime_dialog.h) is likewise a
// real, confirmed constant -- used below for numeric fields instead of
// ORBIS_TYPE_BASIC_LATIN, restricting the on-screen keyboard's own
// layout to digits (and '-' -- see the min<0 comment below).
//
// Genuinely NOT confirmed, because no working IME-dialog sample exists
// in this SDK to check against (samples/keyboard is a *physical*
// keyboard, a different API -- orbis/Keyboard.h -- despite this
// project's earlier README implying it covered this):
//   - Passing NULL for the second (OrbisImeSettingsExtended*) arg to
//     sceImeDialogInit. Plausible (common Sony "NULL = defaults"
//     pattern elsewhere in these SDKs) but not verified against a
//     working call site.
//   - posx/posy's units and coordinate space (assumed screen pixels,
//     centered at 960,540 for this app's 1920x1080 window) and
//     whether ORBIS_H_CENTER/ORBIS_V_CENTER actually center ON that
//     point vs. some other anchor.
//   - supportedLanguages=0 as "default/unrestricted" -- not documented
//     in this header at all, just the least-surprising guess.
//   - Whether ORBIS_TYPE_NUMBER's on-screen layout actually includes a
//     '-' key for entering a negative value (LED Offset, Saturation,
//     and Contrast all allow negatives) -- if it doesn't, this falls
//     back to letting strtol() reject/clamp whatever was actually
//     typeable, which is still safe (never writes an unparseable or
//     out-of-range value) but might mean those three specific fields
//     can't reach their negative range from this dialog. Flagging
//     rather than guessing a workaround for a keyboard layout this
//     session can't see.
// This needs a real hardware run to confirm the dialog actually shows
// where/how expected -- flagging that plainly rather than treating a
// clean compile as proof.
//
// All text here is plain ASCII (IP addresses, hostnames, digits, '-',
// '.'), so this widens/narrows manually instead of via
// mbstowcs/wcstombs -- avoids depending on locale support that hasn't
// been checked in this SDK's libc, for a charset simple enough not to
// need it.
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
// FIELD_U16 item -- FIELD_ENUM/FIELD_BOOL have no sensible "type a
// value" equivalent and are still cycled/toggled with D-Pad Left/Right
// (see handle_grouped_settings_input()), never routed here.
static void open_field_ime_dialog(int itemIndex)
{
    if (g_imeDialogOpen || sceCommonDialogIsUsed()) return;
    const MenuItem *item = &kMenuItems[itemIndex];

    char placeholderAscii[80], titleAscii[80], currentAscii[64];
    if (item->type == FIELD_STRING) {
        snprintf(placeholderAscii, sizeof(placeholderAscii), "192.168.x.x");
        // Was hardcoded to g_cfg.wledHost, the only STRING field at
        // the time -- generalized now that relayHost is a second one,
        // same reasoning as format_item_value() above.
        const char *base = (const char *)&g_cfg + item->offset;
        snprintf(currentAscii, sizeof(currentAscii), "%s", base);
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
        snprintf(g_statusLine, sizeof(g_statusLine), "Couldn't open the keyboard (sceImeDialogInit = %d).", ret);
    }
}

// Call once per frame from the main loop while g_imeDialogOpen.
static void update_ime_dialog(void)
{
    OrbisDialogStatus status = sceImeDialogGetStatus();
    static OrbisDialogStatus s_lastLoggedStatus = (OrbisDialogStatus)-1;
    if (status != s_lastLoggedStatus) {
        printf("[ime] sceImeDialogGetStatus() = %d (0=NONE,1=RUNNING,2=STOPPED)\n", (int)status);
        s_lastLoggedStatus = status;
    }
    if (status != ORBIS_DIALOG_STATUS_STOPPED) return; // still RUNNING (or NONE, briefly) -- nothing to do yet

    OrbisDialogResult result;
    memset(&result, 0, sizeof(result));
    int32_t getResultRet = sceImeDialogGetResult(&result);
    printf("[ime] sceImeDialogGetResult() = %d, endstatus = %d (0=OK,1=CANCEL,2=ABORD)\n", getResultRet, (int)result.endstatus);

    const MenuItem *item = (g_imeItemIndex >= 0 && g_imeItemIndex < kMenuItemCount) ? &kMenuItems[g_imeItemIndex] : NULL;

    if (result.endstatus == ORBIS_DIALOG_OK && item != NULL) {
        char typed[64];
        size_t i = 0;
        for (; i < sizeof(typed) - 1 && g_imeBuffer[i] != L'\0'; i++) typed[i] = (char)g_imeBuffer[i];
        typed[i] = '\0';

        if (item->type == FIELD_STRING) {
            char *dst = (char *)&g_cfg + item->offset;
            snprintf(dst, (size_t)item->max /* buffer size, see STRBUF() in settings.c */, "%s", typed);
            printf("[ime] %s set to \"%s\"\n", item->label, dst);
            snprintf(g_statusLine, sizeof(g_statusLine), "%s set to %s (Options to save).", item->label, dst);
        } else {
            // strtol, not atoi: atoi has no way to report "that wasn't
            // a number at all" (both return 0), and silently writing 0
            // for garbage/empty input is exactly the kind of accepted-
            // but-wrong save this project's settings_load() hardening
            // pass (see clamp_to_schema()) was written to prevent on
            // the load side -- worth the same care here on entry.
            char *end = NULL;
            long v = strtol(typed, &end, 10);
            if (end == typed || *end != '\0') {
                snprintf(g_statusLine, sizeof(g_statusLine), "\"%s\" isn't a number -- %s unchanged.", typed, item->label);
            } else {
                // settings_set_i32() already clamps into [item->min,
                // item->max] -- same single source of truth
                // settings_load() itself uses, so a keyboard-typed
                // out-of-range value can't reach g_cfg unclamped any
                // more than a corrupted ini file's value could.
                settings_set_i32(&g_cfg, item, (int32_t)v);
                snprintf(g_statusLine, sizeof(g_statusLine), "%s set to %d (Options to save).", item->label, settings_get_i32(&g_cfg, item));
            }
        }
    } else {
        snprintf(g_statusLine, sizeof(g_statusLine), "Keyboard cancelled -- %s unchanged.", item ? item->label : "field");
    }

    s_lastLoggedStatus = (OrbisDialogStatus)-1; // reset for the next time the dialog opens
    sceImeDialogTerm();
    g_imeDialogOpen = false;
    g_imeItemIndex = -1;
}

static void handle_grouped_settings_input(const OrbisPadData *pad, const OrbisPadData *prevPad, MenuScreen screen)
{
    bool up    = (pad->buttons & ORBIS_PAD_BUTTON_UP)    && !(prevPad->buttons & ORBIS_PAD_BUTTON_UP);
    bool down  = (pad->buttons & ORBIS_PAD_BUTTON_DOWN)  && !(prevPad->buttons & ORBIS_PAD_BUTTON_DOWN);
    bool left  = (pad->buttons & ORBIS_PAD_BUTTON_LEFT);
    bool right = (pad->buttons & ORBIS_PAD_BUTTON_RIGHT);
    bool leftEdge  = left  && !(prevPad->buttons & ORBIS_PAD_BUTTON_LEFT);
    bool rightEdge = right && !(prevPad->buttons & ORBIS_PAD_BUTTON_RIGHT);
    bool triangle = (pad->buttons & ORBIS_PAD_BUTTON_TRIANGLE) && !(prevPad->buttons & ORBIS_PAD_BUTTON_TRIANGLE);
    bool circle   = (pad->buttons & ORBIS_PAD_BUTTON_CIRCLE)   && !(prevPad->buttons & ORBIS_PAD_BUTTON_CIRCLE);
    bool options  = (pad->buttons & ORBIS_PAD_BUTTON_OPTIONS)  && !(prevPad->buttons & ORBIS_PAD_BUTTON_OPTIONS);
    // Cross opens the on-screen keyboard for numeric/string fields --
    // same release-edge fix this app already relied on for WLED Host
    // (see the original bug: opening on Cross's *press* edge left a
    // stale press for the IME dialog scene to consume as an immediate
    // Cancel before any real input could register). Now applies to
    // every numeric field too, not just the one string field.
    bool crossUp  = !(pad->buttons & ORBIS_PAD_BUTTON_CROSS) && (prevPad->buttons & ORBIS_PAD_BUTTON_CROSS);

    int start, count;
    screen_item_range(screen, &start, &count);

    if (up)   g_selectedIndex = start + (g_selectedIndex - start - 1 + count) % count;
    if (down) g_selectedIndex = start + (g_selectedIndex - start + 1) % count;

    // Keep the selected row scrolled into view -- same logic as
    // before, just bounded to this screen's own [start, start+count)
    // range via settings_last_visible_index_for() instead of the
    // whole flat list.
    if (g_selectedIndex < g_scrollOffset) {
        g_scrollOffset = g_selectedIndex;
    } else {
        while (settings_last_visible_index_for(screen, g_scrollOffset) < g_selectedIndex) g_scrollOffset++;
    }

    const MenuItem *item = &kMenuItems[g_selectedIndex];
    // ENUM/BOOL still cycle/toggle directly with D-Pad Left/Right --
    // there's no sensible "type a value" keyboard equivalent for
    // either (what would you type for "Direction"?). Only
    // numeric/string fields moved to the on-screen keyboard this pass.
    if (item->type == FIELD_ENUM || item->type == FIELD_BOOL) {
        if (leftEdge)  settings_set_i32(&g_cfg, item, settings_get_i32(&g_cfg, item) - item->step);
        if (rightEdge) settings_set_i32(&g_cfg, item, settings_get_i32(&g_cfg, item) + item->step);
    } else if (crossUp) {
        open_field_ime_dialog(g_selectedIndex);
    } else if (leftEdge || rightEdge) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Press Cross to edit %s with the on-screen keyboard.", item->label);
    }

    if (triangle) g_screen = SCREEN_UPDATE;
    if (circle) g_screen = SCREEN_HOME;

    if (options) {
        if (settings_save(&g_cfg, AMBIENT_CONFIG_PATH))
            snprintf(g_statusLine, sizeof(g_statusLine), "Saved to %s -- the plugin will pick this up on its own reload check.", AMBIENT_CONFIG_PATH);
        else
            snprintf(g_statusLine, sizeof(g_statusLine), "Save FAILED -- check %s is writable.", AMBIENT_CONFIG_PATH);
    }
}

static void handle_update_input(const OrbisPadData *pad, const OrbisPadData *prevPad, SDL_Renderer *renderer, TextRenderer *font)
{
    // X = select/open (download and install), Circle = cancel/back --
    // do_plugin_update() doesn't open any system dialog, so there's no
    // stale-button/held-input concern here; a plain press-edge is fine for
    // both.
    bool cross  = (pad->buttons & ORBIS_PAD_BUTTON_CROSS)  && !(prevPad->buttons & ORBIS_PAD_BUTTON_CROSS);
    bool circle = (pad->buttons & ORBIS_PAD_BUTTON_CIRCLE) && !(prevPad->buttons & ORBIS_PAD_BUTTON_CIRCLE);
    if (cross) do_plugin_update(renderer, font);
    if (circle) g_screen = SCREEN_HOME;
}

// ---------------- Entry point ----------------

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    // Load existing settings if present; defaults otherwise -- same
    // "defaults first, overlay what's there" behavior as the plugin's
    // own ambient_load_config, so this never surprises a user who
    // already has a tuned ini.
    if (!settings_load(&g_cfg, AMBIENT_CONFIG_PATH)) {
        settings_set_defaults(&g_cfg);
        snprintf(g_statusLine, sizeof(g_statusLine), "No existing config found -- starting from defaults.");
    } else {
        snprintf(g_statusLine, sizeof(g_statusLine), "Loaded %s", AMBIENT_CONFIG_PATH);
    }
    colorpipeline_rebuild_perchannel_gamma_luts(&g_cfg);

    // NOT SDL_INIT_JOYSTICK: this app reads the controller through the
    // native scePad API (pad_init()/scePadReadState below), never
    // through SDL's joystick layer -- there is no SDL_JoystickOpen()
    // call anywhere in this file. Leaving SDL_INIT_JOYSTICK on made SDL
    // claim the DualShock's HID device for its own (unused) joystick
    // subsystem at the same time scePadOpen() claims it natively, which
    // is a known source of a scePad handle that opens successfully but
    // never reports fresh button state -- exactly the "highlight never
    // moves" symptom. Dropping it costs nothing since the joystick
    // subsystem was dead code.
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 1; // nothing to fall back to -- matches the sample's own for(;;) hang on init failure, but we at least return a code
    }

    SDL_Window *window = SDL_CreateWindow("PS4 Ambient Light", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                           FRAME_WIDTH, FRAME_HEIGHT, 0);
    // NOT SDL_CreateRenderer(..., SDL_RENDERER_ACCELERATED) -- confirmed
    // against OpenOrbis's own real, working samples/SDL2 sample: this PS4
    // SDL2 port needs a software renderer bound to the window's own
    // surface, updated via SDL_UpdateWindowSurface(), not
    // SDL_RenderPresent(). The accelerated path silently renders to
    // nothing visible (black screen, no error) on this SDK.
    // (SDL_RenderCopy still works fine against this software renderer for
    // the text atlas below -- only SDL_RenderPresent is the no-op here,
    // and nothing in this file calls it.)
    g_window = window;
    SDL_Surface *windowSurface = SDL_GetWindowSurface(window);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(windowSurface);

    // Bundled under assets/fonts/ in sce_sys packaging (see build.bat)
    // -- app0 is where the packaged .pkg's own files are mounted at
    // runtime, confirmed by every OpenOrbis sample that loads its own
    // assets this way (e.g. samples/input's PNG loads from
    // /app0/assets/images/...).
    TextRenderer *font = text_renderer_create(renderer, "/app0/assets/fonts/font.ttf", 24);
    if (!font) {
        // Matches the old stub's behavior: draw_text() calls are safe
        // no-ops when font is NULL, so a bad/missing font file degrades
        // to "rects/highlighting only" instead of crashing the app.
        printf("[text] text_renderer_create failed -- UI will run without text labels\n");
    }
    g_titleFont = text_renderer_create(renderer, "/app0/assets/fonts/font.ttf", 40);
    if (!g_titleFont) {
        printf("[text] title text_renderer_create failed -- page titles fall back to the body font size\n");
    }

    if (!pad_init()) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Controller not detected -- plug in a DualShock and restart.");
    }

    // Required before any sce*Dialog call (confirmed: orbis/CommonDialog.h).
    // Return code isn't gated on here since the IME dialog's own
    // sceImeDialogInit return code (checked in
    // open_field_ime_dialog) is the more direct signal that
    // something's actually wrong.
    // BUGFIX -- root cause of a real crash on hardware (putty.log:
    // "PRX_NOT_RESOLVED_FUNCTION", Required Module Name:
    // libSceImeDialog), happening the instant Cross was pressed on the
    // WLED Host row. Linking -lSceCommonDialog/-lSceImeDialog (earlier
    // fix) resolves the symbols at LINK time, but the real system
    // .sprx modules still have to be loaded into the running process
    // at RUNTIME before those calls will actually work -- the exact
    // same requirement this file already handles correctly for
    // Net/Http/Ssl in http_init() below, just missed here originally.
    // The crash report's own dynamic-library dump confirms this
    // directly: libSceCommonDialog.sprx IS listed as loaded (because
    // sceCommonDialogInitialize() happened to work -- likely a
    // different, already-resident export), but libSceImeDialog.sprx is
    // NOT in that list at all, i.e. never loaded, so any call into it
    // faults immediately. Real constants confirmed from
    // orbis/_types/sysmodule.h: ORBIS_SYSMODULE_IME_DIALOG (0x0096,
    // comment there literally says "libSceImeDialog.sprx") is loaded
    // via the plain sceSysmoduleLoadModule(), NOT the _Internal
    // variant -- it's a different enum type (OrbisSysModule, not
    // OrbisSysModuleInternal) than Net/Http/Ssl/CommonDialog use.
    // ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG (0x80000018) IS in the
    // Internal family, loaded via sceSysmoduleLoadModuleInternal, same
    // as the Net/Http/Ssl calls already below.
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG) < 0) {
        printf("[main] sceSysmoduleLoadModuleInternal(COMMON_DIALOG) failed\n");
    }
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG) < 0) {
        printf("[main] sceSysmoduleLoadModule(IME_DIALOG) failed\n");
    }

    int commonDialogRet = sceCommonDialogInitialize();
    printf("[main] sceCommonDialogInitialize() = %d\n", commonDialogRet);

    OrbisPadData pad, prevPad;
    memset(&pad, 0, sizeof(pad));
    memset(&prevPad, 0, sizeof(prevPad));

    bool running = true;
    // Declared here (function scope), not as a `static` inside the
    // loop below, specifically so the exit-cleanup code after the
    // loop can still read its last value -- a block-scoped `static`
    // inside `while (running) { ... }` would not be visible once that
    // block's closing brace is passed, `static` only extends storage
    // duration, not visibility. Tracks whether wled-relay was last
    // told "on".
    bool wasSignalOn = false;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
        }

        if (g_padHandle >= 0) {
            prevPad = pad;
            int readRet = scePadReadState(g_padHandle, &pad);
            // Throttled debug line: only print when the raw button
            // mask actually changes, so this doesn't flood the serial
            // console at 30fps but still shows every real press/release
            // -- and shows definitively whether fresh data is arriving
            // at all (compare against SCE_PAD_BUTTON_UP=0x10,
            // RIGHT=0x20, DOWN=0x40, LEFT=0x80 from the real
            // ScePadButtonDataOffset).
            if (pad.buttons != prevPad.buttons || readRet != 0) {
                printf("[pad] readRet=%d buttons=0x%08X (prev=0x%08X)\n",
                       readRet, pad.buttons, prevPad.buttons);
            }
            // sceCommonDialogIsUsed() (confirmed real, orbis/CommonDialog.h)
            // is true while the on-screen keyboard is up -- skip this
            // app's own D-Pad-driven menu handling that frame so it's
            // not fighting the system dialog for the same button
            // presses underneath it.
            bool commonDialogUsed = sceCommonDialogIsUsed();
            // DIAGNOSTIC (temporary, same investigation as update_ime_dialog's
            // logging): if this ever reads false while g_imeDialogOpen is
            // true, our own handle_grouped_settings_input is firing underneath the
            // system keyboard, which could plausibly explain "X closes
            // instead of types" if it's re-entering open_field_ime_dialog
            // or otherwise reacting to that same X press.
            static bool s_lastLoggedCommonDialogUsed = true; // mismatched on purpose so the first real frame logs
            if (commonDialogUsed != s_lastLoggedCommonDialogUsed) {
                printf("[main] sceCommonDialogIsUsed() = %d (g_imeDialogOpen=%d)\n", (int)commonDialogUsed, (int)g_imeDialogOpen);
                s_lastLoggedCommonDialogUsed = commonDialogUsed;
            }
            if (!commonDialogUsed) {
                if (g_screen == SCREEN_HOME) handle_home_input(&pad, &prevPad, renderer, font);
                else if (g_screen == SCREEN_SETUP) handle_grouped_settings_input(&pad, &prevPad, MENU_SCREEN_SETUP);
                else if (g_screen == SCREEN_CUSTOMIZE) handle_grouped_settings_input(&pad, &prevPad, MENU_SCREEN_CUSTOMIZE);
                else if (g_screen == SCREEN_UPDATE) handle_update_input(&pad, &prevPad, renderer, font);
            }
        }

        if (g_imeDialogOpen) update_ime_dialog();

        // This app has no idle mode -- update_live_preview() below
        // runs every frame regardless of g_screen, so "driving" is
        // simply g_cfg.relaySignalEnabled itself. Comparing against
        // last frame's value and firing only on a real transition
        // covers app startup (wasSignalOn starts false; if the loaded
        // config already has it enabled, this fires "on" on the very
        // first frame) and a live in-app toggle on Customisation,
        // with the same one check.
        if (g_cfg.relaySignalEnabled != wasSignalOn) {
            relay_send_external_source(g_cfg.relaySignalEnabled);
            wasSignalOn = g_cfg.relaySignalEnabled;
        }

        update_live_preview();

        SDL_SetRenderDrawColor(renderer, COL_BG.r, COL_BG.g, COL_BG.b, 255);
        SDL_RenderClear(renderer);

        if (g_screen == SCREEN_HOME) render_home_screen(renderer, font);
        else if (g_screen == SCREEN_SETUP) render_setup_screen(renderer, font);
        else if (g_screen == SCREEN_CUSTOMIZE) render_customize_screen(renderer, font);
        else if (g_screen == SCREEN_UPDATE) render_update_screen(renderer, font);

        // Propagate the software-rendered surface to the actual screen --
        // SDL_RenderPresent() would be a no-op here, see the renderer
        // setup comment above for why.
        SDL_UpdateWindowSurface(window);
        usleep(1000000 / 30); // 30fps UI refresh -- this app has no reason to run faster
    }

    // This app has no plugin_unload-style guaranteed teardown hook --
    // it's a normal SDL app that just exits its own loop. If
    // wled-relay was last told "on" (app closed with Relay Signal
    // enabled), send one last "off" so it isn't left waiting on a flag
    // this process can no longer touch. Matches wasSignalOn's own
    // state from the loop above -- deliberately NOT re-derived from
    // g_cfg.relaySignalEnabled here, since by this point the loop has
    // already exited and that value is just whatever it happened to
    // be on the final frame (though in practice, for this app, those
    // two things can't actually disagree -- wasSignalOn IS
    // g_cfg.relaySignalEnabled by construction, the moment they last
    // differed).
    if (wasSignalOn) {
        relay_send_external_source(false);
    }

    text_renderer_destroy(font);
    text_renderer_destroy(g_titleFont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}