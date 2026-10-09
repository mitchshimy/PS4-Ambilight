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
that possible without reimplementing ini parsing here. Stage D then moved
the per-zone smoothing, which had been pasted here and written inline in
`sample_thread.c`, into one shared header, `plugin/include/smoothing.h`;
both now call it (see `docs/findings.md` for how that was checked).

**Status.** Builds with the same toolchain and link pattern as `plugin/`
(see `Makefile`), now with a second source list for the shared files.
`shell-feed/tools/run_stagec_core_test.sh` compiles and runs the exact
refactored span against the real linked sister files and a real ini
fixture (not the PS4 SDK, which isn't available in this environment) --
see `docs/architecture.md`'s status section for what it checks and what it
found.

**Hardware status (merge plan step 14, done 2026-10-09).** Built and run on
a real 9.00 console with the Stage C code. Cold boot, disable/resume, a game
launch and rest mode all passed; the strip colours were correct; and the
preset the shell feed uses was `game` regardless of `active=` in the ini.
Rest mode: 30 minutes in rest with no crash (person report -- it is not in
the captured log, which has no rest/wake lines). Everything else above is
in the capture: see `docs/findings.md` for the line-by-line evidence.
Those results are for the build *before* Stage D. Stage D (shared
smoothing) changed `plugin/source/sample_thread.c` as well, so it needs its
own console re-test -- see `docs/findings.md`.

**Firmware:** 9.00 only. See `docs/support-matrix.md`.
