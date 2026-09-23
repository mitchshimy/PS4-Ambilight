// ui_widgets.c -- see ui_widgets.h.

#include "ui_widgets.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

// ---- box metrics lifted straight from style.css --------------------
#define CARD_BORDER        1.0f
#define CARD_BORDER_LEFT   3.0f
#define CARD_PAD_TOP      36.0f
#define CARD_PAD_X        44.0f
#define CARD_PAD_BOTTOM   40.0f

#define TITLE_ICON        22.0f
#define TITLE_GAP         14.0f
#define TITLE_MB          10.0f

#define DESC_LH            1.6f     // .card-desc { line-height: 1.6 }
#define DESC_MAX_W      1300.0f
#define DESC_MB           28.0f

#define FIELD_GAP         20.0f
#define FIELD_BASIS      240.0f

#define LABEL_ICON        17.0f
#define LABEL_GAP          9.0f
#define LABEL_MB          10.0f

#define INPUT_PAD_Y       19.0f
#define INPUT_PAD_X       22.0f
#define INPUT_BORDER       1.0f

#define PILL_PAD_Y        19.0f
#define PILL_PAD_X        24.0f
#define PILL_GAP          12.0f
#define PILL_ICON         19.0f

#define TOGGLE_PAD_Y      14.0f
#define TOGGLE_PAD_X      20.0f
#define TOGGLE_W          56.0f
#define TOGGLE_H          30.0f

#define TOTAL_MT          24.0f
#define TOTAL_PAD_TOP     20.0f
#define TOTAL_MB          22.0f
#define TOTAL_ICON        18.0f
#define TOTAL_GAP         10.0f

static float fmaxf2(float a, float b) { return a > b ? a : b; }

// ---------------------------------------------------------------
// Text wrapping
// ---------------------------------------------------------------

int ui_wrap_text(UiFont *font, const char *text, float maxWidth, char out[][256], int maxLines)
{
    int lines = 0;
    const char *p = text;
    char cur[256];
    cur[0] = '\0';
    int curLen = 0;

    while (*p && lines < maxLines) {
        // Grab the next word plus any trailing space.
        const char *wordStart = p;
        while (*p && *p != ' ') p++;
        int wordLen = (int)(p - wordStart);
        while (*p == ' ') p++;

        char candidate[256];
        int n = 0;
        if (curLen > 0) { memcpy(candidate, cur, (size_t)curLen); n = curLen; candidate[n++] = ' '; }
        if (n + wordLen >= 255) wordLen = 254 - n;
        memcpy(candidate + n, wordStart, (size_t)wordLen);
        n += wordLen;
        candidate[n] = '\0';

        if (curLen > 0 && ui_text_width(font, candidate) > maxWidth) {
            memcpy(out[lines], cur, (size_t)curLen + 1);
            lines++;
            if (lines >= maxLines) break;
            if (wordLen > 255) wordLen = 255;
            memcpy(cur, wordStart, (size_t)wordLen);
            cur[wordLen] = '\0';
            curLen = wordLen;
        } else {
            memcpy(cur, candidate, (size_t)n + 1);
            curLen = n;
        }
    }
    if (curLen > 0 && lines < maxLines) { memcpy(out[lines], cur, (size_t)curLen + 1); lines++; }
    return lines;
}

// ---------------------------------------------------------------
// Focus marks
// ---------------------------------------------------------------
//
// style.css draws two 12x12 L-brackets in the accent colour at the top
// corners of a focused control (both ::before and ::after sit at
// top:-2px), replacing the glow ring the old UI used. Note both are at
// the TOP -- not opposite corners.
void ui_focus_brackets(UiCanvas *c, UiRect r, UiColor color)
{
    const float L = 12.0f, T = 2.0f, OFF = 2.0f;
    float x0 = r.x - OFF, y0 = r.y - OFF;
    float x1 = r.x + r.w + OFF;

    // top-left: top edge + left edge
    ui_fill_rect(c, ui_rect(x0, y0, L, T), color);
    ui_fill_rect(c, ui_rect(x0, y0, T, L), color);
    // top-right: top edge + right edge
    ui_fill_rect(c, ui_rect(x1 - L, y0, L, T), color);
    ui_fill_rect(c, ui_rect(x1 - T, y0, T, L), color);
}

void ui_focus_ring(UiCanvas *c, UiRect r, UiRadius radius, float gap, float thickness, UiColor color)
{
    UiRect outer = ui_rect(r.x - gap - thickness, r.y - gap - thickness,
                            r.w + 2.0f * (gap + thickness), r.h + 2.0f * (gap + thickness));
    UiRadius outerRadius = ui_radius4(radius.tl + gap, radius.tr + gap, radius.br + gap, radius.bl + gap);
    ui_stroke_round_rect(c, outer, outerRadius, thickness, color);
}

