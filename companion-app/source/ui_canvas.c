// ui_canvas.c -- see ui_canvas.h for why this module exists.
//
// Rasterisation strategy: a scanline polygon filler with 5 vertical
// sub-samples per pixel row and exact analytic horizontal coverage
// per span. That combination is cheap (no 2D supersample buffer) but
// still resolves the two things this design leans on hardest: 1px
// hairline borders at fractional alpha, and 1.4-1.7px icon strokes at
// arbitrary angles.

#include "ui_canvas.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SUBSAMPLES 5
#define MAX_CROSSINGS 128

// ---------------------------------------------------------------
// Shape / coverage workspace
// ---------------------------------------------------------------

typedef struct {
    int start;      // index into pts (in vertices, not floats)
    int count;
    bool newPath;   // true if this contour opens a new path
} UiContour;

struct UiShape {
    UiCanvas *canvas;

    float *pts;         // flat x,y pairs
    int ptCount, ptCap;

    UiContour *contours;
    int contourCount, contourCap;

    uint8_t *cov;       // canvas-sized coverage, 0..255
    int bx0, by0, bx1, by1;  // dirty bbox of cov (exclusive upper bounds)
};

static void shape_reserve_pts(UiShape *s, int extra)
{
    if (s->ptCount + extra <= s->ptCap) return;
    int cap = s->ptCap ? s->ptCap : 256;
    while (cap < s->ptCount + extra) cap *= 2;
    s->pts = (float *)realloc(s->pts, (size_t)cap * 2 * sizeof(float));
    s->ptCap = cap;
}

static void shape_reserve_contours(UiShape *s)
{
    if (s->contourCount + 1 <= s->contourCap) return;
    int cap = s->contourCap ? s->contourCap : 32;
    while (cap < s->contourCount + 1) cap *= 2;
    s->contours = (UiContour *)realloc(s->contours, (size_t)cap * sizeof(UiContour));
    s->contourCap = cap;
}

static void shape_add_contour(UiShape *s, const float *xy, int n, bool newPath)
{
    if (n < 3) return;
    shape_reserve_pts(s, n);
    shape_reserve_contours(s);
    memcpy(&s->pts[s->ptCount * 2], xy, (size_t)n * 2 * sizeof(float));
    UiContour *c = &s->contours[s->contourCount++];
    c->start = s->ptCount;
    c->count = n;
    c->newPath = newPath;
    s->ptCount += n;
}

UiShape *ui_shape_begin(UiCanvas *c)
{
    UiShape *s = c->shape;
    s->ptCount = 0;
    s->contourCount = 0;
    return s;
}

void ui_shape_polygon(UiShape *s, const float *xy, int n) { shape_add_contour(s, xy, n, true); }
void ui_shape_contour(UiShape *s, const float *xy, int n) { shape_add_contour(s, xy, n, s->contourCount == 0); }

