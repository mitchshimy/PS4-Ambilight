# Plugin v4.x release notes

Full release notes, moved out of the [changelog](../../CHANGELOG.md), which now keeps one line per change.

### v4.0.1
- **PS4 Pro: titles that run in Pro (Neo) mode use `libSceGnmDriverForNeoMode`, and the plugin now hooks it (experimental, `report_file=1` only).** On a
  Pro the strip stayed dark in Red Dead Redemption, Devil May Cry 5, NFS Hot Pursuit Remastered, WRC 10 and YouTube. Load logs from a Pro (firmware 11.02)
  split by `sceKernelIsNeoMode`: those titles reported 1, and in each `libSceVideoOut` loaded, `libSceGnmDriver` returned -1 with handle 0 (so `plugin_load`
  stopped at the early return on a zero library handle and hooked nothing), and the module list had `libSceGnmDriverForNeoMode.sprx` with
  `sceGnmSubmitAndFlipCommandBuffers` and its `ForWorkload` variant both found in it. Assetto Corsa (CUSA01830) and CUSA00900 reported 0, loaded the normal
  driver and were captured. **How the strip is driven does not change for any title that works.** Three parts, all `report_file=1` only:
  - **Module census on a stop.** The `STOP: a video library handle is 0` branch also logs `console (at STOP): sceKernelIsNeoMode = ...`, one
    `module: handle N name ...` line per loaded module (handles 0 to 511 scanned the way the shell feed scans them, 200 us each, at most 160 lines; gnm and
    videoout names marked), a count, and for each module with `gnmdriver` in its name whether the two flip functions resolve in it, with their addresses.
    New files: `plugin/source/module_census.c` and `plugin/include/module_census.h` (the shell feed does not build them).
  - **The Neo driver as the Gnm driver.** When `libSceVideoOut` loads and `libSceGnmDriver` does not, `plugin_load` loads `libSceGnmDriverForNeoMode.sprx` by
    name, falling back to the first `gnmdriver` module in the module list that has `sceGnmSubmitAndFlipCommandBuffers`, and continues with the same symbol
    lookups, forwarding-stub check and hooks. Titles that load the normal driver never reach it. On the Pro this works for Red Dead Redemption,
    Devil May Cry 5 and NFS Hot Pursuit Remastered (Neo driver at handle 56, 52 in Devil May Cry 5; formats `0x80002200` and `0x88740000`; 1,700 to 3,400
    flips by T+60; no guard rejects) and for YouTube and Netflix with `0x88740000`, all with correct colours by eye.
  - **What the title itself has loaded.** Tekken 7 (CUSA06014, Neo mode) fails differently: `libSceGnmDriver` loaded by name (rc 0, handle 135, `libSceVideoOut`
    at 50), the hooks installed, one buffer registration was seen, and all flip counters stayed 0. The guess is that our own load made a copy of the normal
    driver that the title never calls. `module_census_snapshot` now reads the module list at the start of `plugin_load`, before anything is loaded by name,
    and gives the handles the title has for `libSceVideoOut.sprx`, `libSceGnmDriver.sprx` and `libSceGnmDriverForNeoMode.sprx`. If the title has the Neo driver and
    never had the normal one but our load returned a handle, the hooks go on the Neo driver instead; if it has both, the normal one is kept and that is logged
    (hooking both would need a separate trampoline per target); if it has neither, that is logged. The hooked handles are kept (`g_censusHookedVideoOut`,
    `g_censusHookedGnm`), and when the progress thread reports `WAITING` for a title that registered buffers but shows no flips, it logs them and a census.
  Costs: with `report_file=1` the snapshot adds a short scan to every title's load; with it off none of this runs. Write-up:
  `docs/debugging/neo-mode-gnm-driver.md`. The version stays 4.0.1. Host test: `tools/test_load_trace.c` gained stubs for the new calls and a check that the
  census follows the no-flips line once. Built on the OpenOrbis toolchain with the patched GoldHEN SDK (release). Run on a base PS4: a title with the normal
  driver logged as before and Apollo Save Tool stopped with the census lines. **The snapshot and the Tekken 7 change have not been run on any console.**
  Not looked at yet: Street Fighter 6 lights the strip but flickers.
