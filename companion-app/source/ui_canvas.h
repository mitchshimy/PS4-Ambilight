// ui_canvas.h -- a small anti-aliased 2D canvas.
//
// Why this exists: the blueprint (style.css) is a browser design. Its
// whole visual language depends on things SDL_RenderFillRect cannot
// draw -- 8px and 4px rounded corners, 1px hairline borders at
// rgba(255,255,255,0.09), 1.8px-stroke line icons, a soft radial glow
// behind the page. The previous UI approximated all of that with
// axis-aligned opaque rects, which is the single biggest reason it
// didn't look like the blueprint.
//
// So: everything is composited by this module into one ARGB8888 pixel
// buffer with real coverage-based anti-aliasing and real alpha
// blending, and main.c blits that buffer to the window once per
// frame. Nothing in here knows about SDL, which also means the whole
// UI can be compiled and rendered to a PNG on a desktop for visual
// diffing against the blueprint screenshots.

#ifndef UI_CANVAS_H
#define UI_CANVAS_H

#include <stdint.h>
#include <stdbool.h>

// Straight (non-premultiplied) RGBA, matching how CSS colours are
// written in style.css, so tokens transcribe literally.
typedef struct { uint8_t r, g, b, a; } UiColor;

static inline UiColor ui_rgb(uint8_t r, uint8_t g, uint8_t b) { UiColor c = { r, g, b, 255 }; return c; }
static inline UiColor ui_rgba(uint8_t r, uint8_t g, uint8_t b, float a)
{
    UiColor c; c.r = r; c.g = g; c.b = b;
    if (a < 0.0f) a = 0.0f; if (a > 1.0f) a = 1.0f;
    c.a = (uint8_t)(a * 255.0f + 0.5f);
    return c;
}
// Scales an existing colour's alpha -- used for the "fade a token"
// cases CSS expresses as rgba() over an already-named colour.
static inline UiColor ui_fade(UiColor c, float a)
{
    float na = (c.a / 255.0f) * a;
    if (na < 0.0f) na = 0.0f; if (na > 1.0f) na = 1.0f;
    c.a = (uint8_t)(na * 255.0f + 0.5f);
    return c;
}

typedef struct { float x, y, w, h; } UiRect;
static inline UiRect ui_rect(float x, float y, float w, float h) { UiRect r = { x, y, w, h }; return r; }

// Per-corner radii, in the CSS order (top-left, top-right,
// bottom-right, bottom-left) so `border-radius` values transcribe
// directly. ui_radius_all() covers the common uniform case.
typedef struct { float tl, tr, br, bl; } UiRadius;
static inline UiRadius ui_radius_all(float r) { UiRadius k = { r, r, r, r }; return k; }
static inline UiRadius ui_radius4(float tl, float tr, float br, float bl) { UiRadius k = { tl, tr, br, bl }; return k; }

#define UI_CLIP_STACK_MAX 8

typedef struct UiShape UiShape;

typedef struct {
    uint32_t *px;      // ARGB8888, row-major, always fully opaque after ui_canvas_clear
    int w, h;
    // Clip stack -- needed for the blueprint's .content scroll region,
    // which crops cards at a hard edge partway down the page.
    int clipX0, clipY0, clipX1, clipY1;
    int clipStack[UI_CLIP_STACK_MAX][4];
    int clipDepth;
    // One reusable shape/coverage workspace per canvas, so the
    // rasteriser never allocates mid-frame.
    UiShape *shape;
    // Lazily-built cache of ui_draw_scene()'s output (the 80px grid +
    // radial glow), reused on every call after the first instead of
    // repainting it. That background is a pure function of canvas
    // size, which never changes for the app's one canvas, so caching
    // it is always correct, not just an approximation. It matters:
    // the glow alone touches roughly 450,000 pixels with a sqrtf and
    // an alpha blend each -- fine once, not fine 30 times a second,
    // which is what made Test Strip's continuous redraw feel sluggish.
    uint32_t *sceneCache;
} UiCanvas;

