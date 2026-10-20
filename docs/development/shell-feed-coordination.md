# Coordinating with a shell-level feed

Since v3.9.2 the plugin writes `/data/ps4-ambilight/ps4_ambient_state`, a small file saying whether it's
actively driving the strip right now. This page is for anyone writing something that reads it --
today that's [`shell-feed/`](../../shell-feed/), a second plugin in this same repo that samples
the PS4's own Home screen, Settings and quick menu and sends those colors to the same WLED strip
when no game is doing it instead. (It started life as a separate research project,
`ps4-shell-capture-research`; see [`shell-feed/README.md`](../../shell-feed/README.md) for the
provenance of its current code and what's still unreplaced from that project.)

This plugin has no code that reads the file back, and no dependency on anything reading it. It
writes its own state because that's cheap and useful on its own; what another process does with
that is entirely its concern.

## Why this exists

Two different programs can both reasonably want to drive the same WLED strip: this plugin, while
a game, Netflix or YouTube is the foreground title, and a shell-level feed the rest of the time
(Home, Settings, the quick menu, anywhere there's no foreground title with its own buffers to
sample). Game always wins. Something has to say which situation it currently is, from the game plugin that owns its DDP
sender and knows its own foreground status. The signal is the explicit foreground bit, not output
progress or flip activity, with one launch-time exception: the plugin does not claim until its first
DDP send, because a title is "foreground" seconds before it can drive the strip (see
[launch-gap-test](../debugging/launch-gap-test.md)).

## The loopback lease

The primary hand-off channel. The file sections of this page below still apply as the **fallback**, used only when no lease
connection exists.

- The shell feed listens on `127.0.0.1:47200` (never `INADDR_ANY`: nothing on the LAN can take the strip) from the moment its send
  loop starts until it stops (disabled by the disable file or `[shell] enabled=false`), when it closes the listener and every connection.
- A game's plugin (from its state thread) and the companion app (while its Shell feed switch is on) connect and keep the connection.
  A claim is `C <title>\n`, a release is `R\n`. The shell feed never sends anything. Steady state is zero traffic.
- The strip is held while **any** connection holds a claim, so a game and the companion app can both be owners without a
  special case. A connection that closes releases its claim: when an owner is killed or crashes the kernel closes its sockets, which
  is what fixes the stale-`fg=1` case without a timeout.
- An owner that is only backgrounded (PS button) is still alive, so the plugin sends `R` itself when `g_isBackgrounded` flips, exactly
  where it used to write `fg=0`.
- A client with nothing to claim connects only when it wants to know whether a shell feed is running (the plugin during its startup
  window; the app while its Shell feed switch is on). Connected means running and enabled, so
  those callers read no heartbeat file. A refused loopback connect is instant, and clients retry at most every 500 ms.
- If the shell feed starts while a game is already in front, the game's plugin reconnects within 500 ms and re-sends its claim.
- If every one of the 8 slots holds a claiming owner a new connection is refused; otherwise the oldest idle connection is evicted.
- **Fallback:** if the listener cannot open, the shell feed reads the claim file and writes the heartbeat as before. The plugin
  and the app write the claim file only while they have no lease connection, and write it back to `fg=0` once one exists, so a file
  reader never sees a claim the lease already carries. The old stale-claim-at-load rule (`STALE_CLAIM`) applies only on this path.
- The FreeBSD values of the socket flags are defined in `lease.h` (`LEASE_MSG_DONTWAIT`, `LEASE_MSG_NOSIGNAL`, `LEASE_SO_NOSIGPIPE`):
  the OpenOrbis headers carry Linux's `MSG_*` numbers, which the console's kernel reads differently.

Files: `plugin/include/lease.h` (wire format, the shell feed's slot table and parser, the clients' state machine: pure logic, host-tested;
`companion-app/include/lease.h` is a byte-for-byte copy, checked by `tools/check_lease_copy.sh`), `plugin/source/lease.c` (the plugin's
sockets), `shell-feed/source/shell_lease.c` (the listener, run by the I/O worker in `shell_io.c`), `companion-app/source/shell_feed.c`
(the app's sockets). Log lines in the shell feed's log: `LEASE listening`, `LEASE claim|release|connect|released(EOF)`, `LEASE closed`;
in the plugin's `[dev]` channel: `HANDOFF_DIAG LEASE CONNECTED|LOST`.

## The file

`/data/ps4-ambilight/ps4_ambient_state`, exactly 35 bytes, one line:

```
v1 fg=1 seq=000123 title=CUSA00000
```

| Field | Meaning |
|---|---|
| `v1` | Format version. A reader that doesn't recognize it should treat the file as absent. |
| `fg` | `1` while this plugin is foregrounded **and** its first complete DDP send has happened (a one-time launch latch, never cleared); `0` while backgrounded or before that first send. After the latch, output activity and flip-hook freshness do not change it. |
| `seq` | Six digits, wraps at 1,000,000. Increments on each transition into `fg=1`. It is not rewritten while the state is unchanged, so a frozen `seq` is normal and is **not** a freshness signal. |
| `title` | Nine characters, space-padded, the same title ID the report file uses. `-` when unknown. |

## What a reader should do

- **Treat a missing, empty, or unparsable file as "no claim."** The plugin runs fine with
  nothing reading this file; a reader should run fine with the file not there yet, or ever.
- **Gate on the `fg` bit.** `fg=1` means back off, `fg=0` means you may send. Before any valid
  record has been read, you may send. After a valid record, a missing, unreadable or malformed read
  keeps the last known state; the shell feed does not fail open on a timeout and does not use `seq`
  movement as a freshness test. Poll at about 10 Hz (the shell feed does).
- **An explicit `fg=0` is the release signal.** The plugin writes it once on backgrounding. A
  stale readable `fg=1` after a plugin crash is a known limitation: this version has no unload or
  crash reset, and the reader deliberately does not guess from timing. The one exception is at load:
  an `fg=1` already in the file when the shell feed starts is treated as left over (a game can't be
  in front while the payload loads) and ignored until its `seq` changes or `fg=0` is read. A live
  game or the companion app is picked up at its next claim or heartbeat.
- **Read the whole 35 bytes in one call.** The file is written with a single `write()` of the
  whole fixed-width record at offset 0, deliberately without truncating first, so a reader can
  only ever see the old complete record or the new one -- never a partial write. A read of fewer
  than 35 bytes, or content that doesn't match the format, should also be treated as "no claim"
  for that poll rather than acted on.

## What this plugin does and doesn't promise

- `fg=0` is written at load. `fg=1` is written within about one 100 ms state tick of the first
  successful DDP send (so up to about 0.1 s after the first send, then up to one more reader poll
  before the shell backs off: a short overlap of up to about 0.2 s, measured at 47 ms and 94 ms), and
  `fg=0` is written once on backgrounding. There are no heartbeat writes to this file: it does not change
  between transitions (see `state.c`'s header comment for the exact logic).
- A title that never sends (unsupported format, unreadable buffers, hooks that never fire) stays
  `fg=0`, so a reader keeps its own output for it. Known and untested; see
  [shell-feed-foreground-gate](../shell-feed-foreground-gate.md#known-limitations-of-the-launch-latch).
- A game that was already sending when the reader started is seen as `fg=1`. A record left by a
  previous boot is also read as-is, so a reader that wants to ignore it needs its own rule; the shell
  feed doesn't, and the plugin rewrites the file at load.
- The format (`v1`) is considered stable. A future incompatible change would use a new version
  tag, not silently change the field layout under `v1`.
- This plugin makes no promise about *when* GoldHEN loads or unloads it relative to a title's own
  lifecycle -- the same uncertainty the report file already lives with (see
  [report-file.md](../debugging/report-file.md)).

## The shell feed's heartbeat file

The other direction, for one purpose: `/data/ps4_shell_alive` is a single line, a 20-digit
zero-padded counter and a newline, rewritten in place (no truncate) by the shell feed about once a second
from its worker. It sits directly in `/data`, **not** in the project folder: a file rewritten every second
stalled the other files in its own folder for seconds (see
[filesystem-stalls](../debugging/filesystem-stalls.md#root-cause)). The plugin never trusts its value, only
that it keeps changing: the state thread, from load until the plugin's first DDP send (or about one minute, if none ever comes), reads it about
every 300 ms (`ambient_shell_feed_expected()` in `settings.c`) and treats the shell feed as gone when it
has not changed for 3 s. That decides whether `network.c` holds its first send while the frames are black
(see [shell-feed-foreground-gate](../shell-feed-foreground-gate.md#startup-hold-on-black-frames)). A
missing file, an older shell feed without the heartbeat, a crashed one and an uninstalled one all read as
"not running". The companion app reads the same file to say the payload is already running.
A reader of `ps4_ambient_state` does not need this file.

## Source

- [`plugin/include/state_file.h`](../../plugin/include/state_file.h) -- the record format, pure
  string logic, host-testable.
- [`plugin/source/state.c`](../../plugin/source/state.c) -- the thread and the file I/O.
- `tools/test_state.c` -- the host test (a local file, not in the repo).

## The companion app writes it too

The companion app is not a game, but it drives the same strip: the live preview on Set up and
Customization, and Test strip. Without a claim the shell feed sent alongside it and WLED
alternated between the two (a flicker, seen on hardware). So while the Shell feed switch is on
and the app is sending, it writes the same record format with its own send lifecycle and
`title=COMPANION`: `fg=1` the moment it starts sending, a heartbeat once a second, and `fg=0`
when it stops, the switch is turned off, or the app quits. A crash leaves the last record, like a crashed game (the reader does not expire claims by timeout).
`seq` continues from whatever is already in the file, so the next record always differs from the
last one a reader saw. With the switch off the app never touches the file. The code is
`companion-app/include/claim_file.h`; `tests/test_claim_file.c` checks it against the plugin's
real `state_format()`.
