# Changelog

All notable changes to PS4 Ambilight (the GoldHEN plugin and its
companion app) are documented here, newest first.

## Plugin

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
- Merged in the remaining relay-feature-fork commits (v2.3.1, v2.4,
  v2.4.1).

### v2.4
- Merged in the WLED-relay hand-off signal from a fork based on the
  old v2.2 line. Lets this plugin tell a separate `wled-relay`
  companion project when it's actively driving the shared WLED
  controller, so the two don't fight over the same LED strip during
  a game.

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
- Ported the Android companion app's `ColorProcessor` into the
  plugin: contrast, per-channel gamma/brightness, and a pipeline-
  ordering fix.

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
- Removed the wled-relay explanation comment from the default ini,
  matching the companion app's relay-field fix -- relay settings are
  opt-in-by-hand only and don't need documenting in the shipped file.
- `dark_threshold` default changed from 0 to 10, to match the
  companion app's default.
- Personal WLED/relay IPs blanked out of the shipped default ini.
- `main.c` (3,240 lines) split into 9 modules (`gamma`, `settings`,
  `network`, `tiling`, `pixel_formats`, `zones`, `color_processing`,
  `hooks`, `sample_thread`) plus a shared internal header, for
  maintainability.

## Companion App

The companion app is a standalone PS4 homebrew UI (not a GoldHEN
plugin) for editing the plugin's ini config on-console, with a live
color preview and a plugin self-updater.

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
- Fixed `relay_host`/`relay_port`/`relay_signal_enabled` being
  written into the ini on every save even when never set -- these
  are opt-in, hand-edited-only fields with no settings-UI row; a
  fresh install's ini now stays clean unless you add them yourself.
- Updated the app icon.
- Initial version: settings editor, live color preview (mirroring
  the plugin's own gamma/brightness pipeline), and a plugin
  self-update path over HTTP.
- Brought in the WLED-relay hand-off signal to match the plugin,
  then later removed the dedicated UI card for it -- it's ini-only
  now, matching how the plugin itself exposes it.
- `dark_threshold` default changed from 0 to 10, matching the plugin.
- Added disabled-plugin detection and blanked default WLED/relay IPs
  out of shipped defaults.
- Settings-UI, layout-editor, and color-pipeline updates across
  several point releases (v15-v24): scrollable settings list once
  content exceeds one screen, a full-strip LED layout geometry port
  from the Android app (`layout.c`), and assorted merge/reconciliation
  fixes between parallel lines of work.
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

## CI

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

