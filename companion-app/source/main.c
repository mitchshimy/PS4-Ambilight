// main.c -- ps4_ambient_light companion app.
// More: docs/code-comments.md (companion-app/source/main.c:1)

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
#include <orbis/SystemService.h>

#include "settings.h"
#include "ime.h"
#include "net_http.h"
#include "app_state.h"
#include "app_paths.h"
#include "file_util.h"
#include "update_check.h"
#include "plugin_install.h"
#include "fw_check.h"
#include "shell_feed.h"
#include "presets.h"
#include "color_pipeline.h"
#include "ddp.h"
#include "relay_signal.h"
#include "layout.h"
#include "sha256.h"
#include "notify.h"
#include "plugin_common.h" // ORBIS_O_CREAT_TRUNC_WRONLY -- see its BUG FIX comment
#include "ui_canvas.h"
#include "ui_theme.h"
#include "ui_screens.h"
#include "led_frame.h"
#include "pad_input.h"     // buffered controller samples -> button events (host-tested)
#include "frame_prof.h"    // slow-iteration log
#include "scroll_ease.h"   // animated scrolling
#include "ui_widgets.h"   // CONTENT_TOP / CONTENT_H for scroll clamping
#include "help_qr.h"
#include "install_plan.h"  // shell feed + plugin: what is pending, what the Home button does
#include "claim_file.h"    // /data/ps4-ambilight/ps4_ambient_state: keeps the shell feed off the strip while this app drives it
#include "payload_ini.h"   // one-entry edits to GoldHEN's payload AutoRun list
#include "goldhen_config.h" // GoldHEN's own "Enable Plugins Loader" switch, read and flipped in place
#include "update_source.h"  // cleans up [dev] update_base_url

// TCP headers for GoldHEN's BinLoader (port 9090).
// More: docs/code-comments.md (companion-app/source/main.c:86)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FRAME_WIDTH  1920
#define FRAME_HEIGHT 1080

// ---------------- global state ----------------

AmbientConfig g_cfg;
UiState g_state;
bool g_dirty = true;

// [shell] enabled as it is in the ini on disk (set at startup and after every successful save).
// More: docs/code-comments.md (companion-app/source/main.c:107)
static bool g_shellEnabledOnDisk = false;

// True while the PS button has put this app in the background.
// More: docs/code-comments.md (companion-app/source/main.c:113)
static bool g_appBackgrounded = false;

// Set up / Customization have no status line (only Home draws it), so a hint for the row you just...
// More: docs/code-comments.md (companion-app/source/main.c:118)

// Held so do_plugin_update() can force an interim "Downloading..." frame before its blocking...
// More: docs/code-comments.md (companion-app/source/main.c:122)
static UiCanvas *g_canvas = NULL;
static const UiFonts *g_fonts = NULL;
static SDL_Surface *g_windowSurface = NULL;
static SDL_Surface *g_canvasSurface = NULL;
static SDL_Window *g_window = NULL;

void set_status(const char *msg, bool isError)
{
    snprintf(g_state.statusLine, sizeof(g_state.statusLine), "%s", msg);
    g_state.statusIsSaved = false;
    g_state.statusIsError = isError;
    g_dirty = true;
}

// Cached snapshot of ui_render_home_static()'s output -- everything on Home except the LED edge frame.
// More: docs/code-comments.md (companion-app/source/main.c:140)
static uint32_t *g_homeStaticCache = NULL;
static bool g_homeStaticCacheValid = false;

// Cards of Set up / Customization / Help, painted once into a layer (see ui_render_cached).
static UiScreenCache *g_screenCache = NULL;

// Microsecond clock for the slow-iteration log. SDL_GetTicks is only 1 ms and far too coarse to split a frame.
static uint64_t now_us(void)
{
    static uint64_t freq = 0;
    if (!freq) { freq = SDL_GetPerformanceFrequency(); if (!freq) freq = 1000000u; }
    return (uint64_t)((double)SDL_GetPerformanceCounter() * 1e6 / (double)freq);
}
static FrameProf g_prof;
// Which stage the loop is in, so an interim frame drawn from deep inside blocking work (install, repair)
// charges that work to the right stage instead of to "render".
static FrameStage g_profStage = FP_INPUT;

void render_and_present(void)
{
    frame_prof_mark(&g_prof, g_profStage, now_us());
    if (g_state.screen == UI_SCREEN_HOME) {
        // Drawn in two pieces (not ui_render()) specifically so the static part can be snapshotted before the...
        // More: docs/code-comments.md (companion-app/source/main.c:153)
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
        ui_render_cached(g_canvas, g_fonts, &g_cfg, &g_state, g_screenCache);
        // Left Home -- rebuild the cache from scratch next time we're back rather than trust a snapshot from a...
        // More: docs/code-comments.md (companion-app/source/main.c:167)
        g_homeStaticCacheValid = false;
    }
    frame_prof_mark(&g_prof, FP_RENDER, now_us());
    SDL_BlitSurface(g_canvasSurface, NULL, g_windowSurface, NULL);
    SDL_UpdateWindowSurface(g_window);
    frame_prof_mark(&g_prof, FP_PRESENT, now_us());
}

