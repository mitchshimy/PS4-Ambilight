// ui_icons.c -- SVG path parsing + the blueprint's icon set.
//
// The parser handles the subset the blueprint actually uses: M, L, H,
// V, C, S, A and Z, in both absolute and relative form, with SVG's
// usual number syntax (comma or whitespace separated, signs acting as
// separators, decimals without a leading zero). Curves and arcs are
// flattened to polylines at a resolution tied to the drawn size, so a
// 17px field-label icon and a 30px card-title icon both stay smooth.

#include "ui_icons.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_SUBPATH_PTS 512

// ---------------------------------------------------------------
// Shape description
// ---------------------------------------------------------------

typedef enum {
    SH_PATH,       // d string
    SH_LINE,       // x1 y1 x2 y2
    SH_RECT,       // x y w h rx
    SH_CIRCLE,     // cx cy r
    SH_POLYLINE    // d string holding "x y x y ..."
} ShapeKind;

typedef struct {
    ShapeKind kind;
    const char *d;
    float p[5];
    bool fill;            // fill instead of stroke
    float strokeWidth;    // 0 -> inherit the icon's stroke width
    // a == 0 means "currentColor"; otherwise an explicit channel colour,
    // matching the few icons in the blueprint that hard-code fills.
    UiColor color;
} Shape;

typedef struct {
    const Shape *shapes;
    int count;
    float strokeWidth;    // the SVG's stroke-width attribute
    bool roundCap;        // stroke-linecap="round"
} IconDef;

// CUR ("currentColor") and CHAN_* are compound-literal macros, not
// static const objects: a static const UiColor CUR = {...} object
// compiles under GCC's default GNU-extension leniency, but Clang
// (which this project's toolchain uses, and which does not extend
// that particular allowance) hard-errors on it with "initializer
// element is not a compile-time constant" the moment CUR's *value* is
// read into another static Shape[] initializer below -- reading a
// named object's value isn't a constant expression in standard C. A
// compound literal built entirely from constant scalars doesn't have
// that problem, since there's no named object being read.
#define CUR ((UiColor){ 0, 0, 0, 0 })
#define CHAN_RED   ((UiColor){ 0xff, 0x6b, 0x5b, 255 })
#define CHAN_GREEN ((UiColor){ 0x5f, 0xdc, 0x94, 255 })
#define CHAN_BLUE  ((UiColor){ 0x5b, 0x9d, 0xff, 255 })

// ---------------------------------------------------------------
// Number / command scanning
// ---------------------------------------------------------------

static void skip_sep(const char **p)
{
    while (**p == ' ' || **p == ',' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
}

static float read_num(const char **p)
{
    skip_sep(p);
    char buf[40];
    int n = 0;
    if (**p == '+' || **p == '-') { if (n < 39) buf[n++] = *(*p)++; }
    while ((**p >= '0' && **p <= '9') || **p == '.') {
        // A second '.' starts a new number in SVG syntax; stop here.
        if (**p == '.') {
            bool seen = false;
            for (int i = 0; i < n; i++) if (buf[i] == '.') seen = true;
            if (seen) break;
        }
        if (n < 39) buf[n++] = *(*p)++;
    }
    if (**p == 'e' || **p == 'E') {
        if (n < 39) buf[n++] = *(*p)++;
        if (**p == '+' || **p == '-') if (n < 39) buf[n++] = *(*p)++;
        while (**p >= '0' && **p <= '9') if (n < 39) buf[n++] = *(*p)++; else (*p)++;
    }
    buf[n] = '\0';
    return (float)atof(buf);
}

static bool is_cmd(char ch)
{
    return strchr("MmLlHhVvCcSsAaZzQqTt", ch) != NULL;
}

// ---------------------------------------------------------------
// Flattening
// ---------------------------------------------------------------

typedef struct {
    float pts[MAX_SUBPATH_PTS * 2];
    int n;
    bool closed;
} SubPath;

typedef void (*SubPathSink)(void *user, const SubPath *sp);

static void sp_add(SubPath *sp, float x, float y)
{
    if (sp->n > 0) {
        float lx = sp->pts[(sp->n - 1) * 2], ly = sp->pts[(sp->n - 1) * 2 + 1];
        if (fabsf(lx - x) < 1e-5f && fabsf(ly - y) < 1e-5f) return;
    }
    if (sp->n >= MAX_SUBPATH_PTS) return;
    sp->pts[sp->n * 2] = x;
    sp->pts[sp->n * 2 + 1] = y;
    sp->n++;
}

static void flatten_cubic(SubPath *sp, float x0, float y0, float x1, float y1,
                           float x2, float y2, float x3, float y3, float scale)
{
    // Segment count from a cheap control-polygon length estimate, so
    // the flattening resolution follows how large the icon is drawn.
    float len = fabsf(x1 - x0) + fabsf(y1 - y0) + fabsf(x2 - x1) + fabsf(y2 - y1)
              + fabsf(x3 - x2) + fabsf(y3 - y2);
    int segs = (int)(len * scale * 0.4f) + 4;
    if (segs > 64) segs = 64;
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / (float)segs, u = 1.0f - t;
        float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        sp_add(sp, a * x0 + b * x1 + c * x2 + d * x3,
                   a * y0 + b * y1 + c * y2 + d * y3);
    }
}

