# Changelog

All notable changes to PS4 Ambilight (the GoldHEN plugin and its
companion app) are documented here, newest first.

## Plugin

### v4.0.0
- **Plugin v4.0.** Everything in this section ships as v4.0: the loopback TCP lease hand-off, the shell feed's heartbeat move and the
  other file-operation changes below. `g_pluginVersion` is `0x0400` and `AMBIENT_VERSION_STRING` is `4.0.0`; `tools/test_report.c`
  checks that the two agree and that this heading exists. Entries below that say `g_pluginVersion` is unchanged were written while
  they were unreleased and are left as they were. The plugin's "plugin updated to v4.0 (was v3.9)" console notice appears once
  after installing it. Tag the release `v4.0` or `v4.0.0`.
- **The hand-off between a game, the companion app and the shell feed is now a loopback TCP lease, not files.** The shell feed
  listens on `127.0.0.1:47200` (loopback only). The game plugin and the companion app connect once and keep the connection: a claim is
  one line (`C <title>`), a release is `R`, and in between nothing is sent, read or written. The shell feed sleeps in `select()` on its
  I/O worker and wakes only for a connect, a claim, a release or its 1 s housekeeping timeout. Because the kernel closes a dead
  process's sockets, an EOF means "released": the **stale `fg=1` after a game crash is gone with no timeout and no guessing**, and so is
  the up-to-100 ms claim-file poll. The connection also replaces the heartbeat file as proof the shell feed is running: the plugin's
  startup hold and the app's "payload is running" light use the connection, so with the lease up there is no
  `ps4_shell_alive` write at all and no claim-file read or write. If the listener cannot open (or an older shell feed or plugin is
  paired with a newer one) everything falls back to the claim file and heartbeat exactly as before: the plugin writes the file only
  while no lease connection exists, and keeps it at `fg=0` while one does. Checked on a console (claim and release across
  PS-button background and resume, game exit, the shell feed disabled and re-enabled mid-game, 15 minutes of NFS Heat burnouts with no
  stalls and no slow file operation) and on a PC with real loopback sockets (`tools/test_lease.c`, `tools/test_lease_net.c`, including an
  owner that exits without releasing). See
  `docs/development/shell-feed-coordination.md#the-loopback-lease`.
- **The shell feed's heartbeat moved out of the project folder.** The long `/data` stalls (up to 40 s, 14 to 24% of the time in
  burnouts) came from a file rewritten once a second blocking the other files in its folder, among them the ini the plugin reads.
  The heartbeat is now `/data/ps4_shell_alive`, directly in `/data`. Checked on the console: with the heartbeat writing during play
  the plugin had no slow file operation in 33 minutes (`session9b`), while the heartbeat write itself still stalled up to 24 s,
  harmlessly, on the shell feed's worker. The companion app and the plugin read the new path. A shell feed older than this one
  writes the old path and reads as not running. This replaces an earlier approach in this release (no heartbeat while a game holds
  the strip), which is removed. See `docs/debugging/filesystem-stalls.md`.
- **Fewer file operations while a game runs.** `config_reload_check_seconds` now defaults to 5 (was 2); an existing ini keeps its
  own value, and the app and the plugin's default ini agree. The shell feed holds its claim-state file and heartbeat file open
  instead of opening them every poll or beat, reopening periodically so a replaced file is still picked up. The plugin logs
  `FS_SLOW op=state_write` and `op=shell_alive_checks` when its own file work takes 100 ms or more. See
  `docs/debugging/filesystem-stalls.md`. Checked on a PC only.
- **A stuck filesystem no longer stops the strip.** Captures showed WLED falling back to its own color for 4-7 s while the
  game ran. The most likely cause is that both senders read files on the thread that sends frames (the plugin's 2 s ini check,
  the shell feed's disable-file, `[shell] enabled`, heartbeat, claim-state and ini polls), so one blocked filesystem stopped both. The plugin's
  ini reads now run on a watcher thread that hands the sampler an in-memory copy; the shell feed's file work runs on an I/O
  worker (`shell_io.c`) and its heartbeat carries the send loop's tick count. If either thread cannot start, the old inline
  behaviour is used. The shell feed also skips missed ticks after a stall (`SCHED_RESYNC`) instead of replaying them as a
  burst, and slow file operations are logged as `FS_SLOW`. Write-up: `docs/debugging/filesystem-stalls.md`. Checked on a PC
  only (config, claim and worker tests); needs a real build and a console run.
- **The shell feed no longer stays dark after a reboot because of an old `fg=1`.** A game that was killed,
  crashed or lost power while in front leaves `fg=1` in `ps4_ambient_state`, and a freshly loaded shell feed
  backed off until some game was opened and backgrounded. At load, an `fg=1` already in the file is now ignored
  until its `seq` changes or `fg=0` is read; any new claim, including the companion app's next heartbeat, is
  honoured at once. Logged as `STALE_CLAIM`. Checked on a PC only (`tools/test_claim_stale.c`).
- **`shell-feed/source/main.c` is split into modules.** It was 2,244 lines; it is now the entry point only
  (about 200 lines) plus `shell_modules.c`, `shell_hooks.c`, `shell_buffers.c`, `shell_config.c`,
  `shell_frame.c`, `shell_state.c`, `shell_log.c`, `shell_report.c`, `shell_live.c`, `shell_thread.c`,
  `shell_shim.c` and the legacy `shell_probe17.c`, with what they share in `shell-feed/include/shell_internal.h`
  (its file map says what each holds). The code was moved by line range, not retyped. Three small functions are
  new, only because their bodies were inline in `module_start()`: `shell_hooks_install()`,
  `shell_hooks_control_calls()` (both in `shell_hooks.c`, since a hook function, `HOOK_INIT` and `HOOK32` have to
  share a file) and `shell_gates_log()`. `live_send()` is still one function. What was `static` and is now shared is
  hidden, so the module exports exactly the symbols it did before. The Makefile needed no change (it builds every
  `source/*.c`), but the new files are all named `shell_*.c` because the shared plugin sources go into the same
  `build/` folder and a same-named file would overwrite their `.o`. **Checked on a PC against stand-in SDK
  headers, not with the real toolchain, and not on a console:** every file compiles with the same two warnings the
  old `main.c` had; the module links with the seven shared plugin files with no duplicate symbols and the same
  unresolved externals; 77 of the 78 functions compile to the same instruction sequence, and the 78th is
  `module_start()`, whose three carved-out pieces account for the difference; flipping each compile-time gate
  compiles the same as before. Needs a real build and a console run before release.
- **Host tests are no longer committed, and CI no longer runs any.** `tools/test_*.c`, `tools/host_stubs/`,
  `tools/data/`, `companion-app/tests/` and `shell-feed/tools/` are listed in `.gitignore`, so they stay on a
  maintainer's disk but out of the repo (the last commit that has them is `b53e9b0`: `git checkout b53e9b0 -- <path>`
  restores them). CI now only builds the plugin, shell feed and companion app and uploads them; the payload checksum
  check and the tag-versus-plugin-version check stay, since they guard the release rather than test code.
  `companion-app/tools/ui_preview.sh` linked one of the test files, so that stand-in is now tracked as
  `companion-app/tools/sce_file_stubs.c` and the preview no longer needs the ignored folder. The docs and comments that
  name a test are unchanged: they describe the local copies. **Nothing in the repo now checks these by machine, so
  keep them in step by hand when working without the tests:** `default_ini.h` against `plugin/config/ps4_ambient_light.ini`
  and the app's preset values, `ambilight_paths.h` against `companion-app/include/app_paths.h`, and
  `AMBIENT_VERSION_STRING` against `g_pluginVersion`.
- **Companion app: Home reads whether the Ambilight payload is running, "Launch Ambilight payload" says so, and Square quick-saves.**
  The app now watches the shell feed's `ps4_shell_alive` heartbeat (the one the game plugin uses). The Home banner says
  "Shell feed running" while it changes, "Shell feed starting" for up to 12 s after a send, and only otherwise shows the
  old "run it from GoldHEN or turn on Autoload" text, which used to stay up right after the install had already launched
  the payload. Pressing "Launch Ambilight payload" in Set up while it is running (or was sent a moment ago) shows
  "Ambilight payload is already running." instead of sending it again; a real send now shows its result in the save bar,
  since Set up has no status line. Square on Set up and Customization saves like the Save button (same "Saved!" state),
  with a "quick save" hint in the save bar; Options still saves and quits. "Running" appears 1-2 s after the app opens
  (the first heartbeat read is only a baseline). A shell feed waiting for a WLED address writes no heartbeat, so it
  reads as not running. The app now also writes to the console's own notification bar, the one the plugin and shell
  feed use (`source/notify.c`): Test strip with no WLED address (or no LED layout) says so there as well as in the app,
  and pressing Launch while the payload is running or just sent says so there too. The no-address one is sent whatever
  `[notifications] enabled` says, like the plugin's; the launch ones respect it. Set up has a new Notifications card under
  Shell feed with a switch for that key (on by default; absent in the ini still means on, so the app writes the section only
  once it is off or already there), and the app's own messages follow it from the moment it is switched, saved or not. Not built on a console here:
  `ui_screens.c`, `shell_feed.c` and `notify.c` pass a host syntax check against a stub `libkernel.h` and the heartbeat
  logic was run against a fake file, but the rest needs the PS4 SDK, and the notification call itself is untested.