// The actual fast path: reuse the cached static layer, redraw only the (now cheap -- see led_frame.c)...
// More: docs/code-comments.md (companion-app/source/main.c:176)
static void render_home_fast(void)
{
    frame_prof_mark(&g_prof, g_profStage, now_us());
    if (!g_homeStaticCache) return; // shouldn't happen (see g_homeStaticCacheValid's invariant), but cheap to guard
    memcpy(g_canvas->px, g_homeStaticCache, (size_t)FRAME_WIDTH * FRAME_HEIGHT * sizeof(uint32_t));
    led_frame_draw(g_canvas, g_state.ledSlots, g_state.ledSlotCount,
                    g_state.ledColors, g_state.ledColorCount);
    frame_prof_mark(&g_prof, FP_RENDER, now_us());
    SDL_BlitSurface(g_canvasSurface, NULL, g_windowSurface, NULL);
    SDL_UpdateWindowSurface(g_window);
    frame_prof_mark(&g_prof, FP_PRESENT, now_us());
}

// button auto-repeat Holding a D-Pad direction or L1/R1 now fires repeatedly instead of once per press...
// More: docs/code-comments.md (companion-app/source/main.c:190)

// Held D-Pad / L1 / R1 auto-repeat and the press / release edges live in pad_input.c.

// Scroll position: g_scroll.target is where the view is going, g_state.scrollY (what the renderer reads)
// is where it is now, eased toward it every frame by scroll_animate().
static ScrollEase g_scroll;
static void scroll_snap(float v) { scroll_ease_snap(&g_scroll, v); g_state.scrollY = v; }
// Returns true while the view is still moving.
static bool scroll_animate(float dtSec)
{
    float before = g_state.scrollY;
    bool moving = scroll_ease_step(&g_scroll, dtSec);
    g_state.scrollY = g_scroll.pos;
    if (g_state.scrollY != before) g_dirty = true;
    return moving;
}

// field navigation Row structure comes from ui_screen_field_rows() (ui_screens.c), which is the exact...
// More: docs/code-comments.md (companion-app/source/main.c:229)

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

