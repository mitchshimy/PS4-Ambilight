# WLED heartbeat (v2.8)

The plugin resends the last color it sent once a second during any foreground gap
where it has no valid frame. This is the write-up of why, and how it works. It
replaces the working spec that used to live in `docs/`.

## Problem

The strip occasionally flashed an unrelated color. It was root-caused in v2.2.3 to
WLED's own realtime-timeout fallback color firing when packets stop arriving, not
to a decode bug. The v2.2.3 performance pass (a PQ tone-map LUT) made the gap less
frequent by speeding up the pipeline, but didn't remove it. `sample_thread.c` still
had a real silent path:

```c
// If g_haveValidFormat is 0 (unknown/unconfirmed format), we
// deliberately send nothing rather than guess...
```

Any stretch where the pixel format is unknown or changing (loading screens, format
changes) is a real zero-packet window. If it outlasts WLED's realtime timeout, the
strip drops to WLED's fallback color and then snaps back. That is the flash.

## Goal

While the plugin is in the foreground, WLED should get a packet at least once
every ~1 s, even when the color pipeline has nothing new.

The heartbeat never invents a color. It only repeats the last one the real
pipeline already sent.

## Not a goal: no heartbeat while backgrounded

Nothing is sent while `g_isBackgrounded` is set. That silence is deliberate (v2.4):
it avoids a frozen strip on suspend and hands control over to `wled-relay`.
Heartbeating there would bring back the bug that fix removed.

## How it works

All in `plugin/source/network.c`, called from `sample_thread.c`.

- `wled_send_rgb_zones()` keeps a copy of what it last sent (`g_lastSentRgb`,
  `g_lastSentNumZones`, `g_haveLastSent`) and stamps `g_lastSendTicks` on every call.
  That includes heartbeat resends, so the timer always counts from the last real
  packet.
- `wled_send_keepalive_if_stale(nowTicks, tscFreq)` returns early if nothing has
  been sent yet this session, or if less than `WLED_HEARTBEAT_INTERVAL_US` (1 s) has
  passed since the last packet. Otherwise it resends the stored colors.
- `sample_thread.c` calls it only on the no-valid-frame paths: `liveBufferAddr == 0`
  or no valid format, and the case where the unpack function goes NULL partway
  through. The `g_isBackgrounded` branch `continue`s the loop before either, so the
  suspend case never reaches it.
- There's no ini key for it. It is a protocol detail, not something to tune.

The sampler is the only caller of `wled_send_rgb_zones()`, so none of this needed
locking. In the normal valid-frame path the cost is unchanged, since the new work
only happens in the already-abnormal branch.

## How it was checked

With `tools/udp_ground_truth_listener.py`:

- Forced a long unknown-format window (temporarily returning NULL from
  `getUnpackFnForFormat` in a debug build) for over 2 s and confirmed a packet still
  arrived at least once a second.
- Confirmed no packet arrives during a real PS-button suspend. That is the
  regression to watch for.
- On real hardware, triggered a game's loading-screen transition, a known
  format-uncertainty trigger, and saw no flash to WLED's fallback color.
- Checked CPU and network cost in the normal path was unchanged.

The listener and the flicker capture tool both use port 4048, so don't run them
together.
