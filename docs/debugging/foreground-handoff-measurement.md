# Foreground handoff measurement test

This diagnostic build sends event-only `HANDOFF_DIAG` records over the existing development-PC UDP telemetry path to measure the launch-screen-to-game handoff. Plugin records use the `HOF1` prefix; shell-feed records use `SHL1`. These are not dependent on stdout/`printf` output. It does not change foreground ownership rules, polling intervals, color processing, or the sender gate. The extra output occurs only once per milestone (and on actual game foreground transitions), not per frame.

## Run one controlled capture

1. Use the normal merged foreground-gate setup and ensure the shell feed and game plugin are both enabled as in the session where the gap occurs.
2. In `/data/ps4-ambilight/ps4_ambient_light.ini`, set `[dev] dev_ip=<development-PC-IP>` and `dev_logging=true`; both are required for the UDP mirror. Start `tools/udp_ground_truth_listener.py` on that PC and allow UDP port 4048 through its firewall.
3. Start PuTTY logging before injecting/loading the payload if you also want the ordinary console log; the UDP listener is the authoritative channel for these new events.
4. With WLED showing its normal idle/default color, launch a game and do not manually toggle the feed or change the INI during the run.
5. Wait until the game has reached a stable playable screen, then leave it running for another 10 seconds.
6. Save the listener output as well as the complete PuTTY log. Do not trim startup records; the order of the first milestones matters.

## Lines to collect

- `SHELL_FIRST_VALID_STATE`: when the shell reader first successfully reads the shared state record, including the `fg` value.
- `FOREGROUND GATE ON/OFF`: when the shell-feed gate actually changes its decision.
- `SHELL_FIRST_READY_BUFFER`: first shell-side display buffer that passed the readiness guard.
- `SHELL_FIRST_DDP_RESULT`: first shell-side DDP submission; `bytes` should equal `expected` for a complete local UDP submission.
- `GAME_STATE_WRITE`: when the game plugin publishes its state record (`fg=1` means game foregrounded).
- `GAME_FG_POLL`: when the game sampler's system-status polling notices a foreground/background transition.
- `GAME_FIRST_VALID_FRAME`: first game-side buffer with a confirmed pixel format available for sampling.
- `GAME_FIRST_DDP_RESULT`: first game-side DDP submission; `bytes` should equal `expected` for a complete local UDP submission.

Each diagnostic record includes `tsc` and `hz`. The listener should display decoded `HOF1`/`SHL1` telemetry text; if you only see hex, the listener script may need updating. If the values are from the same monotonic counter (matching `hz`), subtract `tsc` values and convert to milliseconds: `(later_tsc - earlier_tsc) * 1000 / hz`. UDP `sendto()` success only proves local submission, not that WLED received or displayed the packet; the capture measures sender readiness and handoff ordering, not WLED's internal rendering.

## How to interpret the result

- If `GAME_STATE_WRITE fg=1` is early but `GAME_FIRST_VALID_FRAME` / `GAME_FIRST_DDP_RESULT` is much later, the delay is on the game hook/buffer/sampler side. Faster state polling will not fix it.
- If the game has already sent DDP but the shell continues sending for a while, inspect the interval from `GAME_STATE_WRITE fg=1` to `FOREGROUND GATE ON`; this isolates state-file publication/read polling delay.
- If the shell gate turns on before the game has submitted its first DDP packet, the handoff has a sender gap. The interval from `FOREGROUND GATE ON` to `GAME_FIRST_DDP_RESULT` estimates it.
- If `GAME_FIRST_DDP_RESULT` is early but WLED still displays its default color, the remaining gap is after local UDP submission (network delivery, DDP/WLED handling, or a competing sender); this log alone cannot distinguish those.
- If `SHELL_FIRST_READY_BUFFER` occurs late, the shell capture's own startup/buffer-discovery path is a factor.

This is a source diagnostic package, not a claimed PS4 SDK build. Compile with the same SDK/toolchain used for the normal merged source, then test on-console. Keep the original known-working build available for rollback.

## Follow-up

The launch-gap capture (`tools/handoff_capture.py`, [launch-gap-test](launch-gap-test.md)) builds on
these events and adds `LG_*` milestones for the stretch between plugin load and the first send. It
found a 3.16-3.17 s silence caused by `fg=1` being published at plugin load; `state.c` now waits for
the game's first DDP send. The event list above is unchanged, but `GAME_STATE_WRITE fg=1` now follows
`GAME_FIRST_DDP_RESULT` instead of preceding it.
