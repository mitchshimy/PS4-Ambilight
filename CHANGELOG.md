# Changelog

All notable changes to PS4 Ambilight (the GoldHEN plugin and its
companion app) are documented here, newest first.

## Plugin

### v3.7
- Fixed flicker and flashes of unrelated colors in Red Dead Redemption (about 58 fps, the
  same with the console's HDR setting on or off). `AMBIENT_SAMPLE_LAG` had only ever been tested as `>= 1`, so setting it
  to 2 compiled to exactly the behavior of 1: nothing tracked a second step back. At 30 fps
  one flip of lag was enough for the GPU to finish the slot, at 58 fps it wasn't. Three
  captures taken at "lag 2" before this were byte-identical to the lag 1 ones, which is how
  the no-op showed. `hooks.c` now tracks `g_prevPrevDisplayBufferIndex` (two real flips
  back, same only-on-a-real-change rule as the first) and the default is 2. **Confirmed on
  real hardware**: the largest output jump in the same RDR1 scene went from 18,670 to
  1,319, the analyzer's verdict went from "output is not steady" to "output is steady",
  the pipeline's read matched the lag 2 slot on 100% of passes, and the flashes were gone
  by eye. Costs one more flip of latency than v3.3: about 33 ms at 60 fps, 66 ms at 30.
- Fixed a flash at game boot that showed up in God of War Ragnarok once the default went
  to 2. Until the lag history exists (three different buffer indices at lag 2) the sampler
  used to fall back to the slot the hook had just reported, which is the one the GPU is
  about to draw into, and that first read also seeds the smoothing with nothing to fall
  back to. In the capture that caught it, the first pass after boot (`seq` 0, flip counter
  4) read a bright value in one zone while all three buffers re-read black in the same
  pass, and the output then decayed over about 8 passes; a second capture showed the same
  decay from its first pass. The sampler now reads no frame until the history exists and
  the strip stays as it was. A title that never changes slot gets the old fallback after
  30 passes (about 1 s at 30 Hz), so it isn't left dark for the whole game. The flash was
  intermittent (it showed in one launch out of seven), so this rests on the mechanism in
  the captures and on the result: 7 launches in a row without a flash, and a 5.7 s capture
  from the first pass with no output change on any of its 155 passes. The strip lights a
  few passes later at game start (estimated at about 100 ms at GOWR's flip rate, not
  measured).
- A zone whose new read jumps by more than 8 (per channel, 0 to 255) is now read again
  from the same buffer after the rest of the pass, and if the two reads disagree the new
  value is dropped and the last accepted one kept, for at most 3 passes in a row per
  zone. That catches a buffer that changes while it is being read, whatever lag it was
  picked with, and costs nothing on a static screen (no zone is flagged) or on a real
  change (a stable buffer agrees with itself, so it is accepted on the first pass).
  Color processing and the dark threshold now run once, after this, on the value that
  was accepted, so a dropped read never moves their hysteresis. A stale slot that is not
  being written during the pass reads the same twice and is not caught. A persistence
  gate (trust a change only after it holds for 2 passes) was tried first and removed: a
  stale slot that stays stale holds, so it let the GOWR flash through, and it delayed
  every real change by a pass.
- Write-up, including the wrong turns:
  [`docs/debugging/sample-lag-and-boot-flash.md`](docs/debugging/sample-lag-and-boot-flash.md).
  `buffer-selection.md` is updated for the second tracked slot.
- Debug builds only: no new packets. FLK1's `curIdx` is still the slot the hook reported,
  so the analyzer's ring-lag columns line up. Nothing in the ini changed.
- Not done: no release build was captured. RDR1 is the only 60 fps title captured. The
  first frame after a reset (boot, a letterbox commit, a settings reload) is accepted
  without the re-read, since there is no earlier value to keep. The cost of the second
  read on a frame where the whole scene changes was not measured. Single-buffer swap
  chains only ever hit the 30 pass fallback in the logic, no real title was seen doing it.

### v3.6
- HITMAN 3 and RDR2, both `0x80002200`, now switch to the HDR decode on the first
  frame with picture data instead of after 53 s. That ID is A8B8G8R8 with the
  console's HDR off and A2R10G10B10 PQ with it on, and the game never says which.
  The smoothness detector had to decide, and on a real HDR-on HITMAN 3 capture it
  took 53.45 s: about 46 s of dark frames under its noise floor, then 4 checks
  2.2 s apart. The words in the buffer were PQ from the first packet, so the strip was
  lit from SDR-decoded PQ the whole time (the sampled words read 80 to 125 out of
  255 on average where the screen was 8 to 36). `detectHdr2200Fast()` now reads
  the alpha byte of 8 zone words on every pass, before the unpack function is
  picked, the same idea as the v3.5 check with the sides swapped: HDR-off words
  have `0xff` there (97% in the capture), HDR-on words never do (top bytes `0xc0`
  to `0xdd` in all 1554), so a black frame decides too, `0xc0000000` against
  `0xff000000`. The smoothness detector still runs on the frames where this one
  holds. **Confirmed on real hardware** in both games: the first frame with data
  decided correctly in every capture (HITMAN 3 at 4.84 s HDR on and 4.38 s off,
  RDR2 at 1.95 s and 0.34 s), no mode flips, and colors right in both modes.
- A fade guard, because the first build had a flaw the hardware run showed: HITMAN 3
  fades to and from black in SDR by writing a frame-wide alpha byte into the words
  (`0x11` up to `0xfe` over about a second), and the 8 zones of one frame differed by
  a few counts (`cc cb cb cb ca ca ca c8`). Those alphas look like PQ, so for about
  0.22 s of a fade-in the strip was decoded as PQ and then a bright frame (`f90d0c0c`)
  held it there. Top bytes within 4 of each other in `0xc1` to `0xfe` now hold the
  mode instead of voting. The cost is that a dark, flat PQ frame seen while the mode
  is still SDR holds until a frame with more variation arrives. Boot goes through
  `0xc0` black, which is exempt, so it never happened in a capture.
- Neither game applies an HDR change until it is restarted (toggled mid-game in
  captures of 37 to 71 s, each way in RDR2 and on in HITMAN 3: the buffers never
  changed), so a live switch isn't a case here. A title that does switch live would be.
- The vote is in `plugin/include/hdr2200_vote.h` and built by
  `tools/test_hdr2200_vote.c` (21 hand-written cases and 281 real frames from
  `tools/data/hdr2200_frames.csv`, including the fade-in frames that fooled the first
  build). Setting `HDR2200_FASTPATH` to 0 in that header brings back the v3.5 behavior.
  Write-up: [`docs/debugging/hdr2200-alpha-detection.md`](docs/debugging/hdr2200-alpha-detection.md).
- Debug builds only: a 58 byte `HDRV` packet goes out on every mode change and about
  every 30 checks. No new FLK1 flag, `0x01` still means the HDR decode is active.
- Not done: no release build was captured, and no title but these two was tried.

### v3.5
- Fixed wrong colors in YouTube when the console's HDR setting is on and it
  plays SDR video: dark gray came out red, blue and green both magenta. HDR
  video was fine. With HDR on the app registers `0x88740000`
  (A2R10G10B10_BT2020_PQ), but for SDR video and its own UI the buffer holds
  plain 8-bit A8R8G8B8 (`0xff212121` for its dark background, the same word it
  writes with HDR off), and it goes back to real PQ for HDR video without
  re-registering. The PQ unpack read `0xffRRGGBB` as red at 8,700 nits or more
  and clipped it. `detectPq8bitMisregistration()` now reads the alpha byte of
  8 zone words on every pass, before the unpack function is picked: 8-bit
  content has `0xff` there on every pixel, and a real 10-bit word can't
  without absurd red. So the frame being decoded gets the right decode, with
  no streak and no delay, and a switch shows no wrong frame. **Confirmed on
  real hardware**: nine switches between SDR and HDR video in a 3 minute
  capture, each new decode landing on the same pass as the decision (0 to
  13 ms), nothing seen wrong on the strip at any of them or at app launch.
  Full investigation, including a first version that voted over 3 checks and
  took about 0.75 s to switch:
  [`docs/debugging/youtube-hdr-8bit.md`](docs/debugging/youtube-hdr-8bit.md).
  Only YouTube tested, and the real-PQ side of the check is mostly generated
  data, see that doc's "Not verified". HITMAN 3's `0x80002200` might have the
  same tell, not checked.
- Debug builds only: FLK1 flags bit `0x10` is set while the 8-bit decode is
  active, and a 58 byte `PQ8C` packet goes out on every mode change and about
  every 30 checks.

### v3.4
- Letterbox zones now sit at the measured bar depth instead of the
  rounded-down value used for the stability gate, which had been leaving
  them a few pixels inside the bar on cutscenes whose depth isn't a
  multiple of the quantize band (only trailers happened to land on one).
  That alone caused loading screens to flash, since a UI layout can
  locally look like a bar on several edges without them agreeing with
  each other the way a real letterbox/pillarbox bar does -- a margin is
  now only applied when both edges of an axis agree. **Confirmed on real
  hardware**, both parts. Full investigation:
  [`docs/debugging/letterbox-placement.md`](docs/debugging/letterbox-placement.md).
  Still open: detection instability on the same cutscene, and only one
  title tested -- see that doc's "Not verified".

### v3.3
- Fixed steady LED flicker on static screens (AC3 Remastered menu) and
  flashes on all-black load screens (God of War Ragnarok). The sampler was
  reading the buffer slot the flip hook had just reported, but the flip
  hooks fire when the game submits a flip, so that slot can still be
  cleared or half drawn. It now reads the slot of the previous flip
  (`AMBIENT_SAMPLE_LAG`, default 1, costs one flip of latency).
  Confirmed on real hardware, and the GOWR flashes were caught frame by
  frame in the slot the fix skips (60 of 60 transients across two
  captures). The full investigation is in
  [`docs/debugging/framebuffer-flicker.md`](docs/debugging/framebuffer-flicker.md),
  with the details of the change and what's still untested in
  [`buffer-selection.md`](docs/debugging/buffer-selection.md).
  Still open: the letterbox cutscene flicker in the same title, no
  capture yet.
- Fixed a delay on dark screens with a small bright element in the
  middle (a loading screen with centered text, for example). The
  letterbox scan only checked 3 fixed points per line (25/50/75%), so
  on a mostly-black frame all four edges scanned nearly to the search
  limit before "finding" the text, and the plugin committed a huge
  border around it. `buildZoneGeometry()` then pulled the edge zones in
  to that margin, so when something flashed in from the real screen
  edge nothing sampled there and the LEDs only reacted once it had
  travelled in to the shrunken area, about a second late. Only showed
  up on dark scenes because bright frames hit non-black on the first
  row and never get that far.
- Letterbox detection now sweeps `EDGE_NUM_SAMPLES` (10) points across
  the whole row/column and only treats the line as bar if 85% of them
  are black (`isRowBlack`/`isColBlack`). A few dozen pixels of text
  only flip a couple of samples, so the scan walks past it to the real
  edge. This is the same approach as `BorderProcessor.findBorderRgb` in
  the Android project, which every real capture path there uses; the
  3-point version this was ported from (`findBorderRgba`) turns out to
  only be exercised by that project's unit tests.
- Search depth is capped at `MAX_BAR_DEPTH_V`/`_H` (1/6 of the screen,
  180px/320px) instead of half the screen. Console output is native
  16:9, so the only bars are cutscenes mastered at a narrower ratio:
  1.85:1 is ~21px per edge and 2.39:1 is ~138px on a 1080p frame. This
  also caps how far a misdetection could ever inset the zones. Worst
  case probe count is about the same as before (~10k vs ~9k) despite
  the extra samples per line.
- `scan_depth` had no upper bound in the plugin's ini loader (every
  neighbouring key checked both ends). Added `SCAN_DEPTH_MAX` (4). Each
  step is `(2*depth+1)^2` tiled reads per zone per pass, so the old
  ceiling of 10 was roughly 54M tiled-offset computations/sec at 512
  zones and 240Hz; 4 brings that to about 10M. A value above 4 in the
  ini is now rejected and the default (1) is kept, same as the other
  bounded keys.
