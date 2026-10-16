# Shell-feed foreground-bit gate

**Status:** implemented in source; console result is preliminary and still being observed.

The shell-feed loop reads `/data/ps4-ambilight/ps4_ambient_state` and uses the explicit `fg` bit as its gate. It no longer uses `seq` movement or a freshness deadline to infer ownership:

- A valid `fg=1` record makes the shell feed back off.
- A valid `fg=0` record allows the shell feed to send again.
- Before any valid record has been read, the shell feed is allowed to send.
- After a valid record has been read, a missing, unreadable, or malformed record preserves the last known `fg` state. It does not fail open after a timeout.
- File reads are rate-limited to 10 Hz; the live-send loop otherwise checks the cached gate state.

The integrated change updates both sides of the contract:

- `plugin/source/state.c` writes the initial state and subsequent transitions based on `g_isBackgrounded`: `fg=1` while the title is foregrounded, `fg=0` while backgrounded. The one exception is a launch-only latch: `fg=1` is withheld until the process's first complete DDP send (`g_wledOutputCount` first non-zero), so the shell feed keeps the strip through the launch picture. The latch is set once and never cleared; after it nothing uses successful sends, output counters, flip counters or sampler activity to decide ownership. A title that never sends stays `fg=0`. Writes stay on the separate 100 ms state thread, not the sampler hot path; there are no heartbeat writes to this file. (The shell feed's own `ps4_shell_alive` heartbeat is a different file; see the startup hold below.)
- `shell-feed/source/main.c` reads the explicit bit and applies the gate rules above.
- `plugin/include/state_file.h` documents the new meaning of `fg` and the transition-based sequence counter.

A stale readable `fg=1` after a game-plugin crash remains a recovery limitation: this version does not yet add an unload/crash reset path, and the reader deliberately does not use sequence freshness as an ownership signal.

## User test note — 2026-10-16

The user reports performing burnouts for some time with **no flickering observed**. This is encouraging but not conclusive: testing is continuing, and the observation does not yet establish that the issue is fixed. Host-side state-writer and shell-feed checks were run for this integrated source update. No PS4 SDK build or new console validation was performed as part of packaging this update.

## Launch latch — 2026-10-16

Three `tools/handoff_capture.py` captures (`docs/debugging/launch-gap-test.md`) showed `fg=1` written at plugin load, the shell backing off within 30-90 ms, and the game's first flip hook (so first DDP) arriving about 3.1 s later: a 3.16-3.17 s sender gap in all three runs. The latch above is the change made for that. Expected after the change: `GAME_STATE_WRITE fg=1` follows `GAME_FIRST_DDP_RESULT` by up to one state tick, and the shell's gate ON follows by up to one more poll, so a short (about 0.2 s) two-sender overlap replaces the gap. Measured on a console afterwards: two captures show no sender gap (the shell backed off 47 ms and 94 ms after the game's first send), and the user reports the fallback color gone on 4 games. See [launch-gap-test](debugging/launch-gap-test.md). `tools/test_state.c` covers the state logic; the burnout case and a wider title sweep are still to be re-checked.

## Startup hold on black frames

The launch latch waits for the first *send*, but in FC26 that first send was a black frame: the game's
registered buffers read all zeros for about 3 s while the launch picture is on screen, so the shell
feed backed off and the strip went dark (see [launch-gap-test](debugging/launch-gap-test.md#second-finding-black-frames-not-silence-fc26-2026-10-16)).
`wled_send_rgb_zones()` in `network.c` now sends nothing until a frame's brightest channel is above 2,
so `fg` stays 0 and the shell feed keeps the strip. It is a one-way latch: once a frame has passed, dark
scenes are sent normally.

The hold only makes sense while the shell feed is there to cover. With it off, holding would leave WLED on
its default color through a black loading screen. So the shell feed rewrites
`/data/ps4-ambilight/ps4_shell_alive` (a changing 20-digit counter and newline) about once a second, and
the plugin's state thread, from load until the first send, checks it about every 300 ms through
`ambient_shell_feed_expected()` in `settings.c`. It reports the shell feed gone when the counter has not
changed for 3 s, or at once when the disable file or `[shell] enabled=false` is present, and publishes the
answer as `g_shellFeedAlive`; with it false, the plugin sends from the first frame as it did before the
hold. A missing file, an older shell feed without the heartbeat, a crashed shell feed and one that was
never installed all read the same way. After the first send nothing is read.

## Known limitations of the launch latch

- **Unsupported titles leave the shell feed in control.** `fg=1` now waits for the game's first DDP
  send, so a title that never sends (a pixel format the plugin doesn't know, a display buffer it can't
  read, flip hooks that never fire) stays `fg=0` and the shell feed keeps the strip. Before the latch,
  such a title published `fg=1` at load and the shell feed backed off. Not observed on a console: no
  unsupported title was available. Planned, untested: also publish `fg=1` when the plugin concludes the
  title is unusable, using the verdict `notices.c` already reaches after its ~5 s dwell. A plain timeout
  was rejected because a supported title with a long splash would hit it and the launch gap would come
  back (the first flip took about 3.1 s in the captured title; others may be slower).
- **Short two-sender overlap at handoff.** The state tick and the shell's poll are both 100 ms, so
  the shell feed can keep sending for up to about 0.2 s after the game's first send (47 ms and 94 ms
  measured). The only flicker seen in testing coincided with the plugin's notification toast; whether
  the toast or the overlap causes it has not been separated (a run with `[notifications] enabled=false`
  would). Halving both intervals would roughly halve the overlap at the price of twice the state-file
  reads in the shell feed; deliberately not done.
- A stale readable `fg=1` after a plugin crash (above) is unchanged.
- **Startup hold:** for the first 3 s after load the plugin assumes the shell feed is running, so a
  black loading screen with no shell feed could show WLED's default color if the first frame arrives sooner
  than 3 s after load (it arrived 3.4-4.9 s after load in the captures). A title whose buffers
  stay black for a long time while the shell feed runs leaves the shell feed in control; there is no
  timeout. The shell-off path has no capture of its own, and the heartbeat reader has no host test.
