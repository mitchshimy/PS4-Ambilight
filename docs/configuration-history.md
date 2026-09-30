# Configuration history

Which ini keys appeared, changed or went away, and when. Useful when you're
upgrading an old `/data/ps4_ambient_light.ini`, or wondering why a key from an old
guide does nothing. The current keys and what they do are in the README's
[Configuration](../README.md#configuration) and [Help](../README.md#help) sections.

The plugin fills in defaults for any missing key, so an old ini keeps working. The
one exception is a removed key, which is ignored, not migrated.

## By version

| Version | Change |
|---|---|
| v2.0 | Everything that had been a `#define` became a setting. `[network]`: `wled_host`, `wled_port`. `[layout]`: `led_count_top/right/bottom/left`, `led_start_corner`, `led_direction`, `led_offset`, `capture_margin_*`, `scan_depth`. `[color]`: `brightness`, `gamma`, `saturation`, `color_order`. `[timing]`: `update_frequency_hz`, `smoothing_enabled`, `settling_time_ms`. The file is created with a commented template if it doesn't exist. |
| v2.1 | `black_level`, `white_level`, `dark_threshold` in `[color]`. `config_reload_check_seconds` in `[timing]` (live reload, on by default at 2 s). |
| v2.1.1 | `saturation` cap raised from 100 to 300 (the integer math was checked for overflow first, the old cap was arbitrary). |
| v2.2 | `contrast`, plus per-channel `brightness_r/g/b` and `gamma_r/g/b`. |
| v2.2.5 | `[dev]` section for debug telemetry (`dev_ip`, `dev_logging`). Replaced a hardcoded debug address. Not in the shipped default ini. |
| v2.4 to v2.6 | Extra hand-edited-only networking keys for a niche multi-source setup. Not shown in the app and not documented in the shipped ini. Only written by the app if already present. |
| v2.9 | `auto_letterbox_enabled` (off), `auto_letterbox_threshold`, `auto_letterbox_stability_frames`, `auto_letterbox_check_interval_frames`. |
| v3.0 | **`capture_margin_*` removed.** Old values are ignored. `auto_letterbox_enabled` now defaults to `true`. |
| v3.3 | `scan_depth` is limited to 0 to 4, and a value above 4 is **rejected** and the default (1) is kept. `saturation` and `contrast` are limited to 100 and clamped on load, so an old `saturation=175` becomes 100 rather than resetting. |
| v3.8 | **Presets.** `[presets] active=` and two complete sections, `[preset_game]` and `[preset_movie]`, hold `scan_depth`, the letterbox threshold and enable, brightness, gamma, saturation, black and white level, contrast, `dark_threshold`, the per-channel brightness and gamma, `smoothing_enabled` and `settling_time_ms`. With a preset section present those keys are read only from it, and the old flat copies in `[layout]`, `[color]` and `[timing]` are ignored. An ini with no preset section still reads those flat keys. Apps in `media_titles.h` always run the movie preset. The two letterbox timing keys and the setup keys stay where they were. |

## Defaults that moved

`dark_threshold` was 0, was changed to 10 to match the companion app, and was later
put back to 0. A build made in between generates an ini with 10. If your strip stays dark on dim
scenes and your ini came from one of those, that's the first thing to check.

## Rejected versus clamped

The two behave differently on purpose:

- `scan_depth`: out of range means "ignore the line", so the default is used.
  That's how the plugin's other bounded keys behave.
- `saturation` and `contrast`: out of range means "clamp to the cap". Otherwise a
  value that used to be valid would silently turn the effect off.

## Companion app display

Some keys are shown differently in the app than they are stored. The ini always holds
the raw value: `brightness` is 0 to 255, and the app shows 255 as 100% and steps it by
5%. Brightness, both black thresholds, saturation, contrast, black and white level, RGB
balance and per-channel gamma are all shown as percentages. Untouched values are saved
exactly as loaded. The full mapping is the table under "Percentages in the companion
app vs. the ini" in the README.

The app also hides `auto_letterbox_stability_frames` and
`auto_letterbox_check_interval_frames`, see
[letterbox-detection](debugging/letterbox-detection.md#why-stability-and-recheck-are-hidden-in-the-app).
It clamps them itself when loading (1 to 30 and 1 to 300).

From v3.8 the app saves the presets and no longer writes the flat `[color]`, `[timing]` and
`[layout]` copies of their values, and it removes the ones an older file had on the first
save. It also drops the two hidden letterbox timing keys when they equal their defaults. The
app's ini reader has never kept comments, so a save also strips the comments the plugin
generated.

The app's Edge depth, Saturation and Contrast ranges follow the plugin's v3.3 caps.
