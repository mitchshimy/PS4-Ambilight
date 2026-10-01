# Testing

There is no test suite for the plugin. It runs inside a game on real hardware, so it
gets tested there, with telemetry. The companion app has a small set of isolation
tests that run on a PC.

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
- **`tools/test_pq8bit_vote.c`.** A host test for the alpha byte vote behind the
  `0x88740000` check, on real words from captures and on generated PQ. The one piece
  of plugin logic that is built and run on a PC, only because it lives in its own
  header. See [tools](tools.md#test_pq8bit_votec).
- **`tools/test_hdr2200_vote.c`.** The same kind of host test for the `0x80002200`
  vote, on hand-written cases and 281 real frames from HITMAN 3 and RDR2 in
  `tools/data/hdr2200_frames.csv`. See [tools](tools.md#test_hdr2200_votec).
- **`tools/test_plugin_config.c`.** Runs the plugin's real `ambient_load_config()` on a PC
  against sample inis: the generated default, both presets, the media title list, an ini
  from before presets, a live switch between presets, and the exact file the companion app
  writes (`tools/data/preset_ini_golden.ini`). Only the console calls it makes are stand-ins.
  See [tools](tools.md#test_plugin_configc).
- **`tools/decode_guard_packets.py`.** Decodes the guard packets from a listener log by
  tag (not length, several debug packets share a length), and summarizes the sampler's
  pass times. See [tools](tools.md#decode_guard_packetspy).
- **`tools/test_buffer_guard.c`.** A host test that compiles `buffer_guard.c` against a
  fake kernel built from what the Mortal Kombat 11 captures showed. It is the only way
  the no-regression case (a readable buffer never reaches the remap path) and the
  refuse-once-don't-retry case are checked without a console. See
  [tools](tools.md#test_buffer_guardc).
- **`tools/test_report.c`.** A host test that compiles `report.c` and drives its write
  logic with made-up counters: what makes it write, that it never writes per frame, that a
  rewrite leaves no stale tail, and the off switch. See [tools](tools.md#test_reportc).
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

## Companion app: isolation tests

`companion-app/tests/`:

| File | Checks |
|---|---|
| `test_settings.c` | menu item count, ini save and load round trip |
| `test_presets.c` | Game and Movie shipped values, switching and reset, save and reload, migrating an ini from before presets, other ini sections surviving a save |
| `test_display_units.c` | percent display conversion, and the load time bounds on the hidden letterbox keys |
| `test_pipeline.c` | color order remap, gamma against known values |
| `test_layout.c` | zone layout and offset behavior |
| `test_sha256.c` | the SHA-256 implementation the updater uses, including multi-block input |
| `test_sce_stubs.c` | stand-ins for `sceKernel*` file calls so `config.c` builds on a PC. Test only. |

Build one from `companion-app/`. `config.c` includes `<orbis/libkernel.h>`, which
isn't there on a PC, so you also need a stub header on the include path:

```bash
mkdir -p /tmp/stub/orbis
cat > /tmp/stub/orbis/libkernel.h <<'H'
#include <stdint.h>
#include <sys/types.h>
int32_t sceKernelOpen(const char*, int, int);
int64_t sceKernelLseek(int32_t, int64_t, int);
ssize_t sceKernelRead(int32_t, void*, size_t);
ssize_t sceKernelWrite(int32_t, const void*, size_t);
int sceKernelClose(int32_t);
H

cd companion-app
gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -o /tmp/test_presets tests/test_presets.c \
    source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c
/tmp/test_presets   # needs ../plugin and ../tools, so run it from companion-app/
gcc -w -I. -Iinclude -I/tmp/stub -o /tmp/test_pipeline tests/test_pipeline.c \
    source/settings.c source/config.c source/color_pipeline.c source/layout.c \
    source/sha256.c tests/test_sce_stubs.c
/tmp/test_pipeline
```

Current state: `test_display_units`, `test_pipeline`, `test_layout`, `test_sha256` and
`test_presets` pass. `test_settings` still fails at the `relayHost` round trip. That failure
predates the current work and is noted in the changelog. Its menu item assert
is 33 (it was a stale 31; the two Preset card rows are the difference).

CI doesn't run these, see [ci-and-releases](ci-and-releases.md). Nothing in the
workflow calls them, so run them yourself before a release if you touched `settings.c`,
`config.c`, `color_pipeline.c` or the update code.

## What isn't covered

- The plugin's own math (tiling, unpack functions, the smoothing step) has no unit
  tests. The smoothing bug in
  [black-screen-shows-color](../debugging/black-screen-shows-color.md) is the kind of
  thing a small test would have caught.
- The on-console QR code was round tripped through a decoder off console, but how it
  reads on a real screen and camera is not verified. See the companion app's own
  README.
- The updater's install path, on every network setup.
