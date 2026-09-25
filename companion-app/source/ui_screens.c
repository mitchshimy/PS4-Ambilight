// ui_screens.c -- Home / Set up / Customization, transcribed from
// home.html, setup.html and customization.html.
//
// Values come from the live AmbientConfig, so nothing here is a
// placeholder: the numbers on screen are the ones in
// /data/ps4_ambient_light.ini.

#include "ui_screens.h"
#include "ui_widgets.h"
#include "ui_icons.h"
#include "led_frame.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

// ---------------------------------------------------------------
// Value formatting
// ---------------------------------------------------------------
//
// Every settings card is now built directly from kMenuItems
// (settings.c) rather than a hand-written, parallel field list --
// settings.c's `label` is the exact on-screen text, so there is
// exactly one place that has to agree with the blueprint, and every
// visible field is wired to a real, working settings_get_i32/
// settings_set_i32 by construction, not by a second lookup that could
// drift out of sync with what's actually editable.
//
// What still lives here, because it's a UI-only concern settings.h
// deliberately knows nothing about: which icon a field/card gets,
// whether an enum renders as a plain input or an icon pill, per-field
// label tinting (the RGB balance channel colours), fixed-vs-flex
// column width, and how each group's fields wrap into rows. All of it
// is keyed off a MenuItem's `key` or `group` string, so reordering
// kMenuItems never breaks it -- only renaming a key/group would, and
// the two are only ever a few lines apart in this file.

static const char *kCornerLabels[4] = { "Bottom left", "Bottom right", "Top left", "Top right" };
static const char *kDirLabels[2]    = { "Clockwise", "Counterclockwise" };
static const char *kOrderLabels[6]  = { "RGB", "RBG", "GRB", "GBR", "BRG", "BGR" };
// The plugin stores gamma as a preset index; the blueprint shows the
// value it stands for, to two decimals.
static const float kGammaPresets[8] = { 1.0f, 1.4f, 1.8f, 2.0f, 2.2f, 2.4f, 2.6f, 2.8f };

typedef struct { char s[24][40]; int n; } ValueBuf;

static const char *vnum(ValueBuf *vb, long v)
{
    if (vb->n >= 24) return "";
    char *d = vb->s[vb->n++];
    snprintf(d, 40, "%ld", v);
    return d;
}

// Same contiguous-run assumption settings.c's own comment documents:
// every MENU_SCREEN_SETUP item comes before every MENU_SCREEN_CUSTOMIZE
// one, so a screen's items are one [start, start+count) span.
void ui_screen_item_range(MenuScreen screen, int *outStart, int *outCount)
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

static UiIconId icon_for_key(const char *key)
{
    if (!strcmp(key, "wled_host") || !strcmp(key, "wled_port"))        return ICON_WIFI;
    if (!strcmp(key, "update_frequency_hz"))                            return ICON_CLOCK;
    if (!strncmp(key, "led_count_", 10))                                 return ICON_LED_STRIP;
    if (!strcmp(key, "led_offset") || !strcmp(key, "led_direction"))    return ICON_LOOP;
    if (!strcmp(key, "led_start_corner"))                                return ICON_CORNER;
    if (!strcmp(key, "color_order"))                                     return ICON_GLOBE;
    if (!strcmp(key, "config_reload_check_seconds"))                    return ICON_REFRESH;
    if (!strcmp(key, "scan_depth"))                                      return ICON_FRAME;
    if (!strcmp(key, "capture_margin_top"))                              return ICON_MARGIN_TL;
    if (!strcmp(key, "capture_margin_right"))                            return ICON_MARGIN_TR;
    if (!strcmp(key, "capture_margin_bottom"))                           return ICON_MARGIN_BL;
    if (!strcmp(key, "capture_margin_left"))                             return ICON_MARGIN_BR;
    if (!strcmp(key, "brightness"))                                      return ICON_SUN;
    if (!strcmp(key, "saturation"))                                      return ICON_DROPLET;
    if (!strcmp(key, "gamma"))                                           return ICON_GAMMA;
    if (!strcmp(key, "black_level"))                                     return ICON_MOON;
    if (!strcmp(key, "white_level"))                                     return ICON_SUN_FILLED;
    if (!strcmp(key, "contrast"))                                        return ICON_SPLIT;
    if (!strcmp(key, "smoothing_enabled"))                               return ICON_WAVES;
    if (!strcmp(key, "settling_time_ms"))                                return ICON_CLOCK;
    if (!strcmp(key, "dark_threshold"))                                  return ICON_MOON;
    if (!strcmp(key, "brightness_r"))                                    return ICON_SLIDER_R;
    if (!strcmp(key, "brightness_g"))                                    return ICON_SLIDER_G;
    if (!strcmp(key, "brightness_b"))                                    return ICON_SLIDER_B;
    if (!strcmp(key, "gamma_r") || !strcmp(key, "gamma_g") || !strcmp(key, "gamma_b")) return ICON_RING;
    return ICON_NONE;
}

