# ps4_ambient_light Companion App

A standalone PS4 homebrew application (not a GoldHEN plugin) for
editing `/data/ps4_ambient_light.ini` through a controller-driven UI,
with a live color preview against your real WLED light, and for
self-updating the plugin binary from your GitHub repo.

## Design notes

- `color_pipeline.c`, `ddp.c`, and `config.c` mirror the plugin's own
  gamma/brightness/color pipeline, WLED UDP sender, and ini parser
  respectively, so the live preview matches what the plugin actually
  sends.
- `settings.c`/`config.c` implement the full 32-key ini schema used
  by the plugin's own `ini_table_get_entry*` call sites.
- **Text rendering** goes through FreeType directly
  (`source/text_render.c`, via `-lSceFreeType`) rather than SDL2_ttf,
  since SDL2_ttf isn't a compiled lib in the toolchain snapshot this
  targets. `draw_text()` in `main.c` calls into the resulting glyph
  atlas.
- Controller navigation uses the native `scePad` API, not SDL's
  joystick subsystem -- `SDL_INIT_JOYSTICK` is deliberately left
  disabled, since it can contend with a separately-opened `scePad`
  handle for the same DualShock HID device and cause
  `scePadReadState` to report stale button state.
- The settings list scrolls once it exceeds one screen's worth of
  rows (32 items across 4 sections); `g_scrollOffset` tracks the
  selection via `settings_last_visible_index()`, with a
  "-- showing X-Y of N --" indicator when the list is cut off.
- LED output goes through `layout.c`, a C port of the Android app's
  `LedLayoutGeometry.kt`, sending one color per configured LED in
  real physical wire order (not a fixed sample-swatch layout).
- On-screen keyboard for `wledHost` uses the real
  `sceImeDialogInit`/`sceImeDialogGetStatus`/`sceImeDialogGetResult`/
  `sceImeDialogTerm` sequence from `orbis/ImeDialog.h`.
  `sceCommonDialogInitialize()` runs once at startup, and input
  handling is gated behind `sceCommonDialogIsUsed()` while the
  keyboard is open so it doesn't fight the system dialog for input.

## Known limitations

1. `PLUGIN_UPDATE_URL` in `main.c` is a placeholder -- replace
   `YOUR_USERNAME/YOUR_REPO` with your actual repo before building.
2. A few IME-dialog details are implemented from the SDK header alone,
   without a matching sample to confirm against: passing `NULL` for
   `sceImeDialogInit`'s second (`OrbisImeSettingsExtended*`) argument,
   `posx`/`posy`'s exact units (assumed screen pixels, centered at
   960,540 for this app's 1920x1080 window), and
   `supportedLanguages=0` as "default/unrestricted."
3. The plugin self-update path (Triangle: Update Plugin) has the
   HTTP/SSL/Net calls checked against SDK samples and builds
   successfully, but hasn't been exercised end-to-end.

## Building

Needs `OO_PS4_TOOLCHAIN` set up per OpenOrbis's normal setup, run from
inside this folder:

```
build.bat <intermediate_dir> ps4_ambient_light_companion <output_dir>
```

## Files

- `source/main.c` -- app entry, SDL2 UI, controller nav, HTTP self-update
- `source/text_render.c` / `include/text_render.h` -- FreeType-based text rendering (not SDL2_ttf)
- `source/settings.c` / `include/settings.h` -- ini schema + load/save
- `source/color_pipeline.c` / `include/color_pipeline.h` -- live preview math (mirrors the plugin)
- `source/ddp.c` / `include/ddp.h` -- WLED UDP sender (mirrors the plugin)
- `source/config.c` / `include/config.h` -- ini_table parser (shared with the plugin)
- `source/layout.c` / `include/layout.h` -- full-strip LED layout geometry (physical wire order + on-screen position), a C port of the Android app's `LedLayoutGeometry.kt`
- `tests/test_settings.c`, `tests/test_pipeline.c` -- isolation tests; `gcc -Iinclude -o test_x test_x.c source/x.c source/config.c tests/test_sce_stubs.c` to rerun
