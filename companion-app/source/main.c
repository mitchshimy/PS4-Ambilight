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
#include "ui_widgets.h"   // CONTENT_TOP / CONTENT_H for scroll clamping
#include "help_qr.h"
#include "install_plan.h"  // shell feed + plugin: what is pending, what the Home button does
#include "claim_file.h"    // /data/ps4-ambilight/ps4_ambient_state: keeps the shell feed off the strip while this app drives it
#include "payload_ini.h"   // one-entry edits to GoldHEN's payload AutoRun list
#include "goldhen_config.h" // GoldHEN's own "Enable Plugins Loader" switch, read and flipped in place
#include "update_source.h"  // cleans up [dev] update_base_url

// TCP headers for GoldHEN's BinLoader (port 9090). On PS4 these are the same
// POSIX socket headers the SDK provides; they're already available here through
// the orbis libc. sys/socket.h, netinet/in.h and unistd.h are all part of the
// standard sysroot included by the OpenOrbis toolchain.
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
// The shell feed reads that file, so it keeps running until the OFF is saved: the strip claim
// below has to follow this, not just the unsaved switch in Set up, or turning the switch off
// releases the claim while the shell feed is still sending and the strip flickers until Save.
static bool g_shellEnabledOnDisk = false;

// True while the PS button has put this app in the background. While it is, the app sends nothing to
// the strip and drops its claim, so the shell feed / a game / WLED's own source can have it back.
// Polled from sceSystemServiceGetStatus (see poll_background_state()).
static bool g_appBackgrounded = false;

// Set up / Customization have no status line (only Home draws it), so a hint for the row you just
// pressed goes in g_state.settingsHint, which the save bar shows in place of "Changes apply after
// saving." It is cleared by the next button press on that screen.

// Held so do_plugin_update() can force an interim "Downloading..."
// frame before its blocking network/file I/O -- same reasoning as the
// version of this that ran on hardware: without it, a slow server
// makes the app look completely frozen with zero feedback.
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

// Cached snapshot of ui_render_home_static()'s output -- everything on
// Home except the LED edge frame. Rebuilt every time render_and_present()
// actually runs the full Home draw; reused by render_home_fast() on
// every animation-only frame in between, which is what turns "redraw
// all of Home's text/icons/background at 30fps" into "redraw ~200
// small squares at 30fps". Allocated once in main() once the frame
// size is known.
static uint32_t *g_homeStaticCache = NULL;
static bool g_homeStaticCacheValid = false;

void render_and_present(void)
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

// ---------------- Home button: pending actions ----------------
//
// The work itself lives in plugin_install.c and shell_feed.c; the downloads and the release lookup
// in net_http.c.

static bool g_quitRequested = false;   // set by the loader-off button; the main loop quits like Options

// What the Home button does whenever it isn't Test strip: all pending work, plugin first (it is
// the part people notice is missing), then the shell feed files. Stops at the first failure so the
// status line keeps the reason.
static void do_pending_actions(void)
{
    // GoldHEN's plugin loader comes first: nothing below can work while it is off, and the app can't
    // change it (see the note above read_plugin_loader_state() in plugin_install.c). Close the app, the same save and quit
    // as Options, so GoldHEN will let the person tick the switch.
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
            if (g_cfg.wledHost[0] == '\0')
                snprintf(msg, sizeof(msg), "No WLED Host set -- add its IP address in Set up. Live output paused.");
            else
                snprintf(msg, sizeof(msg), "WLED Host \"%s\" isn't a valid IPv4 address -- live output paused.", g_cfg.wledHost);
            set_status(msg, true);
        }
        s_wasSending = sent;
    }
}

// ---------------- on-screen keyboard ----------------
//
// Lives in ime.c. This is what it needs from here.
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
            // Anything pending (plugin or, with the Shell feed on, its files) takes the button;
            // Test strip is only offered once nothing is. With the Shell feed off this is exactly
            // the old plugin-only rule.
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
            g_state.scrollY = 0.0f;
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

// ---------------- foreground / background ----------------
//
// The same call the plugin uses to notice a suspended game (plugin/source/sample_thread.c). The
// OpenOrbis header declares sceSystemServiceGetStatus() with no arguments, so it is bound here under
// another C name to the same symbol (the app already links -lSceSystemService). The struct is the
// plugin's AmbientSystemServiceStatus (field layout from shadPS4's source), padded well past the known
// fields in case the firmware writes more. Confirmed on a console (the PS button flips it). If the call
// ever fails or the field never flips, g_appBackgrounded just stays false and the app behaves as before.
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