- `saturation` and `contrast` now top out at 100 (were 300). The color
  chain is clamped to 0-255 per channel at the end, so most of that
  range just clipped: over sample colors, +100 already has a channel
  clipped on ~84% of vivid colors and +300 is ~99.8%; for contrast the
  2x factor at +100 clips half the tonal range. Testing 100 against 175
  on a real strip showed little difference, which lines up. Unlike
  `scan_depth`, these two are clamped to the new cap on load rather
  than rejected, so an existing `saturation=175` becomes 100 instead of
  silently resetting to 0. New `SATURATION_MAX` / `CONTRAST_MAX` in
  `ambient_internal.h`.
- `g_pluginVersion` bumped to `0x00000303` for this release, so it
  doesn't drift behind the changelog again the way it did before v3.1.

### v3.2
- Fixed a real crash: GoldHEN injects every plugin into every title's
  process, including this project's own companion app -- just another
  titleid (`SHMY00091`) as far as GoldHEN is concerned. Before this
  fix that was harmless, since the companion app apparently never
  called the plain `sceGnmSubmitAndFlipCommandBuffers` path this
  plugin hooked pre-v3.1. Once v3.1 added the
  `sceVideoOutSubmitFlip`/`...ForWorkload` hooks, the companion app's
  own screen flips started getting captured too -- its live UI colors
  got streamed to the strip, conflicting with whatever real game
  testing/strip-adjustment session was already running, and
  `ambient_sample_thread` then applied this plugin's game-buffer
  tiling math to the companion app's differently-shaped surface.
  Confirmed via a real crash report: thread `ambient_sample_thread`,
  proc `eboot.bin`, AppName `PS4 Ambilight`, TitleID `SHMY00091`,
  SIGSEGV on a page fault reading an address that matched neither this
  project's real GNM buffer addresses seen in other captures. Fixed at
  the very top of `plugin_load`, before any `dlsym`/hook/thread work:
  read `procInfo.titleid` via `sys_sdk_proc_info()` and bail out
  entirely if it's `SHMY00091`. One check fixes both the crash and the
  color conflict, since neither can happen if no hook is ever
  installed in that process to begin with. **Confirmed on real
  hardware: the companion app no longer streams its own screen colors
  to the strip, and no longer crashes on options save/exit.**

