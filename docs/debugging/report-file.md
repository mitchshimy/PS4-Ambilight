# The report file

Since v3.9.1 the release build can write one small text file per game,
`/data/ps4-ambilight/ps4_ambient_report_<TITLEID>.txt`, for example `ps4_ambient_report_CUSA11395.txt`.
It is what to attach when a game doesn't work, instead of running a debug build and
capturing UDP. **It is off by default**, see below. Everything in it was already kept in plain globals in every build, only the
packets that send them are debug-only. It holds no personal data: no WLED address, nothing from
the ini.

## Turning it on

Add this to `/data/ps4-ambilight/ps4_ambient_light.ini`, at the end of the file or anywhere a new section
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
writes in a row it pauses, then tries once more every 30 s (v4.0.1; before that it stopped for the session).

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

Files from v4.0 on have two more lines before `end of report`, which the two examples above (v3.9.1)
predate (example values):

```
console:        base PS4 (sceKernelIsNeoMode = 0)
tiling_check:   base fits (edge step: base 4.56, Neo 14.10; inner 4.51; 832 pairs, 1 tries)
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
| `console` | from `sceKernelIsNeoMode`: `PS4 Pro (... = 1)`, `base PS4 (... = 0)`, or `unknown` when libkernel has no such function. |
| `tiling_check` | which picture layout fits what the plugin read: `base fits`, `Neo fits`, `flat picture`, `unclear` or `no data`, then the mean step across a tile edge for each layout, the step inside a tile, how many pairs were read and how many tries it took. See [ps4-pro-tiling](ps4-pro-tiling.md). |

## What it points to

| What the file says | Where to look |
|---|---|
| no file at all | first check `report_file=1` is in the ini's `[compat]` section. If it is, the plugin never hooked that title (`plugin_load` returned early), or GoldHEN didn't load it. From v4.0.1 the [load log](#the-load-log) tells those apart. |
| `hooked`, stays there | the title isn't calling `sceVideoOutRegisterBuffers` after the hook, or registered before the plugin loaded. See [videoout-hooks](videoout-hooks.md). |
| `registered`, `not recognized` | a [pixel format](hdr-pixel-format.md) the plugin doesn't decode. |
| `registered`, all flip counts 0 | an unhooked flip entry point, see [videoout-hooks](videoout-hooks.md#if-another-title-never-lights-up). |
| `flipping` and the game died | it went down on or before its first pass. A debug capture of the guard packets is the next step, see [gpu-only-buffers](gpu-only-buffers.md#if-another-title-has-a-dark-strip-or-crashes). |
| `running`, `rejects` climbing, `created 0`, `failed` above 0 | a GPU-only buffer the plugin couldn't make readable. [gpu-only-buffers](gpu-only-buffers.md). |
| `running`, `rejects 0`, strip dark or wrong | the buffer was readable, so look at the format and what it holds: [hdr-pixel-format](hdr-pixel-format.md), [youtube-hdr-8bit](youtube-hdr-8bit.md). |
| `avg` near or above 33,000 | the sampler is falling behind its own interval. |
| `console` says PS4 Pro, `tiling_check` says `Neo fits` | the plugin reads this title's picture with the wrong layout. [ps4-pro-tiling](ps4-pro-tiling.md#reading-a-pro-report). |

These are starting points, not diagnoses. The file records what the plugin saw, and several
causes can look the same from outside.

## The load log

From v4.0.1, with the same `report_file=1`, the plugin also appends to **one** file for every title,
`/data/ps4-ambilight/ps4_ambient_loadlog.txt`. Each line starts with the title ID, so one file covers
any game you play. It answers the question a missing per-title report cannot: was the plugin loaded into the game, and if so where
did `plugin_load` stop?

```
[CUSA01830] ---- plugin_load entered: plugin 4.0.1 build 716c4a7, proc_info ok (pid 123, name 'eboot.bin', app version '01.00')
[CUSA01830] config loaded: report_file 1, notifications on, wled_host set, parse_failed 0, default_created 0, bad_values 0
[CUSA01830] libSceVideoOut: load rc 0, handle 17; libSceGnmDriver: load rc 0, handle 18
[CUSA01830] hooks installed (RegisterBuffers, GnmSubmitAndFlip, SubmitFlip yes, GnmWorkload yes)
[CUSA01830] plugin_load completed: hooks are in, waiting for the title's first frames
[CUSA01830] T+3s register: buffers 3, format 0x80002200 (recognized)
```

(Example lines, not a real capture.) How to read it:

| What the log shows | Meaning |
|---|---|
| no line with that title ID | GoldHEN never loaded the plugin into that game. Check `plugins.ini` and the GoldHEN log. |
| the last line is `plugin_load entered` or `config loaded` | it stopped before the video libraries loaded (a crash, or the title ended). |
| `STOP: a video library handle is 0` | the title has no `libSceVideoOut` or `libSceGnmDriver` handle at that moment, the one early return that was always silent. The load rc and handle values are on the line before. |
| `console (at STOP)` and `module:` lines after a `STOP:` | the console model, and every module the title had loaded when the plugin stopped, with `gnm` and `videoout` names marked. A `gnmdriver candidate ... FOUND` line means a Gnm driver is loaded there and carries the flip functions. Homebrew such as Apollo Save Tool lists only a few modules and no graphics libraries. |
| `trying libSceGnmDriverForNeoMode`, then `using the Neo-mode Gnm driver` | the title runs in PS4 Pro mode and has no `libSceGnmDriver`; the plugin hooks the Neo driver's flip functions instead. See [neo-mode-gnm-driver](neo-mode-gnm-driver.md). |
| `snapshot (before our loads)`, `the title had no libSceGnmDriver ... hooking the Neo driver`, or `hooked VideoOut handle ... module list now:` after a `WAITING` line | what the title had loaded before the plugin loaded anything, which Gnm driver was chosen as a result, and, for a title that registers buffers but never flips, the module list at that moment. See [neo-mode-gnm-driver](neo-mode-gnm-driver.md#tekken-7-the-normal-driver-loads-and-nothing-flips). |
| `STOP: a required video function was not found` | the symbol lines before it say which one. |
| `STOP: a hook target starts with 0xE9` | a forwarding stub, which the plugin refuses to hook. The `first byte at hook targets` line says which function. |
| `plugin_load completed`, then `WAITING:` | hooked, but the title registered no buffer, or flips never came, or the sampler never started. |
| `sampler running` | the plugin is capturing; if the strip is still dark, the per-title report and [hdr-pixel-format](hdr-pixel-format.md) are next. |

The file is appended to, and started over only when a launch finds it over 48 KB (a debug build allows 1 MB). A few events also show a
`PS4 Ambilight: diag: ...` notice on screen. With `report_file` off none of this runs.

## Limits

- It is truncated and rewritten in place, so a crash mid-write can leave it empty or cut off.
- It says nothing about whether the strip looked right. That is what the description
  in the issue is for, see `.github/ISSUE_TEMPLATE/title-report.yml`.
- Host test: `tools/test_report.c`, see [tools](../development/tools.md#test_reportc).