- **The shell feed no longer falls back to a hard-coded WLED address.** `WLED_HOST` in
  `shell-feed/source/main.c` was `192.168.2.110`, the maintainer's own WLED, and `live_send()` used it
  whenever the ini had no `wled_host`, so a shell feed with no address set kept transmitting UDP to that
  address on whatever network it was on (another device there could receive it). It is now empty, like the
  plugin's `wledHost`: with no `wled_host` it does not send. **It does not need a restart to recover:** an
  empty or unusable `wled_host` used to end the probe thread for good ("restart the console"), which with no
  fallback address would have made a first-run setup need a reboot, so the probe thread now waits instead
  (`wait_for_destination()`): it shows "shell feed is waiting: no WLED address is set..." (or "wled_host isn't
  an IP address..."), re-reads the ini once a second, and starts the send loop by itself as soon as
  `wled_host` holds a usable value other than the one that failed, whether set in the ini or the companion
  app. A failed host check no longer counts as a start, so the first real start still says "shell feed
  running (sending to ...)". Once running, a changed `wled_host` is followed live as before
  (`config_reload_check_seconds`, 2 s by default; 0 turns that off). While waiting no `ps4_shell_alive`
  heartbeat is written, so the plugin sees "shell feed not running" and does not hold its first send. Still
  stopped for good, as before: no display buffers and `socket()` failure. A shell feed that already has
  `wled_host` set is unchanged. Not built or run on a console in the environment that wrote this entry (the
  PS4 SDK types stop a host compile; the same 18 SDK-type errors appear before and after, none new), so the
  wait loop is untested; the Stage C host test sets its own `wled_host`. The unused `DEBUG_IP` define in
  `plugin/source/network.c` and the example addresses in comments and test fixtures are left as they are.
- **Stop checking for the shell feed after about one minute if the title never sends.** The state thread's
  heartbeat check (`ambient_shell_feed_expected()`: open the disable file, parse the ini, read the heartbeat
  file, about every 300 ms) ran from load until the first send, so a title that never sends (unsupported
  format, unreadable buffers, hooks that never fire) would have done that for the whole session.
  `ambient_state_shell_poll()` in `state.c` now also stops after `SHELL_POLL_CAP_TICKS` (600 ticks of
  100 ms, about one minute) and `g_shellFeedAlive` keeps its last answer, so a shell feed that starts or
  stops after that is not noticed by a title that has still not sent. `tools/test_state.c` covers the
  schedule (every 3rd tick, none after the first send, none after the cap, last answer kept). A title that
  never sends could not be tried on a console; the one-minute figure is a conservative first value, not a
  measured one. **No change for long black loading screens:** the hold has no timeout, and that is
  intended, because per the maintainer's testing the shell feed lights the LEDs for neither a dark screen
  nor a black loading screen, so leaving it in control of a long-black title costs nothing.
- **Hold the game feed's first send while it is still black, and only while the shell feed is running.**
  In FC26 the launch picture is on screen while the game's registered buffers read all zeros for about
  3 s. The plugin sent those black frames; the first send published `fg=1`; the shell feed, which could
  see the picture, backed off; and the strip went dark until the buffers filled in. Captures
  ([launch-gap-test](docs/debugging/launch-gap-test.md)): with HDR on, 64 black frames over 2362 ms; with
  HDR off, 96 over 3489 ms, both matching the 3-4 s the user watched. WLED never left live mode in either,
  so this was black data, not a stall. `wled_send_rgb_zones()` (`plugin/source/network.c`) now sends
  nothing until it has a frame whose brightest channel is above 2 (a one-way latch), so `fg` stays 0 and
  the shell feed keeps the strip. With the shell feed off that would leave WLED on its default color
  through a genuinely black loading screen, so the hold applies only while the shell feed is actually
  running. The shell feed rewrites `/data/ps4-ambilight/ps4_shell_alive`, a changing 20-digit counter,
  once a second from its main loop (also while the game owns the strip). The plugin's state thread, from
  load until the first send, asks `ambient_shell_feed_expected()` (`settings.c`) about every 300 ms and
  keeps `g_shellFeedAlive`: false once the counter has not changed for 3 s, or at once if the disable file
  or `[shell] enabled=false` is present. Not installed, crashed, switched off and an older shell feed
  without the heartbeat all look the same (the counter never changes), and all mean no hold. After the
  first send nothing is read, so normal play does no extra file or ini reads. **Both the plugin and the
  shell feed must be rebuilt**: with only the plugin, the heartbeat never appears and the hold never
  applies. After the change, a capture of the same title showed the first frame sent at `maxch=92`, no
  `LG_DARK_START`, no send gap, and the shell feed backing off 0.12 s after the first send; the user
  reports no dark period on the LEDs. The one flicker noted in that run fell at a moment when only the
  shell feed was driving the strip and left no event. **Known limitations:** for the first 3 s after load
  the plugin assumes the shell feed is running, so a black loading screen with no shell feed can still show
  WLED's default color if the first frame arrives sooner than 3 s after load (it arrived 3.4-4.9 s after
  load in the captures); a title whose buffers stay black for a long time while the shell feed runs
  leaves the shell feed in control with no timeout (harmless: the shell feed lights nothing on a dark screen, see the entry above); the heartbeat reader has no host test (the state test
  only gained stubs). The shell-off path has no capture of its own. Built and run by the user with
  the OpenOrbis toolchain; no PS4 SDK build in the environment that wrote this entry. `g_pluginVersion` is
  unchanged.
- **More launch-gap events: dark sends, mid-session format changes and the first frame's brightness.**
  `LG_SEND_GAP` only sees silence, so a strip going dark because the game feed was sending black frames
  looked the same as no data. New HOF1 records over the `[dev]` channel: `LG_DARK_START` / `LG_DARK_END`
  (a run of 5 or more sends whose brightest channel is 2 or lower, with frame count and duration),
  `LG_FORMAT_CHANGED` (the active format changed or became invalid after the first confirmation),
  `LG_REREGISTER` (the game registered its buffers again), and `GAME_FIRST_DDP_RESULT` now carries
  `maxch=`. Each kind is capped at 16 per process; they are not `__FINAL__`-gated. A format change that
  reverts inside one sampler pass (about 33 ms) is not seen. `tools/handoff_capture.py` keeps them in the
  capture file but its verdict does not use them yet.
- **Close the launch-time silence: `fg=1` now waits for the game's first DDP send.** Three captures
  (`tools/handoff_capture.py`, [launch-gap-test](docs/debugging/launch-gap-test.md)) showed the plugin
  publishing `fg=1` at load, the shell feed backing off within 30-90 ms, and the game's first flip hook
  (so its first send) arriving about 3.1 s later: a 3.16-3.17 s window in which nothing reached WLED, so
  WLED fell back to its default color. `plugin/source/state.c` now publishes `fg=0` at load and holds `fg=1`
  until the process's first complete DDP send (`g_wledOutputCount` first non-zero). It is a one-time latch,
  set once and never cleared; after it, ownership follows `g_isBackgrounded` exactly as before, with no
  per-send inference and no heartbeat writes. A title that never sends stays `fg=0`, so the shell feed keeps
  the strip. **Known limitation:** that is a change for unsupported titles (unknown pixel format,
  unreadable display buffer, hooks that never fire): before, they published `fg=1` at load and the
  shell feed backed off; now the shell feed stays in control of the strip. This has not been observed on
  a console, because no unsupported title was available to test. A planned, untested fix is to also
  publish `fg=1` when the plugin concludes the title is unusable (the same verdict `notices.c` reaches after
  its ~5 s dwell), rather than a plain timeout, which would bring the launch gap back for slow-starting
  titles. The handoff overlap (both senders for up to about 0.2 s; 47 ms and 94 ms measured) is also
  unchanged: the 100 ms state tick and shell poll were deliberately left as they are. Two post-change captures showed no gap (the shell backs off 47 ms and 94 ms after the game's
  first send), and the user reports the fallback color no longer appears on the 4 games tried; the NFS Heat
  burnout case was not re-run for this change, and this is not yet a broad title sweep. `tools/test_state.c`
  covers the latch (and its link was fixed for the new HOF1 diagnostics); no PS4 SDK build was done for this
  entry. `g_pluginVersion` is unchanged.
- **Handoff and launch-gap diagnostics go over the `[dev]` UDP channel.** `HANDOFF_DIAG` events from the
  plugin are sent as `HOF1` records through `ambient_handoff_diag()` (`network.c`), and the shell feed's go
  out as `SHL1` records, both only with `dev_ip` and `dev_logging=true`. New plugin milestones, each sent
  once per process from the sampler thread (the hooks stay record-only): `LG_SAMPLER_START`,
  `LG_FIRST_REGISTER_HOOK`, `LG_FORMAT_CONFIRMED`, `LG_FIRST_FLIP_HOOK`, `LG_LAG_HISTORY_READY` and
  `LG_LAG_WAIT_EXPIRED`, plus `LG_SEND_GAP ms=N` for any silence of 250 ms or more between game sends.
  They are not `__FINAL__`-gated. `tools/udp_ground_truth_listener.py` prints `HOF1`/`SHL1` text instead of hex.
- **`tools/handoff_capture.py`: isolated launch-gap capture and analysis.** Keeps only the `HOF1`/`SHL1`
  handoff events (everything else on port 4048 is only counted), times them on the PC's clock, optionally
  polls WLED's `/json/info` live flag, splits several launches in one capture, and prints a verdict and the
  largest step. `selftest` runs 14 checks with no console. Shares port 4048 with the other two capture tools.
- **Integrate both sides of the foreground gate.** `plugin/source/state.c` now writes the initial state and state transitions based on `g_isBackgrounded` (`fg=1` foreground, `fg=0` background), without tying ownership to DDP success, output counters, flip counters, or sampler activity (the one exception, a launch-only latch on the first DDP send, is the first entry above). `plugin/include/state_file.h` documents the contract and transition sequence counter. `shell-feed/source/main.c` gates sending on the explicit `fg` bit and preserves the last known state on read errors after a valid record. This version still has no unload/crash reset path, so a stale readable `fg=1` can persist after a plugin crash. The user reports a stretch of NFS Heat burnouts without flicker, but testing is ongoing and this is not yet a confirmed fix. Host-side tests are reported separately; no PS4 SDK build or new console validation is claimed for this source update.
- **Realign the exported plugin callbacks before entering C code.** The CUSA15090 crash dump put the fault
  at a `movaps` store in the bundled `vfprintf`, with the destination 8 bytes off the required 16-byte
  boundary. `plugin_load` and `plugin_unload` are now tiny assembly entry shims that align `RSP` before
  calling their ordinary C implementations, then restore the incoming frame before returning to GoldHEN.
  `tools/test_plugin_entry_alignment.c` deliberately invokes the shim with a misaligned caller and checks
  that the implementation enters with the ABI-required stack alignment. This is a targeted explanation
  for the observed fault mechanism, not yet a claim of hardware validation.
- **Replace the old firmware-offset loader with the standalone payload.** `payloads/ps4-ambilight.bin`
  is now the 58,400-byte standalone loader that starts `ps4_shell_feed.prx`; its source/build stays
  in the separate loader repository and is not copied into this repo. The binary was user-confirmed
  working with the shell feed on PS4 firmware 9.00 and 12.52. Its SHA-256/size are updated in
  `payloads/CHECKSUMS.md`; the existing release asset name and CI/release workflow are unchanged.
- **Make the companion app's firmware notice match the new payload.** The previous warnings described
  the old loader's missing kernel offsets and claimed 12.50+ would refuse to run. The new payload has
  its own per-firmware table (the `init_<n>sdk` sections in the binary, read from the section names
  rather than a disassembly). `fw_support.h` now has three states: confirmed (9.00 and 12.52, the only
  end-to-end tested versions), untested (in the payload's list, never run) and not listed (no entry; the
  payload prints "unsupported firmware" and leaves the kernel alone). Untested and not-listed get
  different wording; confirmed says nothing. Mirrored `fw_support.h` files and `tools/test_fw_support.c`
  are updated. Bump the companion app to `APP_VERSION` 1.01; the game plugin's version is unchanged.
- **Everything of ours now lives in one folder, `/data/ps4-ambilight/`.** The config
  (`ps4_ambient_light.ini`), the claim file (`ps4_ambient_state`), the version marker
  (`ps4_ambient_version`), the report files (`ps4_ambient_report_<TITLEID>.txt` and `_SHELL.txt`),
  the shell feed's disable file (`ps4_shell_feed_off`), its dump files (`shell_probe_v17_*`), the
  companion app's update staging file and `ps4_shell_feed.prx` are all in it, and nothing of ours is
  loose in `/data` any more. File names are unchanged. Only GoldHEN's own locations stay where they
  are: `/data/GoldHEN/` (plugins, `plugins.ini`, `config.ini`, `payloads.ini`) and `/data/payloads/`
  (the payload `.bin`). There is no migration or fallback to the old paths, because no release has
  used them yet. The folder name is one line, `AMBILIGHT_DATA_DIR` in `plugin/include/ambilight_paths.h`
  (the app keeps a matching `APP_DATA_DIR` in `companion-app/include/data_dir.h`, and
  `tests/test_claim_file.c` fails if they differ). The plugin creates the folder in `plugin_load`
  before it reads or writes the ini, the app creates it at startup and before the update download,
  so it exists whichever is run first. `g_pluginVersion` is unchanged.