### v3.1
- Added a third flip hook, `sceGnmSubmitAndFlipCommandBuffersForWorkload`,
  fixing titles that never light up at all. Shadow of the Tomb Raider
  (SDR and HDR) never lit the strip because it calls the `ForWorkload`
  entry point directly and skips the wrapper the plugin was already
  hooking. Also added `sceVideoOutSubmitFlip` as a second flip hook (it
  turned out not to be the cause, but it's harmless), plus flip/register
  counters in the debug telemetry. Confirmed on real hardware. How it
  was tracked down, including the wrong guesses:
  [`docs/debugging/videoout-hooks.md`](docs/debugging/videoout-hooks.md).
- Also bumped `g_pluginVersion`, which had been stuck at the v2.7.2
  comment (`0x00000212`) since that release, with v2.7.3 through v3.0
  landing on top of it unbumped -- caught up now that there's a real
  reason to touch this file again.

### v3.0
- Removed the manual `capture_margin_top/right/bottom/left` ini keys.
  v2.9's auto letterbox detection can now fully cover what these were
  for -- a fixed manual number is either wrong during a letterboxed
  cutscene or wrong for the fullscreen gameplay around it, since almost
  everything actually renders edge-to-edge with no bars at all -- so
  rather than carry unused legacy config, they're gone. `[layout]`'s
  margin comment block is replaced by the `auto_letterbox_*` keys.
  **Breaking ini change**: an existing config with `capture_margin_*`
  set will have those values silently ignored, not migrated -- if you
  were using them to trim overscan rather than an actual letterbox
  (some capture chains show a few pixels of border the display itself
  would normally crop), auto letterbox threshold/stability won't
  reproduce that; there's no equivalent setting for it anymore.
- `auto_letterbox_enabled` now defaults to `true` (was `false` in
  v2.9). It's the only capture inset the plugin applies, so leaving it
  off by default would mean sampling starts at the true edge on every
  side, unconditionally, for anyone who doesn't know to turn it on.

### v2.9
- Added auto letterbox/pillarbox (black bar) detection, ported from
  the Android "inspiration" project's own `BorderProcessor.kt`: each
  edge is probed independently at 3 points (25/50/75% along the
  perpendicular axis) and scanned inward for the first non-black
  row/column, so an asymmetric bar (e.g. a status bar rendered only
  along the top) is handled correctly instead of being averaged away.
  A newly detected border only commits after
  `auto_letterbox_stability_frames` consecutive re-detections agree,
  to avoid a one-frame flicker snapping the LED geometry. New
  `[layout]` keys: `auto_letterbox_enabled` (off by default at
  introduction), `auto_letterbox_threshold`,
  `auto_letterbox_stability_frames`, `auto_letterbox_check_interval_frames`.
  At this point it added to the existing manual `capture_margin_*`
  fields rather than replacing them -- see v3.0 above, which removed
  those fields once this could stand on its own.

### v2.8
- WLED heartbeat: the plugin now resends the last known-good color once
  a second during any foregrounded gap where the pixel format is
  momentarily unknown or unrecognized, instead of going silent. Closes
  the remaining cases of the v2.2.3 "occasional flash of an unrelated
  color" -- that release made the underlying gap less frequent by
  speeding up the pipeline, but didn't remove it; a long enough gap
  could still trip WLED's own realtime-timeout fallback. This never
  invents a new color -- it only repeats the last one the real pipeline
  already verified -- and does not run while backgrounded/suspended,
  where going silent is still intentional (v2.4).
  Write-up: [`docs/debugging/wled-heartbeat.md`](docs/debugging/wled-heartbeat.md).

### v2.7.6
- Reverted the v2.7.3 stale-flip guard. It was blanking the strip
  whenever no flip landed within ~350ms, but menus and static loading
  screens often throttle or stop flipping while idle, and the gaps
  would drift back and forth across that threshold -- blank, resume,
  blank, resume, visible as flicker on anything that stays on screen
  for a while. Wasn't protecting against anything we'd actually
  confirmed happens (the real black-screen-shows-color bug turned out
  to be the v2.7.5 smoothing fix), so pulling it out rather than
  chasing a threshold that might just move the problem.

### v2.7.5
- Fixed color smoothing getting stuck a few shades above black instead
  of actually reaching it. Integer division truncates toward zero, so
  once a channel decayed down to ~3 the per-frame step rounded down to
  0 and just stopped -- the strip would hold a faint, permanent glow
  of whatever was on screen before a cut to black instead of settling
  the rest of the way. Confirmed on hardware: this was the actual
  cause of "black screen shows color", not the format/decode stuff
  chased earlier.

### v2.7.4
- Fixed 4 duplicate LEDs, one at each screen corner. Zone geometry
  generated each edge inclusive of both endpoints, so the corner
  shared between two edges got sampled by a zone from each edge --
  confirmed on hardware (zone 0 and the last zone were sending
  bit-identical raw pixel reads, every frame).

### v2.7.2
- Live HDR/SDR auto-detection for pixel format `0x80002200`, confirmed
  on real hardware.

### v2.7
- Real support for pixel format `0x80002200` (A8B8G8R8_SRGB),
  HDR-off case only.

### v2.6
- Fixed a bug where the internal opt-in gate silently swallowed the
  `true -> false` "off" send, so the light could get stuck on.

### v2.5
- Merged in the remaining feature-fork commits (v2.3.1, v2.4,
  v2.4.1).

### v2.4
- Merged in an advanced, opt-in networking signal from a fork based
  on the old v2.2 line, for a niche multi-source setup most installs
  don't need.

### v2.2.5
- Replaced the hardcoded debug IP with an opt-in `[dev]` ini section.

### v2.2.4
- Diagnostic telemetry now gated behind `__FINAL__==0` (debug builds
  only) instead of always running.

### v2.2.3
- Root-caused a color glitch to a WLED realtime-timeout fallback
  color, not a decode bug. Real fix, plus a performance pass adding
  a PQ tone-map LUT.

### v2.2.2 / v2.2.1
- Diagnostic-only builds: raw pre-decode pixel dump for 3 zones
  (~1x/sec), and pixel-format-on-change logging to the debug IP.

### v2.2
- Ported an existing reference color-processing implementation into
  the plugin: contrast, per-channel gamma/brightness, and a
  pipeline-ordering fix.

### v2.1.1 - v2.1.5
- Fixed an `fopen` crash, added real-hardware reload diagnostics,
  fixed a config size-read bug and a content-hash bug.
- Fixed live config reload never firing; raised the saturation cap
  to 300.

### v2.1
- Added `black_level`/`white_level` and `dark_threshold` settings,
  plus live config reload (no restart needed to pick up ini changes).

### v2.0
- Made everything that was previously hardcoded user-editable via
  ini settings.

### v1.2
- Moved zone sampling off the render/flip hook and onto a dedicated
  worker thread so a full sampling pass can never stall the game's
  frame submission. Added bounds checking and loop-timing telemetry.

### v1.0
- First real per-frame detile-to-WLED pipeline. Samples a fixed set
  of 229 screen zones per frame, matching a measured LED strip
  layout, and streams them to a WLED controller over the DDP UDP
  protocol. Pixel format and buffer tracking are lifted from
  `detile_verify_probe`, whose tiling math this plugin depends on
  being correct.

### Unreleased / maintenance
- Added `docs/` with an architecture overview and write-ups of the
  v3.3 flicker investigation, the v3.1 flip hook hunt and the v2.8
  heartbeat. The long investigation text that used to sit in the v3.3
  and v3.1 entries here moved there.
- More `docs/`, written from this changelog and the git history: a
  version history, notes on the sampling zones, the HDR format ID,
  letterbox detection, the black-screen smoothing bug, the companion
  app's updater, console gotchas, CI and the GoldHEN SDK patch,
  testing, the tools, config changes by version and a per-title notes
  page. Index in [`docs/README.md`](docs/README.md).
- Removed an advanced opt-in networking comment from the default
  ini, matching the companion app's matching fix -- those settings
  are hand-edited-only and don't need documenting in the shipped
  file.
- `dark_threshold` default changed from 0 to 10 (reverted to 0 in 82c8ca0), to match the
  companion app's default.
- Personal WLED IPs blanked out of the shipped default ini.
- `main.c` (3,240 lines) split into 9 modules (`gamma`, `settings`,
  `network`, `tiling`, `pixel_formats`, `zones`, `color_processing`,
  `hooks`, `sample_thread`) plus a shared internal header, for
  maintainability.

## Companion App

The companion app is a standalone PS4 homebrew UI (not a GoldHEN
plugin) for editing the plugin's ini config on-console, with a live
color preview and a plugin self-updater.

- Fixed the Save button losing its focus highlight on Customize. The
  screen field counts were hardcoded (`SETUP_FIELD_COUNT` 12,
  `CUST_FIELD_COUNT` 20), so once rows were hidden from the schema the
  D-Pad moved focus onto Save at the real count while the render code
  kept waiting for the old one. Both now come from
  `ui_screen_item_range()` and the constants are gone.
- Hid the auto letterbox "Stability (frames)" and "Recheck every
  (frames)" rows. They're debounce timings tuned alongside the plugin's
  new detection scan, and lowering stability to "react faster" just
  brings the flicker back. They're still loaded and saved like any
  other key, and since they no longer have a schema row to clamp
  against, `settings_load` bounds them itself (1-30 and 1-300). Edge
  depth and Reload check stay visible.
- "Edge depth" now tops out at 4 (was 10), matching the plugin's new
  `SCAN_DEPTH_MAX`. Saturation and Contrast now top out at +100% (were
  +300%), matching the plugin.
- The Help screen's "Tune the picture" card still told people to shorten
  the recheck interval, which isn't in the app anymore, and it said to
  *lower* the letterbox threshold when the bars aren't pure black. It's
  the other way round: a pixel counts as black if it's below the
  threshold, so bars at 25 need a threshold above 25. Same mistake in
  the README's troubleshooting entry; both fixed.
- Brightness, both black thresholds, saturation, contrast, black/white
  level, RGB balance and per-channel gamma now show as percentages
  (`DisplayUnit` on `MenuItem`). Display only: `brightness` is still
  0-255 in the ini and the config struct, the app just shows 255 as
  100% and steps it by 5%. Typing a value in the keyboard dialog takes
  a percent too. Untouched values are saved exactly as loaded. See the
  README for the full mapping.
- `test_settings.c` asserted 32 menu items when the schema already had
  33; now asserts the real 31. It still fails at the `relayHost`
  round-trip, which was failing before this and isn't touched here.
  Added `test_display_units.c` for the percent conversion and the
  load-time bounds on the hidden keys.
- Removed the four "Capture margin top/right/bottom/left" fields from
  Customize's Screen sampling card, and added a matching "Auto
  letterbox" card (enable toggle, bar threshold, stability, recheck
  interval) -- follows the plugin's own v3.0 removal of the
  `capture_margin_*` ini keys in favor of always-on auto letterbox
  detection.
