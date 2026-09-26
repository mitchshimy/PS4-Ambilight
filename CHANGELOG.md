# Changelog

All notable changes to PS4 Ambilight (the GoldHEN plugin and its
companion app) are documented here, newest first.

## Plugin

### v2.7.4
- Fixed 4 duplicate LEDs, one at each screen corner. Zone geometry
  generated each edge inclusive of both endpoints, so the corner
  shared between two edges got sampled by a zone from each edge --
  confirmed on hardware (zone 0 and the last zone were sending
  bit-identical raw pixel reads, every frame).

### v2.7.3
- Sampling thread now blanks the strip if no flip has landed in
  ~350ms, instead of continuing to read a buffer the engine may have
  silently repurposed (common during loading screens, which often
  just stop flipping). Didn't end up being the "white on black
  screens" bug, but a buffer that's gone stale still isn't safe to
  read, so keeping it.

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
- Removed an advanced opt-in networking comment from the default
  ini, matching the companion app's matching fix -- those settings
  are hand-edited-only and don't need documenting in the shipped
  file.
- `dark_threshold` default changed from 0 to 10, to match the
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
- `dark_threshold` default changed from 0 to 10, matching the plugin.
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

