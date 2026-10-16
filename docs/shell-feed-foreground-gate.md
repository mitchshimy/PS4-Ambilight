# Shell-feed foreground-bit gate

**Status:** implemented in source; console result is preliminary and still being observed.

The shell-feed loop reads `/data/ps4-ambilight/ps4_ambient_state` and uses the explicit `fg` bit as its gate. It no longer uses `seq` movement or a freshness deadline to infer ownership:

- A valid `fg=1` record makes the shell feed back off.
- A valid `fg=0` record allows the shell feed to send again.
- Before any valid record has been read, the shell feed is allowed to send.
- After a valid record has been read, a missing, unreadable, or malformed record preserves the last known `fg` state. It does not fail open after a timeout.
- File reads are rate-limited to 10 Hz; the live-send loop otherwise checks the cached gate state.

The integrated change updates both sides of the contract:

- `plugin/source/state.c` writes the initial state and subsequent transitions based only on `g_isBackgrounded`: `fg=1` while the title is foregrounded, `fg=0` while backgrounded. It does not use successful DDP sends, output counters, flip counters, or sampler activity to decide ownership. Writes stay on the separate 100 ms state thread, not the sampler hot path; there are no heartbeat writes.
- `shell-feed/source/main.c` reads the explicit bit and applies the gate rules above.
- `plugin/include/state_file.h` documents the new meaning of `fg` and the transition-based sequence counter.

A stale readable `fg=1` after a game-plugin crash remains a recovery limitation: this version does not yet add an unload/crash reset path, and the reader deliberately does not use sequence freshness as an ownership signal.

## User test note — 2026-10-16

The user reports performing burnouts for some time with **no flickering observed**. This is encouraging but not conclusive: testing is continuing, and the observation does not yet establish that the issue is fixed. Host-side state-writer and shell-feed checks were run for this integrated source update. No PS4 SDK build or new console validation was performed as part of packaging this update.
