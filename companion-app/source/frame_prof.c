// frame_prof.c -- see frame_prof.h.
#include "frame_prof.h"
#include <stdio.h>
#include <string.h>

static const char *kNames[FP_COUNT] = { "input", "poll", "preview", "io", "render", "present" };

void frame_prof_init(FrameProf *fp) { memset(fp, 0, sizeof(*fp)); }

void frame_prof_begin(FrameProf *fp, uint64_t nowUs)
{
    memset(fp->stageUs, 0, sizeof(fp->stageUs));
    fp->lastMarkUs = nowUs;
}

void frame_prof_mark(FrameProf *fp, FrameStage stage, uint64_t nowUs)
{
    if (nowUs > fp->lastMarkUs) fp->stageUs[stage] += nowUs - fp->lastMarkUs;
    fp->lastMarkUs = nowUs;
}

bool frame_prof_end(FrameProf *fp, uint64_t nowUs, const char *tag, char *buf, size_t bufSize)
{
    uint64_t total = 0;
    for (int i = 0; i < FP_COUNT; i++) total += fp->stageUs[i];
    (void)nowUs;
    if (total < FRAME_PROF_SLOW_US) return false;

    fp->slowTotal++;
    if (fp->lastLogUs != 0 && nowUs - fp->lastLogUs < FRAME_PROF_MIN_GAP_US) { fp->suppressed++; return false; }

    int n = snprintf(buf, bufSize, "[prof] slow iteration %llu ms (%s):",
                     (unsigned long long)(total / 1000), tag ? tag : "");
    for (int i = 0; i < FP_COUNT && n > 0 && (size_t)n < bufSize; i++)
        n += snprintf(buf + n, bufSize - (size_t)n, " %s %llu", kNames[i], (unsigned long long)(fp->stageUs[i] / 1000));
    if (n > 0 && (size_t)n < bufSize) n += snprintf(buf + n, bufSize - (size_t)n, " ms");
    if (fp->suppressed && n > 0 && (size_t)n < bufSize)
        snprintf(buf + n, bufSize - (size_t)n, " (+%u more since the last line)", fp->suppressed);
    fp->suppressed = 0;
    fp->lastLogUs = nowUs ? nowUs : 1;
    return true;
}
