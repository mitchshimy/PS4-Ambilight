# Companion app release notes

Full release notes, moved out of the [changelog](../../CHANGELOG.md), which now keeps one line per change.

## Companion App

The companion app is a standalone PS4 homebrew UI (not a GoldHEN
plugin) for editing the plugin's ini config on-console, with a live
color preview and a plugin self-updater.

- **Shell feed support (off by default).** A new **Shell feed** card on Set up (after LED strip
  layout, before Live reload) with two switches. **Shell feed** (`[shell] enabled`, default off)
  turns the whole feature on; with it off the app is exactly as before (Home is byte for byte the
  same, nothing shell-related is checked, downloaded or written). **Autoload at boot** edits
  GoldHEN's payload AutoRun list: `ps4-ambilight.bin = 1` runs it at boot, `= 0` keeps the line
  but doesn't, and every other entry, comment and line ending in that file is left alone.
  Pressing that row while it can't be turned on says what is missing, in the order to fix it (Shell
  feed switch off, plugin not installed or not enabled, loader or shell feed files missing). Set up
  has no status line, so the hint goes in the save bar in place of "Changes apply after saving." and
  clears on the next button press.
  With the Shell feed on, Home shows a **Shell feed** banner to the left of the plugin banner (the
  same height, so the button doesn't move) and one button for everything pending. It installs or
  repairs the shell feed's two files (`ps4_shell_feed.prx` into `/data/ps4-ambilight`, creating the
  folder if needed, and `ps4-ambilight.bin` into `/data/payloads`, creating that too), and
  updates any of the three files that have a newer published build: one button reads "Update shell
  feed", "Install all (3)", and so on. **Test strip appears only once nothing is pending**, the
  same rule the plugin's own update already followed. Downloads use the same staging, sha256 check
  and rename as the plugin's. The app now saves `[shell] enabled` explicitly (the shell feed
  treats a missing key as on, so "off" has to be written); the plugin ignores it
  (`tools/test_plugin_config.c` loads the new golden ini through the plugin's real loader).
- **Fixed: LED flicker while the app was open.** With the shell feed running, the app's live
  preview (Set up, Customization, Test strip) and the shell feed both sent to WLED, which
  alternated between them. While the Shell feed switch is on and the app is sending, it now claims
  the strip through `/data/ps4_ambient_state` the way a game does (see
  [shell-feed-coordination](../development/shell-feed-coordination.md)). The claim follows the
  saved `[shell] enabled` as well as the switch, because the shell feed keeps running until the OFF
  is saved; it is released once the OFF is saved (or the app is closed, which saves).
- **Fixed: the app kept holding the LEDs after the PS button.** On Set up, Customization and Test
  strip it kept sending (and kept its strip claim) while in the background. It now polls
  `sceSystemServiceGetStatus` (`isInBackgroundExecution`, the same call the plugin uses) twice a
  second, and while backgrounded sends nothing and releases the claim; it takes both back when
  foregrounded.
- **Home checks GoldHEN's plugin loader.** With **Enable Plugins Loader** off in Settings > GoldHEN >
  Plugin Settings, GoldHEN loads no plugin for any game, and Home couldn't tell. The app now reads
  that switch from `/data/GoldHEN/config.ini` (`[PluginLoader] PluginLoader_Enabled`) and, when it
  is off, makes it the first thing Home shows: a "Plugin loader is off" banner and a **Close app to
  turn on plugin loader** button. Install, Update, Enable and Test strip wait until it is on. The app
  only reads the file: GoldHEN keeps the live value in memory and won't change it while an app is
  running, so writing it from here doesn't work (tried: the file said 1, GoldHEN's menu still showed
  it off). It's re-read every couple of seconds on Home and on coming back to the app. A missing file
  or key (an older GoldHEN) is treated as on. The in-app Help and the README no longer say Home
  can't see this switch.
