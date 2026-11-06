# Changelog

Newest first. Each release has a one-line summary, then the changes. The reasoning and
evidence are in [docs/](docs/README.md), and the full original entries are in
[docs/release-notes/](docs/release-notes/).

Terms: the **plugin** is the GoldHEN plugin that reads the game's picture. The **shell feed**
lights the strip from the PS4's own Home screen and menus. The **companion app** is the
on-console settings app.

## Plugin

### v4.0.1
PS4 Pro groundwork. Pro-mode games stayed dark because the plugin never hooked them.

- Hook `libSceGnmDriverForNeoMode`, the driver Pro-mode titles use. The strip now follows Red Dead Redemption, Devil May Cry 5, NFS Hot Pursuit Remastered, YouTube and Netflix on a Pro. Only with `report_file=1` for now. [Details](docs/debugging/neo-mode-gnm-driver.md)
- Tekken 7 loads both drivers and still shows no flips. The plugin now hooks the Neo driver when the title has only that one. Untested on a console.
- Add a load log (`ps4_ambient_loadlog.txt`) that shows where `plugin_load` stopped, per title, plus a module list when it stops on a missing library. `report_file=1` only. [Details](docs/debugging/report-file.md)
- Report file gains the console model and a check of whether a picture uses base or Pro tiling. [Details](docs/debugging/ps4-pro-tiling.md)
- Report file retries a failed write every 30 s. It used to give up after 3.
- App: the firmware notice says it is about the shell feed, and 11.02 is marked confirmed.
- Debug builds can save their debug packets to a file on the console (`[dev] capture_file=1`), so a title can be captured without a PC. [Details](docs/debugging/capture-file.md)

### v4.0.0
The shell feed and the game plugin now share the strip without fighting each other, and the filesystem stalls that made the strip drop out are gone.

- Game, app and shell feed hand off the strip over a loopback TCP lease instead of files.
- The strip no longer falls back to WLED's own color for a few seconds while `/data` is stuck. File reads moved off the threads that send frames, and the shell feed's heartbeat moved to `/data/ps4_shell_alive`. [Details](docs/debugging/filesystem-stalls.md)
- Fewer file operations while a game runs. `config_reload_check_seconds` defaults to 5 (was 2); existing inis keep their value.
- All our files now live in `/data/ps4-ambilight/`. File names are unchanged.
- Closed the 3 s of silence after a game's launch screen: the game claims the strip on its first send, not before. The game's first send is held while the picture is still black. [Details](docs/debugging/launch-gap-test.md)
- A game keeps announcing that it holds the strip, so an outside controller can't take it back and fight the game.
- Ignore a stale claim left behind by a crash or power loss, which kept the shell feed dark after a reboot.
- The shell feed no longer falls back to a hard-coded WLED address.
- Stop polling for the shell feed after about a minute if a title never sends.
- Lapy's PS4 Player samples one flip back, which stops white and blue flashes on its menu. [Details](docs/debugging/sample-lag-and-boot-flash.md#a-title-that-needs-lag-1-lapys-ps4-player)
- Likely fix for the CUSA15090 crash: the plugin's entry points now align the stack before calling C code.
- Replace the firmware-offset loader with a standalone payload, `payloads/ps4-ambilight.bin`, tested on 9.00, 11.02 and 12.52.
- Notification-bar messages for problems that used to fail silently (no WLED address, unreadable ini, hook failed). `[notifications] enabled=false` mutes the informational ones.
- Launch-gap diagnostics go out on the `[dev]` UDP channel, with `tools/handoff_capture.py` to analyse them.
- Repo: `shell-feed/source/main.c` split into modules; host tests are no longer committed and CI runs none.

### v3.9.4
Game and shell feed agree on who owns the strip.

- The claim follows what is actually sent to WLED, not flip-hook counters. Aimed at intermittent flicker with the shell feed; not yet confirmed on a console.

### v3.9.3
- Claim the strip the moment the first flip lands instead of up to a second later. Removes the 1-2 s flicker at every game launch with the shell feed.

### v3.9.2
- Add a small state file (`ps4_ambient_state`) that says whether a game is driving the strip, so a second process like the shell feed can back off. Always written, no ini key. [Details](docs/development/shell-feed-coordination.md)

### v3.9.1
- Add an optional report file (`[compat] report_file=1`) so a game can be checked without a debug build and a UDP capture. [Details](docs/debugging/report-file.md)
- Add the "Game report" issue form.

### v3.9
- Fix Mortal Kombat 11 crashing with the plugin loaded, and make its strip work. Its display buffer is GPU-only and can't be read directly; the plugin now remaps it. New optional `[compat] gpu_only_remap`. [Details](docs/debugging/gpu-only-buffers.md)

### v3.8
- Add Game and Movie presets (`[presets] active=`). Netflix and YouTube always use Movie. An ini without presets reads as before.
- The companion app writes the new format, so app and plugin must be updated together.

### v3.7
- Fix flicker in Red Dead Redemption (58 fps) and a boot flash in God of War Ragnarok. The plugin now reads two flips back, and re-reads a zone whose value jumps by more than 8. [Details](docs/debugging/sample-lag-and-boot-flash.md)

