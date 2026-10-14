# shell-feed: architecture

`shell-feed` is a second GoldHEN plugin, built alongside `plugin/` from this
same repo, that drives the LED strip from the PS4's shell compositor
(`GnmCompositor`) instead of from inside a game. It exists so the strip stays
lit on Home, Settings, Store and the Library, not just while a game is
running.

## Why a second plugin instead of one plugin that does both

The sister plugin (`plugin/`) hooks into a game's own process. The shell
compositor is a different process, found at runtime by name/size
(`CompositorSeverWork`, two buffers, resolver handle `0x2001` -- stable
across four independently captured boots, not hard-coded). Driving both from
one `.prx` would mean one module hooking two unrelated processes under two
different load paths. Two plugins, coordinating through a small state file,
keeps each one's hooks specific to the process it actually targets.

## Who drives the strip, and when

| Situation | Driver |
|---|---|
| Home, Settings, Store, Library | shell-feed |
| Game launching | shell-feed, until the game's first flips, then the game |
| Game running (quick menu / toast / Share menu included) | **the game**, always |
| PS-button suspend to Home | shell-feed (the sister goes silent when backgrounded) |
| Game quit | shell-feed, after the claim below expires (up to ~2.5 s) |
| Netflix / YouTube | the game path (they're title processes) |
| Rest mode | not supported yet -- see support-matrix.md |

shell-feed only sends when the compositor's own flips are sustained (roughly
12+ in a 250 ms window; Home runs ~60/s) **and** no game claim is fresh.
Idle means silent, not black.

## The claim file (how the two plugins hand off)

The sister plugin's `state.c` writes a small fixed-width text record to
`/data/ps4-ambilight/ps4_ambient_state` in place (no truncate, so a reader never sees a
half-written file):

```
v1 fg=1 seq=000123 title=CUSA00000
```

- `fg=1` and a rewrite (`seq` incrementing) about once a second means the
  game is foregrounded and successful WLED DDP sends are still being submitted.
  It follows output progress rather than flip-hook freshness because the sampler can
  keep sending from its selected buffer during a temporary gap in flip callbacks.
- `fg=0`, written once on backgrounding or after 2.2 seconds without a successful
  DDP send, means the game has let go. The 2.2-second window is shorter than the
  reader's 2.5-second stale-claim timeout.
- Historically, v3.9.3 moved the first claim from up to one second after load to the
  first flip (`89199fe`) to narrow a launch-time overlap. The current rule supersedes
  flip-based ownership: the game claim follows successful DDP send progress.

shell-feed polls this file every 50 ms. A claim counts as fresh only after
the reader has *seen `seq` change* -- a leftover record from a previous boot
never arms a claim on its own, so `/data` surviving a reboot is harmless.
Missing, empty or unparsable file means "no claim": shell-feed runs
standalone rather than going dark.

## Shared code vs. shell-feed's own code

shell-feed uses the sister plugin's own colour pipeline -- `tiling.c`,
`pixel_formats.c`, `zones.c`, `gamma.c`, `color_processing.c`, `settings.c`
and `config.c` -- compiled from `../plugin/source/`, the same files the
sister ships, not a copy. This matters because of a real bug those files
fix: the sister's actual zone geometry puts LED index 42 (not 0) at the
first zone nearest the top-left corner, confirmed on a PC against the
person's real ini across all four edges. An earlier hand-rolled probe that
didn't use this code drove the strip rotated by 42 LEDs the whole time --
visually hard to notice because Home and Settings are mostly one colour, but
wrong. Following the ini, via the shared files, fixes this with no new code.

The shared files only need 11 trivial symbols from outside themselves (all
either hard-coded to a safe default or a no-op in shell-feed's context):
`g_autoLetterbox{Top,Right,Bottom,Left}`, `g_readableLimit`,
`g_gpuOnlyRemap`, `g_isBackgrounded`, `g_reportEnabled`,
`g_smoothedRgbValid`, and the no-op functions
`relay_send_external_source`, `send_config_reload_debug_packet` and
`ambient_read_content_preview`. The relay call being a no-op in shell-feed
is intentional, not an oversight -- see "Decisions in force" below.

shell-feed's own code (module entry/compositor hooks, the flip-rate idle
logic, the claim reader, DDP send) stays local to `shell-feed/source/`.

## Decisions in force

1. **No relay.** Only these two plugins exist in this design. The sister's
   `relay_*` code is untouched and stays off.
2. **The shell always uses the game preset** -- same sampling, colour chain
   and smoothing a game would get, not a separate "movie" look.
3. **Strip settings follow the plugin ini.** No host, LED counts or order
   hard-coded in shell-feed.
4. **A foregrounded game always wins**, overlays included.
5. **This repo (PS4-Ambilight) is the main repo** for the merge; the
   research repo stays the lab notebook and keeps the full capture/probe
   history.

## Status of this code at merge time

As of Stage C, this document's "shared code" section is literally true of
the build, not just the design: `shell-feed/source/main.c` now
`#include`s the sister's real `ambient_internal.h`/`config.h`/
`preset_select.h` and is compiled (via `Makefile`'s second source list)
against the sister's real `tiling.c`, `pixel_formats.c`, `zones.c`,
`gamma.c`, `color_processing.c`, `settings.c` and `config.c`, not a pasted
copy. `g_presetOverride` (added to `settings.c` alongside this refactor)
is what makes it possible to use the sister's own `ambient_load_config()`
unmodified instead of re-implementing ini parsing here. This module still
keeps, unmodified from the original `shell_probe_v24` probe: self-heal (30
bad buffer checks in a row before a re-scan, 10 s cooldown doubling to
60 s), a per-gate `GATE:` status line, an optional
`/data/ps4-ambilight/ps4_ambient_report_SHELL.txt` report file, an optional `[dev]` UDP
mirror, its own hardware-verified v17 tiling math (used by the
buffer-discovery/self-heal code, unrelated to the zone sampler above).
Stage D then replaced its pasted copy of the sister's per-frame smoothing
(and the sister's own inline copy in `sample_thread.c`) with
`plugin/include/smoothing.h`, a header-only pair of functions both call;
each process keeps its own smoothing state, only the arithmetic is shared.

Verification done for this stage, without hardware: `shell-feed/tools/
run_stagec_core_test.sh` compiles and links the exact span of
`source/main.c` that now calls into the shared files -- the v17 tiling
math through `shell_process_frame()` -- against those real, linked sister
files, and runs it against a real ini fixture. It confirms the real
`TileParams` struct matches the probe's own hardware-verified tiling
parameters byte for byte, `g_presetOverride` forces `[preset_game]` through
the real `ambient_load_config()` even with `active=movie` in the ini, the
real `buildZoneGeometry()` reproduces the same 229-zone/LED-42-rotation
geometry the research repo's `host_zone_check` already found, and a
synthetic frame samples/colour-processes/smooths correctly end to end (smoothing now via `smoothing.h`). What
this can't prove -- a real compositor buffer, a real console, a real strip
-- was the hardware step (merge plan step 14: cold boot,
disable/resume, a game launch, rest mode). That step has now been run on a
console and passed; see `findings.md` for what was captured and what is
person-reported. See `loading.md`, `findings.md` and `support-matrix.md` in this
same directory for the loading runbook, distilled findings and support
matrix carried over from the research repo.
