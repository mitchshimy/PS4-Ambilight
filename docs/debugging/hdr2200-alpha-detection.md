# HITMAN 3 and RDR2: HDR or SDR under `0x80002200`, from the alpha byte (v3.6)

HITMAN 3 registers `0x80002200` whether the console's HDR setting is on or off, and
so does RDR2. The plugin used to decide between the two decodes with a smoothness
check, and on a real HDR-on capture that took 53 seconds, all of it lit from the
wrong decode. This is what the captures showed and how the plugin now decides on the
first frame.

## What was seen

`0x80002200` is A8B8G8R8_SRGB with HDR off and A2R10G10B10 PQ with it on, and the
game never re-registers or says which ([hdr-pixel-format](hdr-pixel-format.md)
has how that was found). Until v3.5 only the smoothness detector chose. It decodes 8
zone words both ways and keeps the smoother result, but skips frames below a noise
floor and needs 4 agreeing checks, one every 60 passes.

A capture of HITMAN 3 with HDR on, from game start until the detector corrected
itself (debug build, listener log):

| Time | What the detector's packets said |
|---|---|
| 0 to 46 s | nothing. Frames too dark to score, every check skipped by the noise floor |
| 46.88 s | first check that cleared it: SDR total variation 998, HDR 178, streak 1 |
| 49.08 s, 51.31 s | streak 2, then 3 |
| 53.45 s | streak 4, `isHdr` 1, FLK1 flags `0x2a` become `0x2b` |

That was 53.45 s. The buffer held PQ the entire time. Decoded as SDR, the sampled
words read 4.6 out of 255 on average in the first 10 s, 80 in the 20 to 30 s window
and 125 in the 40 to 50 s window, where the PQ decode of the same words gave 0 to 36.
The strip was showing a wrong, brighter picture the whole minute.

## The tell: the alpha byte

Same idea as [youtube-hdr-8bit](youtube-hdr-8bit.md), with the sides swapped. A PQ
word has `0b11` in bits 31:30, so its top byte is `0b11RRRRRR`. `0xff` would need
R10 of 1008 or more, red at 8,700 nits or more, which real content doesn't do. A
plain 8-bit word carries its own alpha, `0xff` here.

| Capture | Words | Top byte |
|---|---|---|
| HITMAN 3, HDR on | 1554 | `0xc0` to `0xdd` in every word, `0xff` in none |
| HITMAN 3, HDR off | 1476 | `0xff` in 1434 (97%). The rest: 11 cleared words, and a fade (below) |

Every word in the HDR-on capture had `11` in the top two bits, from the first packet.
Nothing about it needs scene content: PQ black is `0xc0000000` and SDR black is
`0xff000000`, so a black frame decides too, which the smoothness detector could never
do.

## What it does now

`detectHdr2200Fast()` in `zones.c`, with the vote in
`plugin/include/hdr2200_vote.h`:

- Runs on every sampling pass while the active format is `0x80002200`, from
  `detectHdr2200Format()`, which `sample_thread.c` calls just before
  `getUnpackFnForFormat()`. So the mode is set for the frame being decoded.
- Reads the first 8 zone positions, ignores all-zero words (a cleared buffer), needs
  at least 4 others.
- 6 or more of 8 with the PQ signature (top two bits `11`, top byte not `0xff`) means
  PQ, 2 or fewer means SDR, 3 to 5 holds. One saturated red PQ word, whose top byte
  is `0xff`, is outvoted.
- A decisive vote sets `g_hdr2200IsHdr` and zeroes the smoothness detector's streak.
  On a hold the smoothness detector runs as before, throttled, and can still decide.
- `HDR2200_FASTPATH` at 0 in the header switches all of this off.

Telemetry (debug builds): a `HDRV` packet on every mode change and roughly every 30
checks, 48 bytes after the DDP header, 58 on the wire, laid out like `PQ8C` in the
YouTube write-up:

| Bytes | Contents |
|---|---|
| 0-3 | `HDRV` |
| 4 | vote, int8: +1 PQ, -1 SDR, 0 hold |
| 5 | words read |
| 6 | of those, how many had the PQ signature |
| 7 | mode after this check (1 = PQ) |
| 8-39 | the 8 words, uint32 LE, unread slots 0 |
| 40-43 | `g_numZones` |
| 44-47 | low 32 bits of the buffer address |

No new FLK1 flag: bit `0x01` is still "HDR decode active".

## The first build, and the fade

