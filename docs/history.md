# History

How the project got from a probe plugin to what's in the repo now. Dates are
from the git history (September 2026). This is the short version; the
[changelog](../CHANGELOG.md) has every entry and the docs linked below have the
long write-ups.

## Before the ambient light: proving the pixels

The first goal was only to check that the plugin could read the screen at all.
`tools/ps4_detile_2dthin.c` is a standalone port of the display tiler from
GPCS4, hardcoded for a 1920x1080 32bpp surface captured from a live game, and
`tools/udp_ground_truth_listener.py` plus `tools/decode_verification_dump.py`
were used to compare what the console's memory held against a real screenshot.
The "detile verify" probe plugin that came out of that lives in a separate
repo. Its tiling math is the part this plugin depends on.

## Plugin

**v1.0 (Sep 12).** First real per-frame pipeline. 229 LED zones (73 top, 42
left, 73 bottom, 41 right), index 0 at the bottom left and running clockwise,
each zone a 3x3 average, sent to WLED over DDP at about 30 Hz. The pixel format
was read from the registration hook instead of being hardcoded, and an unknown
format meant sending nothing rather than guessing. It sampled and sent
synchronously inside the flip hook, and the commit says outright that this was
an unmeasured risk to the game's render path.

**v1.2.** Sampling moved to its own worker thread, so a slow pass can't stall
frame submission. Added bounds checking on every tiled read and loop timing
telemetry. Later captures showed the worst case at about 20% of the 33 ms budget.

**v2.0 to v2.1.5.** Everything that had been a `#define` became an ini setting
(`/data/ps4_ambient_light.ini`, created with a commented template if missing):
WLED host and port, per-edge LED counts, start corner, direction, offset, scan
depth, brightness, gamma, saturation, color order, update rate and smoothing.
v2.1 added `black_level`/`white_level`, `dark_threshold` and live config reload.
Live reload took five point releases to get right, because `stat()` on the
console reported an mtime of 0 and a size stuck at 8. The final version reads
the size with `sceKernelLseek` and compares an FNV-1a content hash, which also
catches a same-length edit like RGB to RBG. Around the same time `fopen()` from
the worker thread was crashing the game, so the ini reader now uses
`sceKernelOpen`/`sceKernelRead`.

**v2.2 to v2.2.5.** Ported a reference color processor (contrast, per-channel
gamma and brightness). Then a hunt for a red flash on dark HDR scenes turned out
to be WLED's own realtime-timeout color, not a decode bug. See
[wled-heartbeat](debugging/wled-heartbeat.md). Diagnostic telemetry got gated
behind `__FINAL__==0` and the hardcoded debug IP became the opt-in `[dev]`
section.

**v2.4 to v2.6.** Merged in an opt-in hand-off signal from a fork, for a
multi-source setup most installs don't use. v2.6 fixed a bug in it where an
internal opt-in check swallowed the "off" message.

**v2.7 to v2.7.2.** Support for pixel format `0x80002200`, then live HDR/SDR
detection for it. See [hdr-pixel-format](debugging/hdr-pixel-format.md).

**v2.7.3 to v2.7.6.** The "black screen shows color" hunt: a stale-flip guard
that turned out to be the wrong fix and was reverted, the four duplicate corner
LEDs, and the smoothing bug that was the real cause. See
[black-screen-shows-color](debugging/black-screen-shows-color.md) and
[sampling-zones](sampling-zones.md).

**v2.8.** The WLED heartbeat. [wled-heartbeat](debugging/wled-heartbeat.md).

**v2.9 and v3.0.** Auto letterbox detection, then removal of the manual
`capture_margin_*` keys once it could stand alone. See
[letterbox-detection](debugging/letterbox-detection.md).

**v3.1.** Third flip hook, fixing a title that never lit up.
[videoout-hooks](debugging/videoout-hooks.md).

**v3.2.** The plugin stops loading into its own companion app.
[architecture](architecture.md).

**v3.3.** Previous-flip buffer sampling, which fixed the static-screen flicker
and the black-screen flashes. [framebuffer-flicker](debugging/framebuffer-flicker.md).
The same release tightened the letterbox scan and capped `scan_depth`,
`saturation` and `contrast`.

Along the way `main.c` grew to 3,240 lines and was split into modules (Sep 23).
It is `main.c` plus 11 files now, with the split described in
[architecture](architecture.md).

## Companion app

Started as a settings editor with a live preview and a self-updater. By the time
the repos were merged (Sep 24) it had a scrolling settings list, a layout
editor, FreeType text rendering and native `scePad` input. After that came the
Help screen and its QR code (Sep 25), then a run of self-updater fixes that day.
Those are in [companion-app-updater](development/companion-app-updater.md), and
the platform quirks behind several of them are in
[orbis-gotchas](development/orbis-gotchas.md).

Later changes tracked the plugin: percent display for the color settings, the
Auto letterbox card, the smoothing presets, and lower caps on edge depth,
saturation and contrast.

## One repo

The plugin, companion app and tools were separate repos with their own histories
until Sep 24, when they were merged. That's why the early commits mention a
"test repo" and handoff notes that aren't here. CI, the release checksum and the
GoldHEN SDK patch all came in the following day. See
[ci-and-releases](development/ci-and-releases.md).