static UiColor label_color_for_key(const char *key)
{
    if (!strcmp(key, "brightness_r") || !strcmp(key, "gamma_r")) return COL_CHAN_RED;
    if (!strcmp(key, "brightness_g") || !strcmp(key, "gamma_g")) return COL_CHAN_GREEN;
    if (!strcmp(key, "brightness_b") || !strcmp(key, "gamma_b")) return COL_CHAN_BLUE;
    UiColor none = { 0, 0, 0, 0 };
    return none;
}

// The two enum fields the blueprint draws as icon pills, rather than
// plain text inputs -- Colour order is FIELD_ENUM too but is styled
// as a plain input (see setup.html), so this is a real special case,
// not "every enum is a pill".
static bool is_pill_enum(const char *key)
{
    return !strcmp(key, "led_start_corner") || !strcmp(key, "led_direction");
}

// Fields the blueprint gives a fixed column width instead of letting
// them grow with the row -- everything else uses flex: 1 1 240px.
static bool fixed_width_for_key(const char *key, float *outBasis)
{
    if (!strcmp(key, "led_offset"))                 { *outBasis = 240.0f; return true; }
    if (!strcmp(key, "smoothing_enabled"))           { *outBasis = 240.0f; return true; }
    if (!strcmp(key, "config_reload_check_seconds")) { *outBasis = 300.0f; return true; }
    return false;
}

// Formats item's current value from cfg as display text. Returns NULL
// for FIELD_BOOL (handled as a toggle, not text) -- FIELD_STRING is
// returned directly (points into cfg, valid as long as cfg is).
static const char *format_value(const MenuItem *item, const AmbientConfig *cfg, ValueBuf *vb)
{
    if (item->type == FIELD_BOOL) return NULL;
    if (item->type == FIELD_STRING) return (const char *)cfg + item->offset;

    int32_t v = settings_get_i32(cfg, item);
    if (item->type == FIELD_ENUM) {
        if (!strcmp(item->key, "led_start_corner")) return kCornerLabels[v & 3];
        if (!strcmp(item->key, "led_direction"))     return kDirLabels[v & 1];
        if (!strcmp(item->key, "color_order"))       return kOrderLabels[v % 6];
        return (v >= 0 && v < item->enumCount) ? item->enumNames[v] : "?";
    }
    if (!strcmp(item->key, "gamma")) {
        static char gammaStr[16];
        snprintf(gammaStr, sizeof(gammaStr), "%.2f", kGammaPresets[v >= 0 && v < 8 ? v : 0]);
        return gammaStr;
    }
    return vnum(vb, v);
}

// Per-group visual identity (accent colour, card-title icon) and how
// that group's fields wrap into rows -- both pixel-measured against
// the blueprint captures, same numbers the old hand-written per-card
// functions used.
typedef struct {
    const char *group;
    UiIconId icon;
    int rowSizes[3];
    int rowCount;
    float row2MarginTop;        // applied between row 0 and row 1 (RGB balance's 22px gap)
    bool injectTotalAfterRow0;  // LED strip layout's "Physical strip total" line
} GroupLayout;

static const GroupLayout kGroupLayouts[] = {
    { "WLED connection",     ICON_WIFI,        {3},    1, 0.0f,  false },
    { "LED strip layout",    ICON_LED_STRIP,   {4,4},  2, 0.0f,  true  },
    { "Live reload",         ICON_REFRESH,     {1},    1, 0.0f,  false },
    { "Screen sampling",     ICON_FRAME,       {5},    1, 0.0f,  false },
    { "Colour",               ICON_LEVELS,      {6},    1, 0.0f,  false },
    { "Motion and darkness", ICON_WAVES,       {3},    1, 0.0f,  false },
    { "RGB balance",          ICON_SLIDERS_RGB, {3,3},  2, 22.0f, false },
};
#define GROUP_LAYOUT_COUNT ((int)(sizeof(kGroupLayouts) / sizeof(kGroupLayouts[0])))

static const GroupLayout *layout_for_group(const char *group)
{
    for (int i = 0; i < GROUP_LAYOUT_COUNT; i++)
        if (!strcmp(kGroupLayouts[i].group, group)) return &kGroupLayouts[i];
    return NULL;
}

// Card accent colours -- COL_* tokens are a UI-layer concept, so kept
// as a lookup here rather than folded into kGroupLayouts above.
static UiColor accent_for_group(const char *group)
{
    if (!strcmp(group, "LED strip layout")) return COL_AMBER;
    if (!strcmp(group, "Colour"))            return COL_AMBER;
    if (!strcmp(group, "RGB balance"))       return COL_CHAN_BLUE;
    return COL_CYAN;
}
static UiColor icon_color_for_group(const char *group)
{
    if (!strcmp(group, "RGB balance")) return COL_TEXT_LABEL;
    return accent_for_group(group);
}