// ---------------------------------------------------------------
// Keycap
// ---------------------------------------------------------------

void ui_draw_keycap(UiCanvas *c, const UiFonts *f, float x, float baselineY, const char *label, float *outAdvance)
{
    UiFont *kf = f->monoKeycap;
    float tw = ui_text_width(kf, label);
    float w = tw + 14.0f + 2.0f;          // padding 2px 7px + 1px borders
    if (w < 26.0f) w = 26.0f;
    float h = 2.0f + 4.0f + ui_font_line_normal(kf);

    // An inline-flex box aligns its own text baseline with the
    // surrounding line's baseline.
    float top = baselineY - 3.0f - ui_font_ascent(kf);

    UiRect r = ui_rect(x, top, w, h);
    ui_fill_round_rect(c, r, ui_radius_all(3.0f), COL_PANEL_BG_RAISED);
    ui_stroke_round_rect(c, r, ui_radius_all(3.0f), 1.0f, COL_PANEL_BORDER_STRONG);
    ui_text_draw(c, kf, x + (w - tw) * 0.5f, baselineY, label, COL_TEXT_LABEL);

    if (outAdvance) *outAdvance = w + 6.0f;   // margin-right: 6px
}

// A hint row's line box is taller than its own text, because the
// keycap chips inside it are taller than the surrounding 14px text --
// same CSS line-box reasoning ui_text_baseline_for_box() handles for
// plain text, worked out once here so every caller agrees.
static void ui_hint_row_metrics(const UiFonts *f, float *outAbove, float *outBelow)
{
    UiFont *kf = f->monoKeycap;
    float capH = 2.0f + 4.0f + ui_font_line_normal(kf);
    float capAbove = 3.0f + ui_font_ascent(kf);
    float capBelow = capH - capAbove;
    *outAbove = fmaxf(capAbove, ui_font_ascent(f->hint));
    *outBelow = fmaxf(capBelow, ui_font_descent(f->hint));
}

float ui_hint_row_line_height(const UiFonts *f)
{
    float above, below;
    ui_hint_row_metrics(f, &above, &below);
    return above + below;
}

// Distance from a hint row's line-box top to its baseline -- the
// piece ui_hint_row_line_height's total doesn't expose on its own,
// and every caller that positions a row needs.
float ui_hint_row_baseline_above(const UiFonts *f)
{
    float above, below;
    ui_hint_row_metrics(f, &above, &below);
    (void)below;
    return above;
}

void ui_draw_hint_row(UiCanvas *c, const UiFonts *f, float x, float baselineY,
                       const UiHintItem *items, int count)
{
    float adv = 0.0f;
    for (int i = 0; i < count; i++) {
        ui_draw_keycap(c, f, x, baselineY, items[i].cap, &adv);
        x += adv;
        ui_text_draw(c, f->hint, x, baselineY, items[i].word, COL_TEXT_BODY);
        x += ui_text_width(f->hint, items[i].word);
        x += ui_text_width(f->hint, "   ");   // three &nbsp; between groups, matching the blueprint
    }
}

// ---------------------------------------------------------------
// Field sizing
// ---------------------------------------------------------------

static float field_control_height(const UiFonts *f, const UiField *fd)
{
    switch (fd->kind) {
        case UI_FIELD_PILL:
            return 2 * INPUT_BORDER + 2 * PILL_PAD_Y
                 + fmaxf2(PILL_ICON, ui_font_line_normal(f->monoPill));
        case UI_FIELD_TOGGLE:
            return 2 * INPUT_BORDER + 2 * TOGGLE_PAD_Y
                 + fmaxf2(TOGGLE_H, ui_font_line_normal(f->monoToggle));
        case UI_FIELD_INPUT:
        default:
            return 2 * INPUT_BORDER + 2 * INPUT_PAD_Y + ui_font_line_normal(f->monoInput);
    }
}

static float label_height(const UiFonts *f)
{
    return fmaxf2(LABEL_ICON, ui_font_line_normal(f->fieldLabel));
}

static float row_height(const UiFonts *f, const UiFieldRow *row)
{
    float ctrl = 0.0f;
    for (int i = 0; i < row->count; i++)
        ctrl = fmaxf2(ctrl, field_control_height(f, &row->fields[i]));
    return label_height(f) + LABEL_MB + ctrl;
}