- Added a "Smoothing preset" picker (Off/Responsive/Balanced/Smooth) to
  Customize's Motion and darkness card, next to the existing raw
  "Settling time (ms)" field -- values ported from the Android
  "inspiration" project's own `ColorSmoothing.applyPreset` (50/50/200/
  500ms). This is a UI-only convenience: cycling it writes the two real
  `smoothing_enabled`/`settling_time_ms` fields directly rather than
  persisting a third setting of its own, so it can't drift out of sync
  with them, and the raw ms field is still there to hand-tune if
  neither preset's snapped to fits.
- Reworded Setup's WLED IPv4 field description -- it previously read
  "New setup starts with local-network discovery," left over from a
  plan that was never actually built (there's no scanning code
  anywhere in this app). Now just describes what exists: manual IPv4
  entry, DDP on UDP port 4048 by default.
- Fixed every create/overwrite `sceKernelOpen()` call missing
  `O_TRUNC`: the literal `0x200 | 0x001` was commented as
  `O_TRUNC|O_CREAT`, but on Orbis (FreeBSD-derived) that's actually
  `O_CREAT|O_WRONLY` -- the real `O_TRUNC` bit (`0x0400`) was never
  set. Harmless against a brand-new path, but a path with leftover
  bytes from a previous write -- most importantly
  `do_plugin_update()`'s `.new` staging file -- could keep a stale
  tail after a shorter rewrite, so the file that landed on disk could
  be longer than the SHA-256 verified during download. That's why
  `check_for_plugin_update()` kept reporting "update available" on
  every relaunch even when the release hadn't changed. Added
  `ORBIS_O_CREAT_TRUNC_WRONLY` (correct BSD flag values) to
  `plugin_common.h` and switched every affected call site
  (`config.c`'s ini writer; `main.c`'s tmp download, checksum
  sidecar, `plugins.ini` rewrite, and staged `.prx` copy) to use it.
  Also removed the temporary debug hash dump from the update-check
  status line.
