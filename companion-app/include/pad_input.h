// pad_input.h -- turns controller samples into button events, with no SDK calls so a PC test can run it.
// Every sample the pad buffered (scePadRead) is stepped in time order, so a tap inside one slow frame is not lost.
#ifndef PAD_INPUT_H
#define PAD_INPUT_H

#include <stdint.h>
#include <stdbool.h>

// Button bits as the SDK defines them (ORBIS_PAD_BUTTON_*), filled in by the caller.
typedef struct {
    uint32_t up, down, left, right, l1, r1, cross, circle, triangle, square, options;
} PadMasks;

typedef struct { uint32_t buttons; uint64_t timestamp; } PadSample;

// What one sample produced. up/down/left/right/l1/r1 fire on the press and then auto-repeat while held;
// circle/options/triangle/square fire on the press edge; crossUp fires on the RELEASE of Cross.
typedef struct {
    bool up, down, left, right, l1, r1;
    bool circle, options, triangle, square;
    bool crossUp;
} PadEvents;

#define PAD_REPEAT_INITIAL_DELAY_MS 350u
#define PAD_REPEAT_INTERVAL_MS       90u

typedef struct { bool wasDown; uint32_t pressedAtMs, lastRepeatMs; } PadRepeat;

typedef struct {
    PadMasks m;
    uint32_t prev;          // buttons seen in the last sample stepped
    uint32_t ignore;        // buttons held when padin_resync() ran: ignored until they are let go
    uint64_t lastTs;        // newest timestamp already consumed
    PadRepeat rep[6];
    bool allowMultiAction;  // false (default): Cross/Circle/Triangle/Square/Options act at most once per padin_process call
} PadInput;

void padin_init(PadInput *pi, const PadMasks *masks);

static inline bool padev_any(const PadEvents *e)
{
    return e->up || e->down || e->left || e->right || e->l1 || e->r1 ||
           e->circle || e->options || e->triangle || e->square || e->crossUp;
}

// Feeds `n` samples in any order; old ones (timestamp <= last, 0 = unknown) are skipped, the rest run oldest first. Returns events written.
// Actions (Cross release, Circle, Triangle, Square, Options) fire once per call; moves are unlimited; no new sample = held repeat only.
int padin_process(PadInput *pi, const PadSample *in, int n, uint32_t nowMs, PadEvents *out, int outMax);

// Drops what is queued without events and ignores every button held now until it is released: for after a blocking call,
// the keyboard or the background, so old presses can't fire late.
void padin_resync(PadInput *pi, const PadSample *in, int n, uint32_t nowMs);

#endif
