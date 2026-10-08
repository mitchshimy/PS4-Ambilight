# shell-feed

A second GoldHEN plugin, built from this same repo, that drives the LED
strip from the PS4 shell compositor instead of from inside a game. See
`docs/architecture.md` for how it works and how it hands off to `plugin/`,
`docs/loading.md` for the AutoRun/L1 runbook, `docs/findings.md` for what's
been established and how, and `docs/support-matrix.md` for firmware
coverage.

**Provenance.** `source/main.c` is imported byte-for-byte from the research
repo (`ps4-shell-capture-research`), `overlays/shell_probe_v24-overlay/plugin_src/shell_probe/source/main.c`,
at tag `handoff-v55` (commit `eb7c8be`), sha256
`fda1cc60cff554107cce71df9f2d9cfde752cc47fd7a60ce158f9d2c20fdbc84`. This is
Stage A of the merge: a pure move, no behaviour change, still carrying this
file's own pasted copies of the colour pipeline and its own ini reader.
Stage C (replacing those with this repo's real `plugin/source/` files) is
tracked separately and has not landed here yet.

**Status.** Builds with the same toolchain and link pattern as `plugin/`
(see `Makefile`). PC-side equivalence between this file and the sister's
real code was re-checked against this repo's own checkout during the merge
using the research repo's host test tools (`host_ini_check`,
`host_claim_check`, `host_v24_check`, `host_zone_check`) -- all pass; see
`docs/findings.md` for the specific results. This file has not yet been
built against the real PS4 SDK or run on a console *under this repo's
Makefile*; that's the hardware step the merge plan calls for next.

**Firmware:** 9.00 only. See `docs/support-matrix.md`.