// SVG endpoint-parameterised elliptical arc -> centre parameterisation,
// then flattened. Straight out of the SVG 1.1 implementation notes
// (F.6.5); several blueprint icons (the wifi arcs, the refresh ring,
// the moon, the droplet) are built from these and look wrong if
// approximated as circles.
static void flatten_arc(SubPath *sp, float x0, float y0, float rx, float ry,
                         float phiDeg, bool largeArc, bool sweep,
                         float x1, float y1, float scale)
{
    if (rx == 0.0f || ry == 0.0f) { sp_add(sp, x1, y1); return; }
    rx = fabsf(rx); ry = fabsf(ry);

    float phi = phiDeg * (float)M_PI / 180.0f;
    float cosP = cosf(phi), sinP = sinf(phi);

    float dx2 = (x0 - x1) * 0.5f, dy2 = (y0 - y1) * 0.5f;
    float x1p =  cosP * dx2 + sinP * dy2;
    float y1p = -sinP * dx2 + cosP * dy2;

    float rxs = rx * rx, rys = ry * ry;
    float x1ps = x1p * x1p, y1ps = y1p * y1p;
    float lambda = x1ps / rxs + y1ps / rys;
    if (lambda > 1.0f) {
        float s = sqrtf(lambda);
        rx *= s; ry *= s;
        rxs = rx * rx; rys = ry * ry;
    }

    float sign = (largeArc != sweep) ? 1.0f : -1.0f;
    float num = rxs * rys - rxs * y1ps - rys * x1ps;
    if (num < 0.0f) num = 0.0f;
    float den = rxs * y1ps + rys * x1ps;
    float coef = (den == 0.0f) ? 0.0f : sign * sqrtf(num / den);
    float cxp =  coef * rx * y1p / ry;
    float cyp = -coef * ry * x1p / rx;

    float cx = cosP * cxp - sinP * cyp + (x0 + x1) * 0.5f;
    float cy = sinP * cxp + cosP * cyp + (y0 + y1) * 0.5f;

    float ux = (x1p - cxp) / rx, uy = (y1p - cyp) / ry;
    float vx = (-x1p - cxp) / rx, vy = (-y1p - cyp) / ry;

    float n1 = sqrtf(ux * ux + uy * uy);
    float theta1 = acosf(n1 == 0.0f ? 1.0f : fmaxf(-1.0f, fminf(1.0f, ux / n1)));
    if (uy < 0.0f) theta1 = -theta1;

    float nd = sqrtf((ux * ux + uy * uy) * (vx * vx + vy * vy));
    float cosD = nd == 0.0f ? 1.0f : (ux * vx + uy * vy) / nd;
    float delta = acosf(fmaxf(-1.0f, fminf(1.0f, cosD)));
    if (ux * vy - uy * vx < 0.0f) delta = -delta;
    if (!sweep && delta > 0.0f) delta -= 2.0f * (float)M_PI;
    else if (sweep && delta < 0.0f) delta += 2.0f * (float)M_PI;

    float rmax = rx > ry ? rx : ry;
    int segs = (int)(fabsf(delta) / (float)M_PI * rmax * scale * 1.2f) + 4;
    if (segs > 96) segs = 96;
    for (int i = 1; i <= segs; i++) {
        float t = theta1 + delta * ((float)i / (float)segs);
        float ex = cosf(t) * rx, ey = sinf(t) * ry;
        sp_add(sp, cx + cosP * ex - sinP * ey, cy + sinP * ex + cosP * ey);
    }
}

