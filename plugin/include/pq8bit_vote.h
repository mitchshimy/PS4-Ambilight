// pq8bit_vote.h -- alpha byte vote for 0x88740000 buffers that actually hold
// 8-bit ARGB. Its own header and nothing but stdint so tools/test_pq8bit_vote.c
// can build it on a PC. Why it works: docs/debugging/youtube-hdr-8bit.md.
#ifndef PQ8BIT_VOTE_H
#define PQ8BIT_VOTE_H

#include <stdint.h>

#define PQ8BIT_DETECT_SAMPLES 8  // zone words read per check (capped, not all configured zones)
#define PQ8BIT_MIN_SAMPLES    4  // fewer readable words than this and the check says nothing

// +1: looks like 8-bit A8R8G8B8. -1: looks like real A2R10G10B10 PQ. 0: keep
// whatever mode is active (too few samples, or a mix in between).
//
// A real 10-bit word has alpha in bits 31:30, so an alpha BYTE of 0xff means
// R10 >= 1008, red at 8,700 nits or more. Real content doesn't do that, and
// real PQ black is 0xc0000000 or 0x00000000. 8-bit SDR content is 0xff on
// every pixel. Every check in the captures came out 8 of 8 or 0 of 8, so the
// 25%..75% band is there for a stray overlay pixel and hasn't been hit yet.
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
