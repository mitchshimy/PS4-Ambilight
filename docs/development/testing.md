# Testing

The host tests described below (`tools/test_*.c`, `tools/host_stubs/`, `tools/data/`, `companion-app/tests/`,
`shell-feed/tools/`) are kept locally and are not committed: `.gitignore` excludes them, and CI only builds, it runs no
tests.

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
- **`tools/handoff_capture.py`.** Isolated capture and analysis of the launch gap (nothing sent between a
  game's launch picture and its first frames). Keeps only the HOF1/SHL1 handoff events, times them on the
  PC clock, optionally polls WLED's live flag, and prints a verdict. Its `selftest` needs no console. See
  [launch-gap-test](../debugging/launch-gap-test.md). Same port 4048 as the other two tools.
- **`tools/test_pq8bit_vote.c`.** A host test for the alpha byte vote behind the
  `0x88740000` check, on real words from captures and on generated PQ. The one piece
  of plugin logic that is built and run on a PC, only because it lives in its own
  header. See [tools](tools.md#test_pq8bit_votec).
- **`tools/test_smoothing.c`.** A host test that the shared `smoothing.h` is bit-identical
  to the smoothing code it replaced, exhaustively over alpha, previous value and target.
  See [tools](tools.md#test_smoothingc).
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
- **`tools/test_lease.c` and `tools/test_lease_net.c`.** The loopback lease. The first checks the pure logic in `plugin/include/lease.h`
  (the shell feed's slot table and parser, and the clients' state machine against fake sockets: zero traffic in steady state, one
  message per change, reconnect and re-claim, throttled retries). The second runs the REAL listener (`shell_lease.c`) against the plugin's real
  client (`plugin/source/lease.c`) over loopback on a PC, including a forked owner that exits without releasing (released by EOF),
  a second owner, garbage from an unrelated process, and the shell feed closing and coming back.
  `tools/check_lease_copy.sh` checks the companion app's copy of `lease.h`. Build (from the repo root):

  ```bash
  gcc -Wall -Wextra -Iplugin/include -o /tmp/test_lease tools/test_lease.c && /tmp/test_lease
  gcc -w -pthread -DLEASE_MSG_DONTWAIT=0x40 -DLEASE_MSG_NOSIGNAL=0x4000 -DLEASE_SO_NOSIGPIPE=9 \
      -Itools/host_stubs -Iplugin/include -Icommon -Ishell-feed/include -o /tmp/test_lease_net tools/test_lease_net.c \
      shell-feed/source/shell_lease.c plugin/source/lease.c && /tmp/test_lease_net
  ```
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
| `test_settings.c` | menu item count, ini save and load round trip, the `relay_*` keys kept only when the file already had them |
| `test_github_release_json.c` | the scan that finds one asset's API URL in GitHub's release JSON: each name gets its own asset's url, a name that is only a prefix of another is refused, the spaced JSON form, output size, an asset with no url |
| `test_ime.c` | the keyboard module `source/ime.c`, driven by fake `sceImeDialog*` calls: opens with the row's label and current value, applies and clamps a typed number, rejects garbage and empty input, handles cancel, edits the string row, reports a refused open, closes once on shutdown. Uses the test-only SDK stand-ins in `tests/stubs/orbis/` |
| `test_pad_input.c` | `source/pad_input.c`: a Cross tap that starts and ends inside one frame is seen (oldest or newest first), quick double taps, duplicate history, held-direction repeat timing, resync ignoring held buttons, action buttons limited to once per call. `gcc -Wall -Wextra -Iinclude -o /tmp/test_pad_input tests/test_pad_input.c source/pad_input.c` |
| `test_frame_prof.c` | `source/frame_prof.c`: the slow-iteration line, its stage split and rate limit. `gcc -Iinclude -o /tmp/test_frame_prof tests/test_frame_prof.c source/frame_prof.c` |
| `test_scroll_ease.c` | `include/scroll_ease.h`: settles without overshoot, same result at any frame rate, smooth retarget. `gcc -Iinclude -o /tmp/test_scroll_ease tests/test_scroll_ease.c -lm` |
| `bench_ui_scroll.c` | the cached scroll layer against the direct render, plus timings: `sh tools/ui_scroll_check.sh` from `companion-app/` |
| `test_app_update.c` | `include/app_update.h`: parsing (`1.02`, `1.2`, `v1.03\n`, junk rejected) and comparing as numbers (1.10 newer than 1.09, equal, older, unusable text says nothing) |
| `test_app_update_check.c` | `source/update_check.c` run for real with the network, disk and thread stubbed: the app question is asked with nothing installed, once per launch, retried after a failure or a bad file, and what the status line says. Eight scenarios, one process each |
| `test_app_version.c` | `APP_VERSION` keeps the `N.NN` form (at most 8 characters) that CI, `build.bat` and the SFO field expect |
| `test_action_buttons.c` | every `FIELD_ACTION` button has its own text (`Launch Ambilight payload`, `Reset Game`/`Movie`, `Press again to confirm`), none but the preset reset says "Reset", an unknown action is refused, and the menu says "Ambilight payload" rather than loader or launcher |
| `test_shell_settings.c` | `[shell] enabled` round trip, other sections (`[dev] update_base_url`, `[compat]`, unknown `[shell]` keys) surviving a save, the autoload row never reaching the ini, the two Set up rows' place and card, `[dev] update_base_url` cleanup |
| `test_install_plan.c` | every combination of plugin state and shell feed state: banner state, what the Home button does and says, and that Test strip is offered only when nothing is pending |
| `test_claim_file.c` | the companion's `/data/ps4-ambilight/ps4_ambient_state` record byte for byte against the plugin's real `state_format()`, and the heartbeat on a simulated clock (start, 1 s beats, stop, failed writes, wrap) |
| `test_goldhen_config.c` | reading GoldHEN's "Enable Plugins Loader" value from `/data/GoldHEN/config.ini` (`[PluginLoader] PluginLoader_Enabled`): the shape of two real console files, the value only counting inside its own section and uncommented, spacing, CRLF and BOM, and anything but exactly 0 or 1 reading as unknown |
| `test_payload_ini.c` | adding, reading and flipping one entry in GoldHEN's payload AutoRun list while every other line, comment and line ending comes through unchanged |
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

# the shell feed pieces (the first four need no stub header; test_claim_file needs ../plugin/include)
gcc -Wall -Wextra -Iinclude -o /tmp/test_install_plan tests/test_install_plan.c && /tmp/test_install_plan
gcc -Wall -Wextra -Iinclude -o /tmp/test_payload_ini tests/test_payload_ini.c && /tmp/test_payload_ini
gcc -Wall -Wextra -Iinclude -o /tmp/test_goldhen_config tests/test_goldhen_config.c && /tmp/test_goldhen_config
gcc -Wall -Wextra -Iinclude -I../plugin/include -o /tmp/test_claim_file tests/test_claim_file.c && /tmp/test_claim_file
gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -o /tmp/test_shell_settings tests/test_shell_settings.c \
    source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c && /tmp/test_shell_settings
gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -o /tmp/test_action_buttons tests/test_action_buttons.c \
    source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c && /tmp/test_action_buttons
gcc -w -Iinclude -o /tmp/test_app_version tests/test_app_version.c && /tmp/test_app_version
gcc -Wall -Wextra -Iinclude -o /tmp/test_app_update tests/test_app_update.c && /tmp/test_app_update
# test_app_update_check.c needs a one-header stand-in for <orbis/libkernel.h> whose scePthreadCreate runs the thread
# function before it returns (its header comment has the build line); run each scenario: for n in 1 2 3 4 5 6 7 8; do /tmp/test_app_update_check $n; done
gcc -w -Iinclude -o /tmp/test_github_release_json tests/test_github_release_json.c && /tmp/test_github_release_json
gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -Itests/stubs -o /tmp/test_ime tests/test_ime.c \
    source/ime.c source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c && /tmp/test_ime
gcc -Wall -Wextra -Iinclude -o /tmp/test_pad_input tests/test_pad_input.c source/pad_input.c && /tmp/test_pad_input
gcc -Wall -Wextra -Iinclude -o /tmp/test_frame_prof tests/test_frame_prof.c source/frame_prof.c && /tmp/test_frame_prof
gcc -Wall -Wextra -Iinclude -o /tmp/test_scroll_ease tests/test_scroll_ease.c -lm && /tmp/test_scroll_ease
```

`tools/ui_preview.sh` renders Home and Set up to PNG with the app's real UI code (no console, no SDL), for
checking a layout change by eye. Compared against the code before the shell feed, Home with the
Shell feed off is byte for byte identical. `tools/ui_scroll_check.sh` (run from `companion-app/`, needs FreeType headers)
renders Set up, Customization and Help both directly and through the cached scroll layer over a sweep of scroll
offsets and focus positions, fails if they differ by more than a few icon-edge pixels, and prints ms per frame
for each path.

Current state: every test above passes, `test_settings` included. It used to fail at the
`relayHost` round trip because the test saved to a fresh file and expected `relay_*` keys the
app deliberately doesn't write unless the file already has them; it now seeds the file first and
checks a fresh save adds none. Its menu item assert is 36 (33, plus the two Shell feed rows and the
Ambilight payload button on Set up).

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