int ui_screen_field_rows(UiScreenId screen, int rowStart[], int rowCount[], int maxRows)
{
    MenuScreen menuScreen = (screen == UI_SCREEN_SETUP) ? MENU_SCREEN_SETUP : MENU_SCREEN_CUSTOMIZE;
    int start, count;
    ui_screen_item_range(menuScreen, &start, &count);

    int rows = 0;
    int i = 0;
    while (i < count) {
        const char *group = kMenuItems[start + i].group;
        int groupStart = i;
        while (i < count && !strcmp(kMenuItems[start + i].group, group)) i++;
        int groupCount = i - groupStart;

        const GroupLayout *gl = layout_for_group(group);
        if (!gl) continue;

        int localIdx = 0;
        for (int r = 0; r < gl->rowCount && localIdx < groupCount && rows < maxRows; r++) {
            int n = gl->rowSizes[r];
            if (n > groupCount - localIdx) n = groupCount - localIdx;
            rowStart[rows] = groupStart + localIdx;
            rowCount[rows] = n;
            rows++;
            localIdx += n;
        }
    }
    return rows;
}

// Builds every card for one settings screen directly from kMenuItems.
// `focus` is the 0-based index within this screen's own item range
// (matching what main.c's navigation uses), or -1 for nothing focused.
static int build_group_cards(MenuScreen screen, const AmbientConfig *cfg, int focus, ValueBuf *vb,
                              UiCard *cards, UiField *fp, UiFieldRow *rp)
{
    int start, count;
    ui_screen_item_range(screen, &start, &count);

    UiScreenId uiScreen = (screen == MENU_SCREEN_SETUP) ? UI_SCREEN_SETUP : UI_SCREEN_CUSTOMIZATION;
    int rowFieldStart[16], rowFieldCount[16];
    int totalRows = ui_screen_field_rows(uiScreen, rowFieldStart, rowFieldCount, 16);

    int ci = 0, fi = 0, ri = 0;
    int rowIdx = 0;
    while (rowIdx < totalRows) {
        const char *group = kMenuItems[start + rowFieldStart[rowIdx]].group;
        const MenuItem *first = &kMenuItems[start + rowFieldStart[rowIdx]];
        const GroupLayout *gl = layout_for_group(group);
        if (!gl) { printf("[ui] no GroupLayout for \"%s\" -- check kGroupLayouts\n", group); rowIdx++; continue; }

        int cardFirstRow = ri;
        int cardRowCount = 0;
        while (rowIdx < totalRows && !strcmp(kMenuItems[start + rowFieldStart[rowIdx]].group, group)) {
            int rowStart = fi;
            for (int k = 0; k < rowFieldCount[rowIdx]; k++) {
                int globalFocusIdx = rowFieldStart[rowIdx] + k;
                const MenuItem *item = &kMenuItems[start + globalFocusIdx];

                UiField field;
                memset(&field, 0, sizeof(field));
                field.labelIcon = icon_for_key(item->key);
                field.label = item->label;
                field.labelColor = label_color_for_key(item->key);
                field.focused = (focus == globalFocusIdx);

                float basis = 0.0f;
                bool fixed = fixed_width_for_key(item->key, &basis);
                field.basis = basis;
                field.fixed = fixed;

                if (item->type == FIELD_BOOL) {
                    field.kind = UI_FIELD_TOGGLE;
                    field.toggleOn = settings_get_i32(cfg, item) != 0;
                } else if (item->type == FIELD_ENUM && is_pill_enum(item->key)) {
                    field.kind = UI_FIELD_PILL;
                    field.value = format_value(item, cfg, vb);
                    field.valueIcon = field.labelIcon;
                } else {
                    field.kind = UI_FIELD_INPUT;
                    field.value = format_value(item, cfg, vb);
                }

                fp[fi++] = field;
            }
            // Row index within THIS card (0-based) decides whether the
            // group's row2MarginTop applies -- cardRowCount is that
            // index before incrementing.
            rp[ri++] = (UiFieldRow){ &fp[rowStart], rowFieldCount[rowIdx],
                                      (cardRowCount == 1) ? gl->row2MarginTop : 0.0f };
            cardRowCount++;
            rowIdx++;
        }

        int totalAfterRow = -1;
        static char totalLabelBuf[32];
        static char totalValueBuf[32];
        const char *totalLabel = NULL, *totalValue = NULL;
        if (gl->injectTotalAfterRow0) {
            totalAfterRow = 0;
            int total = (int)(cfg->ledCountTop + cfg->ledCountRight + cfg->ledCountBottom + cfg->ledCountLeft);
            snprintf(totalLabelBuf, sizeof(totalLabelBuf), "Physical strip total: ");
            snprintf(totalValueBuf, sizeof(totalValueBuf), "%d LEDs", total);
            totalLabel = totalLabelBuf;
            totalValue = totalValueBuf;
        }

        cards[ci++] = (UiCard){
            accent_for_group(group), gl->icon, icon_color_for_group(group),
            group, first->groupDesc,
            &rp[cardFirstRow], cardRowCount,
            totalAfterRow, totalLabel, totalValue
        };
    }

    return ci;
}