- **Help and README for the shell feed.** The app's Help screen has a new **Shell feed (optional)**
  card (six cards now), and its Getting started and Troubleshooting cards mention the Test strip
  rule and the two-sources flicker. The main README has a **Shell feed (optional)** section, a
  quick start step and Help entries for Test Strip being hidden, Autoload not turning on and the
  strip flickering. The Set up row for **Shell feed** no longer says "Off keeps this app exactly as
  it was", which meant nothing to someone who never saw the earlier app.
- **Launch Ambilight payload button.** The Shell feed card on Set up has a new **Ambilight payload**
  row whose button, **Launch Ambilight payload**, sends `ps4-ambilight.bin` to GoldHEN's
  BinLoader (port 9090) so the shell feed starts without a reboot or a trip into GoldHEN's menu,
  whether or not Autoload at boot is on. It checks the same things as Autoload first (Shell feed
  on, plugin installed and enabled, payload and `.prx` on the console) and says what is missing.
  A successful install or repair now sends it too. It is a `ui` row, so it never reaches the ini.
  Its text said "Reset Game" because every action button was drawn as the preset reset; it now
  has its own text and icon.
- **One name for the payload.** Wherever the app, its Help and the docs meant `ps4-ambilight.bin`
  by "loader" or "launcher" they now say **Ambilight payload** (Set up, Home banners, status and
  hint messages, the download progress line). "Plugin loader" and "BinLoader" are GoldHEN's own and
  are unchanged. Not yet tried on a console: the button and the new wording
  ([local-install-testing](../development/local-install-testing.md)).
- **The release now carries the shell feed files.** A tagged release attaches
  `ps4-ambilight.bin` and its `.sha256` next to `ps4_shell_feed.prx` and its `.sha256` (the
  `.prx` pair was already attached), so the Shell feed install finds all of them on GitHub and
  no longer works only against the local test server. The `.bin` is the committed file in
  `payloads/`; the publish job checks it against the hash in `payloads/CHECKSUMS.md` and fails
  the release if they differ, then writes the sidecar from the file it attaches. A release
  has seven files now: two `.prx`, the `.bin`, their three `.sha256` files and the `.pkg`.
  `docs/development/ci-and-releases.md` is updated, and now says that `-rc` tags are allowed
  (they publish as pre-releases).
- **Fixed: the Ambilight payload button's text is tested.** The button text now comes from one
  function, `settings_action_text()`, instead of the drawing code, and a new
  `tests/test_action_buttons.c` fails if any action button lacks its own text, if one other than
  the preset reset says "Reset", or if the menu says loader or launcher for the payload.
  `tests/test_settings.c` no longer fails at the `relayHost` round trip: the test was stale (the app
  has kept `relay_*` keys only when the file already has them since `a4c154c`), so it now seeds
  the file and also checks that a fresh save adds none. No app behaviour changed.
- **`tools/update_payload_checksum.sh` keeps `payloads/CHECKSUMS.md` current.** After replacing
  `payloads/ps4-ambilight.bin`, run it and it rewrites the hash and size on that page from the
  file (the note about the build stays yours); `--check` only reports. CI runs `--check` on every
  push, not only on a tag, so a binary changed without the page fails straight away rather than
  at release time. The publish job uses the same script instead of its own copy of the check.
- **The companion app has a version.** `companion-app/include/app_version.h` holds `APP_VERSION`
  (still 1.00) as the one place it lives. The Help screen now says "App version 1.00" under its
  title, and the package's `APP_VER` and `VERSION` are read from that line by both CI and
  `build.bat` instead of being typed into each (CI fails the build if the line is missing).
  Bump it when the app changes; `docs/development/ci-and-releases.md` lists it in "Cutting a
  release". `tests/test_app_version.c` keeps its form right. Not yet run: a real CI package
  build and `build.bat` on Windows with the new lookup.
