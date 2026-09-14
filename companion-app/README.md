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

**NOT verified — genuinely open, not glossed over:**
1. **SDL2_ttf is not confirmed to link.** The header
   (`include/SDL2/SDL_ttf.h`) exists in this SDK, but no compiled
   `SDL2_ttf` library was found anywhere in it, and the real, working
   `samples/SDL2/build.bat` links `-lSDL2 -lSDL2_image -lSceFreeType`
   — never `-lSDL2_ttf`. `build.bat` in this project still links it
   so the gap shows up as a clear link error rather than a silently
   text-less UI. Three real ways to resolve, in the order I'd try
   them: (a) build SDL2_ttf from source against this SDK's real
   `libSceFreeType`, (b) rewrite `draw_text()` in `main.c` to call
   FreeType directly (confirmed present, more work), (c) strip text
   for a v1 and rely on rects/highlighting only (fastest).
2. **Nothing in `main.c` has been compiled with the real OpenOrbis
   toolchain or run on a PS4.** Every API call was checked against
   real headers/samples, which catches wrong names and signatures,
   but not runtime behavior, timing, or anything only a real build +
   real console can show.
3. **`PLUGIN_UPDATE_URL` in `main.c` is a placeholder** — replace
   `YOUR_USERNAME/YOUR_REPO` with your actual repo before building.
4. **On-screen keyboard text entry for `wledHost` isn't wired up.**
   Editing the WLED IP currently requires FTP; the settings screen
   shows a status message saying so rather than silently ignoring
   left/right on that field. `samples/keyboard` in this SDK has a
   real IME dialog example worth using for a follow-up.
5. **No scrolling** in the settings list — it's clamped to fit one
   screen height. Fine for 32 items at the current row height, but
   worth knowing if more settings get added later.

## Building

Needs `OO_PS4_TOOLCHAIN` set up per OpenOrbis's normal setup, run from
inside this folder: `build.bat <intermediate_dir> ps4_ambient_light_companion <output_dir>`.
Resolve the SDL2_ttf question above first, or the link step will fail
on those exact symbols.

## Files

- `source/main.c` — app entry, SDL2 UI, controller nav, HTTP self-update
- `source/settings.c` / `include/settings.h` — ini schema + load/save
- `source/color_pipeline.c` / `include/color_pipeline.h` — live preview math (verbatim from the plugin)
- `source/ddp.c` / `include/ddp.h` — WLED UDP sender (verbatim from the plugin)
- `source/config.c` / `include/config.h` — ini_table parser (verbatim from `plugin_loader`, same as the plugin itself uses)
- `test_settings.c`, `test_pipeline.c` — the isolation tests referenced above; `gcc -Iinclude -o test_x test_x.c source/x.c source/config.c test_sce_stubs.c` to rerun
