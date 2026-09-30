# Architecture

PS4 Ambilight is two separate programs that share a config file.

```
 game process (any title)                 companion app (SHMY00091)
 +---------------------------+            +---------------------------+
 | GoldHEN loads the plugin  |            | on-console settings UI    |
 |  - hooks the flip calls   |            |  - edits the ini          |
 |  - sampler thread reads   |            |  - Test Strip (live       |
 |    the finished frame     |            |    preview to WLED)       |
 |  - color pipeline         |            |  - install / update /     |
 |  - DDP out to WLED        |            |    enable the plugin      |
 +-------------+-------------+            +-------------+-------------+
               |                                        |
               +----> /data/ps4_ambient_light.ini <-----+
               |
               +--DDP--> WLED --> LED strip
```

## Plugin

`plugin/` builds `ps4_ambient_light.prx`, a GoldHEN plugin. GoldHEN injects it
into every title. Inside the game's own process it hooks the calls a game uses
to flip a frame to the screen (see [videoout-hooks](debugging/videoout-hooks.md)),
keeps track of which buffer slot is which, and runs a sampler thread that reads
zones out of the finished frame, pushes them through the color pipeline and
sends them to WLED over DDP.

The sampler runs on its own thread, not in the flip hook. A slow sampling pass
can only make the LEDs late, it can never hold up the game's frame submission.
The hook itself only records which slot was flipped.

Rough map of `plugin/source/`:

| File | What it does |
|---|---|
| `hooks.c` | the flip and buffer-registration hooks, slot tracking |
| `sample_thread.c` | the sampler loop, buffer selection, keepalive |
| `buffer_guard.c` | asks the kernel whether a buffer is readable before it is sampled, and makes a GPU-only one readable |
| `pixel_formats.c`, `tiling.c` | unpacking the console's pixel formats and its tiled memory layout |
| `zones.c`, `letterbox.c` | where the sample zones sit, and black bar detection |
| `color_processing.c`, `gamma.c` | brightness, gamma, saturation, contrast, smoothing |
| `network.c` | DDP packets, the heartbeat, debug telemetry |
| `settings.c`, `config.c` | ini loading and live reload |
| `main.c` | plugin entry, the SHMY00091 check |

## Companion app

`companion-app/` is a normal homebrew `.pkg`, not a plugin. It edits
`/data/ps4_ambient_light.ini` with a controller-driven UI, can send live colors
straight to WLED (Test Strip) so you can check wiring before starting a game, and
installs or updates the plugin `.prx` from the GitHub releases.

`color_pipeline.c`, `ddp.c` and `config.c` in the app are copies of the plugin's
own pipeline, DDP sender and ini parser, so what Test Strip shows is what the
plugin would send.

## Why it is split like this

The sampling has to happen inside the game's process. A homebrew app runs as its
own process and has no way to look at another process's video buffers, so the
part that reads the screen is the plugin. But a plugin has no UI, and editing an
ini file with a controller is miserable, so the app exists to cover setup,
testing and updates. The ini file is the only thing they share.

## GoldHEN loads the plugin into the app too

"Every title" includes the companion app, which GoldHEN sees as just another
title id, `SHMY00091`. Until v3.2 that was harmless, because the app never went
through the flip path the plugin hooked. Once v3.1 added two more flip hooks the
app's own screen flips were being captured and streamed to the strip, and the
sampler was applying the games' tiling math to the app's differently shaped
surface, which ended in a SIGSEGV in `ambient_sample_thread`.

The fix is at the top of `plugin_load` in `main.c`: read the title id with
`sys_sdk_proc_info()` and return before installing any hook or thread if it is
`SHMY00091`. Nothing is hooked in that process, so neither the crash nor the
color conflict can happen. See the v3.2 entry in [CHANGELOG.md](../CHANGELOG.md).