// ---------------------------------------------------------------
// Help
// ---------------------------------------------------------------
//
// Static prose, not data-driven from kMenuItems like Set up/
// Customization are -- there's nothing per-config to show. Reuses
// UiCard purely for its title+icon+desc layout: every card here has
// rowCount 0, so ui_card_draw never reaches its field-row code at
// all.
//
// Content is kept honest against what's actually true elsewhere in
// this app: group names match kMenuItems' own "group" strings
// (settings.c).
//
// Built inside a function, not as a file-scope static const table --
// COL_* (ui_theme.h) expands to a call to ui_rgb(), an ordinary
// function, not a constant expression, so it can't sit in a
// static-storage initializer the way kGroupLayouts' plain ints and
// strings can.
#define HELP_CARD_COUNT 5

static void help_cards_init(UiCard out[HELP_CARD_COUNT])
{
    out[0] = (UiCard){ COL_CYAN, ICON_DOWNLOAD, COL_CYAN, "Getting started",
        "Install ps4_ambient_light.prx from Home once GoldHEN is running -- "
        "the button there reads Install, Update or Enable depending on "
        "what's already on the console. From there, Set up connects WLED "
        "and describes your physical strip, and Customization tunes how "
        "captured colours look. Test strip on Home sends live colours to "
        "your strip so you can check it before starting a game.",
        NULL, 0, -1, NULL, NULL };

    out[1] = (UiCard){ COL_AMBER, ICON_WIFI, COL_AMBER, "Set up your strip",
        "Under WLED connection, enter your WLED controller's IPv4 address "
        "and UDP port (DDP defaults to 4048). Under LED strip layout, set "
        "how many LEDs run along each edge, which corner the strip starts "
        "from, which direction it runs, and colour order -- these have to "
        "match how the strip is physically wired, not just how many LEDs "
        "you own. Live reload controls how often the plugin re-reads this "
        "file while a game is running, so changes apply without closing it.",
        NULL, 0, -1, NULL, NULL };

    out[2] = (UiCard){ COL_AMBER, ICON_LEVELS, COL_AMBER, "Tune the picture",
        "Screen sampling sets how much of each edge is captured, and how "
        "far in from the bezel -- raise the capture margins if letterbox "
        "bars or overscan are being sampled instead of picture content. "
        "Colour shapes brightness, saturation, gamma and black/white "
        "levels. Motion and darkness controls transition smoothing and "
        "how the strip behaves in very dark scenes. RGB balance calibrates "
        "each channel separately, for a strip or TV with an off white point.",
        NULL, 0, -1, NULL, NULL };

    out[3] = (UiCard){ COL_CYAN, ICON_HELP, COL_CYAN, "Controls",
        "D-Pad moves between fields and cards, and scrolls this screen. "
        "Cross selects a highlighted item, opens the on-screen keyboard "
        "for a text field, or cycles an enum value one step. L1/R1 nudge "
        "a focused number up or down directly, without opening the "
        "keyboard -- the fast way to walk a value across its full range. "
        "Circle goes back a screen. Options saves and quits from Home.",
        NULL, 0, -1, NULL, NULL };

    out[4] = (UiCard){ COL_WARN, ICON_WARNING, COL_WARN, "Troubleshooting",
        "Strip stays dark: re-check the WLED host/port on Set up, and "
        "that WLED's realtime UDP listener is actually reachable on your "
        "network. Colours land on the wrong LEDs: revisit start corner, "
        "direction and colour order together -- getting one wrong usually "
        "looks like the others are wrong too. Light stuck on after "
        "quitting a game: update the plugin from Home, older builds had a "
        "bug here.",
        NULL, 0, -1, NULL, NULL };
}

static float help_content_height(const UiFonts *f)
{
    UiCard cards[HELP_CARD_COUNT];
    help_cards_init(cards);

    float h = 0.0f;
    for (int i = 0; i < HELP_CARD_COUNT; i++) {
        if (i) h += CARD_GAP;
        h += ui_card_height(f, &cards[i], CONTENT_W);
    }
    // Clears the back-hint bar (96px) plus the same margin above it
    // that the settings screens leave above their taller save bar.
    h += 40.0f + 96.0f;
    return h;
}

static void render_help_screen(UiCanvas *c, const UiFonts *f, const UiState *st)
{
    ui_draw_scene(c);
    ui_page_header(c, f, "Help",
        "Setup steps, controls and fixes for common problems, in one place.");

    UiCard cards[HELP_CARD_COUNT];
    help_cards_init(cards);

    // .content { top:236px; height:844px; overflow-y:auto } -- same
    // viewport the settings screens scroll within.
    ui_clip_push(c, ui_rect(0.0f, CONTENT_TOP, PAGE_W, CONTENT_H));
    float y = CONTENT_TOP - st->scrollY;
    for (int i = 0; i < HELP_CARD_COUNT; i++) {
        float h = ui_card_height(f, &cards[i], CONTENT_W);
        if (y + h > CONTENT_TOP - 40.0f && y < CONTENT_TOP + CONTENT_H + 40.0f)
            ui_card_draw(c, f, &cards[i], PAGE_MARGIN_X, y, CONTENT_W);
        y += h + CARD_GAP;
    }
    ui_clip_pop(c);

    // A plain hint bar, not the settings screens' save bar -- Help has
    // nothing to save, just somewhere to scroll back from.
    {
        float barH = 96.0f;
        float barY = PAGE_H - barH;
        ui_fill_rect(c, ui_rect(0.0f, barY, PAGE_W, barH), COL_BG0);
        ui_fill_rect(c, ui_rect(0.0f, barY, PAGE_W, 1.0f), COL_PANEL_BORDER);

        float hintsTop = barY + (barH - ui_hint_row_line_height(f)) * 0.5f;
        float hintsBase = hintsTop + ui_hint_row_baseline_above(f);
        UiHintItem hints[2] = {
            { "D-Pad", "scroll" },
            { "\xE2\x97\x8B", "back" },
        };
        ui_draw_hint_row(c, f, PAGE_MARGIN_X, hintsBase, hints, 2);
    }
}