- Fixed a startup crash on real hardware: the background update-check
  thread (see below) was created via `SDL_CreateThread()` with stack
  size 0 ("use the platform default"), and on this SDL port that
  default is small enough to blow past almost immediately -- a
  `SIGSEGV` write fault at exactly `rsp-8` on every launch. Switched
  to the same `scePthreadCreate()` pattern the plugin side already
  uses for its own sampling thread, with an explicit 256KB stack.
- Fixed `check_for_plugin_update()` trusting a stale checksum sidecar
  over the real installed file. It read the
  `PLUGIN_INSTALLED_CHECKSUM_PATH` sidecar first and only fell back
  to hashing the actual `.prx` if that sidecar was missing -- but the
  sidecar is only ever written by this app's own updater, so dropping
  a different build in by hand (FTP, a manual GoldHEN plugins-folder
  edit) left the sidecar pointing at the old build while still
  matching the latest release, so the check kept reporting "up to
  date" while a different build was actually running. Now hashes the
  real file first and only falls back to the sidecar if that read
  fails.
- Moved the plugin update check off the main thread. It does two
  sequential blocking HTTPS round trips to GitHub, and running that
  inline at startup held the UI hostage until both finished -- the
  actual cause of the app feeling slow to open, worse on a bad
  connection where each request could hit its full 10s timeout. Now
  runs on a background thread right after the home screen is up,
  polled once a frame via a pair of atomic flags. Also added the
  missing `sceHttpSetRecvTimeOut()` to both HTTP helpers, which
  previously had no bound on a connection that opened and then
  stalled mid-response.
