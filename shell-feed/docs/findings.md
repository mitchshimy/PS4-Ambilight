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
| Disable and resume work three ways: `[shell] enabled=false` in the ini, the `/data/ps4-ambilight/ps4_shell_feed_off` file (twice), each followed by `STANDBY` then `RESUMING` and a fresh `INI RELOAD` and `LIVE`. | Capture. Every resume came back within about a second of the ini or file change. |
| Per-frame cost is steady: avg about 1.9-2.1 ms, max under 3.7 ms at 30 Hz, `sent` always equal to the tick count, no skips. | Capture: the periodic `LIVE t=` lines (the last stretch, t=5s to t=85s, shows it). |
| 30 minutes in rest mode, no crash, and the feed was correct afterwards. | **Person report only, and the later captures don't change that.** `capture_2.log` ends as the console starts suspending (`Standby Start: Fri, 09 Oct 2026 09:42:04 +0300`, then `System Suspend start`). The serial output stops in rest, so neither the 30 minutes nor the wake is in any log. What the capture does show is the feed healthy right up to that point (next section). |

## Game/shell ownership handoff: output-driven claim (follow-up fix)

The game plugin's state writer previously based `fg=1` on recent flip-hook counter
movement, while `ambient_sample_thread` could continue sampling and submitting DDP from
its selected buffer even during a quiet interval in those counters. That mismatch could
release shell-feed while the game sender remained active. The state writer now watches
`g_wledOutputCount`, incremented only when `sendto()` accepts a complete WLED DDP
datagram, and keeps `fg=1` while the title is foregrounded and successful output has
been observed within the last 2.2 seconds. If sends stop, it emits one `fg=0` before the
shell reader's 2.5-second stale deadline; backgrounding still emits `fg=0` immediately.

Host tests cover output continuing with flip counters unchanged, flips without successful
DDP output, backgrounding while output is active, and sender-stall release. These tests
verify the contract on a host, not actual WLED delivery or the visual symptom. A console
capture is still needed to confirm whether this removes the intermittent flicker.

## Stage D: one smoothing implementation (merge plan, after step 14)

The per-zone smoothing used to exist twice: inline in the plugin's
`sample_thread.c` pass 3, and pasted into `shell-feed/source/main.c`. It is
now two functions in `plugin/include/smoothing.h` (`smoothing_alpha()`,
`smoothing_step()`), header-only and `static inline`, so neither Makefile's
source list changed. Each caller keeps its own state array and valid flag.

| Finding | How established |
|---|---|
| The new functions are bit-identical to the code they replaced. | `tools/test_smoothing.c` keeps a verbatim copy of the old code as the reference. It compares every update rate 1-240 Hz against settling 0-5000 ms (1,200,240 alpha cases) and every previous value x target x alpha 0-256 x valid/not-yet-valid (33,685,504 step cases): 0 differences. |
| The test can fail. | Removing the v2.7.5 minimum-step line from a scratch copy of the header makes it report 1,595,571 failures. |
| The v2.7.5 "dim residual glow" fix still holds: alpha 83 fading to black reaches exactly 0, and any fade with alpha >= 1 reaches its target. | Same test (26,832 fade cases plus the alpha-83 case). |
| shell-feed's refactored span still compiles, links and runs against the real sister files. | `run_stagec_core_test.sh`, all checks passed after the change. |
| The plugin's `sample_thread.c` still compiles after the change. | `gcc -fsyntax-only` against the host stubs. This is a syntax check only: the PS4 SDK isn't available here, so neither module was built with the real toolchain. |
| Console check after this change: smoothing looks right on the strip, and a full game session ran with six clean handoffs. | **Person report** for the visual part; the session itself is in the capture (see "A full session" below). Still not covered: a deliberate fade-to-black test for the stuck dim glow. The missing build-identifying log line is now added (see "Build identity"); it takes effect from the next build. |

## A full session, then entry into rest (`capture_1.log`, `capture_2.log`)

Two PuTTY captures from the same session, one 9.00 console. Which build was
running is **not provable from these captures**: they predate the shell feed's
`BUILD:` log line (below), and the plugin does not print its commit to the
serial log. What can be said is in the "Build identity" table below.
`capture_1.log` stops at about 310 s of feed uptime;
`capture_2.log` is byte-identical for that stretch and then runs on through the
Quick Menu and the start of standby.

