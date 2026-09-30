# Sample lag 2 and the boot flash (v3.7)

The v3.3 fix reads the previous flip's slot. That was enough at 30 fps and not at
about 58 fps. This is the long version of the v3.7 changelog entry: Red Dead
Redemption flickering at lag 1, a lag setting that silently did nothing above 1, the
God of War Ragnarok flash that lag 2 then showed, and a filter that was tried and
taken out.

Background on the slots and `AMBIENT_SAMPLE_LAG` is in
[buffer-selection.md](buffer-selection.md). The v3.3 investigation this follows is
[framebuffer-flicker.md](framebuffer-flicker.md).

## Symptom

Red Dead Redemption (the first one), with the console's HDR setting on and off, the
same both ways. Steady flicker, and flashes of colors that had nothing to do with the
picture. Everything else that had been tested was fine, so it looked title specific.

The format was `0x80002200` for the whole capture and the HDR vote (the `HDRV` packets)
stayed on SDR, with the alpha byte `0xff` on every check and no mode change. That ruled
out the format detection as the cause, which fits it looking the same with HDR on and off.

## Lag 1 was not enough

A capture with `flicker_capture.py` showed a title running at about 58.5 fps (single
fire flip hooks, so the flip counter is a real frame rate, unlike AC3's doubled
count). It is the first title captured at 60 fps class. Two things stood out:

- The pipeline's read matched the lag 1 slot on 99.2% of passes, so the v3.3 code was
  doing what it should.
- The largest output jump between two passes was 18,670 (summed over the strip), and
  two probe zones showed the buffers disagreeing with each other by about 20 out of
  255, and the pipeline's read disagreeing with a re-read of the same buffer by 20 to
  22. That is a buffer being written while it is read, not a stale one.

The reading was that at 30 fps one flip gave the GPU time to finish the slot, and at 58
fps with a heavier frame it didn't. That is the case v3.3 listed as untested: "a 60 fps
title with a heavier GPU load is the real test of whether lag 1 is enough".

## The setting that did nothing

The test was to build with `AMBIENT_SAMPLE_LAG` set to 2. It changed nothing, three
times. The captures at "lag 2" were byte-identical to the lag 1 ones, with the
pipeline's read matching the lag 1 slot at about 99% each time.

The macro was only ever tested as `#if AMBIENT_SAMPLE_LAG >= 1`. Two satisfies that, and
the code tracked exactly one step back (`g_prevDisplayBufferIndex`), so there was nothing
for a `>= 2` branch to read. The setting was a knob wired to nothing, and I had handed it
over as if it worked. Two of the capture rounds were spent on that.

The first real lag 2 build had a second bug of the same kind: the header default was
still 1, so the files that "fixed" it still ran at lag 1 until the default was changed.
The capture that finally tested lag 2 was the first one where the pipeline's read
matched the lag 2 slot, at 100.0%. The same scene then showed:

| | lag 1 | lag 2 |
|---|---|---|
| Largest output jump | 18,670 | 1,319 |
| Passes over the flash threshold | 43 | 0 |
| Analyzer verdict | output is not steady | output is steady |

The all-zero read rate stayed high (around 82%) and that is not a symptom: the scene was
dark, and the output is steady. The flashes were gone by eye. The other reading I had
been holding, that RDR1's checkerboard renderer registers an intermediate target as the
display buffer, was never needed.

## What lag 2 changed in `hooks.c`

`record_flip_index()` now keeps three slots: `g_currentDisplayBufferIndex`,
`g_prevDisplayBufferIndex` and `g_prevPrevDisplayBufferIndex`. All three only move when
the reported index actually changes, so the two flip hooks firing for one real flip
don't shift the history twice. `sample_thread.c` picks the previous one for lag 1 and
the one before it for lag 2.

## Lag 2 and a flash in God of War Ragnarok

With lag 2 in, GOWR flashed on load screens where lag 1 hadn't. First guesses, both
wrong or unproven:

- **A frozen reference.** The docs say GOWR resubmits the same buffer index for seconds
  while still writing to it. The history variables only move on a real change, so they
  freeze during that, and a frozen lag 2 slot is one step older than a frozen lag 1 slot.
  It fit, but the capture didn't show a stale bright slot being read.
- **The smoothing trail.** The first flash capture had all three buffers and the raw
  read at (0,0,0) on every flagged pass, while the output decayed from (129,179,214)
  over about 12 passes. So the raw data was already black at the first captured sample,
  and only the smoothing was still catching up from something bright before the capture
  started. That points at a single bad read stretched by smoothing, not a buffer that
  stays wrong.

The capture also started too late to see the bright read, and the flash was
intermittent (one launch in seven), which made it hard to catch.

## A persistence gate, tried and removed

