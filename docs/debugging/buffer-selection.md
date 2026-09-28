# Buffer selection

Which slot of the swap chain the sampler reads, and why it is the previous flip's
slot and not the one the hook just reported. The evidence for the change is in
[framebuffer-flicker.md](framebuffer-flicker.md).

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

`hooks.c` keeps two indices:

- `g_currentDisplayBufferIndex`, the slot the last flip named
- `g_prevDisplayBufferIndex`, the slot of the flip before that

`record_flip_index()` updates both, but only when the index actually changes. Two
hooks reporting the same index for one real flip therefore don't overwrite the
previous slot with the current one. Every flip hook calls it.

In `sample_thread.c`, `liveBufferAddr` is `g_bufferAddrs[g_prevDisplayBufferIndex]`
when a previous slot is known and has a registered address. Otherwise it falls
back to the slot the hook reported. Everything downstream (letterbox scan, HDR
detection, zone sampling) goes through `liveBufferAddr`, so they all read the
finished frame.

`AMBIENT_SAMPLE_LAG` in `ambient_internal.h` controls this. It defaults to 1.
Building with `-DAMBIENT_SAMPLE_LAG=0` brings back the old behaviour, which is
useful for A/B comparison when a title acts differently.

## Cost

One flip of latency: about 33 ms at 30 fps, 16 ms at 60 fps. A filter or a delay
would have added more on top of that, so this replaces those.

## Not verified

These are the cases to check when testing other titles. Only one title had been
captured when this went in.

- A title that resubmits the same buffer index without alternating would leave
  the "previous" slot stale. Nothing seen so far does that.
- Single-buffer and 2-slot swap chains fall back to the reported slot or use the
  other slot. Untested.
- Blank flips (`sceVideoOutSubmitFlip` with index -1, the sentinel). The sampler
  keeps the old no-frame keepalive path for these instead of re-reading the last
  real slot. No title has been seen sending one. After a blank flip the previous
  slot is the sentinel for one flip, so the sampler falls back to the reported
  slot for that flip. I left it like this rather than touch the verified
  `record_flip_index`.
- Only tested on static screens. Latency on fast moving content and video was
  judged by eye, with no capture.
