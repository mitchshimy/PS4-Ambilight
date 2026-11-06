# PS4 Pro tiling: the console model and the base-vs-Neo check

The plugin reads the game's picture straight out of memory, where it is stored in tiles, not in
rows. The layout it undoes is the one the original PS4 and the Slim use, called base here. It was
confirmed on a console, and `docs/code-comments.md` records base as the settled answer for that
hardware. A PS4 Pro (Neo mode) lays a picture out differently. `tools/ps4_detile_2dthin.c` has a
Neo parameter set, but it was never compared with a capture from a real Pro, and the plugin had no
Neo code at all. The maintainer has no Pro to test on.

So v4.0 does not guess. It adds two lines to the [report file](report-file.md) so that one Pro
owner, running one game, produces the evidence. **It changes nothing about how the strip is
driven.** The sampler still reads with the base layout everywhere.

A Pro title can only reach the tiling check if the plugin hooks it first, and titles that run in Pro mode load a different
Gnm driver from the one the plugin looked for; see [neo-mode-gnm-driver](neo-mode-gnm-driver.md). Once hooked, the first Pro-mode
titles (Red Dead Redemption, Devil May Cry 5, NFS Hot Pursuit Remastered, YouTube, Netflix) showed
correct colours with the base layout. That was seen by eye, not measured, so the check below is still the way to settle it.

## What was added

- `console:` in the report, from `sceKernelIsNeoMode` (looked up at load like the other optional
  kernel calls; if libkernel has no such function the line says `unknown`).
- `tiling_check:` in the report, which reads a fixed set of points through both layouts and says
  which one the picture fits.
- `kParamsNeo` in `tiling.c`, with the pipe and bank math for it. It is used only by the check.
  The base results are unchanged on every pixel (host test).

Both lines exist only when `report_file=1`; with it off the check never runs and reads nothing.
The check lives in `plugin/source/tile_check.c`, which the plugin builds and the shell feed does
not. The scoring and the verdict are in `plugin/include/tile_select.h`, with no SDK calls, so a PC
test runs them.

## Why it compares steps across a tile edge

A picture is stored as 8x8 pixel micro tiles, and base and Neo store the pixels inside one micro
tile the same way. What differs is where each tile goes. Read with the wrong layout, every 8x8
block comes out whole but from the wrong place. Two neighbours inside a tile still look like
neighbours, so a check on those can't tell the layouts apart. Two pixels on either side of a tile
edge come from unrelated blocks, and they only look like neighbours if the layout is right.

For each layout the check reads, on a fixed grid of 26 by 16 points:

- **edge pairs**: one pixel on each side of a tile edge, horizontally and vertically (832 pairs);
- **inner pairs**: two pixels inside one tile (832 pairs).

Each pair's step is the sum over R, G, B of the absolute difference, capped at 96 so a few hard
edges can't decide the result. The picture's own inner step is the reference: both layouts' inner
means, averaged. A layout fits when its edge step stays near that reference.

## The verdicts

| `tiling_check` | Meaning |
|---|---|
| `base fits` / `Neo fits` | one layout's edge step is within 1.00 of the reference, and the other is at least 2.00 above it and at least three times as far above it |
| `flat picture` | both edge steps are under 2.00, so there is nothing to judge by (a black loading screen) |
| `unclear` | the layouts are too close, or the better one still looks scrambled (white noise, or a layout that is neither) |
| `no data` | fewer than 64 pairs were readable, or no check has run yet |

Steps are on a 0 to 255 scale. It reads only inside `g_readableLimit`, the bound the sampler uses.
It runs once every 120 sampler passes, stops at the first `base fits` or `Neo fits`, and gives up
after 30 tries, so a game that opens on a dark screen gets several chances. The report prints the
last result.

## What was checked, and what was not

On a PC, `tools/test_tile_select.c`:

- the base offsets equal the pre-v4.0 code and the standalone port on all 2,073,600 pixels;
- the Neo offsets equal the standalone port's Neo set on all of them;
- both layouts place every pixel in its own spot inside the padded buffer (base 1920x1088, Neo
  1920x1152), with no collisions, which would show a wrong parameter;
- a synthetic picture tiled for either layout is called for that layout, with and without
  letterbox bars; black, one colour and white noise are never given a layout;
- over 48 random pictures none was called the wrong layout and about three quarters the right one
  (90 pictures in the tuning sweep: 63 right, 27 flat or unclear, 0 wrong). Pictures with a lot of
  noise or little detail come out `unclear`;
- nothing is read past `g_readableLimit`, with a guard page behind the buffer, and the whole test
  runs clean under AddressSanitizer and UBSan.

Not checked, because it needs a Pro:

- **Whether the Neo parameters are right.** The test shows they agree with the standalone port and
  form a clean mapping, which is the same source of truth, not an independent one. A real Pro
  capture is the only check.
- **Whether a Pro game's picture is in that layout at all**, or whether a game that isn't
  Pro-enhanced is laid out as base while running on a Pro. The check is meant to find out.
- **The thresholds on real frames.** They were tuned on synthetic pictures. A real game frame has
  more structure than those, which should help, but it is not measured.
- **`sceKernelIsNeoMode` on a console.** Its name and signature match OpenOrbis's `libkernel.h`
  and the library stub, but it was not run on a console, and whether it answers for the Pro's
  hardware or only for a title running in Pro mode is not confirmed. If it is missing the line
  says `unknown` and nothing else is affected.

The plugin and the shell feed, release and debug, were built with the same OpenOrbis toolchain and
patched GoldHEN SDK that CI uses, with no new warnings.

## Reading a Pro report

| `console` | `tiling_check` | What it means |
|---|---|---|
| PS4 Pro | `base fits` | this title reads correctly on a Pro with the plugin as it is |
| PS4 Pro | `Neo fits` | the plugin reads this title with the wrong layout on a Pro; the Neo path is needed |
| PS4 Pro | `unclear` or `flat picture` | ask for another report taken in gameplay, with something detailed on screen |
| PS4 Pro | `unclear` in several games | neither layout fits, so the Neo set is probably wrong; ask for a frame dump |
| base PS4 | `base fits` | as expected, and a check that the check itself works |
| base PS4 | `Neo fits` | something is off with the check or the console model; this would be worth a bug report |

Ask a Pro owner to report two games, one with a Pro-enhanced mode and one without, since the
layout may differ between them.

## What comes after

Not done, because it needs what the table above produces:

- If `Neo fits` shows up on a Pro, pick the layout at run time. The offset call takes its
  parameters as an argument, so the sampler's call sites in `sample_thread.c`, `zones.c` and
  `letterbox.c` would pass the chosen set, and `buffer_guard.c` would size its read for the Neo
  buffer (`NEO_PADDED_BUFFER_BYTES`).
- An optional one-frame dump, to detile offline with
  `tools/ps4_detile_2dthin.c`, if the check is `unclear` on a Pro.
- Until then, a Pro whose picture needs the Neo layout would show wrong colors with no notice.

Host test: `tools/test_tile_select.c`, see [tools](../development/tools.md#test_tile_selectc).