// How long the Save button shows its "Saved!" confirmation state
// (ui_screens.c: UiState.saveJustConfirmed) before reverting to
// normal. Long enough to register, short enough not to feel stuck.
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

        // BUG FIX: setupComplete was only ever computed once, at
        // startup from whatever settings_load() found on disk (see
        // main()'s own copy of this same expression). Saving new
        // values here updated g_cfg and the file on disk, but never
        // this cached flag -- so Home's Test Strip kept reading the
        // stale "not set up yet" state after a fresh Setup+Save,
        // until the app was closed and reopened and main() recomputed
        // it from scratch. Recomputing it here, right after a
        // successful save, means Test Strip reflects reality
        // immediately -- same formula as main()'s startup copy, kept
        // in sync with it on purpose.
        g_state.setupComplete = g_cfg.wledHost[0] != '\0' &&
            (g_cfg.ledCountTop + g_cfg.ledCountRight + g_cfg.ledCountBottom + g_cfg.ledCountLeft) > 0;
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
        // "Launch Ambilight payload" is a FIELD_ACTION in the Shell feed card. L1/R1 nudge reaches FIELD_ACTION
        // only from nudge_field() (handle_settings_input dispatches actions via cross separately);
        // treat any nudge as a press since there is no meaningful direction for a send.
        // The cross-press path is handled in handle_settings_input() below.
        return;
    }

    if (!strcmp(item->key, "shell_autoload")) {
        // Edits GoldHEN's payload list, not this app's ini -- see toggle_autoload().
        toggle_autoload();
        return;
    }

    if (!strcmp(item->key, "smoothing_preset")) {
        // Not a real field -- see settings.h/settings.c -- so it can't
        // go through settings_get_i32/set_i32 below, which only know
        // how to read/write a single struct member at item->offset.
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
        // Step in the percent the person sees, then convert back -- so
        // brightness moves 100% -> 95% -> 90%, not 255 -> 250 -> 245.
        // Only a value that's actually being edited goes through this;
        // untouched settings keep their exact stored number.
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
                    // "Launch Ambilight payload" in the Shell feed card. Guard the same way toggle_autoload()
                    // does: all three files must be present. This is the "send now" path -- no
                    // reboot needed, no GoldHEN menu required, works whether or not Autoload is on.
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
        g_state.scrollY = 0.0f;
        g_dirty = true;
    }
}

// ---------------- Help screen input ----------------
//
// No fields here, so no move_focus_vertical()/scroll_to_focus() --
// just a plain scroll, clamped the same way scroll_to_focus() clamps
// against ui_screen_content_height(), and Circle back to Home.

#define HELP_SCROLL_STEP_PX 80.0f

