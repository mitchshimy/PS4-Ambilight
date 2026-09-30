# Docs

Start with [architecture](architecture.md) if you want to know how it fits together,
or [history](history.md) if you want to know how it got here.

## Understanding it

- [architecture](architecture.md): plugin, companion app, why they're separate
- [history](history.md): how the project evolved, version by version
- [sampling-zones](sampling-zones.md): LED geometry and how pixels are chosen
- [configuration-history](configuration-history.md): ini keys added, removed, changed

## Debugging write-ups

Investigations with the evidence and the wrong turns left in.

- [framebuffer-flicker](debugging/framebuffer-flicker.md): the v3.3 flicker and
  load-screen flashes
- [buffer-selection](debugging/buffer-selection.md): why a slot from one or two flips back
  is read
- [sample-lag-and-boot-flash](debugging/sample-lag-and-boot-flash.md): Red Dead
  Redemption at 58 fps, a lag setting that did nothing, and the boot flash lag 2 showed
- [videoout-hooks](debugging/videoout-hooks.md): the three flip hooks, and the title
  that never lit up
- [wled-heartbeat](debugging/wled-heartbeat.md): WLED's realtime timeout and the
  once-a-second resend
- [black-screen-shows-color](debugging/black-screen-shows-color.md): the smoothing bug,
  and a guard that got reverted
- [hdr-pixel-format](debugging/hdr-pixel-format.md): `0x80002200` and live HDR detection
- [youtube-hdr-8bit](debugging/youtube-hdr-8bit.md): 8-bit pixels under the PQ format ID
  with HDR on, and the per-frame alpha check
- [hdr2200-alpha-detection](debugging/hdr2200-alpha-detection.md): HITMAN 3 and RDR2 HDR
  or SDR from the alpha byte, 53 s down to the first frame
- [letterbox-detection](debugging/letterbox-detection.md): black bar detection, v2.9 to v3.4
- [letterbox-placement](debugging/letterbox-placement.md): zones sampling inside the bar,
  and the loading-screen flashes that the first fix caused
- [gpu-only-buffers](debugging/gpu-only-buffers.md): Mortal Kombat 11 crashing the game
  by reading a display buffer the CPU can't read, and the `mprotect` that fixed it
- [title-compatibility](debugging/title-compatibility.md): what's been seen on specific
  titles

## Development

- [orbis-gotchas](development/orbis-gotchas.md): console behavior that surprised us
- [companion-app-updater](development/companion-app-updater.md): how the self-updater
  works and broke
- [ci-and-releases](development/ci-and-releases.md): the workflow and the GoldHEN SDK patch
- [testing](development/testing.md): telemetry, the tools, and the app's tests
- [tools](development/tools.md): what's in `tools/`

The changelog says what changed. These say why.