- **The on-screen keyboard moved out of `main.c` into `source/ime.c`.** The first step of
  splitting `main.c` (about 135 lines, now 2,829 from 2,952). No behaviour change: the code is the
  same as it ran on hardware, except that it reads and writes the config, the user id, the dialog
  position and the status line through an `ImeHost` that `main.c` passes in, so `ime.c` needs none
  of `main.c`'s globals. `tests/test_ime.c` is new and covers it with a fake keyboard (opening, a
  typed number applied and clamped, garbage and empty input rejected, cancel, the string row, a
  refused open, shutdown); it uses test-only SDK stand-ins in `tests/stubs/orbis/`. Not yet run:
  the CI package build and the keyboard on a console, since `main.c` can't be compiled on a PC.
- **The downloads and the release lookup moved out of `main.c` into `source/net_http.c`.** The
  second step of splitting `main.c` (it is 2,500 lines now, from 2,829). `http_download()`,
  `http_download_text()` and `resolve_asset_url()` are in `net_http.c` with `include/net_http.h`,
  and the GitHub repo and release API URL with the note on why the API is used are in that header.
  The code is the same as it ran on hardware. What changed is wiring: the cached `[dev]
  update_base_url` is now set with `net_set_dev_base_url()` and read with `net_dev_base_url()`
  instead of a `main.c` global (the ini is still read on the main thread only, in `main.c`), and the
  JSON scan for an asset's url is a header, `include/github_release_json.h`, so it can be tested:
  `tests/test_github_release_json.c` is new. Not yet run: the CI package build and an update or
  install on a console, since `main.c` and `net_http.c` need the OpenOrbis toolchain. A syntax check
  of `net_http.c` against stand-in headers was clean.
- **`main.c` is split into modules.** The rest of the split, done in one go: the background update
  check (`source/update_check.c`), plugin install, enable and registration plus GoldHEN's plugin
  loader switch (`source/plugin_install.c`), the shell feed's install, repair, payload launch,
  autoload and strip claim (`source/shell_feed.c`), small file and checksum helpers
  (`source/file_util.c`), the on-console paths and asset names (`include/app_paths.h`) and what
  `main.c` shares with them (`source/app_state.h`). `main.c` is 1,248 lines now, from 2,952 at the
  start of the split; the input handlers, the live preview and the main loop stay in it. No
  behaviour change: the code is the same as it ran on hardware (every old line is accounted for;
  what changed is `static` on the functions and globals the modules now share, includes, and the
  doc comment of `ensure_plugin_registered_in_goldhen()`, which had drifted away from it, now sits on
  it). Checked by compiling the old `main.c` and the new files against stand-in SDK headers and
  comparing the errors: nothing new in any of them. Not yet run: the CI package build, and the
  update check, a plugin install, a shell feed install, Launch Ambilight payload and autoload on a
  console. Since the functions are no longer `static`, a link error about a duplicate symbol, if CI
  reports one, would be a name that clashes with one in the SDK.
- **The Ambilight payload now carries offsets for several firmwares.** Replaces the single-firmware
  build (`payloads/ps4-ambilight.bin`, 24,000 bytes now, was 22,816): it picks the right firmware's
  offsets at runtime instead of only knowing 9.00, and refuses cleanly on a firmware it doesn't
  recognise (writes `PLUGIN_DIR/unsupported_fw`, touches nothing else). Offsets are complete for
  5.05 through 12.02; **only 9.00 is confirmed on a real console so far**, the rest are untested.
  12.50 and up aren't supported yet (missing kernel offsets for those). Re-sending the payload to
  an already-running console was tried deliberately on 9.00, to check the fallback path refuses
  rather than re-injecting or crashing -- it did.
  `shell-feed/docs/support-matrix.md` has the full per-firmware table (from the offset work in the
  research repo, `ps4-shell-capture-research`). `payloads/CHECKSUMS.md`, the top-level README,
  `shell-feed/README.md` and the companion app's Help screen no longer say "9.00 only"; they now
  say which part is confirmed and point to the matrix for the rest. No code in this repo changed
  the firmware-detection or offset logic -- that's the payload's own binary, built elsewhere.
