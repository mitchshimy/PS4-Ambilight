# shell-feed

A second GoldHEN plugin, built from this same repo, that drives the LED
strip from the PS4 shell compositor instead of from inside a game. See
`docs/architecture.md` for how it works and how it hands off to `plugin/`,
`docs/loading.md` for the AutoRun/L1 runbook, `docs/findings.md` for what's
been established and how, and `docs/support-matrix.md` for firmware
coverage.

**Provenance.** `source/main.c` (since split, see Source layout below) started as a byte-for-byte import from the
research repo (`ps4-shell-capture-research`),
`overlays/shell_probe_v24-overlay/plugin_src/shell_probe/source/main.c`, at
tag `handoff-v55` (commit `eb7c8be`), sha256
`fda1cc60cff554107cce71df9f2d9cfde752cc47fd7a60ce158f9d2c20fdbc84` (Stage A:
a pure move, no behaviour change). Stage C has since landed on top of that:
this file's own pasted copies of the colour pipeline and its own ini reader
are gone, replaced with real `#include`s of the sister's headers and a
build (`Makefile`'s second source list) against this repo's real
`plugin/source/tiling.c`, `pixel_formats.c`, `zones.c`, `gamma.c`,
`color_processing.c`, `settings.c` and `config.c`. `g_presetOverride`
(added to `plugin/source/settings.c` alongside this refactor) is what makes
that possible without reimplementing ini parsing here. Stage D then moved
the per-zone smoothing, which had been pasted here and written inline in
`sample_thread.c`, into one shared header, `plugin/include/smoothing.h`;
both now call it (see `docs/findings.md` for how that was checked).

**Status.** Builds with the same toolchain and link pattern as `plugin/`
(see `Makefile`), now with a second source list for the shared files.
An earlier host check of the refactored span, `shell-feed/tools/run_stagec_core_test.sh`
(removed from the repo, last present at `b53e9b0`), is described in
`docs/architecture.md`'s status section, with what it checked and what it found.

**Source layout.** `source/main.c` is the entry point only (`module_start`, `module_stop`). The rest is
split by job, the way `plugin/source/` is, and everything the files share is declared in
`include/shell_internal.h` (which also has the file map):

| File | What it holds |
|---|---|
| `shell_modules.c` | which modules are loaded and where their symbols are |
| `shell_hooks.c` | the RegisterBuffers / SubmitFlip hooks, installing them, the positive-control calls |
| `shell_buffers.c` | memory-map scan, display-buffer matching, TSC calibration, self-heal |
| `shell_config.c` | the ini as the shell reads it: load, live reload, the `[shell] enabled=` poll |
| `shell_frame.c` | one frame: sample the zones, colour chain, smoothing |
| `shell_state.c` | the game plugin's foreground bit (read) and the shell feed's heartbeat file (written) |
| `shell_log.c` | log lines, the `[dev]` UDP mirror, console notifications |
| `shell_report.c` | the compile-time gates, the report file and its thread |
| `shell_live.c` | `live_send()`, the send loop |
| `shell_thread.c` | `probe_thread()`: start-up sequence and the standby waits |
| `shell_shim.c` | the symbols the shared plugin files expect from files this module does not link |
| `shell_probe17.c` | the probe's original v17 sampler. Legacy: only `build_points()` still runs |

Every source here is named `shell_*.c` on purpose: the Makefile builds the plugin's `config.c`, `zones.c`,
`settings.c` and others into the same `build/` folder, and a same-named file would overwrite their `.o`.
The names in the history below (`live_send()`, `map_scan()`, ...) are where those functions are now, not
where they were.

**Hardware status (merge plan step 14, done 2026-10-09).** Built and run on
a real 9.00 console with the Stage C code. Cold boot, disable/resume, a game
launch and rest mode all passed; the strip colours were correct; and the
preset the shell feed uses was `game` regardless of `active=` in the ini.
Rest mode: 30 minutes in rest with no crash (person report -- it is not in
the captured log, which has no rest/wake lines). Everything else above is
in the capture: see `docs/findings.md` for the line-by-line evidence.
The step 14 results above are for the build before Stage D. Stage D (shared
smoothing) changed `plugin/source/sample_thread.c` as well; a later session
on the Stage D build looked right on the strip and ran a full game with clean
handoffs (person report plus capture, see `docs/findings.md`). The rest-mode
result is still a person report: the serial log goes dark in rest.

**Firmware:** the current standalone Ambilight payload and shell feed are confirmed working on PS4
firmware **9.00 and 12.52**. Other versions are untested (the payload has an entry for them) or not in its firmware list (it will most likely refuse); see
`docs/support-matrix.md`. The main repository ships the loader binary as
`payloads/ps4-ambilight.bin`; its implementation source remains in a separate repository and is not
vendored here.
