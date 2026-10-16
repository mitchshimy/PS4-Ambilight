# Shell-feed foreground-bit gate

**Status:** implemented in source; console result is preliminary and still being observed.

The shell-feed loop reads `/data/ps4-ambilight/ps4_ambient_state` and uses the explicit `fg` bit as its gate. It no longer uses `seq` movement or a freshness deadline to infer ownership:

- A valid `fg=1` record makes the shell feed back off.
- A valid `fg=0` record allows the shell feed to send again.
- Before any valid record has been read, the shell feed is allowed to send.
- After a valid record has been read, a missing, unreadable, or malformed record preserves the last known `fg` state. It does not fail open after a timeout.
- File reads are rate-limited to 10 Hz; the live-send loop otherwise checks the cached gate state.

The code change is limited to `shell-feed/source/main.c`. The game-side writer was not changed in this update. In particular, the writer's current definition of `fg=1` still depends on its existing foreground/output-activity logic, so this reader change alone does not make the producer's flag a pure foreground indicator. A stale readable `fg=1` after a game-plugin crash also remains a recovery limitation until an explicit reset/unload path exists.

## User test note — 2026-10-16

The user reports performing burnouts for some time with **no flickering observed**. This is encouraging but not conclusive: testing is continuing, and the observation does not yet establish that the issue is fixed. No PS4 SDK build or console validation was performed as part of this source update.