// Parses `d` (already in viewBox units) and emits flattened subpaths
// through `sink`. `scale` is the viewBox-to-pixel factor, used only to
// pick a flattening resolution; coordinates stay in viewBox units and
// are transformed by the caller.
static void parse_path(const char *d, float scale, SubPathSink sink, void *user)
{
    SubPath sp;
    sp.n = 0; sp.closed = false;

    float cx = 0, cy = 0;        // current point
    float sx = 0, sy = 0;        // subpath start
    float lastC2x = 0, lastC2y = 0;
    bool lastWasCubic = false;

    const char *p = d;
    char cmd = 0;

    while (*p) {
        skip_sep(&p);
        if (!*p) break;
        if (is_cmd(*p)) { cmd = *p; p++; }
        else if (cmd == 0) { p++; continue; }
        // else: an implicit repeat of the previous command

        bool rel = (cmd >= 'a' && cmd <= 'z');
        char c = (char)(rel ? cmd - 32 : cmd);

        if (c == 'Z') {
            if (sp.n > 0) { sp.closed = true; sink(user, &sp); sp.n = 0; sp.closed = false; }
            cx = sx; cy = sy;
            lastWasCubic = false;
            continue;
        }

        if (c == 'M') {
            if (sp.n > 0) { sink(user, &sp); sp.n = 0; sp.closed = false; }
            float x = read_num(&p), y = read_num(&p);
            if (rel) { x += cx; y += cy; }
            cx = sx = x; cy = sy = y;
            sp_add(&sp, cx, cy);
            // Subsequent implicit pairs after an M are line-tos.
            cmd = rel ? 'l' : 'L';
            lastWasCubic = false;
            continue;
        }

        if (sp.n == 0) sp_add(&sp, cx, cy);

        switch (c) {
            case 'L': {
                float x = read_num(&p), y = read_num(&p);
                if (rel) { x += cx; y += cy; }
                cx = x; cy = y; sp_add(&sp, cx, cy);
                lastWasCubic = false;
                break;
            }
            case 'H': {
                float x = read_num(&p);
                if (rel) x += cx;
                cx = x; sp_add(&sp, cx, cy);
                lastWasCubic = false;
                break;
            }
            case 'V': {
                float y = read_num(&p);
                if (rel) y += cy;
                cy = y; sp_add(&sp, cx, cy);
                lastWasCubic = false;
                break;
            }
            case 'C': {
                float x1 = read_num(&p), y1 = read_num(&p);
                float x2 = read_num(&p), y2 = read_num(&p);
                float x  = read_num(&p), y  = read_num(&p);
                if (rel) { x1 += cx; y1 += cy; x2 += cx; y2 += cy; x += cx; y += cy; }
                flatten_cubic(&sp, cx, cy, x1, y1, x2, y2, x, y, scale);
                lastC2x = x2; lastC2y = y2;
                cx = x; cy = y;
                lastWasCubic = true;
                break;
            }
            case 'S': {
                float x2 = read_num(&p), y2 = read_num(&p);
                float x  = read_num(&p), y  = read_num(&p);
                if (rel) { x2 += cx; y2 += cy; x += cx; y += cy; }
                float x1 = lastWasCubic ? (2 * cx - lastC2x) : cx;
                float y1 = lastWasCubic ? (2 * cy - lastC2y) : cy;
                flatten_cubic(&sp, cx, cy, x1, y1, x2, y2, x, y, scale);
                lastC2x = x2; lastC2y = y2;
                cx = x; cy = y;
                lastWasCubic = true;
                break;
            }
            case 'A': {
                float rx = read_num(&p), ry = read_num(&p);
                float rot = read_num(&p);
                // The flag arguments are single digits and may be
                // written without separators ("0 0 1"/"001").
                skip_sep(&p);
                bool laf = (*p == '1'); if (*p == '0' || *p == '1') p++;
                skip_sep(&p);
                bool sf = (*p == '1'); if (*p == '0' || *p == '1') p++;
                float x = read_num(&p), y = read_num(&p);
                if (rel) { x += cx; y += cy; }
                flatten_arc(&sp, cx, cy, rx, ry, rot, laf, sf, x, y, scale);
                cx = x; cy = y;
                lastWasCubic = false;
                break;
            }
            default:
                // Unsupported command: bail rather than loop forever.
                p++;
                break;
        }
    }
    if (sp.n > 0) sink(user, &sp);
}

