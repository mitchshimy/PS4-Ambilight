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
#include <unistd.h>

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
// comment above open_wled_host_ime_dialog() for exactly what's
// confirmed vs. assumed (no working IME-dialog sample exists in this
// SDK to check against; samples/keyboard is a *physical* USB/BT
// keyboard API, orbis/Keyboard.h, a different thing entirely, despite
// this project's own earlier README implying otherwise).
#include <orbis/ImeDialog.h>
#include <orbis/CommonDialog.h>
#include <wchar.h>

#include "settings.h"
#include "color_pipeline.h"
#include "ddp.h"
#include "config.h"

#define FRAME_WIDTH  1920
#define FRAME_HEIGHT 1080

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

typedef enum { SCREEN_SETTINGS, SCREEN_UPDATE, SCREEN_QUIT } Screen;

static AmbientConfig g_cfg;
static int g_selectedIndex = 0;
// Index of the first menu item drawn -- lets the list scroll once there
// are more items than fit on screen. See settings_last_visible_index()
// and its call site in handle_settings_input() for how this stays in
// sync with g_selectedIndex, and render_settings_screen() for where
// it's consumed.
static int g_scrollOffset = 0;
static Screen g_screen = SCREEN_SETTINGS;
static char g_statusLine[256] = "";

// A handful of representative test colors so every pipeline stage
// (gamma, per-channel gamma, brightness, contrast, saturation,
// levels, color order) is visible at once, not just one arbitrary
// swatch. Pure red/green/blue exercise saturation/color-order/
// per-channel controls; white and a mid-gray exercise brightness/
// contrast/levels without a hue to complicate reading the result.
#define NUM_PREVIEW_SWATCHES 5
static const uint8_t kPreviewInputs[NUM_PREVIEW_SWATCHES][3] = {
    {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}, {128, 128, 128}
};

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

static void do_plugin_update(void)
{
    snprintf(g_statusLine, sizeof(g_statusLine), "Downloading plugin...");
    const char *tmpPath = "/data/ps4_ambient_light_update.tmp";
    if (!http_download(PLUGIN_UPDATE_URL, tmpPath)) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Download FAILED -- check PLUGIN_UPDATE_URL and network.");
        return;
    }
    // Move into place. sceKernelRename isn't assumed available/named
    // that in every OpenOrbis version -- copy+delete via
    // sceKernelOpen/Read/Write/Close instead, using primitives already
    // proven working above and in config.c, rather than a rename call
    // this session hasn't verified exists under that name here.
    int32_t src = sceKernelOpen(tmpPath, 0, 0777);
    if (src < 0) { snprintf(g_statusLine, sizeof(g_statusLine), "Update failed: couldn't reopen downloaded file."); return; }
    int32_t dst = sceKernelOpen(PLUGIN_PRX_PATH, 0x200 | 0x001, 0777);
    if (dst < 0) { sceKernelClose(src); snprintf(g_statusLine, sizeof(g_statusLine), "Update failed: can't write to %s", PLUGIN_PRX_PATH); return; }
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
    if (!copyOk) { snprintf(g_statusLine, sizeof(g_statusLine), "Update failed: copy into plugins/ was incomplete."); return; }

    if (!register_plugin_in_goldhen()) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Plugin installed, but plugins.ini update failed -- add it manually.");
        return;
    }
    snprintf(g_statusLine, sizeof(g_statusLine), "Updated. Relaunch your game to load the new build.");
}

// ---------------- Live preview ----------------

// Recomputed every frame from the current settings (cheap -- 5
// swatches through a table-lookup-heavy pipeline, nowhere near a
// per-frame cost concern) and both rendered on screen AND pushed to
// the real WLED light so what the user sees in the app matches what
// their actual strip shows, not a simulation of it.
static void update_live_preview(void)
{
    colorpipeline_rebuild_perchannel_gamma_luts(&g_cfg);

    uint8_t wireBytes[NUM_PREVIEW_SWATCHES * 3];
    for (int i = 0; i < NUM_PREVIEW_SWATCHES; i++) {
        colorpipeline_process(&g_cfg, kPreviewInputs[i][0], kPreviewInputs[i][1], kPreviewInputs[i][2],
                               &wireBytes[i * 3]);
    }
    ddp_send_rgb_zones(g_cfg.wledHost, g_cfg.wledPort, wireBytes, NUM_PREVIEW_SWATCHES);
}

// ---------------- Rendering ----------------

static void draw_text(SDL_Renderer *renderer, TextRenderer *font, int x, int y, const char *text, SDL_Color color)
{
    text_renderer_draw(renderer, font, x, y, text, color);
}

