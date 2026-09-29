# YouTube with HDR on: 8-bit pixels under a PQ format ID (v3.5)

With the console's HDR setting on, the YouTube app lit the strip with wrong colors
while it played SDR video: dark gray came out red, blue and green both came out
magenta. HDR video looked right. This is how it was tracked down, and how the plugin
now tells the two apart.

## What was seen

Two captures of the same screen, YouTube's dark UI, once with HDR off and once with
it on (debug build, raw pixel packets):

| | HDR off | HDR on |
|---|---|---|
| Registered format | `0x80000000` (A8R8G8B8_SRGB) | `0x88740000` (A2R10G10B10_BT2020_PQ) |
| Raw pixel word, UI background | `0xff212121` (117 samples) | `0xff212121` (117 samples) |
| Plugin output for it | (33, 33, 33) | (255, 0, 0) |
| Raw pixel word, a darker area | `0xff0f0f0f` | `0xff0f0f0f` |
| Plugin output for it | (15, 15, 15) | (255, 255, 244) |

The bytes in the buffer are the same. Only the format ID changed, and with it the
decode. `0x88740000` is real PQ as far as the plugin knew, so it ran the PQ unpack on
what is actually plain 8-bit ARGB.

## Why that comes out red

The PQ unpack reads bits 29:20 as red. In `0xffRRGGBB` the top byte `0xff` sets bits
29:24, so red is always at least 1008 out of 1023, about 8,700 nits, and clips. Green
and blue get pieces of the wrong bytes. So neutral grays go red, and anything with a
strong blue or green ends up magenta or yellow.

## The channel order was the first thing checked

HITMAN 3's `0x80002200` needed R and B swapped, so it wasn't safe to assume the
order here. Three captures with a solid color on screen, HDR on:

| On screen | Raw word | Read as ARGB | Read as ABGR | Plugin output |
|---|---|---|---|---|
| blue | `0xff000eff` | (0, 14, 255) | (255, 14, 0) | (255, 0, 255) |
| green | `0xff00d700` | (0, 215, 0) | (0, 215, 0) | (255, 0, 255) |
| red | `0xffff1800` | (255, 24, 0) | (0, 24, 255) | (255, 255, 0) |

ARGB matches the screen. ABGR would have shown a blue screen as red. That is the same
layout as the SDR format, so the fix is only which unpack runs, not a new one.

A rotating color wheel video gave the same picture: 234 samples, 15 different words,
every one with alpha byte `0xff`. Read as ARGB they are a clean wheel. The PQ decode
turned those 15 colors into three (yellow, magenta, red).

## HDR video is fine, and nothing tells you when it switches

The report that narrowed it down: with HDR on, HDR video shows correct colors, only
SDR content is wrong. A capture across a switch from SDR video to HDR video agrees.
The words before it were all `0xff` alpha, and after it 585 samples had alpha bytes
from `0x00` to `0xcb`, none `0xff`, which is what real 10-bit data looks like.

So the same registered format holds two different layouts, depending on what the app
is showing at that moment. The plugin can't be told which:

- No registration event at the switch. The capture across it has none, and a longer
  one with nine switches has only the two from launch.
- The flip's buffer index did move from the 0/1 pair to the 2/3 pair at the switch,
  on the same two addresses. That may be how YouTube manages its buffers, and one
  observation isn't enough to build on, so it isn't used.

The only signal left is the pixels.

## The tell: the alpha byte

A real A2R10G10B10 word has a 2-bit alpha in bits 31:30. An alpha *byte* of `0xff`
would need red of 1008 or more, about 8,700 nits, which real content doesn't reach.
Real PQ black is `0xc0000000` or `0x00000000`. 8-bit SDR content has `0xff` on every
pixel. So 8 of 8 sampled words with alpha byte `0xff` means 8-bit, 0 of 8 means PQ.

The existing HDR2200 detector (the smoothness one, see
[hdr-pixel-format](hdr-pixel-format.md)) wasn't reused. It skips a check when there
isn't enough variation (`HDR2200_NOISE_FLOOR`), and the flat `0xff212121` UI is
exactly that case.

## First version, and why it was replaced

The first cut copied the HDR2200 idea: check every 10th pass and switch after 3
agreeing checks. It worked, and it was slow. In the capture across the SDR to HDR
video switch the first check that saw HDR words was at 02:27:58.285 and the PQ
decode was active at 02:27:59.036, about 0.75 s of wrong colors at every change.

