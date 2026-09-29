# Letterbox placement and the loading-screen flashes (v3.4)

The v3.3 scan finds real bars correctly, but the margin it commits is rounded
down to a 33/60px band for stability, and the LED zones sit exactly on that
rounded row. On a bar whose real depth isn't a multiple of the band, that row
is still inside the bar, so the zones read black. Fixing that by placing zones
at the real measured depth instead then caused a second, unrelated symptom on
loading screens. Both are here.

The mechanism this fixes is written up in
[letterbox-detection.md](letterbox-detection.md#v34-bars-sampled-inside-the-bar-then-a-loading-screen-regression).
This doc is the investigation: the captures, the wrong turn, and what's still
open.

## Symptom 1: dark LEDs on a gameplay cutscene

Assassin's Creed III Remastered (CUSA11711). Trailers played from the game's
own menu letterboxed correctly -- colors came through on the bars. A cutscene
during actual gameplay did not: the top and bottom LEDs went dark and stayed
dark, no flicker, for the whole bar.

## What the captures showed

A debug-only probe packet (test build only, not in this repo) logged the
per-edge measured depth before quantizing, the committed margin, and a
black/not-black read of the exact row the zones sample. 30s captures:

| Content | Measured depth (top / bottom) | Committed margin | Zone row |
|---|---|---|---|
| Trailer | 132 / 132 | 132 | reads picture (brightest ~200-240) |
| Gameplay cutscene | 112 / 105 | 99 | reads black (0% of checks pass) |

The trailer's bar happens to be exactly 4 x 33, so the rounded-down margin
equals the real depth and the coincidence hides the bug. The cutscene's bar
doesn't land on a band edge: 112 rounds down to 99, and the zones -- placed at
exactly that row -- sit 13px inside the black bar. Same story on the bottom
edge, 105 rounding to 99, a 6px gap.

Ruler measurements on the TV (68.6-69cm for 1080 rows, so about 15.7px/cm)
had put the cutscene bar around 111-112px before any of this was captured,
which is what prompted looking here in the first place -- the probe then gave
the exact figures.

## Fix attempt 1: place at the measured depth

`placementMargin()`: instead of the quantized value, use the raw measured
depth plus `scan_depth + 1`. The stability gate is untouched -- it still
compares the quantized rect, so jitter tolerance doesn't change, only where
the committed margin places the zones.

Re-capture confirmed it: the cutscene's zones now read 2-3px inside the
picture, not the bar. The trailer was unaffected (135 instead of 132, still
in the picture).

## Symptom 2: loading screens flashing

With that fix running, a loading screen started flashing the whole strip
through varied, saturated colors.

## What that capture showed

Same probe, 6.5s on the loading screen, 12 letterbox checks:

| Edge | Measured depth |
|---|---|
| Top | 141px |
| Bottom | 37px |
| Left | 194px |
| Right | no bar found (search limit) |

And the flicker-probe packets running in parallel showed the content at the
newly placed zone row was genuinely animated -- standard deviation around 80
of 255 at one probe zone, a single-pass jump as high as 14,439 (summed across
the strip), and every registered buffer agreed with the others on the value.
That rules out a buffer-read bug: the sampled content really was flashing.

## Diagnosis: this isn't a letterbox bar

Top and bottom differ by 104px, and left has a bar while right doesn't at
all. A real letterbox or pillarbox bar comes from cropping to a different
aspect ratio, which is symmetric by construction -- the two bars on an axis
are the same depth. This wasn't that. It was the loading screen's own UI
layout: mostly dark, with a bright element (most likely a spinner or a
progress indicator, not confirmed) positioned off-center, and the per-edge
scan independently found "bar" on three of the four edges without them
agreeing with each other.

Before fix attempt 1, this probably never showed up, because the old rounded
margin -- always shallower than the real depth -- coincidentally left a gap
between the zones and whatever was near that edge. Placing zones exactly at
the measured depth removed that accidental buffer along with the bug it was
fixing.

## Fix attempt 2 (final): require the axis to agree

`axisSymmetric()`: before applying a margin, top and bottom must both be a
real positive depth and within 2 quantize bands of each other (66px
vertical, 120px horizontal); same for left and right. An axis that fails
gets margin 0 on both edges. This runs after the stability gate, only on
what gets applied -- so it doesn't change *when* a commit happens, only
whether that commit's margin is trusted enough to place zones at.

Re-capture, three contents:

| Content | Before (attempt 1) | After (attempt 2) |
|---|---|---|
| Trailer (132/132) | 138/138 | 138/138, unchanged |
| Cutscene (112/105) | 115/108 | 115/108, unchanged |
| Loading screen (141/37, 194/none) | 144/40, 197/0 (flashing) | 0/0/0/0 (LEDs off) |

**Confirmed on real hardware** across two more re-capture rounds on all three:
the loading screen's LEDs go dark instead of flashing, and the trailer and
cutscene are unchanged.

## Not verified

- The cutscene's raw detection is still unstable even though the symptom is
  gone: one capture had 2 margin commits and up to 8 distinct raw depths on
  one edge in 12.7s. It hasn't produced a visible artifact (the placement
  inset seems to absorb a few px of noise either way), but it's still there.
- On both the cutscene and the loading screen, 63-100% of checks show the
  scan stopping on a row that's 5-8 of 10 samples black -- short of the 85%
  needed -- which suggests the true bar may run a little deeper than
  measured in some frames (subtitles, HUD, grain). Not chased further; the
  placement inset (`scan_depth + 1`) gives a little headroom against it, but
  it isn't a fix for it.
- Only one title has been captured for either symptom. A genuine single-sided
  HUD bar, an ultrawide/pillarbox title, or picture-in-picture content are
  all untested against the new symmetry veto and could in principle be
  rejected incorrectly.
- The extra placement inset is a handful of pixels; effect on fast-moving
  content at the bar edge was not captured, only trailers and one cutscene
  (both largely static during the bar).
