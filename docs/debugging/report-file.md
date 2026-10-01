# The report file

Since v3.9.1 the release build can write one small text file per game,
`/data/ps4_ambient_report_<TITLEID>.txt`, for example `ps4_ambient_report_CUSA11395.txt`.
It is what a tester sends when a game doesn't work, instead of running a debug build and
capturing UDP. **It is off by default**, see below. Everything in it was already kept in plain globals in every build, only the
packets that send them are debug-only. It holds no personal data: no WLED address, nothing from
the ini.

## Turning it on

Add this to `/data/ps4_ambient_light.ini`, at the end of the file or anywhere a new section
can go:

```
[compat]
report_file=1
```

Without that key the plugin never writes the file, so an ordinary install is unaffected. The
ini is re-read while a game runs (the `config_reload_check_seconds` interval), so it can be
switched on mid-game; it is safest to add it before starting the game, so the first writes
are there. The companion app keeps the `[compat]` section when it saves the ini (tested), and
`report_file=0`, or removing the line and restarting the game, turns it off again. If the ini
already has a `[compat]` section (for `gpu_only_remap`), put the line in that one.

## Getting it

Copy it off the console with anything that can read `/data` (GoldHEN's FTP server, or a
file manager). Play the game for a minute first, so the file has something to say, and leave
the game running while you copy it if it crashes on its own.

## When it is written

At start, then whenever the stage, the format, or the guard or remap outcome changes, and every
60 s otherwise so the timing stays current. The change-driven writes are why it is useful for a
crash: a game that dies on its first sampling pass never reaches a last write, but the file on
disk still says how far it got. `report.c` has the reasoning.

It runs on its own thread, so a disk write never lands in the sampler's pass. After 3 failed
writes in a row it stops for the session.

## Reading it

Two real files, both from a console. Mortal Kombat 11 (`CUSA11395`) has a GPU-only buffer, so the
guard rejected it and the remap fixed it. EA Sports FC 26 (`CUSA52342`) has buffers the CPU can
read as they are, so neither the guard nor the remap did anything.

```
PS4 Ambilight report
Send this file when you report a game. It has no personal data in it.

plugin:         3.9.1
build:          6cc220d
title:          CUSA11395
stage:          running
uptime_s:       87

format:         0x88740000 (recognized)
buffers:        3
register_calls: 1
flip_calls:     gnm 3634, videoout 0, workload 3634
loop_passes:    2369
backgrounded:   yes
pass_us:        avg 2866, max 3263 (last 30 passes; 78 windows)

guard:          available yes, rejects 3, last_ret 2, last_bad_addr 0x49fec08000
remap:          setting 3, created 3, failed 0, passes 3, last_method mprotect, last_ret 0

end of report
```

```
PS4 Ambilight report
Send this file when you report a game. It has no personal data in it.

plugin:         3.9.1
build:          6cc220d
title:          CUSA52342
stage:          running
uptime_s:       84

format:         0x88740000 (recognized)
buffers:        3
register_calls: 1
flip_calls:     gnm 4652, videoout 0, workload 4652
loop_passes:    2309
backgrounded:   yes
pass_us:        avg 2233, max 3156 (last 30 passes; 76 windows)

guard:          available yes, rejects 0, last_ret 0, last_bad_addr 0x0
remap:          setting 3, created 0, failed 0, passes 0, last_method none, last_ret 0

end of report
```

The last line is always `end of report`, so a cut-off or half-written file is easy to tell.
`backgrounded: yes` in both is because the last write came from the change to backgrounded when
the game was left, and `uptime_s` is the time of that write.

| Line | Meaning |
|---|---|
| `stage` | `hooked`: the plugin is in the game but no buffer has been registered. `registered`: buffers registered, no flip seen. `flipping`: flips are arriving, but the sampler hasn't finished a pass. `running`: the sampler has completed at least one pass. |
| `format` | the registered pixel format, and whether the plugin recognizes it. See [hdr-pixel-format](hdr-pixel-format.md). |
| `flip_calls` | calls to each of the three flip entry points. The plain `gnm` one is a wrapper around `workload` (see [videoout-hooks](videoout-hooks.md#the-actual-cause)), so a title that calls the plain one counts in both, and one that calls `workload` directly counts only there. That is why `gnm` equals `workload` in both files above. `videoout` was 0 in both. All three at 0 means the title uses a flip path that isn't hooked. |
| `loop_passes` | sampler loops completed in the foreground. 0 with flips counting means it never got through one. |
| `pass_us` | the last 30-pass window. 33,000 µs is the budget at 30 Hz. `0 windows` means no timing yet. |
| `guard` | `rejects` counts passes skipped because the buffer wasn't CPU-readable. `available no` means `sceKernelVirtualQuery` couldn't be resolved and the guard is off. |
| `remap` | what the plugin did about a GPU-only buffer. `setting` is `[compat] gpu_only_remap`. |

## What it points to

| What the file says | Where to look |
|---|---|
| no file at all | first check `report_file=1` is in the ini's `[compat]` section. If it is, the plugin never hooked that title (`plugin_load` returned early), or GoldHEN didn't load it. Check the GoldHEN log. |
| `hooked`, stays there | the title isn't calling `sceVideoOutRegisterBuffers` after the hook, or registered before the plugin loaded. See [videoout-hooks](videoout-hooks.md). |
| `registered`, `not recognized` | a [pixel format](hdr-pixel-format.md) the plugin doesn't decode. |
| `registered`, all flip counts 0 | an unhooked flip entry point, see [videoout-hooks](videoout-hooks.md#if-another-title-never-lights-up). |
| `flipping` and the game died | it went down on or before its first pass. A debug capture of the guard packets is the next step, see [gpu-only-buffers](gpu-only-buffers.md#if-another-title-has-a-dark-strip-or-crashes). |
| `running`, `rejects` climbing, `created 0`, `failed` above 0 | a GPU-only buffer the plugin couldn't make readable. [gpu-only-buffers](gpu-only-buffers.md). |
| `running`, `rejects 0`, strip dark or wrong | the buffer was readable, so look at the format and what it holds: [hdr-pixel-format](hdr-pixel-format.md), [youtube-hdr-8bit](youtube-hdr-8bit.md). |
| `avg` near or above 33,000 | the sampler is falling behind its own interval. |

These are starting points, not diagnoses. The file records what the plugin saw, and several
causes can look the same from outside.

## Limits

- It is truncated and rewritten in place, so a crash mid-write can leave it empty or cut off.
- It says nothing about whether the strip looked right. That is what the tester's own words
  in the issue are for, see `.github/ISSUE_TEMPLATE/title-report.yml`.
- Host test: `tools/test_report.c`, see [tools](../development/tools.md#test_reportc).