// ---------------------------------------------------------------
// Settings-screen shell
// ---------------------------------------------------------------

static int build_cards(const AmbientConfig *cfg, const UiState *st, UiScreenId screen, ValueBuf *vb,
                        UiCard *cards, UiField *fp, UiFieldRow *rp)
{
    MenuScreen menuScreen = (screen == UI_SCREEN_SETUP) ? MENU_SCREEN_SETUP : MENU_SCREEN_CUSTOMIZE;
    return build_group_cards(menuScreen, cfg, st->focusField, vb, cards, fp, rp);
}

float ui_screen_content_height(const UiFonts *f, const AmbientConfig *cfg, UiScreenId screen)
{
    if (screen == UI_SCREEN_HELP) return help_content_height(f);

    UiCard cards[6]; UiField fields[32]; UiFieldRow rows[10];
    ValueBuf vb; vb.n = 0;
    UiState st; memset(&st, 0, sizeof(st)); st.focusField = -1;

    int n = build_cards(cfg, &st, screen, &vb, cards, fields, rows);
    float h = 0.0f;
    for (int i = 0; i < n; i++) {
        if (i) h += CARD_GAP;
        h += ui_card_height(f, &cards[i], CONTENT_W);
    }
    // .content-inner padding-bottom: clears the save bar (now 156px,
    // up from 120px, to fit the hint row) plus the same margin above
    // it the original 190px left for a 120px bar.
    h += 226.0f;
    return h;
}

bool ui_screen_focus_bounds(const UiFonts *f, const AmbientConfig *cfg, UiScreenId screen,
                             int fieldIndex, float *outTop, float *outBottom)
{
    if (fieldIndex < 0) return false;

    UiCard cards[6]; UiField fields[32]; UiFieldRow rows[10];
    ValueBuf vb; vb.n = 0;
    UiState st; memset(&st, 0, sizeof(st)); st.focusField = -1; st.screen = screen;

    int n = build_cards(cfg, &st, screen, &vb, cards, fields, rows);
    float y = CONTENT_TOP;
    int seen = 0;
    for (int i = 0; i < n; i++) {
        for (int r = 0; r < cards[i].rowCount; r++) {
            int cnt = cards[i].rows[r].count;
            if (fieldIndex < seen + cnt) {
                float ry, rh;
                if (!ui_card_row_offset(f, &cards[i], r, &ry, &rh)) return false;
                *outTop = y + ry;
                *outBottom = y + ry + rh;
                return true;
            }
            seen += cnt;
        }
        y += ui_card_height(f, &cards[i], CONTENT_W) + CARD_GAP;
    }
    return false;
}