- **Firmware warning, and shell feed notifications.** Only the shell feed needs a firmware check
  (the game plugin works on every firmware). With the **Shell feed** switch on, Home shows a warning
  panel when the console's firmware is one the payload has no complete offsets for (12.50 and up,
  or one not in `shell-feed/docs/support-matrix.md`), and launching the payload says it again in
  the status line. It is a warning, never a block. The firmware is read once at startup from
  `kern.sdk_version` (`source/fw_check.c`), as BCD (12.50 is `0x1250xxxx`); a value that isn't
  valid BCD shows nothing. The support table and the text are in `common/fw_support.h`, with a copy
  in `companion-app/include/` because this app only has `-Iinclude` (keep them identical), and
  `tools/test_fw_support.c` checks them. With no WLED Host set, the live preview's status now says
  so instead of calling an empty host "not a valid IPv4 address".
  The shell feed itself puts a line in the console's notification bar when it starts sending
  ("running", once per boot), when it can't start (no `wled_host`, `wled_host` isn't an address, no
  display buffers, `socket()` failed; none of these is retried until a reboot, and the message
  says so), when `/data/ps4_shell_feed_off` pauses it, when `[shell] enabled=false` pauses it while
  it was running (not at every boot where it has simply been off), and when it resumes. Not yet
  run: the plugin, shell feed and companion app builds, and any of this on a console. In
  particular that `kern.sdk_version` reads in the companion app, that the BCD reading is right, and
  that a notification from the compositor process (where the shell feed runs) shows.
- **Local update source for testing.** `[dev] update_base_url=http://<pc>:8000` in the ini makes
  every download and its `.sha256` come from that address instead of GitHub; delete the line to go
  back. `tools/serve_local_release.py` serves the three files with their checksums.
  Plain `http://` works on the console.
- **Build:** `sceKernelMkdir` (a first use in this repo) creates the two folders; it links and works on
  a console. `APP_MKDIR` in `main.c` is the one line to swap if a later SDK breaks it.

- The app now writes the short ini and needs plugin v3.8. Both presets are saved in full to
  `[preset_game]` and `[preset_movie]` with `[presets] active=`, and the old flat copies of
  those values are removed from `[layout]`, `[color]` and `[timing]`, which keep setup values
  only. This replaces the arrangement in the next entry, where the flat keys held the active
  preset for a plugin that didn't read the preset sections. An ini that still has the old
  flat keys loads as before (they become Game) and is converted on the first save. Once an
  ini has preset sections the app ignores any flat leftovers, the same as the plugin.
  `auto_letterbox_stability_frames` and `auto_letterbox_check_interval_frames`, which have no
  row in the app, are only written when they differ from their defaults. One save call now
  writes the whole file (`presets_save`). `tests/test_presets.c` also checks that the plugin's
  shipped ini agrees with the app's shipped presets, and that the app's output matches
  `tools/data/preset_ini_golden.ini`, which the plugin's own test loads.
- Added Game and Movie presets to Customization. A Preset card at the top
  switches between them (D-Pad or L1/R1), and every value below it belongs
  to the preset showing, so switching never overwrites the other one's
  tweaks. There is no Custom mode: editing a field edits the preset you're
  in, and Reset (X twice) puts just that preset back to its shipped values.
  Game ships at edge depth 2, brightness 100%, saturation +35%, gamma 2.2,
  black level 0% and smoothing on at 50 ms. Movie ships at edge depth 4,
  brightness 80%, saturation +25%, gamma 2.4, black level 1% and smoothing
  on at 200 ms. Everything else stays at the plugin default. The gammas
  assume WLED isn't applying its own gamma to the DDP stream. Setup values
  are never part of a preset. The plugin isn't changed: the flat `[color]`,
  `[timing]` and `[layout]` keys it reads always hold the active preset,
  and both presets are also written in full to `[preset_game]` and
  `[preset_movie]` with `[presets] active=`. An ini from before presets
  keeps its tuning as Game and Movie starts as a copy with Movie's
  smoothing. New `source/presets.c` and `tests/test_presets.c`.
- Added a "by Shimy" credit at the bottom right of Home, in Michroma (SIL
  OFL, licence in `assets/fonts`).
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