// Rasterises one path (a run of contours) into the coverage buffer,
// combining with max() against whatever is already there.
static void rasterise_path(UiShape *s, int firstContour, int contourCount)
{
    UiCanvas *cv = s->canvas;

    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    for (int ci = firstContour; ci < firstContour + contourCount; ci++) {
        const UiContour *c = &s->contours[ci];
        for (int i = 0; i < c->count; i++) {
            float x = s->pts[(c->start + i) * 2];
            float y = s->pts[(c->start + i) * 2 + 1];
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }
    if (maxX <= minX && maxY <= minY) return;

    int y0 = (int)floorf(minY), y1 = (int)ceilf(maxY);
    int x0 = (int)floorf(minX), x1 = (int)ceilf(maxX);
    if (y0 < cv->clipY0) y0 = cv->clipY0;
    if (y1 > cv->clipY1) y1 = cv->clipY1;
    if (x0 < cv->clipX0) x0 = cv->clipX0;
    if (x1 > cv->clipX1) x1 = cv->clipX1;
    if (y0 >= y1 || x0 >= x1) return;

    // Grow the dirty bbox so ui_shape_fill knows what to composite and
    // what to clear afterwards.
    if (x0 < s->bx0) s->bx0 = x0;
    if (y0 < s->by0) s->by0 = y0;
    if (x1 > s->bx1) s->bx1 = x1;
    if (y1 > s->by1) s->by1 = y1;

    int rowW = x1 - x0;
    float *acc = (float *)calloc((size_t)rowW, sizeof(float));
    if (!acc) return;

    float xs[MAX_CROSSINGS];
    int   dirs[MAX_CROSSINGS];

    const float subWeight = 1.0f / (float)SUBSAMPLES;

    for (int py = y0; py < y1; py++) {
        memset(acc, 0, (size_t)rowW * sizeof(float));
        bool any = false;

        for (int sub = 0; sub < SUBSAMPLES; sub++) {
            float sy = (float)py + ((float)sub + 0.5f) * subWeight;
            int nx = 0;

            for (int ci = firstContour; ci < firstContour + contourCount; ci++) {
                const UiContour *c = &s->contours[ci];
                for (int i = 0; i < c->count; i++) {
                    int a = c->start + i;
                    int b = c->start + ((i + 1) % c->count);
                    float ax = s->pts[a * 2], ay = s->pts[a * 2 + 1];
                    float bx = s->pts[b * 2], by = s->pts[b * 2 + 1];
                    if (ay == by) continue;
                    // Half-open rule on y so a vertex exactly on the
                    // sample line is counted once, not twice.
                    if ((sy >= ay && sy < by) || (sy >= by && sy < ay)) {
                        if (nx < MAX_CROSSINGS) {
                            float t = (sy - ay) / (by - ay);
                            xs[nx] = ax + t * (bx - ax);
                            dirs[nx] = (by > ay) ? 1 : -1;
                            nx++;
                        }
                    }
                }
            }
            if (nx < 2) continue;

            // Insertion sort -- nx is tiny for this UI.
            for (int i = 1; i < nx; i++) {
                float kx = xs[i]; int kd = dirs[i];
                int j = i - 1;
                while (j >= 0 && xs[j] > kx) { xs[j + 1] = xs[j]; dirs[j + 1] = dirs[j]; j--; }
                xs[j + 1] = kx; dirs[j + 1] = kd;
            }

            // Nonzero winding (SVG's default), so reversed inner
            // contours punch holes.
            int winding = 0;
            for (int i = 0; i + 1 < nx; i++) {
                winding += dirs[i];
                if (winding == 0) continue;

                float spanA = xs[i], spanB = xs[i + 1];
                if (spanB <= (float)x0 || spanA >= (float)x1) continue;
                if (spanA < (float)x0) spanA = (float)x0;
                if (spanB > (float)x1) spanB = (float)x1;
                if (spanB <= spanA) continue;
                any = true;

                int ia = (int)floorf(spanA), ib = (int)ceilf(spanB) - 1;
                if (ia == ib) {
                    acc[ia - x0] += (spanB - spanA) * subWeight;
                } else {
                    acc[ia - x0] += ((float)(ia + 1) - spanA) * subWeight;
                    for (int px = ia + 1; px < ib; px++) acc[px - x0] += subWeight;
                    acc[ib - x0] += (spanB - (float)ib) * subWeight;
                }
            }
        }

        if (!any) continue;
        uint8_t *covRow = &s->cov[(size_t)py * cv->w];
        for (int i = 0; i < rowW; i++) {
            float v = acc[i];
            if (v <= 0.0f) continue;
            if (v > 1.0f) v = 1.0f;
            uint8_t q = (uint8_t)(v * 255.0f + 0.5f);
            if (q > covRow[x0 + i]) covRow[x0 + i] = q;
        }
    }

    free(acc);
}

void ui_shape_fill(UiShape *s, UiColor color)
{
    UiCanvas *cv = s->canvas;

    s->bx0 = cv->w; s->by0 = cv->h; s->bx1 = 0; s->by1 = 0;

    int i = 0;
    while (i < s->contourCount) {
        int first = i;
        int n = 1;
        i++;
        while (i < s->contourCount && !s->contours[i].newPath) { n++; i++; }
        rasterise_path(s, first, n);
    }

    if (s->bx1 > s->bx0 && s->by1 > s->by0 && color.a > 0) {
        for (int y = s->by0; y < s->by1; y++) {
            uint8_t *covRow = &s->cov[(size_t)y * cv->w];
            for (int x = s->bx0; x < s->bx1; x++) {
                uint8_t q = covRow[x];
                if (q) ui_blend_pixel(cv, x, y, color, q / 255.0f);
            }
        }
    }
    // Clear only what was touched.
    for (int y = s->by0; y < s->by1; y++) {
        if (s->bx1 > s->bx0) memset(&s->cov[(size_t)y * cv->w + s->bx0], 0, (size_t)(s->bx1 - s->bx0));
    }

    s->ptCount = 0;
    s->contourCount = 0;
}

void ui_shape_end(UiShape *s) { s->ptCount = 0; s->contourCount = 0; }

// ---------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------

UiCanvas *ui_canvas_create(int w, int h)
{
    UiCanvas *c = (UiCanvas *)calloc(1, sizeof(UiCanvas));
    if (!c) return NULL;
    c->w = w; c->h = h;
    c->px = (uint32_t *)calloc((size_t)w * h, sizeof(uint32_t));
    UiShape *s = (UiShape *)calloc(1, sizeof(UiShape));
    if (!c->px || !s) { free(c->px); free(s); free(c); return NULL; }
    s->canvas = c;
    s->cov = (uint8_t *)calloc((size_t)w * h, 1);
    if (!s->cov) { free(s); free(c->px); free(c); return NULL; }
    c->shape = s;
    ui_clip_reset(c);
    return c;
}

void ui_canvas_destroy(UiCanvas *c)
{
    if (!c) return;
    if (c->shape) {
        free(c->shape->cov);
        free(c->shape->pts);
        free(c->shape->contours);
        free(c->shape);
    }
    free(c->px);
    free(c->sceneCache);
    free(c);
}

void ui_canvas_clear(UiCanvas *c, UiColor color)
{
    uint32_t v = 0xFF000000u | ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | (uint32_t)color.b;
    size_t n = (size_t)c->w * c->h;
    for (size_t i = 0; i < n; i++) c->px[i] = v;
}

void ui_clip_reset(UiCanvas *c)
{
    c->clipX0 = 0; c->clipY0 = 0; c->clipX1 = c->w; c->clipY1 = c->h;
    c->clipDepth = 0;
}

void ui_clip_push(UiCanvas *c, UiRect r)
{
    if (c->clipDepth >= UI_CLIP_STACK_MAX) return;
    c->clipStack[c->clipDepth][0] = c->clipX0;
    c->clipStack[c->clipDepth][1] = c->clipY0;
    c->clipStack[c->clipDepth][2] = c->clipX1;
    c->clipStack[c->clipDepth][3] = c->clipY1;
    c->clipDepth++;

    int nx0 = (int)floorf(r.x), ny0 = (int)floorf(r.y);
    int nx1 = (int)ceilf(r.x + r.w), ny1 = (int)ceilf(r.y + r.h);
    if (nx0 > c->clipX0) c->clipX0 = nx0;
    if (ny0 > c->clipY0) c->clipY0 = ny0;
    if (nx1 < c->clipX1) c->clipX1 = nx1;
    if (ny1 < c->clipY1) c->clipY1 = ny1;
    if (c->clipX1 < c->clipX0) c->clipX1 = c->clipX0;
    if (c->clipY1 < c->clipY0) c->clipY1 = c->clipY0;
}

void ui_clip_pop(UiCanvas *c)
{
    if (c->clipDepth <= 0) return;
    c->clipDepth--;
    c->clipX0 = c->clipStack[c->clipDepth][0];
    c->clipY0 = c->clipStack[c->clipDepth][1];
    c->clipX1 = c->clipStack[c->clipDepth][2];
    c->clipY1 = c->clipStack[c->clipDepth][3];
}

void ui_blend_pixel(UiCanvas *c, int x, int y, UiColor color, float coverage)
{
    if (x < c->clipX0 || x >= c->clipX1 || y < c->clipY0 || y >= c->clipY1) return;
    float a = (color.a / 255.0f) * coverage;
    if (a <= 0.0f) return;
    if (a > 1.0f) a = 1.0f;

    uint32_t *p = &c->px[(size_t)y * c->w + x];
    uint32_t d = *p;
    float dr = (float)((d >> 16) & 0xFF);
    float dg = (float)((d >> 8) & 0xFF);
    float db = (float)(d & 0xFF);

    float nr = (float)color.r * a + dr * (1.0f - a);
    float ng = (float)color.g * a + dg * (1.0f - a);
    float nb = (float)color.b * a + db * (1.0f - a);

    *p = 0xFF000000u
       | ((uint32_t)(nr + 0.5f) << 16)
       | ((uint32_t)(ng + 0.5f) << 8)
       | (uint32_t)(nb + 0.5f);
}

// ---------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------

// Number of segments used to approximate a quarter turn -- scaled by
// radius so small 4px control corners stay cheap while a 96px CTA
// corner still looks circular.
static int arc_segments(float radius)
{
    int n = (int)(radius * 1.2f) + 3;
    if (n < 3) n = 3;
    if (n > 40) n = 40;
    return n;
}

// Appends an arc from angle a0 to a1 (radians, screen space: +y down)
// to a vertex buffer.
static int append_arc(float *out, int n, float cx, float cy, float r, float a0, float a1)
{
    int segs = arc_segments(r);
    for (int i = 0; i <= segs; i++) {
        float t = a0 + (a1 - a0) * ((float)i / (float)segs);
        out[n * 2] = cx + cosf(t) * r;
        out[n * 2 + 1] = cy + sinf(t) * r;
        n++;
    }
    return n;
}

// Builds a rounded-rect contour. `reverse` emits it wound the other
// way, which is what turns an inner contour into a hole.
static int build_round_rect(float *out, UiRect r, UiRadius k, bool reverse)
{
    float maxR = (r.w < r.h ? r.w : r.h) * 0.5f;
    if (k.tl > maxR) k.tl = maxR;
    if (k.tr > maxR) k.tr = maxR;
    if (k.br > maxR) k.br = maxR;
    if (k.bl > maxR) k.bl = maxR;
    if (k.tl < 0) k.tl = 0;
    if (k.tr < 0) k.tr = 0;
    if (k.br < 0) k.br = 0;
    if (k.bl < 0) k.bl = 0;

    float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
    int n = 0;

    if (k.tl > 0) n = append_arc(out, n, x0 + k.tl, y0 + k.tl, k.tl, (float)M_PI, 1.5f * (float)M_PI);
    else { out[n * 2] = x0; out[n * 2 + 1] = y0; n++; }

    if (k.tr > 0) n = append_arc(out, n, x1 - k.tr, y0 + k.tr, k.tr, 1.5f * (float)M_PI, 2.0f * (float)M_PI);
    else { out[n * 2] = x1; out[n * 2 + 1] = y0; n++; }

    if (k.br > 0) n = append_arc(out, n, x1 - k.br, y1 - k.br, k.br, 0.0f, 0.5f * (float)M_PI);
    else { out[n * 2] = x1; out[n * 2 + 1] = y1; n++; }

    if (k.bl > 0) n = append_arc(out, n, x0 + k.bl, y1 - k.bl, k.bl, 0.5f * (float)M_PI, (float)M_PI);
    else { out[n * 2] = x0; out[n * 2 + 1] = y1; n++; }

    if (reverse) {
        for (int i = 0; i < n / 2; i++) {
            float tx = out[i * 2], ty = out[i * 2 + 1];
            out[i * 2] = out[(n - 1 - i) * 2];
            out[i * 2 + 1] = out[(n - 1 - i) * 2 + 1];
            out[(n - 1 - i) * 2] = tx;
            out[(n - 1 - i) * 2 + 1] = ty;
        }
    }
    return n;
}

#define ROUND_RECT_MAX_PTS 256

// ---------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------

void ui_fill_rect(UiCanvas *c, UiRect r, UiColor color)
{
    ui_fill_round_rect(c, r, ui_radius_all(0.0f), color);
}

void ui_fill_round_rect(UiCanvas *c, UiRect r, UiRadius k, UiColor color)
{
    if (r.w <= 0.0f || r.h <= 0.0f) return;
    float pts[ROUND_RECT_MAX_PTS * 2];
    int n = build_round_rect(pts, r, k, false);
    UiShape *s = ui_shape_begin(c);
    ui_shape_polygon(s, pts, n);
    ui_shape_fill(s, color);
}

void ui_stroke_round_rect(UiCanvas *c, UiRect r, UiRadius k, float width, UiColor color)
{
    if (r.w <= 0.0f || r.h <= 0.0f || width <= 0.0f) return;

    UiRect inner = ui_rect(r.x + width, r.y + width, r.w - 2 * width, r.h - 2 * width);
    if (inner.w <= 0.0f || inner.h <= 0.0f) { ui_fill_round_rect(c, r, k, color); return; }

    // CSS shrinks the inner radius by the border width, which is what
    // keeps a 1px border concentric with an 8px corner.
    UiRadius ik = ui_radius4(k.tl - width, k.tr - width, k.br - width, k.bl - width);
    if (ik.tl < 0) ik.tl = 0;
    if (ik.tr < 0) ik.tr = 0;
    if (ik.br < 0) ik.br = 0;
    if (ik.bl < 0) ik.bl = 0;

    float outer[ROUND_RECT_MAX_PTS * 2], in[ROUND_RECT_MAX_PTS * 2];
    int no = build_round_rect(outer, r, k, false);
    int ni = build_round_rect(in, inner, ik, true);

    UiShape *s = ui_shape_begin(c);
    ui_shape_polygon(s, outer, no);
    ui_shape_contour(s, in, ni);
    ui_shape_fill(s, color);
}

static int build_circle(float *out, float cx, float cy, float r, bool reverse)
{
    int segs = arc_segments(r) * 4;
    if (segs < 12) segs = 12;
    if (segs > 160) segs = 160;
    for (int i = 0; i < segs; i++) {
        float t = 2.0f * (float)M_PI * ((float)i / (float)segs);
        int idx = reverse ? (segs - 1 - i) : i;
        out[idx * 2] = cx + cosf(t) * r;
        out[idx * 2 + 1] = cy + sinf(t) * r;
    }
    return segs;
}

void ui_fill_circle(UiCanvas *c, float cx, float cy, float r, UiColor color)
{
    if (r <= 0.0f) return;
    float pts[340];
    int n = build_circle(pts, cx, cy, r, false);
    UiShape *s = ui_shape_begin(c);
    ui_shape_polygon(s, pts, n);
    ui_shape_fill(s, color);
}

void ui_stroke_circle(UiCanvas *c, float cx, float cy, float r, float width, UiColor color)
{
    if (r <= 0.0f || width <= 0.0f) return;
    float ro = r + width * 0.5f, ri = r - width * 0.5f;
    if (ri <= 0.0f) { ui_fill_circle(c, cx, cy, ro, color); return; }
    float outer[340], inner[340];
    int no = build_circle(outer, cx, cy, ro, false);
    int ni = build_circle(inner, cx, cy, ri, true);
    UiShape *s = ui_shape_begin(c);
    ui_shape_polygon(s, outer, no);
    ui_shape_contour(s, inner, ni);
    ui_shape_fill(s, color);
}

// Stroke outline construction. Each segment becomes a quad and each
// interior joint / round cap becomes a disc; the shared coverage
// buffer's max() combine is what keeps the seams invisible.
void ui_shape_stroke_polyline(UiShape *s, const float *xy, int n, float width, bool closed, bool roundCaps)
{
    if (n < 2 || width <= 0.0f) return;
    float hw = width * 0.5f;

    int segCount = closed ? n : n - 1;
    for (int i = 0; i < segCount; i++) {
        float ax = xy[i * 2], ay = xy[i * 2 + 1];
        int j = (i + 1) % n;
        float bx = xy[j * 2], by = xy[j * 2 + 1];
        float dx = bx - ax, dy = by - ay;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-6f) continue;
        float nx = -dy / len * hw, ny = dx / len * hw;
        float quad[8] = {
            ax + nx, ay + ny,
            bx + nx, by + ny,
            bx - nx, by - ny,
            ax - nx, ay - ny
        };
        ui_shape_polygon(s, quad, 4);
    }

    // Joints (and caps, when round) as discs.
    float disc[340];
    int first = closed ? 0 : 1;
    int last  = closed ? n : n - 1;
    for (int i = first; i < last; i++) {
        int nd = build_circle(disc, xy[i * 2], xy[i * 2 + 1], hw, false);
        ui_shape_polygon(s, disc, nd);
    }
    if (!closed && roundCaps) {
        int nd = build_circle(disc, xy[0], xy[1], hw, false);
        ui_shape_polygon(s, disc, nd);
        nd = build_circle(disc, xy[(n - 1) * 2], xy[(n - 1) * 2 + 1], hw, false);
        ui_shape_polygon(s, disc, nd);
    }
}

