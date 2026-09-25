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
- LED output goes through `layout.c`, converting the configured strip
  layout into one color per configured LED in real physical wire
  order (not a fixed sample-swatch layout).
- The Help screen stays deliberately minimal (five short cards --
  see `ui_screens.c`'s own comment above `help_cards_init()`), since
  there's no room here for the level of detail this repo's own
  README goes into. Triangle on that screen instead opens a QR-code
  popup (`help_qr.c`, drawing through `ui_fill_rect_fast()` in
  `render_qr_modal()`, `ui_screens.c`) pointing at this repo's own
  README `## Help` section, for anyone who wants the long version
  without typing a URL on a controller.
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
2. The plugin self-update path (Triangle: Update Plugin, now pointing
   at this repo's GitHub releases) has the HTTP/SSL/Net calls checked
   against SDK samples and builds successfully, but hasn't been
   exercised end-to-end.
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

## Files

- `source/main.c` -- app entry, SDL2 UI, controller nav, HTTP self-update
- `source/text_render.c` / `include/text_render.h` -- FreeType-based text rendering (not SDL2_ttf)
- `source/settings.c` / `include/settings.h` -- ini schema + load/save
- `source/color_pipeline.c` / `include/color_pipeline.h` -- live preview math (mirrors the plugin)
- `source/ddp.c` / `include/ddp.h` -- WLED UDP sender (mirrors the plugin)
- `source/config.c` / `include/config.h` -- ini_table parser (shared with the plugin)
- `source/layout.c` / `include/layout.h` -- full-strip LED layout geometry (physical wire order + on-screen position)
- `source/help_qr.c` / `include/help_qr.h` -- builds and caches the Help screen's QR bitmap (points at this repo's README `## Help` section)
- `source/qrcodegen.c` / `include/qrcodegen.h` -- vendored, unmodified: [Project Nayuki's QR Code generator library](https://github.com/nayuki/QR-Code-generator) (C edition, MIT license)
- `tests/test_settings.c`, `tests/test_pipeline.c` -- isolation tests; `gcc -Iinclude -o test_x test_x.c source/x.c source/config.c tests/test_sce_stubs.c` to rerun