static void render_settings_screen(UiCanvas *c, const UiFonts *f, const AmbientConfig *cfg,
                                    const UiState *st, const char *title, const char *subtitle)
{
    ui_draw_scene(c);
    ui_page_header(c, f, title, subtitle);

    UiCard cards[6]; UiField fields[32]; UiFieldRow rows[10];
    ValueBuf vb; vb.n = 0;
    int n = build_cards(cfg, st, st->screen, &vb, cards, fields, rows);

    // .content { top:236px; height:844px; overflow-y:auto }
    ui_clip_push(c, ui_rect(0.0f, CONTENT_TOP, PAGE_W, CONTENT_H));
    float y = CONTENT_TOP - st->scrollY;
    for (int i = 0; i < n; i++) {
        float h = ui_card_height(f, &cards[i], CONTENT_W);
        if (y + h > CONTENT_TOP - 40.0f && y < CONTENT_TOP + CONTENT_H + 40.0f)
            ui_card_draw(c, f, &cards[i], PAGE_MARGIN_X, y, CONTENT_W);
        y += h + CARD_GAP;
    }
    ui_clip_pop(c);

    {
        // .save-bar -- shared by Set up and Customization, not
        // Customization-only. Taller than the blueprint's own version
        // (120px -> 156px) to fit a hint row the blueprint doesn't
        // have at all: L1/R1's value-nudge shortcut has no other
        // on-screen indication anywhere, so without this a user would
        // only ever find it by accident.
        float barH = 156.0f;
        float barY = PAGE_H - barH;
        ui_fill_rect(c, ui_rect(0.0f, barY, PAGE_W, barH), COL_BG0);
        ui_fill_rect(c, ui_rect(0.0f, barY, PAGE_W, 1.0f), COL_PANEL_BORDER);

        float noteBoxTop = barY + 36.0f;
        ui_text_draw_box(c, f->note, PAGE_MARGIN_X, noteBoxTop, 0.0f,
                          "Changes apply after saving.", COL_TEXT_BODY);

        float hintsTop = noteBoxTop + ui_font_line_normal(f->note) + 12.0f;
        float hintsBase = hintsTop + ui_hint_row_baseline_above(f);
        UiHintItem hints[3] = {
            { "L1/R1", "nudge value" },
            { "\xE2\x9C\x95", "edit / cycle" },
            { "\xE2\x97\x8B", "back" },
        };
        ui_draw_hint_row(c, f, PAGE_MARGIN_X, hintsBase, hints, 3);

        // .save-btn -- a solid amber fill, same as the blueprint,
        // always, EXCEPT for two states layered on top, neither of
        // which changes that resting look:
        //  - focused: a brighter fill (still clearly "the amber
        //    button") plus a ring drawn outside it rather than
        //    brackets drawn on top of it -- corner brackets sitting
        //    directly on an already-amber fill had too little contrast
        //    to actually see, which was the original bug here.
        //  - just saved: swaps to a green fill, a checkmark, and
        //    "Saved!" for a couple of seconds (main.c owns the
        //    timing) -- pressing Save previously had no visible effect
        //    at all beyond the status line at the very bottom of the
        //    screen, easy to miss entirely.
        bool showConfirm = st->saveJustConfirmed;
        const char *label = showConfirm ? "Saved!"
                           : (st->screen == UI_SCREEN_SETUP) ? "Save setup" : "Save customization";
        UiIconId icon = showConfirm ? ICON_CHECK : ICON_SAVE;

        float tw = ui_text_width(f->btn, label);
        float bw = 28.0f * 2.0f + 20.0f + 12.0f + tw;
        float bh = 18.0f * 2.0f + fmaxf(20.0f, ui_font_line_normal(f->btn));
        float bx = PAGE_W - PAGE_MARGIN_X - bw;
        float by = barY + (barH - bh) * 0.5f;
        UiRect btnRect = ui_rect(bx, by, bw, bh);

        int screenFieldCount = (st->screen == UI_SCREEN_SETUP) ? SETUP_FIELD_COUNT : CUST_FIELD_COUNT;
        bool saveFocused = !showConfirm && (st->focusField >= screenFieldCount);

        UiColor fill = showConfirm ? COL_OK : (saveFocused ? COL_AMBER_BRIGHT : COL_AMBER);
        ui_fill_round_rect(c, btnRect, ui_radius_all(RADIUS_INPUT), fill);
        ui_icon_draw(c, icon, bx + 28.0f, by + (bh - 20.0f) * 0.5f, 20.0f, COL_ON_AMBER);
        ui_text_draw_box(c, f->btn, bx + 28.0f + 20.0f + 12.0f,
                          by + (bh - ui_font_line_normal(f->btn)) * 0.5f, 0.0f, label, COL_ON_AMBER);
        if (saveFocused) ui_focus_ring(c, btnRect, ui_radius_all(RADIUS_INPUT), 5.0f, 3.0f, COL_TEXT_HEADING);
    }

    led_frame_draw(c, st->ledSlots, st->ledSlotCount, st->ledColors, st->ledColorCount);
}

// ---------------------------------------------------------------
// Home
// ---------------------------------------------------------------