The general shape looked like one anomalous read surrounded by good ones. So the first
attempt was a gate: only forward a zone's new value once it has held for 2 consecutive
passes, otherwise repeat the last accepted one. Simulated cases behaved (a one pass
spike absorbed, a real change delayed by one pass, a slow fade passed straight through),
and it cost about one sampler pass (33 to 36 ms at 30 Hz) on every real change.

It did not work. A flash still got through on hardware. Once the capture showed why, it
was a design gap and not bad luck: a stale slot that stays stale is read the same on
every pass, so it "persists" and the gate trusts it. The gate answers "did the value
repeat", and the thing that is wrong repeats.

## The boot capture

The next flash capture was different in one way that mattered: `seq` was 0 on the first
packet and the flip counter was 4. Sampling had just started, at game boot. In that
first pass:

- zone 57 had a raw read of (188,223,244)
- all three buffers, re-read in the same pass, were (0,0,0)
- the output for that zone was (107,189,249) and then decayed to (0,0,0) over 8 passes

The bright read was the first frame the sampler ever took, which is also the frame
that seeds smoothing (there is no earlier value to blend from, so it starts at that
color). And it came from the wrong place: `liveBufferAddr` starts as the slot the hook
just reported, which is the one the GPU is about to render into, and is only replaced by
the older slot once `prevIdx` is valid. At lag 2 that takes three different reported
indices. Until then the sampler read the current, half-drawn slot. The buffer
re-read in the same pass being black means that slot was cleared or being redrawn as
it was read.

That also explains why it was intermittent: it depends on whether the very first slot is
mid-draw at the moment the sampler happens to run its first pass.

## What v3.7 does

Two changes, both in `sample_thread.c`.

**No frame until the history exists.** While the lag's tracked slot is still the
sentinel, `liveBufferAddr` is set to 0, which takes the existing no-frame path (the
WLED heartbeat resends the last color, nothing new goes out). The first frame that is
read comes from a slot that has finished drawing. A title that really never changes
slot, say a single-buffer swap chain, would never build history, so after 30 passes
(about 1 s at 30 Hz) the old fallback is used and it lights up as it did before.

**Read-verify.** A zone whose new read differs from the last accepted one by more than 8
in any channel is read again from the same buffer after the whole pass has been
sampled. If the two reads differ by more than 8, the buffer was changing under the
sampler, so the new value is dropped and the last accepted one kept. A zone can be held
at most 3 passes in a row, after which the newest read is taken, so a title that rewrites
its buffer every pass can't freeze the lights. Nothing stateful runs before the decision:
color processing and the dark threshold (which has hysteresis per zone) now run once, on
the value that was accepted, so a dropped read never moves them.

Costs:

- **Static screen:** no zone jumps, so nothing is re-read.
- **A real change:** a stable buffer agrees with itself, so the value is taken on the
  first pass with no delay. If the whole scene changes, every zone is sampled twice on
  that pass. That cost was not measured on the console.
- **Latency:** lag 2 is one flip more than v3.3, about 33 ms at 60 fps and 66 ms at 30.
- **Game start:** the strip lights a few passes later.

## Result

- RDR1: flicker and the color flashes gone by eye, and the analyzer's verdict "steady".
- GOWR: 7 launches in a row without a flash after the change, where about one in seven
  flashed before. A 5.7 s capture from the first pass (155 passes) had no output change
  on any pass, and its first pass already had a flip counter of 8, so the sampler had
  waited through the first flips.
- The other games checked by eye (ones that never had these problems) show no flicker.

## Not verified

- The boot mechanism is from two captures and a result. No capture caught the bright
  read together with the history state, so "the sampler read the current slot" is an
  inference from the flip counter and the buffers disagreeing, not a logged fact.
- Read-verify does not catch a stale slot that isn't being written while it is read. It
  only sees a buffer that changes under the sampler.
- The first frame after a reset (boot, a letterbox commit, a settings reload) is taken
  without the re-read, because there is no earlier value to keep.
- No release build was captured, and RDR1 is the only 60 fps title.
- The 30 pass fallback for a title that never changes slot has been run in a test of the
  selection logic only. No real title has been seen doing it.
- A title that takes longer than the wait to start flipping would get the old fallback
  and could see the boot flash. The number is `LAG_HISTORY_WAIT_PASSES` in
  `sample_thread.c`.
- `AMBIENT_SAMPLE_LAG` above 2 behaves as 2. There is no third tracked slot.

## Methodology

Same as [framebuffer-flicker.md](framebuffer-flicker.md#methodology): a debug build with
`dev_logging=true`, then `flicker_capture.py capture` and `analyze`. Two things to know
when reading a capture from a v3.7 build:

- FLK1's `curIdx` is still the slot the hook reported, not the one that was sampled, so
  the analyzer's per-lag columns can tell "sampling the lag it should" from "not".
- Passes where no frame is read (the history wait) send no FLK1 packet, so a capture
  taken at boot begins a few passes after the sampler starts.

When a capture shows a fixed lag not matching the columns it should, check the build
before anything about the title. Three of the rounds here were the build.
