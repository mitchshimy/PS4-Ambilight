# ps4_ambient_light Companion App

A standalone PS4 homebrew application (not a GoldHEN plugin) for
editing `/data/ps4-ambilight/ps4_ambient_light.ini` through a controller-driven UI,
with a live color preview against your real WLED light, and for
installing and updating the plugin binary from your GitHub repo. It can
also install and keep current the optional shell feed (a second plugin
plus its Ambilight payload) that lights the strip outside games; that is off by
default and the app does nothing shell-related while it is off.

## Design notes

- `color_pipeline.c`, `ddp.c`, and `config.c` mirror the plugin's own
  gamma/brightness/color pipeline, WLED UDP sender, and ini parser
  respectively, so the live preview matches what the plugin actually
  sends.
- `settings.c`/`config.c` implement the ini schema used by the plugin's
  own `ini_table_get_entry*` call sites, plus the app's own `[shell]` and
  `[dev]` keys.
- **Text rendering** goes through FreeType directly
  (`source/text_render.c`, via `-lSceFreeType`) rather than SDL2_ttf,
  since SDL2_ttf isn't a compiled lib in the toolchain snapshot this
  targets. `draw_text()` in `main.c` calls into the resulting glyph
  atlas.
- **Percent display.** Brightness, the two black thresholds, saturation, contrast, black/white
  level, RGB balance and per-channel gamma are shown as percentages, but the config struct and the
  ini keep the raw value (brightness is still 0-255). Each `MenuItem` carries a `DisplayUnit`, and
  `settings_to_display()` / `settings_from_display()` convert at the edges: on render, on D-Pad/L1/R1
  nudges and on keyboard entry. Nothing else in the app sees a percent. See "Percentages in the
  companion app vs. the ini" in the main README for the mapping; `tests/test_display_units.c`
  covers the conversion.
- Two auto letterbox keys (`auto_letterbox_stability_frames`, `auto_letterbox_check_interval_frames`)
  are intentionally not in `kMenuItems`. They're still loaded and saved, and `settings_load` bounds
  them itself since there's no schema row to clamp against.
- Controller navigation uses the native `scePad` API, not SDL's
  joystick subsystem -- `SDL_INIT_JOYSTICK` is deliberately left
  disabled, since it can contend with a separately-opened `scePad`
  handle for the same DualShock HID device and cause
  `scePadReadState` to report stale button state.
- The settings list scrolls once it exceeds one screen's worth of
  rows (split between Set up and Customize); `g_scrollOffset` tracks the
  selection via `settings_last_visible_index()`, with a
  "-- showing X-Y of N --" indicator when the list is cut off.
- LED output goes through `layout.c`, converting the configured strip
  layout into one color per configured LED in real physical wire
  order (not a fixed sample-swatch layout).
