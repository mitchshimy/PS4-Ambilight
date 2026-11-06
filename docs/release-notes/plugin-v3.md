# Plugin v3.x release notes

Full release notes, moved out of the [changelog](../../CHANGELOG.md), which now keeps one line per change.

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
  moment it isn't. See [shell-feed-coordination](../development/shell-feed-coordination.md).
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
  is off by default**: turn it on by adding `report_file=1` under `[compat]` in
  `/data/ps4_ambient_light.ini`. With it on, the release build writes `/data/ps4_ambient_report_<TITLEID>.txt`, a short text file with the
  plugin version and build, the title ID, how far it got (`hooked`, `registered`, `flipping`
  or `running`), the registered format and whether it was recognized, the flip counts for all
  three entry points, the guard result (rejects, last return, last bad address), the remap
  result (created, failed, method, last return) and the last pass time. Attach that file.
  It holds no personal data: no WLED address and nothing from the ini. Everything in it was
  already kept in plain globals in every build; only the packets that send them are debug-only.
  [report-file](../debugging/report-file.md) explains each line.
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
- Added `.github/ISSUE_TEMPLATE/title-report.yml`, so a report can say works, dark strip or
  crashed and attach the file without editing any markdown. Results worth keeping get copied
  into [title-compatibility](../debugging/title-compatibility.md) by hand.
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
  [gpu-only-buffers](../debugging/gpu-only-buffers.md).

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
  [`docs/debugging/sample-lag-and-boot-flash.md`](../debugging/sample-lag-and-boot-flash.md).
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
  Write-up: [`docs/debugging/hdr2200-alpha-detection.md`](../debugging/hdr2200-alpha-detection.md).
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
  [`docs/debugging/youtube-hdr-8bit.md`](../debugging/youtube-hdr-8bit.md).
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
  [`docs/debugging/letterbox-placement.md`](../debugging/letterbox-placement.md).
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
  [`docs/debugging/framebuffer-flicker.md`](../debugging/framebuffer-flicker.md),
  with the details of the change and what's still untested in
  [`buffer-selection.md`](../debugging/buffer-selection.md).
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
  [`docs/debugging/videoout-hooks.md`](../debugging/videoout-hooks.md).
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