- **Load log: one shareable file that shows where `plugin_load` stopped in a title (for the PS4 Pro reports).** On a Pro, Assetto Corsa
  and Apollo Save Tool light the strip, while Red Dead Redemption, DMC5, Dark Souls, NFS and YouTube show no "debug logging is on"
  notice and write no report, so the plugin either was not loaded into them or stopped early in `plugin_load`, and nothing could tell
  those apart. **How the strip is driven does not change.** With `[compat] report_file=1` the plugin now appends to one file,
  `/data/ps4-ambilight/ps4_ambient_loadlog.txt`, every line tagged with the title ID, so one file covers whichever game was played. Each step of `plugin_load` leaves a line (title and `sys_sdk_proc_info` result, the config values that matter, the load
  result and handle of `libSceVideoOut` and `libSceGnmDriver`, which symbols resolved, the first byte at each hook target with 0xE9
  meaning a forwarding stub, `sceKernelIsNeoMode`, hooks installed, thread results, "load completed"), so the last line is where it
  stopped. **No lines for a title at all means GoldHEN never loaded the plugin into it.** The early return on a zero library handle,
  which had always been silent, now writes a `STOP:` line and shows a notice. Once loaded, a one-second thread adds what the title does
  next (buffers registered, flips seen, sampler running, or `WAITING:` after 20 s with nothing) and a counter summary at 10, 30 and 60 s.
  Four of those events also show a short on-screen `PS4 Ambilight: diag: ...` notice. Lines made before the ini is read wait in memory and
  go out, in order, once `report_file=1` is known. The file is appended to, and started over only when a launch finds it over 48 KB.
  With `report_file` off nothing is written, no notice is shown, the thread is not started and no hook byte is read. New files:
  `plugin/source/load_trace.c`, `plugin/include/load_trace.h` and `load_trace_text.h` (the text and rules, so a host test can drive them).
  The shell feed does not build them.
- **Report file: a failed write no longer ends it for the session.** After 3 failed writes in a row the report stopped for good, so one bad
  moment on the disk early in a launch meant no report at all. It now tries once more every 30 s. The tick and the file text are unchanged.
- The version stays 4.0.1: `AMBIENT_VERSION_STRING` is unchanged and `g_pluginVersion` stays `0x0400`. Host test: `tools/test_load_trace.c` (new,
  with `tools/host_stubs/load_trace/`); it also passed with four deliberate regressions, to check it would catch them. Built on the OpenOrbis
  toolchain with the patched GoldHEN SDK (release, debug and shell feed). Run on a base PS4: the log ran from `plugin_load entered` to
  `sampler running` and the summaries, with the report written as usual. The sampler counts idle passes before a title registers any buffer
  (30 at T+1s in that log), so `sampler running` now waits for the first registration. Not run on a PS4 Pro.
- **Report file: the console model and a base-vs-Neo tiling check (groundwork for the PS4 Pro).** The plugin reads a picture with the
  tiling of the original PS4 and the Slim (base), confirmed on a console. The PS4 Pro (Neo mode) lays a picture out differently, the Neo
  set in `tools/ps4_detile_2dthin.c` was never compared with a real Pro, and the plugin had no Neo code, so on a Pro the strip could read
  scrambled pixels. Nobody on the project has a Pro, so this adds the means for one report to decide it. **How the strip is driven does not
  change.** With `[compat] report_file=1` the report gains two lines: `console:` (from `sceKernelIsNeoMode`, looked up at load like the other
  optional kernel calls, `unknown` if libkernel lacks it) and `tiling_check:`, which reads 832 pairs of pixels across 8x8 tile edges and 832
  inside tiles through both layouts and says `base fits`, `Neo fits`, `flat picture`, `unclear` or `no data`, with the numbers. Only steps
  across a tile edge tell the layouts apart, because a wrong layout keeps each 8x8 block whole and moves it. The check runs once every 120
  sampler passes, stops at the first decisive answer or after 30 tries, reads only inside `g_readableLimit`, and does nothing with the report
  off. `tiling.c` gains `kParamsNeo` and the P16 pipe and 8-bank math (base results unchanged on all 2,073,600 pixels); the check is the only
  reader of it. The check is in `plugin/source/tile_check.c` (plugin only; the shell feed, which shares `tiling.c`, does not build it) with
  its scoring in `plugin/include/tile_select.h`. The game report form asks for the console. On synthetic pictures it never named the wrong
  layout (0 of 90) and named the right one for about 70%; the rest came out `unclear` or `flat picture`. Not run on a console: whether the
  Neo parameters are right, whether a Pro picture is in that layout, how the thresholds do on real frames, and `sceKernelIsNeoMode` itself.
  The full version is 4.0.1 and `g_pluginVersion` stays `0x0400`. Details in [ps4-pro-tiling](../debugging/ps4-pro-tiling.md). Host tests: `tools/test_tile_select.c` (new)
  and `tools/test_report.c` (the two new lines).