- The Help screen stays deliberately minimal (six short cards --
  see `ui_screens.c`'s own comment above `help_cards_init()`), since
  there's no room here for the level of detail this repo's own
  README goes into. Triangle on that screen instead opens a QR-code
  popup (`help_qr.c`, drawing through `ui_fill_rect_fast()` in
  `render_qr_modal()`, `ui_screens.c`) pointing at this repo's own
  README `## Help` section, for anyone who wants the long version
  without typing a URL on a controller.
- **GoldHEN's plugin loader switch.** With "Enable Plugins Loader" off, GoldHEN loads no plugin for
  any game, which used to be invisible from Home. The app now reads
  `[PluginLoader] PluginLoader_Enabled` from `/data/GoldHEN/config.ini` (`goldhen_config.h`, tested
  against two real console files) and, when it is a definite 0, `home_plan()` puts a loader-off
  notice ahead of every other action, including Test strip. It never writes the file: GoldHEN keeps
  the live value in memory (an edit left its menu showing the switch off) and won't change the
  switch while an app is running, so the button closes the app so the person can. Unknown (no file,
  no key) is treated as on, so it never blocks on a guess. Home re-reads it every couple of seconds
  and on returning from the background.
- **Shell feed (off by default, `[shell] enabled`).** Three files can be pending:
  `ps4_ambient_light.prx`, `ps4_shell_feed.prx` and the Ambilight payload
  `ps4-ambilight.bin`. `install_plan.h` is pure decision logic (no
  console calls) that turns the plugin's state and the shell feed's state into
  the Home banner and what the one button does; Test strip is offered only when
  nothing is pending. The shell feed's two files download with the plugin's
  staging, SHA-256 check and rename, into `/data/ps4-ambilight` and
  `/data/payloads` (created with `sceKernelMkdir`). **Autoload at boot** edits
  GoldHEN's payload AutoRun list in place (`payload_ini.h`): one line is added
  or flipped between `= 1` and `= 0`, and every other line, comment and line
  ending is kept. It is a `ui`-section row, so it never reaches this app's own
  ini. Pressing it while it can't be turned on puts a hint in the save bar
  (`UiState.settingsHint`), because only Home draws the status line.
  **Launch Ambilight payload** (the `shell_run_now` action, also a `ui` row) sends
  `ps4-ambilight.bin` to GoldHEN's BinLoader on port 9090 without a reboot, and is guarded the
  same way as Autoload (Shell feed on, plugin installed and enabled, payload and `.prx` present).
  A successful install or repair sends it too. In the UI every mention of this file is
  "Ambilight payload"; "loader" is kept only for GoldHEN's own plugin loader and BinLoader.
- **Strip claim.** While the Shell feed switch is on (in memory or in the saved
  ini) and the app is sending to the strip, it writes `/data/ps4-ambilight/ps4_ambient_state`
  like a game does (`claim_file.h`), so the shell feed doesn't send at the same
  time. It also polls `sceSystemServiceGetStatus` and stops sending and drops
  the claim while the app is in the background (PS button). The format and the
  reader's side are in `docs/development/shell-feed-coordination.md`.
- **Local update source.** `[dev] update_base_url=http://<pc>:8000` in the ini
  makes every download and its `.sha256` come from that address instead of the
  GitHub API (`update_source.h` cleans the value up). `tools/serve_local_release.py`
  at the repo root serves the three files with their checksums. See
  `docs/development/local-install-testing.md`.
- On-screen keyboard for `wledHost` uses the real
  `sceImeDialogInit`/`sceImeDialogGetStatus`/`sceImeDialogGetResult`/
  `sceImeDialogTerm` sequence from `orbis/ImeDialog.h`.
  `sceCommonDialogInitialize()` runs once at startup, and input
  handling is gated behind `sceCommonDialogIsUsed()` while the
  keyboard is open so it doesn't fight the system dialog for input.

## Known limitations

1. A few IME-dialog details are implemented from the SDK header alone,
   without a matching sample to confirm against: passing `NULL` for
   `sceImeDialogInit`'s second (`OrbisImeSettingsExtended*`) argument,
   `posx`/`posy`'s exact units (assumed screen pixels, centered at
   960,540 for this app's 1920x1080 window), and
   `supportedLanguages=0` as "default/unrestricted."
2. The install and update path (the main button on Home, now also for
   the shell feed files) has been run end to end on a console against a
   local test server over plain `http://`, including checksum failures.
   Against GitHub's own releases and on other network setups it has
   not been exercised as widely. The fallback is to put the files in
   place by hand.
3. The Help screen's QR popup: `help_qr.c`'s encode/lookup path is
   verified off-console (built and run against the real vendored
   `qrcodegen.c` on a desktop toolchain, then round-tripped through
   an actual QR decoder -- comes back as exactly
   `https://github.com/mitchshimy/PS4-Ambilight#help`, nothing more,
   nothing less). What isn't yet verified is the on-console rendering
   itself against a real screen and a real phone camera: whether an
   8px-per-module code at this panel size reads reliably off a TV
   from a few feet away, and whether the dark-on-white contrast holds
   up under a TV's own brightness/gamma. If it doesn't scan cleanly,
   raising `moduleSize` in `render_qr_modal()` (`ui_screens.c`) is the
   first thing to try before touching anything in `help_qr.c`.

## Building

Needs `OO_PS4_TOOLCHAIN` set up per OpenOrbis's normal setup, run from
inside this folder:

```
build.bat <intermediate_dir> ps4_ambient_light_companion <output_dir>
```

The PC tests build with plain `gcc`, see `docs/development/testing.md`.
`tools/ui_preview.sh` renders Home, Set up and Help to PNG with the app's real
UI code, for checking a layout change without a console.

## Files

- `source/main.c` -- app entry, the main loop, SDL2 drawing, controller nav, the Home / Set up / Help input handlers and the live preview. Everything below was split out of it; `source/app_state.h` lists what `main.c` shares with them (`g_cfg`, `g_state`, `g_dirty`, `set_status()`, `render_and_present()`) and `include/app_paths.h` holds the on-console paths and release asset names
- `source/file_util.c` / `include/file_util.h` -- small file helpers (`read_small_file`, `write_file_atomic`, `file_size_of`) and the checksum helpers
- `source/update_check.c` / `include/update_check.h` -- the background update check (worker thread, flags, once-a-frame poll) and the shell feed file bookkeeping `g_assetPresent` / `g_assetUpdate`
- `source/plugin_install.c` / `source/plugin_install.h` -- installing, enabling and checking the plugin (`do_plugin_update`, `do_plugin_enable`, the `plugins.ini` registration) and reading GoldHEN's plugin loader switch
- `source/shell_feed.c` / `include/shell_feed.h` -- the shell feed's file work: install and repair, sending the Ambilight payload to the BinLoader, the autoload entry, and the strip claim
- `source/notify.c` / `include/notify.h` -- a line in the console's notification bar (the same one the plugin and shell feed use), honouring the Notifications switch on Set up (`[notifications] enabled`)
- `source/net_http.c` / `include/net_http.h` -- HTTP(S) downloads (`http_download`, `http_download_text`) and the lookup of where a release asset lives (GitHub's release API, or the `[dev] update_base_url` test server), split out of `main.c`
- `include/github_release_json.h` -- the scan that pulls one asset's API URL out of GitHub's release JSON (header-only so a PC test can run it)
- `source/ime.c` / `include/ime.h` -- the on-screen keyboard for the numeric and string rows, split out of `main.c`; it takes everything it needs through an `ImeHost` instead of reaching into `main.c`'s globals
- `source/text_render.c` / `include/text_render.h` -- FreeType-based text rendering (not SDL2_ttf)
- `include/app_version.h` -- `APP_VERSION`, the one place the app's version lives (Help screen, and `APP_VER` / `VERSION` in the package, read by CI and `build.bat`)
- `source/settings.c` / `include/settings.h` -- ini schema + load/save
- `source/color_pipeline.c` / `include/color_pipeline.h` -- live preview math (mirrors the plugin)
- `source/ddp.c` / `include/ddp.h` -- WLED UDP sender (mirrors the plugin)
- `source/presets.c` / `include/presets.h` -- Game / Movie presets: shipped values, per-preset storage, switching, reset
- `source/config.c` / `include/config.h` -- ini_table parser (shared with the plugin)
- `source/layout.c` / `include/layout.h` -- full-strip LED layout geometry (physical wire order + on-screen position)
- `source/help_qr.c` / `include/help_qr.h` -- builds and caches the Help screen's QR bitmap (points at this repo's README `## Help` section)
- `source/qrcodegen.c` / `include/qrcodegen.h` -- vendored, unmodified: [Project Nayuki's QR Code generator library](https://github.com/nayuki/QR-Code-generator) (C edition, MIT license)
- `include/goldhen_config.h` -- reads GoldHEN's "Enable Plugins Loader" value from `/data/GoldHEN/config.ini` (read-only); Home puts it ahead of every other check
- `include/install_plan.h` -- what Home shows and what its one button does, once the shell feed is part of it (header-only, no console calls)
- `include/payload_ini.h` -- reads and flips the Ambilight payload's entry in GoldHEN's payload AutoRun list without touching other lines
- `include/claim_file.h` -- the `/data/ps4-ambilight/ps4_ambient_state` record and heartbeat the app writes while it drives the strip
- `include/update_source.h` -- cleans up `[dev] update_base_url`
- `tests/test_settings.c`, `tests/test_pipeline.c`, `tests/test_display_units.c`, `tests/test_presets.c`, `tests/test_install_plan.c`, `tests/test_goldhen_config.c`, `tests/test_payload_ini.c`, `tests/test_claim_file.c`, `tests/test_shell_settings.c`, `tests/test_action_buttons.c`, `tests/test_app_version.c`, `tests/test_ime.c`, `tests/test_github_release_json.c` -- isolation tests; `gcc -Iinclude -o test_x test_x.c source/x.c source/config.c tests/test_sce_stubs.c` to rerun (exact commands in `docs/development/testing.md`; `test_settings` still fails at its `relayHost` check, as it did before the shell feed)
- `tools/ui_preview.c` / `tools/ui_preview.sh` -- renders the UI to PNG on a PC
