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
// NOTE: SDL2_ttf is NOT confirmed to link in this SDK snapshot -- the
// real OpenOrbis SDL2 sample's own build.bat links only
// -lSDL2 -lSDL2_image -lSceFreeType, never -lSDL2_ttf, and no compiled
// SDL2_ttf library was found anywhere in the SDK, only this header.
// See build.bat's header comment for the three real options to
// resolve this before this file will actually link. Left calling the
// real SDL2_ttf API (not stubbed out) so the gap surfaces as a clear
// link error pointing at these exact symbols, not a silently
// text-less UI nobody asked for.
#include <SDL2/SDL_ttf.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>

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

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, int x, int y, const char *text, SDL_Color color)
{
    if (!font) return; // font failed to load -- degrade to no text rather than crash; rects still render
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_FreeSurface(surf);
    if (tex) { SDL_RenderCopy(renderer, tex, NULL, &dst); SDL_DestroyTexture(tex); }
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

static void render_settings_screen(SDL_Renderer *renderer, TTF_Font *font)
{
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 60, 255};
    SDL_Color gray = {160, 160, 160, 255};

    draw_text(renderer, font, 60, 30, "PS4 Ambient Light -- Settings  (D-Pad: navigate/adjust, Triangle: Update Plugin, Options: Save & Quit)", gray);

    int y = 90;
    const char *currentSection = "";
    for (int i = 0; i < kMenuItemCount; i++) {
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
        if (y > FRAME_HEIGHT - 260) break; // simple v1 clamp -- scrolling is a follow-up, not in this pass
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

static void render_update_screen(SDL_Renderer *renderer, TTF_Font *font)
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

// Verified against OpenOrbis samples/input/input/controller.cpp's real
// init sequence -- there is no ORBIS_USER_SERVICE_USER_ID_SYSTEM
// constant (that was wrong in an earlier draft of this file); the
// user ID has to come from sceUserServiceGetInitialUser, and
// scePadOpen's 4th argument is NULL for standard use, not a params
// struct (OrbisPadExtParam is for scePadOpenExt, a different call).
static bool pad_init(void)
{
    if (scePadInit() != 0) return false;

    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);

    int32_t userID = -1;
    sceUserServiceGetInitialUser(&userID);

    g_padHandle = scePadOpen(userID, 0, 0, NULL);
    return g_padHandle >= 0;
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

    if (up)   g_selectedIndex = (g_selectedIndex - 1 + kMenuItemCount) % kMenuItemCount;
    if (down) g_selectedIndex = (g_selectedIndex + 1) % kMenuItemCount;

    const MenuItem *item = &kMenuItems[g_selectedIndex];
    if (item->type != FIELD_STRING) { // string field (wledHost) needs the IME dialog, not +/- -- not wired up in this pass, see status line
        if (leftEdge)  settings_set_i32(&g_cfg, item, settings_get_i32(&g_cfg, item) - item->step);
        if (rightEdge) settings_set_i32(&g_cfg, item, settings_get_i32(&g_cfg, item) + item->step);
    } else if (leftEdge || rightEdge) {
        snprintf(g_statusLine, sizeof(g_statusLine), "WLED Host is edited via FTP for now -- on-screen keyboard entry is a follow-up.");
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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        return 1; // nothing to fall back to -- matches the sample's own for(;;) hang on init failure, but we at least return a code
    }
    if (TTF_Init() != 0) {
        // Non-fatal -- draw_text() checks for a NULL font and skips
        // text rather than crashing; the app is still usable with
        // rectangles/highlighting alone if fonts genuinely can't load.
    }

    SDL_Window *window = SDL_CreateWindow("PS4 Ambient Light", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                           FRAME_WIDTH, FRAME_HEIGHT, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Bundled under assets/fonts/ in sce_sys packaging (see build.bat)
    // -- app0 is where the packaged .pkg's own files are mounted at
    // runtime, confirmed by every OpenOrbis sample that loads its own
    // assets this way (e.g. samples/input's PNG loads from
    // /app0/assets/images/...).
    TTF_Font *font = TTF_OpenFont("/app0/assets/fonts/font.ttf", 22);

    if (!pad_init()) {
        snprintf(g_statusLine, sizeof(g_statusLine), "Controller not detected -- plug in a DualShock and restart.");
    }

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
            scePadReadState(g_padHandle, &pad);
            if (g_screen == SCREEN_SETTINGS) handle_settings_input(&pad, &prevPad);
            else if (g_screen == SCREEN_UPDATE) handle_update_input(&pad, &prevPad);
        }

        update_live_preview();

        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
        SDL_RenderClear(renderer);

        if (g_screen == SCREEN_SETTINGS) render_settings_screen(renderer, font);
        else if (g_screen == SCREEN_UPDATE) render_update_screen(renderer, font);

        SDL_RenderPresent(renderer);
        usleep(1000000 / 30); // 30fps UI refresh -- this app has no reason to run faster
    }

    if (font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
