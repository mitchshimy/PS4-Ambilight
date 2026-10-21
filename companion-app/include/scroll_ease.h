// scroll_ease.h -- time-based, critically damped follow of a scroll target. No overshoot, and the result
// depends on elapsed time only, so it looks the same at 60 fps and on a slow frame.
#ifndef SCROLL_EASE_H
#define SCROLL_EASE_H

#include <math.h>
#include <stdbool.h>

#define SCROLL_EASE_OMEGA 22.0f      // rad/s: about 0.2 s to settle, about 45 ms time constant

typedef struct { float pos, vel, target; } ScrollEase;

// Jump straight there (new screen).
static inline void scroll_ease_snap(ScrollEase *s, float v) { s->pos = s->target = v; s->vel = 0.0f; }

// Advances by dtSec toward the target. Returns true while it is still moving (the caller must redraw).
static inline bool scroll_ease_step(ScrollEase *s, float dtSec)
{
    float delta = s->pos - s->target;
    if (fabsf(delta) < 0.5f && fabsf(s->vel) < 8.0f) {      // under half a pixel: finish exactly
        bool moved = (s->pos != s->target);
        s->pos = s->target; s->vel = 0.0f;
        return moved;
    }
    if (dtSec <= 0.0f) return true;
    float e = expf(-SCROLL_EASE_OMEGA * dtSec);
    float t = (s->vel + SCROLL_EASE_OMEGA * delta) * dtSec;
    s->pos = s->target + (delta + t) * e;
    s->vel = (s->vel - SCROLL_EASE_OMEGA * t) * e;
    return true;
}

#endif