UiCanvas *ui_canvas_create(int w, int h);
void ui_canvas_destroy(UiCanvas *c);
void ui_canvas_clear(UiCanvas *c, UiColor color);

// Clipping. ui_clip_push intersects with the current clip; the
// matching ui_clip_pop restores it. Depth is small and fixed -- the
// blueprint never nests more than a couple of levels.
void ui_clip_push(UiCanvas *c, UiRect r);
void ui_clip_pop(UiCanvas *c);
void ui_clip_reset(UiCanvas *c);

// ---- direct pixel compositing -------------------------------------
// Blends `color` over the pixel at (x, y) with an extra coverage
// multiplier in [0,1]. Everything else in this file eventually lands
// here.
void ui_blend_pixel(UiCanvas *c, int x, int y, UiColor color, float coverage);

// ---- coverage accumulation ----------------------------------------
// A shape is built up as one or more polygons whose coverage is
// combined with max(), then composited in a single pass. This matters
// for strokes: a polyline drawn as per-segment quads plus round joins
// would double-blend along every seam if each piece were composited
// separately, leaving visible darker notches on the 1.8px icon
// strokes. Accumulating first and blending once avoids that entirely.
// Contours are grouped into paths. Within one path every contour is
// rasterised together under the nonzero winding rule, which is what
// makes a hole possible (outer contour one way, inner contour the
// other) -- that's how an inside-stroke ring is drawn. Between paths,
// coverage combines with max().
UiShape *ui_shape_begin(UiCanvas *c);
void ui_shape_polygon(UiShape *s, const float *xy, int pointCount); // starts a new path
void ui_shape_contour(UiShape *s, const float *xy, int pointCount); // adds to the current path
void ui_shape_fill(UiShape *s, UiColor color);   // composites and resets the shape for reuse
void ui_shape_end(UiShape *s);

// ---- primitives ---------------------------------------------------
void ui_fill_rect(UiCanvas *c, UiRect r, UiColor color);
void ui_fill_round_rect(UiCanvas *c, UiRect r, UiRadius radius, UiColor color);

// A direct, unantialiased pixel fill for small axis-aligned rects --
// no polygon build, no coverage scan, just a nested loop writing
// pixels straight into the canvas. Visually indistinguishable from
// ui_fill_rect() at the sizes it's meant for (a handful of pixels
// across, like an LED tick), but far cheaper when called hundreds of
// times a frame, which the AA rasteriser's per-shape overhead (vertex
// arrays, a 5x-subsampled scanline pass) is not built for. Do not use
// this for anything large enough that the missing edge antialiasing
// would actually show.
void ui_fill_rect_fast(UiCanvas *c, UiRect r, UiColor color);
// Inside-stroke, like CSS `border` on a border-box element: the stroke
// occupies the outermost `width` pixels of `r`, it does not straddle
// the edge.
void ui_stroke_round_rect(UiCanvas *c, UiRect r, UiRadius radius, float width, UiColor color);
void ui_fill_circle(UiCanvas *c, float cx, float cy, float radius, UiColor color);
void ui_stroke_circle(UiCanvas *c, float cx, float cy, float radius, float width, UiColor color);
void ui_stroke_line(UiCanvas *c, float x0, float y0, float x1, float y1, float width, UiColor color, bool roundCaps);
// A dashed horizontal hairline, for `border-top: 1px dashed` (the
// "Physical strip total" divider on Set up).
void ui_dashed_hline(UiCanvas *c, float x, float y, float w, float dash, float gap, float thickness, UiColor color);

// ---- backgrounds --------------------------------------------------
// The shared .scene layer: an 80px measurement grid plus the warm
// radial glow anchored above the top edge. Transcribed from
// style.css's .scene background shorthand.
void ui_draw_scene(UiCanvas *c);

// ---- polyline/polygon stroking used by the icon renderer ----------
// Appends the outline of a stroked polyline (round joins and,
// optionally, round caps) into an in-progress shape.
void ui_shape_stroke_polyline(UiShape *s, const float *xy, int pointCount, float width, bool closed, bool roundCaps);

#endif // UI_CANVAS_H
