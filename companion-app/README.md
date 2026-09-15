# ps4_ambient_light Companion App

A standalone PS4 homebrew application (not a GoldHEN plugin) for
editing `/data/ps4_ambient_light.ini` through a controller-driven UI
with live preview on your real WLED light, and for self-updating the
plugin binary from your GitHub repo. See
`ps4-ambient-light-handoff.md` §51+ for the full design rationale.

## What's actually verified vs. not (read this before building)

This project follows the same discipline as `ps4_ambient_light` and
`detile_verify_probe` throughout their whole history: verify against
real sources before trusting, and say plainly what hasn't been
checked yet rather than implying it has.

**Verified in this session, against real code:**
- `color_pipeline.c` — the gamma LUTs, per-channel gamma builder, and
  the full processing chain are copied byte-for-byte from
  `ps4_ambient_light` v2.2's actual `main.c`, then compiled and run in
  isolation on a Linux sandbox. Cross-checked one LUT entry (gamma
  2.2 at input 128 → 56) against this project's own earlier
  independent Python calculation from a prior session — matches
  exactly.
- `settings.c`/`config.c` — the full 32-key ini schema was extracted
  directly from `ps4_ambient_light`'s real `ini_table_get_entry*`
  call sites, not recalled from memory. Round-trip tested (save →
  reload → compare) for both default and modified values, and a
  partial-ini test confirms missing keys correctly fall back to
  defaults, matching the real plugin's own behavior.
- `ddp.c` — copied from the plugin's own `wled_send_rgb_zones`,
  unchanged wire format.
- Every `orbis/*` API call in `main.c` (pad input, HTTP/SSL/Net,
  Sysmodule) was checked against this SDK's real headers and the real
  `samples/input` and `samples/net_http` sample source — not written
  from memory of what the APIs "should" look like. This caught and
  fixed three real mistakes during the build: a nonexistent
  `ScePadOpenParam` type and `ORBIS_USER_SERVICE_USER_ID_SYSTEM`
  constant (neither exists in this SDK), and the wrong controller-data
  struct name (`OrbisPadData`, not `ScePadData`).
- **Text rendering.** SDL2_ttf was never actually linkable in this SDK
  snapshot (header only, no compiled lib). Resolved by going straight
  to FreeType: `source/text_render.c` rasterizes the printable ASCII
  range into a texture atlas at startup via real `ft2build.h` /
  `FT_FREETYPE_H` calls against `libSceFreeType` (already linked in
  `build.bat`), and `draw_text()` in `main.c` calls into it. This
  compiled and linked successfully with the real OpenOrbis toolchain —
  not just checked against headers.
- **A real build has been compiled, deployed, and run on a real PS4.**
  Confirmed by a screenshot of the actual Settings screen running on
  console: all section headers, labels, and values rendering correctly
  through the FreeType text path, the config genuinely loaded from
  `/data/ps4_ambient_light.ini` (status line reads
  `Loaded /data/ps4_ambient_light.ini`), and the 5-swatch live preview
  (Red/Green/Blue/White/Gray) rendering with visibly correct color
  processing.
- **Controller navigation: confirmed fixed on real hardware.** The
  `SDL_INIT_JOYSTICK` removal (see v5 note below) was the actual fix —
  the same screenshot shows the "LEDs: Top" row highlighted, meaning
  D-Pad navigation is genuinely moving the selection, not just
  compiling cleanly.

**v5 update — first real hardware run, two symptoms reported:**
- Controller navigation didn't move the selection highlight. Code
  audit found `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)` enabled
  SDL's joystick subsystem even though it's never used anywhere in
  this file (no `SDL_JoystickOpen` call exists — all input goes
  through the native `scePad` API). That's a known real contention
  pattern: SDL's joystick layer and a separately-opened native
  `scePad` handle can both claim the same DualShock HID device, and
  `scePadReadState` can then return success on a handle that never
  actually receives fresh button state. Removed `SDL_INIT_JOYSTICK`
  (it cost nothing — dead code) and added `printf` debug lines in
  `pad_init()` and the main loop's read call so a serial-console
  capture (the same tool that produced `putty.log`) will show exactly
  what `scePadInit`/`scePadOpen`/`scePadReadState` are returning and
  what the raw button mask looks like on a press, if this doesn't
  fully resolve it. **Confirmed fixed as of the v6 hardware run above
  — the debug `printf`s are still in place and harmless to leave, but
  this is no longer just the top suspect.**