// Resolves `flex: 1 1 240px` / `flex: 0 0 Npx` across one row.
static void row_widths(const UiFieldRow *row, float containerW, float *outW)
{
    float basisSum = 0.0f;
    int flexCount = 0;
    for (int i = 0; i < row->count; i++) {
        float b = row->fields[i].basis > 0 ? row->fields[i].basis : FIELD_BASIS;
        basisSum += b;
        if (!row->fields[i].fixed) flexCount++;
    }
    float free = containerW - FIELD_GAP * (float)(row->count - 1) - basisSum;
    float share = (flexCount > 0 && free > 0.0f) ? free / (float)flexCount : 0.0f;

    for (int i = 0; i < row->count; i++) {
        float b = row->fields[i].basis > 0 ? row->fields[i].basis : FIELD_BASIS;
        outW[i] = row->fields[i].fixed ? b : b + share;
    }
}

// ---------------------------------------------------------------
// Card measurement
// ---------------------------------------------------------------

static float total_row_height(const UiFonts *f)
{
    return TOTAL_MT + 1.0f + TOTAL_PAD_TOP
         + fmaxf2(TOTAL_ICON, ui_font_line_normal(f->monoTotal)) + TOTAL_MB;
}

float ui_card_height(const UiFonts *f, const UiCard *card, float width)
{
    (void)width;
    float h = CARD_BORDER + CARD_PAD_TOP;

    h += fmaxf2(TITLE_ICON, ui_font_line_normal(f->cardTitle)) + TITLE_MB;

    if (card->desc) {
        char lines[8][256];
        int n = ui_wrap_text(f->cardDesc, card->desc, DESC_MAX_W, lines, 8);
        h += (float)n * (DESC_LH * 16.0f) + DESC_MB;
    }

    for (int r = 0; r < card->rowCount; r++) {
        h += card->rows[r].marginTop;
        h += row_height(f, &card->rows[r]);
        if (card->totalAfterRow == r) h += total_row_height(f);
    }

    h += CARD_PAD_BOTTOM + CARD_BORDER;
    return h;
}

bool ui_card_row_offset(const UiFonts *f, const UiCard *card, int row, float *outY, float *outH)
{
    if (row < 0 || row >= card->rowCount) return false;

    float y = CARD_BORDER + CARD_PAD_TOP;
    y += fmaxf2(TITLE_ICON, ui_font_line_normal(f->cardTitle)) + TITLE_MB;
    if (card->desc) {
        char lines[8][256];
        int n = ui_wrap_text(f->cardDesc, card->desc, DESC_MAX_W, lines, 8);
        y += (float)n * (DESC_LH * 16.0f) + DESC_MB;
    }
    for (int r = 0; r < card->rowCount; r++) {
        y += card->rows[r].marginTop;
        if (r == row) { *outY = y; *outH = row_height(f, &card->rows[r]); return true; }
        y += row_height(f, &card->rows[r]);
        if (card->totalAfterRow == r) y += total_row_height(f);
    }
    return false;
}

// ---------------------------------------------------------------
// Field drawing
// ---------------------------------------------------------------

static void draw_input(UiCanvas *c, const UiFonts *f, UiRect r, const char *value, bool focused)
{
    ui_fill_round_rect(c, r, ui_radius_all(RADIUS_INPUT), COL_INPUT_BG);
    ui_stroke_round_rect(c, r, ui_radius_all(RADIUS_INPUT), INPUT_BORDER, COL_INPUT_BORDER);

    float baseline = ui_text_baseline_for_box(f->monoInput, r.y + INPUT_BORDER + INPUT_PAD_Y, 0.0f);
    ui_text_draw(c, f->monoInput, r.x + INPUT_BORDER + INPUT_PAD_X, baseline, value ? value : "", COL_TEXT_VALUE);

    if (focused) ui_focus_brackets(c, r, COL_AMBER);
}

static void draw_pill(UiCanvas *c, const UiFonts *f, UiRect r, const UiField *fd)
{
    ui_fill_round_rect(c, r, ui_radius_all(RADIUS_INPUT), COL_PANEL_BG_RAISED);
    ui_stroke_round_rect(c, r, ui_radius_all(RADIUS_INPUT), INPUT_BORDER, COL_INPUT_BORDER);

    float lineH = fmaxf2(PILL_ICON, ui_font_line_normal(f->monoPill));
    float contentTop = r.y + INPUT_BORDER + PILL_PAD_Y;
    float x = r.x + INPUT_BORDER + PILL_PAD_X;

    if (fd->valueIcon != ICON_NONE) {
        ui_icon_draw(c, fd->valueIcon, x, contentTop + (lineH - PILL_ICON) * 0.5f, PILL_ICON, COL_TEXT_LABEL);
        x += PILL_ICON + PILL_GAP;
    }
    float baseline = ui_text_baseline_for_box(f->monoPill,
                                               contentTop + (lineH - ui_font_line_normal(f->monoPill)) * 0.5f, 0.0f);
    ui_text_draw(c, f->monoPill, x, baseline, fd->value ? fd->value : "", COL_TEXT_VALUE);

    if (fd->focused) ui_focus_brackets(c, r, COL_AMBER);
}

