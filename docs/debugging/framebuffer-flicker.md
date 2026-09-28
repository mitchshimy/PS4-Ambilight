# Framebuffer flicker (v3.3)

The strip flickered on static screens, and flashed on some all-black screens.
Both had the same cause: the sampler was reading a buffer slot the game was
still drawing into. This is the long version of the v3.3 changelog entry, with
the captures and the wrong turns kept in.

The fix itself (read the previous flip's slot) is written up in
[buffer-selection.md](buffer-selection.md). Why the hook-reported slot is unsafe
is in [videoout-hooks.md](videoout-hooks.md).

## Symptom

Assassin's Creed III Remastered, main menu (CUSA11711). SDR (`0x80000000`),
3-slot swap chain, smoothing on, `scan_depth=2`. The menu is a static screen and
the strip flickered steadily anyway.

Separately, God of War Ragnarok flashed the strip on dark loading screens that
were supposed to be all black. An older handoff note (not in this repo) had put
that down to read tearing, a microsecond scale race between the game writing and
the sampler reading.

## First guesses (all wrong)

The first guesses were HDR flip-flopping, film grain, and smoothing being off.
None of them held up once there was data. The lesson from this one was to stop
guessing and measure what the plugin actually sends.

## What the AC3 capture showed

30 seconds on the static menu, 853 sampler passes at 35 ms.

- 32% of passes moved the whole-strip output by more than 4/255 per channel on
  average, and the p95 per-zone change was 81/255. The flicker was already in
  what the plugin sent, so it wasn't WLED, Wi-Fi or the LEDs.
- 18% of passes had at least 5 of the 6 probe zones reading exactly 0,0,0, which
  is a cleared buffer. Another 3% had 1 to 4 zones black (mid-draw). The raw
  pixel word in those reads was exactly `0xC0000000`. Real frames had varied
  words like `0xe3a8ea39`. Zeros from a genuinely dark scene would not look
  like that.
- It wasn't one bad buffer. Slots 3, 4 and 5 were affected about equally in the
  first capture.
- The bad passes came in bursts of about 4, every ~0.70 s. That is a beat
  between a ~30 fps game and the 35 ms sampler, with the phase drifting about
  1.6 ms per pass.
- The flip counter read 59.9 per second while the real frame rate is around 30.
  That fits both flip hooks firing for every real flip. This is an inference from
  the counts, I have not confirmed it in the hook code.
- Ring position test. Share of zone reads more than 25/255 away from the steady
  value on a static screen:

  | Slot read | Bad reads |
  |---|---|
  | lag 0 (slot the hook reports) | 19.3% |
  | lag 1 (previous flip) | 0.0% |
  | lag 2 (two flips back) | 0.0% |

  In all 165 passes where lag 0 got 3 or more zones wrong, lag 1 and lag 2 were
  clean. Ring order was 0 -> 1 -> 2.
- Smoothing did not hide it. One black pass dropped a lit zone from 157 to 56,
  and a burst pulled it down near 8.

Re-capture with the fix in (856 passes, 30 s): output change was 0 on every pass,
all-zero reads 0%, nothing over the 4/255 threshold. The pipeline's raw read
matched the previous-flip buffer on 100% of passes, and the hook-reported slot on
only 77%. The old behaviour would have read 3 or more of 6 zones wrong on 193
passes (22.5%), and the pipeline read steady values on all 193.

One limit on that: lag 1 and lag 2 hold identical pixels on a static menu, so
this capture can't tell them apart. That the pipeline reads lag 1 comes from the
code, not from the capture.

## Second title: God of War Ragnarok load screens

During a load the game resubmits the same buffer index for seconds at a time
while it is still writing to it, so that slot holds a fading intermediate fill
(bright at first, decaying toward black) for most of the load. The old sampler
reads the hook-reported slot, so it could land in that window. That is the same
root cause as the AC3 menu, not read tearing.

Two captures on the same load screen, using the FLK1 probe (see Methodology):

| Capture | Passes | Length | Pipeline's own read | Strip output | Transients found by re-reading all 3 slots |
|---|---|---|---|---|---|
| `gowr.flk` | 818 | 28.8 s | (0,0,0) on all 3,540 zone reads | `deltaSum` 0 on every pass | 33, up to (42,50,63), fading over the session |
| `gowr2.flk` | 715 | 25.3 s | (0,0,0) on every zone read | `deltaSum` 0 on every pass | 27, up to (199,229,247) at the 0.2 s mark |

In every one of the 60 transient events the bright slot was the slot the flip
hook had reported, and never the slot the fix samples. So it is not "the flash
went away and we assume it's the same cause". The transient was caught frame by
frame, always in the slot the fix skips.

In `gowr2.flk` the analyzer's own numbers agree without hand-checking: lag 0
wrong on 0.5% of zone reads, lag 1 wrong on 0%, and the pipeline's raw read
matched lag 1's buffer on 100% of passes.

The first GOWR capture used a probe with a bug in it: `curIdx` was recovered from
the already-fixed slot instead of the raw hook-reported one. That happened to
still prove the point by hand, but it quietly breaks `flicker_capture.py`'s ring
lag math, so I fixed the probe and captured again (that is `gowr2.flk`).

## Rejected along the way

- HDR flip-flop, film grain, smoothing off: guessed at first, not the cause.
- Read tearing as the GOWR explanation: it is a hook-timing problem, not a
  microsecond race between reader and writer.
- The earlier stale-flip guard (reverted in 80c2873) was aimed at a different
  problem and never covered this.
- A filter or delay on the sampler: the previous-slot read replaces it and only
  costs one flip of latency, where a delay would stack more on top.

## Still open

The letterbox cutscene flicker in the same title. No capture has been taken, so
these are guesses:

- The detected bar depth flipping between quantized bands on dark scenes, with
  each commit rebuilding the zone geometry and dropping smoothing
  (`g_smoothedRgbValid = false`).
- Edge zones landing inside the bar because the depth quantizes down (a 138 px
  bar quantizes to 132).

Retest with the v3.3 fix in place before assuming this is a separate bug.

## Methodology

The FLK1 probe is compiled in with `make DEBUG=1` (the `__FINAL__==0` build). It
sends one 132-byte packet per sampler pass to the `[dev]` channel. Set `dev_ip`
and `dev_logging=true` in the ini to turn it on. Each packet carries:

- total output change since the previous pass
- flip index and flip count
- the HDR/SDR decision
- sampler timing
- for 6 zones: the pipeline's raw read, the color actually sent, and the same
  zone re-read straight from the 3 registered slots

Capture a static screen for 30 seconds, then analyze it:

```
python tools/flicker_capture.py capture menu.flk --seconds 30
python tools/flicker_capture.py analyze menu.flk
```

Read the report in this order:

1. Output change. Is the flicker in what we send at all? If the output is steady,
   the problem is downstream of the plugin.
2. All-zero reads and burst spacing. A cleared buffer, and a beat with the frame
   rate?
3. The ring-lag table. Which slot is clean?

Things that will trip you up:

- Don't run `udp_ground_truth_listener.py` at the same time. Both use port 4048.
- The analyzer's verdict thresholds were guesses made before there was real data.
  Read the numbers, not just the verdict line.
- The lag table is always relative to the slot the hook reports, so on a build
  with the fix, lag 0 still shows ~20% bad even though the plugin no longer reads
  it. The analyzer works out which lag the pipeline actually read (its raw value
  against each lag's buffer) and when that is a clean older lag it prints "FIX
  ACTIVE AND WORKING".
- On a static screen lags 1 and 2 look the same.