static void handle_help_input(bool up, bool down, bool circle, bool cross, bool triangle)
{
    // While the QR popup is up, it owns Cross/Circle/Triangle
    // entirely -- none of Help's own scrolling or back-navigation
    // should also fire underneath it on the same press. up/down are
    // deliberately NOT swallowed here: scrolling the cards behind a
    // popup that fully covers the .content viewport has no visible
    // effect anyway, and letting the D-Pad repeat-fire logic keep
    // running avoids a stale-press artifact the instant the popup
    // closes (the same class of bug crossUp's whole edge-triggered
    // design exists to avoid elsewhere in this file).
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

        g_state.scrollY += down ? HELP_SCROLL_STEP_PX : -HELP_SCROLL_STEP_PX;
        if (g_state.scrollY < 0.0f) g_state.scrollY = 0.0f;
        if (g_state.scrollY > maxScroll) g_state.scrollY = maxScroll;
        g_dirty = true;
    }

    if (triangle) {
        g_state.qrVisible = true;
        g_dirty = true;
        return; // don't also process circle below on the same frame
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

    // Encodes once, up front, rather than on first Triangle press --
    // qrcodegen's Reed-Solomon pass is cheap but there's no reason to
    // pay it on a frame that also has to redraw the whole modal for
    // the first time. Logged, not fatal: render_qr_modal() falls back
    // to a plain "visit this URL" panel if this never succeeds.
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

    // One-time, best-effort update check against the latest GitHub
    // release, only when there's an installed build to compare in the
    // first place -- NONE/DISABLED already have their own CTA and
    // take priority over "is there something newer". See
    // check_for_plugin_update()'s comment for why a failure here
    // (no network, GitHub unreachable, ...) is silent rather than
    // surfaced as an error: this runs unprompted on every launch.
    //
    // Runs on a background thread (start_background_update_check(),
    // polled every frame by poll_background_update_check() in the
    // main loop below) rather than inline here -- this used to block
    // the whole app behind two sequential GitHub round trips before
    // the home screen ever became interactive, which was the actual
    // source of "the app feels slow to open". Now the home screen
    // renders and takes input immediately; the status line just
    // updates in place once the check comes back.
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
            bool circle   = PRESSED(ORBIS_PAD_BUTTON_CIRCLE);
            bool options  = PRESSED(ORBIS_PAD_BUTTON_OPTIONS);
            bool triangle = PRESSED(ORBIS_PAD_BUTTON_TRIANGLE);
            bool square   = PRESSED(ORBIS_PAD_BUTTON_SQUARE);
            // Cross fires on release, not press: opening the keyboard
            // on the press edge left a stale press for the IME dialog
            // to consume as an immediate Cancel before any real input
            // could register -- a real, previously-hit bug, not a
            // hypothetical one.
            bool crossUp = !(pad.buttons & ORBIS_PAD_BUTTON_CROSS) && (prevPad.buttons & ORBIS_PAD_BUTTON_CROSS);
            #undef PRESSED

            if (options || g_quitRequested) running = false;

            if (!commonDialogUsed) {
                if (g_state.screen == UI_SCREEN_HOME)
                    handle_home_input(up, down, left, right, crossUp);
                else if (g_state.screen == UI_SCREEN_HELP)
                    handle_help_input(up, down, circle, crossUp, triangle);
                else
                    handle_settings_input(up, down, left, right, crossUp, circle, l1, r1, square);
            }
        }

        poll_background_update_check();

        if (ime_is_open()) {
            ImeHost host = ime_host();
            if (ime_update(&host)) g_dirty = true;
        }

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
        // on Home doesn't silently drive the real strip. Help has no
        // LED edge frame either and nothing on screen a live preview
        // would even show, so it's excluded the same way Home is.
        bool screenHasLedFrame = (g_state.screen == UI_SCREEN_SETUP || g_state.screen == UI_SCREEN_CUSTOMIZATION);
        poll_background_state(nowMs);
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
        // Sending stops while the app is in the background (PS button), otherwise it keeps holding the strip.
        if ((screenHasLedFrame || g_state.testRunning) && !g_appBackgrounded) {
            update_live_preview();
        }

        // While this app is sending to the strip (the same condition as above) and the Shell feed is
        // on, claim it the way a game does, so the shell feed stays silent instead of fighting the
        // live preview for WLED (which is what made the LEDs flicker). Off => never writes the file.
        // Released (fg=0) as soon as the app stops sending or the switch is turned off.
        {
            static bool s_wasOnHome = true;
            bool onHome = (g_state.screen == UI_SCREEN_HOME);
            // Coming back to Home is when a hand-copied or deleted file is noticed.
            if (onHome && !s_wasOnHome && g_cfg.shellEnabled) rescan_shell_files();
            s_wasOnHome = onHome;

            // Held while EITHER the switch or the saved ini says on (see g_shellEnabledOnDisk).
            claim_tick(&g_claim, nowMs, (g_cfg.shellEnabled || g_shellEnabledOnDisk) && !g_appBackgrounded &&
                                        (screenHasLedFrame || g_state.testRunning),
                       claim_write_cb, NULL);
            sync_shell_summary();
            shell_alive_poll(nowMs);
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

    // BUG FIX: closing the app (Options button or SDL_QUIT) previously
    // went straight to teardown, so any in-memory g_cfg edits that
    // hadn't gone through an explicit Setup/Customization Save press
    // were lost -- silently, since the crash on close (see below) made
    // it hard to even notice there'd been a chance to save at all.
    // Flushing here guarantees whatever's currently in g_cfg is on
    // disk before the process goes away, the same call do_save() makes
    // from the Save button, just without that function's UI feedback
    // (status line / "Saved!" confirmation) since the app is already
    // on its way out.
    save_config();

    // Release the strip claim so the shell feed doesn't wait out the 2.5 s expiry. Called twice, a
    // retry interval apart, in case the first write was refused.
    claim_tick(&g_claim, SDL_GetTicks(), false, claim_write_cb, NULL);
    claim_tick(&g_claim, SDL_GetTicks() + CLAIM_RETRY_MS + 1u, false, claim_write_cb, NULL);

    ime_shutdown();
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

    // BUG FIX (the crash-on-close from a real-hardware putty.log
    // capture): this used to end with a plain `return 0`, letting the
    // process fall off the end of main() and back into libkernel's own
    // process-exit path. On PS4 homebrew that's not a sanctioned way
    // to end a "big app" launched via the system's own LoadExec --
    // checked this against how two other real, shipped homebrew apps
    // close, since neither crashes on exit:
    //   - Apollo Save Tool (bucanero/apollo-ps4, source/main.c):
    //     registers a terminate() via atexit() whose whole job is
    //     `sceSystemServiceLoadExec("exit", NULL)`, to hand control
    //     back to the XMB properly instead of just returning.
    //   - ItemzFlow (LightningMods/Itemzflow, source/sig_handler.cpp,
    //     Exit_Success()): same call, `sceSystemServiceLoadExec("exit", 0)`,
    //     as the last thing it does before the process goes away, on
    //     both the normal-exit and crash-recovery paths.
    // Matched this app's own putty.log crash signature too: signal 12
    // (SIGSYS, "bad syscall") on the eboot.bin main thread, general-
    // purpose registers zeroed, rip sitting inside libkernel.sprx --
    // consistent with returning out of main() into a kernel trampoline
    // that isn't a valid re-entry point for this process, rather than
    // any crash in this app's own settings/save code (which is why it
    // reproduced on every close, saved settings or not).
    sceSystemServiceLoadExec("exit", NULL);

    // Not expected to be reached -- LoadExec("exit", ...) hands control
    // back to the system and doesn't return here on a real console.
    // Kept only so this still compiles/links as a normal C `main` and
    // behaves sanely (e.g. under a host-side syntax check) if it ever
    // somehow does return.
    return 0;
}