void ui_render_home_static(UiCanvas *c, const UiFonts *f, const AmbientConfig *cfg, const UiState *st)
{
    ui_draw_scene(c);

    // .home-header
    ui_text_draw_box(c, f->h1, PAGE_MARGIN_X, 64.0f, 0.0f, "PS4 Ambilight", COL_TEXT_HEADING);
    float subTop = 64.0f + ui_font_line_normal(f->h1) + 12.0f;
    ui_text_draw_box(c, f->pageSub, PAGE_MARGIN_X, subTop, 0.0f,
                      "Screen-reactive ambient lighting for your PS4, powered by GoldHEN.", COL_TEXT_BODY);

    // .toast
    if (st->toastVisible) {
        float tw = 640.0f;
        float lh = 1.5f * 14.0f;
        float th = 1.0f + 18.0f + lh * 2.0f + 18.0f + 1.0f;
        UiRect t = ui_rect(PAGE_MARGIN_X, 236.0f, tw, th);
        ui_fill_round_rect(c, t, ui_radius_all(RADIUS_PANEL), COL_PANEL_BG);
        ui_stroke_round_rect(c, t, ui_radius_all(RADIUS_PANEL), 1.0f, COL_PANEL_BORDER);
        ui_clip_push(c, ui_rect(t.x, t.y, 3.0f, th));
        ui_stroke_round_rect(c, t, ui_radius_all(RADIUS_PANEL), 3.0f, COL_WARN);
        ui_clip_pop(c);

        float cx = t.x + 3.0f + 22.0f;
        float cy = t.y + 1.0f + 18.0f;
        ui_icon_draw(c, ICON_WARNING, cx, cy + 2.0f, 18.0f, COL_WARN);   // .icon { margin-top: 2px }
        float px = cx + 18.0f + 14.0f;
        ui_text_draw_box(c, f->hint, px, cy, lh, "LEDs aren't set up yet.", COL_TEXT_HEADING);
        ui_text_draw_box(c, f->hint, px, cy + lh, lh,
                          "Add your WLED connection and strip layout on Set Up first.", COL_TEXT_LABEL);
    }

    // .banner
    {
        UiColor accent; const char *title; const char *desc;
        switch (st->install) {
            case UI_INSTALL_NONE:
                accent = COL_TEXT_BODY;
                title = "Plugin not installed";
                desc = "Install ps4_ambient_light.prx to enable ambient lighting.";
                break;
            case UI_INSTALL_UPDATE:
                accent = COL_WARN;
                title = "Update available";
                desc = "A newer version of ps4_ambient_light.prx is ready.";
                break;
            case UI_INSTALL_DISABLED:
                accent = COL_WARN;
                title = "Plugin disabled";
                desc = "ps4_ambient_light.prx is installed, but not active in plugins.ini.";
                break;
            default:
                accent = COL_OK;
                title = "Plugin installed";
                desc = "ps4_ambient_light.prx is registered and up to date.";
                break;
        }

        float h2h = ui_font_line_normal(f->cardTitle);
        float ph = 1.55f * 15.0f;
        float block = h2h + 6.0f + ph;
        float bh = 1.0f + 32.0f + block + 32.0f + 1.0f;
        UiRect b = ui_rect(PAGE_MARGIN_X, 330.0f, CONTENT_W, bh);

        ui_fill_round_rect(c, b, ui_radius_all(RADIUS_PANEL), COL_PANEL_BG);
        ui_stroke_round_rect(c, b, ui_radius_all(RADIUS_PANEL), 1.0f, COL_PANEL_BORDER);
        ui_clip_push(c, ui_rect(b.x, b.y, 3.0f, bh));
        ui_stroke_round_rect(c, b, ui_radius_all(RADIUS_PANEL), 3.0f, accent);
        ui_clip_pop(c);

        float cx = b.x + 3.0f + 40.0f;
        float cy = b.y + 1.0f + 32.0f;

        // .dot with its 4px box-shadow halo
        float dcx = cx + 6.0f, dcy = cy + block * 0.5f;
        ui_fill_circle(c, dcx, dcy, 10.0f, ui_fade(accent, 0.16f));
        ui_fill_circle(c, dcx, dcy, 6.0f, accent);

        float tx = cx + 12.0f + 22.0f;
        ui_text_draw_box(c, f->cardTitle, tx, cy, 0.0f, title, COL_TEXT_HEADING);
        ui_text_draw_box(c, f->monoBanner, tx, cy + h2h + 6.0f, ph, desc, COL_TEXT_BODY);
    }

    // .cta
    {
        const char *label; UiIconId icon; UiColor fill = COL_AMBER;
        if (st->install == UI_INSTALL_NONE)        { label = "Install plugin"; icon = ICON_DOWNLOAD; }
        else if (st->install == UI_INSTALL_UPDATE) { label = "Update plugin";  icon = ICON_REFRESH; }
        else if (st->install == UI_INSTALL_DISABLED) { label = "Enable plugin"; icon = ICON_REFRESH; }
        else if (st->testRunning)                  { label = "Stop test";      icon = ICON_STOP; fill = COL_CYAN; }
        else                                        { label = "Test strip";     icon = ICON_ACTIVITY; }

        UiRect r = ui_rect(PAGE_MARGIN_X, 472.0f, CONTENT_W, 96.0f);
        ui_fill_round_rect(c, r, ui_radius_all(RADIUS_PANEL), fill);

        float tw = ui_text_width(f->cta, label);
        float total = 22.0f + 14.0f + tw;
        float sx = r.x + (r.w - total) * 0.5f;
        ui_icon_draw(c, icon, sx, r.y + (r.h - 22.0f) * 0.5f, 22.0f, COL_ON_AMBER);
        ui_text_draw_box(c, f->cta, sx + 22.0f + 14.0f,
                          r.y + (r.h - ui_font_line_normal(f->cta)) * 0.5f, 0.0f, label, COL_ON_AMBER);

        if (st->homeFocus == HOME_FOCUS_CTA) ui_focus_brackets(c, r, COL_TEXT_HEADING);
    }

    // .home-nav
    {
        const char *labels[3] = { "Help", "Set up", "Customization" };
        UiIconId icons[3] = { ICON_HELP, ICON_CALENDAR, ICON_LEVELS };
        float gap = 16.0f;
        float w = (CONTENT_W - gap * 2.0f) / 3.0f;
        // align-items: stretch makes every pill as tall as the tallest,
        // which is the selected one with its 30px icon badge.
        float h = 1.0f + 24.0f + 30.0f + 24.0f + 2.0f;
        float top = 640.0f;

        for (int i = 0; i < 3; i++) {
            float x = PAGE_MARGIN_X + (float)i * (w + gap);
            bool selected = (st->homeFocus == HOME_FOCUS_HELP + i);
            UiRect r = ui_rect(x, top, w, h);

            ui_fill_round_rect(c, r, ui_radius_all(RADIUS_PANEL),
                                selected ? COL_PANEL_BG_RAISED : COL_PANEL_BG);
            ui_stroke_round_rect(c, r, ui_radius_all(RADIUS_PANEL), 1.0f,
                                  selected ? COL_PANEL_BORDER_STRONG : COL_PANEL_BORDER);
            // border-bottom: 2px solid var(--accent-amber) when selected
            if (selected) {
                ui_clip_push(c, ui_rect(r.x, r.y + h - 2.0f, w, 2.0f));
                ui_stroke_round_rect(c, r, ui_radius_all(RADIUS_PANEL), 2.0f, COL_AMBER);
                ui_clip_pop(c);
            }

            float contentTop = r.y + 1.0f + 24.0f;
            float contentH = 30.0f;
            float ix = r.x + 1.0f + 28.0f;

            if (selected) {
                // .icon-badge -- a 30px amber-dim tile holding a 16px icon
                UiRect badge = ui_rect(ix, contentTop, 30.0f, 30.0f);
                ui_fill_round_rect(c, badge, ui_radius_all(2.0f), COL_AMBER_DIM);
                ui_icon_draw(c, icons[i], ix + 7.0f, contentTop + 7.0f, 16.0f, COL_AMBER);
                ix += 30.0f + 16.0f;
            } else {
                ui_icon_draw(c, icons[i], ix, contentTop + (contentH - 23.0f) * 0.5f, 23.0f, COL_TEXT_LABEL);
                ix += 23.0f + 16.0f;
            }

            ui_text_draw_box(c, f->navPill, ix,
                              contentTop + (contentH - ui_font_line_normal(f->navPill)) * 0.5f, 0.0f,
                              labels[i], COL_TEXT_HEADING);
        }
    }

    // .home-footer
    {
        float top = 792.0f;
        ui_fill_rect(c, ui_rect(PAGE_MARGIN_X, top, CONTENT_W, 1.0f), COL_PANEL_BORDER);
        float y = top + 1.0f + 28.0f;

        // The hints line contains inline-flex .keycap boxes, which are
        // taller than the 14px text strut. CSS grows the line box
        // around them -- and since a keycap aligns its own text
        // baseline with the line's, the extra height lands above the
        // baseline. Ignoring this put the whole footer 2px high and
        // the status line under it 5px high.
        float hintsLineH = ui_hint_row_line_height(f);
        float base = y + ui_hint_row_baseline_above(f);

        UiHintItem hints[3] = {
            { "D-Pad", "move" }, { "\xE2\x9C\x95", "select" }, { "Opt", "save & quit" }
        };
        ui_draw_hint_row(c, f, PAGE_MARGIN_X, base, hints, 3);

        float sy = y + hintsLineH + 14.0f;   // .status { margin-top: 14px }
        float sbase = ui_text_baseline_for_box(f->monoStatus, sy, 0.0f);
        const char *line = st->statusLine[0] ? st->statusLine : "Ready.";
        if (st->statusIsSaved) {
            // Blueprint's own two-tone example: a cyan "saved -> " flag
            // ahead of the rest of the line in the muted label colour.
            const char *flag = "saved \xE2\x86\x92 ";
            ui_text_draw(c, f->monoStatus, PAGE_MARGIN_X, sbase, flag, COL_CYAN);
            float fx = PAGE_MARGIN_X + ui_text_width(f->monoStatus, flag);
            ui_text_draw(c, f->monoStatus, fx, sbase, line, COL_TEXT_LABEL);
        } else {
            ui_text_draw(c, f->monoStatus, PAGE_MARGIN_X, sbase, line,
                          st->statusIsError ? COL_WARN : COL_TEXT_LABEL);
        }
    }
}

