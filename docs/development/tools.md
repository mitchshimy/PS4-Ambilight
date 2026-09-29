# Tools

Everything in `tools/` runs on a PC, not the console. Most were built while working
out what the plugin should read, and they're still useful when a new title
misbehaves.

| Tool | What it's for |
|---|---|
| `flicker_capture.py` | capture and analyze the FLK1 flicker probe from a debug plugin build |
| `udp_ground_truth_listener.py` | print every raw UDP packet on port 4048 |
| `decode_verification_dump.py` | decode the plugin's older raw debug packets offline |
| `ps4_detile_2dthin.c` | standalone reference detiler, to cross-check the plugin's tiling |

## flicker_capture.py

The one to reach for first. Needs a debug build (`make DEBUG=1`) and, in
`/data/ps4_ambient_light.ini` on the console:

```ini
[dev]
dev_ip=<this PC's IP>
dev_logging=true
```

```
python tools/flicker_capture.py capture menu.flk --seconds 30
python tools/flicker_capture.py analyze menu.flk
python tools/flicker_capture.py analyze menu.flk --csv menu.csv
python tools/flicker_capture.py selftest
```

`analyze` reports output steadiness, sampler timing, flip rate, all-zero reads and
burst spacing, per-zone disagreement between buffers, a ring-lag table showing which
slot is safe to read, and which lag the pipeline actually read. Its verdict
thresholds were estimates made before there was real data, so read the numbers. How
to read a report is in
[framebuffer-flicker](../debugging/framebuffer-flicker.md#methodology).

## udp_ground_truth_listener.py

A minimal UDP listener. It splits the 10 byte DDP header from the payload, and if the
payload looks like a solid color it prints the first LED's RGB. It's what you use to
confirm what WLED would actually receive: that the heartbeat arrives, that nothing
arrives during suspend. See
[wled-heartbeat](../debugging/wled-heartbeat.md#how-it-was-checked). Port 4048 is
shared with `flicker_capture.py`.

## decode_verification_dump.py

Decodes the raw `payload_hex` lines the listener prints. It came out of the probe
plugin work and picked up support for each packet type as the plugin's wire formats
changed: pixel samples, registration events, the 24 byte loop timing packet, the
config reload packet, HDR/PQ decoding, the R/B swap for A8B8G8R8_SRGB, and runtime
format detection by decode smoothness. Packet types are told apart by length.

Some of those packets (older reload and timing diagnostics) are from versions that
no longer send them, so it's mostly useful for old captures and for checking the HDR
detector offline, which is how [hdr-pixel-format](../debugging/hdr-pixel-format.md)
was validated.

## ps4_detile_2dthin.c

A standalone C port of the display tiler from GPCS4, hardcoded for the surface the
project first captured: 1920x1080, 32bpp, `kTileModeDisplay_2dThin`. Both the base
and Neo parameter sets are in it (`kParamsBase`, `kParamsNeo`), because it wasn't
known at the time whether the console was in Neo mode. Detiling a real capture with
each and seeing which looks right settles it. The plugin's `tiling.c` uses the base
parameters, and this file is the independent reference to compare against when
touching that code. See also [sampling-zones](../sampling-zones.md#tiling).