| Finding | How established |
|---|---|
| The feed never dropped a frame: `skipped=0` in all 71 periodic reports across both captures, and `sent` rises by exactly 150 per 5 s (30 Hz) through to t=355 s. | Capture: every `LIVE t=` line. |
| Per-frame cost is flat through a real game session and the Quick Menu: avg about 1.6-2.4 ms, max under 2.7 ms in normal running, 30 Hz budget 33 ms. Isolated spikes of 11.2 ms (t=120 s, during claim handoffs) and 6.5 ms (t=350 s, as suspend begins). | Capture: `LIVE t=` lines. |
| A real game session handed the strip over and back six times: six `CLAIM ACQUIRED` / `CLAIM RELEASED` pairs. The game's plugin owned 5,780 ticks (about 193 s) by t=300 s, with `skipped=0` throughout. | Capture. |
| One release was a **timeout**, not an explicit `fg=0`: acquired at t=128,901 ms (seq=10), released by timeout at t=192,296 ms, then re-acquired about 1 s later at seq=69. `seq` advanced by 59 across that gap, so the game side kept writing; the heartbeat looked stalled to the reader for one window and recovered by itself. The shell feed would have been sending alongside the game for about a second. Cause not established from the log. | Capture: the `CLAIM` lines. Worth watching in a longer session; no visible effect was reported. |
| Smoothing, now shared (`smoothing.h`), behaved as expected on the strip: nothing wrong seen on the LEDs. | **Person report** (visual). The capture can't show LED output. That the Stage D plugin was the one on the console is likely but not proven by the capture; see "Build identity". |
| The feed ran up to the moment the console began suspending, with the strip going to black as the UI left the screen (`first3` goes to `000000` from t=345 s). | Capture: last `LIVE t=` lines, then `PowerManager.RequestStateChange state:Suspend` and `Standby Start`. |

### Build identity

| Finding | How established |
|---|---|
| The `ps4_ambient_light.prx` the person built and supplied carries the full commit hash `ec9d4c76d5e0f482d4e1a615e9fc2084e2922318`, which is the Stage D commit (`ec9d4c7`, "smoothing: share one implementation between the plugin and shell-feed"). | Read from the binary's embedded `GIT_COMMIT` string, which the Makefile writes from `git rev-parse HEAD` at build time, and matched to this repo. The hash covers the author, so it also shows the build came from the repo with the corrected author, not an earlier checkout. |
| `ps4_shell_feed.prx` carries no commit string in the binary, but the build that followed (see the last row) prints one at startup, and it names the tree below. The first shell feed build was made in the same session as the plugin, from a clean tree (`rm -rf build ../bin` first, so no stale objects). | Build log supplied by the person; for the later build, the capture in the last row. |
| Limits of the hash: it is HEAD when `make` ran, so uncommitted edits in the working tree are not reflected; and nothing here ties these two files to the moment of the captures, only to the same development session. | Reasoning, not a measurement. |
| The shell feed now prints its own identity at startup: `[SHELLPROBE24] BUILD: commit=... branch=... num=... built=... (Release)`, from the same `git_ver.h` the plugin uses. Checked on a PC with a stand-in `git_ver.h` (falls back to `unknown` for missing fields) and with a real-shaped one. | Added in this commit. |
| **Confirmed on a console:** a ~2 minute capture (`putty.log`, 2026-10-09 10:00) shows `BUILD: commit=ba2663ec8e2572b8f1f746a14439f27e590f2c08 branch=main num=113 built=Oct 09 2026 @ 09:56:15 (Release)`. `num=113` is that commit's depth in the history. The same run: 229 zones, both buffers found, `preset_game`, `skipped=0` in all 24 periodic reports, timing about 1.8 ms average with one 11 ms spike during a handoff, four claim handoffs all ending by explicit `fg=0` (no timeout), no faults. The capture ends at about 120 s, so it adds nothing on rest mode. | Capture. **Note on the hash:** `ba2663e` is the commit this change was first made as; the commit was then amended to add this very paragraph, so that exact hash no longer exists in the repo. The amend touched only `shell-feed/docs/findings.md`, so the source that was built is identical to the source in the current commit. |

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
smoothing) were still verbatim, `0 not verbatim`, when this was written;
Stage D has since removed both (see its section above), so there is nothing left to compare.

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
  and the smoothing step (`smoothing.h` since Stage D; a pasted copy when
  this check was first written), in that order -- comes back
  colour-processed rather than black.

What this can't check: a real compositor buffer, a real console, a real
strip. That was merge plan step 14, since run on a console -- see "Stage C
on a real console" above.