// Keeps the focused row inside the .content viewport, the way the browser's scroll-into-view would.
// More: docs/code-comments.md (companion-app/source/main.c:263)
static void scroll_to_focus(const UiFonts *fonts, int fieldCount)
{
    float maxScroll = ui_screen_content_height(fonts, &g_cfg, g_state.screen) - CONTENT_H;
    if (maxScroll < 0.0f) maxScroll = 0.0f;

    if (g_state.focusField >= fieldCount) {
        g_scroll.target = maxScroll;
        return;
    }

    float top, bottom;
    if (!ui_screen_focus_bounds(fonts, &g_cfg, g_state.screen, g_state.focusField, &top, &bottom))
        return;

    // Measured against where the view is going, not where it is right now, so a second press while it is
    // still gliding keeps heading the right way.
    float viewTop = CONTENT_TOP + g_scroll.target;
    float viewBottom = viewTop + CONTENT_H;
    const float pad = 24.0f;
    float t = g_scroll.target;

    if (top - pad < viewTop) t -= (viewTop - (top - pad));
    else if (bottom + pad > viewBottom) t += ((bottom + pad) - viewBottom);

    if (t > maxScroll) t = maxScroll;
    if (t < 0.0f) t = 0.0f;
    g_scroll.target = t;
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

// scePadRead returns every sample queued since the last call (up to 64); pad_input.c orders them by timestamp.
#define PAD_READ_MAX 64
// 1 = buffered scePadRead; 0 = one scePadReadState per pass, as other homebrew does (buffered is only checked against shadPS4).
#define PAD_BUFFERED_READ 1
static PadInput g_padIn;
static OrbisPadData g_padRaw[PAD_READ_MAX];
static PadSample g_padSamples[PAD_READ_MAX];
static bool g_padStateOnly = !PAD_BUFFERED_READ;   // true: one snapshot per read (set at runtime too if scePadRead is not delivering)

static void pad_input_setup(void)
{
    PadMasks m = {
        ORBIS_PAD_BUTTON_UP, ORBIS_PAD_BUTTON_DOWN, ORBIS_PAD_BUTTON_LEFT, ORBIS_PAD_BUTTON_RIGHT,
        ORBIS_PAD_BUTTON_L1, ORBIS_PAD_BUTTON_R1, ORBIS_PAD_BUTTON_CROSS, ORBIS_PAD_BUTTON_CIRCLE,
        ORBIS_PAD_BUTTON_TRIANGLE, ORBIS_PAD_BUTTON_SQUARE, ORBIS_PAD_BUTTON_OPTIONS
    };
    padin_init(&g_padIn, &m);
}

// Fills g_padSamples with what the pad has for us. Returns the count (0 = nothing new).
static int pad_collect(uint32_t nowMs)
{
    int n = 0;
    if (!g_padStateOnly) {
        int32_t r = scePadRead(g_padHandle, g_padRaw, PAD_READ_MAX);
        if (r < 0) {
            printf("[pad] scePadRead failed (%d) -- using single snapshots\n", (int)r);
            g_padStateOnly = true;
        } else {
            n = r > PAD_READ_MAX ? PAD_READ_MAX : (int)r;
            for (int i = 0; i < n; i++) {
                g_padSamples[i].buttons = g_padRaw[i].buttons;
                g_padSamples[i].timestamp = g_padRaw[i].timestamp;
            }
        }
    }
    // Safety net: if scePadRead keeps saying "nothing" while a single snapshot shows the buttons are not what
    // we last saw, the buffered read is not working on this system. Checked every 100 ms, three misses in a row.
    static uint32_t lastCheckMs = 0;
    static int misses = 0;
    if (!g_padStateOnly && n == 0 && nowMs - lastCheckMs >= 100u) {
        lastCheckMs = nowMs;
        OrbisPadData snap;
        memset(&snap, 0, sizeof(snap));
        if (scePadReadState(g_padHandle, &snap) == 0) {
            uint32_t seen = g_padIn.prev | g_padIn.ignore;
            if (snap.buttons != seen) {
                if (++misses >= 3) {
                    printf("[pad] scePadRead returns nothing while the pad changes -- using single snapshots\n");
                    g_padStateOnly = true;
                }
            } else misses = 0;
        }
    }
    if (g_padStateOnly) {
        OrbisPadData snap;
        memset(&snap, 0, sizeof(snap));
        if (scePadReadState(g_padHandle, &snap) == 0) {
            g_padSamples[0].buttons = snap.buttons;
            g_padSamples[0].timestamp = 0;      // no history: every snapshot counts
            n = 1;
        }
    }
    return n;
}

// Drops whatever is queued and ignores buttons held right now until they are let go. After a blocking call,
// the on-screen keyboard, or coming back from the background, presses made meanwhile must not fire late.
static void pad_flush(uint32_t nowMs)
{
    if (g_padHandle < 0) return;
    int n = pad_collect(nowMs);
    padin_resync(&g_padIn, g_padSamples, n, nowMs);
}

// Home button: pending actions The work itself lives in plugin_install.c and shell_feed.c; the...
// More: docs/code-comments.md (companion-app/source/main.c:307)

static bool g_quitRequested = false;   // set by the loader-off button; the main loop quits like Options

// What the Home button does whenever it isn't Test strip: all pending work, plugin first (it is the...
// More: docs/code-comments.md (companion-app/source/main.c:314)
static void do_pending_actions(void)
{
    // GoldHEN's plugin loader comes first: nothing below can work while it is off, and the app can't...
    // More: docs/code-comments.md (companion-app/source/main.c:319)
    if (g_state.loader == LOADER_OFF) {
        set_status("Closing -- now tick Enable Plugins Loader in Settings > GoldHEN > Plugin Settings.", false);
        g_quitRequested = true;
        return;
    }
    if (g_state.install == UI_INSTALL_DISABLED) {
        do_plugin_enable();
        if (g_state.install != UI_INSTALL_OK) return;
    } else if (g_state.install != UI_INSTALL_OK) {
        do_plugin_update();
        if (g_state.install != UI_INSTALL_OK) return;
    }
    if (g_state.shell.state != SHELL_OFF && g_state.shell.state != SHELL_OK) {
        do_shell_repair();
    }
}

// live preview / test strip Builds one color per *configured* LED in real physical wire order...
// More: docs/code-comments.md (companion-app/source/main.c:339)

static LedSlot g_layoutSlots[LAYOUT_MAX_LEDS];
static int g_layoutCount = 0;
// Must outlive update_live_preview() -- UiState.ledColors (set below) borrows this pointer through to...
// More: docs/code-comments.md (companion-app/source/main.c:351)
static uint8_t g_wireBytes[LAYOUT_MAX_LEDS * 3];

// Advanced by real elapsed time (not an assumed frame duration) once per loop iteration in main().
// More: docs/code-comments.md (companion-app/source/main.c:357)
static float g_animSeconds = 0.0f;

// Test Strip cycles between two visually distinct animation modes rather than running one pattern...
// More: docs/code-comments.md (companion-app/source/main.c:364)
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
        // Nothing configured -- nothing to send, and nothing real to show on the edge frame either.
        // More: docs/code-comments.md (companion-app/source/main.c:428)
        g_state.ledColors = NULL;
        g_state.ledColorCount = 0;
        g_state.ledSlots = NULL;
        g_state.ledSlotCount = 0;
        return;
    }

    // Set up/Customization show a *stable* per-position colour on purpose -- the point there is seeing...
    // More: docs/code-comments.md (companion-app/source/main.c:441)
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
            // Phase 2: a single travelling band with a fading trail. basePos reuses baseHue/360...
            // More: docs/code-comments.md (companion-app/source/main.c:470)
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

    // The edge frame shows these bytes regardless of whether the DDP send below actually reaches the strip...
    // More: docs/code-comments.md (companion-app/source/main.c:491)
    g_state.ledColors = g_wireBytes;
    g_state.ledColorCount = g_layoutCount;
    g_state.ledSlots = g_layoutSlots;
    g_state.ledSlotCount = g_layoutCount;

    bool sent = ddp_send_rgb_zones(g_cfg.wledHost, g_cfg.wledPort, g_wireBytes, g_layoutCount);

    // Only touch the status line on the transition into/out of failure -- this runs every frame, so...
    // More: docs/code-comments.md (companion-app/source/main.c:503)
    static bool s_wasSending = true; // mismatched on purpose so a bad host present at startup is reported too
    if (sent != s_wasSending) {
        if (!sent) {
            char msg[256];
            if (g_cfg.wledHost[0] == '\0')
                snprintf(msg, sizeof(msg), "No WLED Host set -- add its IP address in Set up. Live output paused.");
            else
                snprintf(msg, sizeof(msg), "WLED Host \"%s\" isn't a valid IPv4 address -- live output paused.", g_cfg.wledHost);
            set_status(msg, true);
        }
        s_wasSending = sent;
    }
}