- **Companion app: the firmware warning says it is about the shell feed, and 11.02 is now confirmed.** The Home warning (shown only with the
  Shell feed switch on, and repeated in the status line after the payload is sent) called the firmware "not yet verified with the shell
  loader" and said "only 9.00 and 12.52 are confirmed". "Shell loader" means nothing to most people and the text never said what it did not
  cover, so it was unclear whether game lighting was confirmed on 12.52. It now reads "Firmware 11.52 hasn't been tested with the
  shell feed. This only concerns the shell feed. Game lighting doesn't use it.", and for a firmware outside the loader's list "Firmware
  14.00 isn't supported by the shell feed. It will most likely not start. Game lighting doesn't use the shell feed." It still promises
  nothing about the game plugin on any firmware. **11.02 moves from untested to confirmed** for the shell feed: the shell feed's own report
  (a PS4 Pro, GoldHEN 2.4b18.12) shows state `live`, 3 live sessions and 169,341 frames sent in 6,483 s. GoldHEN 2.4b18.12 is also
  reported working with the plugin and the shell feed. The README, the Help text, `shell-feed/README.md`, `support-matrix.md` and
  `loading.md` list 11.02 and cite the report. `fw_support.h` (both copies) holds the list and the wording; `tools/ui_preview.c` now
  uses the real wording instead of its own sample. `APP_VERSION` is unchanged, so installed apps are not offered anything for this until
  the next app release. Host test: `tools/test_fw_support.c`.

### v4.0.0
- **Plugin v4.0.** Everything in this section ships as v4.0: the loopback TCP lease hand-off, the shell feed's heartbeat move and the
  other file-operation changes below. `g_pluginVersion` is `0x0400` and `AMBIENT_VERSION_STRING` is `4.0.0`; `tools/test_report.c`
  checks that the two agree and that this heading exists. Entries below that say `g_pluginVersion` is unchanged were written while
  they were unreleased and are left as they were. The plugin's "plugin updated to v4.0 (was v3.9)" console notice appears once
  after installing it. Tag the release `v4.0` or `v4.0.0`.
