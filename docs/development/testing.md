# Testing

There is no automated test suite, and CI only builds. The plugin runs inside a game on
real hardware, so it gets tested there, with telemetry. The companion app is checked on a
console too, with `companion-app/tools/ui_preview.sh` for layout work on a PC.

## Plugin: real hardware and telemetry

What has actually been used to check the plugin:

- **Debug builds.** `make DEBUG=1` compiles in the telemetry (`__FINAL__==0`): the
  flip and register counters, the per-pass FLK1 flicker probe, format-change
  logging, raw pixel dumps, HDR detection numbers, the `PQ8C` packets from the
  `0x88740000` 8-bit check, the `HDRV` packets from the `0x80002200` check, and the
  `GRDC`, `GRDI` and `RMAP` packets from the buffer readability guard. Release builds have none of it.
- **The `[dev]` ini section.** Set `dev_ip` to the PC's address and `dev_logging=true`
  to send the telemetry there. Without it nothing is sent, and there is no hardcoded
  debug address.
- **`tools/flicker_capture.py`.** Records the FLK1 stream and analyzes it. See
  [framebuffer-flicker](../debugging/framebuffer-flicker.md#methodology). Its
  `selftest` runs the analyzer against 10 synthetic scenarios, which is the closest
  thing to an automated test in the repo. It doesn't need a console.
- **`tools/udp_ground_truth_listener.py`.** Prints raw packets, useful for checking
  what WLED would receive, for example that the heartbeat still arrives and that
  nothing is sent while suspended. Both tools use port 4048, so don't run them
  together. If the output is redirected from PowerShell with `>` the file is UTF-16
  and a parser reads nothing from it. Convert it first (`iconv -f UTF-16 -t UTF-8`)
  or use `Out-File -Encoding utf8`.
- **`tools/handoff_capture.py`.** Isolated capture and analysis of the launch gap (nothing sent between a
  game's launch picture and its first frames). Keeps only the HOF1/SHL1 handoff events, times them on the
  PC clock, optionally polls WLED's live flag, and prints a verdict. Its `selftest` needs no console. See
  [launch-gap-test](../debugging/launch-gap-test.md). Same port 4048 as the other two tools.
- **`tools/decode_guard_packets.py`.** Decodes the guard packets from a listener log by
  tag (not length, several debug packets share a length), and summarizes the sampler's
  pass times. See [tools](tools.md#decode_guard_packetspy).
- **Offline replicas.** Several algorithms were run in Python first against real
  captures, then ported: the zone geometry for all 8 corner and direction
  combinations, the saturation formula, gamma tables, the dark threshold hysteresis,
  and the HDR detector. When the live behavior looked wrong, comparing against the
  replica told apart "the algorithm is off" from "the wiring is off".

Things that only real hardware showed, so keep testing on a console: `stat()`
returning garbage, `fopen()` crashing a worker thread, the flip hooks firing at
submit time, a title that skips the flip wrapper, and a title whose display buffers
the CPU may not read (the kernel refusing a second mapping with `EBUSY` and accepting
an `mprotect` instead is not something a fake kernel can tell you about another title).

## Companion app

`companion-app/tools/ui_preview.sh` renders Home and Set up to PNG with the app's real UI
code (no console, no SDL), for checking a layout change by eye. Run it from `companion-app/`;
it needs gcc, FreeType headers and Python with Pillow. Everything else about the app is
checked on a console: settings round trips, the updater, the install and Home states, and
the shell feed controls.

## What isn't covered

- Nothing is checked automatically. The plugin's own math (tiling, unpack functions, the
  smoothing step) and the app's settings, presets and updater logic are only exercised by
  running them. The smoothing bug in
  [black-screen-shows-color](../debugging/black-screen-shows-color.md) is the kind of
  thing a small test would have caught.
- The on-console QR code was round tripped through a decoder off console, but how it
  reads on a real screen and camera is not verified. See the companion app's own
  README.
- The updater's install path, on every network setup.