// on-screen keyboard Lives in ime.c. This is what it needs from here.
// More: docs/code-comments.md (companion-app/source/main.c:522)
static ImeHost ime_host(void)
{
    ImeHost host = { &g_cfg, g_userId, FRAME_WIDTH / 2.0f, FRAME_HEIGHT / 2.0f, set_status };
    return host;
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
            // Anything pending (plugin or, with the Shell feed on, its files) takes the button; Test strip is only...
            // More: docs/code-comments.md (companion-app/source/main.c:546)
            if (home_plan(g_state.install, g_state.shell, g_state.loader).cta != CTA_TEST_STRIP) {
                do_pending_actions();
            } else if (g_state.testRunning) {
                g_state.testRunning = false;
                set_status("Test strip stopped.", false);
            } else if (!g_state.setupComplete) {
                g_state.toastVisible = true;
                // Also in the console's notification bar, where the shell feed and the plugin say the same
                // thing. Essential: the strip can't work without it, so it ignores [notifications] enabled.
                if (g_cfg.wledHost[0] == '\0')
                    app_notify(true, "Test strip can't start: no WLED address is set. Set wled_host in Set up.");
                else
                    app_notify(true, "Test strip can't start: no LED strip layout is set. Set the LED counts in Set up.");
            } else {
                g_state.testRunning = true;
                char msg[128];
                snprintf(msg, sizeof(msg), "Test strip running -- sending live colours to %s.", g_cfg.wledHost);
                set_status(msg, false);
            }
            break;
        case HOME_FOCUS_HELP:
            g_state.screen = UI_SCREEN_HELP;
            scroll_snap(0.0f);
            break;
        case HOME_FOCUS_SETUP:
            g_state.screen = UI_SCREEN_SETUP;
            g_state.focusField = 0;
            scroll_snap(0.0f);
            break;
        case HOME_FOCUS_CUST:
            g_state.screen = UI_SCREEN_CUSTOMIZATION;
            g_state.focusField = 0;
            scroll_snap(0.0f);
            break;
        default:
            break;
    }
    g_dirty = true;
}

// foreground / background The same call the plugin uses to notice a suspended game...
// More: docs/code-comments.md (companion-app/source/main.c:589)
typedef struct {
    int32_t eventNum;
    bool isSystemUiOverlaid;
    bool isInBackgroundExecution;
    bool isCpuMode7CpuNormal;
    bool isGameLiveStreamingOnAir;
    bool isOutOfVrPlayArea;
    uint8_t reservedPadding[128];
} AppSystemServiceStatus;
extern int32_t app_sceSystemServiceGetStatus(AppSystemServiceStatus *status) __asm__("sceSystemServiceGetStatus");

static void poll_background_state(uint32_t nowMs)
{
    static uint32_t lastPollMs = 0;
    if (nowMs - lastPollMs < 500u) return;      // twice a second is plenty for a button press
    lastPollMs = nowMs;

    AppSystemServiceStatus st;
    memset(&st, 0, sizeof(st));
    if (app_sceSystemServiceGetStatus(&st) != 0) return;
    bool bg = st.isInBackgroundExecution;
    if (bg != g_appBackgrounded) {
        g_appBackgrounded = bg;
        printf("[main] %s\n", bg ? "backgrounded -- releasing the strip" : "foregrounded -- driving the strip again");
        g_dirty = true;
    }
}

// ---------------- Settings screen input ----------------

// How long the Save button shows its "Saved!" confirmation state (ui_screens.c...
// More: docs/code-comments.md (companion-app/source/main.c:627)
#define SAVE_CONFIRM_DURATION_MS 1600u
static uint32_t g_saveConfirmUntilMs = 0;

// Every place that persists g_cfg goes through here. presets_save writes the
// whole ini in one pass (setup values, [presets] and both preset sections).
static bool save_config(void)
{
    bool ok = presets_save(&g_cfg, AMBIENT_CONFIG_PATH);
    if (ok) g_shellEnabledOnDisk = g_cfg.shellEnabled != 0;
    return ok;
}

static void do_save(void)
{
    if (save_config()) {
        snprintf(g_state.statusLine, sizeof(g_state.statusLine),
                 "%s -- the plugin picks this up on its own reload check.", AMBIENT_CONFIG_PATH);
        g_state.statusIsSaved = true;
        g_state.statusIsError = false;
        g_saveConfirmUntilMs = SDL_GetTicks() + SAVE_CONFIRM_DURATION_MS;

        // BUG FIX: setupComplete was only ever computed once, at startup from whatever settings_load() found...
        // More: docs/code-comments.md (companion-app/source/main.c:651)
        g_state.setupComplete = g_cfg.wledHost[0] != '\0' &&
            (g_cfg.ledCountTop + g_cfg.ledCountRight + g_cfg.ledCountBottom + g_cfg.ledCountLeft) > 0;
    } else {
        set_status("Save FAILED -- check the config path is writable.", true);
    }
    g_dirty = true;
}

