# Launch-gap test

**Symptom:** after a game's launch picture appears and before its flip hooks start producing frames,
nothing is sent to WLED, so the strip drops to WLED's own default color.

This is a separate, isolated capture so the answer isn't buried in the other tools' telemetry. It
extends the [foreground handoff measurement](foreground-handoff-measurement.md) events with the
milestones that sit *between* plugin load and the first send, and adds one tool that keeps only those
events and reads them off a single clock (the PC's).

## What was added

Plugin, all event-only HOF1 records over the existing `[dev] dev_ip` + `dev_logging=true` channel, each
sent once per process from the sampler thread (the hooks stay record-only):

| Event | Meaning |
|---|---|
| `LG_SAMPLER_START` | the sampler thread began (plugin is live in the game process) |
| `LG_FIRST_REGISTER_HOOK` | first `sceVideoOutRegisterBuffers` call seen, with the format it registered |
| `LG_FORMAT_CONFIRMED` | `g_haveValidFormat` first became true |
| `LG_FIRST_FLIP_HOOK` | first flip hook call seen (which of the three entry points, and the slot) |
| `LG_LAG_HISTORY_READY` / `LG_LAG_WAIT_EXPIRED` | the sample-lag history existed / the ~1 s wait ran out |
| `LG_SEND_GAP ms=N` | any silence of 250 ms or more between two game-feed sends (heartbeat resends count as sends) |

They are not `__FINAL__`-gated, so a **release build** carries them. Use a release build for this test:
a `DEBUG=1` build also streams FLK1/GRDC/... every second, which the tool ignores but you don't need.

Milestones are "first seen by the sampler", so resolution is one pass (about 33 ms).

## Run it

1. `[dev] dev_ip=<PC IP>` and `dev_logging=true` in `/data/ps4-ambilight/ps4_ambient_light.ini`; allow UDP
   4048 through the PC firewall. Shell feed and game plugin both enabled, as in the failing session.
2. Close `flicker_capture.py` and `udp_ground_truth_listener.py` (same port).
3. On the PC: `python tools/handoff_capture.py capture --out run1.jsonl --wled-ip <WLED IP>`
   (`--wled-ip` is optional; it polls `/json/info` at 10 Hz so the visible fallback is timed too).
4. With WLED on its idle color, launch the game, wait for stable play, give it 10 s, Ctrl+C.
5. Read the printed verdict, or later: `python tools/handoff_capture.py analyze run1.jsonl`.

The shell feed's `SHELL_FIRST_*` events fire once per shell-feed session, so for a second launch in the
same capture only the gate and `LG_*`/`GAME_*` events repeat. That is enough for the verdict; restart the
shell feed between runs if you want the shell milestones every time. A capture with several launches is
split per `LG_SAMPLER_START` and each run is judged on its own.

## Reading the result

The tool takes the silence as starting when the shell feed backs off (`FOREGROUND GATE ON`) and ending at
`GAME_FIRST_DDP_RESULT`, then lists every milestone in between and names the largest step.

| Verdict | Meaning |
|---|---|
| `NO SENDER GAP` | the game's first DDP followed the shell backing off within 100 ms, or came first and the shell backed off up to 0.25 s later (the expected handoff since the launch latch) |
| `SENDER GAP` | real silence; the largest step says which stage the game was waiting on |
| `OVERLAP` | the game was already sending more than 0.25 s before the shell backed off (longer than the state tick plus shell poll explain) |
| `INCOMPLETE` | a needed event never arrived; UDP is lossy, so repeat before concluding |

What the largest step points at: `LG_FIRST_FLIP_HOOK` = the game hadn't flipped through a hooked path yet;
`LG_LAG_HISTORY_READY`/`LG_LAG_WAIT_EXPIRED` = the lag-2 history wait in `sample_thread.c`;
`LG_FORMAT_CONFIRMED` = buffers registered but format unknown; `GAME_FIRST_VALID_FRAME` and later = the
sampler pipeline itself.

## What it found (2026-10-16)

Three captures before any change, one title, all the same shape:

| | Run 1 | Run 2 | Run 3 |
|---|---|---|---|
| `GAME_STATE_WRITE fg=1` | +0.00 s | +0.00 s | +0.00 s |
| Shell backs off (gate ON) | +0.03 s | +0.03 s | +0.09 s |
| Buffers registered, format OK | +0.06 s | +0.06 s | +0.09 s |
| **First flip hook** | **+3.13 s** | **+3.13 s** | **+3.22 s** |
| First game DDP | +3.19 s | +3.20 s | +3.27 s |
| **Silent window** | **3.16 s** | **3.17 s** | **3.17 s** |

`g_isBackgrounded` starts false, so `state.c` published `fg=1` within one 100 ms tick of load, before
the title had flipped anything; the shell feed backed off on it, and the game could not send until
its first flip hook about 3.1 s later. The lag-history wait and the sampler pipeline were not the
cause (under 0.1 s after the first flip), and the v2.8 heartbeat could not help because it does nothing
until something has been sent. The game's own first flip is what takes about 3 s; the fault was
claiming the strip before then.

## The fix and what changed

`state.c` now holds `fg=1` back until the process's first complete DDP send (a one-time latch; see
[shell-feed-foreground-gate](../shell-feed-foreground-gate.md)). Two captures afterwards:

| | Run 4 | Run 5 |
|---|---|---|
| First game DDP | +3.23 s | +3.25 s |
| `GAME_STATE_WRITE fg=1` | +3.25 s | +3.34 s |
| Gate ON | +3.28 s | +3.34 s |
| Verdict | no sender gap | no sender gap |

The game still takes about 3.1 s to flip, but the shell feed now keeps the strip until the game
sends, and the handoff overlap was 47 ms and 94 ms. The user then reported the WLED fallback color no
longer appearing on 4 games. That is the evidence for the visible result: the WLED live flag
(`--wled-ip`) stayed live in every run, before and after, with 1-7 poll errors per run, so it never
detected the fallback and shouldn't be read as proof either way.

Known limitations of the fix (unsupported titles stay with the shell feed; a short handoff overlap) are
listed in [shell-feed-foreground-gate](../shell-feed-foreground-gate.md#known-limitations-of-the-launch-latch).

Not covered: a wider title sweep, the NFS Heat burnout case re-run against this change, a title that
never sends on a console (the host test covers it), and PS-button suspend/resume on a console.

## Checks

`python tools/handoff_capture.py selftest` parses the packet shapes (including the shell's no-space
`SHL1` form) and runs the verdict logic on synthetic scenarios: flip-wait gap, lag-wait gap, no gap,
the latch handoff (shell 150 ms late), overlap, incomplete, two launches in one capture, and a WLED
fallback interval. No console needed.
The state writer's latch has its own host test, `tools/test_state.c` (see [tools](../development/tools.md#test_statec)).
The console evidence is the captures above; no PS4 SDK build was done in the environment that wrote this.