The first build had only the alpha test. The HDR-off capture from that build showed a
problem the HDR-off log from before the change had hinted at. HITMAN 3 fades to and
from black in SDR by writing a frame-wide alpha byte into the words, ramping
`0x11` up to `0xfe` over about a second (the earlier capture's fade-out goes `ed`,
`ba`, `87`, `55`, `10`). Alphas of `0xc1` to `0xfe` pass for PQ. At 49.59 s a fade-in
frame read `cc181515 cb181615 cb181616 ... c81b1817`, eight words with the PQ
signature, and the vote said PQ. One frame later, `f90d0c0c ...`, a bright PQ frame
if read that way and black in SDR, held the mode in PQ until the alpha reached `0xff`
at 49.82 s. About 0.22 s of a wrong decode, with FLK1 flags `0x2a` becoming `0x2b` and
back, and it is exactly the case where a wrong decode is brightest.

The words could not be told apart by their top byte alone, since real dark PQ scenes
sit at `0xc1` to `0xc5`. The difference is how tightly they cluster. The 8 zones of
one fade frame differ by only a few counts, because the game is still writing while
the plugin reads. Equal top bytes were tried first and missed it for that reason, so
the guard is a spread: top bytes within 4 of each other and all in `0xc1` to `0xfe`
hold the mode instead of voting (`HDR2200_FADE_SPREAD`). PQ black, `0xc0`, is
exempt, and so is a fade that has reached `0xff` or is below `0xc0`.

The cost is on the PQ side: a dark flat PQ frame, say `c4` on every word, holds
instead of deciding. That only matters if the mode is SDR when it arrives. HITMAN 3
and RDR2 in HDR both start on a `c0000000` frame, which decides, so it did not
happen in any capture. In an HDR-on session, 10 of 33 RDR2 frames and 15 of 63
HITMAN 3 frames hold, and they keep the PQ mode they already have.

## Confirmed on hardware

Debug builds, `HDRV` packets. Frames are the plugin's 8-word reads. The counts are
PQ / SDR / hold and are in `tools/data/hdr2200_frames.csv`, which
`tools/test_hdr2200_vote.c` replays.

| Capture | Frames | PQ / SDR / hold | First frame with data | Wrong-way votes |
|---|---|---|---|---|
| HITMAN 3, HDR on | 63 | 48 / 0 / 15 | 4.84 s, PQ | 0 |
| HITMAN 3, HDR off | 53 | 0 / 49 / 4 | 4.38 s, SDR | 0 |
| RDR2, HDR on | 33 | 23 / 0 / 10 | 1.95 s, PQ | 0 |
| RDR2, HDR off | 23 | 0 / 23 / 0 | 0.34 s, SDR | 0 |
| first build, HITMAN 3 on | 52 | 46 / 0 / 6 | 4.79 s, PQ | 0 |
| first build, HITMAN 3 off | 57 | 0 / 54 / 3 | 4.40 s, SDR | 0 |

The counts are what the final vote says about each recorded frame. The first build
voted PQ on one frame of that last HDR-off capture, the fade-in described above, which
is why the fixture is in the test. The frames before the first one with data are cleared buffers (all zero words), and
the vote holds on those. In the final build no HDR-off capture flipped the mode and
the flags stayed at `0x2a` through the fade-in at about 48.6 s, where the same
kind of frame that fooled the first build held. Colors were right in both modes in
both games. Compared with 53.45 s, the first frame with data is the whole delay.

Also checked: neither game applies a change to the console's HDR setting until it is
restarted. RDR2 started with HDR on and switched to SDR mid-game (53 s), and started
with it off and switched to HDR (37 s). HITMAN 3 was toggled to on mid-game (71 s).
None of the three captures changed: the PQ one stayed PQ, the SDR ones stayed 8-bit
(all 675 raw words in the RDR2 off capture have alpha `0xff`).

`tools/test_hdr2200_vote.c` also checks, with the exact-equality guard the first
build would have had, that the fade-in fixtures fail. So they do catch the flaw.

## Not verified

- **Two titles.** HITMAN 3 and RDR2. Any other title registering `0x80002200` is
  untried. The SDR side is checked only on these two, and an 8-bit title whose alpha
  is neither `0xff` nor `0x00` would be a problem.
- **A live HDR switch** on a title that applies it without a restart. The fade guard
  would make the plugin hold on a dark flat PQ frame there, and the smoothness detector
  would decide instead.
- **A release build.** The decision is not inside the debug-only telemetry, but only
  debug builds were captured.
- **Partial-alpha content that is really PQ.** A PQ scene that is flat and dark across
  all 8 zones with a top byte of `0xc1` to `0xc5` holds. Not seen to matter.
- **The 8 samples are the first 8 zones**, next to each other at the start corner,
  like the YouTube check.

## Checking a new title

A debug build, `[dev]` telemetry on (see [tools](../development/tools.md)), one
capture per HDR setting, then look at the top byte of the raw words or at the `HDRV`
votes. HDR on should vote `+1` from the first frame with data, HDR off `-1`, with
holds only on cleared buffers and fades. If a title's SDR words have an alpha between
`0xc1` and `0xfe`, the vote will misread it and `HDR2200_FASTPATH` should be 0 until
it has its own handling.