// Thin wrapper matching this function's old combined behavior, used
// by ui_render()'s own dispatch below (which callers like the desktop
// harness still go through -- they have no reason to know about the
// caching split main.c uses).
static void render_home(UiCanvas *c, const UiFonts *f, const AmbientConfig *cfg, const UiState *st)
{
    ui_render_home_static(c, f, cfg, st);
    if (st->testRunning) led_frame_draw(c, st->ledSlots, st->ledSlotCount, st->ledColors, st->ledColorCount);
}

// ---------------------------------------------------------------

void ui_render(UiCanvas *c, const UiFonts *f, const AmbientConfig *cfg, const UiState *st)
{
    ui_clip_reset(c);
    switch (st->screen) {
        case UI_SCREEN_SETUP:
            render_settings_screen(c, f, cfg, st, "Set up",
                "Connect WLED and describe how the physical LED strip runs around your display.");
            break;
        case UI_SCREEN_CUSTOMIZATION:
            render_settings_screen(c, f, cfg, st, "Customization",
                "Tune how captured colours are sampled, balanced and animated on the strip.");
            break;
        case UI_SCREEN_HELP:
            render_help_screen(c, f, st);
            break;
        case UI_SCREEN_HOME:
        default:
            render_home(c, f, cfg, st);
            break;
    }
}