static void draw_toggle(UiCanvas *c, const UiFonts *f, UiRect r, const UiField *fd)
{
    ui_fill_round_rect(c, r, ui_radius_all(RADIUS_INPUT), COL_INPUT_BG);
    ui_stroke_round_rect(c, r, ui_radius_all(RADIUS_INPUT), INPUT_BORDER, COL_INPUT_BORDER);

    float lineH = fmaxf2(TOGGLE_H, ui_font_line_normal(f->monoToggle));
    float contentTop = r.y + INPUT_BORDER + TOGGLE_PAD_Y;

    float baseline = ui_text_baseline_for_box(f->monoToggle,
                                               contentTop + (lineH - ui_font_line_normal(f->monoToggle)) * 0.5f, 0.0f);
    ui_text_draw(c, f->monoToggle, r.x + INPUT_BORDER + TOGGLE_PAD_X, baseline,
                 fd->toggleOn ? "On" : "Off", COL_TEXT_VALUE);

    // justify-content: space-between puts the switch at the far end.
    float tx = r.x + r.w - INPUT_BORDER - TOGGLE_PAD_X - TOGGLE_W;
    float ty = contentTop + (lineH - TOGGLE_H) * 0.5f;
    UiRect sw = ui_rect(tx, ty, TOGGLE_W, TOGGLE_H);

    if (fd->toggleOn) {
        ui_fill_round_rect(c, sw, ui_radius_all(3.0f), COL_AMBER_DIM);
        ui_stroke_round_rect(c, sw, ui_radius_all(3.0f), 1.0f, COL_AMBER);
        ui_fill_round_rect(c, ui_rect(tx + 29.0f, ty + 3.0f, 22.0f, 22.0f), ui_radius_all(2.0f), COL_AMBER);
    } else {
        ui_fill_round_rect(c, sw, ui_radius_all(3.0f), COL_INPUT_BG);
        ui_stroke_round_rect(c, sw, ui_radius_all(3.0f), 1.0f, COL_INPUT_BORDER);
        ui_fill_round_rect(c, ui_rect(tx + 3.0f, ty + 3.0f, 22.0f, 22.0f), ui_radius_all(2.0f),
                            ui_rgb(0x3a, 0x3f, 0x48));
    }

    if (fd->focused) ui_focus_brackets(c, r, COL_AMBER);
}

static void draw_field(UiCanvas *c, const UiFonts *f, const UiField *fd, float x, float y, float w)
{
    // .field-label -- icon and text on one centred flex line
    float labelH = label_height(f);
    UiColor lc = (fd->labelColor.a == 0) ? COL_TEXT_LABEL : fd->labelColor;

    float lx = x;
    if (fd->labelIcon != ICON_NONE) {
        ui_icon_draw(c, fd->labelIcon, lx, y + (labelH - LABEL_ICON) * 0.5f, LABEL_ICON, lc);
        lx += LABEL_ICON + LABEL_GAP;
    }
    float lbase = ui_text_baseline_for_box(f->fieldLabel,
                                            y + (labelH - ui_font_line_normal(f->fieldLabel)) * 0.5f, 0.0f);
    ui_text_draw(c, f->fieldLabel, lx, lbase, fd->label, lc);

    float cy = y + labelH + LABEL_MB;
    UiRect r = ui_rect(x, cy, w, field_control_height(f, fd));

    switch (fd->kind) {
        case UI_FIELD_PILL:   draw_pill(c, f, r, fd); break;
        case UI_FIELD_TOGGLE: draw_toggle(c, f, r, fd); break;
        case UI_FIELD_INPUT:
        default:              draw_input(c, f, r, fd->value, fd->focused); break;
    }
}

// ---------------------------------------------------------------
// Card drawing
// ---------------------------------------------------------------