- **Notifications for things that used to fail silently.** The plugin now puts a line in the
  console's notification bar (`plugin/source/notices.c`), each at most once per game launch (the
  plugin is injected into every title): no `wled_host` set, so the strip can't light, and again
  when one is set; the default ini was just created (and where); the ini couldn't be read; an ini
  value was ignored, naming the first one (`[network] wled_port`) and how many; the config was
  reloaded (or the reload hit one of those); the plugin couldn't hook this title; the title uses a
  screen format the plugin doesn't know; the title's screen buffer can't be read; debug logging
  (`[dev]` UDP or `report_file`) is on; and a new plugin version replaced the old one (the first
  run only records the version in `/data/ps4-ambilight/ps4_ambient_version`). The last two states are only
  reported after about five seconds, so a loading screen doesn't trigger them. Nothing is said
  about whether WLED answers: a UDP strip can't tell, and a router or power problem isn't the
  plugin's. For this, `settings.c` records what each load found (`g_configLoadCount`,
  `g_configDefaultCreated`, `g_configParseFailed`, `g_configBadCount`, `g_configBadKey`) and every
  ranged ini value goes through one helper, `cfg_int_in()`, which accepts exactly what the old
  inline checks did. `settings.c` is also compiled into the shell feed, so it only sets flags; all
  the notification calls are in plugin-only files. `tools/test_notices.c` drives them on a PC, and
  `tools/test_plugin_config.c` has new cases for the flags. `g_pluginVersion` is unchanged.
- **`[notifications] enabled` switches the informational ones off.** Optional, on by default, not
  in the shipped ini (add it by hand, like `[compat]`); the companion app keeps it when it saves
  (`tests/test_presets.c`). `false` silences the update, default-config-created, config-reloaded,
  could-not-hook, unknown-format and unreadable-buffer notices here, and the shell feed's
  started, paused, resumed and stopped ones. What the strip can't work without is sent regardless:
  no WLED address (and the note when one is set), an ignored or unreadable ini value, debug logging
  left on, and the shell feed being unable to start for want of a usable `wled_host`. Read like the
  ini's other switches: `on`, `true` or `1` is on, any other word is off. The companion app's own
  status lines and the Home firmware panel are not affected by it.

### v3.9.4
- **Keep game/shell strip ownership aligned with actual output.** The game-side state writer no
  longer releases `fg=1` just because flip-hook counters temporarily stop changing while the sampler
  continues sending from its selected buffer. `network.c` advances an output counter only when
  `sendto()` accepts a complete WLED DDP datagram; `state.c` bases its 1 Hz claim heartbeat on that
  counter while the title is foregrounded. Backgrounding releases immediately; otherwise a stalled
  output stream writes one `fg=0` after 2.2 seconds, before shell-feed's 2.5-second stale timeout.
  The record format stays `v1`/35 bytes. Host regression tests cover output without flip changes, flip
  changes without output, failed/short sends, backgrounding and stalled output. Real-console validation
  of the intermittent flicker fix remains pending. Bump the full report version to `3.9.4`;
  `g_pluginVersion` stays `0x0309` because CI checks only major.minor.

### v3.9.3
- The state file's `fg=1` is now written the moment the plugin starts flipping, not up to a
  second later. In v3.9.2 the state thread looked at the flip counters once a second, so the
  first `fg=1` could land up to a second after a game's first flip, and a reader that backs off
  while the file says `fg=1` could only back off after that. For that second both this plugin
  and the reader were sending to the same strip, and WLED alternated between the two: a 1-2 s
  flicker at every game launch, seen on hardware with the shell-level feed. The thread now looks
  every 100 ms and writes `fg=1` on the first tick it sees a flip.
- What counts as `fg=1` has **not** changed: foreground and the flip hooks moved in the last
  second. It is now a one-second window of 100 ms ticks instead of "any flip in this 1 s tick",
  so a game that flips slower than 10 times a second still holds the claim between flips.
- The write rate while a claim holds is unchanged: one record a second (`seq` +1 each time).
  The only extra write is the one on the way in. `fg=0` is still written exactly once on the
  way out, after the same ~1 s of no flips (or at once when the plugin is backgrounded), and
  the thread then goes quiet. A failed write is retried on the next 100 ms tick and no longer
  uses up a `seq` number. The record format (`v1`, 35 bytes) is unchanged, so a v3.9.2 reader
  works as before, only sooner.
- `tools/test_state.c` now drives 100 ms ticks: the first flipping tick writes `fg=1` (also
  after 25 flip-less loading ticks), writes between heartbeats stay at none, a stopped game
  keeps the claim for the window and then writes one `fg=0`, and a failed rising-edge write is
  retried without skipping a `seq`. All earlier checks kept.
- `g_pluginVersion` stays `0x0309` (CI compares only major.minor): the tag is `v3.9.3`.

### v3.9.2
- Added a small state file, `/data/ps4_ambient_state`, so a second, independent process can
  tell whether this plugin is actively driving the strip right now. It is written
  unconditionally -- no ini key, no opt-in -- because a missing or stale file is already the
  correct "nothing is claiming the strip" signal for a reader, so there is nothing to protect
  an ordinary install from. This plugin never reads the file and has no code that knows what
  (if anything) reads it. First consumer: a separate, unreleased compositor-level feed in the
  `ps4-shell-capture-research` repo that reads the PS4's own Home screen, Settings and quick
  menu and needs to back off the moment a game is actually driving the strip, and resume the
  moment it isn't. See [shell-feed-coordination](docs/development/shell-feed-coordination.md).
- The record is one fixed-width line -- `v1 fg=1 seq=000123 title=CUSA00000\n`, 35 bytes,
  always exactly that many -- written at offset 0 **without** `O_TRUNC`. That is deliberate and
  different from the report file: this one is polled by another process several times a second,
  so a reader must never be able to catch a torn write, only the old complete record or the new
  one. `fg=1` means foreground **and** the flip hooks moved in the last second; a static
  foreground frame, or a backgrounded game that's still flipping somehow, is not a claim either
  way. `seq` only advances while `fg=1`; it is not a wall-clock heartbeat.
- Written once at load (`fg=0`, so a reader is never stuck treating "the file doesn't exist
  yet" as its own special case during the first second a title runs), then once a second while
  `fg=1`, then **exactly once** on the transition back to `fg=0` -- after that one write the
  thread goes quiet and touches the file again only once play resumes. A crash leaves the file
  on a frozen `fg=1`; recovering from that is entirely the reader's own staleness timeout, not
  anything this plugin does.
- New `state.c` and `state_file.h`, split the same way as `report.c` / `report_file.h`: the
  header is pure string logic, host-testable with no console calls, and the `.c` file does the
  two things that need the console (reading the existing flip/backgrounded globals, and the
  file I/O). Its own thread (`ambient_state_thread`), for the same reason `report.c` has one: a
  disk write can take milliseconds, and the sampler's whole budget is 33 ms a pass at 30 Hz.
  Nothing in the sampler is touched by this change at all.
- Host-tested: `tools/test_state.c` includes `state.c` and runs the real tick against made-up
  flip counters and a fake `g_isBackgrounded`, including that the record length is a true
  constant (checked, not hand-counted) across a wrapping sequence number and an oversized title,
  that a write never carries `O_TRUNC`, that exactly one `fg=0` write happens on the
  foreground-to-background transition and none after, and that `seq` keeps counting across a
  background/foreground cycle instead of resetting.
- `g_pluginVersion` stays `0x0309`, same as the v3.9.1 patch release: CI compares only
  major.minor, so the tag is `v3.9.2`. The full version string is `AMBIENT_VERSION_STRING` in
  `ambient_internal.h`.

### v3.9.1
- Added a report file, so testing a game no longer needs a debug build and a UDP capture. **It
  is off by default**: a tester turns it on by adding `report_file=1` under `[compat]` in
  `/data/ps4_ambient_light.ini`. With it on, the release build writes `/data/ps4_ambient_report_<TITLEID>.txt`, a short text file with the
  plugin version and build, the title ID, how far it got (`hooked`, `registered`, `flipping`
  or `running`), the registered format and whether it was recognized, the flip counts for all
  three entry points, the guard result (rejects, last return, last bad address), the remap
  result (created, failed, method, last return) and the last pass time. A tester sends that file.
  It holds no personal data: no WLED address and nothing from the ini. Everything in it was
  already kept in plain globals in every build; only the packets that send them are debug-only.
  [report-file](docs/debugging/report-file.md) explains each line.
- It is written when something changes (stage, format, guard or remap outcome) and every 60 s
  otherwise, not only at the end. That is deliberate: a game that dies on its first sampling
  pass, as Mortal Kombat 11 did before v3.9, never reaches a last write, but the file on disk
  still says `flipping` and what the guard saw. With the key set, no file at all means the
  plugin never hooked that title (`plugin_load` returned early) or GoldHEN didn't load it.
- It runs on its own thread, `ambient_report_thread` (new `report.c`), so a disk write never
  lands in the sampler's 33 ms pass. The sampler itself gained one counter increment per loop
  and three stores once per 30-pass timing window. The thread only reads.
- The optional ini key `[compat] report_file` is 0 (off) when absent, so an ordinary install
  never writes the file, and 1 turns it on. It isn't in the shipped ini. It is read on the
  live ini reload too, so it can be switched on mid-game. The companion app keeps a
  hand-added `[compat]` section when it saves (tested in `tests/test_presets.c`). After 3
  failed writes in a row (no `/data`, disk full) it stops for the session.
- Host-tested: `tools/test_report.c` includes `report.c` and runs the real write path against
  made-up counters, including that a rewrite leaves no stale tail, that per-frame counters
  don't cause writes, and that the version string matches `g_pluginVersion`.
  `tools/test_plugin_config.c` now also checks the new key.
- Added `.github/ISSUE_TEMPLATE/title-report.yml`, so a tester can say works, dark strip or
  crashed and attach the file without editing any markdown. Results worth keeping get copied
  into [title-compatibility](docs/debugging/title-compatibility.md) by hand.
- `g_pluginVersion` stays `0x0309`: CI compares only major.minor, so the tag is `v3.9.1`.
  The full version string is `AMBIENT_VERSION_STRING` in `ambient_internal.h`.

### v3.9
- Fixed Mortal Kombat 11 (CUSA11395) crashing the game with the plugin loaded. The game
  was fine with the plugin disabled and went down with it enabled: `SIGSEGV` on
  `ambient_sample_thread`, page fault on a user read (page not present) at
  `0x49ff38b6d0`, plugin offset `+0x719c`, on the first real sampling pass. The sampler
  had never asked whether a display buffer was readable. `sampleZoneAverage()` bounded
  the offset and trusted the address, and every title tested until then happened to map
  its buffers CPU-readable. MK11's are GPU-only (protection `0x30`, memory type 3,
  write-combined, one `0x7EC000` byte direct-memory region per slot). The registered
  format (`0x88740000`) was recognized and isn't the cause: a working title registers the
  same one. The new `buffer_guard.c` asks `sceKernelVirtualQuery` how many bytes from the
  buffer base the CPU may read, before every pass, and `zones.c`, `letterbox.c` and the
  debug probe check each pixel read against that. A buffer that can't be read at all takes
  the existing keepalive path, so the strip holds its last color and the game isn't
  touched. If `sceKernelVirtualQuery` can't be resolved the guard fails open (v3.8
  behavior).
- Made MK11's strip work. For a GPU-only direct-memory buffer the plugin adds `CPU_READ`
  to the game's own mapping with `sceKernelMprotect` (`0x30` to `0x31`, memory type
  unchanged) and checks with a fresh query that the bit stuck. **Confirmed on real
  hardware** on MK11: `created 3, failed 0` for three slots, the same for the whole of a
  53 s gameplay capture, all slots readable on all 1,422 probe packets, zone output up to
  210 of 255, sampling about 2.6 to 2.9 ms per pass (uncached reads; 30 Hz gives 33 ms),
  no crash in any capture, and the colors on the strip are right by eye. The optional ini
  key `[compat] gpu_only_remap` turns it off (0) or limits it, default 3, and nothing needs
  to set it. It only runs on a buffer the guard already found unreadable, so a title that
  works today never reaches it. Host-tested with a fake kernel, see `tools/test_buffer_guard.c`. `tools/test_plugin_config.c`
  now also checks that key is optional, in range 0 to 3, and doesn't disturb a preset.
