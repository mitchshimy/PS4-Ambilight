# Testing

There is no test suite for the plugin. It runs inside a game on real hardware, so it
gets tested there, with telemetry. The companion app has a small set of isolation
tests that run on a PC.

## Plugin: real hardware and telemetry

What has actually been used to check the plugin:

- **Debug builds.** `make DEBUG=1` compiles in the telemetry (`__FINAL__==0`): the
  flip and register counters, the per-pass FLK1 flicker probe, format-change
  logging, raw pixel dumps and HDR detection numbers. Release builds have none of it.
- **The `[dev]` ini section.** Set `dev_ip` to the PC's address and `dev_logging=true`
  to send the telemetry there. Without it nothing is sent, and there is no hardcoded
  debug address.
- **`tools/flicker_capture.py`.** Records the FLK1 stream and analyzes it. See
  [framebuffer-flicker](../debugging/framebuffer-flicker.md#methodology). Its
  `selftest` runs the analyzer against 9 synthetic scenarios, which is the closest
  thing to an automated test in the repo. It doesn't need a console.
- **`tools/udp_ground_truth_listener.py`.** Prints raw packets, useful for checking
  what WLED would receive, for example that the heartbeat still arrives and that
  nothing is sent while suspended. Both tools use port 4048, so don't run them
  together.
- **Offline replicas.** Several algorithms were run in Python first against real
  captures, then ported: the zone geometry for all 8 corner and direction
  combinations, the saturation formula, gamma tables, the dark threshold hysteresis,
  and the HDR detector. When the live behavior looked wrong, comparing against the
  replica told apart "the algorithm is off" from "the wiring is off".

Things that only real hardware showed, so keep testing on a console: `stat()`
returning garbage, `fopen()` crashing a worker thread, the flip hooks firing at
submit time, and a title that skips the flip wrapper.

## Companion app: isolation tests

`companion-app/tests/`:

| File | Checks |
|---|---|
| `test_settings.c` | menu item count, ini save and load round trip |
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
gcc -w -I. -Iinclude -I/tmp/stub -o /tmp/test_pipeline tests/test_pipeline.c \
    source/settings.c source/config.c source/color_pipeline.c source/layout.c \
    source/sha256.c tests/test_sce_stubs.c
/tmp/test_pipeline
```

Current state: `test_display_units`, `test_pipeline`, `test_layout` and `test_sha256`
pass. `test_settings` still fails at the `relayHost` round trip. That failure
predates the current work and is noted in the changelog. Its menu item assert
(31) was a stale number that got corrected.

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
