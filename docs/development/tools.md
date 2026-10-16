# Tools

Everything in `tools/` runs on a PC, not the console. Most were built while working
out what the plugin should read, and they're still useful when a new title
misbehaves.

| Tool | What it's for |
|---|---|
| `flicker_capture.py` | capture and analyze the FLK1 flicker probe from a debug plugin build |
| `udp_ground_truth_listener.py` | print every raw UDP packet on port 4048 (decodes `HOF1`/`SHL1` telemetry text) |
| `handoff_capture.py` | isolated launch-gap capture: keeps only the `HOF1`/`SHL1` handoff events, times them on the PC clock, prints a verdict; `selftest` needs no console |
| `decode_verification_dump.py` | decode the plugin's older raw debug packets offline |
| `decode_guard_packets.py` | decode the buffer guard's `GRDC`, `GRDI` and `RMAP` packets and the sampler's pass times from a listener log |
| `ps4_detile_2dthin.c` | standalone reference detiler, to cross-check the plugin's tiling |
| `update_payload_checksum.sh` | rewrite the hash and size in `payloads/CHECKSUMS.md` from `payloads/ps4-ambilight.bin`; `--check` only reports, and CI runs it |

## flicker_capture.py

The one to reach for first. Needs a debug build (`make DEBUG=1`) and, in
`/data/ps4-ambilight/ps4_ambient_light.ini` on the console:

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
slot is safe to read, and which lag the pipeline actually read. It also counts how
often the `0x88740000` 8-bit decode (flag `0x10`) switched. Its verdict
thresholds were estimates made before there was real data, so read the numbers. How
to read a report is in
[framebuffer-flicker](../debugging/framebuffer-flicker.md#methodology).

## udp_ground_truth_listener.py

A minimal UDP listener. It splits the 10 byte DDP header from the payload, and if the
payload looks like a solid color it prints the first LED's RGB. It's what you use to
confirm what WLED would actually receive: that the heartbeat arrives, that nothing
arrives during suspend. See
[wled-heartbeat](../debugging/wled-heartbeat.md#how-it-was-checked). Port 4048 is
shared with `flicker_capture.py` and `handoff_capture.py`. It prints `HOF1` (plugin) and `SHL1`
(shell feed) records as decoded text. Redirecting its output from PowerShell with `>` gives
a UTF-16 file, convert it before parsing.

## handoff_capture.py

The isolated capture behind the [launch-gap test](../debugging/launch-gap-test.md). Run
`python tools/handoff_capture.py capture --seconds 90 --out run.jsonl [--wled-ip IP]`, launch the
game, and it prints the handoff timeline and a verdict (`NO SENDER GAP`, `SENDER GAP` with the largest
step, `OVERLAP`, `INCOMPLETE`). Only `HOF1` and `SHL1` handoff events are kept; every other packet
on port 4048 is counted and dropped, so a debug build's other telemetry doesn't get in the way. All
times are arrival times on the PC. `analyze run.jsonl` re-reads a saved file; `selftest` runs the
analyzer on synthetic scenarios, no console needed. Write the file with `--out`, not PowerShell's
`>` (UTF-16). Shares port 4048 with the two tools above.

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

## decode_guard_packets.py

Reads a log from `udp_ground_truth_listener.py` (UTF-8 or the UTF-16 PowerShell makes)
and prints the v3.9 buffer guard telemetry: `GRDC` (guard counters), `GRDI` (kernel map
info for the last rejected region), `RMAP` (remap status, with `0x80020010` called out as
`EBUSY`), and a summary of the 36 byte timing packets (median and worst average pass,
worst single pass, over-budget passes).

```
python tools/decode_guard_packets.py capture.log          # first and last few of each
python tools/decode_guard_packets.py capture.log --all
python tools/decode_guard_packets.py --selftest           # synthetic packets, no capture needed
```

It goes by the 4 byte tag, not the length: `GRDI`, `PQ8C` and `HDRV` are the same length
after the DDP header, and a length-only decoder reads one as another. The `--selftest`
checks that, on synthetic packets built from the layouts in `network.c`. What
the fields mean and how to read a capture is in
[gpu-only-buffers](../debugging/gpu-only-buffers.md#telemetry-debug-builds).

## ps4_detile_2dthin.c

A standalone C port of the display tiler from GPCS4, hardcoded for the surface the
project first captured: 1920x1080, 32bpp, `kTileModeDisplay_2dThin`. Both the base
and Neo parameter sets are in it (`kParamsBase`, `kParamsNeo`), because it wasn't
known at the time whether the console was in Neo mode. Detiling a real capture with
each and seeing which looks right settles it. The plugin's `tiling.c` uses the base
parameters, and this file is the independent reference to compare against when
touching that code. See also [sampling-zones](../sampling-zones.md#tiling).

## update_payload_checksum.sh

Keeps `payloads/CHECKSUMS.md` in step with the Ambilight payload,
`payloads/ps4-ambilight.bin`. Run it from anywhere in the repo after replacing the binary:

```
sh tools/update_payload_checksum.sh           # rewrite the hash and size from the file
sh tools/update_payload_checksum.sh --check   # change nothing, exit 1 if the page is stale
```

It touches three things: the hash in the `sha256sum -c` block, and the size and hash in the
table row. The row's note about which build it is stays as you wrote it. It prints what it
changed, and exits 2 if the page no longer has the block or the row it looks for. Needs `sh`,
`sed`, `wc` and `sha256sum` (or `shasum`), so it also runs in Git Bash. CI runs `--check` on every
push and again before a release is published, see [ci-and-releases](ci-and-releases.md).
