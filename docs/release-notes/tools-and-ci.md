# Tools and CI release notes

Full release notes, moved out of the [changelog](../../CHANGELOG.md), which now keeps one line per change.

## Tools

- `tools/decode_verification_dump.py` -- decodes the plugin's raw UDP
  debug/telemetry packets for offline analysis. Went through several
  format-support passes as the plugin's own wire formats changed:
  registration-event decoding, config-reload packet decoding,
  auto-selecting the unpack format from registration packets, timing-
  packet decoding, HDR/PQ decoding, an R/B channel-swap fix for
  A8B8G8R8_SRGB, and runtime pixel-format auto-detection via decode
  smoothness.
- `tools/ps4_detile_2dthin.c` -- reference detiling implementation used
  to cross-check the plugin's own tiling math.
- `tools/udp_ground_truth_listener.py` -- minimal UDP listener used for
  capturing real packets from the plugin during development.
- `tools/flicker_capture.py` -- captures and analyzes the flicker probe
  packets ("FLK1", 132 bytes, one per sampler pass) from a debug build
  of the plugin. `capture <file> [--seconds N]` records to a `.flk`
  file, `analyze <file> [--csv out.csv]` prints a report (output
  steadiness, sampler timing, flip rate, all-zero reads and burst
  spacing, per-zone tear/disagreement between buffers, a ring-lag
  table showing which slot is safe to read, and which lag the pipeline
  actually read, so a working sample-lag fix is recognised), and
  `selftest` runs the analyzer against 10 synthetic scenarios. Needs a debug build of the plugin
  (`make DEBUG=1`, the FLK1 probe is compiled out of release builds) with
  `[dev] dev_ip` and `dev_logging=true` set. Uses UDP port 4048, so it
  can't run alongside `udp_ground_truth_listener.py`. Its verdict
  thresholds are estimates; trust the printed numbers. Found the
  cleared-buffer sampling bug described under Plugin > v3.3. Also counts
  how often the `0x88740000` 8-bit decode (FLK1 flags `0x10`) switched.
- `tools/test_pq8bit_vote.c` -- host test for the alpha byte vote behind the
  YouTube fix, run on words from real captures and on generated PQ. Builds
  with plain `gcc`, no SDK. See Plugin > v3.5.

## CI

- A tag push now fails early if the tag doesn't match `g_pluginVersion`
  (major.minor only, the tag's patch part is free). It's the first step
  of `build_prx`, and `publish` needs that job, so a mismatched tag such
  as `v3.5.0` on a plugin that still says 3.4 produces no release.
  Non-version tags like `v3.4.0-rc1` are rejected too. See
  [`docs/development/ci-and-releases.md`](../development/ci-and-releases.md).
- Tagged-release builds now also build and package the companion app
  (`build_pkg` job: compiles `companion-app/source/*.c` against the
  same OpenOrbis toolchain the plugin job uses, via its Linux
  `create-fself`/`create-gp4`/`PkgTool.Core` binaries instead of
  `build.bat`'s Windows ones, producing a `.pkg`). Releasing is now
  its own `publish` job that waits on both the plugin and companion
  app builds and is the only job that touches
  `softprops/action-gh-release`, so the two builds run in parallel
  without a chance of racing each other to create the same release.
- `build_pkg` pinned to `ubuntu-22.04` instead of `ubuntu-latest`:
  `PkgTool.Core`'s `pkg_build` subcommand needs `libssl1.1`, which
  `ubuntu-latest` (24.04) no longer ships, so it aborted with "No
  usable version of libssl was found" on the first real run.
- First real compile of `ui_icons.c` (this CI job is the first time
  the companion app has ever actually been build-tested) turned up a
  real bug, not a CI issue: `CUR`, the "currentColor" sentinel used
  84 times across the file's icon tables, was a `static const
  UiColor` object. Clang hard-errors reading a named const object's
  value into another file-scope initializer ("initializer element is
  not a compile-time constant") -- GCC's default GNU-extension mode
  had been silently tolerating this locally. Changed `CUR` to a
  compound-literal macro, matching the file's own `CHAN_RED`/
  `CHAN_GREEN`/`CHAN_BLUE` pattern, which doesn't have this problem.

