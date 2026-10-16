# Foreground handoff measurement test

This diagnostic build adds event-only `HANDOFF_DIAG` lines to measure the launch-screen-to-game handoff. It does not change foreground ownership rules, polling intervals, color processing, or the sender gate. The extra output occurs only once per milestone (and on actual game foreground transitions), not per frame.

## Run one controlled capture

1. Use the normal merged foreground-gate setup and ensure the shell feed and game plugin are both enabled as in the session where the gap occurs.
2. Start PuTTY logging before injecting/loading the payload so the first shell-feed lines are captured.
3. With WLED showing its normal idle/default color, launch a game and do not manually toggle the feed or change the INI during the run.
4. Wait until the game has reached a stable playable screen, then leave it running for another 10 seconds.
5. Save the complete PuTTY log. Do not trim the startup lines; the order of the first milestones matters.

## Lines to collect

- `SHELL_FIRST_VALID_STATE`: when the shell reader first successfully reads the shared state record, including the `fg` value.
- `FOREGROUND GATE ON/OFF`: when the shell-feed gate actually changes its decision.
- `SHELL_FIRST_READY_BUFFER`: first shell-side display buffer that passed the readiness guard.
- `SHELL_FIRST_DDP_RESULT`: first shell-side DDP submission; `bytes` should equal `expected` for a complete local UDP submission.
- `GAME_STATE_WRITE`: when the game plugin publishes its state record (`fg=1` means game foregrounded).
- `GAME_FG_POLL`: when the game sampler's system-status polling notices a foreground/background transition.
- `GAME_FIRST_VALID_FRAME`: first game-side buffer with a confirmed pixel format available for sampling.
- `GAME_FIRST_DDP_RESULT`: first game-side DDP submission; `bytes` should equal `expected` for a complete local UDP submission.

Each diagnostic line includes `tsc` and `hz`. If the values are from the same monotonic counter (matching `hz`), subtract `tsc` values and convert to milliseconds: `(later_tsc - earlier_tsc) * 1000 / hz`. UDP `sendto()` success only proves local submission, not that WLED received or displayed the packet; the capture measures sender readiness and handoff ordering, not WLED's internal rendering.

## How to interpret the result

- If `GAME_STATE_WRITE fg=1` is early but `GAME_FIRST_VALID_FRAME` / `GAME_FIRST_DDP_RESULT` is much later, the delay is on the game hook/buffer/sampler side. Faster state polling will not fix it.
- If the game has already sent DDP but the shell continues sending for a while, inspect the interval from `GAME_STATE_WRITE fg=1` to `FOREGROUND GATE ON`; this isolates state-file publication/read polling delay.
- If the shell gate turns on before the game has submitted its first DDP packet, the handoff has a sender gap. The interval from `FOREGROUND GATE ON` to `GAME_FIRST_DDP_RESULT` estimates it.
- If `GAME_FIRST_DDP_RESULT` is early but WLED still displays its default color, the remaining gap is after local UDP submission (network delivery, DDP/WLED handling, or a competing sender); this log alone cannot distinguish those.
- If `SHELL_FIRST_READY_BUFFER` occurs late, the shell capture's own startup/buffer-discovery path is a factor.

This is a source diagnostic package, not a claimed PS4 SDK build. Compile with the same SDK/toolchain used for the normal merged source, then test on-console. Keep the original known-working build available for rollback.