- **Lapy's PS4 Player flashed white and blue on part of the strip; it now samples one flip back.** The player has a 2-slot swap chain
  at format `0x80000000`, and with `AMBIENT_SAMPLE_LAG` 2 the plugin read a slot that was still being rewritten: about one pass in ten
  caught a flat grey-white (203,203,203) over a blue menu, in a few zones at a time. The read-verify caught the ones it could see, but a
  step that lasts several passes passes it by design, and the flashes showed in the menu, before playback and on a static screen.
  `plugin/include/lag_titles.h` lists title IDs that read one flip back (`LAPY20002`); every other title keeps lag 2. The sampler logs
  `LG_LAG_TITLE_OVERRIDE lag=1 title=...` once when it applies. Confirmed on a console: the player's reads came from the lag 1 slot and
  the flashes stopped, and RDR1's reads still came from the lag 2 slot. A rule keyed on the slot history alone (use the previous slot
  when two flips back equals the current one) was tried and dropped: it brought RDR1's flicker back. Details in
  [sample-lag-and-boot-flash](../debugging/sample-lag-and-boot-flash.md#a-title-that-needs-lag-1-lapys-ps4-player).
- **Companion app 1.02: it now tells you when a newer app is out.** The app could find a newer plugin or shell feed but never a
  newer version of itself, so a release that needed a new app (the shell feed work) left people on the old one with no hint. Every
  tagged release now carries `ps4-ambilight-app.version`, a one-line file CI makes from `app_version.h`. At launch the app fetches it
  from the latest release, compares it with its own `APP_VERSION` (as numbers, so 1.10 is newer than 1.09) and, when it is newer,
  shows **App update available: 1.03** on Home, a line on Help and a status line with where to get it. It asks whether or not the
  plugin is installed, in its own slot beside the LED and firmware warnings so neither can hide it, and says nothing when offline,
  when the release has no version file or when the text isn't a version (it asks again next launch). It can't install itself, but
  **Triangle on Home downloads the new `.pkg`** to `/data/pkg` (making the folder when it is missing, as the console's installer lists packages only there) as
  `ps4-ambilight-app-1.03.pkg`, with its version in the name, checks it against a published
  `.sha256`, deletes it if the checksum is wrong, removes this app's older packages first (by name, so nothing else in the folder is touched), and says to install it from **GoldHEN > Debug Settings >
  Package Installer** (it lists packages in `/data/pkg`, not `/data`). The app also posts "App update v1.03 available" in the console's
  notification bar when it starts and finds one (once per version per launch, following the Notifications switch). CI now publishes the package as `ps4-ambilight-app.pkg` with its `.sha256` (nine release files).
  `tools/serve_local_release.py` serves them too (`--app-version 9.99 --pkg X.pkg` to try it). Apps older than
  1.02 can't know, so the plugin's "updated to v4.0 (was v3.x)" notice, shown once when crossing v4.0, now ends with "Also update the
  companion app". Host tests: `tests/test_app_update.c` (the version comparison), `tests/test_app_update_check.c` (the check's
  state machine, eight scenarios) and `tests/test_app_download.c` (the download over a fake disk). The download, the installer
  listing the file in `/data/pkg` and the notification were confirmed on a console.
- **Companion app 1.02: the controls no longer feel sticky.** Host measurements (not yet timed on a console) showed the app was built
  with no optimisation flag (clang `-O0`) while its UI is a CPU rasteriser, so one redraw of Set up cost about 2.3x what it should, and
  a press that began and ended inside one slow redraw was never seen. Changes: (1) `-O2` in `build.bat` and CI. (2) The pad is read
  with `scePadRead`, which returns every sample queued since the last read; each is stepped through the same edge and repeat logic in
  time order (`source/pad_input.c`, host test `tests/test_pad_input.c`). Cross, Circle, Triangle, Square and Options act at most once per
  loop pass (directions and L1/R1 are never limited), so a mashed button can't repeat an action behind a slow frame. Buttons held when the
  on-screen keyboard closes, when the app returns from the background, or after a press that did blocking work are ignored until
  released, and presses queued meanwhile are dropped. If `scePadRead` errors, or stays silent while the pad changes, the app falls back
  to one `scePadReadState` snapshot per pass; `PAD_BUFFERED_READ` in `main.c` (1 by default) selects that mode outright, which is how
  other PS4 homebrew (Apollo, liborbis) reads the pad. Buffered reads are checked against the shadPS4 emulator only, not real hardware.
  (3) The loop reads the pad every 2 ms when idle and draws only when something changed or is moving; the network, file, lease and claim
  work keeps its 30 Hz cadence. (4) Scrolling glides (critically damped and time based, `include/scroll_ease.h`) instead of jumping; the
  focus brackets still jump. (5) Set up, Customization and Help paint their cards once into a premultiplied layer
  (`ui_canvas_create_layer`) and composite the visible slice (`ui_render_cached`), repainting only a card whose content changed
  (`ui_card_signature`). `tools/ui_scroll_check.sh` compares it with the direct render (within 2 levels, bar a few icon-edge pixels) and
  times both: a scrolled frame on Set up went from 32 ms to 7 ms at `-O2` on a PC. (6) A loop iteration over 50 ms logs one
  `[prof] slow iteration ...` line split into input, poll, preview, io, render and present, at most one per 250 ms (`source/frame_prof.c`,
  `tests/test_frame_prof.c`); a slow `io` figure would point at the file reads on `/data`. `APP_VERSION` is 1.02; the plugin's version is
  not affected.
- **A game keeps announcing that it holds the strip.** The optional external-source signal was refreshed only by the shell feed
  while it ran, so with the shell feed switched off during a game nothing renewed it and an outside controller could take the strip
  back and fight the game. The game now announces it every 5 s while it holds the strip, and at once when the shell feed's lease
  closes. A closed lease also reads as "shell feed not running" immediately, not after 3 s, and the shell feed decides about its own
  release before it closes the lease, counting lease claims as well. Checked on a PC only.
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
  ([launch-gap-test](../debugging/launch-gap-test.md)): with HDR on, 64 black frames over 2362 ms; with
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
  (`tools/handoff_capture.py`, [launch-gap-test](../debugging/launch-gap-test.md)) showed the plugin
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