void ui_stroke_line(UiCanvas *c, float x0, float y0, float x1, float y1, float width, UiColor color, bool roundCaps)
{
    float pts[4] = { x0, y0, x1, y1 };
    UiShape *s = ui_shape_begin(c);
    ui_shape_stroke_polyline(s, pts, 2, width, false, roundCaps);
    ui_shape_fill(s, color);
}

void ui_dashed_hline(UiCanvas *c, float x, float y, float w, float dash, float gap, float thickness, UiColor color)
{
    UiShape *s = ui_shape_begin(c);
    for (float cx = x; cx < x + w; cx += dash + gap) {
        float seg = dash;
        if (cx + seg > x + w) seg = x + w - cx;
        if (seg <= 0.0f) break;
        float quad[8] = {
            cx, y,
            cx + seg, y,
            cx + seg, y + thickness,
            cx, y + thickness
        };
        ui_shape_polygon(s, quad, 4);
    }
    ui_shape_fill(s, color);
}

// ---------------------------------------------------------------
// .scene background
// ---------------------------------------------------------------
//
// style.css:
//   background:
//     linear-gradient(var(--bg-grid) 1px, transparent 1px) 0 0/80px 80px,
//     linear-gradient(90deg, var(--bg-grid) 1px, transparent 1px) 0 0/80px 80px,
//     radial-gradient(1200px 700px at 50% -10%, rgba(255,180,84,0.05), transparent 60%),
//     #0b0c0e;
// Layers paint back-to-front in reverse source order, so: flat base,
// then the warm glow, then the grid lines on top.
void ui_draw_scene(UiCanvas *c)
{
    if (c->sceneCache) {
        memcpy(c->px, c->sceneCache, (size_t)c->w * c->h * sizeof(uint32_t));
        return;
    }

    ui_canvas_clear(c, ui_rgb(0x0b, 0x0c, 0x0e));

    // Radial glow. The ending shape is an ellipse with radii
    // 1200x700 centred at (50%, -10%) of the canvas; the colour stop
    // reaches full transparency at 60% of that radius, so nothing
    // outside t = 0.6 needs touching.
    {
        float cx = c->w * 0.5f;
        float cy = c->h * -0.10f;
        float rx = 1200.0f, ry = 700.0f;
        float cutoff = 0.60f;

        int x0 = (int)floorf(cx - rx * cutoff), x1 = (int)ceilf(cx + rx * cutoff);
        int y0 = (int)floorf(cy - ry * cutoff), y1 = (int)ceilf(cy + ry * cutoff);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > c->w) x1 = c->w;
        if (y1 > c->h) y1 = c->h;

        for (int y = y0; y < y1; y++) {
            float dy = ((float)y + 0.5f - cy) / ry;
            for (int x = x0; x < x1; x++) {
                float dx = ((float)x + 0.5f - cx) / rx;
                float t = sqrtf(dx * dx + dy * dy);
                if (t >= cutoff) continue;
                float a = 0.05f * (1.0f - t / cutoff);
                ui_blend_pixel(c, x, y, ui_rgb(255, 180, 84), a);
            }
        }
    }

    // 80px measurement grid, 1px lines at rgba(255,255,255,0.035).
    {
        UiColor line = ui_rgba(255, 255, 255, 0.035f);
        for (int y = 0; y < c->h; y += 80)
            for (int x = 0; x < c->w; x++) ui_blend_pixel(c, x, y, line, 1.0f);
        for (int x = 0; x < c->w; x += 80)
            for (int y = 0; y < c->h; y++) ui_blend_pixel(c, x, y, line, 1.0f);
    }

    // Cache what was just built. Built once, valid for the canvas's
    // entire lifetime -- ui_draw_scene() takes no arguments beyond the
    // canvas itself, so its output can only ever depend on c->w/c->h,
    // which never change after ui_canvas_create().
    size_t bytes = (size_t)c->w * c->h * sizeof(uint32_t);
    c->sceneCache = (uint32_t *)malloc(bytes);
    if (c->sceneCache) memcpy(c->sceneCache, c->px, bytes);
    // If the allocation failed, sceneCache stays NULL and every future
    // call just repaints the scene the slow way -- correct, only not
    // fast, so there's no failure mode here worse than "as before".
}

void ui_fill_rect_fast(UiCanvas *c, UiRect r, UiColor color)
{
    if (color.a == 0) return;
    int x0 = (int)(r.x + 0.5f), y0 = (int)(r.y + 0.5f);
    int x1 = (int)(r.x + r.w + 0.5f), y1 = (int)(r.y + r.h + 0.5f);
    if (x0 < c->clipX0) x0 = c->clipX0;
    if (y0 < c->clipY0) y0 = c->clipY0;
    if (x1 > c->clipX1) x1 = c->clipX1;
    if (y1 > c->clipY1) y1 = c->clipY1;
    if (x0 >= x1 || y0 >= y1) return;

    if (color.a == 255) {
        uint32_t v = 0xFF000000u | ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | (uint32_t)color.b;
        for (int y = y0; y < y1; y++) {
            uint32_t *row = &c->px[(size_t)y * c->w];
            for (int x = x0; x < x1; x++) row[x] = v;
        }
    } else {
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++)
                ui_blend_pixel(c, x, y, color, 1.0f);
    }
}