// ---------------------------------------------------------------
// Icon data -- transcribed from the blueprint's SVG markup
// ---------------------------------------------------------------

static const Shape SH_WIFI[] = {
    { SH_PATH, "M4.5 9.2a11 11 0 0 1 15 0", {0}, false, 0, CUR },
    { SH_PATH, "M7.3 12.6a7 7 0 0 1 9.4 0", {0}, false, 0, CUR },
    { SH_PATH, "M10.1 16a3 3 0 0 1 3.8 0", {0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 19, 12, 19.01f, 0}, false, 0, CUR },
};
static const Shape SH_CLOCK[] = {
    { SH_CIRCLE, NULL, {12, 12, 9, 0, 0}, false, 0, CUR },
    { SH_PATH, "M12 7v5l3.2 2", {0}, false, 0, CUR },
};
static const Shape SH_LED_STRIP[] = {
    { SH_RECT, NULL, {3, 9.5f, 18, 5, 1}, false, 0, CUR },
    { SH_CIRCLE, NULL, {8, 12, 0.9f, 0, 0}, true, 0, CUR },
    { SH_CIRCLE, NULL, {12, 12, 0.9f, 0, 0}, true, 0, CUR },
    { SH_CIRCLE, NULL, {16, 12, 0.9f, 0, 0}, true, 0, CUR },
};
static const Shape SH_LOOP[] = {
    { SH_POLYLINE, "17 1 21 5 17 9", {0}, false, 0, CUR },
    { SH_PATH, "M3 11V9a4 4 0 0 1 4-4h14", {0}, false, 0, CUR },
    { SH_POLYLINE, "7 23 3 19 7 15", {0}, false, 0, CUR },
    { SH_PATH, "M21 13v2a4 4 0 0 1-4 4H3", {0}, false, 0, CUR },
};
static const Shape SH_CORNER[] = {
    { SH_PATH, "M8 3H5a2 2 0 0 0-2 2v3", {0}, false, 0, CUR },
    { SH_PATH, "M9 15l-6 6", {0}, false, 0, CUR },
    { SH_PATH, "M3 15v6h6", {0}, false, 0, CUR },
};
static const Shape SH_GLOBE[] = {
    { SH_CIRCLE, NULL, {12, 12, 9, 0, 0}, false, 0, CUR },
    { SH_PATH, "M12 3a9 9 0 0 1 0 18", {0}, false, 0, CUR },
};
static const Shape SH_REFRESH[] = {
    { SH_PATH, "M21 12a9 9 0 1 1-2.6-6.35", {0}, false, 0, CUR },
    { SH_POLYLINE, "21 3 21 9 15 9", {0}, false, 0, CUR },
};
static const Shape SH_FRAME[] = {
    { SH_RECT, NULL, {4, 4, 16, 16, 1.5f}, false, 0, CUR },
    { SH_RECT, NULL, {8.5f, 8.5f, 7, 7, 1}, false, 0, CUR },
};
static const Shape SH_MARGIN_TL[] = { { SH_PATH, "M4 9V5a1 1 0 0 1 1-1h4", {0}, false, 0, CUR } };
static const Shape SH_MARGIN_TR[] = { { SH_PATH, "M20 9V5a1 1 0 0 0-1-1h-4", {0}, false, 0, CUR } };
static const Shape SH_MARGIN_BL[] = { { SH_PATH, "M4 15v4a1 1 0 0 0 1 1h4", {0}, false, 0, CUR } };
static const Shape SH_MARGIN_BR[] = { { SH_PATH, "M20 15v4a1 1 0 0 1-1 1h-4", {0}, false, 0, CUR } };
static const Shape SH_LEVELS[] = {
    { SH_PATH, "M4 17h16", {0}, false, 0, CUR },
    { SH_PATH, "M4 12h10", {0}, false, 0, CUR },
    { SH_PATH, "M4 7h6", {0}, false, 0, CUR },
};
static const Shape SH_SUN[] = {
    { SH_CIRCLE, NULL, {12, 12, 4, 0, 0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 2, 12, 4.5f, 0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 19.5f, 12, 22, 0}, false, 0, CUR },
    { SH_LINE, NULL, {2, 12, 4.5f, 12, 0}, false, 0, CUR },
    { SH_LINE, NULL, {19.5f, 12, 22, 12, 0}, false, 0, CUR },
};
static const Shape SH_DROPLET[] = {
    { SH_PATH, "M12 3s6 6.4 6 10.5a6 6 0 1 1-12 0C6 9.4 12 3 12 3Z", {0}, false, 0, CUR },
};
static const Shape SH_GAMMA[] = {
    { SH_CIRCLE, NULL, {12, 12, 8.5f, 0, 0}, false, 0, CUR },
    { SH_PATH, "M12 3.5a8.5 8.5 0 0 1 0 17", {0}, true, 0, CUR },
};
static const Shape SH_MOON[] = {
    { SH_PATH, "M21 12.5A9 9 0 1 1 11.5 3 7 7 0 0 0 21 12.5Z", {0}, false, 0, CUR },
};
static const Shape SH_SUN_FILLED[] = {
    { SH_CIRCLE, NULL, {12, 12, 5, 0, 0}, true, 0, CUR },
    { SH_LINE, NULL, {12, 2, 12, 4.5f, 0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 19.5f, 12, 22, 0}, false, 0, CUR },
    { SH_LINE, NULL, {4.2f, 4.2f, 6, 6, 0}, false, 0, CUR },
    { SH_LINE, NULL, {18, 18, 19.8f, 19.8f, 0}, false, 0, CUR },
    { SH_LINE, NULL, {2, 12, 4.5f, 12, 0}, false, 0, CUR },
    { SH_LINE, NULL, {19.5f, 12, 22, 12, 0}, false, 0, CUR },
};
static const Shape SH_SPLIT[] = {
    { SH_RECT, NULL, {3, 3, 18, 18, 2}, false, 0, CUR },
    { SH_PATH, "M12 3v18", {0}, false, 0, CUR },   // opacity 0.5 applied at draw time
};
static const Shape SH_WAVES[] = {
    { SH_PATH, "M3 8c1.5-1.5 3-1.5 4.5 0s3 1.5 4.5 0 3-1.5 4.5 0 3 1.5 4.5 0", {0}, false, 0, CUR },
    { SH_PATH, "M3 15c1.5-1.5 3-1.5 4.5 0s3 1.5 4.5 0 3-1.5 4.5 0 3 1.5 4.5 0", {0}, false, 0, CUR },
};
static const Shape SH_SLIDERS_RGB[] = {
    { SH_LINE, NULL, {6, 4, 6, 20, 0}, false, 0, CUR },
    { SH_CIRCLE, NULL, {6, 9, 2, 0, 0}, true, 0, CHAN_RED },
    { SH_LINE, NULL, {12, 4, 12, 20, 0}, false, 0, CUR },
    { SH_CIRCLE, NULL, {12, 15, 2, 0, 0}, true, 0, CHAN_GREEN },
    { SH_LINE, NULL, {18, 4, 18, 20, 0}, false, 0, CUR },
    { SH_CIRCLE, NULL, {18, 7, 2, 0, 0}, true, 0, CHAN_BLUE },
};
static const Shape SH_SLIDER_R[] = {
    { SH_LINE, NULL, {6, 4, 6, 20, 0}, false, 0, CUR },
    { SH_CIRCLE, NULL, {6, 9, 2, 0, 0}, true, 0, CUR },
    { SH_LINE, NULL, {12, 4, 12, 20, 0}, false, 0, CUR },
    { SH_LINE, NULL, {18, 4, 18, 20, 0}, false, 0, CUR },
};
static const Shape SH_SLIDER_G[] = {
    { SH_LINE, NULL, {6, 4, 6, 20, 0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 4, 12, 20, 0}, false, 0, CUR },
    { SH_CIRCLE, NULL, {12, 15, 2, 0, 0}, true, 0, CUR },
    { SH_LINE, NULL, {18, 4, 18, 20, 0}, false, 0, CUR },
};
static const Shape SH_SLIDER_B[] = {
    { SH_LINE, NULL, {6, 4, 6, 20, 0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 4, 12, 20, 0}, false, 0, CUR },
    { SH_LINE, NULL, {18, 4, 18, 20, 0}, false, 0, CUR },
    { SH_CIRCLE, NULL, {18, 7, 2, 0, 0}, true, 0, CUR },
};
static const Shape SH_RING[] = {
    { SH_CIRCLE, NULL, {12, 12, 8.5f, 0, 0}, false, 0, CUR },
};
static const Shape SH_HELP[] = {
    { SH_CIRCLE, NULL, {12, 12, 9.25f, 0, 0}, false, 0, CUR },
    { SH_PATH, "M9.2 9.3a2.8 2.8 0 1 1 4.4 2.6c-.7.5-1.1 1-1.1 2.1", {0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 17, 12, 17.01f, 0}, false, 0, CUR },
};
static const Shape SH_QR[] = {
    { SH_RECT, NULL, {3, 3, 5, 5, 1}, false, 0, CUR },
    { SH_RECT, NULL, {16, 3, 5, 5, 1}, false, 0, CUR },
    { SH_RECT, NULL, {3, 16, 5, 5, 1}, false, 0, CUR },
    { SH_PATH, "M21 16h-3a2 2 0 0 0-2 2v3", {0}, false, 0, CUR },
    { SH_LINE, NULL, {21, 21, 21, 21.01f, 0}, false, 0, CUR },
    { SH_PATH, "M12 7v3a2 2 0 0 1-2 2H7", {0}, false, 0, CUR },
    { SH_LINE, NULL, {3, 12, 3.01f, 12, 0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 3, 12.01f, 3, 0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 16, 12, 16.01f, 0}, false, 0, CUR },
    { SH_LINE, NULL, {16, 12, 17, 12, 0}, false, 0, CUR },
    { SH_LINE, NULL, {21, 12, 21, 12.01f, 0}, false, 0, CUR },
    { SH_LINE, NULL, {12, 21, 12, 20, 0}, false, 0, CUR },
};
static const Shape SH_CALENDAR[] = {
    { SH_RECT, NULL, {3, 4, 18, 16, 1.5f}, false, 0, CUR },
    { SH_PATH, "M3 9h18", {0}, false, 0, CUR },
    { SH_PATH, "M8 4v5", {0}, false, 0, CUR },
};
static const Shape SH_ACTIVITY[] = {
    { SH_PATH, "M4 17h4l2-9 4 14 2-9h4", {0}, false, 0, CUR },
};
static const Shape SH_DOWNLOAD[] = {
    { SH_PATH, "M12 3v12", {0}, false, 0, CUR },
    { SH_POLYLINE, "7 10 12 15 17 10", {0}, false, 0, CUR },
    { SH_PATH, "M4 19h16", {0}, false, 0, CUR },
};
static const Shape SH_STOP[] = {
    { SH_RECT, NULL, {6, 6, 12, 12, 1.5f}, false, 0, CUR },
};
static const Shape SH_SAVE[] = {
    { SH_PATH, "M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2Z", {0}, false, 0, CUR },
    { SH_PATH, "M17 21v-8H7v8", {0}, false, 0, CUR },
    { SH_PATH, "M7 3v5h8", {0}, false, 0, CUR },
};
static const Shape SH_WARNING[] = {
    { SH_PATH, "M12 9v4", {0}, false, 0, CUR },
    { SH_PATH, "M12 17h.01", {0}, false, 0, CUR },
    { SH_PATH, "M10.3 3.9 2.7 17.3a1.8 1.8 0 0 0 1.6 2.7h15.4a1.8 1.8 0 0 0 1.6-2.7L13.7 3.9a1.8 1.8 0 0 0-3.4 0Z", {0}, false, 0, CUR },
};
static const Shape SH_CHECK[] = {
    { SH_POLYLINE, "4 12.5 9.5 18 20 6", {0}, false, 0, CUR },
};