// Nudges/cycles/toggles the focused field by one step without opening the keyboard.
// More: docs/code-comments.md (companion-app/source/main.c:670)
static void nudge_field(const MenuItem *item, int dir)
{
    if (item->type == FIELD_STRING) return; // no numeric nudge for an IP

    if (!strcmp(item->key, "preset")) {
        // Game <-> Movie. presets.c stashes the values we're leaving and loads
        // the other preset's into g_cfg, so nothing is lost either way.
        int next = (preset_active() + (dir < 0 ? PRESET_COUNT - 1 : 1)) % PRESET_COUNT;
        preset_select(&g_cfg, next);
        colorpipeline_rebuild_perchannel_gamma_luts(&g_cfg);
        char msg[128];
        snprintf(msg, sizeof(msg), "Preset: %s.", kPresetNames[next]);
        set_status(msg, false);
        return;
    }

    if (item->type == FIELD_ACTION) return; // buttons are pressed (cross), not nudged

    if (!strcmp(item->key, "shell_run_now")) {
        // "Launch Ambilight payload" is a FIELD_ACTION in the Shell feed card.
        // More: docs/code-comments.md (companion-app/source/main.c:692)
        return;
    }

    if (!strcmp(item->key, "shell_autoload")) {
        // Edits GoldHEN's payload list, not this app's ini -- see toggle_autoload().
        toggle_autoload();
        return;
    }

    if (!strcmp(item->key, "smoothing_preset")) {
        // Not a real field -- see settings.h/settings.c -- so it can't go through settings_get_i32/set_i32...
        // More: docs/code-comments.md (companion-app/source/main.c:706)
        int next = smoothing_preset_index(&g_cfg) + dir;
        if (next < 0) next = SMOOTHING_PRESET_COUNT - 1;
        if (next >= SMOOTHING_PRESET_COUNT) next = 0;
        smoothing_apply_preset(&g_cfg, next);
        char msg[128];
        snprintf(msg, sizeof(msg), "%s: %s.", item->label, kSmoothingPresetNames[next]);
        set_status(msg, false);
        return;
    }

    int32_t cur = settings_get_i32(&g_cfg, item);
    int32_t next;
    if (item->type == FIELD_BOOL) {
        next = cur ? 0 : 1;
    } else if (item->type == FIELD_ENUM) {
        next = cur + dir;
        if (next < item->min) next = item->max;
        if (next > item->max) next = item->min;
    } else if (item->unit == UNIT_PCT_OF_MAX) {
        // Step in the percent the person sees, then convert back -- so brightness moves 100% -> 95% -> 90%...
        // More: docs/code-comments.md (companion-app/source/main.c:728)
        int32_t d = settings_to_display(item, cur) + dir * settings_display_step(item);
        next = settings_from_display(item, d);
    } else {
        next = cur + dir * item->step;
    }
    settings_set_i32(&g_cfg, item, next);
    if (!strcmp(item->section, "shell") && !strcmp(item->key, "enabled")) on_shell_enabled_changed();

    char msg[128];
    if (item->type == FIELD_BOOL) snprintf(msg, sizeof(msg), "%s: %s.", item->label, next ? "On" : "Off");
    else {
        char vbuf[24];
        settings_format_display(item, settings_to_display(item, settings_get_i32(&g_cfg, item)), vbuf, sizeof(vbuf));
        snprintf(msg, sizeof(msg), "%s: %s.", item->label, vbuf);
    }
    set_status(msg, false);
}

static void handle_settings_input(bool up, bool down, bool left, bool right,
                                   bool cross, bool circle, bool l1, bool r1, bool square)
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

    // Anything other than the confirming press itself cancels a pending reset.
    if (moved || l1 || r1 || circle || square) { preset_reset_disarm(); g_dirty = true; }

    bool onSaveButton = (g_state.focusField >= fieldCount);

    // The row hint (see autoload_hint) lasts until the next press; a press that needs it again re-sets it.
    if (moved || cross || circle || l1 || r1 || square) g_state.settingsHint[0] = '\0';

    // Square = quick save, from anywhere on the page. Same call as pressing the Save button, so the
    // button turns to "Saved!" the same way. (Options still saves and quits.)
    if (square) do_save();

    if (cross) {
        if (onSaveButton) {
            do_save();
        } else {
            const MenuItem *item = &kMenuItems[start + g_state.focusField];
            if (item->type == FIELD_ACTION) {
                if (!strcmp(item->key, "shell_run_now")) {
                    // "Launch Ambilight payload" in the Shell feed card.
                    // More: docs/code-comments.md (companion-app/source/main.c:784)
                    if (!g_cfg.shellEnabled) {
                        autoload_hint("Launch Ambilight payload needs the Shell feed: turn the Shell feed switch on first.");
                    } else if (g_state.install == UI_INSTALL_NONE) {
                        autoload_hint("Launch Ambilight payload needs the plugin: install it from Home first.");
                    } else if (g_state.install == UI_INSTALL_DISABLED) {
                        autoload_hint("Launch Ambilight payload needs the plugin enabled: enable it from Home first.");
                    } else if (!g_assetPresent[ASSET_PAYLOAD] || !g_assetPresent[ASSET_SHELL_PRX]) {
                        autoload_hint("Launch Ambilight payload needs the Ambilight payload and shell feed files: install them from Home first.");
                    } else if (g_state.shellFeedRunning) {
                        // Its heartbeat is changing, so it is up: don't send it again.
                        autoload_hint("Ambilight payload is already running.");
                        app_notify(false, "Ambilight payload is already running.");
                    } else if (g_state.shellFeedStarting) {
                        // Sent a moment ago, no heartbeat yet: a second send now would only race the first.
                        autoload_hint("Ambilight payload was just sent and is still starting up.");
                        app_notify(false, "Ambilight payload was just sent and is still starting up.");
                    } else {
                        // Set up has no status line, so show launch_payload_now()'s result (sent, or why not) in the save bar.
                        launch_payload_now();
                        autoload_hint(g_state.statusLine);
                    }
                } else {
                    // Default FIELD_ACTION handling: "Reset preset" (the only other action today).
                    // first press arms, second press does it.
                    if (!preset_reset_is_armed()) {
                        preset_reset_arm();
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Press X again to reset %s to its defaults.", kPresetNames[preset_active()]);
                        set_status(msg, false);
                    } else {
                        preset_reset_active(&g_cfg);
                        colorpipeline_rebuild_perchannel_gamma_luts(&g_cfg);
                        char msg[128];
                        snprintf(msg, sizeof(msg), "%s reset to its defaults -- save to keep it.", kPresetNames[preset_active()]);
                        set_status(msg, false);
                    }
                }
            } else if (item->type == FIELD_BOOL) nudge_field(item, +1);
            else if (item->type == FIELD_ENUM) nudge_field(item, +1);
            else { ImeHost host = ime_host(); ime_open_field(start + g_state.focusField, &host); }
        }
        g_dirty = true;
    } else if ((l1 || r1) && !onSaveButton) {
        const MenuItem *item = &kMenuItems[start + g_state.focusField];
        nudge_field(item, r1 ? +1 : -1);
        g_dirty = true;
    }

    if (circle) {
        g_state.screen = UI_SCREEN_HOME;
        scroll_snap(0.0f);
        g_dirty = true;
    }
}