- Paths that were tried and didn't work, kept because the next person will think of them:
  - **A second, CPU-readable view of the same direct memory** (`sceKernelMapDirectMemory2`,
    with the original's memory type so a cached view can't freeze on stale pixels). The
    kernel refused it every time, `0x80020010` (`EBUSY`): the range is already mapped.
    The untyped `sceKernelMapDirectMemory` was refused too, inferred from the capture (the
    `mprotect` only runs after both failed) and not recorded. Both are still in the code
    behind `gpu_only_remap`, as the tested build had them; they are the first thing to
    delete if nothing ever needs them.
  - **Retrying a refused region on every pass.** The first build of that path did, 435
    times in about 23 s, at about 6 ms a refused call, so the pass time went from tens of
    microseconds to 6.2 ms. A refused region is now remembered and not retried until its
    start, size or direct-memory offset changes (3 refusals, 40 to 70 µs passes in the
    same run after the fix).
  - **A guard that required the whole padded buffer size (`0x7F8000`) to be readable.**
    MK11's region is `0x7EC000`, so a title with a CPU-readable buffer that short would
    have been rejected outright, a dark strip on a game that used to work. MK11 didn't
    show it (rejected for its protection first), so it was found while decoding the
    capture, not by a test. It now measures the readable length and reads up to it; a zone
    that sits entirely in the skipped tail reads as black. No captured title has hit this.
  - **The `sceKernelVirtualQuery` info struct from memory, `flags` as four bytes.** It is
    one byte; the capture's next bytes were the start of the region name.
- New debug-build telemetry: `GRDC` (guard counters), `GRDI` (kernel map info for the
  last rejected region) and `RMAP` (remap status), each with a 4 byte ASCII tag.
  `tools/decode_guard_packets.py` decodes them by tag, because `GRDI`, `PQ8C` and `HDRV`
  are the same length after the DDP header and a length-only decoder confuses them (it
  did, on the first capture). Two other things that misled while reading these captures:
  the listener's "first LED = RGB" is the tag's ASCII for a `FLK1` packet (70, 76, 75), and
  a file redirected from PowerShell is UTF-16.
- Not done / not known: only MK11 has been captured with a GPU-only buffer, so whether
  `mprotect` is accepted for another title's buffers is untested. A GPU-only region that
  isn't direct memory can't be handled by any of the three methods and would give a dark
  strip, not a crash (nobody has seen one). Two single passes in the gameplay capture took
  94 ms and 123 ms, in two of 47 windows; the others stayed under 4 ms average, the cause
  is unknown. A buffer freed mid-pass can still fault, and a game that frees and
  re-creates its buffers was tested only on the PC, not on a console. No release build
  was captured. The captures quoted here are from the overlay builds over v3.7 that the
  guard was first run as, with untagged packets. It was then merged onto v3.8
  (`hooks.c`, `settings.c`, `main.c` and `ambient_internal.h` were touched by both changes,
  every conflict kept both sides, nothing of the presets was dropped) and the packets got
  their tags. The merged build was built and tested on a console and works. The presets' own
  host test, `tools/test_plugin_config.c`, passes on the merged tree with the new key
  covered, and so do the other host tests.
- Details, the capture tables and the decoding wrong turns:
  [gpu-only-buffers](docs/debugging/gpu-only-buffers.md).

### v3.8
- Added Game and Movie presets to the ini. `[presets] active=game|movie` picks one, and
  `[preset_game]` and `[preset_movie]` each hold a complete set of the values a preset owns:
  `scan_depth`, the letterbox settings, `brightness`, `saturation`, `gamma`, `black_level`,
  `white_level`, `contrast`, `dark_threshold`, the per-channel brightness and gamma, and
  `smoothing_enabled` / `settling_time_ms`. Setup values (`[network]`, the LED layout,
  `color_order`, `update_frequency_hz`, `config_reload_check_seconds`) stay in their own
  sections and are never part of a preset. A preset section that exists is used as it is, so
  the values are stored once and `[layout]`, `[color]` and `[timing]` shrink to setup only. A
  preset that lacks a key keeps the plugin's built-in default for it, and a preset with no
  section at all falls back to the other one.
- An ini from before v3.8 reads exactly as it did: with no preset section the same keys are
  read from `[layout]`, `[color]` and `[timing]`. Once a preset section exists those old
  keys are ignored, so a hand-edited leftover can't override a preset.
- The plugin now runs Movie by itself for Netflix and YouTube, whatever `active` says. It
  matches the running title ID against `plugin/include/media_titles.h` (Netflix `CUSA00129`,
  `CUSA00127`, `CUSA02988`; YouTube `CUSA01015`, `CUSA01116`, `CUSA01065`, `CUSA01034`), so
  nobody has to open the companion app to switch. Everything else runs the preset `active` names. Only IDs
  confirmed against independent sources are listed, because a listed ID forces Movie
  (sampling depth 4, 200 ms smoothing) on whatever runs under it. One ID that circulates as
  a YouTube one, `CUSA05682`, is Horizon Zero Dawn, and a test keeps it off the list.
- The shipped `ps4_ambient_light.ini` and the file the plugin writes when none exists are now
  the short version with both presets, and the two are one string (`default_ini.h`) so they
  can't drift. Game is `scan_depth=2`, saturation 35, gamma 2.2, smoothing on at 50 ms.
  Movie is `scan_depth=4`, brightness 204, saturation 25, gamma 2.4, black level 1,
  smoothing on at 200 ms. The gammas assume WLED isn't applying its own gamma to the DDP
  stream.
- **The companion app and the plugin have to be updated together.** The app now writes the
  presets and no longer writes the old flat copies of those values, so an older plugin
  would find none of them and run its built-in defaults.
- Checked on a PC against the plugin's real `ambient_load_config()`
  (`tools/test_plugin_config.c`): the generated ini, both presets, the media titles,
  pre-v3.8 inis, a live switch between presets, out-of-range values, and the exact file the
  app writes (`tools/data/preset_ini_golden.ini`). Not yet run on a console.

