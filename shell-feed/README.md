# shell-feed

A second GoldHEN plugin, built from this same repo, that drives the LED
strip from the PS4 shell compositor instead of from inside a game. See
`docs/architecture.md` for how it works and how it hands off to `plugin/`,
`docs/loading.md` for the AutoRun/L1 runbook, `docs/findings.md` for what's
been established and how, and `docs/support-matrix.md` for firmware
coverage.

**Provenance.** `source/main.c` started as a byte-for-byte import from the
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
that possible without reimplementing ini parsing here. The two
`sample_thread.c` smoothing blocks are the one piece still intentionally
pasted -- that's Stage D, a separate, later change, since it's code in a
game's hot path.

**Status.** Builds with the same toolchain and link pattern as `plugin/`
(see `Makefile`), now with a second source list for the shared files.
`shell-feed/tools/run_stagec_core_test.sh` compiles and runs the exact
refactored span against the real linked sister files and a real ini
fixture (not the PS4 SDK, which isn't available in this environment) --
see `docs/architecture.md`'s status section for what it checks and what it
found. This module has not yet been built against the real PS4 SDK or run
on a console; that's the hardware step the merge plan calls for next
(step 14: cold boot, disable/resume, a game launch, rest mode), and it's
the one thing in this module nothing on a PC can substitute for.

**Firmware:** 9.00 only. See `docs/support-matrix.md`.
