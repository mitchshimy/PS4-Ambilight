# Title compatibility notes

What has actually been seen on specific titles. This is not a compatibility list,
just the titles that taught something. Anything not here hasn't been captured, so
"works" means "nobody has reported a problem", not "verified".

| Title | What was seen | Where to read more |
|---|---|---|
| Assassin's Creed III Remastered (CUSA11711) | SDR `0x80000000`, 3-slot swap chain, about 30 fps. Static menu flickered until v3.3. A letterboxed cutscene still flickers (open). | [framebuffer-flicker](framebuffer-flicker.md) |
| God of War Ragnarok | Dark loading screens flashed the strip until v3.3. The game resubmits the same buffer index for seconds while still writing to it. | [framebuffer-flicker](framebuffer-flicker.md#second-title-god-of-war-ragnarok-load-screens) |
| Shadow of the Tomb Raider | Never lit the strip, in SDR (`0x80000000`) or HDR (`0x88740000`), until v3.1. It calls the `ForWorkload` flip entry point directly. Two swap-chain slots. | [videoout-hooks](videoout-hooks.md) |
| HITMAN 3 | Reports `0x80002200`. That ID is A8B8G8R8 with HDR off and PQ data with HDR on, and it never re-registers. The plugin decides live, and in testing the first decision took about 43 s from boot. | [hdr-pixel-format](hdr-pixel-format.md) |
| PS4 Ambilight companion app (`SHMY00091`) | Not a game, but GoldHEN loads the plugin into it too. The plugin skips it since v3.2. | [architecture](../architecture.md) |

## Patterns worth knowing

- **Menus and static loading screens throttle or stop flipping.** Don't build a
  timeout on flip gaps. The v2.7.3 guard did and made idle screens flicker, see
  [black-screen-shows-color](black-screen-shows-color.md).
- **Some titles re-submit the same buffer index** for a long stretch while still
  drawing into it (GOWR loads). The previous-slot read in
  [buffer-selection](buffer-selection.md) exists because of this, and it is also the
  reason a title that resubmits without alternating is listed there as untested.
- **A title can skip the flip wrapper.** If the flip count stays at 0 while
  registration looks normal, check for an unhooked flip entry point.
- **Format IDs can be ambiguous.** Most titles re-register with a new ID when HDR
  turns on. HITMAN 3 doesn't.
- **A black loading screen gives the HDR detector nothing to work with.** It just
  waits.

## Adding a title

When a new title misbehaves, the order that has worked so far:

1. Did the register hook fire, and was the format recognized? If not, it's a
   [pixel format](hdr-pixel-format.md) problem.
2. Is the flip counter moving and is `g_currentDisplayBufferIndex` off its sentinel?
   If not, see [videoout-hooks](videoout-hooks.md#if-another-title-never-lights-up).
3. Is the output steady when it should be? Capture it with `flicker_capture.py`, see
   the [methodology](framebuffer-flicker.md#methodology).
4. Note what you found here, with the title id and format, even if the answer is
   "nothing wrong".

Things still worth checking on a new title, from the "not verified" list in
[buffer-selection](buffer-selection.md#not-verified): single-buffer and 2-slot swap
chains, blank (-1) flips, and fast-moving content.
