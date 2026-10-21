// frame_prof.h -- where did a slow loop iteration go? Pure bookkeeping, no clock or SDK in here (the caller
// passes microsecond timestamps), so it runs on a PC test.
#ifndef FRAME_PROF_H
#define FRAME_PROF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    FP_INPUT,      // controller read + handling the presses (includes any blocking work a press started)
    FP_POLL,       // update check, keyboard, relay signal, background-state query
    FP_PREVIEW,    // live preview: colour pipeline + UDP send to WLED
    FP_IO,         // plugin-loader config read, lease/claim ticks, shell heartbeat file
    FP_RENDER,     // drawing the frame into the canvas
    FP_PRESENT,    // copy to the window surface + flip (waits for vsync)
    FP_COUNT
} FrameStage;

#define FRAME_PROF_SLOW_US   50000u    // an iteration over this is logged
#define FRAME_PROF_MIN_GAP_US 250000u  // ...at most this often, the rest are counted

typedef struct {
    uint64_t stageUs[FP_COUNT];     // this iteration
    uint64_t lastMarkUs;
    uint64_t lastLogUs;
    uint32_t suppressed;            // slow iterations skipped since the last line
    uint32_t slowTotal;
} FrameProf;

void frame_prof_init(FrameProf *fp);
// Call at the top of every iteration.
void frame_prof_begin(FrameProf *fp, uint64_t nowUs);
// Charges the time since the previous mark (or begin) to `stage`.
void frame_prof_mark(FrameProf *fp, FrameStage stage, uint64_t nowUs);
// End of the iteration, before any sleep: if it was slow and the rate limit allows, writes one line to buf and returns true.
// Mark the last stage first; the caller prints the line.
bool frame_prof_end(FrameProf *fp, uint64_t nowUs, const char *tag, char *buf, size_t bufSize);

#endif
