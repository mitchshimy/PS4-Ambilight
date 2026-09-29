# HDR and pixel format `0x80002200` (v2.7 to v2.7.2)

One pixel format ID turned out to mean two different things depending on whether
HDR was on. This is how that was worked out, and how the plugin tells the two apart
at runtime.

## How the plugin reads formats

The plugin doesn't assume a format. It takes it from the buffer registration hook
and looks up an unpack function in `getUnpackFnForFormat()` (`pixel_formats.c`).
If a format has no unpack function the plugin sends nothing for that title rather
than guessing, which is deliberate. The formats it handles:

| Format | Meaning |
|---|---|
| `0x80000000` | A8R8G8B8_SRGB, the common SDR format |
| `0x88000000`, `0x88060000` | SDR A2R10G10B10 and A2R10G10B10_SRGB |
| `0x88740000` | A2R10G10B10_BT2020_PQ, decoded as real PQ (an earlier version truncated it as if it were SDR, which was a bug). YouTube also puts plain 8-bit ARGB under this ID, checked per frame since v3.5, see [youtube-hdr-8bit](youtube-hdr-8bit.md) |
| `0x80002200` | A8B8G8R8_SRGB with HDR off, or A2R10G10B10_BT2020_PQ with HDR on, see below |

## v2.7: the format nobody handled

`0x80002200` had been identified by name earlier (HITMAN 3, matched against
fpPS4's enum table), but it was never wired into `getUnpackFnForFormat`, so the
title got NULL back and the plugin correctly sent nothing.

The investigation, which lived mostly in the detile-verify probe repo, ruled out
tiled versus linear addressing, the wrong swap-chain slot, multiple flips per frame
and a second video-out target. In the end it was HDR. With HDR off the buffer
decodes correctly with plain tiled addressing once R and B are swapped relative to
the existing A8R8G8B8 unpack (so it is `unpackA8B8G8R8_to_rgb888`).

It was confirmed two ways. A probe capture was compared pixel for pixel against a
real screenshot at 5 known coordinates, with single digit to low teens RGB error at
all 5 at once, the first hypothesis in the whole investigation that matched at every
point. Then colors were checked live on real hardware.

v2.7 was confirmed for HDR off only. The plugin had no way to know the console's HDR
setting.

## v2.7.1: the same ID means two things

On this title, HDR on registers the same format ID `0x80002200`, but the buffer now
holds A2R10G10B10_BT2020_PQ data. Most titles re-register with a different format ID
when HDR engages. This one never does, and no hooked call reveals which mode is
active.

The direct route, hooking `sceVideoOutAddBufferHdrPrivilege`, was looked at and
ruled out. There was no reference implementation of it anywhere to derive the call
signature from, and a wrong guess in a hook is not something to risk.

So the plugin detects it from the pixels. `detectHdr2200Format()` in `zones.c`
decodes a few sampled pixels both ways and compares how smooth each result is. Real
image content decodes smoothly under the right hypothesis and noisily under the
wrong one, so the lower total variation wins.

Tuning, all in `pixel_formats.c`:

| Constant | Value | Why |
|---|---|---|
| `HDR2200_DETECT_SAMPLES` | 8 | capped no matter how many zones are configured |
| `HDR2200_DETECT_INTERVAL` | 60 | run the check every 60th pass, not every one |
| `HDR2200_STREAK_THRESHOLD` | 4 | consecutive agreeing checks needed before switching |
| `HDR2200_NOISE_FLOOR` | 24 | skip a check when the winning variation is below this, e.g. a black loading screen |

It is deliberately cheap so the plugin doesn't compete with the game for CPU, it
reuses zone coordinates that already exist, and it is only ever called for this one
format. Every other format's path is untouched.

The method was validated offline first, in `decode_verification_dump.py`, against
real captures before being ported to C.

## v2.7.2: it worked, it just took a while

The first live test looked broken. An offline Python replica of the same algorithm
converged cleanly on a real capture, which pointed to either a wiring bug or
impatience. Telemetry (`sdrTv`, `hdrTv`, `streak`, `isHdr`) settled it: the live
numbers matched the offline prediction exactly, with a clean accumulate-and-flip.
The flip just took about 43 seconds from boot, because a black loading screen
produces no valid checks at all, and then several more seconds are needed to build a
streak. Watching for less than that looks the same as "broken".

The obvious follow-up worry was a dark gameplay scene flipping it back to SDR. A
real 6.5 minute gameplay session (not menus) had exactly one flip, the correct
initial SDR to HDR one. The margin between the two hypotheses got much smaller in
dark, busy scenes (down to about 2.3x apart, versus 15 to 40x on simple menu
content) but never came close to reversing. That's real evidence but not proof for
every scene.

## v3.5: `0x88740000` can hold 8-bit data too

The same kind of problem showed up the other way round in YouTube. With HDR on it
registers `0x88740000`, real PQ, but while it plays SDR video the buffer holds
8-bit A8R8G8B8, and it goes back to real PQ for HDR video without re-registering. The
smoothness detector above wasn't used for it, since it skips flat frames like
YouTube's dark UI and this one needed to decide on the frame being decoded, not a
few seconds later. `detectPq8bitMisregistration()` reads the alpha byte of 8 zone
words every pass instead: `0xff` cannot be a real 10-bit word without red at 8,700
nits. Full write-up in [youtube-hdr-8bit](youtube-hdr-8bit.md).

## If colors look wrong in an HDR title

- First check what format the title registered. The `[dev]` telemetry reports it on
  change (debug builds).
- If it is `0x80002200`, wait a minute after boot before deciding it's broken.
- If it is `0x88740000` and the colors are only wrong during SDR content, look at
  FLK1 flags bit `0x10` and the `PQ8C` packets (debug builds). The bit should be on
  while the buffer is 8-bit.
- If it is a format that isn't in the table above, the plugin sends nothing on
  purpose, and it needs a new unpack function.
