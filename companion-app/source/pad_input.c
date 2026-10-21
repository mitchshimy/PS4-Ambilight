// pad_input.c -- see pad_input.h.
#include "pad_input.h"
#include <string.h>

static bool repeat_fire(PadRepeat *rs, bool down, uint32_t nowMs)
{
    if (!down) { rs->wasDown = false; return false; }
    if (!rs->wasDown) {
        rs->wasDown = true;
        rs->pressedAtMs = nowMs;
        rs->lastRepeatMs = nowMs;
        return true;                         // fires immediately on the initial press
    }
    if (nowMs - rs->pressedAtMs < PAD_REPEAT_INITIAL_DELAY_MS) return false;
    if (nowMs - rs->lastRepeatMs >= PAD_REPEAT_INTERVAL_MS) {
        rs->lastRepeatMs = nowMs;
        return true;
    }
    return false;
}

void padin_init(PadInput *pi, const PadMasks *masks)
{
    memset(pi, 0, sizeof(*pi));
    pi->m = *masks;
}

static PadEvents step(PadInput *pi, uint32_t raw, uint32_t nowMs)
{
    // A button that was already down at resync stays invisible until it has been released once.
    pi->ignore &= raw;
    uint32_t b = raw & ~pi->ignore;
    const PadMasks *m = &pi->m;
    PadEvents e;
    memset(&e, 0, sizeof(e));
    e.up    = repeat_fire(&pi->rep[0], (b & m->up) != 0,    nowMs);
    e.down  = repeat_fire(&pi->rep[1], (b & m->down) != 0,  nowMs);
    e.left  = repeat_fire(&pi->rep[2], (b & m->left) != 0,  nowMs);
    e.right = repeat_fire(&pi->rep[3], (b & m->right) != 0, nowMs);
    e.l1    = repeat_fire(&pi->rep[4], (b & m->l1) != 0,    nowMs);
    e.r1    = repeat_fire(&pi->rep[5], (b & m->r1) != 0,    nowMs);
#define PRESSED(bit) (((b) & (bit)) && !(pi->prev & (bit)))
    e.circle   = PRESSED(m->circle);
    e.options  = PRESSED(m->options);
    e.triangle = PRESSED(m->triangle);
    e.square   = PRESSED(m->square);
#undef PRESSED
    e.crossUp = !(b & m->cross) && (pi->prev & m->cross);
    pi->prev = b;
    return e;
}

#define PAD_MAX_SAMPLES 64

// Indices of the samples worth stepping, oldest first. Stable insertion sort: n is at most a few dozen.
static int pick_new(const PadInput *pi, const PadSample *in, int n, int *idx)
{
    int k = 0;
    if (n > PAD_MAX_SAMPLES) n = PAD_MAX_SAMPLES;
    for (int i = 0; i < n; i++) {
        if (in[i].timestamp != 0 && in[i].timestamp <= pi->lastTs) continue;
        int j = k++;
        while (j > 0 && in[idx[j - 1]].timestamp > in[i].timestamp) { idx[j] = idx[j - 1]; j--; }
        idx[j] = i;
    }
    return k;
}

int padin_process(PadInput *pi, const PadSample *in, int n, uint32_t nowMs, PadEvents *out, int outMax)
{
    int idx[PAD_MAX_SAMPLES];
    int k = (n > 0 && in) ? pick_new(pi, in, n, idx) : 0;
    int w = 0;
    if (k == 0) {
        PadEvents e = step(pi, pi->prev | pi->ignore, nowMs);   // hold only: repeats, never an edge
        if (padev_any(&e) && w < outMax) out[w++] = e;
        return w;
    }
    bool seenCross = false, seenCircle = false, seenTri = false, seenSq = false, seenOpt = false;
    for (int i = 0; i < k; i++) {
        const PadSample *s = &in[idx[i]];
        PadEvents e = step(pi, s->buttons, nowMs);
        if (s->timestamp > pi->lastTs) pi->lastTs = s->timestamp;
        if (!pi->allowMultiAction) {
            if (e.crossUp)   { if (seenCross)  e.crossUp = false;   seenCross = true; }
            if (e.circle)    { if (seenCircle) e.circle = false;    seenCircle = true; }
            if (e.triangle)  { if (seenTri)    e.triangle = false;  seenTri = true; }
            if (e.square)    { if (seenSq)     e.square = false;    seenSq = true; }
            if (e.options)   { if (seenOpt)    e.options = false;   seenOpt = true; }
        }
        if (padev_any(&e) && w < outMax) out[w++] = e;
    }
    return w;
}

void padin_resync(PadInput *pi, const PadSample *in, int n, uint32_t nowMs)
{
    (void)nowMs;
    if (n > PAD_MAX_SAMPLES) n = PAD_MAX_SAMPLES;
    const PadSample *newest = NULL;
    for (int i = 0; i < n; i++) {
        if (!newest || in[i].timestamp >= newest->timestamp) newest = &in[i];
        if (in[i].timestamp > pi->lastTs) pi->lastTs = in[i].timestamp;
    }
    uint32_t held = newest ? newest->buttons : pi->prev;
    pi->ignore = held;
    pi->prev = 0;
    memset(pi->rep, 0, sizeof(pi->rep));
}
