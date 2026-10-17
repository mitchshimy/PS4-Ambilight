// pq8bit_vote.h -- alpha byte vote for 0x88740000 buffers that actually hold 8-bit ARGB.
// More: docs/code-comments.md (plugin/include/pq8bit_vote.h:1)
#ifndef PQ8BIT_VOTE_H
#define PQ8BIT_VOTE_H

#include <stdint.h>

#define PQ8BIT_DETECT_SAMPLES 8  // zone words read per check (capped, not all configured zones)
#define PQ8BIT_MIN_SAMPLES    4  // fewer readable words than this and the check says nothing

// +1: looks like 8-bit A8R8G8B8. -1: looks like real A2R10G10B10 PQ. 0: keep whatever mode is active...
// More: docs/code-comments.md (plugin/include/pq8bit_vote.h:12)
static inline int pq8bitVote(const uint32_t *px, uint32_t n)
{
    if (n < PQ8BIT_MIN_SAMPLES) return 0;
    uint32_t ff = 0;
    for (uint32_t i = 0; i < n; i++)
        if ((px[i] >> 24) == 0xFFu) ff++;
    if (ff * 4u >= n * 3u) return +1;
    if (ff * 4u <= n)      return -1;
    return 0;
}

#endif
