// ui_canvas.h -- a small anti-aliased 2D canvas.
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:1)

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

// Per-corner radii, in the CSS order (top-left, top-right, bottom-right, bottom-left) so...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:49)
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
    // Lazily-built cache of ui_draw_scene()'s output (the 80px grid + radial glow), reused on every call...
    // More: docs/code-comments.md (companion-app/source/ui_canvas.h:71)
    uint32_t *sceneCache;
    // Layer canvases (ui_canvas_create_layer) keep PREMULTIPLIED ARGB and may be partly or wholly transparent...
    // Everything else (the screen canvas) stays opaque, straight-ARGB as before.
    bool premul;
} UiCanvas;

UiCanvas *ui_canvas_create(int w, int h);
void ui_canvas_destroy(UiCanvas *c);
void ui_canvas_clear(UiCanvas *c, UiColor color);

// ---- layers: an off-screen canvas that starts transparent and stores premultiplied ARGB: paint once, composite at any offset.
// Normal drawing calls work on it (ui_blend_pixel checks c->premul); ui_draw_scene / ui_canvas_clear are for opaque canvases only.
UiCanvas *ui_canvas_create_layer(int w, int h);
// Makes rows [y0, y1) fully transparent again (clamped to the layer).
void ui_layer_clear_rows(UiCanvas *layer, int y0, int y1);
// Composites rows [srcY, srcY + rows) of `layer` onto the opaque canvas `dst` from row dstY, full width;
// rows outside the layer are skipped, rows outside dst clipped.
void ui_layer_composite(UiCanvas *dst, const UiCanvas *layer, int srcY, int dstY, int rows);

// Clipping. ui_clip_push intersects with the current clip; the matching ui_clip_pop restores it.
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:86)
void ui_clip_push(UiCanvas *c, UiRect r);
void ui_clip_pop(UiCanvas *c);
void ui_clip_reset(UiCanvas *c);

// direct pixel compositing Blends `color` over the pixel at (x, y) with an extra coverage multiplier...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:93)
void ui_blend_pixel(UiCanvas *c, int x, int y, UiColor color, float coverage);

// coverage accumulation A shape is built up as one or more polygons whose coverage is combined with...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:99)
UiShape *ui_shape_begin(UiCanvas *c);
void ui_shape_polygon(UiShape *s, const float *xy, int pointCount); // starts a new path
void ui_shape_contour(UiShape *s, const float *xy, int pointCount); // adds to the current path
void ui_shape_fill(UiShape *s, UiColor color);   // composites and resets the shape for reuse
void ui_shape_end(UiShape *s);

// ---- primitives ---------------------------------------------------
void ui_fill_rect(UiCanvas *c, UiRect r, UiColor color);
void ui_fill_round_rect(UiCanvas *c, UiRect r, UiRadius radius, UiColor color);

// A direct, unantialiased pixel fill for small axis-aligned rects -- no polygon build, no coverage...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:121)
void ui_fill_rect_fast(UiCanvas *c, UiRect r, UiColor color);
// Inside-stroke, like CSS `border` on a border-box element: the stroke occupies the outermost `width`...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:131)
void ui_stroke_round_rect(UiCanvas *c, UiRect r, UiRadius radius, float width, UiColor color);
void ui_fill_circle(UiCanvas *c, float cx, float cy, float radius, UiColor color);
void ui_stroke_circle(UiCanvas *c, float cx, float cy, float radius, float width, UiColor color);
void ui_stroke_line(UiCanvas *c, float x0, float y0, float x1, float y1, float width, UiColor color, bool roundCaps);
// A dashed horizontal hairline, for `border-top: 1px dashed` (the
// "Physical strip total" divider on Set up).
void ui_dashed_hline(UiCanvas *c, float x, float y, float w, float dash, float gap, float thickness, UiColor color);

// backgrounds The shared .scene layer: an 80px measurement grid plus the warm radial glow anchored...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:142)
void ui_draw_scene(UiCanvas *c);

// polyline/polygon stroking used by the icon renderer Appends the outline of a stroked polyline (round...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:148)
void ui_shape_stroke_polyline(UiShape *s, const float *xy, int pointCount, float width, bool closed, bool roundCaps);

#endif // UI_CANVAS_H