### v3.7
- Fixed flicker and flashes of unrelated colors in Red Dead Redemption (about 58 fps, the
  same with the console's HDR setting on or off). `AMBIENT_SAMPLE_LAG` had only ever been tested as `>= 1`, so setting it
  to 2 compiled to exactly the behavior of 1: nothing tracked a second step back. At 30 fps
  one flip of lag was enough for the GPU to finish the slot, at 58 fps it wasn't. Three
  captures taken at "lag 2" before this were byte-identical to the lag 1 ones, which is how
  the no-op showed. `hooks.c` now tracks `g_prevPrevDisplayBufferIndex` (two real flips
  back, same only-on-a-real-change rule as the first) and the default is 2. **Confirmed on
  real hardware**: the largest output jump in the same RDR1 scene went from 18,670 to
  1,319, the analyzer's verdict went from "output is not steady" to "output is steady",
  the pipeline's read matched the lag 2 slot on 100% of passes, and the flashes were gone
  by eye. Costs one more flip of latency than v3.3: about 33 ms at 60 fps, 66 ms at 30.
- Fixed a flash at game boot that showed up in God of War Ragnarok once the default went
  to 2. Until the lag history exists (three different buffer indices at lag 2) the sampler
  used to fall back to the slot the hook had just reported, which is the one the GPU is
  about to draw into, and that first read also seeds the smoothing with nothing to fall
  back to. In the capture that caught it, the first pass after boot (`seq` 0, flip counter
  4) read a bright value in one zone while all three buffers re-read black in the same
  pass, and the output then decayed over about 8 passes; a second capture showed the same
  decay from its first pass. The sampler now reads no frame until the history exists and
  the strip stays as it was. A title that never changes slot gets the old fallback after
  30 passes (about 1 s at 30 Hz), so it isn't left dark for the whole game. The flash was
  intermittent (it showed in one launch out of seven), so this rests on the mechanism in
  the captures and on the result: 7 launches in a row without a flash, and a 5.7 s capture
  from the first pass with no output change on any of its 155 passes. The strip lights a
  few passes later at game start (estimated at about 100 ms at GOWR's flip rate, not
  measured).
- A zone whose new read jumps by more than 8 (per channel, 0 to 255) is now read again
  from the same buffer after the rest of the pass, and if the two reads disagree the new
  value is dropped and the last accepted one kept, for at most 3 passes in a row per
  zone. That catches a buffer that changes while it is being read, whatever lag it was
  picked with, and costs nothing on a static screen (no zone is flagged) or on a real
  change (a stable buffer agrees with itself, so it is accepted on the first pass).
  Color processing and the dark threshold now run once, after this, on the value that
  was accepted, so a dropped read never moves their hysteresis. A stale slot that is not
  being written during the pass reads the same twice and is not caught. A persistence
  gate (trust a change only after it holds for 2 passes) was tried first and removed: a
  stale slot that stays stale holds, so it let the GOWR flash through, and it delayed
  every real change by a pass.
- Write-up, including the wrong turns:
  [`docs/debugging/sample-lag-and-boot-flash.md`](docs/debugging/sample-lag-and-boot-flash.md).
  `buffer-selection.md` is updated for the second tracked slot.
- Debug builds only: no new packets. FLK1's `curIdx` is still the slot the hook reported,
  so the analyzer's ring-lag columns line up. Nothing in the ini changed.
- Not done: no release build was captured. RDR1 is the only 60 fps title captured. The
  first frame after a reset (boot, a letterbox commit, a settings reload) is accepted
  without the re-read, since there is no earlier value to keep. The cost of the second
  read on a frame where the whole scene changes was not measured. Single-buffer swap
  chains only ever hit the 30 pass fallback in the logic, no real title was seen doing it.

### v3.6
- HITMAN 3 and RDR2, both `0x80002200`, now switch to the HDR decode on the first
  frame with picture data instead of after 53 s. That ID is A8B8G8R8 with the
  console's HDR off and A2R10G10B10 PQ with it on, and the game never says which.
  The smoothness detector had to decide, and on a real HDR-on HITMAN 3 capture it
  took 53.45 s: about 46 s of dark frames under its noise floor, then 4 checks
  2.2 s apart. The words in the buffer were PQ from the first packet, so the strip was
  lit from SDR-decoded PQ the whole time (the sampled words read 80 to 125 out of
  255 on average where the screen was 8 to 36). `detectHdr2200Fast()` now reads
  the alpha byte of 8 zone words on every pass, before the unpack function is
  picked, the same idea as the v3.5 check with the sides swapped: HDR-off words
  have `0xff` there (97% in the capture), HDR-on words never do (top bytes `0xc0`
  to `0xdd` in all 1554), so a black frame decides too, `0xc0000000` against
  `0xff000000`. The smoothness detector still runs on the frames where this one
  holds. **Confirmed on real hardware** in both games: the first frame with data
  decided correctly in every capture (HITMAN 3 at 4.84 s HDR on and 4.38 s off,
  RDR2 at 1.95 s and 0.34 s), no mode flips, and colors right in both modes.
- A fade guard, because the first build had a flaw the hardware run showed: HITMAN 3
  fades to and from black in SDR by writing a frame-wide alpha byte into the words
  (`0x11` up to `0xfe` over about a second), and the 8 zones of one frame differed by
  a few counts (`cc cb cb cb ca ca ca c8`). Those alphas look like PQ, so for about
  0.22 s of a fade-in the strip was decoded as PQ and then a bright frame (`f90d0c0c`)
  held it there. Top bytes within 4 of each other in `0xc1` to `0xfe` now hold the
  mode instead of voting. The cost is that a dark, flat PQ frame seen while the mode
  is still SDR holds until a frame with more variation arrives. Boot goes through
  `0xc0` black, which is exempt, so it never happened in a capture.
- Neither game applies an HDR change until it is restarted (toggled mid-game in
  captures of 37 to 71 s, each way in RDR2 and on in HITMAN 3: the buffers never
  changed), so a live switch isn't a case here. A title that does switch live would be.
- The vote is in `plugin/include/hdr2200_vote.h` and built by
  `tools/test_hdr2200_vote.c` (21 hand-written cases and 281 real frames from
  `tools/data/hdr2200_frames.csv`, including the fade-in frames that fooled the first
  build). Setting `HDR2200_FASTPATH` to 0 in that header brings back the v3.5 behavior.
  Write-up: [`docs/debugging/hdr2200-alpha-detection.md`](docs/debugging/hdr2200-alpha-detection.md).
- Debug builds only: a 58 byte `HDRV` packet goes out on every mode change and about
  every 30 checks. No new FLK1 flag, `0x01` still means the HDR decode is active.
- Not done: no release build was captured, and no title but these two was tried.

### v3.5
- Fixed wrong colors in YouTube when the console's HDR setting is on and it
  plays SDR video: dark gray came out red, blue and green both magenta. HDR
  video was fine. With HDR on the app registers `0x88740000`
  (A2R10G10B10_BT2020_PQ), but for SDR video and its own UI the buffer holds
  plain 8-bit A8R8G8B8 (`0xff212121` for its dark background, the same word it
  writes with HDR off), and it goes back to real PQ for HDR video without
  re-registering. The PQ unpack read `0xffRRGGBB` as red at 8,700 nits or more
  and clipped it. `detectPq8bitMisregistration()` now reads the alpha byte of
  8 zone words on every pass, before the unpack function is picked: 8-bit
  content has `0xff` there on every pixel, and a real 10-bit word can't
  without absurd red. So the frame being decoded gets the right decode, with
  no streak and no delay, and a switch shows no wrong frame. **Confirmed on
  real hardware**: nine switches between SDR and HDR video in a 3 minute
  capture, each new decode landing on the same pass as the decision (0 to
  13 ms), nothing seen wrong on the strip at any of them or at app launch.
  Full investigation, including a first version that voted over 3 checks and
  took about 0.75 s to switch:
  [`docs/debugging/youtube-hdr-8bit.md`](docs/debugging/youtube-hdr-8bit.md).
  Only YouTube tested, and the real-PQ side of the check is mostly generated
  data, see that doc's "Not verified". HITMAN 3's `0x80002200` might have the
  same tell, not checked.
- Debug builds only: FLK1 flags bit `0x10` is set while the 8-bit decode is
  active, and a 58 byte `PQ8C` packet goes out on every mode change and about
  every 30 checks.

### v3.4
- Letterbox zones now sit at the measured bar depth instead of the
  rounded-down value used for the stability gate, which had been leaving
  them a few pixels inside the bar on cutscenes whose depth isn't a
  multiple of the quantize band (only trailers happened to land on one).
  That alone caused loading screens to flash, since a UI layout can
  locally look like a bar on several edges without them agreeing with
  each other the way a real letterbox/pillarbox bar does -- a margin is
  now only applied when both edges of an axis agree. **Confirmed on real
  hardware**, both parts. Full investigation:
  [`docs/debugging/letterbox-placement.md`](docs/debugging/letterbox-placement.md).
  Still open: detection instability on the same cutscene, and only one
  title tested -- see that doc's "Not verified".

### v3.3
- Fixed steady LED flicker on static screens (AC3 Remastered menu) and
  flashes on all-black load screens (God of War Ragnarok). The sampler was
  reading the buffer slot the flip hook had just reported, but the flip
  hooks fire when the game submits a flip, so that slot can still be
  cleared or half drawn. It now reads the slot of the previous flip
  (`AMBIENT_SAMPLE_LAG`, default 1, costs one flip of latency).
  Confirmed on real hardware, and the GOWR flashes were caught frame by
  frame in the slot the fix skips (60 of 60 transients across two
  captures). The full investigation is in
  [`docs/debugging/framebuffer-flicker.md`](docs/debugging/framebuffer-flicker.md),
  with the details of the change and what's still untested in
  [`buffer-selection.md`](docs/debugging/buffer-selection.md).
  Still open: the letterbox cutscene flicker in the same title, no
  capture yet.
- Fixed a delay on dark screens with a small bright element in the
  middle (a loading screen with centered text, for example). The
  letterbox scan only checked 3 fixed points per line (25/50/75%), so
  on a mostly-black frame all four edges scanned nearly to the search
  limit before "finding" the text, and the plugin committed a huge
  border around it. `buildZoneGeometry()` then pulled the edge zones in
  to that margin, so when something flashed in from the real screen
  edge nothing sampled there and the LEDs only reacted once it had
  travelled in to the shrunken area, about a second late. Only showed
  up on dark scenes because bright frames hit non-black on the first
  row and never get that far.
- Letterbox detection now sweeps `EDGE_NUM_SAMPLES` (10) points across
  the whole row/column and only treats the line as bar if 85% of them
  are black (`isRowBlack`/`isColBlack`). A few dozen pixels of text
  only flip a couple of samples, so the scan walks past it to the real
  edge. This is the same approach as `BorderProcessor.findBorderRgb` in
  the Android project, which every real capture path there uses; the
  3-point version this was ported from (`findBorderRgba`) turns out to
  only be exercised by that project's unit tests.
- Search depth is capped at `MAX_BAR_DEPTH_V`/`_H` (1/6 of the screen,
  180px/320px) instead of half the screen. Console output is native
  16:9, so the only bars are cutscenes mastered at a narrower ratio:
  1.85:1 is ~21px per edge and 2.39:1 is ~138px on a 1080p frame. This
  also caps how far a misdetection could ever inset the zones. Worst
  case probe count is about the same as before (~10k vs ~9k) despite
  the extra samples per line.
- `scan_depth` had no upper bound in the plugin's ini loader (every
  neighbouring key checked both ends). Added `SCAN_DEPTH_MAX` (4). Each
  step is `(2*depth+1)^2` tiled reads per zone per pass, so the old
  ceiling of 10 was roughly 54M tiled-offset computations/sec at 512
  zones and 240Hz; 4 brings that to about 10M. A value above 4 in the
  ini is now rejected and the default (1) is kept, same as the other
  bounded keys.
- `saturation` and `contrast` now top out at 100 (were 300). The color
  chain is clamped to 0-255 per channel at the end, so most of that
  range just clipped: over sample colors, +100 already has a channel
  clipped on ~84% of vivid colors and +300 is ~99.8%; for contrast the
  2x factor at +100 clips half the tonal range. Testing 100 against 175
  on a real strip showed little difference, which lines up. Unlike
  `scan_depth`, these two are clamped to the new cap on load rather
  than rejected, so an existing `saturation=175` becomes 100 instead of
  silently resetting to 0. New `SATURATION_MAX` / `CONTRAST_MAX` in
  `ambient_internal.h`.
- `g_pluginVersion` bumped to `0x00000303` for this release, so it
  doesn't drift behind the changelog again the way it did before v3.1.

### v3.2
- Fixed a real crash: GoldHEN injects every plugin into every title's
  process, including this project's own companion app -- just another
  titleid (`SHMY00091`) as far as GoldHEN is concerned. Before this
  fix that was harmless, since the companion app apparently never
  called the plain `sceGnmSubmitAndFlipCommandBuffers` path this
  plugin hooked pre-v3.1. Once v3.1 added the
  `sceVideoOutSubmitFlip`/`...ForWorkload` hooks, the companion app's
  own screen flips started getting captured too -- its live UI colors
  got streamed to the strip, conflicting with whatever real game
  testing/strip-adjustment session was already running, and
  `ambient_sample_thread` then applied this plugin's game-buffer
  tiling math to the companion app's differently-shaped surface.
  Confirmed via a real crash report: thread `ambient_sample_thread`,
  proc `eboot.bin`, AppName `PS4 Ambilight`, TitleID `SHMY00091`,
  SIGSEGV on a page fault reading an address that matched neither this
  project's real GNM buffer addresses seen in other captures. Fixed at
  the very top of `plugin_load`, before any `dlsym`/hook/thread work:
  read `procInfo.titleid` via `sys_sdk_proc_info()` and bail out
  entirely if it's `SHMY00091`. One check fixes both the crash and the
  color conflict, since neither can happen if no hook is ever
  installed in that process to begin with. **Confirmed on real
  hardware: the companion app no longer streams its own screen colors
  to the strip, and no longer crashes on options save/exit.**