### v3.6
- HITMAN 3 and RDR2 (`0x80002200`) choose HDR or SDR decoding from the first frames. [Details](docs/debugging/hdr2200-alpha-detection.md)

### v3.5
- Fix wrong colors in YouTube with HDR on while playing SDR video (dark gray came out red). The plugin now checks the alpha byte to tell 8-bit from 10-bit pixels. [Details](docs/debugging/youtube-hdr-8bit.md)

### v3.4
- Letterbox zones sit at the measured bar depth, not a few pixels inside the bar. [Details](docs/debugging/letterbox-placement.md)

### v3.3
- Fix steady flicker on static screens (AC3 Remastered menu) and flashes on all-black screens. The sampler was reading a buffer the game was still drawing into; it now reads the previous flip. [Details](docs/debugging/framebuffer-flicker.md)
- Fix a delay on dark screens with a small bright element.
- Letterbox detection samples 10 points per edge and caps the search depth.
- Limits: `scan_depth` above 4 is rejected, and `saturation` and `contrast` cap at 100.

### v3.2
- Fix a crash and stray colors: the plugin was also injected into the companion app (`SHMY00091`) and streamed its UI colors. It now exits at load for that title. [Details](docs/architecture.md)

### v3.1
- Hook `sceGnmSubmitAndFlipCommandBuffersForWorkload` as a third flip entry point. Shadow of the Tomb Raider never lit up without it. [Details](docs/debugging/videoout-hooks.md)

### v3.0
- Remove the manual `capture_margin_*` keys. Automatic letterbox detection is on by default.

### v2.9
- Add automatic letterbox detection, so zones sample the picture and not the black bars. Off by default.

### v2.8
- Resend the last good color once a second when the pixel format is briefly unknown. Stops the occasional flash of an unrelated color, which was WLED's realtime-timeout fallback. [Details](docs/debugging/wled-heartbeat.md)

### v2.7.6
- Revert the stale-flip guard from v2.7.3. It made menus and loading screens flicker.

### v2.7.5
- Fix color smoothing stopping a few shades above black. This was the real cause of "black screen shows color".

### v2.7.4
- Fix four duplicate LEDs, one at each screen corner.

### v2.7.2
- Detect HDR vs SDR live for pixel format `0x80002200`.

### v2.7
- Support pixel format `0x80002200` (SDR).

### v2.6, v2.5, v2.4
- Add an opt-in external signal merged from a fork, for multi-source setups. v2.6 fixes it swallowing the "off" message.

### v2.2.5 to v2.2
- Add contrast and per-channel gamma and brightness.
- Fix a color glitch that turned out to be WLED's own realtime-timeout fallback, and speed up HDR decoding.
- Telemetry only in debug builds. The hard-coded debug IP became the opt-in `[dev]` section.

### v2.1 to v2.1.5
- Add `black_level`, `white_level` and `dark_threshold`, and live config reload.
- Fix the `fopen` crash, and the file size and content-hash reads that stopped reload from firing.

### v2.0
- Every setting is now in the ini instead of compiled in.

### v1.2
- Sample on a worker thread so a slow pass can't stall the game's frame submission.

### v1.0
- First per-frame pipeline: 229 zones sampled from the game's picture and sent to WLED over DDP.

## Companion app

Newest first. App versions: 1.02 is current.

- 1.02: tells you when a newer app is out, and the controls no longer feel sticky.
- Home shows "Shell feed running" or "starting" instead of stale install text. "Launch Ambilight payload" says when it is already running. Square quick-saves.
- Home warns when GoldHEN's plugin loader is off, since no plugin loads then.
- The firmware notice only concerns the shell feed. The payload carries offsets for several firmwares and refuses cleanly on an unknown one.
- Add the Shell feed card (off by default), its Help section and the Launch Ambilight payload button. Each release now ships the shell feed files.
- Fix LED flicker while the app was open with the shell feed running, and the app holding the strip after the PS button.
- Add an app version, shown on Help. Split `main.c` into modules. Add `[dev] update_base_url` for local update testing.
- Game and Movie presets in Customization. Needs plugin v3.8.
- Add a Smoothing preset picker, limit edge depth to 4, remove the capture margin fields.
- Self-updater: fix downloads that always failed, a stale checksum, and a startup crash. Update checks run off the main thread.
- Add the Help screen with a QR popup.
- Fix unsaved settings lost on close, a crash on exit, and a stale Test Strip card.
- Initial release: settings editor with live color preview and a plugin updater.

## Tools and CI

- A tag must match `g_pluginVersion` (major.minor) or no release is made. [Details](docs/development/ci-and-releases.md)
- Tagged builds also package the companion app. A separate `publish` job makes the release, so the two builds can't race.
- Pin `build_pkg` to `ubuntu-22.04`, since `PkgTool.Core` needs `libssl1.1`.
- Make `CUR` in `ui_icons.c` a macro so clang compiles it.
- Helper tools: `decode_verification_dump.py`, `ps4_detile_2dthin.c`, `udp_ground_truth_listener.py`, `flicker_capture.py`.