// Help screen input No fields here, so no move_focus_vertical()/scroll_to_focus() -- just a plain...
// More: docs/code-comments.md (companion-app/source/main.c:842)

#define HELP_SCROLL_STEP_PX 80.0f

static void handle_help_input(bool up, bool down, bool circle, bool cross, bool triangle)
{
    // While the QR popup is up, it owns Cross/Circle/Triangle entirely -- none of Help's own scrolling or...
    // More: docs/code-comments.md (companion-app/source/main.c:852)
    if (g_state.qrVisible) {
        if (circle || cross) {
            g_state.qrVisible = false;
            g_dirty = true;
        }
        return;
    }

    if (up || down) {
        float maxScroll = ui_screen_content_height(g_fonts, &g_cfg, UI_SCREEN_HELP) - CONTENT_H;
        if (maxScroll < 0.0f) maxScroll = 0.0f;

        float t = g_scroll.target + (down ? HELP_SCROLL_STEP_PX : -HELP_SCROLL_STEP_PX);
        if (t < 0.0f) t = 0.0f;
        if (t > maxScroll) t = maxScroll;
        g_scroll.target = t;
        g_dirty = true;
    }

    if (triangle) {
        g_state.qrVisible = true;
        g_dirty = true;
        return; // don't also process circle below on the same frame
    }

    if (circle) {
        g_state.screen = UI_SCREEN_HOME;
        scroll_snap(0.0f);
        g_dirty = true;
    }
}

