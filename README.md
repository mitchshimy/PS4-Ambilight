# PS4 Ambilight

An Ambilight-style setup for jailbroken PS4/PS5 consoles: a [GoldHEN](https://github.com/GoldHEN/GoldHEN)
plugin samples the console's video output every frame and streams the edge colors over the
network (DDP protocol) to a real [WLED](https://kno.wled.ge/) LED controller in real time while
you play. A companion homebrew app provides an on-console UI for editing settings and previewing
colors live.

See [CHANGELOG.md](CHANGELOG.md) for the version history of both the plugin and the companion app.

## Layout

```
PS4-Ambilight/
├── plugin/            # the GoldHEN plugin itself (runs on the PS4/PS5, hooks the video-out path)
│   ├── source/          # main.c + 8 supporting modules (gamma, network, tiling, hooks, ...)
│   ├── include/         # config.h, ambient_internal.h
│   ├── config/          # default ps4_ambient_light.ini
│   └── Makefile
├── common/             # plugin_common.{h,c} -- shared GoldHEN plugin helpers this build needs
├── companion-app/      # standalone PS4/PS5 homebrew app: on-console settings UI + live preview
│   ├── source/, include/, assets/, sce_sys/, sce_module/
│   └── tests/           # isolated unit tests for settings/pipeline/layout logic
├── tools/               # PC-side Python/C helper scripts used for capture and verification
├── .github/workflows/CI.yml
├── CHANGELOG.md
├── LICENSE
└── .gitignore
```

## Building the plugin

Requires the standard GoldHEN plugin toolchain:
- [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) (`OO_PS4_TOOLCHAIN`)
- [GoldHEN SDK](https://github.com/GoldHEN/GoldHEN_SDK) (`GOLDHEN_SDK`) -- headers + `libGoldHEN_Hook.a`

```
cd plugin
make
```

Output lands in `bin/plugins/` (created next to this repo root).

## Building the companion app

See [`companion-app/README.md`](companion-app/README.md) for its build steps and design notes.

## Configuration

Copy `plugin/config/ps4_ambient_light.ini` to `/data/ps4_ambient_light.ini` on the console (or
edit it live from the companion app) and set `wled_host`/`wled_port` to your WLED controller,
plus your strip's per-edge LED counts under `[layout]`.

## Credits

Built on top of [GoldHEN](https://github.com/GoldHEN/GoldHEN) and its
[plugin SDK](https://github.com/GoldHEN/GoldHEN_SDK); `common/plugin_common.{h,c}` is vendored
from the [GoldHEN Plugins Repository](https://github.com/GoldHEN/GoldHEN_Plugins_Repository).
