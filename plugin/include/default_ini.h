// default_ini.h -- the ini the plugin writes on first run when none exists.
// Kept identical to plugin/config/ps4_ambient_light.ini (the copy people
// download) -- companion-app/tests/test_presets.c compares the two, and
// compares the two preset sections against the app's shipped presets, so a
// change to either side that forgets the other fails a test instead of
// shipping. Own header, plain C string, so a PC can build it.
#ifndef DEFAULT_INI_H
#define DEFAULT_INI_H

#define AMBIENT_DEFAULT_INI \
    "[network]\n" \
    "; The real WLED controller's IP -- NOT this PC's own IP.\n" \
    "wled_host=\n" \
    "wled_port=4048\n" \
    "\n" \
    "[layout]\n" \
    "; Physical LED counts per screen edge. Defaults match this\n" \
    "; project's own measured strip (73/41/73/42 = 229 total).\n" \
    "led_count_top=73\n" \
    "led_count_right=41\n" \
    "led_count_bottom=73\n" \
    "led_count_left=42\n" \
    "; Which corner physical LED index 0 sits at, and which way the\n" \
    "; strip runs from there. Valid led_start_corner: bottom_left,\n" \
    "; bottom_right, top_left, top_right. Valid led_direction:\n" \
    "; clockwise, counterclockwise.\n" \
    "led_start_corner=bottom_left\n" \
    "led_direction=clockwise\n" \
    "; If the light show is correct but rotated around the border\n" \
    "; (e.g. everything is one LED off from where it should be),\n" \
    "; adjust this instead of led_start_corner/led_direction.\n" \
    "led_offset=0\n" \
    "; Two more keys can be added here by hand. They are left out of this file\n" \
    "; and of the companion app because they are tuned together with the\n" \
    "; detection scan, so they are only written if someone changes them.\n" \
    "; auto_letterbox_stability_frames (default 3):\n" \
    "; How many consecutive detections must agree before a new\n" \
    "; border is actually applied -- higher = slower to react but\n" \
    "; more resistant to flicker across a scene cut.\n" \
    "; auto_letterbox_check_interval_frames (default 15):\n" \
    "; How many sample-thread passes to wait between re-checks. Each\n" \
    "; check probes every edge inward (up to 180px from top/bottom,\n" \
    "; 320px from left/right) with several tiled reads per line, so\n" \
    "; it isn't free -- raise this if update_frequency_hz is high and\n" \
    "; CPU headroom is tight.\n" \
    "\n" \
    "[color]\n" \
    "; Match your strip's actual wiring. Valid values: RGB, RBG,\n" \
    "; GRB, GBR, BRG, BGR. Most WS2812B/NeoPixel strips are GRB.\n" \
    "color_order=RGB\n" \
    "\n" \
    "[timing]\n" \
    "; How many times per second to sample and send color.\n" \
    "update_frequency_hz=30\n" \
    "; How often (seconds) to check this file for changes WHILE\n" \
    "; RUNNING and apply them live -- no need to close/reopen the\n" \
    "; game. 0 = only read this file once, at plugin load (the\n" \
    "; original v2.0 behavior).\n" \
    "config_reload_check_seconds=2\n" \
    "\n" \
    "[presets]\n" \
    "; Which preset runs: game or movie. The companion app changes\n" \
    "; this. Apps listed in media_titles.h (plugin source) always run\n" \
    "; movie, whatever this says.\n" \
    "; Each preset is a complete set of the values below. A preset\n" \
    "; section that exists is used as-is: a key missing from it keeps\n" \
    "; the plugin's built-in default. If neither section exists (an\n" \
    "; ini from before v3.8) the same keys are read from [layout],\n" \
    "; [color] and [timing] as they used to be.\n" \
    "active=game\n" \
    "\n" \
    "[preset_game]\n" \
    "; Sample radius per zone: (2*scan_depth+1)^2 pixels averaged.\n" \
    "; Higher = smoother/less noisy but more CPU per frame. Capped\n" \
    "; at 4 -- values above that are rejected and the previous\n" \
    "; value is kept.\n" \
    "scan_depth=2\n" \
    "; Auto-detects black letterbox/pillarbox bars and insets\n" \
    "; sampling to stay off them, per edge independently (so e.g.\n" \
    "; a status bar only on top is handled correctly). This is the\n" \
    "; ONLY sampling inset the plugin applies -- there's no manual\n" \
    "; margin setting -- so leave this on unless you have a\n" \
    "; specific reason to sample every pixel unconditionally.\n" \
    "auto_letterbox_enabled=true\n" \
    "; A probed pixel counts as part of a black bar if every\n" \
    "; channel is below this (0-255).\n" \
    "auto_letterbox_threshold=18\n" \
    "; Global brightness scale, 0-255. 255 = no change.\n" \
    "brightness=255\n" \
    "; -100 (grayscale) to 100 (2x color boost). 0 = unchanged. Past\n" \
    "; 100 most colors clip at 255/0 and stop looking different, so\n" \
    "; values above 100 are clamped to 100.\n" \
    "saturation=35\n" \
    "; Must be exactly one of: 1.0 1.4 1.8 2.0 2.2 2.4 2.6 2.8\n" \
    "; (precomputed lookup tables -- no other value is accepted).\n" \
    "; Applied on top of WLED's own gamma if that is enabled for\n" \
    "; realtime data, so turn WLED's colour gamma off.\n" \
    "gamma=2.2\n" \
    "; Levels adjustment (0-100, percent of the 0-255 range).\n" \
    "; Anything at/below black_level becomes 0; anything at/above\n" \
    "; white_level becomes 255; the rest stretches to fill the gap.\n" \
    "; Defaults (0, 100) are a no-op.\n" \
    "black_level=0\n" \
    "white_level=100\n" \
    "; Contrast: -100..100, 0 = unchanged (stretches/shrinks around\n" \
    "; mid-grey 128, same math as saturation but around brightness\n" \
    "; instead of hue). At 100 (2x) half the tonal range already\n" \
    "; clips to black/white, so values above 100 are clamped.\n" \
    "contrast=0\n" \
    "; Blend each new sample with the previous one over roughly\n" \
    "; settling_time_ms, instead of snapping instantly -- reduces\n" \
    "; flicker on fast scene cuts. false = send raw samples as-is.\n" \
    "smoothing_enabled=true\n" \
    "settling_time_ms=50\n" \
    "; If a zone's brightest channel drops below this (0-255), that\n" \
    "; zone is forced fully black instead of showing a faint/noisy\n" \
    "; near-black color. Has built-in hysteresis (must rise 10 above\n" \
    "; this value again before turning back on) so it won't flicker\n" \
    "; on scenes hovering right at the threshold. 0 = disabled.\n" \
    "dark_threshold=0\n" \
    "; --- Per-channel values, ported from the Android version's own\n" \
    "; per-channel color engine. NOTE THE DIFFERENT CONVENTION: these\n" \
    "; use Android's \"100 = unchanged\" percent scale, NOT this file's\n" \
    "; usual \"0 = unchanged\" scale used by saturation/brightness above.\n" \
    "; Per-channel brightness, 0-500, 100 = unchanged. Multiplies with\n" \
    "; the single brightness above rather than replacing it.\n" \
    "brightness_r=100\n" \
    "brightness_g=100\n" \
    "brightness_b=100\n" \
    "; Per-channel gamma, 10-500, 100 = unchanged. Unlike the fixed\n" \
    "; gamma= list above (8 preset curves, shared across all 3\n" \
    "; channels), these accept ANY value in range and are independent\n" \
    "; per channel. Applied PER SAMPLE PIXEL before zone-averaging,\n" \
    "; not to the already-averaged zone color -- matches how the\n" \
    "; Android version does it, and avoids a single stray bright\n" \
    "; pixel in an otherwise-dark zone getting averaged in BEFORE\n" \
    "; being gamma-crushed.\n" \
    "gamma_r=100\n" \
    "gamma_g=100\n" \
    "gamma_b=100\n" \
    "\n" \
    "[preset_movie]\n" \
    "; Same keys and ranges as [preset_game] above; see the comments there.\n" \
    "scan_depth=4\n" \
    "auto_letterbox_enabled=true\n" \
    "auto_letterbox_threshold=18\n" \
    "brightness=204\n" \
    "saturation=25\n" \
    "gamma=2.4\n" \
    "black_level=1\n" \
    "white_level=100\n" \
    "contrast=0\n" \
    "smoothing_enabled=true\n" \
    "settling_time_ms=200\n" \
    "dark_threshold=0\n" \
    "brightness_r=100\n" \
    "brightness_g=100\n" \
    "brightness_b=100\n" \
    "gamma_r=100\n" \
    "gamma_g=100\n" \
    "gamma_b=100\n"

#endif // DEFAULT_INI_H
