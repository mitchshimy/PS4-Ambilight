# Plugin v2.x and v1.x release notes

Full release notes, moved out of the [changelog](../../CHANGELOG.md), which now keeps one line per change.

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
  Write-up: [`docs/debugging/wled-heartbeat.md`](../debugging/wled-heartbeat.md).

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
  page. Index in [`docs/README.md`](../README.md).
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