#define DEF(arr, sw, cap) { arr, (int)(sizeof(arr) / sizeof((arr)[0])), sw, cap }

static const IconDef kIcons[ICON_COUNT] = {
    [ICON_NONE]        = { NULL, 0, 0, false },
    [ICON_WIFI]        = DEF(SH_WIFI, 1.8f, true),
    [ICON_CLOCK]       = DEF(SH_CLOCK, 1.8f, false),
    [ICON_LED_STRIP]   = DEF(SH_LED_STRIP, 1.8f, true),
    [ICON_LOOP]        = DEF(SH_LOOP, 1.8f, true),
    [ICON_CORNER]      = DEF(SH_CORNER, 1.8f, true),
    [ICON_GLOBE]       = DEF(SH_GLOBE, 1.8f, true),
    [ICON_REFRESH]     = DEF(SH_REFRESH, 1.8f, true),
    [ICON_FRAME]       = DEF(SH_FRAME, 1.8f, false),
    [ICON_MARGIN_TL]   = DEF(SH_MARGIN_TL, 1.8f, true),
    [ICON_MARGIN_TR]   = DEF(SH_MARGIN_TR, 1.8f, true),
    [ICON_MARGIN_BL]   = DEF(SH_MARGIN_BL, 1.8f, true),
    [ICON_MARGIN_BR]   = DEF(SH_MARGIN_BR, 1.8f, true),
    [ICON_LEVELS]      = DEF(SH_LEVELS, 1.6f, true),
    [ICON_SUN]         = DEF(SH_SUN, 1.8f, true),
    [ICON_DROPLET]     = DEF(SH_DROPLET, 1.8f, false),
    [ICON_GAMMA]       = DEF(SH_GAMMA, 1.8f, false),
    [ICON_MOON]        = DEF(SH_MOON, 1.8f, false),
    [ICON_SUN_FILLED]  = DEF(SH_SUN_FILLED, 1.8f, false),
    [ICON_SPLIT]       = DEF(SH_SPLIT, 1.8f, false),
    [ICON_WAVES]       = DEF(SH_WAVES, 1.8f, true),
    [ICON_SLIDERS_RGB] = DEF(SH_SLIDERS_RGB, 1.8f, true),
    [ICON_SLIDER_R]    = DEF(SH_SLIDER_R, 1.8f, true),
    [ICON_SLIDER_G]    = DEF(SH_SLIDER_G, 1.8f, true),
    [ICON_SLIDER_B]    = DEF(SH_SLIDER_B, 1.8f, true),
    [ICON_RING]        = DEF(SH_RING, 1.8f, false),
    [ICON_HELP]        = DEF(SH_HELP, 1.8f, true),
    [ICON_QR]          = DEF(SH_QR, 2.0f, true),
    [ICON_CALENDAR]    = DEF(SH_CALENDAR, 1.8f, true),
    [ICON_ACTIVITY]    = DEF(SH_ACTIVITY, 2.0f, true),
    [ICON_DOWNLOAD]    = DEF(SH_DOWNLOAD, 2.0f, true),
    [ICON_STOP]        = DEF(SH_STOP, 2.0f, true),
    [ICON_SAVE]        = DEF(SH_SAVE, 1.8f, true),
    [ICON_WARNING]     = DEF(SH_WARNING, 1.8f, true),
    [ICON_CHECK]       = DEF(SH_CHECK, 2.2f, true),
};

