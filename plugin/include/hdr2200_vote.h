// hdr2200_vote.h -- per-frame PQ-or-SDR vote for 0x80002200 buffers.
// More: docs/code-comments.md (plugin/include/hdr2200_vote.h:1)
#ifndef HDR2200_VOTE_H
#define HDR2200_VOTE_H

#include <stdint.h>

#define HDR2200_VOTE_SAMPLES    8  // zone words read per check (capped, not all configured zones)
#define HDR2200_VOTE_MIN_WORDS  4  // fewer non-zero words than this and the check says nothing
#define HDR2200_FASTPATH        1  // 0 = only the older smoothness detector decides, as in v3.5
#define HDR2200_FADE_SPREAD     4  // top bytes this close together, all in 0xc1..0xfe, are a fade, not PQ

// +1: looks like A2R10G10B10 PQ. -1: looks like 8-bit SDR. 0: keep whatever mode is active (too few...
// More: docs/code-comments.md (plugin/include/hdr2200_vote.h:14)
static inline int hdr2200Vote(const uint32_t *px, uint32_t n)
{
    uint32_t valid = 0, pq = 0, topLo = 0xFFu, topHi = 0u;
    for (uint32_t i = 0; i < n; i++) {
        if (px[i] == 0u) continue;
        uint32_t top = px[i] >> 24;
        if (top < topLo) topLo = top;
        if (top > topHi) topHi = top;
        valid++;
        if ((px[i] >> 30) == 3u && top != 0xFFu) pq++;
    }
    if (valid < HDR2200_VOTE_MIN_WORDS) return 0;
    if (topLo >= 0xC1u && topHi <= 0xFEu && (topHi - topLo) <= HDR2200_FADE_SPREAD) return 0;
    if (pq * 4u >= valid * 3u) return +1;
    if (pq * 4u <= valid)      return -1;
    return 0;
}

#endif