### v3.1
- Added a third flip hook, `sceGnmSubmitAndFlipCommandBuffersForWorkload`,
  fixing titles that never light up at all. Shadow of the Tomb Raider
  (SDR and HDR) never lit the strip because it calls the `ForWorkload`
  entry point directly and skips the wrapper the plugin was already
  hooking. Also added `sceVideoOutSubmitFlip` as a second flip hook (it
  turned out not to be the cause, but it's harmless), plus flip/register
  counters in the debug telemetry. Confirmed on real hardware. How it
  was tracked down, including the wrong guesses:
  [`docs/debugging/videoout-hooks.md`](docs/debugging/videoout-hooks.md).
- Also bumped `g_pluginVersion`, which had been stuck at the v2.7.2
  comment (`0x00000212`) since that release, with v2.7.3 through v3.0
  landing on top of it unbumped -- caught up now that there's a real
  reason to touch this file again.

### v3.0
- Removed the manual `capture_margin_top/right/bottom/left` ini keys.
  v2.9's auto letterbox detection can now fully cover what these were
  for -- a fixed manual number is either wrong during a letterboxed
  cutscene or wrong for the fullscreen gameplay around it, since almost
  everything actually renders edge-to-edge with no bars at all -- so
  rather than carry unused legacy config, they're gone. `[layout]`'s
  margin comment block is replaced by the `auto_letterbox_*` keys.
  **Breaking ini change**: an existing config with `capture_margin_*`
  set will have those values silently ignored, not migrated -- if you
  were using them to trim overscan rather than an actual letterbox
  (some capture chains show a few pixels of border the display itself
  would normally crop), auto letterbox threshold/stability won't
  reproduce that; there's no equivalent setting for it anymore.
- `auto_letterbox_enabled` now defaults to `true` (was `false` in
  v2.9). It's the only capture inset the plugin applies, so leaving it
  off by default would mean sampling starts at the true edge on every
  side, unconditionally, for anyone who doesn't know to turn it on.

### v2.9
- Added auto letterbox/pillarbox (black bar) detection, ported from
  the Android "inspiration" project's own `BorderProcessor.kt`: each
  edge is probed independently at 3 points (25/50/75% along the
  perpendicular axis) and scanned inward for the first non-black
  row/column, so an asymmetric bar (e.g. a status bar rendered only
  along the top) is handled correctly instead of being averaged away.
  A newly detected border only commits after
  `auto_letterbox_stability_frames` consecutive re-detections agree,
  to avoid a one-frame flicker snapping the LED geometry. New
  `[layout]` keys: `auto_letterbox_enabled` (off by default at
  introduction), `auto_letterbox_threshold`,
  `auto_letterbox_stability_frames`, `auto_letterbox_check_interval_frames`.
  At this point it added to the existing manual `capture_margin_*`
  fields rather than replacing them -- see v3.0 above, which removed
  those fields once this could stand on its own.

### v2.8
- WLED heartbeat: the plugin now resends the last known-good color once
  a second during any foregrounded gap where the pixel format is
  momentarily unknown or unrecognized, instead of going silent. Closes
  the remaining cases of the v2.2.3 "occasional flash of an unrelated
  color" -- that release made the underlying gap less frequent by
  speeding up the pipeline, but didn't remove it; a long enough gap
  could still trip WLED's own realtime-timeout fallback. This never
  invents a new color -- it only repeats the last one the real pipeline
  already verified -- and does not run while backgrounded/suspended,
  where going silent is still intentional (v2.4).
  Write-up: [`docs/debugging/wled-heartbeat.md`](docs/debugging/wled-heartbeat.md).

### v2.7.6
- Reverted the v2.7.3 stale-flip guard. It was blanking the strip
  whenever no flip landed within ~350ms, but menus and static loading
  screens often throttle or stop flipping while idle, and the gaps
  would drift back and forth across that threshold -- blank, resume,
  blank, resume, visible as flicker on anything that stays on screen
  for a while. Wasn't protecting against anything we'd actually
  confirmed happens (the real black-screen-shows-color bug turned out
  to be the v2.7.5 smoothing fix), so pulling it out rather than
  chasing a threshold that might just move the problem.

### v2.7.5
- Fixed color smoothing getting stuck a few shades above black instead
  of actually reaching it. Integer division truncates toward zero, so
  once a channel decayed down to ~3 the per-frame step rounded down to
  0 and just stopped -- the strip would hold a faint, permanent glow
  of whatever was on screen before a cut to black instead of settling
  the rest of the way. Confirmed on hardware: this was the actual
  cause of "black screen shows color", not the format/decode stuff
  chased earlier.

### v2.7.4
- Fixed 4 duplicate LEDs, one at each screen corner. Zone geometry
  generated each edge inclusive of both endpoints, so the corner
  shared between two edges got sampled by a zone from each edge --
  confirmed on hardware (zone 0 and the last zone were sending
  bit-identical raw pixel reads, every frame).

### v2.7.2
- Live HDR/SDR auto-detection for pixel format `0x80002200`, confirmed
  on real hardware.

### v2.7
- Real support for pixel format `0x80002200` (A8B8G8R8_SRGB),
  HDR-off case only.

### v2.6
- Fixed a bug where the internal opt-in gate silently swallowed the
  `true -> false` "off" send, so the light could get stuck on.

### v2.5
- Merged in the remaining feature-fork commits (v2.3.1, v2.4,
  v2.4.1).

### v2.4
- Merged in an advanced, opt-in networking signal from a fork based
  on the old v2.2 line, for a niche multi-source setup most installs
  don't need.

### v2.2.5
- Replaced the hardcoded debug IP with an opt-in `[dev]` ini section.

### v2.2.4
- Diagnostic telemetry now gated behind `__FINAL__==0` (debug builds
  only) instead of always running.

### v2.2.3
- Root-caused a color glitch to a WLED realtime-timeout fallback
  color, not a decode bug. Real fix, plus a performance pass adding
  a PQ tone-map LUT.

### v2.2.2 / v2.2.1
- Diagnostic-only builds: raw pre-decode pixel dump for 3 zones
  (~1x/sec), and pixel-format-on-change logging to the debug IP.

### v2.2
- Ported an existing reference color-processing implementation into
  the plugin: contrast, per-channel gamma/brightness, and a
  pipeline-ordering fix.

### v2.1.1 - v2.1.5
- Fixed an `fopen` crash, added real-hardware reload diagnostics,
  fixed a config size-read bug and a content-hash bug.
- Fixed live config reload never firing; raised the saturation cap
  to 300.

### v2.1
- Added `black_level`/`white_level` and `dark_threshold` settings,
  plus live config reload (no restart needed to pick up ini changes).

### v2.0
- Made everything that was previously hardcoded user-editable via
  ini settings.

### v1.2
- Moved zone sampling off the render/flip hook and onto a dedicated
  worker thread so a full sampling pass can never stall the game's
  frame submission. Added bounds checking and loop-timing telemetry.

### v1.0
- First real per-frame detile-to-WLED pipeline. Samples a fixed set
  of 229 screen zones per frame, matching a measured LED strip
  layout, and streams them to a WLED controller over the DDP UDP
  protocol. Pixel format and buffer tracking are lifted from
  `detile_verify_probe`, whose tiling math this plugin depends on
  being correct.

### Unreleased / maintenance
- Added `docs/` with an architecture overview and write-ups of the
  v3.3 flicker investigation, the v3.1 flip hook hunt and the v2.8
  heartbeat. The long investigation text that used to sit in the v3.3
  and v3.1 entries here moved there.
- More `docs/`, written from this changelog and the git history: a
  version history, notes on the sampling zones, the HDR format ID,
  letterbox detection, the black-screen smoothing bug, the companion
  app's updater, console gotchas, CI and the GoldHEN SDK patch,
  testing, the tools, config changes by version and a per-title notes
  page. Index in [`docs/README.md`](docs/README.md).
- Removed an advanced opt-in networking comment from the default
  ini, matching the companion app's matching fix -- those settings
  are hand-edited-only and don't need documenting in the shipped
  file.
- `dark_threshold` default changed from 0 to 10 (reverted to 0 in 82c8ca0), to match the
  companion app's default.
- Personal WLED IPs blanked out of the shipped default ini.
- `main.c` (3,240 lines) split into 9 modules (`gamma`, `settings`,
  `network`, `tiling`, `pixel_formats`, `zones`, `color_processing`,
  `hooks`, `sample_thread`) plus a shared internal header, for
  maintainability.

## Companion App

The companion app is a standalone PS4 homebrew UI (not a GoldHEN
plugin) for editing the plugin's ini config on-console, with a live
color preview and a plugin self-updater.

- **Shell feed support (off by default).** A new **Shell feed** card on Set up (after LED strip
  layout, before Live reload) with two switches. **Shell feed** (`[shell] enabled`, default off)
  turns the whole feature on; with it off the app is exactly as before (Home is byte for byte the
  same, nothing shell-related is checked, downloaded or written). **Autoload at boot** edits
  GoldHEN's payload AutoRun list: `ps4-ambilight.bin = 1` runs it at boot, `= 0` keeps the line
  but doesn't, and every other entry, comment and line ending in that file is left alone.
  Pressing that row while it can't be turned on says what is missing, in the order to fix it (Shell
  feed switch off, plugin not installed or not enabled, loader or shell feed files missing). Set up
  has no status line, so the hint goes in the save bar in place of "Changes apply after saving." and
  clears on the next button press.
  With the Shell feed on, Home shows a **Shell feed** banner to the left of the plugin banner (the
  same height, so the button doesn't move) and one button for everything pending. It installs or
  repairs the shell feed's two files (`ps4_shell_feed.prx` into `/data/ps4-ambilight`, creating the
  folder if needed, and `ps4-ambilight.bin` into `/data/payloads`, creating that too), and
  updates any of the three files that have a newer published build: one button reads "Update shell
  feed", "Install all (3)", and so on. **Test strip appears only once nothing is pending**, the
  same rule the plugin's own update already followed. Downloads use the same staging, sha256 check
  and rename as the plugin's. The app now saves `[shell] enabled` explicitly (the shell feed
  treats a missing key as on, so "off" has to be written); the plugin ignores it
  (`tools/test_plugin_config.c` loads the new golden ini through the plugin's real loader).
- **Fixed: LED flicker while the app was open.** With the shell feed running, the app's live
  preview (Set up, Customization, Test strip) and the shell feed both sent to WLED, which
  alternated between them. While the Shell feed switch is on and the app is sending, it now claims
  the strip through `/data/ps4_ambient_state` the way a game does (see
  [shell-feed-coordination](docs/development/shell-feed-coordination.md)). The claim follows the
  saved `[shell] enabled` as well as the switch, because the shell feed keeps running until the OFF
  is saved; it is released once the OFF is saved (or the app is closed, which saves).
- **Fixed: the app kept holding the LEDs after the PS button.** On Set up, Customization and Test
  strip it kept sending (and kept its strip claim) while in the background. It now polls
  `sceSystemServiceGetStatus` (`isInBackgroundExecution`, the same call the plugin uses) twice a
  second, and while backgrounded sends nothing and releases the claim; it takes both back when
  foregrounded.
- **Home checks GoldHEN's plugin loader.** With **Enable Plugins Loader** off in Settings > GoldHEN >
  Plugin Settings, GoldHEN loads no plugin for any game, and Home couldn't tell. The app now reads
  that switch from `/data/GoldHEN/config.ini` (`[PluginLoader] PluginLoader_Enabled`) and, when it
  is off, makes it the first thing Home shows: a "Plugin loader is off" banner and a **Close app to
  turn on plugin loader** button. Install, Update, Enable and Test strip wait until it is on. The app
  only reads the file: GoldHEN keeps the live value in memory and won't change it while an app is
  running, so writing it from here doesn't work (tried: the file said 1, GoldHEN's menu still showed
  it off). It's re-read every couple of seconds on Home and on coming back to the app. A missing file
  or key (an older GoldHEN) is treated as on. The in-app Help and the README no longer say Home
  can't see this switch.
- **Help and README for the shell feed.** The app's Help screen has a new **Shell feed (optional)**
  card (six cards now), and its Getting started and Troubleshooting cards mention the Test strip
  rule and the two-sources flicker. The main README has a **Shell feed (optional)** section, a
  quick start step and Help entries for Test Strip being hidden, Autoload not turning on and the
  strip flickering. The Set up row for **Shell feed** no longer says "Off keeps this app exactly as
  it was", which meant nothing to someone who never saw the earlier app.
- **Launch Ambilight payload button.** The Shell feed card on Set up has a new **Ambilight payload**
  row whose button, **Launch Ambilight payload**, sends `ps4-ambilight.bin` to GoldHEN's
  BinLoader (port 9090) so the shell feed starts without a reboot or a trip into GoldHEN's menu,
  whether or not Autoload at boot is on. It checks the same things as Autoload first (Shell feed
  on, plugin installed and enabled, payload and `.prx` on the console) and says what is missing.
  A successful install or repair now sends it too. It is a `ui` row, so it never reaches the ini.
  Its text said "Reset Game" because every action button was drawn as the preset reset; it now
  has its own text and icon.
- **One name for the payload.** Wherever the app, its Help and the docs meant `ps4-ambilight.bin`
  by "loader" or "launcher" they now say **Ambilight payload** (Set up, Home banners, status and
  hint messages, the download progress line). "Plugin loader" and "BinLoader" are GoldHEN's own and
  are unchanged. Not yet tried on a console: the button and the new wording
  ([local-install-testing](docs/development/local-install-testing.md)).
- **The release now carries the shell feed files.** A tagged release attaches
  `ps4-ambilight.bin` and its `.sha256` next to `ps4_shell_feed.prx` and its `.sha256` (the
  `.prx` pair was already attached), so the Shell feed install finds all of them on GitHub and
  no longer works only against the local test server. The `.bin` is the committed file in
  `payloads/`; the publish job checks it against the hash in `payloads/CHECKSUMS.md` and fails
  the release if they differ, then writes the sidecar from the file it attaches. A release
  has seven files now: two `.prx`, the `.bin`, their three `.sha256` files and the `.pkg`.
  `docs/development/ci-and-releases.md` is updated, and now says that `-rc` tags are allowed
  (they publish as pre-releases).
- **Fixed: the Ambilight payload button's text is tested.** The button text now comes from one
  function, `settings_action_text()`, instead of the drawing code, and a new
  `tests/test_action_buttons.c` fails if any action button lacks its own text, if one other than
  the preset reset says "Reset", or if the menu says loader or launcher for the payload.
  `tests/test_settings.c` no longer fails at the `relayHost` round trip: the test was stale (the app
  has kept `relay_*` keys only when the file already has them since `a4c154c`), so it now seeds
  the file and also checks that a fresh save adds none. No app behaviour changed.