// ---------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------

typedef struct {
    UiShape *shape;
    float scale, ox, oy;
    float strokeWidth;
    bool fill;
    bool roundCap;
} DrawCtx;

static void emit_subpath(void *user, const SubPath *sp)
{
    DrawCtx *ctx = (DrawCtx *)user;
    if (sp->n < 2) return;

    float xy[MAX_SUBPATH_PTS * 2];
    for (int i = 0; i < sp->n; i++) {
        xy[i * 2]     = ctx->ox + sp->pts[i * 2] * ctx->scale;
        xy[i * 2 + 1] = ctx->oy + sp->pts[i * 2 + 1] * ctx->scale;
    }

    if (ctx->fill) {
        if (sp->n >= 3) ui_shape_polygon(ctx->shape, xy, sp->n);
    } else {
        ui_shape_stroke_polyline(ctx->shape, xy, sp->n, ctx->strokeWidth, sp->closed, ctx->roundCap);
    }
}

static void stroke_points(DrawCtx *ctx, const float *vbXY, int n, bool closed)
{
    float xy[64];
    if (n > 32) n = 32;
    for (int i = 0; i < n; i++) {
        xy[i * 2]     = ctx->ox + vbXY[i * 2] * ctx->scale;
        xy[i * 2 + 1] = ctx->oy + vbXY[i * 2 + 1] * ctx->scale;
    }
    ui_shape_stroke_polyline(ctx->shape, xy, n, ctx->strokeWidth, closed, ctx->roundCap);
}