There was no reason for it. The check is 8 four-byte reads, and it can run before the
decode function is picked, on the buffer that is about to be decoded, so the answer
is right for that frame. The streak existed to protect against a noisy decision, and
this decision isn't noisy: in that capture all 96 checks were 8 of 8 or 0 of 8.

## What it does now

`detectPq8bitMisregistration()` in `zones.c`, with the vote itself in
`plugin/include/pq8bit_vote.h`:

- Runs on every sampling pass while the active format is `0x88740000`, in
  `sample_thread.c` just before `getUnpackFnForFormat()`.
- Reads the first 8 zone positions (`PQ8BIT_DETECT_SAMPLES`), needs at least 4 of them
  readable.
- 6 or more of 8 with alpha byte `0xff` means 8-bit (`g_pq8bitMode = 1`), 2 or fewer
  means PQ, 3 to 5 leaves the mode as it was. That band is for a stray overlay pixel
  and hasn't been hit in any capture.
- `getUnpackFnForFormat(0x88740000)` returns the A8R8G8B8 unpack when the mode is 1,
  the PQ unpack otherwise.
- Cleared to PQ on a format change in the register hook, so toggling HDR or starting
  another title decides again from the pixels.

There is no state to build up, so a new game session or the app opening gets the
right decode on its first frame.

Telemetry (debug builds): FLK1 flags bit `0x10` is set while the 8-bit decode is
active, and a `PQ8C` packet goes out on every mode change and roughly every 30 checks.
It is 48 bytes after the DDP header, 58 on the wire:

| Bytes | Contents |
|---|---|
| 0-3 | `PQ8C` |
| 4 | vote, int8: +1, -1 or 0 |
| 5 | words read |
| 6 | of those, how many had alpha byte `0xff` |
| 7 | mode after this check |
| 8-39 | the 8 words, uint32 LE, unread slots 0 |
| 40-43 | `g_numZones` |
| 44-47 | low 32 bits of the buffer address |

`tools/flicker_capture.py analyze` reports the flag as "0x88740000 8-bit/PQ decode
changes".

## Confirmed on hardware

One 3 minute capture (09:51:10 to 09:54:05, 4,443 FLK1 passes) starting from launching
the app, then switching between SDR and HDR video:

- The app registered its buffers at 09:51:10.511. The first check was at .789 and
  already 8-bit, and so was the first FLK1 pass at .790. Nothing was seen wrong on
  launch either.
- 9 switches after that (4 to 8-bit, 5 to PQ). In each one the first FLK1 pass
  carrying the new mode came within 0 to 13 ms of the `PQ8C` packet that flipped it,
  so it lands on the same pass.
- 158 checks were reported, 100 were 8 of 8 and 58 were 0 of 8. None fell in the hold
  band.
- No wrong color was seen by eye on the strip, at launch or at any of the switches.

`tools/test_pq8bit_vote.c` runs the vote on words copied from these captures and on
generated PQ, see [tools](../development/tools.md).

## Not verified

- **Only YouTube.** Nothing here was captured on another title. Shadow of the Tomb
  Raider also registers `0x88740000` with HDR on, and its data is presumably real PQ,
  but there is no capture of it in the repo to check against.
- **The false-positive side is mostly generated data.** The real PQ words that were
  checked are YouTube's HDR video, and the rest are built from nits with the ST 2084
  curve. A real PQ frame with red at 8,700 nits or more in 6 of the 8 sampled zones
  would be read as 8-bit. That isn't a scene anyone should be able to make.
- **8-bit content with a non-`0xff` alpha** would be decoded as PQ, which is what
  the plugin did before this, so no worse. Not seen.
- **The 8 samples are the first 8 zones**, next to each other at the start corner. That was enough in
  every capture, including letterboxed and dark scenes, where black is `0xff000000`
  in 8-bit and `0x00...` or `0xc0...` in PQ.
- **A release build was not captured.** The decision is not inside the debug-only
  telemetry, but only debug builds were run.

## HITMAN 3 had the same tell

`pixel_formats.c` said HDR on corrupts the buffer under `0x80002200`, "a
garbage-looking alpha byte, among other things". Captures of HITMAN 3 and RDR2 with
HDR on and off showed it: HDR-on words never have `0xff` in the top byte, HDR-off words
nearly always do, so the same test replaced the smoothness detector's 53 seconds in
v3.6. It needed a guard that the YouTube check doesn't have, for the partial alpha
bytes HITMAN 3 writes while it fades. See
[hdr2200-alpha-detection](hdr2200-alpha-detection.md).
