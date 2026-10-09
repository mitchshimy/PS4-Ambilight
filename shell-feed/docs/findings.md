# shell-feed: distilled findings

Carried over from `ps4-shell-capture-research/docs/integration_plan_v2.md`
and `HANDOFF.md`. Each row says how it was established.

| Finding | How established |
|---|---|
| The sister's real zone geometry puts the first zone (nearest top-left) at **LED index 42**, not 0. An earlier probe that didn't use the sister's real code drove the strip rotated by 42 of 229 LEDs the whole time. | Compiled the sister's real `settings.c`/`config.c`/`zones.c`/`gamma.c`/`color_processing.c`/`pixel_formats.c`/`tiling.c` on a PC against stand-in headers, fed the person's real ini, called the real `buildZoneGeometry()`. Re-run against this merged repo's checkout (`89199fe`) during this merge: same result. |
| The shared colour-pipeline files need exactly 11 outside symbols, all either a hard-coded safe default or a no-op in shell-feed's context. | Read directly from source while building the host zone/ini checks. |
| The compositor's two target buffers (`CompositorSeverWork`, 8,355,840 B each) and resolver handle `0x2001` are stable, found by name/size at runtime, not hard-coded. | Identical across four independently captured boots (v15-A, v16-A, v17-A, v19-A). Not tested across firmware versions or with a different title set. |
| The game-side claim record format (35 bytes, `v1 fg=1 seq=NNNNNN title=TITLEID`) is agreed on both ends. | Verified three ways on a PC: the sister's real `state_format()` fed into the probe's real parser; the probe's own 35-byte record accepted by the sister's real parser; byte layout matches the design doc. |
| `fg=1` is now written on the game's first flip rather than up to a second later (plugin v3.9.2 -> v3.9.3, `89199fe`), closing a 1-2 s flicker window at game launch where both plugins could send at once. | Read from the sister's commit history and `state.c`. |
| A leftover claim record from a previous boot never arms a claim on its own -- only an observed `seq` *change* arms it -- so `/data` surviving a reboot is harmless. | Design decision in v21, confirmed by re-running `tools/host_claim_check` against the current probe source: the previously-reported 2.5 s false-arm window at startup (v20) is gone. |
| Cold boot through to a live, sending probe, with no PC step, takes about 45 s and the probe keeps up (1,801/1,801 packets sent in the first 60 s, 0 skipped, 0 crash-like lines). | Real capture (v23-E), not a PC check. |
| The same running probe instance survives a PS-button suspend/resume (~13 s) with no reload and no skipped ticks, and (separately) survives several full rest/wake cycles with the correct colours restored on wake. | Capture v23-F (suspend/resume) plus person report (rest/wake cycles). |
| Two FTP reads of the sister's claim file across real sessions show a consistent ~1 Hz heartbeat (`seq` advancing by the elapsed seconds) through both a suspend/resume and a cold launch. | Captured directly: seq 35->36->67 over 31.5 s (+31), then 68->91 over 24.0 s, then a fresh cold launch restarting `seq` at 1. |

## Stage C on a real console (merge plan step 14)

Run 2026-10-09 on a 9.00 console, built from the Stage C tree. The capture
is `merged.log` (PuTTY serial log, 149 KB). Each row says what it rests on.

| Finding | How established |
|---|---|
| Cold boot reaches a live, sending feed: module hooks fire, the two `CompositorSeverWork` buffers (8,355,840 B each) are found by name/size, 229 zones sampled with 0 points outside the buffer, then `LIVE: sending ... 30 Hz`. | Capture: the `BUFS`, `SAMPLER`, `INI LOAD` and `LIVE` lines. |
| The shell feed uses `[preset_game]` regardless of `active=` in the ini. | Capture: every `INI LOAD` / `INI RELOAD` line reads `preset=preset_game`. That the ini's `active=` was something else is the person's report; the log can't show it. The PC check (`g_presetOverride` with `active=movie`) agrees. |
| Strip colours are correct. | Person report (visual). The log only shows sends succeeding, not what the strip showed. |
| Game launch hands the strip over and back: `CLAIM ACQUIRED` (seq=1) then `CLAIM RELEASED (explicit fg=0)`, and the feed resumes sending. | Capture. In that 197 s run, `gameOwned=215` ticks (3.6%) were left to the game's plugin, `skipped=0`. |
| Disable and resume work three ways: `[shell] enabled=false` in the ini, the `/data/ps4_shell_feed_off` file (twice), each followed by `STANDBY` then `RESUMING` and a fresh `INI RELOAD` and `LIVE`. | Capture. Every resume came back within about a second of the ini or file change. |
| Per-frame cost is steady: avg about 1.9-2.1 ms, max under 3.7 ms at 30 Hz, `sent` always equal to the tick count, no skips. | Capture: the periodic `LIVE t=` lines (the last stretch, t=5s to t=85s, shows it). |
| 30 minutes in rest mode, no crash, and the feed was correct afterwards. | **Person report only.** The capture has no rest or wake lines, so there is nothing in the log to cite for this one. |