static void format_item_value(const MenuItem *item, const AmbientConfig *cfg, char *out, size_t outSize)
{
    if (item->type == FIELD_STRING) {
        snprintf(out, outSize, "%s", cfg->wledHost); // only STRING field in this schema
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

// Mirrors render_settings_screen()'s row layout (section headers + row
// height + the same bottom clamp) without drawing anything, so
// handle_settings_input() can tell whether a given item index is
// actually on screen for a given scroll offset. Kept deliberately in
// lockstep with render_settings_screen() below -- if that function's
// pixel layout changes, this has to change with it, or scrolling and
// rendering will disagree about what's visible.
static int settings_last_visible_index(int scrollOffset)
{
    int y = 90;
    const char *currentSection = "";
    int last = scrollOffset - 1; // nothing shown yet
    for (int i = scrollOffset; i < kMenuItemCount; i++) {
        const MenuItem *item = &kMenuItems[i];
        if (strcmp(currentSection, item->section) != 0) {
            currentSection = item->section;
            y += 34;
        }
        y += 30;
        if (y > FRAME_HEIGHT - 260) break;
        last = i;
    }
    return last;
}

static void render_settings_screen(SDL_Renderer *renderer, TextRenderer *font)
{
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 60, 255};
    SDL_Color gray = {160, 160, 160, 255};

    draw_text(renderer, font, 60, 30, "PS4 Ambient Light -- Settings  (D-Pad: navigate/adjust, Cross: edit text, Triangle: Update Plugin, Options: Save & Quit)", gray);

    int y = 90;
    const char *currentSection = "";
    int lastShown = g_scrollOffset - 1; // nothing drawn yet
    for (int i = g_scrollOffset; i < kMenuItemCount; i++) {
        const MenuItem *item = &kMenuItems[i];
        if (strcmp(currentSection, item->section) != 0) {
            currentSection = item->section;
            char sectionLabel[32];
            snprintf(sectionLabel, sizeof(sectionLabel), "[%s]", currentSection);
            draw_text(renderer, font, 60, y, sectionLabel, yellow);
            y += 34;
        }
        char valueStr[80];
        format_item_value(item, &g_cfg, valueStr, sizeof(valueStr));
        char line[160];
        snprintf(line, sizeof(line), "%-20s %s", item->label, valueStr);

        if (i == g_selectedIndex) {
            SDL_SetRenderDrawColor(renderer, 40, 80, 40, 255);
            SDL_Rect hl = { 50, y - 4, 700, 30 };
            SDL_RenderFillRect(renderer, &hl);
        }
        draw_text(renderer, font, 70, y, line, i == g_selectedIndex ? yellow : white);
        y += 30;
        lastShown = i;
        if (y > FRAME_HEIGHT - 260) break; // same clamp as before -- now paired with scrolling instead of just cutting the list off
    }

    // Single scroll-position line between the header and the list --
    // avoids drawing anything near the bottom clamp, where there isn't
    // reliable vertical room between the last row and the live-preview
    // swatches below it. ASCII-only since the bundled font's glyph
    // coverage isn't guaranteed beyond that (same reasoning as the IME
    // buffer's manual char<->wchar_t widening elsewhere in this file).
    if (g_scrollOffset > 0 || lastShown < kMenuItemCount - 1) {
        char scrollLine[64];
        snprintf(scrollLine, sizeof(scrollLine), "-- showing %d-%d of %d --",
                 g_scrollOffset + 1, lastShown + 1, kMenuItemCount);
        draw_text(renderer, font, 700, 64, scrollLine, gray);
    }

    // Live preview swatches, actually reflecting the current settings
    // through the real pipeline -- not decoration.
    int swatchX = 900, swatchY = 120, swatchSize = 140;
    const char *swatchLabels[NUM_PREVIEW_SWATCHES] = {"Red in", "Green in", "Blue in", "White in", "Gray in"};
    for (int i = 0; i < NUM_PREVIEW_SWATCHES; i++) {
        uint8_t rgbOut[3];
        // NOTE: colorpipeline_process's out3 is in WIRE order (per
        // colorOrder); for an on-screen "what does this really look
        // like" swatch we want true RGB, so remap back for display --
        // reprocess and only take the RGB triplet before the final
        // wire-order write. Simplest correct way: call the pipeline
        // manually up to levels, or just always preview with RGB order
        // for the on-screen swatch regardless of the configured wire
        // order. Second option chosen here since it's simpler and the
        // real color-order effect matters for the STRIP, which the DDP
        // send already exercises correctly -- the screen swatch's job
        // is showing color-correctness, not wire-order.
        AmbientConfig previewCfg = g_cfg;
        previewCfg.colorOrder = ORDER_RGB;
        colorpipeline_process(&previewCfg, kPreviewInputs[i][0], kPreviewInputs[i][1], kPreviewInputs[i][2], rgbOut);

        SDL_SetRenderDrawColor(renderer, rgbOut[0], rgbOut[1], rgbOut[2], 255);
        SDL_Rect sw = { swatchX + i * (swatchSize + 20), swatchY, swatchSize, swatchSize };
        SDL_RenderFillRect(renderer, &sw);
        draw_text(renderer, font, swatchX + i * (swatchSize + 20), swatchY + swatchSize + 8, swatchLabels[i], gray);
    }
    draw_text(renderer, font, swatchX, swatchY - 40, "Live preview (also sent to your real WLED light right now):", gray);

    draw_text(renderer, font, 60, FRAME_HEIGHT - 60, g_statusLine, yellow);
}

static void render_update_screen(SDL_Renderer *renderer, TextRenderer *font)
{
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {160, 160, 160, 255};
    draw_text(renderer, font, 60, 30, "Update Plugin  (X: download and install now, Circle: back)", gray);
    draw_text(renderer, font, 60, 100, "This fetches the latest ps4_ambient_light.prx from:", white);
    draw_text(renderer, font, 60, 130, PLUGIN_UPDATE_URL, gray);
    draw_text(renderer, font, 60, 200, g_statusLine, white);
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

// ---------------- On-screen keyboard (wledHost field) ----------------
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
// This needs a real hardware run to confirm the dialog actually shows
// where/how expected -- flagging that plainly rather than treating a
// clean compile as proof.
//
// wledHost is always plain ASCII (IP address or hostname), so this
// widens/narrows manually instead of via mbstowcs/wcstombs -- avoids
// depending on locale support that hasn't been checked in this SDK's
// libc, for a charset simple enough not to need it.
static bool g_imeDialogOpen = false;
static wchar_t g_imeBuffer[64];

static void open_wled_host_ime_dialog(void)
{
    if (g_imeDialogOpen || sceCommonDialogIsUsed()) return;

    // NOT L"..." literals: this SDK's own wchar.h resolves wchar_t to
    // unsigned short (2 bytes, UTF-16-style, matching what
    // OrbisImeDialogSetting's placeholder/title/inputTextBuffer fields
    // actually expect -- confirmed by a real build's own compiler
    // warning: "assigning to 'const wchar_t *' (aka 'const unsigned
    // short *')"). But clang's built-in L"..." literal type is fixed
    // by the target's __WCHAR_TYPE__ (4-byte int on this
    // x86_64-freebsd target) regardless of that header typedef -- a
    // typedef can't override what the compiler produces for a wide
    // string literal. So L"..." here silently builds the wrong-width
    // array and gets flagged as an incompatible pointer type. These
    // are plain ASCII, so populated element-by-element into an
    // explicitly-sized wchar_t array instead, same reasoning as the
    // manual char<->wchar_t widening used elsewhere in this function
    // rather than trusting mbstowcs/wcstombs.
    static const char kPlaceholderAscii[] = "192.168.x.x";
    static const char kTitleAscii[] = "WLED Host";
    static wchar_t placeholderBuf[sizeof(kPlaceholderAscii)];
    static wchar_t titleBuf[sizeof(kTitleAscii)];
    for (size_t i = 0; i < sizeof(kPlaceholderAscii); i++) placeholderBuf[i] = (wchar_t)(unsigned char)kPlaceholderAscii[i];
    for (size_t i = 0; i < sizeof(kTitleAscii); i++) titleBuf[i] = (wchar_t)(unsigned char)kTitleAscii[i];

    memset(g_imeBuffer, 0, sizeof(g_imeBuffer));
    size_t i = 0;
    for (; i < sizeof(g_imeBuffer) / sizeof(g_imeBuffer[0]) - 1 && g_cfg.wledHost[i] != '\0'; i++) {
        g_imeBuffer[i] = (wchar_t)(unsigned char)g_cfg.wledHost[i];
    }

    OrbisImeDialogSetting setting;
    memset(&setting, 0, sizeof(setting));
    setting.userId = g_userId;
    setting.type = ORBIS_TYPE_BASIC_LATIN;
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
    printf("[ime] sceImeDialogInit() = %d\n", ret);
    if (ret == 0) {
        g_imeDialogOpen = true;
    } else {
        snprintf(g_statusLine, sizeof(g_statusLine), "Couldn't open the keyboard (sceImeDialogInit = %d).", ret);
    }
}

// Call once per frame from the main loop while g_imeDialogOpen.
static void update_ime_dialog(void)
{
    OrbisDialogStatus status = sceImeDialogGetStatus();
    // DIAGNOSTIC (temporary): reported symptom is the dialog closing on
    // the first character selected, not just on an actual Enter/Close.
    // No working PS4 sample exists to check OrbisImeDialogSetting's
    // real behavior against, and the closest documented analog
    // (Vita's SceImeDialogButton: ENTER and CLOSE are distinct from
    // ordinary character input) suggests this shouldn't happen by
    // design -- so before changing any setting/option blind, log every
    // status transition so the next putty.log capture shows exactly
    // what's actually being reported the instant X is pressed on a
    // letter, rather than guessing at a fix that might not address the
    // real cause.
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

    if (result.endstatus == ORBIS_DIALOG_OK) {
        size_t i = 0;
        for (; i < sizeof(g_cfg.wledHost) - 1 && g_imeBuffer[i] != L'\0'; i++) {
            g_cfg.wledHost[i] = (char)g_imeBuffer[i];
        }
        g_cfg.wledHost[i] = '\0';
        printf("[ime] result text (widened back to char): \"%s\"\n", g_cfg.wledHost);
        snprintf(g_statusLine, sizeof(g_statusLine), "WLED Host set to %s (Options to save).", g_cfg.wledHost);
    } else {
        snprintf(g_statusLine, sizeof(g_statusLine), "Keyboard cancelled -- WLED Host unchanged.");
    }

    s_lastLoggedStatus = (OrbisDialogStatus)-1; // reset for the next time the dialog opens
    sceImeDialogTerm();
    g_imeDialogOpen = false;
}

static void handle_settings_input(const OrbisPadData *pad, const OrbisPadData *prevPad)
{
    bool up    = (pad->buttons & ORBIS_PAD_BUTTON_UP)    && !(prevPad->buttons & ORBIS_PAD_BUTTON_UP);
    bool down  = (pad->buttons & ORBIS_PAD_BUTTON_DOWN)  && !(prevPad->buttons & ORBIS_PAD_BUTTON_DOWN);
    bool left  = (pad->buttons & ORBIS_PAD_BUTTON_LEFT);
    bool right = (pad->buttons & ORBIS_PAD_BUTTON_RIGHT);
    bool leftEdge  = left  && !(prevPad->buttons & ORBIS_PAD_BUTTON_LEFT);
    bool rightEdge = right && !(prevPad->buttons & ORBIS_PAD_BUTTON_RIGHT);
    bool triangle = (pad->buttons & ORBIS_PAD_BUTTON_TRIANGLE) && !(prevPad->buttons & ORBIS_PAD_BUTTON_TRIANGLE);
    bool options  = (pad->buttons & ORBIS_PAD_BUTTON_OPTIONS)  && !(prevPad->buttons & ORBIS_PAD_BUTTON_OPTIONS);
    // BUGFIX: open the keyboard on Cross's RELEASE edge, not its press edge.
    // putty.log showed sceImeDialogGetResult() coming back CANCEL within a
    // couple of frames of sceImeDialogInit(), with no second Cross press
    // logged in between -- i.e. the dialog was cancelling itself before the
    // user could have tapped anything. The pad trace makes the mechanism
    // clear: right after ImeDialogBaseScene goes Alive, the *same* Cross
    // press that triggered open_wled_host_ime_dialog() is still being
    // reported as held (buttons=0x80004000, ORBIS_PAD_BUTTON_INTERCEPTED |
    // CROSS) before it's released a frame later (0x80000000). Opening the
    // dialog on the press edge means Cross is still physically down the
    // instant the system UI takes focus, so that leftover press is what the
    // dialog scene sees as its first input -- landing on Close/Cancel
    // instead of any key the user goes on to actually press. Triggering on
    // release instead guarantees Cross is already up by the time
    // sceImeDialogInit() runs, so there's no stale press left for the
    // dialog to consume as an immediate Cancel.
    //
    // Cross stays the select/open button here (X = select, Circle =
    // cancel) -- that's confirmed correct for this console; the earlier
    // "Circle selects" read turned out to be this same stale-press bug
    // showing up as the on-screen keyboard closing on the first X instead
    // of registering a keystroke, not an actual button-assignment swap.
    bool crossUp  = !(pad->buttons & ORBIS_PAD_BUTTON_CROSS) && (prevPad->buttons & ORBIS_PAD_BUTTON_CROSS);

    if (up)   g_selectedIndex = (g_selectedIndex - 1 + kMenuItemCount) % kMenuItemCount;
    if (down) g_selectedIndex = (g_selectedIndex + 1) % kMenuItemCount;

    // Keep the selected row scrolled into view. Scrolling up is direct
    // (the selected index just becomes the new top row); scrolling down
    // has to walk forward since a page's worth of rows varies with how
    // many section headers fall inside it. Also correctly handles
    // wraparound at either end of the list: Down from the last item
    // sets g_selectedIndex to 0, which is always < g_scrollOffset once
    // scrolled, snapping the view back to the top; Up from the first
    // item sets it to the last item, which the while loop below scrolls
    // down to reveal.
    if (g_selectedIndex < g_scrollOffset) {
        g_scrollOffset = g_selectedIndex;
    } else {
        while (settings_last_visible_index(g_scrollOffset) < g_selectedIndex) g_scrollOffset++;
    }

    const MenuItem *item = &kMenuItems[g_selectedIndex];
    if (item->type != FIELD_STRING) {
        if (leftEdge)  settings_set_i32(&g_cfg, item, settings_get_i32(&g_cfg, item) - item->step);
        if (rightEdge) settings_set_i32(&g_cfg, item, settings_get_i32(&g_cfg, item) + item->step);
    } else if (crossUp) {
        open_wled_host_ime_dialog();
    } else if (leftEdge || rightEdge) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Press Cross to edit WLED Host with the on-screen keyboard.");
    }

    if (triangle) g_screen = SCREEN_UPDATE;

    if (options) {
        if (settings_save(&g_cfg, AMBIENT_CONFIG_PATH))
            snprintf(g_statusLine, sizeof(g_statusLine), "Saved to %s -- the plugin will pick this up on its own reload check.", AMBIENT_CONFIG_PATH);
        else
            snprintf(g_statusLine, sizeof(g_statusLine), "Save FAILED -- check %s is writable.", AMBIENT_CONFIG_PATH);
    }
}

static void handle_update_input(const OrbisPadData *pad, const OrbisPadData *prevPad)
{
    // X = select/open (download and install), Circle = cancel/back --
    // do_plugin_update() doesn't open any system dialog, so there's no
    // stale-button/held-input concern here; a plain press-edge is fine for
    // both.
    bool cross  = (pad->buttons & ORBIS_PAD_BUTTON_CROSS)  && !(prevPad->buttons & ORBIS_PAD_BUTTON_CROSS);
    bool circle = (pad->buttons & ORBIS_PAD_BUTTON_CIRCLE) && !(prevPad->buttons & ORBIS_PAD_BUTTON_CIRCLE);
    if (cross) do_plugin_update();
    if (circle) g_screen = SCREEN_SETTINGS;
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

    if (!pad_init()) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Controller not detected -- plug in a DualShock and restart.");
    }

    // Required before any sce*Dialog call (confirmed: orbis/CommonDialog.h).
    // Return code isn't gated on here since the IME dialog's own
    // sceImeDialogInit return code (checked in
    // open_wled_host_ime_dialog) is the more direct signal that
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
            // true, our own handle_settings_input is firing underneath the
            // system keyboard, which could plausibly explain "X closes
            // instead of types" if it's re-entering open_wled_host_ime_dialog
            // or otherwise reacting to that same X press.
            static bool s_lastLoggedCommonDialogUsed = true; // mismatched on purpose so the first real frame logs
            if (commonDialogUsed != s_lastLoggedCommonDialogUsed) {
                printf("[main] sceCommonDialogIsUsed() = %d (g_imeDialogOpen=%d)\n", (int)commonDialogUsed, (int)g_imeDialogOpen);
                s_lastLoggedCommonDialogUsed = commonDialogUsed;
            }
            if (!commonDialogUsed) {
                if (g_screen == SCREEN_SETTINGS) handle_settings_input(&pad, &prevPad);
                else if (g_screen == SCREEN_UPDATE) handle_update_input(&pad, &prevPad);
            }
        }

        if (g_imeDialogOpen) update_ime_dialog();

        update_live_preview();

        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
        SDL_RenderClear(renderer);

        if (g_screen == SCREEN_SETTINGS) render_settings_screen(renderer, font);
        else if (g_screen == SCREEN_UPDATE) render_update_screen(renderer, font);

        // Propagate the software-rendered surface to the actual screen --
        // SDL_RenderPresent() would be a no-op here, see the renderer
        // setup comment above for why.
        SDL_UpdateWindowSurface(window);
        usleep(1000000 / 30); // 30fps UI refresh -- this app has no reason to run faster
    }

    text_renderer_destroy(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}