void ui_card_draw(UiCanvas *c, const UiFonts *f, const UiCard *card, float x, float y, float width)
{
    float h = ui_card_height(f, card, width);
    UiRect outer = ui_rect(x, y, width, h);

    ui_fill_round_rect(c, outer, ui_radius_all(RADIUS_PANEL), COL_PANEL_BG);
    ui_stroke_round_rect(c, outer, ui_radius_all(RADIUS_PANEL), CARD_BORDER, COL_PANEL_BORDER);

    // border-left: 3px solid var(--panel-accent). Drawn as a clipped
    // rounded rect so it follows the panel's own 8px corners instead of
    // sticking out as a square stripe.
    ui_clip_push(c, ui_rect(x, y, CARD_BORDER_LEFT, h));
    ui_stroke_round_rect(c, outer, ui_radius_all(RADIUS_PANEL), CARD_BORDER_LEFT, card->accent);
    ui_clip_pop(c);

    float contentX = x + CARD_BORDER_LEFT + CARD_PAD_X;
    float contentW = width - CARD_BORDER_LEFT - CARD_BORDER - 2 * CARD_PAD_X;
    float cy = y + CARD_BORDER + CARD_PAD_TOP;

    // .card-title
    {
        float lineH = fmaxf2(TITLE_ICON, ui_font_line_normal(f->cardTitle));
        float tx = contentX;
        if (card->icon != ICON_NONE) {
            UiColor ic = (card->iconColor.a == 0) ? card->accent : card->iconColor;
            ui_icon_draw(c, card->icon, tx, cy + (lineH - TITLE_ICON) * 0.5f, TITLE_ICON, ic);
            tx += TITLE_ICON + TITLE_GAP;
        }
        float base = ui_text_baseline_for_box(f->cardTitle,
                                               cy + (lineH - ui_font_line_normal(f->cardTitle)) * 0.5f, 0.0f);
        ui_text_draw(c, f->cardTitle, tx, base, card->title, COL_TEXT_HEADING);
        cy += lineH + TITLE_MB;
    }

    // .card-desc
    if (card->desc) {
        char lines[8][256];
        int n = ui_wrap_text(f->cardDesc, card->desc, DESC_MAX_W, lines, 8);
        float lh = DESC_LH * 16.0f;
        for (int i = 0; i < n; i++) {
            ui_text_draw_box(c, f->cardDesc, contentX, cy + (float)i * lh, lh, lines[i], COL_TEXT_BODY);
        }
        cy += (float)n * lh + DESC_MB;
    }

    // field rows
    for (int r = 0; r < card->rowCount; r++) {
        const UiFieldRow *row = &card->rows[r];
        cy += row->marginTop;

        float widths[12];
        row_widths(row, contentW, widths);

        float fx = contentX;
        for (int i = 0; i < row->count && i < 12; i++) {
            draw_field(c, f, &row->fields[i], fx, cy, widths[i]);
            fx += widths[i] + FIELD_GAP;
        }
        cy += row_height(f, row);

        if (card->totalAfterRow == r) {
            cy += TOTAL_MT;
            ui_dashed_hline(c, contentX, cy, contentW, 6.0f, 6.0f, 1.0f, COL_PANEL_BORDER);
            cy += 1.0f + TOTAL_PAD_TOP;

            float lineH = fmaxf2(TOTAL_ICON, ui_font_line_normal(f->monoTotal));
            float ix = contentX;
            ui_icon_draw(c, ICON_LED_STRIP, ix, cy + (lineH - TOTAL_ICON) * 0.5f, TOTAL_ICON, COL_AMBER);
            ix += TOTAL_ICON + TOTAL_GAP;

            float base = ui_text_baseline_for_box(f->monoTotal,
                                                   cy + (lineH - ui_font_line_normal(f->monoTotal)) * 0.5f, 0.0f);
            ui_text_draw(c, f->monoTotal, ix, base, card->totalLabel, COL_TEXT_LABEL);
            ix += ui_text_width(f->monoTotal, card->totalLabel);
            ui_text_draw(c, f->monoTotal, ix, base, card->totalValue, COL_AMBER);

            cy += lineH + TOTAL_MB;
        }
    }
}

// ---------------------------------------------------------------
// Page header
// ---------------------------------------------------------------

void ui_page_header(UiCanvas *c, const UiFonts *f, const char *title, const char *subtitle)
{
    // .page-header { left: 144px; top: 64px }
    ui_text_draw_box(c, f->h1, PAGE_MARGIN_X, 64.0f, 0.0f, title, COL_TEXT_HEADING);
    float pTop = 64.0f + ui_font_line_normal(f->h1) + 12.0f;   // h1 line box + p margin-top
    ui_text_draw_box(c, f->pageSub, PAGE_MARGIN_X, pTop, 0.0f, subtitle, COL_TEXT_BODY);
}