## What this merge's own equivalence checks reproduced

Running the research repo's four PC-only host checks against this merged
repo's real checkout and the real `shell_probe_v24` source (the exact
equivalence proof the merge plan calls for in its step 6):

- `host_zone_check` -- confirms the 42-LED rotation finding above, against
  this checkout specifically.
- `host_ini_check` -- 17/17 provenance blocks verified byte-identical
  between the probe and the sister's real source; all four cross-ini
  reload/equivalence cases pass; probe-only hash/poll/aux-key checks pass.
- `host_claim_check` -- all claim-file read/poll/staleness cases pass
  against the v24 probe source (A through M).
- `host_v24_check` -- all 47 self-heal, gate-table, report-file and
  `[dev]`-mirror checks pass, including a real loopback UDP send/receive and
  a real report thread writing a real file.

All four ran clean on this repo's own file tree during this merge, not just
in the research repo.

## Stage C's own verification (merge plan step 13)

Once the pasted copies were replaced with real `#include`s and a real
second-source-list build, the research repo's `host_ini_check` could no
longer run its equivalence-harness step at all against `source/main.c` --
that tool assumes a self-contained single-file probe (true of Stage A/B's
shape), and `source/main.c` now legitimately depends on the sister's real
headers via a proper Makefile. Its *provenance* check still ran and still
means something: of the original 17 pasted blocks, 15 are gone (replaced by
the real files) and the remaining 2 (`sample_thread.c` smoothing alpha and
smoothing -- Stage D's concern) are still verbatim, `0 not verbatim`.

For everything Stage C actually changed, this merge added its own
equivalent: `shell-feed/tools/run_stagec_core_test.sh`, which extracts the
current shared-code span of `source/main.c` by marker (not a frozen
snapshot) and compiles + links + runs it against the real, linked
`plugin/source/{tiling,pixel_formats,zones,gamma,color_processing,
settings,config}.c` and a real ini fixture. Results:

- The real `TileParams` struct is 48 bytes, matching the probe's own
  hardware-verified `TileParams17` byte for byte -- the "same BASE
  parameters, verbatim formulas" finding above now holds for the literal
  linked struct, not just an asserted equivalence between two separate
  implementations.
- `g_presetOverride` forces `[preset_game]` through the real
  `ambient_load_config()` even with `active=movie` set in the ini
  (`scanDepth=2`, `brightness=200`, `settlingTimeMs=50` -- the game values,
  not movie's `4`/`100`/`200`).
- All four shell-only aux keys (`[shell] enabled=`, `[dev] dev_ip=`,
  `[dev] dev_logging=`, `[compat] report_file=`) still parse correctly
  through the second, separate ini read `shell_cfg_load()` still does for
  them.
- The real `buildZoneGeometry()` reproduces `g_numZones=229` (73+41+73+42)
  with the first zone at `(0, 1079)` -- the exact same geometry
  `host_zone_check` found above, now produced by shell-feed's own call
  site into the real linked function rather than a separate test harness.
- A synthetic frame run through `shell_process_frame()` -- the real
  `sampleZoneAverage()`, `applyColorProcessing()`, `applyDarkThreshold()`
  and this file's own (still-pasted, Stage D) smoothing block, in that
  order -- comes back colour-processed rather than black.

What this can't check: a real compositor buffer, a real console, a real
strip. That's merge plan step 14, still ahead of this code.
