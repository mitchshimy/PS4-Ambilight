// hdr2200_vote.h -- per-frame PQ-or-SDR vote for 0x80002200 buffers. Its own
// header and nothing but stdint so tools/test_hdr2200_vote.c can build it on a
// PC. Why it works: docs/debugging/hdr2200-alpha-detection.md.
#ifndef HDR2200_VOTE_H
#define HDR2200_VOTE_H

#include <stdint.h>

#define HDR2200_VOTE_SAMPLES    8  // zone words read per check (capped, not all configured zones)
#define HDR2200_VOTE_MIN_WORDS  4  // fewer non-zero words than this and the check says nothing
#define HDR2200_FASTPATH        1  // 0 = only the older smoothness detector decides, as in v3.5
#define HDR2200_FADE_SPREAD     4  // top bytes this close together, all in 0xc1..0xfe, are a fade, not PQ

// +1: looks like A2R10G10B10 PQ. -1: looks like 8-bit SDR. 0: keep whatever
// mode is active (too few words, a mix in between, or a fade frame).
//
// A PQ word has 0b11 in bits 31:30 (opaque 2-bit alpha), so its top byte is
// 0b11RRRRRR. 0xff would need R10 >= 1008, red at 8,700 nits or more, and real
// content doesn't do that. In the captures every HDR-on word had a top byte of
// 0xc0..0xdd and 97% of the HDR-off words had 0xff. Unlike the smoothness
// detector this needs no scene content, so a black frame decides too: 0xc0000000
// is PQ black, 0xff000000 is SDR black. All-zero words are a cleared buffer
// and count for neither side.
//
// Fade guard. Hitman fades to and from black in SDR by writing a frame-wide
// alpha byte into the SDR words (0x11 up to 0xfe over about a second). Alphas
// 0xc1..0xfe pass the PQ test above. The 8 zones of one fade frame differ by a
// few counts (cc cb cb cb ca ca ca c8) because the game is still writing while
// we read, so the guard is a spread, not equality. Real PQ only clusters that
// tightly in 0xc1..0xfe when the scene is dark and flat, so the frame holds.
// Cost: a dark flat PQ frame seen while the mode is SDR holds until a frame
// with more variation arrives. Pure black (0xc0) is exempt and decides.
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
