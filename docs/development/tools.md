# Tools

Everything in `tools/` runs on a PC, not the console. Most were built while working
out what the plugin should read, and they're still useful when a new title
misbehaves.

| Tool | What it's for |
|---|---|
| `flicker_capture.py` | capture and analyze the FLK1 flicker probe from a debug plugin build |
| `udp_ground_truth_listener.py` | print every raw UDP packet on port 4048 |
| `decode_verification_dump.py` | decode the plugin's older raw debug packets offline |
| `decode_guard_packets.py` | decode the buffer guard's `GRDC`, `GRDI` and `RMAP` packets and the sampler's pass times from a listener log |
| `test_buffer_guard.c` | host test for `buffer_guard.c`, run against a fake kernel, for the GPU-only buffer fix |
| `test_notices.c` | host test for the plugin's notifications (`notices.c`): what each says, and that each is said once |
| `test_fw_support.c` | host test for the firmware support table and warning text the companion app uses (`common/fw_support.h`) |
| `test_report.c` | host test for the report file: its text, when `report.c` writes, and the write itself |
| `ps4_detile_2dthin.c` | standalone reference detiler, to cross-check the plugin's tiling |
| `test_pq8bit_vote.c` | host test for the alpha byte vote that spots 8-bit ARGB under `0x88740000` |
| `test_hdr2200_vote.c` | host test for the alpha byte vote that picks HDR or SDR under `0x80002200` |
| `test_smoothing.c` | host test that `smoothing.h` (shared by the plugin and shell-feed) is bit-identical to the smoothing code it replaced |
| `update_payload_checksum.sh` | rewrite the hash and size in `payloads/CHECKSUMS.md` from `payloads/ps4-ambilight.bin`; `--check` only reports, and CI runs it |
| `data/hdr2200_frames.csv` | real 8-word frames from HITMAN 3 and RDR2, the recorded input for that test |

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
shared with `flicker_capture.py`. Redirecting its output from PowerShell with `>` gives
a UTF-16 file, convert it before parsing.

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

## test_buffer_guard.c

Builds and runs on a PC from the repo root, no console and no SDK. The two stub headers
stand in for the GoldHEN SDK:

```
mkdir -p /tmp/stubbg
echo '#include <stdint.h>' > /tmp/stubbg/plugin_common.h
printf '#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n#define BASE_PADDED_BUFFER_BYTES ((uint64_t)1920 * 1088 * 4)\n' > /tmp/stubbg/ambient_internal.h
gcc -Wall -I/tmp/stubbg -o /tmp/test_buffer_guard tools/test_buffer_guard.c
/tmp/test_buffer_guard
```

It `#include`s `plugin/source/buffer_guard.c` itself and supplies fake
`sceKernelVirtualQuery`, `MapDirectMemory2`, `MapDirectMemory`, `Munmap` and `Mprotect`
over a fake address space. 27 checks, exits 0 if all pass:

- The fail-open case (`sceKernelVirtualQuery` unresolved).
- A normal readable buffer comes back unchanged with the full limit and **no** remap call.
  This is the no-regression case for every title that works today.
- A readable buffer shorter than the padded size, one with a hole after it, and one split
  over two regions.
- An unmapped address.
- MK11 as captured: second mapping refused with `0x80020010`, `mprotect` accepted. The
  original address is returned, protection is `0x31`, memory type still 3, and the next
  pass makes no further calls.
- Everything refused, 100 passes: each method is tried exactly once. The first build
  retried every pass (435 refusals, about 6 ms each), which is why this is here. The
  same address re-created with a different offset gets a new try.
- An `mprotect` that returns 0 but leaves the protection alone is treated as a failure.
- Each `gpu_only_remap` value (0, 1, 2, 3), and 3 without `sceKernelMprotect` resolved.
- A GPU-only region that isn't direct memory is left alone.
- If a kernel did accept the typed second view: created with the original's memory type,
  reused, and unmapped once the game's mapping changes.

The fake kernel answers what the captures showed and nothing more. It can't tell whether
a real kernel accepts `mprotect` for some other title's buffers, or how write-combined
memory behaves under CPU reads. See [gpu-only-buffers](../debugging/gpu-only-buffers.md).

## ps4_detile_2dthin.c