- **`tools/update_payload_checksum.sh` keeps `payloads/CHECKSUMS.md` current.** After replacing
  `payloads/ps4-ambilight.bin`, run it and it rewrites the hash and size on that page from the
  file (the note about the build stays yours); `--check` only reports. CI runs `--check` on every
  push, not only on a tag, so a binary changed without the page fails straight away rather than
  at release time. The publish job uses the same script instead of its own copy of the check.
- **The companion app has a version.** `companion-app/include/app_version.h` holds `APP_VERSION`
  (still 1.00) as the one place it lives. The Help screen now says "App version 1.00" under its
  title, and the package's `APP_VER` and `VERSION` are read from that line by both CI and
  `build.bat` instead of being typed into each (CI fails the build if the line is missing).
  Bump it when the app changes; `docs/development/ci-and-releases.md` lists it in "Cutting a
  release". `tests/test_app_version.c` keeps its form right. Not yet run: a real CI package
  build and `build.bat` on Windows with the new lookup.
- **The on-screen keyboard moved out of `main.c` into `source/ime.c`.** The first step of
  splitting `main.c` (about 135 lines, now 2,829 from 2,952). No behaviour change: the code is the
  same as it ran on hardware, except that it reads and writes the config, the user id, the dialog
  position and the status line through an `ImeHost` that `main.c` passes in, so `ime.c` needs none
  of `main.c`'s globals. `tests/test_ime.c` is new and covers it with a fake keyboard (opening, a
  typed number applied and clamped, garbage and empty input rejected, cancel, the string row, a
  refused open, shutdown); it uses test-only SDK stand-ins in `tests/stubs/orbis/`. Not yet run:
  the CI package build and the keyboard on a console, since `main.c` can't be compiled on a PC.
- **The downloads and the release lookup moved out of `main.c` into `source/net_http.c`.** The
  second step of splitting `main.c` (it is 2,500 lines now, from 2,829). `http_download()`,
  `http_download_text()` and `resolve_asset_url()` are in `net_http.c` with `include/net_http.h`,
  and the GitHub repo and release API URL with the note on why the API is used are in that header.
  The code is the same as it ran on hardware. What changed is wiring: the cached `[dev]
  update_base_url` is now set with `net_set_dev_base_url()` and read with `net_dev_base_url()`
  instead of a `main.c` global (the ini is still read on the main thread only, in `main.c`), and the
  JSON scan for an asset's url is a header, `include/github_release_json.h`, so it can be tested:
  `tests/test_github_release_json.c` is new. Not yet run: the CI package build and an update or
  install on a console, since `main.c` and `net_http.c` need the OpenOrbis toolchain. A syntax check
  of `net_http.c` against stand-in headers was clean.
- **`main.c` is split into modules.** The rest of the split, done in one go: the background update
  check (`source/update_check.c`), plugin install, enable and registration plus GoldHEN's plugin
  loader switch (`source/plugin_install.c`), the shell feed's install, repair, payload launch,
  autoload and strip claim (`source/shell_feed.c`), small file and checksum helpers
  (`source/file_util.c`), the on-console paths and asset names (`include/app_paths.h`) and what
  `main.c` shares with them (`source/app_state.h`). `main.c` is 1,248 lines now, from 2,952 at the
  start of the split; the input handlers, the live preview and the main loop stay in it. No
  behaviour change: the code is the same as it ran on hardware (every old line is accounted for;
  what changed is `static` on the functions and globals the modules now share, includes, and the
  doc comment of `ensure_plugin_registered_in_goldhen()`, which had drifted away from it, now sits on
  it). Checked by compiling the old `main.c` and the new files against stand-in SDK headers and
  comparing the errors: nothing new in any of them. Not yet run: the CI package build, and the
  update check, a plugin install, a shell feed install, Launch Ambilight payload and autoload on a
  console. Since the functions are no longer `static`, a link error about a duplicate symbol, if CI
  reports one, would be a name that clashes with one in the SDK.
- **The Ambilight payload now carries offsets for several firmwares.** Replaces the single-firmware
  build (`payloads/ps4-ambilight.bin`, 24,000 bytes now, was 22,816): it picks the right firmware's
  offsets at runtime instead of only knowing 9.00, and refuses cleanly on a firmware it doesn't
  recognise (writes `PLUGIN_DIR/unsupported_fw`, touches nothing else). Offsets are complete for
  5.05 through 12.02; **only 9.00 is confirmed on a real console so far**, the rest are untested.
  12.50 and up aren't supported yet (missing kernel offsets for those). Re-sending the payload to
  an already-running console was tried deliberately on 9.00, to check the fallback path refuses
  rather than re-injecting or crashing -- it did.
  `shell-feed/docs/support-matrix.md` has the full per-firmware table (from the offset work in the
  research repo, `ps4-shell-capture-research`). `payloads/CHECKSUMS.md`, the top-level README,
  `shell-feed/README.md` and the companion app's Help screen no longer say "9.00 only"; they now
  say which part is confirmed and point to the matrix for the rest. No code in this repo changed
  the firmware-detection or offset logic -- that's the payload's own binary, built elsewhere.
- **Firmware warning, and shell feed notifications.** Only the shell feed needs a firmware check
  (the game plugin works on every firmware). With the **Shell feed** switch on, Home shows a warning
  panel when the console's firmware is one the payload has no complete offsets for (12.50 and up,
  or one not in `shell-feed/docs/support-matrix.md`), and launching the payload says it again in
  the status line. It is a warning, never a block. The firmware is read once at startup from
  `kern.sdk_version` (`source/fw_check.c`), as BCD (12.50 is `0x1250xxxx`); a value that isn't
  valid BCD shows nothing. The support table and the text are in `common/fw_support.h`, with a copy
  in `companion-app/include/` because this app only has `-Iinclude` (keep them identical), and
  `tools/test_fw_support.c` checks them. With no WLED Host set, the live preview's status now says
  so instead of calling an empty host "not a valid IPv4 address".
  The shell feed itself puts a line in the console's notification bar when it starts sending
  ("running", once per boot), when it can't start (no `wled_host`, `wled_host` isn't an address, no
  display buffers, `socket()` failed; none of these is retried until a reboot, and the message
  says so), when `/data/ps4_shell_feed_off` pauses it, when `[shell] enabled=false` pauses it while
  it was running (not at every boot where it has simply been off), and when it resumes. Not yet
  run: the plugin, shell feed and companion app builds, and any of this on a console. In
  particular that `kern.sdk_version` reads in the companion app, that the BCD reading is right, and
  that a notification from the compositor process (where the shell feed runs) shows.
- **Local update source for testing.** `[dev] update_base_url=http://<pc>:8000` in the ini makes
  every download and its `.sha256` come from that address instead of GitHub; delete the line to go
  back. `tools/serve_local_release.py` serves the three files with their checksums.
  Plain `http://` works on the console.
- **Build:** `sceKernelMkdir` (a first use in this repo) creates the two folders; it links and works on
  a console. `APP_MKDIR` in `main.c` is the one line to swap if a later SDK breaks it.

- The app now writes the short ini and needs plugin v3.8. Both presets are saved in full to
  `[preset_game]` and `[preset_movie]` with `[presets] active=`, and the old flat copies of
  those values are removed from `[layout]`, `[color]` and `[timing]`, which keep setup values
  only. This replaces the arrangement in the next entry, where the flat keys held the active
  preset for a plugin that didn't read the preset sections. An ini that still has the old
  flat keys loads as before (they become Game) and is converted on the first save. Once an
  ini has preset sections the app ignores any flat leftovers, the same as the plugin.
  `auto_letterbox_stability_frames` and `auto_letterbox_check_interval_frames`, which have no
  row in the app, are only written when they differ from their defaults. One save call now
  writes the whole file (`presets_save`). `tests/test_presets.c` also checks that the plugin's
  shipped ini agrees with the app's shipped presets, and that the app's output matches
  `tools/data/preset_ini_golden.ini`, which the plugin's own test loads.
- Added Game and Movie presets to Customization. A Preset card at the top
  switches between them (D-Pad or L1/R1), and every value below it belongs
  to the preset showing, so switching never overwrites the other one's
  tweaks. There is no Custom mode: editing a field edits the preset you're
  in, and Reset (X twice) puts just that preset back to its shipped values.
  Game ships at edge depth 2, brightness 100%, saturation +35%, gamma 2.2,
  black level 0% and smoothing on at 50 ms. Movie ships at edge depth 4,
  brightness 80%, saturation +25%, gamma 2.4, black level 1% and smoothing
  on at 200 ms. Everything else stays at the plugin default. The gammas
  assume WLED isn't applying its own gamma to the DDP stream. Setup values
  are never part of a preset. The plugin isn't changed: the flat `[color]`,
  `[timing]` and `[layout]` keys it reads always hold the active preset,
  and both presets are also written in full to `[preset_game]` and
  `[preset_movie]` with `[presets] active=`. An ini from before presets
  keeps its tuning as Game and Movie starts as a copy with Movie's
  smoothing. New `source/presets.c` and `tests/test_presets.c`.
- Added a "by Shimy" credit at the bottom right of Home, in Michroma (SIL
  OFL, licence in `assets/fonts`).
- Fixed the Save button losing its focus highlight on Customize. The
  screen field counts were hardcoded (`SETUP_FIELD_COUNT` 12,
  `CUST_FIELD_COUNT` 20), so once rows were hidden from the schema the
  D-Pad moved focus onto Save at the real count while the render code
  kept waiting for the old one. Both now come from
  `ui_screen_item_range()` and the constants are gone.
- Hid the auto letterbox "Stability (frames)" and "Recheck every
  (frames)" rows. They're debounce timings tuned alongside the plugin's
  new detection scan, and lowering stability to "react faster" just
  brings the flicker back. They're still loaded and saved like any
  other key, and since they no longer have a schema row to clamp
  against, `settings_load` bounds them itself (1-30 and 1-300). Edge
  depth and Reload check stay visible.
- "Edge depth" now tops out at 4 (was 10), matching the plugin's new
  `SCAN_DEPTH_MAX`. Saturation and Contrast now top out at +100% (were
  +300%), matching the plugin.
- The Help screen's "Tune the picture" card still told people to shorten
  the recheck interval, which isn't in the app anymore, and it said to
  *lower* the letterbox threshold when the bars aren't pure black. It's
  the other way round: a pixel counts as black if it's below the
  threshold, so bars at 25 need a threshold above 25. Same mistake in
  the README's troubleshooting entry; both fixed.
- Brightness, both black thresholds, saturation, contrast, black/white
  level, RGB balance and per-channel gamma now show as percentages
  (`DisplayUnit` on `MenuItem`). Display only: `brightness` is still
  0-255 in the ini and the config struct, the app just shows 255 as
  100% and steps it by 5%. Typing a value in the keyboard dialog takes
  a percent too. Untouched values are saved exactly as loaded. See the
  README for the full mapping.
- `test_settings.c` asserted 32 menu items when the schema already had
  33; now asserts the real 31. It still fails at the `relayHost`
  round-trip, which was failing before this and isn't touched here.
  Added `test_display_units.c` for the percent conversion and the
  load-time bounds on the hidden keys.
- Removed the four "Capture margin top/right/bottom/left" fields from
  Customize's Screen sampling card, and added a matching "Auto
  letterbox" card (enable toggle, bar threshold, stability, recheck
  interval) -- follows the plugin's own v3.0 removal of the
  `capture_margin_*` ini keys in favor of always-on auto letterbox
  detection.
