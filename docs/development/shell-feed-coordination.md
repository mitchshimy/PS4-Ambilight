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
sender and knows its own foreground status. The claim must track successful output progress,
not flips alone: sampling/sending can continue from the selected buffer during a temporary
quiet period in the flip hooks.

## The file

`/data/ps4-ambilight/ps4_ambient_state`, exactly 35 bytes, one line:

```
v1 fg=1 seq=000123 title=CUSA00000
```

| Field | Meaning |
|---|---|
| `v1` | Format version. A reader that doesn't recognize it should treat the file as absent. |
| `fg` | `1` while this plugin is foregrounded and a successful WLED DDP send has been observed within the last 2.2 s; `0` otherwise. Flip-hook freshness alone does not grant or release ownership. |
| `seq` | Six digits, wraps at 1,000,000. Advances only while `fg=1`. A frozen `seq` while `fg=1` means the plugin stopped updating without writing `fg=0` -- a crash, not a clean handoff. |
| `title` | Nine characters, space-padded, the same title ID the report file uses. `-` when unknown. |

## What a reader should do

- **Treat a missing, empty, or unparsable file as "no claim."** The plugin runs fine with
  nothing reading this file; a reader should run fine with the file not there yet, or ever.
- **A claim is fresh if `fg=1` and `seq` has changed within your own staleness window** (the
  shell feed used 2.5 s against this plugin's roughly 1 Hz write rate -- comfortable margin,
  not a contract this plugin promises to keep exactly). Don't treat a single read as proof by
  itself; poll and compare `seq` across reads.
- **An explicit `fg=0` is a clean signal, not just an absence of freshness** -- the plugin writes
  it exactly once on the way out, so a reader doesn't have to wait out a staleness timeout for an
  ordinary game quit. The timeout is for the crash case, where no `fg=0` is ever written.
- **Read the whole 35 bytes in one call.** The file is written with a single `write()` of the
  whole fixed-width record at offset 0, deliberately without truncating first, so a reader can
  only ever see the old complete record or the new one -- never a partial write. A read of fewer
  than 35 bytes, or content that doesn't match the format, should also be treated as "no claim"
  for that poll rather than acted on.

## What this plugin does and doesn't promise

- `fg=1` is written within about 100 ms of the first successful DDP send observed by the state
  thread, then once a second while output activity continues; `fg=0` is written once on backgrounding
  or after 2.2 s without a successful send. The 2.2 s release window is shorter than shell-feed's
  2.5 s claim-staleness timeout. A reader that wants to back off
  without a visible overlap should poll every 50-100 ms; polling faster than that gains
  nothing. It is **not** a high-rate heartbeat: between the first write and the next
  heartbeat the file does not change (see `state.c`'s header comment for the exact logic).
- A game that was already sending when the reader started is seen as `fg=1` with a `seq` the
  reader has not seen change yet. A reader may wait for the next heartbeat (at most about a
  second) before trusting it; the first read after the reader's own start is also where a
  record left by a previous boot shows up, which is why the freshness rule above compares
  `seq` across reads.
- The format (`v1`) is considered stable. A future incompatible change would use a new version
  tag, not silently change the field layout under `v1`.
- This plugin makes no promise about *when* GoldHEN loads or unloads it relative to a title's own
  lifecycle -- the same uncertainty the report file already lives with (see
  [report-file.md](../debugging/report-file.md)).

## Source

- [`plugin/include/state_file.h`](../../plugin/include/state_file.h) -- the record format, pure
  string logic, host-testable.
- [`plugin/source/state.c`](../../plugin/source/state.c) -- the thread and the file I/O.
- [`tools/test_state.c`](../../tools/test_state.c) -- the host test.

## The companion app writes it too

The companion app is not a game, but it drives the same strip: the live preview on Set up and
Customization, and Test strip. Without a claim the shell feed sent alongside it and WLED
alternated between the two (a flicker, seen on hardware). So while the Shell feed switch is on
and the app is sending, it writes the same record format with its own send lifecycle and
`title=COMPANION`: `fg=1` the moment it starts sending, a heartbeat once a second, and `fg=0`
when it stops, the switch is turned off, or the app quits. A crash expires like a crashed game.
`seq` continues from whatever is already in the file, so the next record always differs from the
last one a reader saw. With the switch off the app never touches the file. The code is
`companion-app/include/claim_file.h`; `tests/test_claim_file.c` checks it against the plugin's
real `state_format()`.
