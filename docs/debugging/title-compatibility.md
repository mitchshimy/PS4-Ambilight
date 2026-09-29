# Title compatibility notes

What has actually been seen on specific titles. This is not a compatibility list,
just the titles that taught something. Anything not here hasn't been captured, so
"works" means "nobody has reported a problem", not "verified".

| Title | What was seen | Where to read more |
|---|---|---|
| Assassin's Creed III Remastered (CUSA11711) | SDR `0x80000000`, 3-slot swap chain, about 30 fps. Static menu flickered until v3.3. A letterboxed cutscene still flickers (open). | [framebuffer-flicker](framebuffer-flicker.md) |
| God of War Ragnarok | Dark loading screens flashed the strip until v3.3. The game resubmits the same buffer index for seconds while still writing to it. | [framebuffer-flicker](framebuffer-flicker.md#second-title-god-of-war-ragnarok-load-screens) |
| Shadow of the Tomb Raider | Never lit the strip, in SDR (`0x80000000`) or HDR (`0x88740000`), until v3.1. It calls the `ForWorkload` flip entry point directly. Two swap-chain slots. | [videoout-hooks](videoout-hooks.md) |
| HITMAN 3 | Reports `0x80002200`. That ID is A8B8G8R8 with HDR off and PQ data with HDR on, and it never re-registers. The smoothness detector took 43 to 53 s from boot to decide. Since v3.6 the alpha byte decides on the first frame with data (4.4 to 4.8 s in captures, the game's own loading time). Fades to and from black in SDR write partial alpha bytes, which the vote holds on. HDR changes only apply after a restart. | [hdr2200-alpha-detection](hdr2200-alpha-detection.md), [hdr-pixel-format](hdr-pixel-format.md) |
| Red Dead Redemption 2 | Same `0x80002200` behavior as HITMAN 3, HDR on or off, no re-registration. Decided on the first frame with data since v3.6 (1.95 s with HDR on, 0.34 s off). HDR on is very dark, nearly every word has a top byte of `0xc0` to `0xc4`. An HDR change only applies after a restart. | [hdr2200-alpha-detection](hdr2200-alpha-detection.md) |
| YouTube (the PS4 app) | With HDR on it registers `0x88740000`, but the buffer holds 8-bit ARGB while SDR video or its own UI is showing and real PQ for HDR video, with no re-registration between. Wrong colors on SDR content until v3.5. | [youtube-hdr-8bit](youtube-hdr-8bit.md) |
| Netflix (the PS4 app) | Works in SDR and with the console's HDR on, with nothing title-specific in the plugin. That is from testing it, no capture of it is kept in the repo, so the formats it registers aren't recorded here. | none yet |
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
- **A registered format can be wrong about the content.** YouTube with HDR on keeps
  `0x88740000` while it draws 8-bit ARGB for SDR video. If the raw words in a capture
  look like plain `0xffRRGGBB` under a PQ format, check the alpha byte before
  suspecting the decode.
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
