# Buffer selection

Which slot of the swap chain the sampler reads, and why it is a slot from one or two
flips back and not the one the hook just reported. The evidence for the change is in
[framebuffer-flicker.md](framebuffer-flicker.md), and for going from one flip back to
two in [sample-lag-and-boot-flash.md](sample-lag-and-boot-flash.md).

## Background

The game registers a small ring of display buffers with the console
(`sceVideoOutRegisterBuffersPtr` is hooked, and `g_bufferAddrs[]` holds the
address of each slot). Every frame it submits a flip that names one of those
slots. The plugin hooks the flip calls and the sampler thread reads pixels out of
one of the registered buffers on its own clock.

The flip hooks run when the game submits the flip, not when the frame is scanned
out. So the index they report is the slot the GPU is about to render into. At the
moment the sampler looks, that slot can be cleared or half drawn.

## What the sampler does

`hooks.c` keeps three indices:

- `g_currentDisplayBufferIndex`, the slot the last flip named
- `g_prevDisplayBufferIndex`, the slot of the flip before that
- `g_prevPrevDisplayBufferIndex`, the slot of the flip before that one (v3.7)

`record_flip_index()` updates them, but only when the index actually changes. Two
hooks reporting the same index for one real flip therefore don't shift the history
twice. Every flip hook calls it.

In `sample_thread.c`, `liveBufferAddr` is `g_bufferAddrs[g_prevPrevDisplayBufferIndex]`
at the default lag of 2, or `g_prevDisplayBufferIndex` at lag 1, when that slot is
known and has a registered address. Everything downstream (letterbox scan, HDR
detection, zone sampling) goes through `liveBufferAddr`, so they all read the
finished frame.

Until that slot is known the sampler reads no frame (`liveBufferAddr` is 0, the
existing no-frame path, so the strip keeps its last color). Before v3.7 it fell back
to the slot the hook reported, and that first read, taken while the game was still
drawing into it, flashed at boot in God of War Ragnarok. If the history hasn't
appeared after 30 passes (`LAG_HISTORY_WAIT_PASSES`, about 1 s at 30 Hz) the old
fallback is used, so a title that never changes slot still lights up.

`AMBIENT_SAMPLE_LAG` in `ambient_internal.h` controls this. It defaults to 2 since
v3.7 (it was 1 from v3.3). `-DAMBIENT_SAMPLE_LAG=1` gives the v3.3 behaviour and
`-DAMBIENT_SAMPLE_LAG=0` the original one, which is useful for A/B comparison when a
title acts differently. Values above 2 behave as 2, there is no third tracked slot.
Before v3.7 a value of 2 compiled to the same code as 1.

On top of the slot choice, a zone that jumps is read a second time from the same
buffer and dropped if the two reads disagree, see
[sample-lag-and-boot-flash.md](sample-lag-and-boot-flash.md#what-v37-does).

## Cost

Two flips of latency at the default: about 66 ms at 30 fps, 33 ms at 60 fps. Lag 1 was
enough at 30 fps and not at 58 fps (Red Dead Redemption). A filter or a delay would
have added more on top of that, so this replaces those. The strip also lights a few
passes later at game start while the history fills.

## Not verified

These are the cases to check when testing other titles. Only one title had been
captured when this went in.

- A title that resubmits the same buffer index without alternating would leave
  the lagged slot stale. GOWR does this during loads and its output was steady
  through them, but only the one title has been captured doing it.
- Single-buffer and 2-slot swap chains fall back to the reported slot or use the
  other slot. Untested.
- Blank flips (`sceVideoOutSubmitFlip` with index -1, the sentinel). The sampler
  keeps the old no-frame keepalive path for these instead of re-reading the last
  real slot. No title has been seen sending one. After a blank flip the lagged
  slots are the sentinel for one or two flips, so the sampler now waits (up to 30
  passes) instead of falling back to the reported slot. I left `record_flip_index`
  alone rather than touch the verified code.
- Latency on fast moving content and video was judged by eye, with no capture. Lag 2
  adds one flip to that.
- The 30 pass fallback for a title that never gets history has only been run in a test
  of the selection logic, no title was seen taking it.