A standalone C port of the display tiler from GPCS4, hardcoded for the surface the
project first captured: 1920x1080, 32bpp, `kTileModeDisplay_2dThin`. Both the base
and Neo parameter sets are in it (`kParamsBase`, `kParamsNeo`), because it wasn't
known at the time whether the console was in Neo mode. Detiling a real capture with
each and seeing which looks right settles it. The plugin's `tiling.c` uses the base
parameters, and this file is the independent reference to compare against when
touching that code. See also [sampling-zones](../sampling-zones.md#tiling).

## test_smoothing.c

Builds and runs on a PC, no console and no SDK:

```
gcc -Wall -Iplugin/include -o /tmp/test_smoothing tools/test_smoothing.c
/tmp/test_smoothing
```

It includes `plugin/include/smoothing.h` and keeps a verbatim copy of the smoothing code as
it was before that header existed (inline in `sample_thread.c`, pasted into shell-feed) as
the reference. It compares the two on every update rate and settling time the ini allows,
and on every previous value, target and alpha, with and without a valid previous frame. It
then checks that a fade always reaches its target exactly (the v2.7.5 fix). The header is
the one piece of smoothing logic shared by the plugin and `shell-feed/`.

## test_pq8bit_vote.c

Builds and runs on a PC, no console and no SDK:

```
gcc -Wall -o /tmp/test_pq8bit_vote tools/test_pq8bit_vote.c -lm
/tmp/test_pq8bit_vote
```

It includes `plugin/include/pq8bit_vote.h` directly and runs `pq8bitVote()` on three
kinds of input:

- 8-word reads copied from `PQ8C` packets while YouTube played SDR video and HDR
  video, and the words from the labeled solid color captures (3 sampled zones
  repeated out to 8, since those packets only carry 3).
- Generated real PQ (black, 203 / 1000 / 4000 nit white, a mixed frame), built from
  nits with the ST 2084 curve. This is the false-positive side, and it is generated
  because there is no capture of a truthful PQ title in the repo.
- The hold band: 7, 6, 4 and 2 of 8 words with alpha byte `0xff`, and too few words.

It prints the lowest red code whose word has alpha byte `0xff` as a sanity check on
the reasoning (1008, about 8,700 nits). Why the vote works is in
[youtube-hdr-8bit](../debugging/youtube-hdr-8bit.md).

## test_hdr2200_vote.c

Builds and runs on a PC from the repo root, no console and no SDK:

```
gcc -Wall -o /tmp/test_hdr2200_vote tools/test_hdr2200_vote.c
/tmp/test_hdr2200_vote
```

It includes `plugin/include/hdr2200_vote.h` and runs `hdr2200Vote()` on:

- Hand-written cases: SDR with alpha `0xff` and `0x00`, PQ black and dark and bright
  frames, cleared buffers, too few words, the 75% and 25% edges, one saturated red
  PQ word, and fade frames, including the ones from the first build's flaw.
- `tools/data/hdr2200_frames.csv`, 281 8-word frames the plugin read on a PS4 (`HDRV`
  packets, debug build): HITMAN 3 and RDR2 with HDR on and off, plus the first build's
  captures. Each capture is replayed the way `zones.c` uses the vote, and it fails
  if any frame votes the wrong way or leaves the mode wrong. Pass another CSV path as
  the first argument to try new frames. The columns are `source,t_seconds,w0..w7`, and
  a new source name has to be added to the test.

Why the vote works is in [hdr2200-alpha-detection](../debugging/hdr2200-alpha-detection.md).

## test_plugin_config.c

Builds and runs on a PC from the repo root, no console and no SDK:

```
gcc -Wall -Wl,--wrap=stat -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_plugin_config tools/test_plugin_config.c plugin/source/settings.c plugin/source/config.c
/tmp/test_plugin_config
```

It links the plugin's real `settings.c` and `config.c`, so it runs the actual
`ambient_load_config()`. The console calls the ini code makes (`sceKernelOpen` and friends,
`stat`, `klog`) are stand-ins that read and write `/tmp/test_plugin_config.ini`, and the
small headers in `tools/host_stubs` only exist so the plugin's own headers parse without
the SDK. It checks the ini the plugin generates on first run, that `active=movie` and each
title in `media_titles.h` pick the Movie section, that games and unknown titles don't
(`CUSA05682` is Horizon Zero Dawn and is checked by name), that an ini from before v3.8
still reads as it did, that leftover flat keys are ignored once a preset exists, the
fallbacks between presets, a live reload from one preset to the other, and the bad-value
handling, what each load reports for the plugin's notifications (ignored keys, default created, load count), and (v3.9) that the optional `[compat] gpu_only_remap` key is absent from the
generated ini, defaults to 3, reads 0 to 3, ignores other numbers and doesn't disturb a preset.
The last case loads `tools/data/preset_ini_golden.ini`, the file the companion
app writes for a fresh setup. `companion-app/tests/test_presets.c` compares the app's output
to that same file, so the app's writer and the plugin's reader can't drift apart unnoticed.

## test_notices.c

Builds and runs on a PC from the repo root:

```
gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_notices tools/test_notices.c plugin/source/notices.c
/tmp/test_notices
```

It compiles the plugin's real `notices.c` and drives it with made-up config state; `Notify` and the
few file calls are stand-ins. It checks the update notice (the first run is silent, a new version is
said once, the marker follows), the missing-WLED-address warning (once, the fix is acknowledged, a
second loss warns again), the default-config and unreadable-ini messages, that a bad value is named
instead of "config reloaded", the debug logging notice (needs both `[dev]` keys, or the report file),
the hook-failure message, and that a title with an unknown format or an unreadable buffer is only
reported after the dwell, once, and a recovery restarts the clock.

## test_fw_support.c

```
gcc -Wall -Wextra -Icommon -o /tmp/test_fw_support tools/test_fw_support.c
/tmp/test_fw_support
```

Checks `common/fw_support.h`, which the companion app's firmware warning uses: the BCD decode of the
system SDK version (and that anything that isn't BCD is refused), the support table (supported,
partial, and the gaps between versions, which count as unsupported) and the warning text. When
`shell-feed/docs/support-matrix.md` changes, change the table in that header and in
`companion-app/include/fw_support.h` to match.

## test_report.c

Builds and runs on a PC from the repo root, no console and no SDK:

```
gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_report tools/test_report.c
/tmp/test_report
```

It `#include`s `plugin/source/report.c` itself, so it runs the real `ambient_report_tick()` against
made-up counters. The console's three file calls are stand-ins that redirect `/data` to a scratch
directory and honor the flags `report.c` really passes, so a missing `O_TRUNC` shows up as a
stale tail. Exits 0 if all pass:

- The text has the title, stage, format, guard and remap lines, ends with `end of report`, and
  has nothing from the ini in it. An undersized buffer is cut safely.
- Stages follow the counters in order, and a title ID can't change where the file goes.
- Only stage, format and guard or remap outcome count as a change. Flip counts, pass counts and
  timing don't, or the file would be rewritten every second.
- The first turn writes at once with `O_WRONLY | O_CREAT | O_TRUNC`, a rewrite leaves no stale
  tail, and a change is written while more of the same is not. A refresh comes round every 60 turns.
- Off by default: with no key set, a game that is running fine gets no file and no open call.
  Turning it on mid-game starts the file. Three failed writes in a row stop it for the session,
  and a success resets the count.
- `AMBIENT_VERSION_STRING` agrees with `g_pluginVersion` (major.minor) and has a changelog entry.

It can't show that a real console lets a game process write `/data` with these flags, or what the
counters read on a title nobody has tried. See [report-file](../debugging/report-file.md).

## test_state.c

Builds and runs on a PC from the repo root, no console and no SDK:

```
gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_state tools/test_state.c
/tmp/test_state
```

It `#include`s `plugin/source/state.c` itself, so it runs the real `ambient_state_tick()` against
made-up flip counters and a fake `g_isBackgrounded`, one 100 ms tick at a time. Same stand-in file calls as `test_report.c`,
redirecting `/data` to a scratch directory and honoring the real flags, so a missing `O_TRUNC`
(the wrong thing here, unlike the report file) would show up as an unexpected one. Exits 0 if all
pass:

- The very first tick writes `fg=0` immediately, before anything about foreground or flipping is
  known, so a reader is never stuck treating a missing file as a special case.
- `fg=1` only when foreground **and** a flip hook moved in the last second (ten 100 ms ticks);
  foreground alone, or flipping alone while backgrounded, is not a claim.
- The first flipping tick writes `fg=1` at once (also after a long run of flip-less loading
  ticks), and while the claim holds the file is rewritten only at the one-second heartbeat.
- A game whose flips just stop keeps the claim for the window, then gets exactly one `fg=0`.
- Exactly one `fg=0` write on the transition out of a claim, then total silence while it stays
  that way -- checked by counting open calls across several idle ticks, not just reading the file
  once.
- `seq` keeps counting up across a background/foreground cycle; it does not reset to 0.
- The record length is a true constant -- checked against `state_format()` directly, not just
  the ticks above -- across `fg` 0 and 1, a sequence number that needs to wrap at 1,000,000, and
  a title longer than the field, which must truncate rather than overrun.
- A write never carries `O_TRUNC`.
- A disk that keeps failing just keeps retrying the same tick forever; there is no failure
  counter to reset, unlike the report file, because every tick here is already cheap and
  idempotent to repeat.

It can't show that a real console lets the compositor's own process (not a game process) read
`/data`, or how a reader in another process actually behaves under real polling timing. See
[shell-feed-coordination](shell-feed-coordination.md).

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