void ui_icon_draw(UiCanvas *c, UiIconId id, float x, float y, float size, UiColor color)
{
    if (id <= ICON_NONE || id >= ICON_COUNT) return;
    const IconDef *def = &kIcons[id];
    if (!def->shapes) return;

    float scale = size / 24.0f;

    for (int i = 0; i < def->count; i++) {
        const Shape *sh = &def->shapes[i];
        UiColor col = (sh->color.a == 0) ? color : sh->color;
        // .icon inherits its parent's alpha; the one place the
        // blueprint sets opacity on a sub-shape is the contrast icon's
        // centre rule.
        if (id == ICON_SPLIT && i == 1) col = ui_fade(col, 0.5f);

        float sw = (sh->strokeWidth > 0 ? sh->strokeWidth : def->strokeWidth) * scale;

        DrawCtx ctx;
        ctx.shape = ui_shape_begin(c);
        ctx.scale = scale; ctx.ox = x; ctx.oy = y;
        ctx.strokeWidth = sw;
        ctx.fill = sh->fill;
        ctx.roundCap = def->roundCap;

        switch (sh->kind) {
            case SH_PATH:
                parse_path(sh->d, scale, emit_subpath, &ctx);
                break;

            case SH_POLYLINE: {
                float pts[64];
                int n = 0;
                const char *p = sh->d;
                while (*p && n < 32) {
                    skip_sep(&p);
                    if (!*p) break;
                    pts[n * 2] = read_num(&p);
                    pts[n * 2 + 1] = read_num(&p);
                    n++;
                }
                stroke_points(&ctx, pts, n, false);
                break;
            }

            case SH_LINE: {
                float pts[4] = { sh->p[0], sh->p[1], sh->p[2], sh->p[3] };
                // A zero-length "line" is how the blueprint draws a dot
                // (e.g. the wifi icon's base). With round caps that is a
                // disc of the stroke width; with butt caps it would
                // vanish, so force a cap here.
                bool degenerate = (fabsf(pts[0] - pts[2]) < 0.05f && fabsf(pts[1] - pts[3]) < 0.05f);
                bool saveCap = ctx.roundCap;
                if (degenerate) ctx.roundCap = true;
                stroke_points(&ctx, pts, 2, false);
                ctx.roundCap = saveCap;
                break;
            }

            case SH_RECT: {
                float rx = sh->p[0], ry = sh->p[1], rw = sh->p[2], rh = sh->p[3], rr = sh->p[4];
                // Built as a path so rounded corners come out right,
                // and so stroke and fill share one code route.
                char buf[512];
                if (rr > 0.0f) {
                    snprintf(buf, sizeof(buf),
                             "M%g %g H%g A%g %g 0 0 1 %g %g V%g A%g %g 0 0 1 %g %g H%g A%g %g 0 0 1 %g %g V%g A%g %g 0 0 1 %g %g Z",
                             rx + rr, ry, rx + rw - rr,
                             rr, rr, rx + rw, ry + rr,
                             ry + rh - rr,
                             rr, rr, rx + rw - rr, ry + rh,
                             rx + rr,
                             rr, rr, rx, ry + rh - rr,
                             ry + rr,
                             rr, rr, rx + rr, ry);
                } else {
                    snprintf(buf, sizeof(buf), "M%g %g H%g V%g H%g Z", rx, ry, rx + rw, ry + rh, rx);
                }
                parse_path(buf, scale, emit_subpath, &ctx);
                break;
            }

            case SH_CIRCLE: {
                float ccx = x + sh->p[0] * scale;
                float ccy = y + sh->p[1] * scale;
                float r = sh->p[2] * scale;
                ui_shape_end(ctx.shape);
                if (sh->fill) ui_fill_circle(c, ccx, ccy, r, col);
                else ui_stroke_circle(c, ccx, ccy, r, sw, col);
                continue;
            }
        }

        ui_shape_fill(ctx.shape, col);
    }
}
