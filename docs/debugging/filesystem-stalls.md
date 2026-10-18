# Filesystem stalls and the WLED timeout

## What was seen

Two captures of NFS Heat (`flicker-session.flk`, `flicker-session1.flk`, 2026-10-18) showed WLED dropping to its own
color for a few seconds, then recovering. Not a flicker and not a handoff problem. Four stalls in about two and a half
minutes of capture, 0.8 s to 7.2 s each. The ones over WLED's realtime timeout (about 2.5 s by default) are the visible ones.

How the capture shows it:

- The plugin logged `LG_SEND_GAP ms=7206 bg=0`, and the console clock between two consecutive probe packets jumped by the
  same 7.206 s, so the console really stopped (it is not packet loss). In `flicker-session.flk` the stall fell inside a
  pass instead: that pass's start stamp is only 38 ms after the previous one, but its send came 7.2 s later, while the game
  kept flipping at 60 fps (432 flips in the gap).
- The 2 s config-reload diagnostic packet arrives every 2.00-2.04 s. Each stall starts on the pass where the next one was due
  and that packet is the first thing out when the stall ends: the sampler was blocked inside the ini read.
- The shell feed froze 1-2 s before the game did and both resumed in the same instant. One shared blocked resource, most
  likely the filesystem (the shell feed opens a file every 30 Hz tick, the game only every 2 s, so it hit the block first).
- After a stall the shell feed replayed its missed ticks (224 ticks in 1.5 s, about 5x the real rate).

The cause of the filesystem stall itself is not known. Candidates: the game's own heavy disk loading queuing ahead of our
small `/data` reads on a hard drive, or a SceShellCore background job. The PuTTY logs have no timestamps, so they cannot
settle it.

## What changed

Neither process now touches the filesystem on the thread that sends frames.

- **Plugin:** a watcher thread (`ambient_config_thread`) reads and hashes the ini every `config_reload_check_seconds`.
  A changed ini is parked as an in-memory copy; the sampler applies it with `ambient_apply_pending_config()`, which parses
  the copy (`ini_table_read_from_buffer`) and opens no file. If the thread cannot start, the sampler checks inline as before.
- **Shell feed:** an I/O worker (`shell_io.c`) does the disable-file check, the `[shell] enabled` poll, the heartbeat write,
  the claim-state read (twice the old 10 Hz, to keep the handoff latency) and the ini watch. The send loop only reads what it
  publishes. The heartbeat file carries the send loop's tick count, so it still stops changing if the loop itself stalls.
  If the worker cannot start, the loop polls inline as before.
- **Scheduler:** when the loop is more than two frames late it skips the missed ticks (`SCHED_RESYNC late_ms=`) instead of
  replaying them.
- **Slow-operation log:** any file operation on a worker that takes 100 ms or more logs
  `HANDOFF_DIAG FS_SLOW op=<name> ms=<n>` when it returns. Ops: `cfg_read`, `state_read`, `disable_file`, `enabled_poll`,
  `heartbeat`. It goes over the opt-in `[dev]` channel like the other HOF1 lines.

## Fewer file operations

A third capture (`flicker-session2.flk`, 5.2 minutes, no color fallback) still had 59 slow file operations, up to 17.7 s, in both
processes at once. They can't be stopped from here, so the shell feed and plugin now touch the disk less:

- `config_reload_check_seconds` defaults to 5 (was 2). An existing ini keeps its own value.
- The shell feed keeps the claim-state file open and re-reads it from offset 0, reopening it every 2 s (it was 20 opens a second).
- The shell heartbeat keeps its file open and rewrites it in place, reopened every 10 s.
- The plugin now times its own state write (`op=state_write`) and its shell-alive checks (`op=shell_alive_checks`) as `FS_SLOW`, so a
  late hand-off during a stall shows up in the log.

`[shell] enabled=` is still polled on its own 1 s cadence.

## Root cause

Phased captures separated the shell feed's disk work (`session5` to `session8`, 2026-10-18):

- With the shell feed running, 14 to 24% of the time was stalled in NFS Heat burnouts, in both processes at once. With the
  shell feed absent (`session4`), or with only its heartbeat write switched off, there were no stalls in over 25 minutes
  across four separate phases. The claim-state read, the directory and ini opens, the disable file and the `enabled` poll made
  no difference when switched off.
- The heartbeat period made no difference either: 1 s, 3 s and 10 s all stalled at about 2 per minute, the console's roughly 30 s
  flush cadence. Only no writes at all removed the stalls.
- The 3 minutes of driving before the burnouts had no stalls with the heartbeat at 1 s, so it takes the heartbeat writes plus a
  busy game. Why a 21-byte rewrite blocks the rest of `/data` is not known; the best fit is write-back of a continually
  dirtied file on that cycle.

The fix is to write no heartbeat while a game holds the strip (`shell_io.c`). The companion app's claim (`title=COMPANION`) is not a game, so the heartbeat keeps
running for it: the app reads that file to say the payload is already running. The plugin keeps its last "shell feed alive" answer
while it holds the strip and re-arms the check when it backgrounds (`state.c`, `ambient_shell_feed_rearm()` in `settings.c`).
With it, `session8` had 0 stalls in the burnouts (4.7 s stalled in 498 s, all of it at 38 s and just after a trip to Home,
where the heartbeat had resumed).

## What it does not fix

A stalled filesystem can still delay a config edit, the foreground bit, or the heartbeat, but not the frames. The state
thread (`state.c`) and the report thread still do file I/O on their own threads, which is intended.

## Reading the next capture

`FS_SLOW` lines name which call was stuck and for how long. A stall with no `FS_SLOW` and a `LG_SEND_GAP` means the sampler
stopped for some other reason. `SCHED_RESYNC` says the shell feed's loop itself was delayed.