- Wired up plugin update detection: `UI_INSTALL_UPDATE` already
  existed in the UI enum and had a screen ready for it, but nothing
  ever set it. Added a local checksum sidecar written after every
  successful install, and a startup check that compares it (or the
  installed `.prx`'s own hash, as a fallback) against the latest
  release's published checksum, flipping to the update state on a
  mismatch. Fails silently on any network/IO error, since this runs
  unprompted on every launch.
- Resolves GitHub release assets via the `api.github.com` releases
  API instead of `releases/latest/download/...` links, requesting
  `Accept: application/octet-stream` so the redirect goes straight to
  `objects.githubusercontent.com` rather than through github.com's
  web app. The updater now resolves both the `.prx` and `.sha256`
  asset URLs up front, failing fast with a clear status if either is
  missing from the release.
- Fixed the self-updater's `http_download()`/`http_download_text()`
  always failing ("Download FAILED -- check PLUGIN_UPDATE_URL and
  network") even against a valid `PLUGIN_UPDATE_URL`/
  `PLUGIN_CHECKSUM_URL` and a working network. Both URLs are GitHub
  `/releases/latest/download/...` links, which GitHub serves as a
  302 to a signed `objects.githubusercontent.com` URL rather than
  the asset itself; `sceHttp` doesn't follow redirects unless told
  to, so the strict `statusCode == 200` check was seeing the 302 and
  failing every time -- worked fine in a browser (which follows
  redirects transparently) but never on-console. Added
  `sceHttpSetAutoRedirect(tpl, 1)` on both request templates.
- Added a QR-code popup to the Help screen (Triangle), pointing at
  this repo's README `## Help` section -- the on-console cards stay
  short on purpose, so this is the hand-off to the long-form version
  (setup walkthrough, full ini reference, troubleshooting) that
  doesn't fit in a controller-navigable list. Vendored Project
  Nayuki's QR Code generator library (`qrcodegen.c`, MIT license) for
  the encode; the popup itself draws the module grid straight into
  the canvas with `ui_fill_rect_fast()` rather than shipping a
  pre-rendered image asset. Round-tripped through a real QR decoder
  off-console to confirm the encoded bitmap is correct -- see the
  companion app's own README for what's still unverified about how
  it reads on an actual screen and camera.
- Added a Help screen (five scrollable cards: getting started, strip
  setup, picture tuning, controls, troubleshooting), reachable from
  Home and navigated the same way as Setup/Customization. Live
  preview is now scoped to an explicit Setup/Customization check
  instead of "anything that isn't Home", so Help doesn't
  inadvertently send DDP frames to the real strip either.
- Fixed a crash on close: the app previously fell off the end of
  `main()` after `SDL_Quit()`, which isn't a valid way to end a
  process launched via the PS4's `LoadExec` and reliably crashed with
  a `SIGSYS` inside `libkernel.sprx`. Now exits via
  `sceSystemServiceLoadExec("exit", NULL)`, the same pattern used by
  Apollo Save Tool and ItemzFlow.
- Fixed unsaved settings being lost on close -- the app now flushes
  the in-memory config to disk before shutdown, not just on an
  explicit Save press.
- Fixed the Home screen's "Test Strip" card showing a stale
  "not set up yet" state after Setup -> Save, until the app was
  fully closed and reopened.
- Fixed three advanced, hand-edited-only ini keys being written on
  every save even when never set -- there's no settings-UI row for
  them; a fresh install's ini now stays clean unless you add them
  yourself.
- Updated the app icon.
- Initial version: settings editor, live color preview (mirroring
  the plugin's own gamma/brightness pipeline), and a plugin
  self-update path over HTTP.
- Brought in an advanced opt-in networking signal to match the
  plugin, then later removed the dedicated UI card for it -- it's
  ini-only now, matching how the plugin itself exposes it.
- `dark_threshold` default changed from 0 to 10, matching the plugin (both reverted to 0 in 82c8ca0).
- Added disabled-plugin detection and blanked default WLED IPs out
  of shipped defaults.
- Settings-UI, layout-editor, and color-pipeline updates across
  several point releases (v15-v24): scrollable settings list once
  content exceeds one screen, a full-strip LED layout geometry
  rewrite (`layout.c`), and assorted merge/reconciliation fixes
  between parallel lines of work.
- Text rendering switched to FreeType directly (`text_render.c`),
  since SDL2_ttf wasn't a compiled lib in the target SDK snapshot.
- Controller navigation fixed on real hardware by removing
  `SDL_INIT_JOYSTICK`, which was contending with the native `scePad`
  handle for the same controller and causing stale button state.

## Tools

- `tools/decode_verification_dump.py` -- decodes the plugin's raw UDP
  debug/telemetry packets for offline analysis. Went through several
  format-support passes as the plugin's own wire formats changed:
  registration-event decoding, config-reload packet decoding,
  auto-selecting the unpack format from registration packets, timing-
  packet decoding, HDR/PQ decoding, an R/B channel-swap fix for
  A8B8G8R8_SRGB, and runtime pixel-format auto-detection via decode
  smoothness.
- `tools/ps4_detile_2dthin.c` -- reference detiling implementation used
  to cross-check the plugin's own tiling math.
- `tools/udp_ground_truth_listener.py` -- minimal UDP listener used for
  capturing real packets from the plugin during development.
- `tools/flicker_capture.py` -- captures and analyzes the flicker probe
  packets ("FLK1", 132 bytes, one per sampler pass) from a debug build
  of the plugin. `capture <file> [--seconds N]` records to a `.flk`
  file, `analyze <file> [--csv out.csv]` prints a report (output
  steadiness, sampler timing, flip rate, all-zero reads and burst
  spacing, per-zone tear/disagreement between buffers, a ring-lag
  table showing which slot is safe to read, and which lag the pipeline
  actually read, so a working sample-lag fix is recognised), and
  `selftest` runs the analyzer against 10 synthetic scenarios. Needs a debug build of the plugin
  (`make DEBUG=1`, the FLK1 probe is compiled out of release builds) with
  `[dev] dev_ip` and `dev_logging=true` set. Uses UDP port 4048, so it
  can't run alongside `udp_ground_truth_listener.py`. Its verdict
  thresholds are estimates; trust the printed numbers. Found the
  cleared-buffer sampling bug described under Plugin > v3.3. Also counts
  how often the `0x88740000` 8-bit decode (FLK1 flags `0x10`) switched.
- `tools/test_pq8bit_vote.c` -- host test for the alpha byte vote behind the
  YouTube fix, run on words from real captures and on generated PQ. Builds
  with plain `gcc`, no SDK. See Plugin > v3.5.

## CI

- A tag push now fails early if the tag doesn't match `g_pluginVersion`
  (major.minor only, the tag's patch part is free). It's the first step
  of `build_prx`, and `publish` needs that job, so a mismatched tag such
  as `v3.5.0` on a plugin that still says 3.4 produces no release.
  Non-version tags like `v3.4.0-rc1` are rejected too. See
  [`docs/development/ci-and-releases.md`](docs/development/ci-and-releases.md).
- Tagged-release builds now also build and package the companion app
  (`build_pkg` job: compiles `companion-app/source/*.c` against the
  same OpenOrbis toolchain the plugin job uses, via its Linux
  `create-fself`/`create-gp4`/`PkgTool.Core` binaries instead of
  `build.bat`'s Windows ones, producing a `.pkg`). Releasing is now
  its own `publish` job that waits on both the plugin and companion
  app builds and is the only job that touches
  `softprops/action-gh-release`, so the two builds run in parallel
  without a chance of racing each other to create the same release.
- `build_pkg` pinned to `ubuntu-22.04` instead of `ubuntu-latest`:
  `PkgTool.Core`'s `pkg_build` subcommand needs `libssl1.1`, which
  `ubuntu-latest` (24.04) no longer ships, so it aborted with "No
  usable version of libssl was found" on the first real run.
- First real compile of `ui_icons.c` (this CI job is the first time
  the companion app has ever actually been build-tested) turned up a
  real bug, not a CI issue: `CUR`, the "currentColor" sentinel used
  84 times across the file's icon tables, was a `static const
  UiColor` object. Clang hard-errors reading a named const object's
  value into another file-scope initializer ("initializer element is
  not a compile-time constant") -- GCC's default GNU-extension mode
  had been silently tolerating this locally. Changed `CUR` to a
  compound-literal macro, matching the file's own `CHAN_RED`/
  `CHAN_GREEN`/`CHAN_BLUE` pattern, which doesn't have this problem.

