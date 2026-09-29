# Letterbox detection (v2.9, v3.0, v3.3)

How the plugin finds black bars, why the first version was replaced, and what is
still open.

## Why it exists

Console output is native 16:9, so the only bars are cutscenes mastered at a
narrower ratio: 1.85:1 is about 21 px per edge and 2.39:1 is about 138 px on a
1080p frame. If the zones sit on those bars the strip goes dark on cutscenes. A
fixed margin can't work because it is either wrong during the letterboxed scene or
wrong for the full-screen gameplay around it, which is almost everything.

## v2.9: first version

Ported from an Android reference project's `BorderProcessor`. Each edge is probed
independently at 3 points (25, 50 and 75% along the perpendicular axis) and scanned
inward for the first row or column that isn't black. Independent edges mean an
asymmetric bar, like a status bar on the top only, is handled instead of averaged
away.

A new border only commits after `auto_letterbox_stability_frames` consecutive
detections agree, so a one-frame flicker doesn't snap the LED geometry. New keys:
`auto_letterbox_enabled` (off by default at this point), `auto_letterbox_threshold`,
`auto_letterbox_stability_frames`, `auto_letterbox_check_interval_frames`. It sat
alongside the manual `capture_margin_*` keys.

## v3.0: manual margins removed

Auto detection could fully cover the manual margins, so `capture_margin_*` were
deleted and `auto_letterbox_enabled` now defaults to `true`. It is the only capture
inset the plugin applies.

This was a breaking ini change. Old values are ignored, not migrated. Anyone using
the margins to trim display overscan, rather than a real letterbox, has no
equivalent setting now.

## v3.3: the dark screen delay

Symptom: on a mostly black frame with a small bright element in the middle (a
loading screen with centered text), the LEDs reacted about a second late when
something flashed in from the real screen edge.

Cause: with only 3 fixed probe points per line, a dark frame had all four edges
scan nearly to the search limit before finding the text. The plugin then committed a
huge border around the text, `buildZoneGeometry()` pulled the edge zones in to that
margin, and nothing sampled the real edge. The LEDs only reacted once the new
content had travelled into the shrunken area. It only happened on dark scenes,
because bright frames hit non-black on the first row and never got that far.

Changes:

- The scan now sweeps `EDGE_NUM_SAMPLES` (10) points across the whole row or column
  and treats a line as bar only if 85% of them are black (`isRowBlack`,
  `isColBlack`). A few dozen pixels of text flip a couple of samples, so the scan
  walks past it to the real edge. This matches `BorderProcessor.findBorderRgb` in the
  Android project, which is the version every real capture path there uses. The
  3-point version this was originally ported from (`findBorderRgba`) turned out to
  only be exercised by that project's unit tests.
- Search depth is capped at `MAX_BAR_DEPTH_V` and `MAX_BAR_DEPTH_H`, 1/6 of the
  screen (180 px and 320 px), instead of half. It also caps how far a misdetection
  could ever inset the zones. Worst case probe count is about the same as before
  (roughly 10k versus 9k) despite the extra samples per line.

## Threshold direction

A pixel counts as black if every channel is below the threshold. So if bars aren't
pure black, for example they sit at 25, the threshold has to go up above 25, not
down. The companion app's Help card and the README troubleshooting entry both said
the opposite for a while. Both are fixed. In the app it appears as **Bar threshold**
and is shown as a percent.

## Why stability and recheck are hidden in the app

`auto_letterbox_stability_frames` and `auto_letterbox_check_interval_frames` are
debounce timings tuned together with the detection scan. Lowering stability to "react
faster" just brings the flicker back. The app doesn't show them, but still loads and
saves them, and clamps them itself (1 to 30 and 1 to 300) since they have no menu
row to clamp against. They can still be edited in the ini.

## Still open

The letterbox cutscene flicker in AC3. See "Still open" in
[framebuffer-flicker](framebuffer-flicker.md#still-open). Retest with the v3.3
buffer fix before assuming it's separate.
