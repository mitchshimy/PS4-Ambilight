# "Black screen shows color" (v2.7.3 to v2.7.6)

Report: on black screens, especially loading screens, the strip kept showing a
faint color instead of going dark. Three things got changed while chasing it.
Only one of them was the cause, and one of the others had to be reverted.

## What the earlier investigation had assumed

A lot of time had already gone into the pixel format and decode path (see
[hdr-pixel-format](hdr-pixel-format.md)), on the theory that a wrong decode was
turning black into a color. That was reasonable, since a format problem really did
exist on one title. It just wasn't this bug.

## Attempt 1: stale-flip guard (v2.7.3, reverted in v2.7.6)

The idea was that most engines stop flipping during a loading screen and are free
to reuse that memory, so the plugin might be sampling a buffer nobody was updating
anymore. The guard blanked the strip if no flip had landed in about 350 ms.

The commit is upfront that this wasn't confirmed as the cause, only "a real gap
worth closing". It made things worse. Menus and static loading screens throttle or
stop flipping while idle, and the gaps kept drifting back and forth across the 350
ms line: blank, resume, blank, resume, which shows up as flicker on anything that
sits on screen for a while.

Reverted in `80c2873`, confirmed fixed on hardware. The lesson noted in the commit
is to not chase a threshold that just moves the problem, especially when the guard
wasn't protecting against anything that had actually been seen to happen.

(The v3.3 sampler change looks superficially similar, but it isn't a timeout. It
picks a different slot rather than deciding whether to send. See
[buffer-selection](buffer-selection.md).)

## Attempt 2: duplicate corner LEDs (v2.7.4)

Found while looking at raw reads: zone 0 and the last zone were sending
bit-identical pixel reads on every frame. That fix was real but unrelated to the
black-screen symptom. Details in [sampling-zones](../sampling-zones.md).

## The actual cause: smoothing that stops short of black (v2.7.5)

The smoothing step moves each channel toward its target by a proportion of the
difference, in integer math. Integer division truncates toward zero, so once a
channel had decayed to about 3 the per-frame step rounded down to 0 and it just
stopped. Not for a frame, permanently. Any bright scene decayed to a faint residual
and stayed there for as long as the screen stayed black.

Fix: force a minimum step of 1 toward the target whenever the proportional step
would round to 0, so it always finishes the job. Tested three times on hardware,
consistent. This is why it looked like a "black screen shows color" bug and why it
showed up most on loading screens: a bright scene fading to a long black screen is
exactly when the residual is visible.

## What to take from it

- A decode bug and a smoothing bug can produce the same symptom. Check what is
  actually being sent (raw read, then output) before assuming the decode is wrong.
  The `[dev]` telemetry exists for this, see
  [framebuffer-flicker](framebuffer-flicker.md#methodology).
- Integer smoothing needs an explicit minimum step, or it can never reach its
  target from above.
- A guard added as a hunch, with no confirmed failure behind it, can cost more than
  it saves.