- LEDs only lighting "one part of the corner": this is the preview's
  actual designed behavior, not a bug. `update_live_preview()` sends
  exactly 5 RGB triplets at DDP pixel offset 0 — the swatches are a
  quick correctness check of the color pipeline, not a full-strip
  demo. Offset 0 always lands at the physical start of the strip
  (wherever `startCorner` puts pixel index 0), so only that corner
  lights up. If you want the preview to visibly span the whole strip
  instead, that's a small, separate change (spread the 5 swatches
  across `ledCountTop+Right+Bottom+Left` instead of packing them at
  offset 0) — say the word and it can be added.

**v6 update — real hardware run, everything tested so far works as
intended.** Worth a quick look, not flagged as broken: the screenshot
shows `black_level=3`, where earlier sessions on the plugin side had
it at the default `0`. Not addressed here since it wasn't reported as
a problem — just noting the difference in case it was unintentional
rather than a deliberate tuning change.

**NOT verified — genuinely open, not glossed over:**
1. **`PLUGIN_UPDATE_URL` in `main.c` is a placeholder** — replace
   `YOUR_USERNAME/YOUR_REPO` with your actual repo before building.
2. **On-screen keyboard for `wledHost` is now implemented** (Cross on
   that row opens it), using the real `sceImeDialogInit` /
   `sceImeDialogGetStatus` / `sceImeDialogGetResult` /
   `sceImeDialogTerm` sequence and `OrbisImeDialogSetting` struct from
   `orbis/ImeDialog.h` — pulled directly from that header, not written
   from memory. `sceCommonDialogInitialize()` runs once at startup, and
   the app's own input handling is gated behind the real
   `sceCommonDialogIsUsed()` while the keyboard is up, so it's not
   fighting the system dialog for the same button presses.
   **Correction to this file's own earlier claim:** `samples/keyboard`
   in this SDK is *not* an IME-dialog example — it's a *physical*
   USB/Bluetooth keyboard API (`orbis/Keyboard.h`), a different thing
   entirely. There's no working IME-dialog sample in this SDK at all,
   so the following are implemented from the header alone and **not
   yet confirmed on real hardware**:
   - Passing `NULL` for `sceImeDialogInit`'s second
     (`OrbisImeSettingsExtended*`) argument — plausible (`NULL` =
     defaults is a common pattern elsewhere in this SDK) but unverified.
   - `posx`/`posy`'s units and center-anchor behavior (assumed screen
     pixels, centered at 960,540 for this app's 1920x1080 window).
   - `supportedLanguages=0` as "default/unrestricted" — not documented
     in this header, just the least-surprising guess.
3. **Settings list now scrolls.** Previously clamped to whatever fit on
   one screen — and that clamp's own comment claiming "fine for 32
   items at the current row height" was wrong: with all 4 sections'
   headers counted, only the first 20 of the 32 items were ever drawn;
   the rest were selectable (D-Pad Down still moved `g_selectedIndex`
   onto them) but invisible. `g_scrollOffset` now tracks the selection
   via `settings_last_visible_index()`, which mirrors
   `render_settings_screen()`'s row math to decide when to scroll, plus
   a small "-- showing X-Y of N --" indicator when the list is cut off
   in either direction. Still not exercised on real hardware.
4. **The plugin self-update path itself (Triangle: Update Plugin)
   hasn't been exercised yet** — the HTTP/SSL/Net API calls were
   checked against real SDK samples, and the build links successfully,
   but no one has actually pressed Triangle on real hardware and
   confirmed a `.prx` downloads and installs correctly.

## Building

Needs `OO_PS4_TOOLCHAIN` set up per OpenOrbis's normal setup, run from
inside this folder: `build.bat <intermediate_dir> ps4_ambient_light_companion <output_dir>`.
Text rendering goes through `source/text_render.c` (real FreeType, via
`-lSceFreeType`) rather than SDL2_ttf, which isn't a compiled lib in
this SDK — this is already wired up in `build.bat`, confirmed building
and running on real hardware.

## Files

- `source/main.c` — app entry, SDL2 UI, controller nav, HTTP self-update
- `source/text_render.c` / `include/text_render.h` — FreeType-based text rendering (see above; not SDL2_ttf)
- `source/settings.c` / `include/settings.h` — ini schema + load/save
- `source/color_pipeline.c` / `include/color_pipeline.h` — live preview math (verbatim from the plugin)
- `source/ddp.c` / `include/ddp.h` — WLED UDP sender (verbatim from the plugin)
- `source/config.c` / `include/config.h` — ini_table parser (verbatim from `plugin_loader`, same as the plugin itself uses)
- `test_settings.c`, `test_pipeline.c` — the isolation tests referenced above; `gcc -Iinclude -o test_x test_x.c source/x.c source/config.c test_sce_stubs.c` to rerun