- Added a "Smoothing preset" picker (Off/Responsive/Balanced/Smooth) to
  Customize's Motion and darkness card, next to the existing raw
  "Settling time (ms)" field -- values ported from the Android
  "inspiration" project's own `ColorSmoothing.applyPreset` (50/50/200/
  500ms). This is a UI-only convenience: cycling it writes the two real
  `smoothing_enabled`/`settling_time_ms` fields directly rather than
  persisting a third setting of its own, so it can't drift out of sync
  with them, and the raw ms field is still there to hand-tune if
  neither preset's snapped to fits.
- Reworded Setup's WLED IPv4 field description -- it previously read
  "New setup starts with local-network discovery," left over from a
  plan that was never actually built (there's no scanning code
  anywhere in this app). Now just describes what exists: manual IPv4
  entry, DDP on UDP port 4048 by default.
- Fixed every create/overwrite `sceKernelOpen()` call missing
  `O_TRUNC`: the literal `0x200 | 0x001` was commented as
  `O_TRUNC|O_CREAT`, but on Orbis (FreeBSD-derived) that's actually
  `O_CREAT|O_WRONLY` -- the real `O_TRUNC` bit (`0x0400`) was never
  set. Harmless against a brand-new path, but a path with leftover
  bytes from a previous write -- most importantly
  `do_plugin_update()`'s `.new` staging file -- could keep a stale
  tail after a shorter rewrite, so the file that landed on disk could
  be longer than the SHA-256 verified during download. That's why
  `check_for_plugin_update()` kept reporting "update available" on
  every relaunch even when the release hadn't changed. Added
  `ORBIS_O_CREAT_TRUNC_WRONLY` (correct BSD flag values) to
  `plugin_common.h` and switched every affected call site
  (`config.c`'s ini writer; `main.c`'s tmp download, checksum
  sidecar, `plugins.ini` rewrite, and staged `.prx` copy) to use it.
  Also removed the temporary debug hash dump from the update-check
  status line.
- Fixed a startup crash on real hardware: the background update-check
  thread (see below) was created via `SDL_CreateThread()` with stack
  size 0 ("use the platform default"), and on this SDL port that
  default is small enough to blow past almost immediately -- a
  `SIGSEGV` write fault at exactly `rsp-8` on every launch. Switched
  to the same `scePthreadCreate()` pattern the plugin side already
  uses for its own sampling thread, with an explicit 256KB stack.
- Fixed `check_for_plugin_update()` trusting a stale checksum sidecar
  over the real installed file. It read the
  `PLUGIN_INSTALLED_CHECKSUM_PATH` sidecar first and only fell back
  to hashing the actual `.prx` if that sidecar was missing -- but the
  sidecar is only ever written by this app's own updater, so dropping
  a different build in by hand (FTP, a manual GoldHEN plugins-folder
  edit) left the sidecar pointing at the old build while still
  matching the latest release, so the check kept reporting "up to
  date" while a different build was actually running. Now hashes the
  real file first and only falls back to the sidecar if that read
  fails.
- Moved the plugin update check off the main thread. It does two
  sequential blocking HTTPS round trips to GitHub, and running that
  inline at startup held the UI hostage until both finished -- the
  actual cause of the app feeling slow to open, worse on a bad
  connection where each request could hit its full 10s timeout. Now
  runs on a background thread right after the home screen is up,
  polled once a frame via a pair of atomic flags. Also added the
  missing `sceHttpSetRecvTimeOut()` to both HTTP helpers, which
  previously had no bound on a connection that opened and then
  stalled mid-response.
- Wired up plugin update detection: `UI_INSTALL_UPDATE` already
  existed in the UI enum and had a screen ready for it, but nothing
  ever set it. Added a local checksum sidecar written after every
  successful install, and a startup check that compares it (or the
  installed `.prx`'s own hash, as a fallback) against the latest
  release's published checksum, flipping to the update state on a
  mismatch. Fails silently on any network/IO error, since this runs
  unprompted on every launch.
- Resolves GitHub release assets via the `api.github.com` releases
  API instead of `releases/latest/download/...` links, requesting
  `Accept: application/octet-stream` so the redirect goes straight to
  `objects.githubusercontent.com` rather than through github.com's
  web app. The updater now resolves both the `.prx` and `.sha256`
  asset URLs up front, failing fast with a clear status if either is
  missing from the release.
- Fixed the self-updater's `http_download()`/`http_download_text()`
  always failing ("Download FAILED -- check PLUGIN_UPDATE_URL and
  network") even against a valid `PLUGIN_UPDATE_URL`/
  `PLUGIN_CHECKSUM_URL` and a working network. Both URLs are GitHub
  `/releases/latest/download/...` links, which GitHub serves as a
  302 to a signed `objects.githubusercontent.com` URL rather than
  the asset itself; `sceHttp` doesn't follow redirects unless told
  to, so the strict `statusCode == 200` check was seeing the 302 and
  failing every time -- worked fine in a browser (which follows
  redirects transparently) but never on-console. Added
  `sceHttpSetAutoRedirect(tpl, 1)` on both request templates.
- Added a QR-code popup to the Help screen (Triangle), pointing at
  this repo's README `## Help` section -- the on-console cards stay
  short on purpose, so this is the hand-off to the long-form version
  (setup walkthrough, full ini reference, troubleshooting) that
  doesn't fit in a controller-navigable list. Vendored Project
  Nayuki's QR Code generator library (`qrcodegen.c`, MIT license) for
  the encode; the popup itself draws the module grid straight into
  the canvas with `ui_fill_rect_fast()` rather than shipping a
  pre-rendered image asset. Round-tripped through a real QR decoder
  off-console to confirm the encoded bitmap is correct -- see the
  companion app's own README for what's still unverified about how
  it reads on an actual screen and camera.
- Added a Help screen (five scrollable cards: getting started, strip
  setup, picture tuning, controls, troubleshooting), reachable from
  Home and navigated the same way as Setup/Customization. Live
  preview is now scoped to an explicit Setup/Customization check
  instead of "anything that isn't Home", so Help doesn't
  inadvertently send DDP frames to the real strip either.
- Fixed a crash on close: the app previously fell off the end of
  `main()` after `SDL_Quit()`, which isn't a valid way to end a
  process launched via the PS4's `LoadExec` and reliably crashed with
  a `SIGSYS` inside `libkernel.sprx`. Now exits via
  `sceSystemServiceLoadExec("exit", NULL)`, the same pattern used by
  Apollo Save Tool and ItemzFlow.
- Fixed unsaved settings being lost on close -- the app now flushes
  the in-memory config to disk before shutdown, not just on an
  explicit Save press.
- Fixed the Home screen's "Test Strip" card showing a stale
  "not set up yet" state after Setup -> Save, until the app was
  fully closed and reopened.
- Fixed three advanced, hand-edited-only ini keys being written on
  every save even when never set -- there's no settings-UI row for
  them; a fresh install's ini now stays clean unless you add them
  yourself.
- Updated the app icon.
- Initial version: settings editor, live color preview (mirroring
  the plugin's own gamma/brightness pipeline), and a plugin
  self-update path over HTTP.
- Brought in an advanced opt-in networking signal to match the
  plugin, then later removed the dedicated UI card for it -- it's
  ini-only now, matching how the plugin itself exposes it.
- `dark_threshold` default changed from 0 to 10, matching the plugin (both reverted to 0 in 82c8ca0).
- Added disabled-plugin detection and blanked default WLED IPs out
  of shipped defaults.
- Settings-UI, layout-editor, and color-pipeline updates across
  several point releases (v15-v24): scrollable settings list once
  content exceeds one screen, a full-strip LED layout geometry
  rewrite (`layout.c`), and assorted merge/reconciliation fixes
  between parallel lines of work.
- Text rendering switched to FreeType directly (`text_render.c`),
  since SDL2_ttf wasn't a compiled lib in the target SDK snapshot.
- Controller navigation fixed on real hardware by removing
  `SDL_INIT_JOYSTICK`, which was contending with the native `scePad`
  handle for the same controller and causing stale button state.

## Tools

- `tools/decode_verification_dump.py` -- decodes the plugin's raw UDP
  debug/telemetry packets for offline analysis. Went through several
  format-support passes as the plugin's own wire formats changed:
  registration-event decoding, config-reload packet decoding,
  auto-selecting the unpack format from registration packets, timing-
  packet decoding, HDR/PQ decoding, an R/B channel-swap fix for
  A8B8G8R8_SRGB, and runtime pixel-format auto-detection via decode
  smoothness.
- `tools/ps4_detile_2dthin.c` -- reference detiling implementation used
  to cross-check the plugin's own tiling math.
- `tools/udp_ground_truth_listener.py` -- minimal UDP listener used for
  capturing real packets from the plugin during development.
- `tools/flicker_capture.py` -- captures and analyzes the flicker probe
  packets ("FLK1", 132 bytes, one per sampler pass) from a debug build
  of the plugin. `capture <file> [--seconds N]` records to a `.flk`
  file, `analyze <file> [--csv out.csv]` prints a report (output
  steadiness, sampler timing, flip rate, all-zero reads and burst
  spacing, per-zone tear/disagreement between buffers, a ring-lag
  table showing which slot is safe to read, and which lag the pipeline
  actually read, so a working sample-lag fix is recognised), and
  `selftest` runs the analyzer against 10 synthetic scenarios. Needs a debug build of the plugin
  (`make DEBUG=1`, the FLK1 probe is compiled out of release builds) with
  `[dev] dev_ip` and `dev_logging=true` set. Uses UDP port 4048, so it
  can't run alongside `udp_ground_truth_listener.py`. Its verdict
  thresholds are estimates; trust the printed numbers. Found the
  cleared-buffer sampling bug described under Plugin > v3.3. Also counts
  how often the `0x88740000` 8-bit decode (FLK1 flags `0x10`) switched.
- `tools/test_pq8bit_vote.c` -- host test for the alpha byte vote behind the
  YouTube fix, run on words from real captures and on generated PQ. Builds
  with plain `gcc`, no SDK. See Plugin > v3.5.

## CI

- A tag push now fails early if the tag doesn't match `g_pluginVersion`
  (major.minor only, the tag's patch part is free). It's the first step
  of `build_prx`, and `publish` needs that job, so a mismatched tag such
  as `v3.5.0` on a plugin that still says 3.4 produces no release.
  Non-version tags like `v3.4.0-rc1` are rejected too. See
  [`docs/development/ci-and-releases.md`](docs/development/ci-and-releases.md).
- Tagged-release builds now also build and package the companion app
  (`build_pkg` job: compiles `companion-app/source/*.c` against the
  same OpenOrbis toolchain the plugin job uses, via its Linux
  `create-fself`/`create-gp4`/`PkgTool.Core` binaries instead of
  `build.bat`'s Windows ones, producing a `.pkg`). Releasing is now
  its own `publish` job that waits on both the plugin and companion
  app builds and is the only job that touches
  `softprops/action-gh-release`, so the two builds run in parallel
  without a chance of racing each other to create the same release.
- `build_pkg` pinned to `ubuntu-22.04` instead of `ubuntu-latest`:
  `PkgTool.Core`'s `pkg_build` subcommand needs `libssl1.1`, which
  `ubuntu-latest` (24.04) no longer ships, so it aborted with "No
  usable version of libssl was found" on the first real run.
- First real compile of `ui_icons.c` (this CI job is the first time
  the companion app has ever actually been build-tested) turned up a
  real bug, not a CI issue: `CUR`, the "currentColor" sentinel used
  84 times across the file's icon tables, was a `static const
  UiColor` object. Clang hard-errors reading a named const object's
  value into another file-scope initializer ("initializer element is
  not a compile-time constant") -- GCC's default GNU-extension mode
  had been silently tolerating this locally. Changed `CUR` to a
  compound-literal macro, matching the file's own `CHAN_RED`/
  `CHAN_GREEN`/`CHAN_BLUE` pattern, which doesn't have this problem.