// ---------------- Entry point ----------------

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    // Everything of ours lives in one folder; make it before the first read or write of any file in it.
    APP_MKDIR(APP_DATA_DIR);

    if (!settings_load(&g_cfg, AMBIENT_CONFIG_PATH)) {
        settings_set_defaults(&g_cfg);
        printf("[main] no config at %s -- using defaults\n", AMBIENT_CONFIG_PATH);
    }
    // After settings_load/defaults: reads the Game/Movie presets, migrates an
    // ini from before presets existed, and leaves g_cfg showing the active one.
    presets_init(&g_cfg, AMBIENT_CONFIG_PATH);
    g_shellEnabledOnDisk = g_cfg.shellEnabled != 0;
    colorpipeline_rebuild_perchannel_gamma_luts(&g_cfg);

    // Not SDL_INIT_JOYSTICK: input comes from the native scePad API below, and letting SDL's joystick...
    // More: docs/code-comments.md (companion-app/source/main.c:911)
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("[main] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("PS4 Ambilight", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                           FRAME_WIDTH, FRAME_HEIGHT, 0);
    if (!window) { printf("[main] SDL_CreateWindow failed: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    g_window = window;

    // This SDL port needs the window surface, not an accelerated renderer -- confirmed against OpenOrbis's...
    // More: docs/code-comments.md (companion-app/source/main.c:925)
    SDL_Surface *windowSurface = SDL_GetWindowSurface(window);
    g_windowSurface = windowSurface;

    UiCanvas *canvas = ui_canvas_create(FRAME_WIDTH, FRAME_HEIGHT);
    if (!canvas) { printf("[main] canvas allocation failed\n"); SDL_Quit(); return 1; }
    g_canvas = canvas;

    g_homeStaticCache = (uint32_t *)malloc((size_t)FRAME_WIDTH * FRAME_HEIGHT * sizeof(uint32_t));
    if (!g_homeStaticCache) {
        // Not fatal -- render_and_present() and render_home_fast() both already check for NULL and simply skip...
        // More: docs/code-comments.md (companion-app/source/main.c:938)
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

    // Load the ImeDialog and CommonDialog sysmodules at runtime before any sce*Dialog call;
    // linking -lSceImeDialog alone crashes with PRX_NOT_RESOLVED_FUNCTION. [docs/code-comments.md]
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG) < 0) {
        printf("[main] sceSysmoduleLoadModuleInternal(COMMON_DIALOG) failed\n");
    }
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG) < 0) {
        printf("[main] sceSysmoduleLoadModule(IME_DIALOG) failed\n");
    }
    int commonDialogRet = sceCommonDialogInitialize();
    printf("[main] sceCommonDialogInitialize() = %d\n", commonDialogRet);

    // Encodes once, up front, rather than on first Triangle press -- qrcodegen's Reed-Solomon pass is...
    // More: docs/code-comments.md (companion-app/source/main.c:982)
    if (!help_qr_init()) {
        printf("[main] help_qr_init() failed -- QR popup will show the URL as text only\n");
    }

    memset(&g_state, 0, sizeof(g_state));
    g_state.screen = UI_SCREEN_HOME;
    g_state.install = plugin_registration_state();
    // Shell feed: what is on disk, and GoldHEN's autoload entry. Pure bookkeeping while the switch
    // is off (the summary stays SHELL_OFF), so this costs two file opens and changes nothing visible.
    rescan_shell_files();
    g_cfg.shellAutoload = read_autoload_on() ? 1 : 0;
    sync_shell_summary();
    claim_start();
    g_state.setupComplete = g_cfg.wledHost[0] != '\0' &&
        (g_cfg.ledCountTop + g_cfg.ledCountRight + g_cfg.ledCountBottom + g_cfg.ledCountLeft) > 0;
    g_state.homeFocus = HOME_FOCUS_CTA;
    g_state.focusField = 0;

    // One-time, best-effort update check against the latest GitHub release, only when there's an installed...
    // More: docs/code-comments.md (companion-app/source/main.c:1005)
    if (g_state.install == UI_INSTALL_OK || (g_cfg.shellEnabled && (g_assetPresent[ASSET_SHELL_PRX] || g_assetPresent[ASSET_PAYLOAD]))) {
        set_status("Checking for updates...", false);
        start_background_update_check();
    } else {
        set_status(g_state.install == UI_INSTALL_DISABLED ? "Plugin installed but disabled -- enable it below." :
                   "Plugin not installed yet.", false);
    }

    // GoldHEN's plugin loader: read it once now so the very first frame of Home is right; a definite
    // OFF replaces whatever the line above said, because nothing else matters while it is off.
    g_state.loader = read_plugin_loader_state();
    if (g_state.loader == LOADER_OFF)
        set_status("GoldHEN's plugin loader is off -- close this app and tick it in Settings > GoldHEN > Plugin Settings.", true);

    // Firmware the standalone shell loader is untested on, or has no entry for: read once and shown as a note.
    firmware_check();

    pad_input_setup();
    scroll_snap(0.0f);
    g_screenCache = ui_screen_cache_create();
    frame_prof_init(&g_prof);

    uint32_t lastTicksMs = SDL_GetTicks();

    // Only input and drawing run fast; network, files, lease and update check keep their old 30 Hz cadence,
    // or polling the pad every few ms would also send the UDP preview and touch the claim files that often.
    const uint32_t SERVICE_INTERVAL_MS = 33u;
    const uint32_t IDLE_SLEEP_US = 2000u;        // between pad reads when there is nothing to draw
    const uint32_t BLOCKING_HANDLER_MS = 250u;   // a press that took this long did blocking work: drop what queued meanwhile
    uint32_t lastServiceMs = lastTicksMs - SERVICE_INTERVAL_MS;
    bool wasDialog = false, wasBackgroundedForPad = false;
    bool lastConfirmShown = false;

    bool running = true;
    while (running) {
        uint64_t iterStartUs = now_us();
        frame_prof_begin(&g_prof, iterStartUs);
        g_profStage = FP_INPUT;
        uint32_t nowMs = SDL_GetTicks();
        float dtSec = (float)(nowMs - lastTicksMs) / 1000.0f;
        g_animSeconds += dtSec;
        lastTicksMs = nowMs;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
        }

        // ---- controller: every sample since the last pass, oldest first ----
        if (g_padHandle >= 0) {
            // While the on-screen keyboard is up, this app's own D-Pad-driven input has to sit out so...
            // More: docs/code-comments.md (companion-app/source/main.c:1067)
            bool commonDialogUsed = sceCommonDialogIsUsed();
            bool dialogJustClosed = wasDialog && !commonDialogUsed;
            wasDialog = commonDialogUsed;
            bool justForegrounded = wasBackgroundedForPad && !g_appBackgrounded;
            wasBackgroundedForPad = g_appBackgrounded;

            if (dialogJustClosed || justForegrounded) {
                pad_flush(nowMs);
            } else {
                int n = pad_collect(nowMs);
                PadEvents pe[PAD_READ_MAX + 1];
                int ne = padin_process(&g_padIn, g_padSamples, n, nowMs, pe, PAD_READ_MAX + 1);
                for (int i = 0; i < ne; i++) {
                    if (pe[i].options) running = false;
                    if (commonDialogUsed) continue;      // edges are still tracked above, just not acted on
                    uint32_t h0 = SDL_GetTicks();
                    if (g_state.screen == UI_SCREEN_HOME)
                        handle_home_input(pe[i].up, pe[i].down, pe[i].left, pe[i].right, pe[i].crossUp);
                    else if (g_state.screen == UI_SCREEN_HELP)
                        handle_help_input(pe[i].up, pe[i].down, pe[i].circle, pe[i].crossUp, pe[i].triangle);
                    else
                        handle_settings_input(pe[i].up, pe[i].down, pe[i].left, pe[i].right, pe[i].crossUp,
                                              pe[i].circle, pe[i].l1, pe[i].r1, pe[i].square);
                    if (SDL_GetTicks() - h0 >= BLOCKING_HANDLER_MS) {   // install / save / launch on a slow disk
                        pad_flush(SDL_GetTicks());
                        break;
                    }
                }
            }
        }
        if (g_quitRequested) running = false;
        frame_prof_mark(&g_prof, FP_INPUT, now_us());

        // ---- everything else, at 30 Hz ----
        bool serviceTick = (nowMs - lastServiceMs) >= SERVICE_INTERVAL_MS;
        bool screenHasLedFrame = (g_state.screen == UI_SCREEN_SETUP || g_state.screen == UI_SCREEN_CUSTOMIZATION);
        if (serviceTick) {
            lastServiceMs = nowMs;
            g_profStage = FP_POLL;

            poll_background_update_check();

            if (ime_is_open()) {
                ImeHost host = ime_host();
                if (ime_update(&host)) g_dirty = true;
            }

            // wled-relay hand-off signal: this app has no idle/Home-only mode for this purpose -- "driving the...
            // More: docs/code-comments.md (companion-app/source/main.c:1115)
            static bool s_relayWasActive = false;
            if (g_cfg.relaySignalEnabled != s_relayWasActive) {
                relay_send_external_source(g_cfg.relayHost, g_cfg.relayPort, g_cfg.relaySignalEnabled != 0);
                s_relayWasActive = g_cfg.relaySignalEnabled != 0;
            }
            poll_background_state(nowMs);
            frame_prof_mark(&g_prof, FP_POLL, now_us());

            g_profStage = FP_PREVIEW;
            // Sending stops while the app is in the background (PS button), otherwise it keeps holding the strip.
            if ((screenHasLedFrame || g_state.testRunning) && !g_appBackgrounded) {
                update_live_preview();
            }
            frame_prof_mark(&g_prof, FP_PREVIEW, now_us());

            g_profStage = FP_IO;
            {   // GoldHEN's plugin loader switch: every 2 s on Home, and at once on coming back to the app
                static uint32_t lastLoaderPollMs = 0;
                static bool wasBackgrounded = false;
                bool justForegrounded = wasBackgrounded && !g_appBackgrounded;
                wasBackgrounded = g_appBackgrounded;
                if (g_state.screen == UI_SCREEN_HOME && !g_appBackgrounded &&
                    (justForegrounded || nowMs - lastLoaderPollMs >= 2000u)) {
                    lastLoaderPollMs = nowMs;
                    refresh_plugin_loader_state();
                }
            }

            // While this app is sending to the strip (the same condition as the preview) and the Shell feed is on, claim...
            // More: docs/code-comments.md (companion-app/source/main.c:1162)
            {
                static bool s_wasOnHome = true;
                bool onHome = (g_state.screen == UI_SCREEN_HOME);
                // Coming back to Home is when a hand-copied or deleted file is noticed.
                if (onHome && !s_wasOnHome && g_cfg.shellEnabled) rescan_shell_files();
                s_wasOnHome = onHome;

                // Held while EITHER the switch or the saved ini says on (see g_shellEnabledOnDisk).
                // The claim goes over the loopback lease when the payload listens (lease_app_tick); the claim file is the fallback, kept at fg=0 otherwise.
                bool driving = (g_cfg.shellEnabled || g_shellEnabledOnDisk) && !g_appBackgrounded &&
                               (screenHasLedFrame || g_state.testRunning);
                bool leased = lease_app_tick(nowMs, driving);
                claim_tick(&g_claim, nowMs, driving && !leased, claim_write_cb, NULL);
                sync_shell_summary();
                shell_alive_poll(nowMs);
            }
            frame_prof_mark(&g_prof, FP_IO, now_us());
        }
        g_profStage = FP_INPUT;

        g_state.saveJustConfirmed = (nowMs < g_saveConfirmUntilMs);
        if (g_state.saveJustConfirmed != lastConfirmShown) { lastConfirmShown = g_state.saveJustConfirmed; g_dirty = true; }

        // Ease the scroll toward its target: each frame of that is a redraw, so the loop runs full rate while it glides.
        bool scrolling = scroll_animate(dtSec);

        // ---- draw ---- only when something changed (g_dirty) or is moving, so a static screen sits idle.
        bool onHomeTest = (g_state.screen == UI_SCREEN_HOME && g_state.testRunning);
        bool rendered = false;
        if (g_dirty) {
            // Something actually changed (focus moved, install state flipped, a field was edited, ...) -- a full...
            // More: docs/code-comments.md (companion-app/source/main.c:1195)
            render_and_present();
            g_dirty = false;
            rendered = true;
        } else if (onHomeTest && serviceTick) {
            // The only thing that changed is the strip animation, which advances on the 30 Hz tick: reuse the
            // cached static layer and redraw just the LED frame (full redraw if the cache is not ready yet).
            if (g_homeStaticCacheValid) render_home_fast(); else render_and_present();
            rendered = true;
        }

        char profLine[256];
        const char *tag = g_state.screen == UI_SCREEN_HOME ? "Home" :
                          g_state.screen == UI_SCREEN_SETUP ? "Set up" :
                          g_state.screen == UI_SCREEN_CUSTOMIZATION ? "Customization" : "Help";
        if (frame_prof_end(&g_prof, now_us(), tag, profLine, sizeof(profLine))) printf("%s\n", profLine);

        // A drawn frame already waited for the flip; otherwise nap briefly and look at the pad again.
        if (!rendered && !scrolling) usleep(IDLE_SLEEP_US);
    }

    // BUG FIX: closing the app (Options button or SDL_QUIT) previously went straight to teardown, so any...
    // More: docs/code-comments.md (companion-app/source/main.c:1222)
    save_config();

    // Release the strip claim so the shell feed doesn't wait out the 2.5 s expiry. Called twice, a
    // retry interval apart, in case the first write was refused.
    lease_app_shutdown();
    claim_tick(&g_claim, SDL_GetTicks(), false, claim_write_cb, NULL);
    claim_tick(&g_claim, SDL_GetTicks() + CLAIM_RETRY_MS + 1u, false, claim_write_cb, NULL);

    ime_shutdown();
    // Defensive final "off" if the signal was last left on -- this app has no plugin_unload-style...
    // More: docs/code-comments.md (companion-app/source/main.c:1240)
    if (g_cfg.relaySignalEnabled) {
        relay_send_external_source(g_cfg.relayHost, g_cfg.relayPort, false);
    }
    ui_fonts_destroy(&fonts);
    free(g_homeStaticCache);
    ui_screen_cache_destroy(g_screenCache);
    SDL_FreeSurface(canvasSurface);
    ui_canvas_destroy(canvas);
    SDL_DestroyWindow(window);
    SDL_Quit();

    // Exit through sceSystemServiceLoadExec("exit"), never by returning from main():
    // returning crashed on close (SIGSYS inside libkernel). [docs/code-comments.md]
    sceSystemServiceLoadExec("exit", NULL);

    // Not expected to be reached -- LoadExec("exit", ...) hands control back to the system and doesn't...
    // More: docs/code-comments.md (companion-app/source/main.c:1277)
    return 0;
}