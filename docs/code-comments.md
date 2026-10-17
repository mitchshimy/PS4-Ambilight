# Long code comments

Every comment longer than two lines was moved here, in full, when the code comments were trimmed. In the code each one is replaced by a
short note and a pointer of the form `(path:line)`, where the line is the one the comment started on **before** trimming, so it is the
line number in the commit just before the trim (the entries below are listed by file and original line). Notes that guard against a
regression (crashes, hardware findings, invariants) were kept in the code as a two-line summary; their full text is here too.

Not touched: `companion-app/include/qrcodegen.h` and `companion-app/source/qrcodegen.c` (vendored, MIT licence), licence and copyright
comments, and Python docstrings.

## Contents

- [.github/workflows/CI.yml](#github-workflows-ci-yml) (15)
- [common/fw_support.h](#common-fw-support-h) (1)
- [companion-app/include/app_paths.h](#companion-app-include-app-paths-h) (5)
- [companion-app/include/app_version.h](#companion-app-include-app-version-h) (1)
- [companion-app/include/claim_file.h](#companion-app-include-claim-file-h) (1)
- [companion-app/include/color_pipeline.h](#companion-app-include-color-pipeline-h) (6)
- [companion-app/include/config.h](#companion-app-include-config-h) (9)
- [companion-app/include/data_dir.h](#companion-app-include-data-dir-h) (1)
- [companion-app/include/ddp.h](#companion-app-include-ddp-h) (1)
- [companion-app/include/file_util.h](#companion-app-include-file-util-h) (1)
- [companion-app/include/fw_support.h](#companion-app-include-fw-support-h) (1)
- [companion-app/include/github_release_json.h](#companion-app-include-github-release-json-h) (4)
- [companion-app/include/goldhen_config.h](#companion-app-include-goldhen-config-h) (1)
- [companion-app/include/help_qr.h](#companion-app-include-help-qr-h) (3)
- [companion-app/include/ime.h](#companion-app-include-ime-h) (1)
- [companion-app/include/install_plan.h](#companion-app-include-install-plan-h) (1)
- [companion-app/include/layout.h](#companion-app-include-layout-h) (3)
- [companion-app/include/net_http.h](#companion-app-include-net-http-h) (4)
- [companion-app/include/notify.h](#companion-app-include-notify-h) (2)
- [companion-app/include/payload_ini.h](#companion-app-include-payload-ini-h) (2)
- [companion-app/include/plugin_common.h](#companion-app-include-plugin-common-h) (2)
- [companion-app/include/presets.h](#companion-app-include-presets-h) (3)
- [companion-app/include/relay_signal.h](#companion-app-include-relay-signal-h) (1)
- [companion-app/include/settings.h](#companion-app-include-settings-h) (14)
- [companion-app/include/sha256.h](#companion-app-include-sha256-h) (2)
- [companion-app/include/shell_feed.h](#companion-app-include-shell-feed-h) (1)
- [companion-app/include/update_check.h](#companion-app-include-update-check-h) (1)
- [companion-app/include/update_source.h](#companion-app-include-update-source-h) (1)
- [companion-app/source/color_pipeline.c](#companion-app-source-color-pipeline-c) (4)
- [companion-app/source/config.c](#companion-app-source-config-c) (3)
- [companion-app/source/ddp.c](#companion-app-source-ddp-c) (1)
- [companion-app/source/file_util.c](#companion-app-source-file-util-c) (4)
- [companion-app/source/fw_check.c](#companion-app-source-fw-check-c) (1)
- [companion-app/source/help_qr.c](#companion-app-source-help-qr-c) (3)
- [companion-app/source/ime.c](#companion-app-source-ime-c) (5)
- [companion-app/source/layout.c](#companion-app-source-layout-c) (3)
- [companion-app/source/led_frame.c](#companion-app-source-led-frame-c) (3)
- [companion-app/source/led_frame.h](#companion-app-source-led-frame-h) (2)
- [companion-app/source/main.c](#companion-app-source-main-c) (56)
- [companion-app/source/net_http.c](#companion-app-source-net-http-c) (10)
- [companion-app/source/notify.c](#companion-app-source-notify-c) (1)
- [companion-app/source/plugin_install.c](#companion-app-source-plugin-install-c) (17)
- [companion-app/source/presets.c](#companion-app-source-presets-c) (4)
- [companion-app/source/relay_signal.c](#companion-app-source-relay-signal-c) (1)
- [companion-app/source/settings.c](#companion-app-source-settings-c) (21)
- [companion-app/source/sha256.c](#companion-app-source-sha256-c) (1)
- [companion-app/source/shell_feed.c](#companion-app-source-shell-feed-c) (9)
- [companion-app/source/text_render.c](#companion-app-source-text-render-c) (9)
- [companion-app/source/text_render.h](#companion-app-source-text-render-h) (3)
- [companion-app/source/ui_canvas.c](#companion-app-source-ui-canvas-c) (11)
- [companion-app/source/ui_canvas.h](#companion-app-source-ui-canvas-h) (10)
- [companion-app/source/ui_icons.c](#companion-app-source-ui-icons-c) (11)
- [companion-app/source/ui_icons.h](#companion-app-source-ui-icons-h) (2)
- [companion-app/source/ui_screens.c](#companion-app-source-ui-screens-c) (25)
- [companion-app/source/ui_screens.h](#companion-app-source-ui-screens-h) (12)
- [companion-app/source/ui_text.c](#companion-app-source-ui-text-c) (9)
- [companion-app/source/ui_text.h](#companion-app-source-ui-text-h) (4)
- [companion-app/source/ui_theme.c](#companion-app-source-ui-theme-c) (3)
- [companion-app/source/ui_theme.h](#companion-app-source-ui-theme-h) (3)
- [companion-app/source/ui_widgets.c](#companion-app-source-ui-widgets-c) (11)
- [companion-app/source/ui_widgets.h](#companion-app-source-ui-widgets-h) (4)
- [companion-app/source/update_check.c](#companion-app-source-update-check-c) (13)
- [companion-app/tests/stubs/orbis/ImeDialog.h](#companion-app-tests-stubs-orbis-imedialog-h) (1)
- [companion-app/tests/test_action_buttons.c](#companion-app-tests-test-action-buttons-c) (1)
- [companion-app/tests/test_app_version.c](#companion-app-tests-test-app-version-c) (1)
- [companion-app/tests/test_claim_file.c](#companion-app-tests-test-claim-file-c) (1)
- [companion-app/tests/test_github_release_json.c](#companion-app-tests-test-github-release-json-c) (1)
- [companion-app/tests/test_ime.c](#companion-app-tests-test-ime-c) (1)
- [companion-app/tests/test_layout.c](#companion-app-tests-test-layout-c) (8)
- [companion-app/tests/test_payload_ini.c](#companion-app-tests-test-payload-ini-c) (1)
- [companion-app/tests/test_presets.c](#companion-app-tests-test-presets-c) (1)
- [companion-app/tests/test_sce_stubs.c](#companion-app-tests-test-sce-stubs-c) (1)
- [companion-app/tests/test_settings.c](#companion-app-tests-test-settings-c) (2)
- [companion-app/tests/test_sha256.c](#companion-app-tests-test-sha256-c) (2)
- [companion-app/tests/test_shell_settings.c](#companion-app-tests-test-shell-settings-c) (1)
- [companion-app/tools/sce_file_stubs.c](#companion-app-tools-sce-file-stubs-c) (1)
- [companion-app/tools/ui_preview.c](#companion-app-tools-ui-preview-c) (1)
- [plugin/Makefile](#plugin-makefile) (2)
- [plugin/include/ambient_internal.h](#plugin-include-ambient-internal-h) (26)
- [plugin/include/ambilight_paths.h](#plugin-include-ambilight-paths-h) (1)
- [plugin/include/config.h](#plugin-include-config-h) (9)
- [plugin/include/default_ini.h](#plugin-include-default-ini-h) (1)
- [plugin/include/hdr2200_vote.h](#plugin-include-hdr2200-vote-h) (2)
- [plugin/include/media_titles.h](#plugin-include-media-titles-h) (1)
- [plugin/include/plugin_entry_alignment.h](#plugin-include-plugin-entry-alignment-h) (1)
- [plugin/include/pq8bit_vote.h](#plugin-include-pq8bit-vote-h) (2)
- [plugin/include/preset_select.h](#plugin-include-preset-select-h) (1)
- [plugin/include/report_file.h](#plugin-include-report-file-h) (3)
- [plugin/include/smoothing.h](#plugin-include-smoothing-h) (4)
- [plugin/include/state_file.h](#plugin-include-state-file-h) (4)
- [plugin/source/buffer_guard.c](#plugin-source-buffer-guard-c) (9)
- [plugin/source/color_processing.c](#plugin-source-color-processing-c) (14)
- [plugin/source/config.c](#plugin-source-config-c) (2)
- [plugin/source/gamma.c](#plugin-source-gamma-c) (5)
- [plugin/source/hooks.c](#plugin-source-hooks-c) (11)
- [plugin/source/letterbox.c](#plugin-source-letterbox-c) (18)
- [plugin/source/main.c](#plugin-source-main-c) (15)
- [plugin/source/network.c](#plugin-source-network-c) (28)
- [plugin/source/notices.c](#plugin-source-notices-c) (3)
- [plugin/source/pixel_formats.c](#plugin-source-pixel-formats-c) (12)
- [plugin/source/report.c](#plugin-source-report-c) (2)
- [plugin/source/sample_thread.c](#plugin-source-sample-thread-c) (31)
- [plugin/source/settings.c](#plugin-source-settings-c) (25)
- [plugin/source/state.c](#plugin-source-state-c) (3)
- [plugin/source/tiling.c](#plugin-source-tiling-c) (3)
- [plugin/source/zones.c](#plugin-source-zones-c) (18)
- [shell-feed/Makefile](#shell-feed-makefile) (3)
- [shell-feed/include/shell_internal.h](#shell-feed-include-shell-internal-h) (3)
- [shell-feed/source/main.c](#shell-feed-source-main-c) (4)
- [shell-feed/source/shell_buffers.c](#shell-feed-source-shell-buffers-c) (2)
- [shell-feed/source/shell_config.c](#shell-feed-source-shell-config-c) (5)
- [shell-feed/source/shell_frame.c](#shell-feed-source-shell-frame-c) (2)
- [shell-feed/source/shell_hooks.c](#shell-feed-source-shell-hooks-c) (4)
- [shell-feed/source/shell_live.c](#shell-feed-source-shell-live-c) (7)
- [shell-feed/source/shell_log.c](#shell-feed-source-shell-log-c) (2)
- [shell-feed/source/shell_modules.c](#shell-feed-source-shell-modules-c) (3)
- [shell-feed/source/shell_probe17.c](#shell-feed-source-shell-probe17-c) (3)
- [shell-feed/source/shell_relay.c](#shell-feed-source-shell-relay-c) (2)
- [shell-feed/source/shell_report.c](#shell-feed-source-shell-report-c) (2)
- [shell-feed/source/shell_shim.c](#shell-feed-source-shell-shim-c) (5)
- [shell-feed/source/shell_state.c](#shell-feed-source-shell-state-c) (3)
- [shell-feed/source/shell_thread.c](#shell-feed-source-shell-thread-c) (8)
- [shell-feed/tools/run_stagec_core_test.sh](#shell-feed-tools-run-stagec-core-test-sh) (1)
- [shell-feed/tools/test_stagec_core.c](#shell-feed-tools-test-stagec-core-c) (5)
- [tools/decode_verification_dump.py](#tools-decode-verification-dump-py) (10)
- [tools/flicker_capture.py](#tools-flicker-capture-py) (1)
- [tools/host_stubs/Common.h](#tools-host-stubs-common-h) (1)
- [tools/ps4_detile_2dthin.c](#tools-ps4-detile-2dthin-c) (10)
- [tools/test_buffer_guard.c](#tools-test-buffer-guard-c) (3)
- [tools/test_hdr2200_vote.c](#tools-test-hdr2200-vote-c) (2)
- [tools/test_notices.c](#tools-test-notices-c) (1)
- [tools/test_plugin_config.c](#tools-test-plugin-config-c) (4)
- [tools/test_plugin_entry_alignment.c](#tools-test-plugin-entry-alignment-c) (1)
- [tools/test_pq8bit_vote.c](#tools-test-pq8bit-vote-c) (1)
- [tools/test_report.c](#tools-test-report-c) (1)
- [tools/test_smoothing.c](#tools-test-smoothing-c) (2)
- [tools/test_state.c](#tools-test-state-c) (1)
- [tools/udp_ground_truth_listener.py](#tools-udp-ground-truth-listener-py) (1)
- [tools/update_payload_checksum.sh](#tools-update-payload-checksum-sh) (1)

## .github/workflows/CI.yml

### .github/workflows/CI.yml:23

In the code now:

```
# payloads/CHECKSUMS.md is what people verify ps4-ambilight.bin against, and the release attaches that...
# More: docs/code-comments.md (.github/workflows/CI.yml:23)
```

Original (lines 23-25):

```
# payloads/CHECKSUMS.md is what people verify ps4-ambilight.bin against, and the
# release attaches that binary. Fail here, on every push, if the binary was replaced
# without the page. Fix: sh tools/update_payload_checksum.sh, then commit both.
```

### .github/workflows/CI.yml:29

In the code now:

```
# On a tag push, the tag has to name the version the plugin says it is, so tagging v3.5.0 can't ship a...
# More: docs/code-comments.md (.github/workflows/CI.yml:29)
```

Original (lines 29-33):

```
# On a tag push, the tag has to name the version the plugin says it
# is, so tagging v3.5.0 can't ship a plugin that still reports 3.4.
# g_pluginVersion in plugin/source/hooks.c is 0xMMmm (0x0304 = 3.4),
# so only major.minor is compared and the tag's patch part is free.
# Failing here also stops the publish job, which needs this one.
```

### .github/workflows/CI.yml:46

In the code now:

```
# Allows an optional "-rc.N" pre-release suffix (e.g. v3.9.4-rc1) so a shell-feed hardware test build...
# More: docs/code-comments.md (.github/workflows/CI.yml:46)
```

Original (lines 46-51):

```
# Allows an optional "-rc.N" pre-release suffix (e.g. v3.9.4-rc1)
# so a shell-feed hardware test build (see publish job below) can
# be tagged and built through this same check without it being
# mistaken for a real release tag. The suffix sits on the patch
# segment, after the last ".", so it doesn't affect the
# major.minor extraction two lines below.
```

### .github/workflows/CI.yml:69

In the code now:

```
# Upstream's 64-bit detour path has real bugs (wrong instruction size, wrong jump-write variant, and a...
# More: docs/code-comments.md (.github/workflows/CI.yml:69)
```

Original (lines 69-76):

```
# Upstream's 64-bit detour path has real bugs (wrong instruction
# size, wrong jump-write variant, and a non-executable malloc'd
# trampoline that either crashes or gets silently blocked under
# DEP/W^X) -- see patches/goldhen-sdk-detour64-fix.patch's own
# header for the full breakdown of each hunk. Applying this is
# what makes the SDK checkout above match what was actually
# tested crash-free on hardware; without it, this job would
# silently build against upstream's broken version every time.
```

### .github/workflows/CI.yml:84

In the code now:

```
# The 'llvm' package on ubuntu-latest only installs versioned binaries (llvm-ar-NN), not an...
# More: docs/code-comments.md (.github/workflows/CI.yml:84)
```

Original (lines 84-86):

```
# The 'llvm' package on ubuntu-latest only installs versioned
# binaries (llvm-ar-NN), not an unversioned 'llvm-ar' -- and the
# GoldHEN SDK's Makefile calls the unversioned name.
```

### .github/workflows/CI.yml:111

In the code now:

```
# shell-feed Stage A: built against the same patched SDK, same toolchain env, as the Stage-A merge...
# More: docs/code-comments.md (.github/workflows/CI.yml:111)
```

Original (lines 111-117):

```
# shell-feed Stage A: built against the same patched SDK, same
# toolchain env, as the Stage-A merge plan calls for. Its Makefile
# computes the same bin/plugins/ output folder plugin/Makefile does
# (both resolve "../bin/plugins" from their own directory), so both
# plugins land in prx_final/ and prx_debug/ together -- the upload
# steps below select each .prx by name rather than globbing the
# whole folder, so the two artifacts stay distinct.
```

### .github/workflows/CI.yml:155

In the code now:

```
# PkgTool.Core's pkg_build subcommand (unlike sfo_new/sfo_setentry, which don't touch OpenSSL) needs...
# More: docs/code-comments.md (.github/workflows/CI.yml:155)
```

Original (lines 155-162):

```
# PkgTool.Core's pkg_build subcommand (unlike sfo_new/sfo_setentry,
# which don't touch OpenSSL) needs libssl1.1 to sign/hash the
# package, and ubuntu-latest is Ubuntu 24.04, which dropped
# libssl1.1 from its repos entirely -- only libssl3 is available,
# so PkgTool.Core aborts with "No usable version of libssl was
# found" (confirmed on a real run). ubuntu-22.04 still ships
# libssl1.1 by default, so pin here rather than hand-installing a
# legacy .deb alongside libssl3.
```

### .github/workflows/CI.yml:173

In the code now:

```
# PkgTool.Core is a self-contained .NET build that otherwise crashes on a bare runner looking for ICU...
# More: docs/code-comments.md (.github/workflows/CI.yml:173)
```

Original (lines 173-175):

```
# PkgTool.Core is a self-contained .NET build that otherwise
# crashes on a bare runner looking for ICU -- this skips
# culture-aware globalization instead of installing libicu.
```

### .github/workflows/CI.yml:182

In the code now:

```
# The release tarball doesn't reliably preserve the executable bit for the Linux tools...
# More: docs/code-comments.md (.github/workflows/CI.yml:182)
```

Original (lines 182-184):

```
# The release tarball doesn't reliably preserve the executable
# bit for the Linux tools (create-fself/create-gp4/PkgTool.Core)
# the way it does for the Windows .exes build.bat calls.
```

### .github/workflows/CI.yml:188

In the code now:

```
# Linux equivalent of build.bat's compile loop.
# More: docs/code-comments.md (.github/workflows/CI.yml:188)
```

Original (lines 188-192):

```
# Linux equivalent of build.bat's compile loop. The Windows
# script's extra -I for include\freetype2 is a dead flag on this
# toolchain snapshot -- ft2build.h already resolves via the
# top-level include dir below -- so it's dropped here rather
# than carried over.
```

### .github/workflows/CI.yml:257

In the code now:

```
# Separate from both build jobs so build_prx and build_pkg can run in parallel: this job just waits on...
# More: docs/code-comments.md (.github/workflows/CI.yml:257)
```

Original (lines 257-260):

```
# Separate from both build jobs so build_prx and build_pkg can run in
# parallel: this job just waits on both and is the only one that
# talks to softprops/action-gh-release, so there's no risk of the two
# builds racing each other to create/update the same tagged release.
```

### .github/workflows/CI.yml:268

In the code now:

```
# Only for payloads/ and the script that checks it: the Ambilight payload (ps4-ambilight.bin) is a...
# More: docs/code-comments.md (.github/workflows/CI.yml:268)
```

Original (lines 268-270):

```
# Only for payloads/ and the script that checks it: the Ambilight payload
# (ps4-ambilight.bin) is a committed binary, not a build output, so the release takes
# it from the tagged commit.
```

### .github/workflows/CI.yml:297

In the code now:

```
# Only the Release .prx is ever fetched by anything at runtime -- companion-app/source/main.c's...
# More: docs/code-comments.md (.github/workflows/CI.yml:297)
```

Original (lines 297-308):

```
# Only the Release .prx is ever fetched by anything at runtime --
# companion-app/source/main.c's PLUGIN_UPDATE_URL points at
# ".../releases/latest/download/ps4_ambient_light.prx", which
# only resolves once an actual GitHub Release exists with an
# asset of exactly that name attached. This job produces both
# that asset and its checksum sidecar (PLUGIN_CHECKSUM_URL,
# "<same name>.sha256") from the SAME build_prx artifact, so they
# can never drift out of sync with each other -- do_plugin_update()
# in companion-app/source/plugin_install.c refuses to install if they don't match. The same
# happens for ps4_shell_feed.prx: the companion app's Shell feed
# install downloads it and its sidecar by these names and checks one
# against the other. (The Ambilight payload has its own step below.)
```

### .github/workflows/CI.yml:315

In the code now:

```
# The Ambilight payload (payloads/ps4-ambilight.bin).
# More: docs/code-comments.md (.github/workflows/CI.yml:315)
```

Original (lines 315-319):

```
# The Ambilight payload (payloads/ps4-ambilight.bin). The companion app's Shell feed
# install fetches it, and its ".sha256" sidecar, by exactly these names from the latest
# release, and refuses the file if the two disagree. The sidecar is made from the file
# being attached. build_prx already ran this check on the push; running it again on the
# tagged commit means a binary changed without payloads/CHECKSUMS.md can't ship.
```

### .github/workflows/CI.yml:328

In the code now:

```
# A tag like v3.9.4-rc1 passes the build_prx version check (see its comment) but must never become the...
# More: docs/code-comments.md (.github/workflows/CI.yml:328)
```

Original (lines 328-333):

```
# A tag like v3.9.4-rc1 passes the build_prx version check (see its
# comment) but must never become the release that "latest" resolves
# to, since do_plugin_update() in companion-app and the researchers'
# own notes both treat "latest" as what real users receive. This is
# what makes it safe to test shell-feed's release-asset naming on a
# real tag (merge plan step 10) before it ever ships for real.
```

## common/fw_support.h

### common/fw_support.h:1

In the code now:

```
// fw_support.h -- firmware status for the current standalone shell-feed loader...
// More: docs/code-comments.md (common/fw_support.h:1)
```

Original (lines 1-12):

```
// fw_support.h -- firmware status for the current standalone shell-feed loader (payloads/ps4-ambilight.bin).
// Informational only, never a block: the app warns, the payload decides at runtime.
//
// Three states, because the loader has a per-firmware table:
//   confirmed  -- run end to end on a console (9.00, 12.52)
//   untested   -- the loader has an entry for it, nobody has run it
//   not listed -- no entry; the loader reports "unsupported firmware" and leaves the kernel alone
// The table below is the init sections in the shipped binary (.text.init_<n>sdk, n = major*100+minor),
// read from its section names, not from a disassembly. The loader's source is in a separate repo, so
// when payloads/ps4-ambilight.bin is replaced, re-read the list and update kListed and the confirmed
// versions by hand. The old bundled loader's offset table does not apply.
// Keep common/fw_support.h and companion-app/include/fw_support.h identical.
```

## companion-app/include/app_paths.h

### companion-app/include/app_paths.h:9

In the code now:

```
// Every file of ours lives in APP_DATA_DIR (data_dir.h); only /data/GoldHEN and /data/payloads, which...
// More: docs/code-comments.md (companion-app/include/app_paths.h:9)
```

Original (lines 9-12):

```
// Every file of ours lives in APP_DATA_DIR (data_dir.h); only /data/GoldHEN and /data/payloads, which
// belong to GoldHEN, are outside it.
// Same path the plugin itself reads -- this app edits that exact
// file, not a copy, so there's only ever one source of truth.
```

### companion-app/include/app_paths.h:16

In the code now:

```
// Where the plugin binary and GoldHEN's own plugin registry live.
// More: docs/code-comments.md (companion-app/include/app_paths.h:16)
```

Original (lines 16-18):

```
// Where the plugin binary and GoldHEN's own plugin registry live.
// Confirmed via GOLDHEN_PATH in the plugin repo's own common/
// plugin_common.h and multiple independent install-guide sources.
```

### companion-app/include/app_paths.h:32

In the code now:

```
// Companion checksum asset -- must be uploaded to the SAME GitHub release as the .prx above, named by...
// More: docs/code-comments.md (companion-app/include/app_paths.h:32)
```

Original (lines 32-52):

```
// Companion checksum asset -- must be uploaded to the SAME GitHub
// release as the .prx above, named by appending ".sha256" to the prx
// filename. Its contents are expected to be (or at least start with)
// 64 lowercase/uppercase hex chars: the plain output of
// `sha256sum ps4_ambient_light.prx` is exactly this, including its
// trailing "  ps4_ambient_light.prx" -- sha256_hex_matches() only
// reads the first 64 hex chars, so that suffix is fine as-is. A
// release build's CI should run that command and upload the result
// as this exact filename alongside the .prx.
//
// This catches a corrupted or tampered download (the two things
// flagged about this path: no integrity check on what gets installed,
// and the SSL callback below accepts any certificate). It is NOT a
// substitute for real certificate validation -- an attacker able to
// MITM the .prx download can equally serve a fake checksum file next
// to it. Real protection against that would mean signing releases
// with a keypair and embedding the public key here instead of
// fetching a hash from the same untrusted connection; this is the
// lighter-weight step that at least catches corruption and
// accidental/incidental tampering, and is worth doing regardless of
// whether signing is added later.
```

### companion-app/include/app_paths.h:56

In the code now:

```
// Local sidecar recording the checksum of whatever build this app itself last installed at...
// More: docs/code-comments.md (companion-app/include/app_paths.h:56)
```

Original (lines 56-64):

```
// Local sidecar recording the checksum of whatever build this app
// itself last installed at PLUGIN_PRX_PATH -- written by
// do_plugin_update() right after a successful install (see
// write_installed_checksum() below). check_for_plugin_update() only
// falls back to this when it can't hash PLUGIN_PRX_PATH directly --
// this sidecar isn't touched by anything outside this app, so it goes
// stale the moment someone replaces the .prx by hand (FTP, a manual
// GoldHEN plugins folder edit, ...) and can't be trusted as the
// primary source of "what's actually installed right now."
```

### companion-app/include/app_paths.h:67

In the code now:

```
// Shell feed files (shell-feed/) Three files make up an install: the plugin above, the shell feed's...
// More: docs/code-comments.md (companion-app/include/app_paths.h:67)
```

Original (lines 67-71):

```
// ---- Shell feed files (shell-feed/) ----
// Three files make up an install: the plugin above, the shell feed's own .prx, and the small
// Ambilight payload that puts that .prx into the console's compositor. The payload finds the .prx
// at SHELL_PRX_PATH (shell-feed/docs/loading.md). Nothing in this block is touched while the
// "Shell feed" switch in Set up is off.
```

## companion-app/include/app_version.h

### companion-app/include/app_version.h:1

In the code now:

```
// The companion app's version: the one place to change it.
// More: docs/code-comments.md (companion-app/include/app_version.h:1)
```

Original (lines 1-8):

```
// The companion app's version: the one place to change it.
//
// It is shown on the Help screen and becomes APP_VER and VERSION in the package's param.sfo. Both
// build paths read this line: .github/workflows/CI.yml (sed) and build.bat (findstr). Keep it a
// single "#define APP_VERSION" line in the form "N.NN", at most 8 characters (the SFO field).
// tests/test_app_version.c checks the form.
// #pragma once, not an include guard named APP_VERSION_*: build.bat finds the version line by
// its "#define APP_VERSION" prefix, and a guard would match it too.
```

## companion-app/include/claim_file.h

### companion-app/include/claim_file.h:1

In the code now:

```
// claim_file.h -- the companion app's half of /data/ps4-ambilight/ps4_ambient_state.
// More: docs/code-comments.md (companion-app/include/claim_file.h:1)
```

Original (lines 1-22):

```
// claim_file.h -- the companion app's half of /data/ps4-ambilight/ps4_ambient_state.
//
// The shell feed (shell-feed/) samples the console's own screens and sends them to the strip
// whenever no game is driving it. It learns about a game from this file. The companion app is
// not a game, but it drives the same strip (the live preview on Set up and Customization, and
// Test strip), and used to fight the shell feed for it: WLED alternated between the two
// senders and the LEDs flickered. So while the app is sending, it claims the strip the same
// way a game's plugin does, and the shell feed stays out of the way.
//
// Same record, same rules as plugin/include/state_file.h (v1, 35 bytes, written at offset 0
// without truncating so a reader never sees a torn write); only the title differs. That file
// is deliberately not included from here -- this app's build has no ../plugin on its include
// path -- so the format is repeated below, and tests/test_claim_file.c checks it byte for
// byte against the plugin's real state_format().
//
// Reader rules this has to satisfy (docs/development/shell-feed-coordination.md):
//  * a claim is fresh while fg=1 and seq keeps CHANGING (staleness window 2.5 s),
//  * the first record a reader ever sees cannot arm a claim by itself, only a change can,
//  * fg=0 releases at once, and a crash just stops the heartbeat and expires on its own.
// So: seq continues from whatever is on disk (the next record always differs from the last
// one a reader saw), fg=1 is written the moment the app starts sending, then once a second,
// and fg=0 exactly once when it stops.
```

## companion-app/include/color_pipeline.h

### companion-app/include/color_pipeline.h:6

In the code now:

```
// AmbientConfig, StartCorner, LedDirection, ColorOrder below are copied VERBATIM from...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:6)
```

Original (lines 6-11):

```
// AmbientConfig, StartCorner, LedDirection, ColorOrder below are
// copied VERBATIM from ps4_ambient_light v2.2's main.c. This must stay
// byte-for-byte in sync with the real plugin struct -- the settings
// UI and ini reader/writer in this app depend on field names matching
// exactly. Re-sync from the plugin's main.c whenever it changes,
// don't hand-edit independently.
```

### companion-app/include/color_pipeline.h:21

In the code now:

```
// wled-relay hand-off signal (ps4_ambient_light's own v2.3+ feature, ported to this app at v17, then...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:21)
```

Original (lines 21-27):

```
// wled-relay hand-off signal (ps4_ambient_light's own v2.3+ feature,
// ported to this app at v17, then lost when v18's UI rebuild
// apparently started from a pre-v17 baseline -- reintroduced here,
// same field layout/position as v17 had it). Deliberately NOT
// exposed in kMenuItems/the settings UI -- see settings.c's
// settings_load/settings_save for why, and main.c for where the
// signal itself actually fires.
```

### companion-app/include/color_pipeline.h:57

In the code now:

```
// [shell] -- the shell feed (shell-feed/), the second plugin that lights the strip from the console's...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:57)
```

Original (lines 57-59):

```
// [shell] -- the shell feed (shell-feed/), the second plugin that lights the strip from the
// console's own screens. Both are Set up rows. Default OFF: with it off the app behaves as it
// did before the shell feed existed.
```

### companion-app/include/color_pipeline.h:67

In the code now:

```
// Must be called once whenever gammaR/gammaG/gammaB change (config load, or a live edit in the...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:67)
```

Original (lines 67-69):

```
// Must be called once whenever gammaR/gammaG/gammaB change (config
// load, or a live edit in the settings UI) before colorpipeline_process
// is called -- mirrors the real plugin calling this at config load.
```

### companion-app/include/color_pipeline.h:72

In the code now:

```
// Runs the exact same processing chain as the real plugin's applyColorProcessing on a single input...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:72)
```

Original (lines 72-74):

```
// Runs the exact same processing chain as the real plugin's
// applyColorProcessing on a single input color. out3 receives the
// final 3 bytes already in the configured wire order (colorOrder).
```

### companion-app/include/color_pipeline.h:77

In the code now:

```
// Exposed separately in case the app needs the wire-order remap on its own (e.g. showing "what the...
// More: docs/code-comments.md (companion-app/include/color_pipeline.h:77)
```

Original (lines 77-79):

```
// Exposed separately in case the app needs the wire-order remap on its
// own (e.g. showing "what the strip receives" vs "the true RGB color"
// as two separate preview swatches).
```

## companion-app/include/config.h

### companion-app/include/config.h:23

In the code now:

```
// @brief Creates an empty ini_table_s struct for writing new entries to. @return ini_table_s*
// More: docs/code-comments.md (companion-app/include/config.h:23)
```

Original (lines 23-26):

```
/**
 * @brief Creates an empty ini_table_s struct for writing new entries to.
 * @return ini_table_s*
 */
```

### companion-app/include/config.h:29

In the code now:

```
// @brief Free up all the allocated resources in the ini_table_s struct. @param table
// More: docs/code-comments.md (companion-app/include/config.h:29)
```

Original (lines 29-32):

```
/**
 * @brief Free up all the allocated resources in the ini_table_s struct.
 * @param table
 */
```

### companion-app/include/config.h:35

In the code now:

```
// @brief Creates an ini_table_s struct filled with data from the specified `file'.
// More: docs/code-comments.md (companion-app/include/config.h:35)
```

Original (lines 35-41):

```
/**
 * @brief Creates an ini_table_s struct filled with data from the specified
 *        `file'.  Returns NULL if the file can not be read.
 * @param table
 * @param file
 * @return ini_table_s*
 */
```

### companion-app/include/config.h:44

In the code now:

```
// @brief Writes the specified ini_table_s struct to the specified `file'.
// More: docs/code-comments.md (companion-app/include/config.h:44)
```

Original (lines 44-51):

```
/**
 * @brief Writes the specified ini_table_s struct to the specified `file'.
 *        Returns false if the file could not be opened for writing, otherwise
 *        true.
 * @param table
 * @param file
 * @return bool
 */
```

### companion-app/include/config.h:54

In the code now:

```
// @brief Creates a new entry in the `table' containing the `key' and `value' provided if it does not...
// More: docs/code-comments.md (companion-app/include/config.h:54)
```

Original (lines 54-62):

```
/**
 * @brief Creates a new entry in the `table' containing the `key' and `value'
 *        provided if it does not exist.  Otherwise, modifies an exsiting `key'
 *        with the new `value'
 * @param table
 * @param section_name
 * @param key
 * @param value
 */
```

### companion-app/include/config.h:65

In the code now:

```
// @brief Checks for the existance of an entry in the specified `table'.
// More: docs/code-comments.md (companion-app/include/config.h:65)
```

Original (lines 65-72):

```
/**
 * @brief Checks for the existance of an entry in the specified `table'.  Returns
 *        false if the entry does not exist, otherwise true.
 * @param table
 * @param section_name
 * @param key
 * @return bool
 */
```

### companion-app/include/config.h:75

In the code now:

```
// @brief Retrieves the unmodified value of the specified `key' in `section_name'.
// More: docs/code-comments.md (companion-app/include/config.h:75)
```

Original (lines 75-83):

```
/**
 * @brief Retrieves the unmodified value of the specified `key' in `section_name'.
 *        Returns NULL if the entry does not exist, otherwise a pointer to the
 *        entry value data.
 * @param table
 * @param section_name
 * @param key
 * @return const char*
 */
```

### companion-app/include/config.h:86

In the code now:

```
// @brief Retrieves the value of the specified `key' in `section_name', converted to int.
// More: docs/code-comments.md (companion-app/include/config.h:86)
```

Original (lines 86-94):

```
/**
 * @brief Retrieves the value of the specified `key' in `section_name', converted
 *        to int.  Returns false on failure, otherwise true.
 * @param table
 * @param section_name
 * @param key
 * @param [out]value
 * @return int
 */
```

### companion-app/include/config.h:97

In the code now:

```
// @brief Retrieves the value of the specified `key' in `section_name', converted to bool.
// More: docs/code-comments.md (companion-app/include/config.h:97)
```

Original (lines 97-105):

```
/**
 * @brief Retrieves the value of the specified `key' in `section_name', converted
 *        to bool.  Returns false on failure, true otherwise.
 * @param table
 * @param section_name
 * @param key
 * @param [out]value
 * @return bool
 */
```

## companion-app/include/data_dir.h

### companion-app/include/data_dir.h:1

In the code now:

```
// data_dir.h -- the one folder every file of ours lives in on the console.
// More: docs/code-comments.md (companion-app/include/data_dir.h:1)
```

Original (lines 1-8):

```
// data_dir.h -- the one folder every file of ours lives in on the console.
//
// Config, claim file, version marker, reports, update staging and the shell feed .prx all sit in
// APP_DATA_DIR, so finding, backing up or removing the project's files is one folder. Only
// /data/GoldHEN and /data/payloads, which belong to GoldHEN, are outside it.
//
// This is the app's copy of AMBILIGHT_DATA_DIR in plugin/include/ambilight_paths.h (this app's build
// has no ../plugin on its include path). tests/test_claim_file.c fails if the two differ.
```

## companion-app/include/ddp.h

### companion-app/include/ddp.h:10

In the code now:

```
// Sends numZones RGB triplets (in whatever wire order the caller has already arranged them in -- see...
// More: docs/code-comments.md (companion-app/include/ddp.h:10)
```

Original (lines 10-23):

```
// Sends numZones RGB triplets (in whatever wire order the caller has
// already arranged them in -- see colorpipeline_write_color_ordered)
// to host:port via a single DDP UDP packet. Wire format copied
// verbatim from ps4_ambient_light's wled_send_rgb_zones -- same
// header bytes, same offset-0 behavior, so a real WLED controller
// treats this exactly like a frame from the plugin itself.
//
// Returns false if host isn't a literal IPv4 address (inet_pton
// rejects hostnames -- e.g. "wled.local" typed into the on-screen
// keyboard instead of an IP) or the socket couldn't be created.
// Previously void: a bad host failed the same way every frame at
// 30fps with nothing ever telling the user why the light wasn't
// responding. Does NOT detect an unreachable-but-valid IP -- UDP
// sendto() doesn't report that synchronously.
```

## companion-app/include/file_util.h

### companion-app/include/file_util.h:1

In the code now:

```
// Small file helpers and checksums over the SDK's sceKernelOpen/Read/Write, split out of main.c. The...
// More: docs/code-comments.md (companion-app/include/file_util.h:1)
```

Original (lines 1-3):

```
// Small file helpers and checksums over the SDK's sceKernelOpen/Read/Write, split out of main.c.
// The code is as it ran on hardware (stat() and fopen() are not trusted on this SDK, see the note
// on http_download() in net_http.c).
```

## companion-app/include/fw_support.h

### companion-app/include/fw_support.h:1

In the code now:

```
// fw_support.h -- firmware status for the current standalone shell-feed loader...
// More: docs/code-comments.md (companion-app/include/fw_support.h:1)
```

Original (lines 1-12):

```
// fw_support.h -- firmware status for the current standalone shell-feed loader (payloads/ps4-ambilight.bin).
// Informational only, never a block: the app warns, the payload decides at runtime.
//
// Three states, because the loader has a per-firmware table:
//   confirmed  -- run end to end on a console (9.00, 12.52)
//   untested   -- the loader has an entry for it, nobody has run it
//   not listed -- no entry; the loader reports "unsupported firmware" and leaves the kernel alone
// The table below is the init sections in the shipped binary (.text.init_<n>sdk, n = major*100+minor),
// read from its section names, not from a disassembly. The loader's source is in a separate repo, so
// when payloads/ps4-ambilight.bin is replaced, re-read the list and update kListed and the confirmed
// versions by hand. The old bundled loader's offset table does not apply.
// Keep common/fw_support.h and companion-app/include/fw_support.h identical.
```

## companion-app/include/github_release_json.h

### companion-app/include/github_release_json.h:1

In the code now:

```
// The scan that finds one asset's API URL in GitHub's "latest release" JSON, split out of main.c so a...
// More: docs/code-comments.md (companion-app/include/github_release_json.h:1)
```

Original (lines 1-3):

```
// The scan that finds one asset's API URL in GitHub's "latest release" JSON, split out of main.c so
// a PC test can run it (tests/test_github_release_json.c). Used by net_http.c. The code is as it
// ran on hardware; only "static" became "static inline".
```

### companion-app/include/github_release_json.h:12

In the code now:

```
// Hand-rolled scan for one field, NOT a general JSON parser -- there's no JSON library linked into...
// More: docs/code-comments.md (companion-app/include/github_release_json.h:12)
```

Original (lines 12-26):

```
// Hand-rolled scan for one field, NOT a general JSON parser -- there's
// no JSON library linked into this app, and GitHub's release-asset
// response shape (each asset object listing "url" as its first field,
// well before "name") is stable enough that this is a reasonable,
// low-risk way to pull one string out of it rather than pulling in a
// dependency for one lookup. It only has to survive being handed
// GitHub's own JSON, not arbitrary/hostile input.
//
// Finds the asset object in releaseJson whose "name" field equals
// assetName exactly (not e.g. a prefix match against
// "ps4_ambient_light.prx.sha256" when looking for
// "ps4_ambient_light.prx"), and copies that object's "url" field
// (the asset's own small api.github.com URL -- NOT
// "browser_download_url", which just redirects through github.com's
// web app again, the exact thing this whole fix avoids) into outUrl.
```

### companion-app/include/github_release_json.h:40

In the code now:

```
// needle/needleSpaced both end in the closing '"' right after assetName, so a match against either...
// More: docs/code-comments.md (companion-app/include/github_release_json.h:40)
```

Original (lines 40-44):

```
// needle/needleSpaced both end in the closing '"' right after
// assetName, so a match against either already rules out matching
// "ps4_ambient_light.prx" as a prefix inside
// "ps4_ambient_light.prx.sha256" -- no separate boundary check
// needed.
```

### companion-app/include/github_release_json.h:50

In the code now:

```
// The asset object's "url" field comes before "name" in GitHub's schema, so walk backward from namePos...
// More: docs/code-comments.md (companion-app/include/github_release_json.h:50)
```

Original (lines 50-54):

```
// The asset object's "url" field comes before "name" in GitHub's
// schema, so walk backward from namePos to this asset's opening
// '{' (the nearest unmatched '{' scanning left), then search
// forward from there (not from the start of the whole response)
// so we can't accidentally grab a different asset's "url".
```

## companion-app/include/goldhen_config.h

### companion-app/include/goldhen_config.h:1

In the code now:

```
// goldhen_config.h -- READ one setting from GoldHEN's own config file: [PluginLoader]...
// More: docs/code-comments.md (companion-app/include/goldhen_config.h:1)
```

Original (lines 1-21):

```
// goldhen_config.h -- READ one setting from GoldHEN's own config file:
//
//   [PluginLoader]
//   PluginLoader_Enabled = 1        <- "Enable Plugins Loader" in Settings > GoldHEN > Plugin Settings
//
// The file name, section and key come from two real files off a console, one with the switch
// off and one with it on; the value was the only difference between them. GoldHEN writes the
// file when the switch is changed in its menu.
//
// This is READ-ONLY on purpose. An earlier version also wrote the value, and on a console that
// did not work: the file said 1 but GoldHEN's menu still showed the switch off, because GoldHEN
// keeps the live value in memory and never re-reads the file. Worse, the file then disagreed
// with the real state, so Home would have reported the loader on while it was off. GoldHEN also
// refuses to change the switch while a game or app is running, and this app counts as one. So
// the only fix is the person's: close the app, tick the switch in GoldHEN's menu.
//
// Anything unexpected is LOADER_UNKNOWN, which means "don't claim anything": a missing file, a
// GoldHEN version without the key, a value that isn't exactly 0 or 1. The app only ever acts
// on a definite OFF, so an unfamiliar config can never block the plugin's own buttons.
//
// Header-only and free of console calls so tests/test_goldhen_config.c can run it on a PC.
```

## companion-app/include/help_qr.h

### companion-app/include/help_qr.h:6

In the code now:

```
// The one URL this app ever turns into a QR code: the GitHub README's own Help section, kept in sync...
// More: docs/code-comments.md (companion-app/include/help_qr.h:6)
```

Original (lines 6-11):

```
// The one URL this app ever turns into a QR code: the GitHub README's
// own Help section, kept in sync by hand with README.md's actual
// "## Help" heading (GitHub slugifies that to #help). If that heading
// ever gets renamed, this string has to be updated to match, or the
// QR code will still scan fine but land above/below the section it's
// meant to jump to.
```

### companion-app/include/help_qr.h:14

In the code now:

```
// Encodes HELP_QR_URL exactly once (on first call) and caches the result for the rest of the process...
// More: docs/code-comments.md (companion-app/include/help_qr.h:14)
```

Original (lines 14-21):

```
// Encodes HELP_QR_URL exactly once (on first call) and caches the
// result for the rest of the process -- the source text is a
// compile-time constant, so there's nothing to invalidate. Returns
// false if encoding somehow fails (it can't, in practice, for a URL
// this short: qrcodegen_encodeText only fails by running out of the
// version range or the buffer it's given, and HELP_QR_URL is a fixed
// ~45 chars against a generous max version -- but main.c still checks
// the return value once at startup rather than assuming).
```

### companion-app/include/help_qr.h:28

In the code now:

```
// True if module (x, y) is a dark square, false if light. x and y run [0, help_qr_size())...
// More: docs/code-comments.md (companion-app/include/help_qr.h:28)
```

Original (lines 28-32):

```
// True if module (x, y) is a dark square, false if light. x and y run
// [0, help_qr_size()); out-of-range coordinates are treated as light
// rather than asserting, since the quiet-zone border baked into
// help_qr_size() means callers iterating the full grid never need to
// special-case the edge.
```

## companion-app/include/ime.h

### companion-app/include/ime.h:1

In the code now:

```
// The on-screen keyboard for the numeric and string rows of the settings screens, split out of main.c....
// More: docs/code-comments.md (companion-app/include/ime.h:1)
```

Original (lines 1-7):

```
// The on-screen keyboard for the numeric and string rows of the settings screens, split out of
// main.c. It knows nothing about main.c's globals: everything it needs comes in through ImeHost.
//
// Flow: Cross on a FIELD_STRING / FIELD_U32 / FIELD_I32 / FIELD_U16 row calls ime_open_field();
// every frame while ime_is_open(), the main loop calls ime_update(), which does nothing until the
// system keyboard closes, then applies (or discards) what was typed and reports through
// host->status. ime_shutdown() closes a dialog that is still open when the app quits.
```

## companion-app/include/install_plan.h

### companion-app/include/install_plan.h:1

In the code now:

```
// install_plan.h -- what Home shows, and what its one button does, once the shell feed is part of the...
// More: docs/code-comments.md (companion-app/include/install_plan.h:1)
```

Original (lines 1-17):

```
// install_plan.h -- what Home shows, and what its one button does, once the shell feed is
// part of the picture. Pure decision logic with no console calls, so
// tests/test_install_plan.c can check every combination on a PC.
//
// Three files can be pending: the plugin, the shell feed's .prx and the Ambilight payload.
// The plugin keeps the states it always had (UiInstallState). The other two are folded
// into one "Shell feed" unit, because nobody using the app cares which of the two a
// problem is in -- they get one banner and one state.
//
// Shell feed OFF is the default and means "behave exactly as before": shell_summarize()
// returns SHELL_OFF, home_plan() ignores it, and the Test strip rule is the old one.
//
// GoldHEN's own "Enable Plugins Loader" switch outranks everything: with it off GoldHEN loads
// no plugin at all, so installing, updating or testing the plugin would only look like it worked.
// It is read from GoldHEN's config (goldhen_config.h). Only a definite OFF changes anything. The app
// can't turn it on (GoldHEN keeps the live value in memory and won't change it while an app runs),
// so the button for it just closes the app so the person can.
```

## companion-app/include/layout.h

### companion-app/include/layout.h:4

In the code now:

```
// Ported from the Android app's LedLayoutGeometry.kt (calculateLedPositions + getEdgeOrder) so the PS4...
// More: docs/code-comments.md (companion-app/include/layout.h:4)
```

Original (lines 4-16):

```
// Ported from the Android app's LedLayoutGeometry.kt
// (calculateLedPositions + getEdgeOrder) so the PS4 companion app's
// layout preview agrees with the phone app's own preview, and so the
// order LEDs are emitted in here is the same physical wire order the
// real strip expects -- this is what update_live_preview() was
// missing: it only ever built and sent 5 swatches at DDP pixel offset
// 0, so on any real installFor with more than 5 LEDs only the first
// few pixels (wherever offset 0 physically is) ever lit up, no matter
// how ledCountTop/Right/Bottom/Left were configured. This module
// builds one slot per *configured* LED, in the order they're actually
// wired, so the caller can both (a) send a full-strip buffer over DDP
// and (b) draw every LED at its real position around a TV outline,
// same as the phone app's LedVisualization.kt does.
```

### companion-app/include/layout.h:32

In the code now:

```
// Fills outSlots (capacity maxSlots) with one entry per LED configured in cfg (ledCountTop +...
// More: docs/code-comments.md (companion-app/include/layout.h:32)
```

Original (lines 32-46):

```
// Fills outSlots (capacity maxSlots) with one entry per LED configured
// in cfg (ledCountTop + ledCountRight + ledCountBottom + ledCountLeft
// total, capped at LAYOUT_MAX_LEDS and maxSlots), in real physical
// wire order: outSlots[0] is the first pixel the strip receives,
// outSlots[1] the second, and so on -- startCorner/direction pick
// which edge is walked first and which way (mirrors
// getEdgeOrder()/calculateLedPositions() exactly), and ledOffset then
// rotates that order the same way the Android preview rotates its own
// array, so outSlots[0] still lines up with physical pixel 0 after
// accounting for a strip that doesn't start exactly at your chosen
// corner. Positions are laid out around a screenW x screenH rectangle
// inset by `padding` on every side, with each LED's depth-into-the-
// screen size driven by cfg->scanDepth (as a percent of that side's
// length, mirroring the Android version's scanDepthV/scanDepthH).
// Returns the number of slots written.
```

### companion-app/include/layout.h:50

In the code now:

```
// Returns a hue (0-360) derived purely from slot's (cx, cy, zone) -- its fixed physical-position...
// More: docs/code-comments.md (companion-app/include/layout.h:50)
```

Original (lines 50-72):

```
// Returns a hue (0-360) derived purely from slot's (cx, cy, zone) --
// its fixed physical-position identity -- via distance walked around
// a fixed reference perimeter (always top L->R, right T->B, bottom
// R->L, left B->T; this reference has nothing to do with
// startCorner/direction, it only needs to be *some* consistent,
// unchanging parameterization). screenW/screenH/padding must match
// whatever was passed to layout_build() so positions map back
// correctly.
//
// This exists because a hue derived from a slot's position in
// outSlots[] (i.e. from array index, post-reorder) is wrong: DDP
// pixel-offset i always refers to the same physical bulb (WLED has no
// concept of startCorner/direction/ledOffset; it just lights bulb i
// with whatever the i'th triplet says), so a color assigned by index
// alone never changes when those settings reorder *which* physical
// location ends up at index i -- the reordering becomes invisible on
// the real strip even though it's genuinely happening and visibly
// correct in an on-screen preview drawn from the same outSlots[].
// Deriving hue from the slot's own position instead means a given
// physical bulb's color visibly changes as different physical
// locations get rotated into its wire-offset, which is what makes
// these settings something you can actually tune by eye against a
// real light.
```

## companion-app/include/net_http.h

### companion-app/include/net_http.h:1

In the code now:

```
// Downloads over HTTP(S) and the lookup of where a release asset lives.
// More: docs/code-comments.md (companion-app/include/net_http.h:1)
```

Original (lines 1-3):

```
// Downloads over HTTP(S) and the lookup of where a release asset lives. Split out of main.c; the
// code is as it ran on hardware. Everything here may be called from the update-check thread except
// net_set_dev_base_url(), which main.c calls from the main thread only (see below).
```

### companion-app/include/net_http.h:12  (kept in code as a summary)

In the code now:

```
// Never GET github.com's /releases/latest/download directly: its huge redirect headers overflow sceHttp's header
// buffer (0x80431073). Resolve the asset via api.github.com first, then follow its small 302. [docs/code-comments.md]
```

Original (lines 12-29):

```
// BUG FIX (this file's own install log, 0x80431073 == SCE_HTTP_ERROR_
// TOO_LARGE_RESPONSE_HEADER): a "/releases/latest/download/..." link
// is served by github.com's web app, and the redirect response it
// sends back carries that app's full header set (a several-KB
// content-security-policy header alone, plus strict-transport-
// security, vary, x-github-request-id, ...). sceHttp has a small
// fixed header-parsing buffer and rejects that response outright,
// before sceHttpSetAutoRedirect(tpl, 1) in net_http.c ever gets a chance to
// act on the redirect -- which is why this always worked in a browser
// (no such fixed buffer) and always failed here. api.github.com is a
// plain JSON API, not the web app, and doesn't send those headers, so
// the fix is the standard two-hop pattern instead of hitting
// github.com directly: (1) look up the release's asset list here,
// (2) GET the resolved asset's own small api.github.com URL with
// Accept: application/octet-stream, which 302s (small headers) to a
// signed, short-lived objects.githubusercontent.com URL that
// sceHttpSetAutoRedirect follows the rest of the way. See
// github_resolve_asset_api_url() in net_http.c and do_plugin_update() in main.c.
```

### companion-app/include/net_http.h:35

In the code now:

```
// Downloads full_url to local_dst.
// More: docs/code-comments.md (companion-app/include/net_http.h:35)
```

Original (lines 35-37):

```
// Downloads full_url to local_dst. Returns true on a clean 200 + complete read. If outDigest is
// non-NULL it is filled with the SHA-256 of exactly the bytes written to local_dst, computed as each
// chunk arrives. acceptHeader may be NULL.
```

### companion-app/include/net_http.h:49

In the code now:

```
// The [dev] update_base_url override, cached. main.c reads it from the ini on the MAIN thread only...
// More: docs/code-comments.md (companion-app/include/net_http.h:49)
```

Original (lines 49-53):

```
// The [dev] update_base_url override, cached. main.c reads it from the ini on the MAIN thread only
// (config.c reads the file with fopen(), which this codebase does not trust from a worker thread,
// see the note on http_download() in net_http.c) and hands it over with net_set_dev_base_url(); the
// update-check thread only reads it, through resolve_asset_url() and net_dev_base_url().
// net_dev_base_url() is "" when downloads come from GitHub.
```

## companion-app/include/notify.h

### companion-app/include/notify.h:1

In the code now:

```
// notify.h -- a line in the console's own notification bar, the same bar the plugin and the shell feed...
// More: docs/code-comments.md (companion-app/include/notify.h:1)
```

Original (lines 1-3):

```
// notify.h -- a line in the console's own notification bar, the same bar the plugin and the shell feed
// use ("PS4 Ambilight: shell feed is waiting: no WLED address is set..."), for the few things a person
// should see even when the app's own screen is not where they are looking.
```

### companion-app/include/notify.h:9

In the code now:

```
// essential = the strip can't work, or the person is being misled, without it: sent whatever...
// More: docs/code-comments.md (companion-app/include/notify.h:9)
```

Original (lines 9-11):

```
// essential = the strip can't work, or the person is being misled, without it: sent whatever
// [notifications] enabled says (the Notifications switch on Set up). Everything else is information and respects that switch
// (on by default; the same rule as the plugin's say() and the shell feed's sh_notify()).
```

## companion-app/include/payload_ini.h

### companion-app/include/payload_ini.h:1

In the code now:

```
// payload_ini.h -- read and flip ONE entry in GoldHEN's payload AutoRun list, touching nothing else.
// More: docs/code-comments.md (companion-app/include/payload_ini.h:1)
```

Original (lines 1-16):

```
// payload_ini.h -- read and flip ONE entry in GoldHEN's payload AutoRun list, touching nothing else.
//
//   [AutoRun]
//   /user/data/payloads/ps4-ambilight.bin = 1
//
// "= 1" runs the payload when GoldHEN loads, "= 0" keeps the entry but doesn't run it
// (confirmed on a console). The file belongs to GoldHEN and is shared with every other
// payload the person has, so this is text surgery, not the app's ini_table: that generic
// parser drops comments and would rewrite lines it doesn't understand (see the long comment
// above ensure_plugin_registered_in_goldhen() in main.c, which hit exactly that on plugins.ini).
// Everything outside the one line this changes is copied through byte for byte.
//
// Entries are matched by FILE NAME, not by the whole path, because the same file shows up as
// /data/payloads/x.bin or /user/data/payloads/x.bin depending on who added the line.
//
// Header-only and free of console calls so tests/test_payload_ini.c can run it on a PC.
```

### companion-app/include/payload_ini.h:132

In the code now:

```
// Returns a malloc'd, NUL-terminated copy of the file with the entry for `fileName` set to on (1) or...
// More: docs/code-comments.md (companion-app/include/payload_ini.h:132)
```

Original (lines 132-141):

```
// Returns a malloc'd, NUL-terminated copy of the file with the entry for `fileName` set to
// on (1) or off (0); the caller frees it. *outLen is its length, *changed whether it differs
// from the input.
//  * An existing entry (every one, if there are duplicates) has only its value rewritten --
//    path text, spacing and any trailing comment stay as they were.
//  * No entry and on=true: `entryPath = 1` is added at the end of [AutoRun] (the section is
//    created at the end of the file if there is none).
//  * No entry and on=false: nothing to turn off, the text is returned unchanged.
//  * Line endings of the file (LF or CRLF) are kept for inserted lines.
// Returns NULL only if out of memory.
```

## companion-app/include/plugin_common.h

### companion-app/include/plugin_common.h:4

In the code now:

```
// config.c (copied verbatim from ps4_ambient_light's plugin source) expects this header to provide...
// More: docs/code-comments.md (companion-app/include/plugin_common.h:4)
```

Original (lines 4-8):

```
// config.c (copied verbatim from ps4_ambient_light's plugin source)
// expects this header to provide debug_printf. This app is a
// standalone homebrew application, not a GoldHEN plugin, so none of
// the real plugin_common.h's hook/attr_public/GOLDHEN_PATH machinery
// applies here -- only config.c's one dependency is stubbed.
```

### companion-app/include/plugin_common.h:16  (kept in code as a summary)

In the code now:

```
// Orbis open(2) flags are BSD, not Linux: O_WRONLY=0x1, O_CREAT=0x200, O_TRUNC=0x400.
// Create-or-overwrite must include 0x400, or a shorter rewrite keeps the old tail bytes. [docs/code-comments.md]
```

Original (lines 16-44):

```
// BUG FIX: every sceKernelOpen() call in this app that means to
// create-or-overwrite a file was passing the literal "0x200 | 0x001"
// with a comment claiming that's O_TRUNC|O_CREAT. It isn't, on this
// OS. Orbis (the PS4's kernel) is FreeBSD-derived, and sceKernelOpen
// is a thin wrapper straight onto the real BSD open(2) syscall, whose
// flag bits are 0x0001=O_WRONLY, 0x0200=O_CREAT, 0x0400=O_TRUNC (NOT
// the Linux/glibc values, which some of this file's flags were
// evidently guessed from). So "0x200 | 0x001" is actually
// O_CREAT|O_WRONLY, with O_TRUNC's 0x400 bit missing entirely.
//
// For a path being written for the very first time this is invisible
// -- O_CREAT alone against a nonexistent file already starts it at
// length 0. It only shows up against a path that already has content
// from a previous write: the new data overwrites the front of the
// file, but if the new write is shorter than whatever was already
// there, the old trailing bytes are never discarded. do_plugin_update()
// in main.c hits this on its staging file (PLUGIN_PRX_PATH ".new"),
// which can carry leftover bytes from an earlier update attempt: the
// SHA-256 verified against the release is computed while streaming
// the download (so it's correct), but the file that then actually
// gets copied/renamed into place can be longer than what was
// verified, and PLUGIN_PRX_PATH's real on-disk hash stops matching
// GitHub's published checksum from then on -- "update available"
// every launch even though the release itself never changed.
//
// Named here (rather than re-guessing 0x601 inline at each call site)
// so every "create or overwrite" sceKernelOpen() across this app --
// config.c's ini writer, main.c's tmp download / checksum sidecar /
// plugins.ini rewrite / staged .prx copy -- shares one correct value.
```

## companion-app/include/presets.h

### companion-app/include/presets.h:1

In the code now:

```
// presets.h -- Game / Movie presets for the Customization screen.
// More: docs/code-comments.md (companion-app/include/presets.h:1)
```

Original (lines 1-17):

```
// presets.h -- Game / Movie presets for the Customization screen.
//
// A preset is a snapshot of every Customization-screen value (colour,
// smoothing, letterbox, sampling, RGB balance...). Setup values (WLED
// address, LED layout, colour order, update rate) are never part of a
// preset.
//
// HOW IT'S STORED: the ini holds [presets] active=game|movie and the full set of
// each preset's values in [preset_game] / [preset_movie] (same key names and value
// formats the plugin has always used). The plugin (v3.8+) reads the active preset's
// section itself, so those keys are NOT also written to [layout] / [color] / [timing]
// -- those sections keep only setup values (LED layout, colour order, update rate...).
// Loading an ini from before presets still works: its flat values become Game.
//
// Editing a field edits the active preset -- there is no Custom mode.
// Switching presets stashes the current values into their slot first,
// so unsaved edits to one preset survive a trip to the other.
```

### companion-app/include/presets.h:39

In the code now:

```
// the live preset set (module state) Call once after settings_load().
// More: docs/code-comments.md (companion-app/include/presets.h:39)
```

Original (lines 39-44):

```
// ---- the live preset set (module state) ----
// Call once after settings_load(). Reads the preset sections from `path`
// (missing file / sections are fine), migrates an ini written before
// presets existed, and leaves `cfg` showing the active preset. Once an ini has
// preset sections they are the only source: leftover flat values are ignored,
// exactly as the plugin ignores them.
```

### companion-app/include/presets.h:56

In the code now:

```
// Stashes cfg into the active slot, then writes the whole ini in one pass: setup values in their own...
// More: docs/code-comments.md (companion-app/include/presets.h:56)
```

Original (lines 56-59):

```
// Stashes cfg into the active slot, then writes the whole ini in one pass: setup
// values in their own sections, [presets], and both preset sections, merged into
// what's already at `path` so anything else in the file survives. This replaces
// settings_save() for the app; it removes the old flat copies of preset values.
```

## companion-app/include/relay_signal.h

### companion-app/include/relay_signal.h:7

In the code now:

```
// Reintroduced -- this app had this at v17 (ps4_ambient_light's own wled-relay hand-off signal, ported...
// More: docs/code-comments.md (companion-app/include/relay_signal.h:7)
```

Original (lines 7-19):

```
// Reintroduced -- this app had this at v17 (ps4_ambient_light's own
// wled-relay hand-off signal, ported over from the real plugin), then
// lost it when v18's UI rebuild started from a pre-v17 baseline. See
// AmbientConfig's relayHost/relayPort/relaySignalEnabled in
// color_pipeline.h and main.c's use of this function for the rest of
// the story.
//
// Sends a single one-shot UDP datagram to host:port carrying the bare
// ASCII text "on" or "off" (no header, no MQTT client needed) --
// wled-relay's own listener treats this exactly like the signal it
// already gets from the real plugin. Same "returns false on a bad
// host, does not detect unreachable-but-valid" caveat as
// ddp_send_rgb_zones -- see that function's own comment in ddp.h.
```

## companion-app/include/settings.h

### companion-app/include/settings.h:9

In the code now:

```
// Field types for the generic, data-driven settings menu.
// More: docs/code-comments.md (companion-app/include/settings.h:9)
```

Original (lines 9-12):

```
// Field types for the generic, data-driven settings menu. One
// MenuItem per ini key the real plugin reads (verified against
// ps4_ambient_light's actual ini_table_get_entry* call sites, not
// recalled from memory -- see settings.c's kMenuItems table).
```

### companion-app/include/settings.h:24

In the code now:

```
// Which of the two settings screens (Set Up / Customisation) an item belongs to -- see the comment...
// More: docs/code-comments.md (companion-app/include/settings.h:24)
```

Original (lines 24-26):

```
// Which of the two settings screens (Set Up / Customisation) an item
// belongs to -- see the comment above kMenuItems in settings.c for how
// this and the group/groupDesc fields below are used to lay out cards.
```

### companion-app/include/settings.h:32

In the code now:

```
// How a numeric field is SHOWN and TYPED in the UI.
// More: docs/code-comments.md (companion-app/include/settings.h:32)
```

Original (lines 32-35):

```
// How a numeric field is SHOWN and TYPED in the UI. Purely presentation --
// the value stored in AmbientConfig, written to the ini and read by the
// plugin is always the raw number in [min,max]; only what the person sees
// and enters changes. "255" means nothing to most people, "100%" does.
```

### companion-app/include/settings.h:51

In the code now:

```
// repurposed as the destination buffer's size in bytes (e.g. sizeof(cfg.wledHost)) -- see settings.c's...
// More: docs/code-comments.md (companion-app/include/settings.h:51)
```

Original (lines 51-56):

```
// repurposed as the destination buffer's
// size in bytes (e.g. sizeof(cfg.wledHost))
// -- see settings.c's STRBUF() macro --
// rather than adding a whole new struct
// field just for the one string field
// this schema currently has.
```

### companion-app/include/settings.h:61

In the code now:

```
// expected to sit contiguously per group (see kMenuItems' own ordering); a run of items sharing the...
// More: docs/code-comments.md (companion-app/include/settings.h:61)
```

Original (lines 61-64):

```
// expected to sit contiguously per group
// (see kMenuItems' own ordering); a run of
// items sharing the same group string is
// one card
```

### companion-app/include/settings.h:66

In the code now:

```
// the heading, on the first item of a new group -- NULL on every other item in that same group
// More: docs/code-comments.md (companion-app/include/settings.h:66)
```

Original (lines 66-68):

```
// the heading, on the first item of a new
// group -- NULL on every other item in
// that same group
```

### companion-app/include/settings.h:77

In the code now:

```
// Fills cfg with the plugin's own compile-time defaults (copied verbatim from AmbientConfig's real...
// More: docs/code-comments.md (companion-app/include/settings.h:77)
```

Original (lines 77-82):

```
// Fills cfg with the plugin's own compile-time defaults (copied
// verbatim from AmbientConfig's real initializer in main.c) BEFORE
// settings_load overlays whatever the ini file actually contains --
// same "defaults first, then overlay only what's present" behavior as
// the plugin's own ambient_load_config, so a field missing from an
// edited-down ini behaves identically here and in the real plugin.
```

### companion-app/include/settings.h:85

In the code now:

```
// Reads path into cfg, overlaying settings_set_defaults().
// More: docs/code-comments.md (companion-app/include/settings.h:85)
```

Original (lines 85-88):

```
// Reads path into cfg, overlaying settings_set_defaults(). Returns
// false if the file doesn't exist or can't be parsed -- caller should
// still proceed with defaults in that case, matching the plugin's own
// "keep defaults on parse failure" behavior.
```

### companion-app/include/settings.h:91

In the code now:

```
// Writes cfg out to path using config.h's ini_table_write_to_file, preserving the exact section/key...
// More: docs/code-comments.md (companion-app/include/settings.h:91)
```

Original (lines 91-95):

```
// Writes cfg out to path using config.h's ini_table_write_to_file,
// preserving the exact section/key names the plugin reads. Does NOT
// attempt to preserve comments or key order from an existing file --
// this always produces a fresh, canonical layout. Returns false on
// write failure (path unwritable, etc.).
```

### companion-app/include/settings.h:98

In the code now:

```
// True for the values a preset owns: every real row on the Customization screen (not the "ui" rows...
// More: docs/code-comments.md (companion-app/include/settings.h:98)
```

Original (lines 98-100):

```
// True for the values a preset owns: every real row on the Customization screen
// (not the "ui" rows -- the preset selector, its reset button, the smoothing
// shortcut -- and not text or button rows). Setup values are never preset-owned.
```

### companion-app/include/settings.h:103

In the code now:

```
// The text on a FIELD_ACTION button (what the UI draws inside it).
// More: docs/code-comments.md (companion-app/include/settings.h:103)
```

Original (lines 103-108):

```
// The text on a FIELD_ACTION button (what the UI draws inside it). Kept here, away from the
// drawing code, so a PC test can check every button. Returns false for an action key it does not
// know, and the caller then falls back to the row's own label: a new action must never inherit
// another button's text (the payload button once read "Reset Game" because every action was
// drawn as the preset reset). `resetArmed` only matters for "preset_reset"; `presetIndex` is the
// active preset (Game or Movie), used by it too.
```

### companion-app/include/settings.h:112

In the code now:

```
// Puts the setup keys (and, if `presetKeys`, the flat preset-owned keys too) into `table`, which...
// More: docs/code-comments.md (companion-app/include/settings.h:112)
```

Original (lines 112-118):

```
// Puts the setup keys (and, if `presetKeys`, the flat preset-owned keys too) into
// `table`, which should already hold the current file so unknown sections survive.
// With presetKeys == false the preset-owned keys are REMOVED from their old flat
// sections, and the two hidden letterbox timing keys are only kept if they differ
// from their defaults. That is
// the concise layout presets_save writes; settings_save (presetKeys == true) is the
// old all-flat layout.
```

### companion-app/include/settings.h:121

In the code now:

```
// Generic get/set through a MenuItem's offset -- used by the UI so navigation/adjustment code is...
// More: docs/code-comments.md (companion-app/include/settings.h:121)
```

Original (lines 121-125):

```
// Generic get/set through a MenuItem's offset -- used by the UI so
// navigation/adjustment code is written once, not per-field.
// Display-unit helpers (settings.c). The UI shows/accepts "display" values;
// everything stored stays raw. Round-tripping an UNTOUCHED value never
// goes through these, so merely opening a screen can't perturb a setting.
```

### companion-app/include/settings.h:136

In the code now:

```
// v2.9: smoothing preset convenience (settings.c) -- see that file's comment above...
// More: docs/code-comments.md (companion-app/include/settings.h:136)
```

Original (lines 136-144):

```
// v2.9: smoothing preset convenience (settings.c) -- see that file's
// comment above kSmoothingPresetNames for why this is a UI-only view
// over smoothingEnabled/settlingTimeMs rather than a real settings
// field. The kMenuItems row for this uses section "ui" (not a real ini
// section) precisely so it's never mistaken for one of the actual
// plugin-schema entries around it.
// The eight gamma presets, as the ini stores them (a string, not the
// index the struct holds). Shared by settings.c's load/save and the
// preset sections written by presets.c so all three can't drift.
```

## companion-app/include/sha256.h

### companion-app/include/sha256.h:1

In the code now:

```
// sha256.h -- minimal streaming SHA-256, no dependencies beyond stdint.h/stddef.h. Public domain...
// More: docs/code-comments.md (companion-app/include/sha256.h:1)
```

Original (lines 1-9):

```
// sha256.h -- minimal streaming SHA-256, no dependencies beyond
// stdint.h/stddef.h. Public domain implementation (Brad Conte's
// crypto-algorithms, adapted): https://github.com/B-Con/crypto-algorithms
//
// Added for verifying a downloaded plugin .prx against a published
// checksum before it's ever staged/installed (see do_plugin_update()
// in main.c) -- neither OpenOrbis nor the GoldHEN SDK expose a hash
// primitive we could find, so this is self-contained rather than
// pulled from libSceSsl/libSceHttp.
```

### companion-app/include/sha256.h:36

In the code now:

```
// Case-insensitive compare of two hex digest strings.
// More: docs/code-comments.md (companion-app/include/sha256.h:36)
```

Original (lines 36-40):

```
// Case-insensitive compare of two hex digest strings. Returns true if
// they represent the same 32 bytes. Tolerant of a trailing filename
// after the hex (the common "<hash>  filename" sha256sum format) --
// only the first 64 hex chars of `hexWithMaybeSuffix` are considered,
// so callers don't need to pre-trim a downloaded checksum file.
```

## companion-app/include/shell_feed.h

### companion-app/include/shell_feed.h:1

In the code now:

```
// The shell feed's file work, split out of main.c: install and repair of ps4_shell_feed.prx and the...
// More: docs/code-comments.md (companion-app/include/shell_feed.h:1)
```

Original (lines 1-4):

```
// The shell feed's file work, split out of main.c: install and repair of ps4_shell_feed.prx and the
// Ambilight payload, sending the payload to GoldHEN's BinLoader, the autoload entry in GoldHEN's
// payload list, and the claim that keeps the shell feed off the strip while this app drives it.
// The code is as it ran on hardware.
```

## companion-app/include/update_check.h

### companion-app/include/update_check.h:1

In the code now:

```
// The background update check, split out of main.c: the worker thread that asks the update source...
// More: docs/code-comments.md (companion-app/include/update_check.h:1)
```

Original (lines 1-3):

```
// The background update check, split out of main.c: the worker thread that asks the update source
// whether the plugin or a shell feed file has a newer build, the flags it publishes through, and
// the once-a-frame poll that turns the answer into Home's state. The code is as it ran on hardware.
```

## companion-app/include/update_source.h

### companion-app/include/update_source.h:10

In the code now:

```
// " http://192.168.2.117:8000/ \r" -> "http://192.168.2.117:8000".
// More: docs/code-comments.md (companion-app/include/update_source.h:10)
```

Original (lines 10-12):

```
// "  http://192.168.2.117:8000/ \r" -> "http://192.168.2.117:8000". Accepts only http:// or https://
// URLs (anything else would turn a typo into a request to nowhere), trims blanks and trailing
// slashes, and fails if it doesn't fit. out is "" on failure.
```

## companion-app/source/color_pipeline.c

### companion-app/source/color_pipeline.c:1

In the code now:

```
// color_pipeline.c -- the companion app's live-preview color math.
// More: docs/code-comments.md (companion-app/source/color_pipeline.c:1)
```

Original (lines 1-21):

```
// color_pipeline.c -- the companion app's live-preview color math.
//
// This is NOT a reimplementation. Every function below (the gamma
// LUTs, the fast log2/exp2 approximation, the per-channel gamma LUT
// builder, and applyBrightness/ChannelBrightnessPct/Contrast/
// Saturation/Levels/writeColorOrdered/applyColorProcessing) is copied
// verbatim from ps4_ambient_light v2.2's own main.c, specifically so
// the preview this app shows is numerically identical to what the
// plugin actually produces -- not an approximation that could quietly
// drift from the real pipeline as it evolves.
//
// If ps4_ambient_light's color pipeline changes again, re-sync this
// file from the plugin's main.c -- don't hand-edit it independently.
//
// One deliberate adaptation for the preview use case: the real plugin
// applies per-channel gamma (gamma_r/g/b) PER SAMPLE PIXEL, before
// zone-averaging several samples together. There's no buffer of
// samples here, just one preview swatch color -- treating that single
// color as the one sampled pixel and applying per-channel gamma to it
// directly is the mathematically correct equivalent, not a
// simplification (averaging a single value is a no-op).
```

### companion-app/source/color_pipeline.c:175

In the code now:

```
// verbatim from ps4_ambient_light v2.2 main.c (fast log2/exp2 for the arbitrary-percentage per-channel...
// More: docs/code-comments.md (companion-app/source/color_pipeline.c:175)
```

Original (lines 175-177):

```
// ---- verbatim from ps4_ambient_light v2.2 main.c (fast log2/exp2 for
// the arbitrary-percentage per-channel gamma controls -- no libm
// linked in either project, see each file's own Makefile) ----
```

### companion-app/source/color_pipeline.c:228

In the code now:

```
// verbatim from ps4_ambient_light v2.2 main.c (the rest of the pipeline -- unclamped int32_t between...
// More: docs/code-comments.md (companion-app/source/color_pipeline.c:228)
```

Original (lines 228-233):

```
// ---- verbatim from ps4_ambient_light v2.2 main.c (the rest of the
// pipeline -- unclamped int32_t between every stage, one clamp+round
// at the very end in applyLevels, matching Android's real float
// pipeline exactly rather than the v2.0/v2.1 byte-clamped version --
// see the plugin's own extensive comments on why, reproduced there,
// not duplicated here) ----
```

### companion-app/source/color_pipeline.c:298

In the code now:

```
// Preview entry point. Applies per-channel gamma first (see the file header note on why that's correct...
// More: docs/code-comments.md (companion-app/source/color_pipeline.c:298)
```

Original (lines 298-302):

```
// Preview entry point. Applies per-channel gamma first (see the file
// header note on why that's correct for a single swatch color), then
// hands off to the exact same chain the real plugin runs per zone:
// gamma(legacy global LUT) -> brightness(global+per-channel) ->
// contrast -> saturation -> levels -> wire order.
```

## companion-app/source/config.c

### companion-app/source/config.c:105  (kept in code as a summary)

In the code now:

```
// Raw sceKernel* file calls only: libc stdio (fopen, FILE*) is not safe off the plugin-load thread and crashed.
// Keep every read and write here on raw syscalls. [docs/code-comments.md]
```

Original (lines 105-113):

```
// v2.2: reads via sceKernelOpen/sceKernelRead/sceKernelClose into a
// memory buffer instead of fopen()/fgetc(). This function is reachable
// from ambient_check_config_reload() on ambient_sample_thread (a worker
// thread spawned separately from plugin load), and libc stdio (FILE*,
// buffering, locks) isn't safe to touch off the main plugin-load thread
// on this toolchain -- that's what was crashing. Same raw-syscall
// approach game_patch/utils.cpp already uses successfully elsewhere in
// this repo. The parsing state machine below is unchanged; only the
// byte source changed, from fgetc(f) to indexing into filebuf.
```

### companion-app/source/config.c:126

In the code now:

```
// malloc(0) is legal-but-implementation-defined (may return NULL even on success), so a genuinely...
// More: docs/code-comments.md (companion-app/source/config.c:126)
```

Original (lines 126-128):

```
// malloc(0) is legal-but-implementation-defined (may return NULL even
// on success), so a genuinely empty file gets a 1-byte buffer that
// nothing ever reads into rather than being misread as a malloc failure.
```

### companion-app/source/config.c:247  (kept in code as a summary)

In the code now:

```
// Write path stays on raw sceKernel* calls (fopen crashed off the load thread): build in memory, one write,
// and return false unless the full length was written, so a failed save is reported. [docs/code-comments.md]
```

Original (lines 247-263):

```
// NOT fopen/fprintf/fclose -- ini_table_read_from_file() above was
// already rewritten off fopen for a documented reason (fopen from an
// unusual thread context isn't reliably safe in this environment,
// per that function's own comment). This write path never got the
// same fix, silently reintroducing that exact crash risk for every
// caller (settings_save(), register_plugin_in_goldhen()). Also fixes
// a second, separate bug: the old version always returned true once
// fopen succeeded, even if fprintf/fflush/fsync failed -- a disk-full
// write would report success while leaving a truncated/corrupt file,
// and the "Save FAILED" UI path built around this return value could
// never actually fire for that case.
//
// Builds the full output in memory first, then writes it in one
// sceKernelWrite call -- same O_TRUNC|O_CREAT flags and success check
// (return value compared against the full requested length) already
// used by http_download()/do_plugin_update() in main.c, so a failed
// write is reported as failure, not silently accepted.
```

## companion-app/source/ddp.c

### companion-app/source/ddp.c:1

In the code now:

```
// ddp.c -- copied/adapted from ps4_ambient_light's wled_send_rgb_zones.
// More: docs/code-comments.md (companion-app/source/ddp.c:1)
```

Original (lines 1-8):

```
// ddp.c -- copied/adapted from ps4_ambient_light's wled_send_rgb_zones.
// The packet header bytes (0x41, 0x00, 0x0B, 0x01, offset=0) are NOT
// arbitrary -- they're the exact DDP header this whole project has
// used since detile_verify_probe's very first capture, verified
// against real WLED behavior throughout. Copied here unchanged rather
// than re-derived from the DDP spec, same "reuse, don't reinvent"
// rule this project has followed since the Makefile/config.c reuse
// in the plugin itself.
```

## companion-app/source/file_util.c

### companion-app/source/file_util.c:1

In the code now:

```
// Small file helpers and checksums over the SDK's sceKernelOpen/Read/Write, split out of main.c. The...
// More: docs/code-comments.md (companion-app/source/file_util.c:1)
```

Original (lines 1-3):

```
// Small file helpers and checksums over the SDK's sceKernelOpen/Read/Write, split out of main.c.
// The code is as it ran on hardware (stat() and fopen() are not trusted on this SDK, see the note
// on http_download() in net_http.c).
```

### companion-app/source/file_util.c:66

In the code now:

```
// Hashes an on-disk file with the same buffered-read pattern used throughout this file...
// More: docs/code-comments.md (companion-app/source/file_util.c:66)
```

Original (lines 66-71):

```
// Hashes an on-disk file with the same buffered-read pattern used
// throughout this file (sceKernelOpen/Read/Close, not fopen -- see
// the note on http_download() in net_http.c for why). check_for_plugin_update()'s
// primary source of "what's actually installed" -- see
// PLUGIN_INSTALLED_CHECKSUM_PATH's comment for why the sidecar below
// is only a fallback, not the other way around.
```

### companion-app/source/file_util.c:97

In the code now:

```
// Reads the plain 64-hex-char PLUGIN_INSTALLED_CHECKSUM_PATH sidecar (no sha256sum-style " filename"...
// More: docs/code-comments.md (companion-app/source/file_util.c:97)
```

Original (lines 97-102):

```
// Reads the plain 64-hex-char PLUGIN_INSTALLED_CHECKSUM_PATH sidecar
// (no sha256sum-style " filename" suffix -- write_installed_checksum()
// below never writes one) into outHex. check_for_plugin_update()'s
// fallback, used only when sha256_hex_of_file() can't read the real
// .prx -- see PLUGIN_INSTALLED_CHECKSUM_PATH's comment for why this
// sidecar can't be trusted as the primary answer to "what's installed."
```

### companion-app/source/file_util.c:115

In the code now:

```
// Writes actualHex -- the plain, already-verified digest computed in do_plugin_update() -- to...
// More: docs/code-comments.md (companion-app/source/file_util.c:115)
```

Original (lines 115-120):

```
// Writes actualHex -- the plain, already-verified digest computed in
// do_plugin_update() -- to PLUGIN_INSTALLED_CHECKSUM_PATH. Best-
// effort: a failed write just means the next update check falls back
// to sha256_hex_of_file() on the .prx itself, so this doesn't affect
// the install's own success/failure and callers don't check its
// return value.
```

## companion-app/source/fw_check.c

### companion-app/source/fw_check.c:1

In the code now:

```
// fw_check.c -- reads the console firmware once at startup and adds a notice when the current...
// More: docs/code-comments.md (companion-app/source/fw_check.c:1)
```

Original (lines 1-5):

```
// fw_check.c -- reads the console firmware once at startup and adds a notice when the current
// standalone shell-feed loader is untested on that version, or has no entry for it (it will most
// likely refuse). The notice never blocks anything; the payload decides at runtime. The old loader's
// offset table no longer describes the shipped payload.
// The game plugin's firmware behavior is separate from the shell-feed loader.
```

## companion-app/source/help_qr.c

### companion-app/source/help_qr.c:1

In the code now:

```
// help_qr.c -- wraps qrcodegen.c (vendored, see its own header) into the one thing ui_screens.c's QR...
// More: docs/code-comments.md (companion-app/source/help_qr.c:1)
```

Original (lines 1-6):

```
// help_qr.c -- wraps qrcodegen.c (vendored, see its own header) into
// the one thing ui_screens.c's QR modal actually needs: "is module
// (x, y) dark, in a grid that already includes the quiet-zone
// border". Everything else -- picking an ECC level, a version range,
// a mask, the quiet zone width -- is decided once, here, so the
// renderer never has to know qrcodegen's API at all.
```

### companion-app/source/help_qr.c:13

In the code now:

```
// Standard-recommended quiet zone is 4 modules; anything less risks a scanner that expects full margin...
// More: docs/code-comments.md (companion-app/source/help_qr.c:13)
```

Original (lines 13-16):

```
// Standard-recommended quiet zone is 4 modules; anything less risks a
// scanner that expects full margin failing to lock onto the finder
// patterns, especially photographed off a TV/monitor rather than
// printed.
```

### companion-app/source/help_qr.c:28

In the code now:

```
// MEDIUM: matches this app's own on-console rendering (modules land on exact pixel-aligned rects, no...
// More: docs/code-comments.md (companion-app/source/help_qr.c:28)
```

Original (lines 28-33):

```
// MEDIUM: matches this app's own on-console rendering (modules
// land on exact pixel-aligned rects, no photographic blur to fight,
// so LOW would scan fine too) while still tolerating a stray dead
// pixel or two and a phone camera at a shallow angle. boostEcl=true
// lets qrcodegen raise this further for free if the fixed version
// range below has spare capacity at MEDIUM already.
```

## companion-app/source/ime.c

### companion-app/source/ime.c:1

In the code now:

```
// On-screen keyboard for the settings screens' numeric and string rows.
// More: docs/code-comments.md (companion-app/source/ime.c:1)
```

Original (lines 1-3):

```
// On-screen keyboard for the settings screens' numeric and string rows. See ime.h for the flow.
// Split out of main.c; the code is otherwise as it ran on hardware, with main.c's globals
// replaced by the ImeHost the caller passes in.
```

### companion-app/source/ime.c:16

In the code now:

```
// Real API surface pulled from orbis/ImeDialog.h, orbis/_types/ ime_dialog.h and orbis/CommonDialog.h...
// More: docs/code-comments.md (companion-app/source/ime.c:16)
```

Original (lines 16-22):

```
// Real API surface pulled from orbis/ImeDialog.h, orbis/_types/
// ime_dialog.h and orbis/CommonDialog.h -- see the extensive
// verification notes this was ported from for exactly what's
// confirmed vs. assumed (posx/posy units, whether NULL is a valid
// second sceImeDialogInit argument, whether ORBIS_TYPE_NUMBER's layout
// includes '-' for negative fields). None of that re-verified this
// session; ported as-is.
```

### companion-app/source/ime.c:35

In the code now:

```
// itemIndex must refer to a FIELD_STRING, FIELD_U32, FIELD_I32, or FIELD_U16 item...
// More: docs/code-comments.md (companion-app/source/ime.c:35)
```

Original (lines 35-37):

```
// itemIndex must refer to a FIELD_STRING, FIELD_U32, FIELD_I32, or
// FIELD_U16 item -- FIELD_ENUM/FIELD_BOOL are cycled/toggled directly
// by Cross instead (see handle_settings_input()).
```

### companion-app/source/ime.c:128

In the code now:

```
// strtol, not atoi: atoi can't distinguish "not a number" from a real 0, and silently writing 0 for...
// More: docs/code-comments.md (companion-app/source/ime.c:128)
```

Original (lines 128-132):

```
// strtol, not atoi: atoi can't distinguish "not a number"
// from a real 0, and silently writing 0 for garbage input
// is exactly the kind of accepted-but-wrong save
// settings_load()'s own hardening was written to prevent
// on the load side.
```

### companion-app/source/ime.c:139

In the code now:

```
// What was typed is in DISPLAY units (a percent for the percent-style rows); settings_from_display...
// More: docs/code-comments.md (companion-app/source/ime.c:139)
```

Original (lines 139-141):

```
// What was typed is in DISPLAY units (a percent for the
// percent-style rows); settings_from_display converts and
// clamps into [item->min, item->max].
```

## companion-app/source/layout.c

### companion-app/source/layout.c:1

In the code now:

```
// layout.c -- see layout.h. Ported from LedLayoutGeometry.kt (calculateLedPositions + getEdgeOrder)...
// More: docs/code-comments.md (companion-app/source/layout.c:1)
```

Original (lines 1-4):

```
// layout.c -- see layout.h. Ported from LedLayoutGeometry.kt
// (calculateLedPositions + getEdgeOrder), kept as close to the
// original edge-by-edge structure as C allows so the two can still be
// compared side by side if the Android layout logic ever changes.
```

### companion-app/source/layout.c:12

In the code now:

```
// Direct port of getEdgeOrder(): which side is walked first, and in which rotational direction, for...
// More: docs/code-comments.md (companion-app/source/layout.c:12)
```

Original (lines 12-15):

```
// Direct port of getEdgeOrder(): which side is walked first, and in
// which rotational direction, for each of the 4 physical start
// corners x 2 directions. Order here IS wire order -- pixel 0 on the
// real strip is the first LED of the first edge listed.
```

### companion-app/source/layout.c:135

In the code now:

```
// ledOffset rotation -- same as the Android preview's own "Apply same offset in visualization so the...
// More: docs/code-comments.md (companion-app/source/layout.c:135)
```

Original (lines 135-140):

```
// ledOffset rotation -- same as the Android preview's own
// "Apply same offset in visualization so the numbering matches"
// step, so outSlots[0] always corresponds to the pixel the real
// strip's controller treats as index 0, even if the user's strip
// physically starts partway around the frame from their chosen
// startCorner.
```

## companion-app/source/led_frame.c

### companion-app/source/led_frame.c:12

In the code now:

```
// A muted, unmistakably-not-real-data placeholder for when there's no color to show yet (no...
// More: docs/code-comments.md (companion-app/source/led_frame.c:12)
```

Original (lines 12-15):

```
// A muted, unmistakably-not-real-data placeholder for when there's no
// color to show yet (no rgbTriplets, or fewer than this LED's index).
// Deliberately desaturated so it can't be confused with an actual
// sent color, which is the whole point of this rewrite.
```

### companion-app/source/led_frame.c:31

In the code now:

```
// LED 0 -- the wire's physical start -- still gets its own real colour, with a small amber ring...
// More: docs/code-comments.md (companion-app/source/led_frame.c:31)
```

Original (lines 31-36):

```
// LED 0 -- the wire's physical start -- still gets its own
// real colour, with a small amber ring overlaid to mark it
// as the origin (not a substitute for showing its color).
// Only one of these is ever drawn per frame, so it stays
// on the full AA path -- no performance reason to fast-
// path something that happens once.
```

### companion-app/source/led_frame.c:43

In the code now:

```
// Every other tick uses the fast, unantialiased fill -- there can be several hundred of these drawn...
// More: docs/code-comments.md (companion-app/source/led_frame.c:43)
```

Original (lines 43-50):

```
// Every other tick uses the fast, unantialiased fill --
// there can be several hundred of these drawn every single
// animation frame while Test Strip is running, and at 7px
// across the missing edge AA was never going to be visible
// anyway. This was the single biggest per-frame cost left
// after caching the scene background, since the full AA
// path builds a small polygon and runs a 5x-subsampled
// scanline pass for every tick, hundreds of times a frame.
```

## companion-app/source/led_frame.h

### companion-app/source/led_frame.h:1

In the code now:

```
// led_frame.h -- the blueprint's #led-frame, showing what's actually being sent to the strip.
// More: docs/code-comments.md (companion-app/source/led_frame.h:1)
```

Original (lines 1-16):

```
// led_frame.h -- the blueprint's #led-frame, showing what's actually
// being sent to the strip.
//
// One tick per physical LED, in real wire order, coloured with the
// literal bytes main.c is sending over DDP for that LED -- not a
// decorative approximation.
//
// Positions are passed in rather than computed here. An earlier
// version called layout_build() itself to get them, which meant every
// animation frame ran layout_build() twice -- once in main.c to get
// the positions used for the actual DDP send, and again in here,
// purely to re-derive the same positions a second time for drawing.
// Harmless for correctness (both calls agree, since they're driven by
// the same cfg), but wasted work on every one of the ~30 frames a
// second Test Strip's animation draws. main.c already has the
// positions it just computed; this just uses them.
```

### companion-app/source/led_frame.h:25

In the code now:

```
// slots/slotCount are the exact LedSlot array and count layout_build() produced -- the same one used...
// More: docs/code-comments.md (companion-app/source/led_frame.h:25)
```

Original (lines 25-36):

```
// slots/slotCount are the exact LedSlot array and count layout_build()
// produced -- the same one used to build the DDP frame, so tick i and
// rgbTriplets[i] always refer to the same physical LED, by
// construction, with no second position calculator that could drift
// out of sync with the first.
//
// rgbTriplets is ledCount*3 bytes, one RGB triplet per LED in that
// same order -- rgbTriplets[i*3 .. i*3+2] must be the same bytes sent
// over DDP for LED i. Pass NULL/0 for rgbTriplets/ledCount when
// nothing has been sent yet (or sending failed): every tick then draws
// as a dim, uncoloured placeholder rather than inventing a color that
// was never actually sent.
```

## companion-app/source/main.c

### companion-app/source/main.c:1

In the code now:

```
// main.c -- ps4_ambient_light companion app.
// More: docs/code-comments.md (companion-app/source/main.c:1)
```

Original (lines 1-32):

```
// main.c -- ps4_ambient_light companion app.
//
// This build wires the visual layer (ui_*.c) up to real, working
// logic: every field on Set up/Customization reads and writes the
// live AmbientConfig through settings.c's generic get/set-by-offset,
// numeric/string fields open the real on-screen keyboard, Save writes
// the ini the plugin itself reads, the CTA installs or updates the
// real plugin .prx over HTTP, and Set up/Customization/Test strip all
// drive the real WLED strip over DDP using the same wire-order layout
// math the Android app uses.
//
// Almost none of this logic is new. It's a port of a previously
// hardened implementation (see the comments throughout this file that
// say what was actually confirmed, and the couple of real bugs that
// were found and fixed there -- the IME-dialog sysmodule load and the
// DDP offset-0 bug are both explained where they're fixed below) onto
// the new visual layer, not a rewrite from scratch. What changed is
// how a field gets drawn and where its on-screen position comes from
// (ui_screens.c, driven by settings.c's kMenuItems); what a field
// means and how editing it is saved has not.
//
// SDL's role stays deliberately small: create the window, hand over
// its surface, and present it. All drawing goes through UiCanvas.
//
// NOT YET VERIFIED: this file has not been compiled with the real
// OpenOrbis toolchain or run on a PS4. Two interaction choices below
// are new conventions introduced here (not carried over from prior
// art) and are flagged where they're implemented: L1/R1 as a quick
// +-step nudge on the focused field, and Cross as the single
// "activate this field" button for every field type. Both are
// plausible, standard console-UI choices, but neither was exercised
// by the version of this app that was actually run on hardware.
```

### companion-app/source/main.c:86

In the code now:

```
// TCP headers for GoldHEN's BinLoader (port 9090).
// More: docs/code-comments.md (companion-app/source/main.c:86)
```

Original (lines 86-89):

```
// TCP headers for GoldHEN's BinLoader (port 9090). On PS4 these are the same
// POSIX socket headers the SDK provides; they're already available here through
// the orbis libc. sys/socket.h, netinet/in.h and unistd.h are all part of the
// standard sysroot included by the OpenOrbis toolchain.
```

### companion-app/source/main.c:107

In the code now:

```
// [shell] enabled as it is in the ini on disk (set at startup and after every successful save).
// More: docs/code-comments.md (companion-app/source/main.c:107)
```

Original (lines 107-110):

```
// [shell] enabled as it is in the ini on disk (set at startup and after every successful save).
// The shell feed reads that file, so it keeps running until the OFF is saved: the strip claim
// below has to follow this, not just the unsaved switch in Set up, or turning the switch off
// releases the claim while the shell feed is still sending and the strip flickers until Save.
```

### companion-app/source/main.c:113

In the code now:

```
// True while the PS button has put this app in the background.
// More: docs/code-comments.md (companion-app/source/main.c:113)
```

Original (lines 113-115):

```
// True while the PS button has put this app in the background. While it is, the app sends nothing to
// the strip and drops its claim, so the shell feed / a game / WLED's own source can have it back.
// Polled from sceSystemServiceGetStatus (see poll_background_state()).
```

### companion-app/source/main.c:118

In the code now:

```
// Set up / Customization have no status line (only Home draws it), so a hint for the row you just...
// More: docs/code-comments.md (companion-app/source/main.c:118)
```

Original (lines 118-120):

```
// Set up / Customization have no status line (only Home draws it), so a hint for the row you just
// pressed goes in g_state.settingsHint, which the save bar shows in place of "Changes apply after
// saving." It is cleared by the next button press on that screen.
```

### companion-app/source/main.c:122

In the code now:

```
// Held so do_plugin_update() can force an interim "Downloading..." frame before its blocking...
// More: docs/code-comments.md (companion-app/source/main.c:122)
```

Original (lines 122-125):

```
// Held so do_plugin_update() can force an interim "Downloading..."
// frame before its blocking network/file I/O -- same reasoning as the
// version of this that ran on hardware: without it, a slow server
// makes the app look completely frozen with zero feedback.
```

### companion-app/source/main.c:140

In the code now:

```
// Cached snapshot of ui_render_home_static()'s output -- everything on Home except the LED edge frame.
// More: docs/code-comments.md (companion-app/source/main.c:140)
```

Original (lines 140-146):

```
// Cached snapshot of ui_render_home_static()'s output -- everything on
// Home except the LED edge frame. Rebuilt every time render_and_present()
// actually runs the full Home draw; reused by render_home_fast() on
// every animation-only frame in between, which is what turns "redraw
// all of Home's text/icons/background at 30fps" into "redraw ~200
// small squares at 30fps". Allocated once in main() once the frame
// size is known.
```

### companion-app/source/main.c:153

In the code now:

```
// Drawn in two pieces (not ui_render()) specifically so the static part can be snapshotted before the...
// More: docs/code-comments.md (companion-app/source/main.c:153)
```

Original (lines 153-155):

```
// Drawn in two pieces (not ui_render()) specifically so the
// static part can be snapshotted before the LED frame goes on
// top of it -- see ui_render_home_static()'s own comment.
```

### companion-app/source/main.c:167

In the code now:

```
// Left Home -- rebuild the cache from scratch next time we're back rather than trust a snapshot from a...
// More: docs/code-comments.md (companion-app/source/main.c:167)
```

Original (lines 167-169):

```
// Left Home -- rebuild the cache from scratch next time we're
// back rather than trust a snapshot from a potentially
// different install/toast/focus state.
```

### companion-app/source/main.c:176

In the code now:

```
// The actual fast path: reuse the cached static layer, redraw only the (now cheap -- see led_frame.c)...
// More: docs/code-comments.md (companion-app/source/main.c:176)
```

Original (lines 176-179):

```
// The actual fast path: reuse the cached static layer, redraw only the
// (now cheap -- see led_frame.c) LED frame on top of it. Only valid to
// call when g_homeStaticCacheValid and we're still on Home; the main
// loop below is the only caller and it checks both.
```

### companion-app/source/main.c:190

In the code now:

```
// button auto-repeat Holding a D-Pad direction or L1/R1 now fires repeatedly instead of once per press...
// More: docs/code-comments.md (companion-app/source/main.c:190)
```

Original (lines 190-205):

```
// ---------------- button auto-repeat ----------------
//
// Holding a D-Pad direction or L1/R1 now fires repeatedly instead of
// once per press -- moving focus, or nudging a numeric field, used to
// need one separate press per step, which made anything more than a
// couple of steps tedious (walking Brightness from 0 to 255 five at a
// time is 51 individual presses). Fires immediately on the initial
// press, waits REPEAT_INITIAL_DELAY_MS before the first repeat (so a
// quick tap never double-fires), then repeats every
// REPEAT_INTERVAL_MS for as long as the button stays held.
//
// Only applied to the 6 buttons where repeated firing is actually
// useful (4 D-Pad directions + L1/R1). Cross, Circle and Options stay
// edge-only (PRESSED()/crossUp below) -- those open a dialog, navigate
// once, or quit, and firing those repeatedly while held would be
// actively wrong (e.g. re-opening the keyboard every 90ms).
```

### companion-app/source/main.c:229

In the code now:

```
// field navigation Row structure comes from ui_screen_field_rows() (ui_screens.c), which is the exact...
// More: docs/code-comments.md (companion-app/source/main.c:229)
```

Original (lines 229-235):

```
// ---------------- field navigation ----------------
//
// Row structure comes from ui_screen_field_rows() (ui_screens.c),
// which is the exact same source the card renderer uses -- navigation
// and drawing can't disagree about where a field is, because they're
// built from the same table.
// built from the same table.
```

### companion-app/source/main.c:263

In the code now:

```
// Keeps the focused row inside the .content viewport, the way the browser's scroll-into-view would.
// More: docs/code-comments.md (companion-app/source/main.c:263)
```

Original (lines 263-266):

```
// Keeps the focused row inside the .content viewport, the way the
// browser's scroll-into-view would. Reaching the Save button scrolls
// all the way to the bottom, since that button lives in the fixed
// save bar below the scrolling content, not in a row within it.
```

### companion-app/source/main.c:307

In the code now:

```
// Home button: pending actions The work itself lives in plugin_install.c and shell_feed.c; the...
// More: docs/code-comments.md (companion-app/source/main.c:307)
```

Original (lines 307-310):

```
// ---------------- Home button: pending actions ----------------
//
// The work itself lives in plugin_install.c and shell_feed.c; the downloads and the release lookup
// in net_http.c.
```

### companion-app/source/main.c:314

In the code now:

```
// What the Home button does whenever it isn't Test strip: all pending work, plugin first (it is the...
// More: docs/code-comments.md (companion-app/source/main.c:314)
```

Original (lines 314-316):

```
// What the Home button does whenever it isn't Test strip: all pending work, plugin first (it is
// the part people notice is missing), then the shell feed files. Stops at the first failure so the
// status line keeps the reason.
```

### companion-app/source/main.c:319

In the code now:

```
// GoldHEN's plugin loader comes first: nothing below can work while it is off, and the app can't...
// More: docs/code-comments.md (companion-app/source/main.c:319)
```

Original (lines 319-321):

```
// GoldHEN's plugin loader comes first: nothing below can work while it is off, and the app can't
// change it (see the note above read_plugin_loader_state() in plugin_install.c). Close the app, the same save and quit
// as Options, so GoldHEN will let the person tick the switch.
```

### companion-app/source/main.c:339

In the code now:

```
// live preview / test strip Builds one color per *configured* LED in real physical wire order...
// More: docs/code-comments.md (companion-app/source/main.c:339)
```

Original (lines 339-347):

```
// ---------------- live preview / test strip ----------------
//
// Builds one color per *configured* LED in real physical wire order
// (layout_build -- layout.c, a C port of the Android app's
// LedLayoutGeometry.kt) and sends the whole strip over DDP, rather
// than a handful of fixed swatches at DDP offset 0 -- offset 0 is
// always the physical start of the strip, so a fixed-swatch version
// only ever lit whichever few pixels happen to sit right at the
// start, no matter how the LED counts were configured.
```

### companion-app/source/main.c:351

In the code now:

```
// Must outlive update_live_preview() -- UiState.ledColors (set below) borrows this pointer through to...
// More: docs/code-comments.md (companion-app/source/main.c:351)
```

Original (lines 351-354):

```
// Must outlive update_live_preview() -- UiState.ledColors (set below)
// borrows this pointer through to render_and_present(), which runs
// later in the same loop iteration; a stack-local buffer here would be
// a dangling pointer by the time ui_render() actually reads it.
```

### companion-app/source/main.c:357

In the code now:

```
// Advanced by real elapsed time (not an assumed frame duration) once per loop iteration in main().
// More: docs/code-comments.md (companion-app/source/main.c:357)
```

Original (lines 357-361):

```
// Advanced by real elapsed time (not an assumed frame duration) once
// per loop iteration in main(). Only actually used to offset colour
// while Test Strip is running (see update_live_preview()) -- letting
// it run continuously the rest of the time is harmless and simpler
// than gating the accumulation itself.
```

### companion-app/source/main.c:364

In the code now:

```
// Test Strip cycles between two visually distinct animation modes rather than running one pattern...
// More: docs/code-comments.md (companion-app/source/main.c:364)
```

Original (lines 364-380):

```
// Test Strip cycles between two visually distinct animation modes
// rather than running one pattern forever -- aesthetic choices, not
// something the blueprint specifies (it has no concept of a running
// preview at all).
//
// Phase 1, "rotating rainbow": the same per-position rainbow
// Set up/Customization show at rest, slowly rotated -- the whole
// strip is lit at once, in a fixed spatial arrangement that just
// turns. One revolution every 10 seconds, four revolutions per phase.
//
// Phase 2, "flowing wave": a single saturated band of colour travels
// around the joined perimeter with a fading trail behind it (the rest
// of the strip stays dim), and the band's own hue slowly cycles as it
// travels -- motion you can track with your eye, not just a static
// rainbow shape turning, which is the actual "variety" this is for:
// the two modes don't just differ in speed or colour, they differ in
// *shape* (everywhere-at-once vs. one moving point).
```

### companion-app/source/main.c:428

In the code now:

```
// Nothing configured -- nothing to send, and nothing real to show on the edge frame either.
// More: docs/code-comments.md (companion-app/source/main.c:428)
```

Original (lines 428-433):

```
// Nothing configured -- nothing to send, and nothing real to
// show on the edge frame either. Leaving a stale g_wireBytes
// pointer in g_state here would have the frame keep showing
// whatever was last computed under a *previous* config, which
// is exactly the "doesn't represent it perfectly" complaint
// this was fixed for -- so explicitly clear it.
```

### companion-app/source/main.c:441

In the code now:

```
// Set up/Customization show a *stable* per-position colour on purpose -- the point there is seeing...
// More: docs/code-comments.md (companion-app/source/main.c:441)
```

Original (lines 441-447):

```
// Set up/Customization show a *stable* per-position colour on
// purpose -- the point there is seeing which physical LED
// corresponds to which part of the screen, and a colour that kept
// moving would work against that. Test Strip on Home has the
// opposite goal (feel like the strip is alive, the way it will be
// once a game is actually running), so only that state gets any
// animation at all.
```

### companion-app/source/main.c:470

In the code now:

```
// Phase 2: a single travelling band with a fading trail. basePos reuses baseHue/360...
// More: docs/code-comments.md (companion-app/source/main.c:470)
```

Original (lines 470-475):

```
// Phase 2: a single travelling band with a fading trail.
// basePos reuses baseHue/360 -- layout_hue_for_slot()
// already maps position around the joined perimeter to
// 0-360, so dividing back by 360 recovers that same 0..1
// position without a second implementation of the same
// perimeter math.
```

### companion-app/source/main.c:491

In the code now:

```
// The edge frame shows these bytes regardless of whether the DDP send below actually reaches the strip...
// More: docs/code-comments.md (companion-app/source/main.c:491)
```

Original (lines 491-495):

```
// The edge frame shows these bytes regardless of whether the DDP
// send below actually reaches the strip -- they're an accurate
// preview of what this configuration *produces*, which stays
// meaningful even while WLED Host is unreachable (e.g. while
// tuning Customization settings offline).
```

### companion-app/source/main.c:503

In the code now:

```
// Only touch the status line on the transition into/out of failure -- this runs every frame, so...
// More: docs/code-comments.md (companion-app/source/main.c:503)
```

Original (lines 503-507):

```
// Only touch the status line on the transition into/out of
// failure -- this runs every frame, so without throttling, a bad
// WLED Host would permanently overwrite every other status
// message (save confirmations, update results) the instant it
// happened.
```

### companion-app/source/main.c:522

In the code now:

```
// on-screen keyboard Lives in ime.c. This is what it needs from here.
// More: docs/code-comments.md (companion-app/source/main.c:522)
```

Original (lines 522-524):

```
// ---------------- on-screen keyboard ----------------
//
// Lives in ime.c. This is what it needs from here.
```

### companion-app/source/main.c:546

In the code now:

```
// Anything pending (plugin or, with the Shell feed on, its files) takes the button; Test strip is only...
// More: docs/code-comments.md (companion-app/source/main.c:546)
```

Original (lines 546-548):

```
// Anything pending (plugin or, with the Shell feed on, its files) takes the button;
// Test strip is only offered once nothing is. With the Shell feed off this is exactly
// the old plugin-only rule.
```

### companion-app/source/main.c:589

In the code now:

```
// foreground / background The same call the plugin uses to notice a suspended game...
// More: docs/code-comments.md (companion-app/source/main.c:589)
```

Original (lines 589-596):

```
// ---------------- foreground / background ----------------
//
// The same call the plugin uses to notice a suspended game (plugin/source/sample_thread.c). The
// OpenOrbis header declares sceSystemServiceGetStatus() with no arguments, so it is bound here under
// another C name to the same symbol (the app already links -lSceSystemService). The struct is the
// plugin's AmbientSystemServiceStatus (field layout from shadPS4's source), padded well past the known
// fields in case the firmware writes more. Confirmed on a console (the PS button flips it). If the call
// ever fails or the field never flips, g_appBackgrounded just stays false and the app behaves as before.
```

### companion-app/source/main.c:627

In the code now:

```
// How long the Save button shows its "Saved!" confirmation state (ui_screens.c...
// More: docs/code-comments.md (companion-app/source/main.c:627)
```

Original (lines 627-629):

```
// How long the Save button shows its "Saved!" confirmation state
// (ui_screens.c: UiState.saveJustConfirmed) before reverting to
// normal. Long enough to register, short enough not to feel stuck.
```

### companion-app/source/main.c:651

In the code now:

```
// BUG FIX: setupComplete was only ever computed once, at startup from whatever settings_load() found...
// More: docs/code-comments.md (companion-app/source/main.c:651)
```

Original (lines 651-661):

```
// BUG FIX: setupComplete was only ever computed once, at
// startup from whatever settings_load() found on disk (see
// main()'s own copy of this same expression). Saving new
// values here updated g_cfg and the file on disk, but never
// this cached flag -- so Home's Test Strip kept reading the
// stale "not set up yet" state after a fresh Setup+Save,
// until the app was closed and reopened and main() recomputed
// it from scratch. Recomputing it here, right after a
// successful save, means Test Strip reflects reality
// immediately -- same formula as main()'s startup copy, kept
// in sync with it on purpose.
```

### companion-app/source/main.c:670

In the code now:

```
// Nudges/cycles/toggles the focused field by one step without opening the keyboard.
// More: docs/code-comments.md (companion-app/source/main.c:670)
```

Original (lines 670-672):

```
// Nudges/cycles/toggles the focused field by one step without opening
// the keyboard. New in this pass (see the file header) -- L1/R1 were
// never used by the version of this app that ran on hardware.
```

### companion-app/source/main.c:692

In the code now:

```
// "Launch Ambilight payload" is a FIELD_ACTION in the Shell feed card.
// More: docs/code-comments.md (companion-app/source/main.c:692)
```

Original (lines 692-695):

```
// "Launch Ambilight payload" is a FIELD_ACTION in the Shell feed card. L1/R1 nudge reaches FIELD_ACTION
// only from nudge_field() (handle_settings_input dispatches actions via cross separately);
// treat any nudge as a press since there is no meaningful direction for a send.
// The cross-press path is handled in handle_settings_input() below.
```

### companion-app/source/main.c:706

In the code now:

```
// Not a real field -- see settings.h/settings.c -- so it can't go through settings_get_i32/set_i32...
// More: docs/code-comments.md (companion-app/source/main.c:706)
```

Original (lines 706-708):

```
// Not a real field -- see settings.h/settings.c -- so it can't
// go through settings_get_i32/set_i32 below, which only know
// how to read/write a single struct member at item->offset.
```

### companion-app/source/main.c:728

In the code now:

```
// Step in the percent the person sees, then convert back -- so brightness moves 100% -> 95% -> 90%...
// More: docs/code-comments.md (companion-app/source/main.c:728)
```

Original (lines 728-731):

```
// Step in the percent the person sees, then convert back -- so
// brightness moves 100% -> 95% -> 90%, not 255 -> 250 -> 245.
// Only a value that's actually being edited goes through this;
// untouched settings keep their exact stored number.
```

### companion-app/source/main.c:784

In the code now:

```
// "Launch Ambilight payload" in the Shell feed card.
// More: docs/code-comments.md (companion-app/source/main.c:784)
```

Original (lines 784-786):

```
// "Launch Ambilight payload" in the Shell feed card. Guard the same way toggle_autoload()
// does: all three files must be present. This is the "send now" path -- no
// reboot needed, no GoldHEN menu required, works whether or not Autoload is on.
```

### companion-app/source/main.c:842

In the code now:

```
// Help screen input No fields here, so no move_focus_vertical()/scroll_to_focus() -- just a plain...
// More: docs/code-comments.md (companion-app/source/main.c:842)
```

Original (lines 842-846):

```
// ---------------- Help screen input ----------------
//
// No fields here, so no move_focus_vertical()/scroll_to_focus() --
// just a plain scroll, clamped the same way scroll_to_focus() clamps
// against ui_screen_content_height(), and Circle back to Home.
```

### companion-app/source/main.c:852

In the code now:

```
// While the QR popup is up, it owns Cross/Circle/Triangle entirely -- none of Help's own scrolling or...
// More: docs/code-comments.md (companion-app/source/main.c:852)
```

Original (lines 852-860):

```
// While the QR popup is up, it owns Cross/Circle/Triangle
// entirely -- none of Help's own scrolling or back-navigation
// should also fire underneath it on the same press. up/down are
// deliberately NOT swallowed here: scrolling the cards behind a
// popup that fully covers the .content viewport has no visible
// effect anyway, and letting the D-Pad repeat-fire logic keep
// running avoids a stale-press artifact the instant the popup
// closes (the same class of bug crossUp's whole edge-triggered
// design exists to avoid elsewhere in this file).
```

### companion-app/source/main.c:911

In the code now:

```
// Not SDL_INIT_JOYSTICK: input comes from the native scePad API below, and letting SDL's joystick...
// More: docs/code-comments.md (companion-app/source/main.c:911)
```

Original (lines 911-914):

```
// Not SDL_INIT_JOYSTICK: input comes from the native scePad API
// below, and letting SDL's joystick layer claim the same HID
// device was the cause of a real "highlight never moves" bug on
// hardware.
```

### companion-app/source/main.c:925

In the code now:

```
// This SDL port needs the window surface, not an accelerated renderer -- confirmed against OpenOrbis's...
// More: docs/code-comments.md (companion-app/source/main.c:925)
```

Original (lines 925-928):

```
// This SDL port needs the window surface, not an accelerated
// renderer -- confirmed against OpenOrbis's own samples/SDL2. The
// UI is composited into our own ARGB buffer and blitted here, so
// no SDL_Renderer is created at all.
```

### companion-app/source/main.c:938

In the code now:

```
// Not fatal -- render_and_present() and render_home_fast() both already check for NULL and simply skip...
// More: docs/code-comments.md (companion-app/source/main.c:938)
```

Original (lines 938-943):

```
// Not fatal -- render_and_present() and render_home_fast()
// both already check for NULL and simply skip the caching
// optimization if it's missing, falling back to a full redraw
// every frame (correct, just not fast). Worth knowing about if
// it happens, since it means Test Strip will be as sluggish as
// before this fix.
```

### companion-app/source/main.c:959  (kept in code as a summary)

In the code now:

```
// Load the ImeDialog and CommonDialog sysmodules at runtime before any sce*Dialog call;
// linking -lSceImeDialog alone crashes with PRX_NOT_RESOLVED_FUNCTION. [docs/code-comments.md]
```

Original (lines 959-972):

```
// Required before any sce*Dialog call. BUGFIX carried over from
// real hardware testing of the version this was ported from: the
// crash was PRX_NOT_RESOLVED_FUNCTION, Required Module Name
// libSceImeDialog, happening the instant Cross was pressed on a
// string field. Linking -lSceImeDialog resolves the symbol at
// link time; the real system .sprx module still has to be loaded
// into the running process at runtime before calls into it work --
// the same class of bug the earlier putty.log fix (the
// __ORBIS__ guard that silently dropped the FreeType sysmodule
// load) turned out to be. ORBIS_SYSMODULE_IME_DIALOG loads via the
// plain sceSysmoduleLoadModule (not the _Internal variant --
// that's a different enum family than Net/Http/Ssl/CommonDialog
// use); ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG loads via
// sceSysmoduleLoadModuleInternal, same as Net/Http/Ssl above.
```

### companion-app/source/main.c:982

In the code now:

```
// Encodes once, up front, rather than on first Triangle press -- qrcodegen's Reed-Solomon pass is...
// More: docs/code-comments.md (companion-app/source/main.c:982)
```

Original (lines 982-986):

```
// Encodes once, up front, rather than on first Triangle press --
// qrcodegen's Reed-Solomon pass is cheap but there's no reason to
// pay it on a frame that also has to redraw the whole modal for
// the first time. Logged, not fatal: render_qr_modal() falls back
// to a plain "visit this URL" panel if this never succeeds.
```

### companion-app/source/main.c:1005

In the code now:

```
// One-time, best-effort update check against the latest GitHub release, only when there's an installed...
// More: docs/code-comments.md (companion-app/source/main.c:1005)
```

Original (lines 1005-1020):

```
// One-time, best-effort update check against the latest GitHub
// release, only when there's an installed build to compare in the
// first place -- NONE/DISABLED already have their own CTA and
// take priority over "is there something newer". See
// check_for_plugin_update()'s comment for why a failure here
// (no network, GitHub unreachable, ...) is silent rather than
// surfaced as an error: this runs unprompted on every launch.
//
// Runs on a background thread (start_background_update_check(),
// polled every frame by poll_background_update_check() in the
// main loop below) rather than inline here -- this used to block
// the whole app behind two sequential GitHub round trips before
// the home screen ever became interactive, which was the actual
// source of "the app feels slow to open". Now the home screen
// renders and takes input immediately; the status line just
// updates in place once the check comes back.
```

### companion-app/source/main.c:1067

In the code now:

```
// While the on-screen keyboard is up, this app's own D-Pad-driven input has to sit out that frame so...
// More: docs/code-comments.md (companion-app/source/main.c:1067)
```

Original (lines 1067-1070):

```
// While the on-screen keyboard is up, this app's own
// D-Pad-driven input has to sit out that frame so it's
// not fighting the system dialog for the same presses
// underneath it.
```

### companion-app/source/main.c:1088

In the code now:

```
// Cross fires on release, not press: opening the keyboard on the press edge left a stale press for the...
// More: docs/code-comments.md (companion-app/source/main.c:1088)
```

Original (lines 1088-1092):

```
// Cross fires on release, not press: opening the keyboard
// on the press edge left a stale press for the IME dialog
// to consume as an immediate Cancel before any real input
// could register -- a real, previously-hit bug, not a
// hypothetical one.
```

### companion-app/source/main.c:1115

In the code now:

```
// wled-relay hand-off signal: this app has no idle/Home-only mode for this purpose -- "driving the...
// More: docs/code-comments.md (companion-app/source/main.c:1115)
```

Original (lines 1115-1130):

```
// wled-relay hand-off signal: this app has no idle/Home-only
// mode for this purpose -- "driving the strip" here just means
// "this app is running with Relay Signal enabled", independent
// of which screen is up or whether Test Strip is running. Fire
// only on a real transition (comparing against last frame's
// value), same as v17 had it -- covers app startup (already-
// enabled config fires "on" on the very first frame, since the
// sentinel below starts false), a live toggle, and nothing
// else. No internal opt-in gate inside
// relay_send_external_source() itself on purpose: the call
// site only ever fires on a real transition, so a user who
// never enables it can never produce one, and a gate that
// re-checked the same flag that just changed would silently
// swallow the "off" send on a true->false transition -- the
// exact bug this avoids by construction, same as the real
// plugin's own fix.
```

### companion-app/source/main.c:1137

In the code now:

```
// Set up/Customization always keep the live preview running, since their own LED edge frame is always...
// More: docs/code-comments.md (companion-app/source/main.c:1137)
```

Original (lines 1137-1143):

```
// Set up/Customization always keep the live preview running,
// since their own LED edge frame is always on screen there.
// Home only runs it -- and therefore only actually sends
// anything over DDP -- while Test Strip is active, so sitting
// on Home doesn't silently drive the real strip. Help has no
// LED edge frame either and nothing on screen a live preview
// would even show, so it's excluded the same way Home is.
```

### companion-app/source/main.c:1162

In the code now:

```
// While this app is sending to the strip (the same condition as above) and the Shell feed is on, claim...
// More: docs/code-comments.md (companion-app/source/main.c:1162)
```

Original (lines 1162-1165):

```
// While this app is sending to the strip (the same condition as above) and the Shell feed is
// on, claim it the way a game does, so the shell feed stays silent instead of fighting the
// live preview for WLED (which is what made the LEDs flicker). Off => never writes the file.
// Released (fg=0) as soon as the app stops sending or the switch is turned off.
```

### companion-app/source/main.c:1183

In the code now:

```
// Normally the app only redraws when something actually changed (g_dirty), which is what lets it sit...
// More: docs/code-comments.md (companion-app/source/main.c:1183)
```

Original (lines 1183-1190):

```
// Normally the app only redraws when something actually
// changed (g_dirty), which is what lets it sit idle between
// presses instead of burning CPU on an unchanging screen. Two
// things need a continuously *moving* picture with no input at
// all, though: the Test Strip animation, and the Save
// button's confirmation window counting down -- both need to
// keep redrawing every frame for their own duration, not just
// on the frame that started them.
```

### companion-app/source/main.c:1195

In the code now:

```
// Something actually changed (focus moved, install state flipped, a field was edited, ...) -- a full...
// More: docs/code-comments.md (companion-app/source/main.c:1195)
```

Original (lines 1195-1197):

```
// Something actually changed (focus moved, install state
// flipped, a field was edited, ...) -- a full redraw,
// which also refreshes the Home cache below reuses.
```

### companion-app/source/main.c:1201

In the code now:

```
// The only thing that changed this frame is the animation -- reuse the cached static layer and redraw...
// More: docs/code-comments.md (companion-app/source/main.c:1201)
```

Original (lines 1201-1209):

```
// The only thing that changed this frame is the
// animation -- reuse the cached static layer and redraw
// just the LED frame on top of it, instead of repainting
// all of Home again for content that's identical to last
// frame. This is the actual fix for Test Strip feeling
// sluggish: previously every one of these ~30-times-a-
// second frames repainted the background glow, the
// header, the banner, the CTA, the nav tiles and the
// footer, none of which had changed at all.
```

### companion-app/source/main.c:1212

In the code now:

```
// Continuous redraw is needed but the fast path isn't available yet (e.g. the very first animation...
// More: docs/code-comments.md (companion-app/source/main.c:1212)
```

Original (lines 1212-1215):

```
// Continuous redraw is needed but the fast path isn't
// available yet (e.g. the very first animation frame,
// before the cache exists) -- fall back to a full render,
// which populates the cache for every subsequent frame.
```

### companion-app/source/main.c:1222

In the code now:

```
// BUG FIX: closing the app (Options button or SDL_QUIT) previously went straight to teardown, so any...
// More: docs/code-comments.md (companion-app/source/main.c:1222)
```

Original (lines 1222-1231):

```
// BUG FIX: closing the app (Options button or SDL_QUIT) previously
// went straight to teardown, so any in-memory g_cfg edits that
// hadn't gone through an explicit Setup/Customization Save press
// were lost -- silently, since the crash on close (see below) made
// it hard to even notice there'd been a chance to save at all.
// Flushing here guarantees whatever's currently in g_cfg is on
// disk before the process goes away, the same call do_save() makes
// from the Save button, just without that function's UI feedback
// (status line / "Saved!" confirmation) since the app is already
// on its way out.
```

### companion-app/source/main.c:1240

In the code now:

```
// Defensive final "off" if the signal was last left on -- this app has no plugin_unload-style...
// More: docs/code-comments.md (companion-app/source/main.c:1240)
```

Original (lines 1240-1242):

```
// Defensive final "off" if the signal was last left on -- this app
// has no plugin_unload-style guaranteed teardown hook the way the
// real plugin does, so this is the closest equivalent (same as v17).
```

### companion-app/source/main.c:1253  (kept in code as a summary)

In the code now:

```
// Exit through sceSystemServiceLoadExec("exit"), never by returning from main():
// returning crashed on close (SIGSYS inside libkernel). [docs/code-comments.md]
```

Original (lines 1253-1274):

```
// BUG FIX (the crash-on-close from a real-hardware putty.log
// capture): this used to end with a plain `return 0`, letting the
// process fall off the end of main() and back into libkernel's own
// process-exit path. On PS4 homebrew that's not a sanctioned way
// to end a "big app" launched via the system's own LoadExec --
// checked this against how two other real, shipped homebrew apps
// close, since neither crashes on exit:
//   - Apollo Save Tool (bucanero/apollo-ps4, source/main.c):
//     registers a terminate() via atexit() whose whole job is
//     `sceSystemServiceLoadExec("exit", NULL)`, to hand control
//     back to the XMB properly instead of just returning.
//   - ItemzFlow (LightningMods/Itemzflow, source/sig_handler.cpp,
//     Exit_Success()): same call, `sceSystemServiceLoadExec("exit", 0)`,
//     as the last thing it does before the process goes away, on
//     both the normal-exit and crash-recovery paths.
// Matched this app's own putty.log crash signature too: signal 12
// (SIGSYS, "bad syscall") on the eboot.bin main thread, general-
// purpose registers zeroed, rip sitting inside libkernel.sprx --
// consistent with returning out of main() into a kernel trampoline
// that isn't a valid re-entry point for this process, rather than
// any crash in this app's own settings/save code (which is why it
// reproduced on every close, saved settings or not).
```

### companion-app/source/main.c:1277

In the code now:

```
// Not expected to be reached -- LoadExec("exit", ...) hands control back to the system and doesn't...
// More: docs/code-comments.md (companion-app/source/main.c:1277)
```

Original (lines 1277-1281):

```
// Not expected to be reached -- LoadExec("exit", ...) hands control
// back to the system and doesn't return here on a real console.
// Kept only so this still compiles/links as a normal C `main` and
// behaves sanely (e.g. under a host-side syntax check) if it ever
// somehow does return.
```

## companion-app/source/net_http.c

### companion-app/source/net_http.c:15

In the code now:

```
// HTTP downloads and the release lookup.
// More: docs/code-comments.md (companion-app/source/net_http.c:15)
```

Original (lines 15-21):

```
// HTTP downloads and the release lookup. See net_http.h.
//
// Verified real APIs, not guessed: SDK's samples/net_http/main.c is a
// real, working https://www.google.com download demo this sequence is
// copied from -- sceSysmoduleLoadModuleInternal for NET/HTTP/SSL,
// sceNetInit/PoolCreate, sceSslInit, sceHttpInit, then the
// template/connection/request chain below.
```

### companion-app/source/net_http.c:56

In the code now:

```
// Downloads full_url to local_dst.
// More: docs/code-comments.md (companion-app/source/net_http.c:56)
```

Original (lines 56-60):

```
// Downloads full_url to local_dst. Returns true on a clean 200 +
// complete read. If outDigest is non-NULL, it's filled with the
// SHA-256 of exactly the bytes written to local_dst, computed
// incrementally as each chunk arrives -- no separate re-read of the
// file afterwards.
```

### companion-app/source/net_http.c:70

In the code now:

```
// Follows the small-header 302 from an api.github.com asset URL (or, previously, straight from...
// More: docs/code-comments.md (companion-app/source/net_http.c:70)
```

Original (lines 70-75):

```
// Follows the small-header 302 from an api.github.com asset URL
// (or, previously, straight from github.com's web app -- see the
// BUG FIX comment on GITHUB_RELEASES_LATEST_API_URL in
// net_http.h for why that direct route no longer works) on to
// the signed objects.githubusercontent.com URL the asset actually
// lives at.
```

### companion-app/source/net_http.c:78

In the code now:

```
// Bounds how long a hung/unreachable host can freeze the app.
// More: docs/code-comments.md (companion-app/source/net_http.c:78)
```

Original (lines 78-82):

```
// Bounds how long a hung/unreachable host can freeze the app.
// Confirmed real 2-arg signatures; NOT confirmed against a
// working call site in this SDK (no sample calls these), so treat
// as a real, compilable improvement over no timeout at all, not a
// confirmed-correct one.
```

### companion-app/source/net_http.c:96

In the code now:

```
// NOT YET VERIFIED (same status as sceHttpSetConnectTimeOut et al. above): sceHttpAddRequestHeader's...
// More: docs/code-comments.md (companion-app/source/net_http.c:96)
```

Original (lines 96-104):

```
// NOT YET VERIFIED (same status as sceHttpSetConnectTimeOut
// et al. above): sceHttpAddRequestHeader's signature is
// taken from the usual OpenOrbis/orbis-sdk shape
// (id, name, value, mode) with mode=1 meaning "overwrite
// any existing value for this header" -- not exercised
// against a real call site in this SDK snapshot. This is
// what asks the GitHub asset API for the raw asset (a
// small-header 302 to objects.githubusercontent.com)
// instead of its default JSON metadata body.
```

### companion-app/source/net_http.c:111

In the code now:

```
// sceKernelOpen/Write/Close, not fopen -- fopen from an unusual thread context is not reliably safe in...
// More: docs/code-comments.md (companion-app/source/net_http.c:111)
```

Original (lines 111-114):

```
// sceKernelOpen/Write/Close, not fopen -- fopen
// from an unusual thread context is not reliably
// safe in this environment (same fix the plugin
// itself needed).
```

### companion-app/source/net_http.c:141

In the code now:

```
// Downloads full_url straight into outBuf (NUL-terminated on success), for small text responses...
// More: docs/code-comments.md (companion-app/source/net_http.c:141)
```

Original (lines 141-151):

```
// Downloads full_url straight into outBuf (NUL-terminated on success),
// for small text responses -- just the checksum sidecar file, which
// is a couple dozen bytes. Same template/connection/request machinery
// as http_download() above, just written into memory instead of a
// file since there's no reason to touch the filesystem for this much
// data. Returns false on any HTTP failure OR if the response would
// overflow outBufSize (including the NUL) -- a checksum file should
// never be anywhere close to outBufSize, so an overflow here almost
// certainly means the URL served something else (an HTML error page,
// a redirect-to-login, etc.), which must not be silently truncated
// and treated as a hash.
```

### companion-app/source/net_http.c:188

In the code now:

```
// Buffer's full but the server might still have more to send -- that's the overflow case described...
// More: docs/code-comments.md (companion-app/source/net_http.c:188)
```

Original (lines 188-191):

```
// Buffer's full but the server might still
// have more to send -- that's the overflow
// case described above, so treat it as a
// failure rather than guess we got it all.
```

### companion-app/source/net_http.c:209

In the code now:

```
// First hop of the api.github.com fix described on GITHUB_RELEASES_LATEST_API_URL in net_http.h...
// More: docs/code-comments.md (companion-app/source/net_http.c:209)
```

Original (lines 209-214):

```
// First hop of the api.github.com fix described on
// GITHUB_RELEASES_LATEST_API_URL in net_http.h: fetches
// the latest release's metadata and resolves assetName to that
// asset's own small api.github.com URL, ready to be GETted with
// Accept: application/octet-stream by http_download()/
// http_download_text() to reach the real bytes.
```

### companion-app/source/net_http.c:217

In the code now:

```
// Release JSON is dominated by the (markdown) release-notes body and can run to several KB -- generous...
// More: docs/code-comments.md (companion-app/source/net_http.c:217)
```

Original (lines 217-221):

```
// Release JSON is dominated by the (markdown) release-notes body
// and can run to several KB -- generous vs. the couple-dozen-byte
// checksum sidecar http_download_text() is more typically used
// for. Heap-allocated: too large to comfortably put on this
// thread's stack.
```

## companion-app/source/notify.c

### companion-app/source/notify.c:1

In the code now:

```
// notify.c -- see notify.h. The request layout is the one the plugin's Notify() sends (GoldHEN SDK's...
// More: docs/code-comments.md (companion-app/source/notify.c:1)
```

Original (lines 1-3):

```
// notify.c -- see notify.h. The request layout is the one the plugin's Notify() sends (GoldHEN SDK's
// OrbisNotificationRequest, from OSM's PS4-Notify); this app is a plain title, not a GoldHEN plugin, so it
// carries its own copy of the struct and calls the same libkernel function.
```

## companion-app/source/plugin_install.c

### companion-app/source/plugin_install.c:26

In the code now:

```
// Read-only scan of GoldHEN's plugins.ini for an exact, active (i.e. not commented-out) line matching...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:26)
```

Original (lines 26-35):

```
// Read-only scan of GoldHEN's plugins.ini for an exact, active (i.e.
// not commented-out) line matching PLUGIN_PRX_PATH -- same trimmed
// exact-match rule ensure_plugin_registered_in_goldhen() uses to avoid
// duplicate lines, factored out so the home screen's disabled-state
// check (plugin_registration_state() below) can reuse it without
// opening the file for write. A commented line (";path" or "#path",
// after trim) does NOT match the bare path, so a commented entry and
// a missing one both correctly read as "not registered" here -- the
// caller doesn't need to tell those two cases apart any further than
// that.
```

### companion-app/source/plugin_install.c:71

In the code now:

```
// Three-way install state for the home screen: not installed (no .prx on disk at all), installed but...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:71)
```

Original (lines 71-76):

```
// Three-way install state for the home screen: not installed (no
// .prx on disk at all), installed but disabled (.prx present, but
// plugins.ini's entry for it is commented out or missing), or
// installed and enabled. Doesn't distinguish "commented" from
// "missing" any further -- see plugin_registered_in_goldhen_ini()'s
// comment for why that's fine.
```

### companion-app/source/plugin_install.c:83

In the code now:

```
// Registers the plugin in GoldHEN's plugins.ini under [default] (loads for every title).
// More: docs/code-comments.md (companion-app/source/plugin_install.c:83)
```

Original (lines 83-114):

```
// Registers the plugin in GoldHEN's plugins.ini under [default]
// (loads for every title).
//
// This is deliberately NOT built on this file's own ini_table_s
// (settings_load/save's machinery) -- that's a generic parser meant
// for OUR OWN ini, where this app owns every key. plugins.ini is
// GoldHEN's file, shared with every other installed plugin, and uses
// a different dialect: bare-path lines with no "key = value" at all,
// plus ";"-prefixed comment lines GoldHEN itself ships as inert
// examples. Routing this through ini_table_read_from_file +
// ini_table_write_to_file was tried and empirically checked against a
// file shaped exactly like the one this app will actually find: it
// silently drops every comment line (ini_table_read_from_file's
// Comment state is dead code -- nothing ever transitions into it) and
// rewrites every bare-path entry as "path = ", corrupting even a
// pre-existing, unrelated, already-working plugin registration
// ([CUSA00411]'s entry in that test) into a form GoldHEN's own loader
// likely won't recognize. None of that is acceptable for a file this
// app doesn't own.
//
// So this instead only ever inserts one line, entirely by byte
// position -- find "[default]", find where its body ends (the next
// "[...]" header or EOF), insert our path there if it isn't already
// present anywhere under that section, and copy every other byte of
// the file through completely unchanged. If the file doesn't exist
// yet, it's created with just [default] and our own entry -- nothing
// else, since this app has no business guessing what other plugins a
// given user does or doesn't have installed. Verified against all of:
// no file yet, a file shaped like the one this app will actually find
// (comments + our section + an unrelated section with its own real
// entry), running it twice in a row (must not duplicate the line),
// and a file that has no [default] section at all yet.
```

### companion-app/source/plugin_install.c:163

In the code now:

```
// No existing file (or an empty one): create it with just [default] and our own entry.
// More: docs/code-comments.md (companion-app/source/plugin_install.c:163)
```

Original (lines 163-169):

```
// No existing file (or an empty one): create it with just
// [default] and our own entry. Earlier drafts of this
// function also wrote out several other plugins' filenames as
// commented-out examples -- those came from one particular
// user's own existing plugins.ini, not from anything GoldHEN
// itself ships or every install has, and had no business being
// hardcoded into a file this app creates for anyone else.
```

### companion-app/source/plugin_install.c:200

In the code now:

```
// Trim trailing blank lines from the body so the new line lands right after the last real line, not...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:200)
```

Original (lines 200-202):

```
// Trim trailing blank lines from the body so the new line
// lands right after the last real line, not after a run of
// blank ones.
```

### companion-app/source/plugin_install.c:210

In the code now:

```
// Keep a blank-line separator before the next section, if there is one -- matches the convention...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:210)
```

Original (lines 210-213):

```
// Keep a blank-line separator before the next section, if
// there is one -- matches the convention already visible
// in a real plugins.ini (a blank line between [default]
// and the next title's section).
```

### companion-app/source/plugin_install.c:239

In the code now:

```
// Downloads the latest plugin build and installs it.
// More: docs/code-comments.md (companion-app/source/plugin_install.c:239)
```

Original (lines 239-241):

```
// Downloads the latest plugin build and installs it. Used for both
// "Install plugin" (first run) and re-fetching a newer build once
// already installed.
```

### companion-app/source/plugin_install.c:248

In the code now:

```
// First hop: resolve both assets' own small api.github.com URLs from the release metadata, up front...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:248)
```

Original (lines 248-253):

```
// First hop: resolve both assets' own small api.github.com URLs
// from the release metadata, up front, before downloading
// anything -- so a stale/renamed asset fails fast with a clear
// status message instead of after the (much larger) .prx download.
// See the BUG FIX comment on GITHUB_RELEASES_LATEST_API_URL in net_http.h
// for why this replaces a direct GET of PLUGIN_UPDATE_URL.
```

### companion-app/source/plugin_install.c:269

In the code now:

```
// Second hop: Accept: application/octet-stream is what makes the GitHub asset API respond with a...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:269)
```

Original (lines 269-271):

```
// Second hop: Accept: application/octet-stream is what makes the
// GitHub asset API respond with a small-header redirect straight
// to the asset bytes instead of its default JSON metadata body.
```

### companion-app/source/plugin_install.c:285

In the code now:

```
// NOT YET VERIFIED (same status as the IME-dialog and L1/R1 conventions flagged at the top of this...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:285)
```

Original (lines 285-293):

```
// NOT YET VERIFIED (same status as the IME-dialog and L1/R1
// conventions flagged at the top of this file): sceKernelUnlink
// matches libkernel's usual POSIX-mirroring naming
// (sceKernelOpen/Read/Write/Close/Rename are all real and
// already used above/below), but no sample in this SDK
// snapshot was found that actually calls it. If it's missing
// at link time, the fallback is to just leave the .tmp file on
// disk -- harmless (it's overwritten by O_TRUNC next attempt)
// -- rather than something this function must have.
```

### companion-app/source/plugin_install.c:307

In the code now:

```
// Stage the new build alongside the live one, and only ever touch PLUGIN_PRX_PATH itself via...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:307)
```

Original (lines 307-310):

```
// Stage the new build alongside the live one, and only ever touch
// PLUGIN_PRX_PATH itself via sceKernelRename() once the staged
// copy is fully verified good -- a copy failure partway through
// never touches the live, working file.
```

### companion-app/source/plugin_install.c:314  (kept in code as a summary)

In the code now:

```
// Open the staging file with O_TRUNC (see plugin_common.h), or leftover bytes from an earlier attempt
// ride through the rename and break the SHA-256 match on every launch. [docs/code-comments.md]
```

Original (lines 314-324):

```
// BUG FIX: this used to be the bare literal "0x200 | 0x001" with
// no O_TRUNC at all (see plugin_common.h's BUG FIX comment) -- if
// stagingPath already had leftover bytes from an earlier update
// attempt and the new download was the same size or smaller, this
// open() didn't truncate them, so the copy loop below (which reads
// src until ITS eof, not stagingPath's old length) could carry
// that leftover tail through the rename into the live
// PLUGIN_PRX_PATH -- corrupting the install with extra trailing
// bytes that were never part of the SHA-256-verified download,
// which is exactly what made check_for_plugin_update() see a
// mismatch against GitHub's checksum on every subsequent launch.
```

### companion-app/source/plugin_install.c:336  (kept in code as a summary)

In the code now:

```
// fsync the staged copy before renaming it into place, and abort the install if fsync fails;
// otherwise a power loss can leave a rename over data that never reached disk. [docs/code-comments.md]
```

Original (lines 336-351):

```
// DURABILITY FIX: force the staged copy's data out of the OS
// write-back cache and onto physical storage BEFORE it's renamed
// into place. Without this, sceKernelWrite() above only guarantees
// the bytes are visible to reads within this same running OS
// instance -- nothing stopped a reboot (or GoldHEN/the console
// dropping power) between "Installed." and the plugin actually
// being reloaded from disk from silently reverting stagingPath's
// dirty pages, so the rename below could complete against data
// that was never actually committed. fsync() operates on the
// fd, same POSIX call already used (in a different file) on the
// libc FILE*-based path in config.c -- sceKernelOpen()'s fds are
// the same POSIX-compatible fds, so this works on the raw fd here
// too, no sceKernel-prefixed equivalent needed. Checked: a failed
// fsync means the data isn't safely on disk, so this must abort
// the install rather than rename an unverified-durable file into
// the live path.
```

### companion-app/source/plugin_install.c:364

In the code now:

```
// Same reasoning as above, applied to the rename itself: a rename is a directory-metadata change...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:364)
```

Original (lines 364-371):

```
// Same reasoning as above, applied to the rename itself: a rename
// is a directory-metadata change, which the OS can also hold in
// write-back cache rather than committing immediately. Re-opening
// PLUGIN_PRX_PATH here and fsyncing that fd (rather than trying to
// fsync stagingPath's now-stale fd, or a directory fd -- open()
// on a directory isn't reliably supported by this SDK's libkernel)
// pushes the renamed directory entry and its data out to disk
// before this function reports success.
```

### companion-app/source/plugin_install.c:393

In the code now:

```
// Re-enables a .prx that's already on disk but whose plugins.ini entry is commented out or missing...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:393)
```

Original (lines 393-396):

```
// Re-enables a .prx that's already on disk but whose plugins.ini entry
// is commented out or missing (UI_INSTALL_DISABLED) -- just rewrites
// the ini line locally via the same ensure_plugin_registered_in_goldhen()
// used after a fresh install/update, no network download needed.
```

### companion-app/source/plugin_install.c:407

In the code now:

```
// GoldHEN's plugin loader switch Settings > GoldHEN > Plugin Settings > Enable Plugins Loader is saved...
// More: docs/code-comments.md (companion-app/source/plugin_install.c:407)
```

Original (lines 407-415):

```
// ---------------- GoldHEN's plugin loader switch ----------------
//
// Settings > GoldHEN > Plugin Settings > Enable Plugins Loader is saved in /data/GoldHEN/config.ini
// ([PluginLoader] PluginLoader_Enabled, goldhen_config.h). With it off GoldHEN loads no plugin for
// any game, and for a long time Home couldn't tell. Now Home reads it and shows it ahead of every
// other check. The app only READS it: GoldHEN keeps the live value in memory and doesn't re-read
// the file (writing 1 left its menu showing the switch off), and it won't change the switch at all
// while a game or app is running, which includes this one. So the button just closes the app.
// Only a definite 0 counts as off: a missing file or key (an older GoldHEN) is UNKNOWN.
```

### companion-app/source/plugin_install.c:427

In the code now:

```
// Re-reads the switch and updates Home when it changed.
// More: docs/code-comments.md (companion-app/source/plugin_install.c:427)
```

Original (lines 427-429):

```
// Re-reads the switch and updates Home when it changed. Cheap (the file is about 1 KB), and the
// person can flip it in GoldHEN's own menu while this app sits in the background, so Home calls
// this every couple of seconds and right after coming back to the foreground.
```

## companion-app/source/presets.c

### companion-app/source/presets.c:41

In the code now:

```
// Shipped values. Anything not listed keeps the plugin default, which is deliberate for: letterbox...
// More: docs/code-comments.md (companion-app/source/presets.c:41)
```

Original (lines 41-58):

```
// Shipped values. Anything not listed keeps the plugin default, which is
// deliberate for: letterbox (on, 7% bar threshold -- already tuned), white
// level 100%, contrast 0 (its pivot sits on the post-gamma value, so any
// non-zero setting crushes darks), black threshold 0 (its fixed +10 LED-unit
// hysteresis blacks out dim scenes once gamma is on), and RGB balance /
// per-channel gamma 100% (they depend on the individual strip's white point,
// so they're set by eye).
//
//                       Game      Movie
//   edge depth           2 (5x5)   4 (9x9)   -- Movie has CPU to spare; Game runs inside the game's process
//   brightness           100%      80%       -- stored 255 / 204
//   saturation           +35%      +25%      -- Movie lower: skin tones dominate
//   gamma                2.2       2.4       -- game-mode vs cinema-mode display
//   black level          0%        1%        -- Movie: stops the strip toggling on grainy dark scenes
//   smoothing            on, 50ms  on, 200ms
//
// Gamma here stacks with WLED's own gamma if that's enabled for realtime
// data, so WLED's colour gamma should be off.
```

### companion-app/source/presets.c:150

In the code now:

```
// What settings_load left in cfg for the preset-owned fields: the old flat keys from an ini that...
// More: docs/code-comments.md (companion-app/source/presets.c:150)
```

Original (lines 150-152):

```
// What settings_load left in cfg for the preset-owned fields: the old flat
// keys from an ini that predates presets, or the plugin defaults if the
// file has none. Only used to migrate an old ini (below).
```

### companion-app/source/presets.c:178

In the code now:

```
// An existing, tuned setup: it becomes Game exactly as it is, and Movie starts as a copy of it with...
// More: docs/code-comments.md (companion-app/source/presets.c:178)
```

Original (lines 178-180):

```
// An existing, tuned setup: it becomes Game exactly as it is, and
// Movie starts as a copy of it with Movie's shipped smoothing -- so
// nobody loses their colour calibration by updating.
```

### companion-app/source/presets.c:191

In the code now:

```
// The ini has presets, and from here on the plugin reads ONLY those for these values (any flat...
// More: docs/code-comments.md (companion-app/source/presets.c:191)
```

Original (lines 191-193):

```
// The ini has presets, and from here on the plugin reads ONLY those for
// these values (any flat leftovers are ignored), so this must too. A
// preset section that's missing entirely starts from its shipped values.
```

## companion-app/source/relay_signal.c

### companion-app/source/relay_signal.c:1

In the code now:

```
// relay_signal.c -- same one-shot-UDP-datagram approach ddp.c already uses for the real strip data...
// More: docs/code-comments.md (companion-app/source/relay_signal.c:1)
```

Original (lines 1-6):

```
// relay_signal.c -- same one-shot-UDP-datagram approach ddp.c already
// uses for the real strip data, just a different (much smaller)
// payload and destination. Kept as its own file rather than folded
// into ddp.c since this isn't DDP at all -- no header, no zone data,
// just the bare ASCII text "on"/"off" -- and ddp.c's own DDP_HEADER_SIZE/
// DDP_MAX_ZONES constants are specific to that protocol, not this one.
```

## companion-app/source/settings.c

### companion-app/source/settings.c:1

In the code now:

```
// settings.c -- ini schema and defaults copied verbatim from ps4_ambient_light v2.2's own g_config...
// More: docs/code-comments.md (companion-app/source/settings.c:1)
```

Original (lines 1-6):

```
// settings.c -- ini schema and defaults copied verbatim from
// ps4_ambient_light v2.2's own g_config initializer and
// ambient_load_config's ini_table_get_entry* calls. Every (section,
// key) pair below was checked against that file directly, not
// recalled from memory. If the plugin's schema changes, re-sync this
// table from its main.c.
```

### companion-app/source/settings.c:23

In the code now:

```
// v2.9: smoothing presets, ported from the Android "inspiration" project's own ColorSmoothing.kt...
// More: docs/code-comments.md (companion-app/source/settings.c:23)
```

Original (lines 23-36):

```
// v2.9: smoothing presets, ported from the Android "inspiration"
// project's own ColorSmoothing.kt (applyPreset) -- the ms values are
// copied verbatim from its "off"/"responsive"/"balanced"/"smooth"
// cases (that project's other two per-preset knobs, output delay and a
// forced update-frequency override, don't have an equivalent here:
// this plugin has no output-delay concept, and update_frequency_hz is
// this project's own independent CPU-budget setting that a smoothing
// preset has no business silently overriding). This is a UI
// convenience over the two REAL fields below, not a third setting of
// its own -- nothing here is a new ini key, so kMenuItems' own
// "every (section,key) pair is exactly what the real plugin reads"
// invariant for its actual entries is untouched. See its own kMenuItems
// row (section "ui", not a real ini section) and smoothing_preset_index/
// smoothing_apply_preset below.
```

### companion-app/source/settings.c:40

In the code now:

```
// Bucketed rather than an exact-value match against kSmoothingPresetMs: this always has a defined...
// More: docs/code-comments.md (companion-app/source/settings.c:40)
```

Original (lines 40-46):

```
// Bucketed rather than an exact-value match against kSmoothingPresetMs:
// this always has a defined answer (no "Custom" case to render/cycle
// out of), including for a settling_time_ms a user typed by hand via
// the raw field rather than picking a preset. Off (index 0) is decided
// by smoothingEnabled alone -- matches the Android original, where
// "off" and "responsive" share the exact same 50ms settling time and
// are distinguished only by whether smoothing is enabled at all.
```

### companion-app/source/settings.c:68

In the code now:

```
// Grouped and ordered to match the blueprint's own card layout (setup.html / customisation.html), not...
// More: docs/code-comments.md (companion-app/source/settings.c:68)
```

Original (lines 68-93):

```
// Grouped and ordered to match the blueprint's own card layout
// (setup.html / customisation.html), not the plugin's ini section
// order -- MENU_SCREEN_SETUP items first, then MENU_SCREEN_CUSTOMIZE,
// each screen's items one contiguous run so the UI can group by a run
// of matching `group` strings into one card. `label` is now the exact
// on-screen text, since ui_screens.c builds every settings card
// directly from this table -- there is no second, hand-written copy of
// these strings to keep in sync. Percent-style units are not part of
// the label: they come from the trailing DisplayUnit, so the number
// shown can differ from the stored one (see settings.h).
//
// This reshuffles TWO fields from where the plugin's own ini
// sections would naturally put them, because the blueprint's cards
// don't follow section boundaries:
//  - color_order sits on Set Up's "LED strip layout" card (it's a
//    pill next to Direction there), not on Customize's "Colour" card.
//  - config_reload_check_seconds gets its own "Live reload" card on
//    Set Up, not folded into Customize's "Motion and darkness".
// Every (section, key, type, min, max) tuple is still exactly what the
// real plugin reads -- only screen/group/label/order changed. `step`
// is too, except on UNIT_PCT_OF_MAX rows (brightness and the two
// thresholds), where it's in the percent the person sees.
//
// A few ini keys have no row here on purpose (the two auto letterbox
// debounce timings). settings_load/settings_save still handle them
// directly; see the note where they used to sit.
```

### companion-app/source/settings.c:120

In the code now:

```
// Shell feed: the second plugin, which lights the strip from Home, Settings and the quick menu while...
// More: docs/code-comments.md (companion-app/source/settings.c:120)
```

Original (lines 120-124):

```
// Shell feed: the second plugin, which lights the strip from Home, Settings and the quick menu
// while no game is doing it. "enabled" is the real [shell] key the shell feed itself reads.
// "shell_autoload" is section "ui" on purpose (not an ini key -- it lives in GoldHEN's own
// payload list, which main.c edits when it is toggled); presets.c and settings_fill_table
// skip every "ui" row, so it can never leak into the ini.
```

### companion-app/source/settings.c:129

In the code now:

```
// FIELD_ACTION: sends the Ambilight payload (ps4-ambilight.bin) to GoldHEN's BinLoader (port 9090).
// More: docs/code-comments.md (companion-app/source/settings.c:129)
```

Original (lines 129-132):

```
// FIELD_ACTION: sends the Ambilight payload (ps4-ambilight.bin) to GoldHEN's BinLoader (port 9090).
// Section "ui", so presets.c/settings_fill_table never touch it. The dummy offset reuses
// shellAutoload (same pattern as preset_reset / smoothing_preset). main.c dispatches on
// key == "shell_run_now"; it guards the send on the Ambilight payload and .prx both being present.
```

### companion-app/source/settings.c:144

In the code now:

```
// Customize Capped at 4, not the plugin's old ceiling of 10 -- each step here is (2*scan_depth+1)^2...
// More: docs/code-comments.md (companion-app/source/settings.c:144)
```

Original (lines 144-150):

```
// ======================== Customize ========================
// Capped at 4, not the plugin's old ceiling of 10 -- each step here
// is (2*scan_depth+1)^2 real tiled-memory reads PER ZONE, PER
// Preset selector + reset button. Both are section "ui": neither is an ini key,
// and their offsets are dummies (same trick as smoothing_preset below) -- presets.c
// owns the real state, main.c's nudge_field / input code dispatch on `key`, and
// presets.c's own field walk skips every "ui" row.
```

### companion-app/source/settings.c:156

In the code now:

```
// SAMPLE-THREAD PASS (see letterbox.c's own probeNonBlack comment on why a tiled read isn't a free...
// More: docs/code-comments.md (companion-app/source/settings.c:156)
```

Original (lines 156-162):

```
// SAMPLE-THREAD PASS (see letterbox.c's own probeNonBlack comment
// on why a tiled read isn't a free array index). 4 still gives a
// clearly smoother average than the default of 1 without the
// worst-case cost climbing anywhere near what 10 allowed. The
// plugin's own ini loader enforces this same ceiling independently
// (SCAN_DEPTH_MAX in ambient_internal.h), so a hand-edited ini
// can't bypass it even if this slider's max ever drifts from that.
```

### companion-app/source/settings.c:170

In the code now:

```
// "Stability (frames)" and "Recheck every (frames)" (auto_letterbox_ stability_frames /...
// More: docs/code-comments.md (companion-app/source/settings.c:170)
```

Original (lines 170-177):

```
// "Stability (frames)" and "Recheck every (frames)" (auto_letterbox_
// stability_frames / auto_letterbox_check_interval_frames) are also
// dropped from the UI -- these are the debounce timing constants
// tuned alongside the depth-bounded, majority-vote detection scan
// (see the plugin's own letterbox.c). A user turning "Stability"
// down to react faster is exactly how the flicker that setting
// exists to prevent comes back. The "Auto letterbox" card's row
// layout ({4} -> {2}) is updated to match in kGroupLayouts.
```

### companion-app/source/settings.c:221

In the code now:

```
// wledHost intentionally left blank (not pre-filled with a real address) -- matches...
// More: docs/code-comments.md (companion-app/source/settings.c:221)
```

Original (lines 221-224):

```
// wledHost intentionally left blank (not pre-filled with a real
// address) -- matches ps4_ambient_light's own compiled default.
// Leaves the Setup screen's WLED Host field empty until a user
// fills in their own controller's IP.
```

### companion-app/source/settings.c:250

In the code now:

```
// relayHost intentionally left blank (memset above) and relaySignalEnabled defaults off, same...
// More: docs/code-comments.md (companion-app/source/settings.c:250)
```

Original (lines 250-255):

```
// relayHost intentionally left blank (memset above) and
// relaySignalEnabled defaults off, same reasoning as wledHost
// above -- this app shouldn't ship pointed at this project's own
// relay setup. relayPort's default matches ps4_ambient_light's
// own compiled default (24689) since, unlike a host address,
// there's nothing personal about a port number.
```

### companion-app/source/settings.c:278

In the code now:

```
// Clamps value into the [min,max] that kMenuItems declares for (section,key) -- the exact same bounds...
// More: docs/code-comments.md (companion-app/source/settings.c:278)
```

Original (lines 278-288):

```
// Clamps value into the [min,max] that kMenuItems declares for
// (section,key) -- the exact same bounds settings_set_i32() already
// enforces for every D-Pad-driven edit. settings_load() previously
// re-implemented range checks by hand per field, and several fields
// only got a partial check (e.g. LED counts/margins accepted any
// value >= their lower bound with no upper cap at all, led_offset had
// no bound check whatsoever) -- a hand-edited, corrupted, or future-
// schema ini could load values the UI itself would never let you set.
// Routing every numeric field through the schema's own min/max here
// means there's exactly one place bounds are defined, and it can't
// silently drift out of sync the way hand-copied literals could.
```

### companion-app/source/settings.c:301

In the code now:

```
// These two letterbox debounce keys have no row in kMenuItems any more (they're hidden from the UI)...
// More: docs/code-comments.md (companion-app/source/settings.c:301)
```

Original (lines 301-303):

```
// These two letterbox debounce keys have no row in kMenuItems any more (they're
// hidden from the UI), so clamp_to_schema() can't find bounds for them and
// would pass any value straight through. Same limits the rows used to carry.
```

### companion-app/source/settings.c:329

In the code now:

```
// Reintroduced (was lost between v17 and v18 -- see color_pipeline.h's struct comment).
// More: docs/code-comments.md (companion-app/source/settings.c:329)
```

Original (lines 329-332):

```
// Reintroduced (was lost between v17 and v18 -- see color_pipeline.h's
// struct comment). Deliberately NOT in kMenuItems, so no
// clamp_to_schema() call here -- there's no schema entry for these
// three to look up, same as v17 had it.
```

### companion-app/source/settings.c:356

In the code now:

```
// gamma preset is stored as a string ("1.0".."2.8") in the real ini, not an index -- match that...
// More: docs/code-comments.md (companion-app/source/settings.c:356)
```

Original (lines 356-359):

```
// gamma preset is stored as a string ("1.0".."2.8") in the real ini,
// not an index -- match that exactly rather than inventing our own
// representation, so a file this app writes still loads correctly
// in the real plugin.
```

### companion-app/source/settings.c:381

In the code now:

```
// Absent means OFF here (the app's default), even though the shell feed itself treats an absent key as...
// More: docs/code-comments.md (companion-app/source/settings.c:381)
```

Original (lines 381-383):

```
// Absent means OFF here (the app's default), even though the shell feed itself treats an absent
// key as on: an install that never touched this setting shows it off, and the first save writes
// it out explicitly so the two agree from then on.
```

### companion-app/source/settings.c:415

In the code now:

```
// Removes `key` from `section`. ini_table_s is a plain struct (see config.h), so this edits it directly.
// More: docs/code-comments.md (companion-app/source/settings.c:415)
```

Original (lines 415-417):

```
// Removes `key` from `section`. ini_table_s is a plain struct (see config.h), so this
// edits it directly. (Comment lines never get here: ini_table_read_from_file drops
// them, which is why an app save has always stripped the plugin's generated comments.)
```

### companion-app/source/settings.c:436

In the code now:

```
// MERGE FIX (reapplied -- this line of work still hasn't picked it up as of v16): this used to start...
// More: docs/code-comments.md (companion-app/source/settings.c:436)
```

Original (lines 436-446):

```
// MERGE FIX (reapplied -- this line of work still hasn't picked it
// up as of v16): this used to start from a blank table containing
// ONLY the fields this app knows about, then write that out --
// silently destroying anything else already in the file, including
// a hand-added [dev] section (dev_ip/dev_logging -- ps4_ambient_
// light v2.2.5, confirmed working on real hardware). Loading the
// existing file into the same table FIRST, then upserting just the
// known fields on top of it via the same ini_table_create_entry
// calls already below, preserves everything else untouched. (The
// read happens in the callers now: settings_save and presets_save
// load the file into `table` before calling this.)
```

### companion-app/source/settings.c:448  (kept in code as a summary)

In the code now:

```
// Write relay_host/relay_port/relay_signal_enabled back only if the file already had them: this app has no UI
// for them and must never be what first introduces a relay setup. [docs/code-comments.md]
```

Original (lines 448-460):

```
// BUG FIX: relay_host/relay_port/relay_signal_enabled have no
// settings-UI row (by design -- this app doesn't manage them), so
// cfg's copies of these three are only ever whatever settings_load
// happened to read in, or the struct defaults (blank/24689/false)
// if the file never had them. Writing them out unconditionally on
// every save -- as this used to -- meant a completely fresh ini
// (generated by the plugin itself, which never writes these keys
// at all unless a user hand-adds them) grew all three keys the
// moment it was ever saved from this app, even though nothing
// opted in. Capture whether each key already existed in the file
// BEFORE this save touches anything, and only write it back if it
// did -- so this app can preserve a hand-added relay setup across
// a save without ever being the thing that first introduces one.
```

### companion-app/source/settings.c:551

In the code now:

```
// Generic get/set through a MenuItem's byte offset into AmbientConfig.
// More: docs/code-comments.md (companion-app/source/settings.c:551)
```

Original (lines 551-558):

```
// Generic get/set through a MenuItem's byte offset into AmbientConfig.
// Only used for the numeric/bool/enum types (U32/I32/U16/BOOL/ENUM) --
// FIELD_STRING (wledHost) is handled separately by the UI via the
// on-screen keyboard, not through these.
// ---- display-unit helpers (see DisplayUnit in settings.h) -------------
// Presentation only: stored values stay raw 0-255 / 0-500 / etc. so the
// ini format and the plugin never see percentages. Only what is shown and
// typed changes -- "255" means nothing to most people, "100%" does.
```

### companion-app/source/settings.c:594

In the code now:

```
// For UNIT_PCT_OF_MAX rows `step` in the table is ALREADY in percent (so brightness nudges by a clean...
// More: docs/code-comments.md (companion-app/source/settings.c:594)
```

Original (lines 594-597):

```
// For UNIT_PCT_OF_MAX rows `step` in the table is ALREADY in percent (so
// brightness nudges by a clean 5%, thresholds by 1%); every other unit
// keeps stepping in stored units, which for the "direct" percents are
// percent anyway.
```

## companion-app/source/sha256.c

### companion-app/source/sha256.c:1

In the code now:

```
// sha256.c -- see sha256.h. Public-domain streaming SHA-256, adapted from Brad Conte's...
// More: docs/code-comments.md (companion-app/source/sha256.c:1)
```

Original (lines 1-5):

```
// sha256.c -- see sha256.h. Public-domain streaming SHA-256, adapted
// from Brad Conte's crypto-algorithms (public domain). Verified
// against known test vectors below (sha256("") and sha256("abc")) in
// tests/test_sha256.c -- this file has no PS4-specific dependencies,
// so those tests run on desktop like the rest of tests/.
```

## companion-app/source/shell_feed.c

### companion-app/source/shell_feed.c:1

In the code now:

```
// The shell feed's file work, split out of main.c: install and repair of ps4_shell_feed.prx and the...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:1)
```

Original (lines 1-4):

```
// The shell feed's file work, split out of main.c: install and repair of ps4_shell_feed.prx and the
// Ambilight payload, sending the payload to GoldHEN's BinLoader, the autoload entry in GoldHEN's
// payload list, and the claim that keeps the shell feed off the strip while this app drives it.
// The code is as it ran on hardware.
```

### companion-app/source/shell_feed.c:32

In the code now:

```
// shell feed: install, repair, autoload, claim Everything below runs only while "Shell feed" is on in...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:32)
```

Original (lines 32-37):

```
// ---------------- shell feed: install, repair, autoload, claim ----------------
//
// Everything below runs only while "Shell feed" is on in Set up (g_cfg.shellEnabled); with it off
// the only code here that executes is the cheap bookkeeping that keeps g_state.shell at SHELL_OFF.
// The decisions (what to show, what the button does, whether Test strip is offered) live in
// install_plan.h and are tested on a PC; this section only does the file and network work.
```

### companion-app/source/shell_feed.c:49

In the code now:

```
// Recomputes the shell feed's summary for Home from the facts above.
// More: docs/code-comments.md (companion-app/source/shell_feed.c:49)
```

Original (lines 49-51):

```
// Recomputes the shell feed's summary for Home from the facts above. Cheap (no I/O), called every
// frame, so the banner and button follow the files, the update check and the switch without anyone
// having to remember to refresh them.
```

### companion-app/source/shell_feed.c:70

In the code now:

```
// is the payload already running?
// More: docs/code-comments.md (companion-app/source/shell_feed.c:70)
```

Original (lines 70-76):

```
// ---- is the payload already running? ----
//
// The running shell feed rewrites /data/ps4-ambilight/ps4_shell_alive (a changing counter) about once a
// second; that is the same proof of life the game plugin uses (plugin/source/settings.c). A value that
// stops changing for SHELL_ALIVE_STALE_MS means it is off, stopped, waiting for a WLED address, or was
// never loaded. The first read after this app starts is only a baseline, so "running" shows up about
// one heartbeat (1-2 s) after the app opens.
```

### companion-app/source/shell_feed.c:120

In the code now:

```
// Downloads one file from the update source into its own staging path, checks it against the published...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:120)
```

Original (lines 120-123):

```
// Downloads one file from the update source into its own staging path, checks it against the
// published sha256, and only then renames it over the live file -- the same sequence do_plugin_update()
// uses, so a failure at any point leaves whatever was installed before exactly as it was.
// On failure writes a short reason into `err`.
```

### companion-app/source/shell_feed.c:183

In the code now:

```
// Sends the Ambilight payload (.bin) to GoldHEN's BinLoader on port 9090 -- the same protocol every...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:183)
```

Original (lines 183-198):

```
// Sends the Ambilight payload (.bin) to GoldHEN's BinLoader on port 9090 -- the same protocol
// every independent payload-sender tool uses: open a plain TCP connection, write the raw bytes,
// close. No handshake, no header -- confirmed against multiple independent client implementations
// (an Android payload sender and a Node.js tool), all doing exactly this against the same port.
//
// Called automatically as the last step of do_shell_repair() after a successful install or update,
// so "Install shell feed" also means "running within seconds, not at next boot". Also wired to the
// "Launch Ambilight payload" button in Set up, so the person can restart the payload at any time without
// rebooting or going into GoldHEN's menu.
//
// Sending the same payload more than once while it is already running is confirmed safe: tested
// manually via GoldHEN's Launchpad (which drives the same BinLoader), no crash or ill effect.
// One known quirk on some GoldHEN versions: BinLoader may need to be disabled and re-enabled in
// GoldHEN's menu before it will accept a second network send. That only affects repeated *network*
// sends; the manual-Launchpad test above is unaffected and not a concern for this button's
// primary use case (first run after install / starting a stopped payload).
```

### companion-app/source/shell_feed.c:283

In the code now:

```
// The Home button's shell feed half: creates the folders if they aren't there, then fetches whatever...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:283)
```

Original (lines 283-285):

```
// The Home button's shell feed half: creates the folders if they aren't there, then fetches
// whatever is missing or out of date. The .prx goes first so the Ambilight payload is never on disk
// without the thing it loads.
```

### companion-app/source/shell_feed.c:324

In the code now:

```
// Install succeeded: run the launcher immediately via BinLoader so the shell feed is live right now...
// More: docs/code-comments.md (companion-app/source/shell_feed.c:324)
```

Original (lines 324-327):

```
// Install succeeded: run the launcher immediately via BinLoader so the shell feed
// is live right now, not just "next boot". The launch_payload_now() status line
// replaces this one, so only set it as a fallback if the send fails (in which case
// launch_payload_now() already wrote an error). If Autoload is on, mention that too.
```

### companion-app/source/shell_feed.c:337

In the code now:

```
// (commented-out code removed)
// More: docs/code-comments.md (companion-app/source/shell_feed.c:337)
```

Original (lines 337-339):

```
// Update: the Ambilight payload is already running (or was); just note that a restart is needed
// for the updated build. We could try to re-launch here, but sending a payload update
// while the previous build is still resident is less predictable than a clean boot.
```

## companion-app/source/text_render.c

### companion-app/source/text_render.c:1

In the code now:

```
// text_render.c -- see text_render.h for why this exists (SDL2_ttf was never confirmed to link in this...
// More: docs/code-comments.md (companion-app/source/text_render.c:1)
```

Original (lines 1-8):

```
// text_render.c -- see text_render.h for why this exists (SDL2_ttf was
// never confirmed to link in this SDK; SceFreeType is).
//
// Real FreeType API used here, matching the upstream FreeType 2 docs:
// FT_Init_FreeType / FT_New_Face / FT_Set_Pixel_Sizes / FT_Load_Char
// (with FT_LOAD_RENDER, which rasterizes in the same call) /
// FT_Done_Face / FT_Done_FreeType. Nothing PS4-specific here --
// SceFreeType is Sony's packaged build of stock FreeType2, same API.
```

### companion-app/source/text_render.c:17

In the code now:

```
// --- Workaround for this SDK snapshot's freetype2 headers --- The headers here expose the FreeType...
// More: docs/code-comments.md (companion-app/source/text_render.c:17)
```

Original (lines 17-25):

```
// --- Workaround for this SDK snapshot's freetype2 headers ---
// The headers here expose the FreeType typedefs/constants (FT_Library,
// FT_Face, FT_GlyphSlot, FT_LOAD_RENDER, ...) and even declare
// FT_Set_Pixel_Sizes, but leave the five base-API functions below
// undeclared -- even though the compiled libSceFreeType.so we already
// link (-lSceFreeType) exports the real, standard FreeType symbols
// under these exact names. Redeclaring them here with FreeType's
// stable public signatures is harmless (a duplicate extern decl isn't
// an error in C) and lets the linker resolve them as normal.
```

### companion-app/source/text_render.c:55

In the code now:

```
// Reads the whole font file into memory so FT_New_Memory_Face can load it without needing stdio path...
// More: docs/code-comments.md (companion-app/source/text_render.c:55)
```

Original (lines 55-58):

```
// Reads the whole font file into memory so FT_New_Memory_Face can load
// it without needing stdio path semantics to match FreeType's
// expectations exactly -- matches the pattern the rest of this
// codebase already uses for reading /app0-relative files in config.c.
```

### companion-app/source/text_render.c:78

In the code now:

```
// FreeType is a real sysmodule on PS4, separate from linking -lSceFreeType at build time -- confirmed...
// More: docs/code-comments.md (companion-app/source/text_render.c:78)
```

Original (lines 78-83):

```
// FreeType is a real sysmodule on PS4, separate from linking
// -lSceFreeType at build time -- confirmed against OpenOrbis's own
// samples/_common/graphics.cpp (Scene2D::Init), which loads this
// exact module before its first FT_Init_FreeType call. Without
// this, FT_Init_FreeType fails, text_renderer_create returns NULL,
// and every draw_text() call becomes a silent no-op.
```

### companion-app/source/text_render.c:104

In the code now:

```
// FT_New_Memory_Face keeps fontData alive for the lifetime of the face -- freed further down once the...
// More: docs/code-comments.md (companion-app/source/text_render.c:104)
```

Original (lines 104-106):

```
// FT_New_Memory_Face keeps fontData alive for the lifetime of the
// face -- freed further down once the atlas is baked and the face
// is no longer needed.
```

### companion-app/source/text_render.c:138

In the code now:

```
// ARGB8888 surface: RGB channels stay white, alpha carries the glyph's actual AA coverage.
// More: docs/code-comments.md (companion-app/source/text_render.c:138)
```

Original (lines 138-140):

```
// ARGB8888 surface: RGB channels stay white, alpha carries the
// glyph's actual AA coverage. Texture color-mod tints this per
// draw call so one atlas serves every label color in main.c.
```

### companion-app/source/text_render.c:174

In the code now:

```
// Copy FreeType's 8bpp grayscale bitmap into the ARGB atlas, pixel row by pixel row -- FreeType's...
// More: docs/code-comments.md (companion-app/source/text_render.c:174)
```

Original (lines 174-176):

```
// Copy FreeType's 8bpp grayscale bitmap into the ARGB atlas,
// pixel row by pixel row -- FreeType's pitch can differ from
// width (it's allowed to pad rows), so index with pitch, not width.
```

### companion-app/source/text_render.c:212

In the code now:

```
// tr->lineHeight is repurposed here to hold ascent (top-of-line to baseline distance in px), which is...
// More: docs/code-comments.md (companion-app/source/text_render.c:212)
```

Original (lines 212-216):

```
// tr->lineHeight is repurposed here to hold ascent (top-of-line to
// baseline distance in px), which is what text_renderer_draw and
// text_renderer_measure actually need -- the font's full line
// height (ascent+descent+gap) isn't used anywhere in this UI since
// main.c positions each line manually.
```

### companion-app/source/text_render.c:240

In the code now:

```
// Unrenderable / space-like character: still advance the pen using the font's average advance so...
// More: docs/code-comments.md (companion-app/source/text_render.c:240)
```

Original (lines 240-243):

```
// Unrenderable / space-like character: still advance the
// pen using the font's average advance so layout doesn't
// collapse. Fall back to a quarter of lineHeight if we
// have nothing better.
```

## companion-app/source/text_render.h

### companion-app/source/text_render.h:1

In the code now:

```
// text_render.h -- minimal bitmap-font text renderer built directly on SceFreeType (real FreeType API...
// More: docs/code-comments.md (companion-app/source/text_render.h:1)
```

Original (lines 1-25):

```
// text_render.h -- minimal bitmap-font text renderer built directly on
// SceFreeType (real FreeType API: ft2build.h / FT_FREETYPE_H), not
// SDL2_ttf.
//
// Why this exists: SDL2_ttf was never actually linkable in this SDK
// snapshot (header only, no compiled lib -- see build.bat's original
// comment). SceFreeType *is* confirmed present (both freetype.h and
// libSceFreeType.so), and it's already linked via -lSceFreeType in
// build.bat. So instead of depending on a library that doesn't exist
// in this SDK, this renders text straight from FreeType:
//
//   1. At startup, rasterize the printable ASCII range (32..126) once
//      into a single texture atlas (each glyph's real FreeType
//      bitmap, packed into a grid).
//   2. Every draw_text() call after that is just SDL_RenderCopy() of
//      sub-rects from that atlas -- cheap enough for a 30fps UI with
//      ~10-20 lines of text per frame, and it works fine against the
//      software renderer this app already uses (SDL_CreateTextureFromSurface
//      + SDL_RenderCopy both work on a software-backed renderer; only
//      SDL_RenderPresent is the no-op here, and text draws never call it).
//
// Color is applied via SDL_SetTextureColorMod/AlphaMod on the shared
// atlas texture per draw call, not baked into the atlas -- so the same
// atlas serves the white/yellow/gray labels already used throughout
// main.c.
```

### companion-app/source/text_render.h:35

In the code now:

```
// Loads fontPath (expects the app0-relative path used everywhere else in this codebase, e.g....
// More: docs/code-comments.md (companion-app/source/text_render.h:35)
```

Original (lines 35-41):

```
// Loads fontPath (expects the app0-relative path used everywhere else
// in this codebase, e.g. "/app0/assets/fonts/font.ttf"), rasterizes
// the atlas at pixelHeight, and returns a renderer ready for
// text_renderer_draw(). Returns NULL on any failure (missing font,
// FreeType init failure, etc.) -- callers should treat that the same
// way the old always-NULL TTF_Font* was treated (draw calls become
// silent no-ops), not crash.
```

### companion-app/source/text_render.h:44

In the code now:

```
// Draws text with (x, y) as the top-left corner of the line, matching every existing draw_text() call...
// More: docs/code-comments.md (companion-app/source/text_render.h:44)
```

Original (lines 44-47):

```
// Draws text with (x, y) as the top-left corner of the line, matching
// every existing draw_text() call site in main.c. Safe to call with
// tr == NULL (no-op), so callers don't need to null-check before every
// draw.
```

## companion-app/source/ui_canvas.c

### companion-app/source/ui_canvas.c:1

In the code now:

```
// ui_canvas.c -- see ui_canvas.h for why this module exists.
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:1)
```

Original (lines 1-8):

```
// ui_canvas.c -- see ui_canvas.h for why this module exists.
//
// Rasterisation strategy: a scanline polygon filler with 5 vertical
// sub-samples per pixel row and exact analytic horizontal coverage
// per span. That combination is cheap (no 2D supersample buffer) but
// still resolves the two things this design leans on hardest: 1px
// hairline borders at fractional alpha, and 1.4-1.7px icon strokes at
// arbitrary angles.
```

### companion-app/source/ui_canvas.c:23

In the code now:

```
// Shape / coverage workspace
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:23)
```

Original (lines 23-25):

```
// ---------------------------------------------------------------
// Shape / coverage workspace
// ---------------------------------------------------------------
```

### companion-app/source/ui_canvas.c:244

In the code now:

```
// Canvas
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:244)
```

Original (lines 244-246):

```
// ---------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------
```

### companion-app/source/ui_canvas.c:343

In the code now:

```
// Geometry helpers
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:343)
```

Original (lines 343-345):

```
// ---------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------
```

### companion-app/source/ui_canvas.c:347

In the code now:

```
// Number of segments used to approximate a quarter turn -- scaled by radius so small 4px control...
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:347)
```

Original (lines 347-349):

```
// Number of segments used to approximate a quarter turn -- scaled by
// radius so small 4px control corners stay cheap while a 96px CTA
// corner still looks circular.
```

### companion-app/source/ui_canvas.c:415

In the code now:

```
// Primitives
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:415)
```

Original (lines 415-417):

```
// ---------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------
```

### companion-app/source/ui_canvas.c:497

In the code now:

```
// Stroke outline construction.
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:497)
```

Original (lines 497-499):

```
// Stroke outline construction. Each segment becomes a quad and each
// interior joint / round cap becomes a disc; the shared coverage
// buffer's max() combine is what keeps the seams invisible.
```

### companion-app/source/ui_canvas.c:565

In the code now:

```
// .scene background style.css: background: linear-gradient(var(--bg-grid) 1px, transparent 1px) 0...
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:565)
```

Original (lines 565-576):

```
// ---------------------------------------------------------------
// .scene background
// ---------------------------------------------------------------
//
// style.css:
//   background:
//     linear-gradient(var(--bg-grid) 1px, transparent 1px) 0 0/80px 80px,
//     linear-gradient(90deg, var(--bg-grid) 1px, transparent 1px) 0 0/80px 80px,
//     radial-gradient(1200px 700px at 50% -10%, rgba(255,180,84,0.05), transparent 60%),
//     #0b0c0e;
// Layers paint back-to-front in reverse source order, so: flat base,
// then the warm glow, then the grid lines on top.
```

### companion-app/source/ui_canvas.c:586

In the code now:

```
// Radial glow. The ending shape is an ellipse with radii 1200x700 centred at (50%, -10%) of the...
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:586)
```

Original (lines 586-589):

```
// Radial glow. The ending shape is an ellipse with radii
// 1200x700 centred at (50%, -10%) of the canvas; the colour stop
// reaches full transparency at 60% of that radius, so nothing
// outside t = 0.6 needs touching.
```

### companion-app/source/ui_canvas.c:624

In the code now:

```
// Cache what was just built. Built once, valid for the canvas's entire lifetime -- ui_draw_scene()...
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:624)
```

Original (lines 624-627):

```
// Cache what was just built. Built once, valid for the canvas's
// entire lifetime -- ui_draw_scene() takes no arguments beyond the
// canvas itself, so its output can only ever depend on c->w/c->h,
// which never change after ui_canvas_create().
```

### companion-app/source/ui_canvas.c:631

In the code now:

```
// If the allocation failed, sceneCache stays NULL and every future call just repaints the scene the...
// More: docs/code-comments.md (companion-app/source/ui_canvas.c:631)
```

Original (lines 631-633):

```
// If the allocation failed, sceneCache stays NULL and every future
// call just repaints the scene the slow way -- correct, only not
// fast, so there's no failure mode here worse than "as before".
```

## companion-app/source/ui_canvas.h

### companion-app/source/ui_canvas.h:1

In the code now:

```
// ui_canvas.h -- a small anti-aliased 2D canvas.
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:1)
```

Original (lines 1-16):

```
// ui_canvas.h -- a small anti-aliased 2D canvas.
//
// Why this exists: the blueprint (style.css) is a browser design. Its
// whole visual language depends on things SDL_RenderFillRect cannot
// draw -- 8px and 4px rounded corners, 1px hairline borders at
// rgba(255,255,255,0.09), 1.8px-stroke line icons, a soft radial glow
// behind the page. The previous UI approximated all of that with
// axis-aligned opaque rects, which is the single biggest reason it
// didn't look like the blueprint.
//
// So: everything is composited by this module into one ARGB8888 pixel
// buffer with real coverage-based anti-aliasing and real alpha
// blending, and main.c blits that buffer to the window once per
// frame. Nothing in here knows about SDL, which also means the whole
// UI can be compiled and rendered to a PNG on a desktop for visual
// diffing against the blueprint screenshots.
```

### companion-app/source/ui_canvas.h:49

In the code now:

```
// Per-corner radii, in the CSS order (top-left, top-right, bottom-right, bottom-left) so...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:49)
```

Original (lines 49-51):

```
// Per-corner radii, in the CSS order (top-left, top-right,
// bottom-right, bottom-left) so `border-radius` values transcribe
// directly. ui_radius_all() covers the common uniform case.
```

### companion-app/source/ui_canvas.h:71

In the code now:

```
// Lazily-built cache of ui_draw_scene()'s output (the 80px grid + radial glow), reused on every call...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:71)
```

Original (lines 71-78):

```
// Lazily-built cache of ui_draw_scene()'s output (the 80px grid +
// radial glow), reused on every call after the first instead of
// repainting it. That background is a pure function of canvas
// size, which never changes for the app's one canvas, so caching
// it is always correct, not just an approximation. It matters:
// the glow alone touches roughly 450,000 pixels with a sqrtf and
// an alpha blend each -- fine once, not fine 30 times a second,
// which is what made Test Strip's continuous redraw feel sluggish.
```

### companion-app/source/ui_canvas.h:86

In the code now:

```
// Clipping. ui_clip_push intersects with the current clip; the matching ui_clip_pop restores it.
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:86)
```

Original (lines 86-88):

```
// Clipping. ui_clip_push intersects with the current clip; the
// matching ui_clip_pop restores it. Depth is small and fixed -- the
// blueprint never nests more than a couple of levels.
```

### companion-app/source/ui_canvas.h:93

In the code now:

```
// direct pixel compositing Blends `color` over the pixel at (x, y) with an extra coverage multiplier...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:93)
```

Original (lines 93-96):

```
// ---- direct pixel compositing -------------------------------------
// Blends `color` over the pixel at (x, y) with an extra coverage
// multiplier in [0,1]. Everything else in this file eventually lands
// here.
```

### companion-app/source/ui_canvas.h:99

In the code now:

```
// coverage accumulation A shape is built up as one or more polygons whose coverage is combined with...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:99)
```

Original (lines 99-110):

```
// ---- coverage accumulation ----------------------------------------
// A shape is built up as one or more polygons whose coverage is
// combined with max(), then composited in a single pass. This matters
// for strokes: a polyline drawn as per-segment quads plus round joins
// would double-blend along every seam if each piece were composited
// separately, leaving visible darker notches on the 1.8px icon
// strokes. Accumulating first and blending once avoids that entirely.
// Contours are grouped into paths. Within one path every contour is
// rasterised together under the nonzero winding rule, which is what
// makes a hole possible (outer contour one way, inner contour the
// other) -- that's how an inside-stroke ring is drawn. Between paths,
// coverage combines with max().
```

### companion-app/source/ui_canvas.h:121

In the code now:

```
// A direct, unantialiased pixel fill for small axis-aligned rects -- no polygon build, no coverage...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:121)
```

Original (lines 121-129):

```
// A direct, unantialiased pixel fill for small axis-aligned rects --
// no polygon build, no coverage scan, just a nested loop writing
// pixels straight into the canvas. Visually indistinguishable from
// ui_fill_rect() at the sizes it's meant for (a handful of pixels
// across, like an LED tick), but far cheaper when called hundreds of
// times a frame, which the AA rasteriser's per-shape overhead (vertex
// arrays, a 5x-subsampled scanline pass) is not built for. Do not use
// this for anything large enough that the missing edge antialiasing
// would actually show.
```

### companion-app/source/ui_canvas.h:131

In the code now:

```
// Inside-stroke, like CSS `border` on a border-box element: the stroke occupies the outermost `width`...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:131)
```

Original (lines 131-133):

```
// Inside-stroke, like CSS `border` on a border-box element: the stroke
// occupies the outermost `width` pixels of `r`, it does not straddle
// the edge.
```

### companion-app/source/ui_canvas.h:142

In the code now:

```
// backgrounds The shared .scene layer: an 80px measurement grid plus the warm radial glow anchored...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:142)
```

Original (lines 142-145):

```
// ---- backgrounds --------------------------------------------------
// The shared .scene layer: an 80px measurement grid plus the warm
// radial glow anchored above the top edge. Transcribed from
// style.css's .scene background shorthand.
```

### companion-app/source/ui_canvas.h:148

In the code now:

```
// polyline/polygon stroking used by the icon renderer Appends the outline of a stroked polyline (round...
// More: docs/code-comments.md (companion-app/source/ui_canvas.h:148)
```

Original (lines 148-150):

```
// ---- polyline/polygon stroking used by the icon renderer ----------
// Appends the outline of a stroked polyline (round joins and,
// optionally, round caps) into an in-progress shape.
```

## companion-app/source/ui_icons.c

### companion-app/source/ui_icons.c:1

In the code now:

```
// ui_icons.c -- SVG path parsing + the blueprint's icon set.
// More: docs/code-comments.md (companion-app/source/ui_icons.c:1)
```

Original (lines 1-8):

```
// ui_icons.c -- SVG path parsing + the blueprint's icon set.
//
// The parser handles the subset the blueprint actually uses: M, L, H,
// V, C, S, A and Z, in both absolute and relative form, with SVG's
// usual number syntax (comma or whitespace separated, signs acting as
// separators, decimals without a leading zero). Curves and arcs are
// flattened to polylines at a resolution tied to the drawn size, so a
// 17px field-label icon and a 30px card-title icon both stay smooth.
```

### companion-app/source/ui_icons.c:24

In the code now:

```
// Shape description
// More: docs/code-comments.md (companion-app/source/ui_icons.c:24)
```

Original (lines 24-26):

```
// ---------------------------------------------------------------
// Shape description
// ---------------------------------------------------------------
```

### companion-app/source/ui_icons.c:54

In the code now:

```
// CUR ("currentColor") and CHAN_* are compound-literal macros, not static const objects: a static...
// More: docs/code-comments.md (companion-app/source/ui_icons.c:54)
```

Original (lines 54-63):

```
// CUR ("currentColor") and CHAN_* are compound-literal macros, not
// static const objects: a static const UiColor CUR = {...} object
// compiles under GCC's default GNU-extension leniency, but Clang
// (which this project's toolchain uses, and which does not extend
// that particular allowance) hard-errors on it with "initializer
// element is not a compile-time constant" the moment CUR's *value* is
// read into another static Shape[] initializer below -- reading a
// named object's value isn't a constant expression in standard C. A
// compound literal built entirely from constant scalars doesn't have
// that problem, since there's no named object being read.
```

### companion-app/source/ui_icons.c:69

In the code now:

```
// Number / command scanning
// More: docs/code-comments.md (companion-app/source/ui_icons.c:69)
```

Original (lines 69-71):

```
// ---------------------------------------------------------------
// Number / command scanning
// ---------------------------------------------------------------
```

### companion-app/source/ui_icons.c:107

In the code now:

```
// Flattening
// More: docs/code-comments.md (companion-app/source/ui_icons.c:107)
```

Original (lines 107-109):

```
// ---------------------------------------------------------------
// Flattening
// ---------------------------------------------------------------
```

### companion-app/source/ui_icons.c:148

In the code now:

```
// SVG endpoint-parameterised elliptical arc -> centre parameterisation, then flattened.
// More: docs/code-comments.md (companion-app/source/ui_icons.c:148)
```

Original (lines 148-152):

```
// SVG endpoint-parameterised elliptical arc -> centre parameterisation,
// then flattened. Straight out of the SVG 1.1 implementation notes
// (F.6.5); several blueprint icons (the wifi arcs, the refresh ring,
// the moon, the droplet) are built from these and look wrong if
// approximated as circles.
```

### companion-app/source/ui_icons.c:211

In the code now:

```
// Parses `d` (already in viewBox units) and emits flattened subpaths through `sink`.
// More: docs/code-comments.md (companion-app/source/ui_icons.c:211)
```

Original (lines 211-214):

```
// Parses `d` (already in viewBox units) and emits flattened subpaths
// through `sink`. `scale` is the viewBox-to-pixel factor, used only to
// pick a flattening resolution; coordinates stay in viewBox units and
// are transformed by the caller.
```

### companion-app/source/ui_icons.c:329

In the code now:

```
// Icon data -- transcribed from the blueprint's SVG markup
// More: docs/code-comments.md (companion-app/source/ui_icons.c:329)
```

Original (lines 329-331):

```
// ---------------------------------------------------------------
// Icon data -- transcribed from the blueprint's SVG markup
// ---------------------------------------------------------------
```

### companion-app/source/ui_icons.c:525

In the code now:

```
// Drawing
// More: docs/code-comments.md (companion-app/source/ui_icons.c:525)
```

Original (lines 525-527):

```
// ---------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------
```

### companion-app/source/ui_icons.c:577

In the code now:

```
// .icon inherits its parent's alpha; the one place the blueprint sets opacity on a sub-shape is the...
// More: docs/code-comments.md (companion-app/source/ui_icons.c:577)
```

Original (lines 577-579):

```
// .icon inherits its parent's alpha; the one place the
// blueprint sets opacity on a sub-shape is the contrast icon's
// centre rule.
```

### companion-app/source/ui_icons.c:613

In the code now:

```
// A zero-length "line" is how the blueprint draws a dot (e.g. the wifi icon's base).
// More: docs/code-comments.md (companion-app/source/ui_icons.c:613)
```

Original (lines 613-616):

```
// A zero-length "line" is how the blueprint draws a dot
// (e.g. the wifi icon's base). With round caps that is a
// disc of the stroke width; with butt caps it would
// vanish, so force a cap here.
```

## companion-app/source/ui_icons.h

### companion-app/source/ui_icons.h:1

In the code now:

```
// ui_icons.h -- the blueprint's line icons.
// More: docs/code-comments.md (companion-app/source/ui_icons.h:1)
```

Original (lines 1-10):

```
// ui_icons.h -- the blueprint's line icons.
//
// Every icon in this file is a literal transcription of the SVG
// markup in home.html / setup.html / customization.html: same 24x24
// viewBox, same `d` strings, same stroke width, same caps. Rather
// than redrawing each one by hand out of rects and circles (which is
// what the old UI did, and why its "icons" were three bars and a
// filled disc), ui_icons.c contains a small SVG path parser -- M/L/H/
// V/C/S/A/Z, absolute and relative -- so the shapes come out as
// drawn.
```

### companion-app/source/ui_icons.h:59

In the code now:

```
// Draws the icon scaled from its 24x24 viewBox into a size x size box with its top-left at (x, y).
// More: docs/code-comments.md (companion-app/source/ui_icons.h:59)
```

Original (lines 59-61):

```
// Draws the icon scaled from its 24x24 viewBox into a size x size box
// with its top-left at (x, y). Stroke width scales with the icon, the
// same way an SVG's does.
```

## companion-app/source/ui_screens.c

### companion-app/source/ui_screens.c:1

In the code now:

```
// ui_screens.c -- Home / Set up / Customization, transcribed from home.html, setup.html and...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:1)
```

Original (lines 1-6):

```
// ui_screens.c -- Home / Set up / Customization, transcribed from
// home.html, setup.html and customization.html.
//
// Values come from the live AmbientConfig, so nothing here is a
// placeholder: the numbers on screen are the ones in
// /data/ps4-ambilight/ps4_ambient_light.ini.
```

### companion-app/source/ui_screens.c:20

In the code now:

```
// Value formatting Every settings card is now built directly from kMenuItems (settings.c) rather than...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:20)
```

Original (lines 20-39):

```
// ---------------------------------------------------------------
// Value formatting
// ---------------------------------------------------------------
//
// Every settings card is now built directly from kMenuItems
// (settings.c) rather than a hand-written, parallel field list --
// settings.c's `label` is the exact on-screen text, so there is
// exactly one place that has to agree with the blueprint, and every
// visible field is wired to a real, working settings_get_i32/
// settings_set_i32 by construction, not by a second lookup that could
// drift out of sync with what's actually editable.
//
// What still lives here, because it's a UI-only concern settings.h
// deliberately knows nothing about: which icon a field/card gets,
// whether an enum renders as a plain input or an icon pill, per-field
// label tinting (the RGB balance channel colours), fixed-vs-flex
// column width, and how each group's fields wrap into rows. All of it
// is keyed off a MenuItem's `key` or `group` string, so reordering
// kMenuItems never breaks it -- only renaming a key/group would, and
// the two are only ever a few lines apart in this file.
```

### companion-app/source/ui_screens.c:58

In the code now:

```
// Same contiguous-run assumption settings.c's own comment documents: every MENU_SCREEN_SETUP item...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:58)
```

Original (lines 58-60):

```
// Same contiguous-run assumption settings.c's own comment documents:
// every MENU_SCREEN_SETUP item comes before every MENU_SCREEN_CUSTOMIZE
// one, so a screen's items are one [start, start+count) span.
```

### companion-app/source/ui_screens.c:119

In the code now:

```
// The two enum fields the blueprint draws as icon pills, rather than plain text inputs -- Colour order...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:119)
```

Original (lines 119-122):

```
// The two enum fields the blueprint draws as icon pills, rather than
// plain text inputs -- Colour order is FIELD_ENUM too but is styled
// as a plain input (see setup.html), so this is a real special case,
// not "every enum is a pill".
```

### companion-app/source/ui_screens.c:140

In the code now:

```
// Formats item's current value from cfg as display text.
// More: docs/code-comments.md (companion-app/source/ui_screens.c:140)
```

Original (lines 140-142):

```
// Formats item's current value from cfg as display text. Returns NULL
// for FIELD_BOOL (handled as a toggle, not text) -- FIELD_STRING is
// returned directly (points into cfg, valid as long as cfg is).
```

### companion-app/source/ui_screens.c:173

In the code now:

```
// Per-group visual identity (accent colour, card-title icon) and how that group's fields wrap into...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:173)
```

Original (lines 173-176):

```
// Per-group visual identity (accent colour, card-title icon) and how
// that group's fields wrap into rows -- both pixel-measured against
// the blueprint captures, same numbers the old hand-written per-card
// functions used.
```

### companion-app/source/ui_screens.c:253

In the code now:

```
// Builds every card for one settings screen directly from kMenuItems.
// More: docs/code-comments.md (companion-app/source/ui_screens.c:253)
```

Original (lines 253-255):

```
// Builds every card for one settings screen directly from kMenuItems.
// `focus` is the 0-based index within this screen's own item range
// (matching what main.c's navigation uses), or -1 for nothing focused.
```

### companion-app/source/ui_screens.c:298

In the code now:

```
// Two actions: the Shell feed's "send the Ambilight payload now" button, and the preset reset (its...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:298)
```

Original (lines 298-302):

```
// Two actions: the Shell feed's "send the Ambilight payload now" button,
// and the preset reset (its text says which preset it will reset, and what
// the next press will do). The text comes from settings_action_text(), which
// tests/test_action_buttons.c checks; an action it doesn't know shows its
// own label rather than borrowing another button's text.
```

### companion-app/source/ui_screens.c:323

In the code now:

```
// Row index within THIS card (0-based) decides whether the group's row2MarginTop applies...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:323)
```

Original (lines 323-325):

```
// Row index within THIS card (0-based) decides whether the
// group's row2MarginTop applies -- cardRowCount is that
// index before incrementing.
```

### companion-app/source/ui_screens.c:356

In the code now:

```
// Help Static prose, not data-driven from kMenuItems like Set up/ Customization are -- there's nothing...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:356)
```

Original (lines 356-374):

```
// ---------------------------------------------------------------
// Help
// ---------------------------------------------------------------
//
// Static prose, not data-driven from kMenuItems like Set up/
// Customization are -- there's nothing per-config to show. Reuses
// UiCard purely for its title+icon+desc layout: every card here has
// rowCount 0, so ui_card_draw never reaches its field-row code at
// all.
//
// Content is kept honest against what's actually true elsewhere in
// this app: group names match kMenuItems' own "group" strings
// (settings.c).
//
// Built inside a function, not as a file-scope static const table --
// COL_* (ui_theme.h) expands to a call to ui_rgb(), an ordinary
// function, not a constant expression, so it can't sit in a
// static-storage initializer the way kGroupLayouts' plain ints and
// strings can.
```

### companion-app/source/ui_screens.c:510

In the code now:

```
// Help -> QR popup (Triangle) A dim backdrop over whatever Help already drew, plus one centered panel...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:510)
```

Original (lines 510-520):

```
// ---------------------------------------------------------------
// Help -> QR popup (Triangle)
// ---------------------------------------------------------------
//
// A dim backdrop over whatever Help already drew, plus one centered
// panel: title, one line of instructions, the QR code itself (drawn
// module-by-module with ui_fill_rect_fast -- see that function's own
// comment on why it, not the AA rasteriser, is the right tool for a
// grid of single-digit-px squares), and the URL spelled out in mono
// underneath for anyone who'd rather type it than scan it. help_qr.c
// owns the actual QR bitmap; this only lays it out.
```

### companion-app/source/ui_screens.c:523

In the code now:

```
// Backdrop -- same idea as a CSS position:fixed dimmer, just a flat rect the size of the page at low...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:523)
```

Original (lines 523-525):

```
// Backdrop -- same idea as a CSS position:fixed dimmer, just a
// flat rect the size of the page at low alpha over everything
// already drawn this frame.
```

### companion-app/source/ui_screens.c:530

In the code now:

```
// help_qr_init() failed (see main.c's startup check) -- rather than draw an empty panel that looks...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:530)
```

Original (lines 530-532):

```
// help_qr_init() failed (see main.c's startup check) -- rather
// than draw an empty panel that looks broken, say so plainly
// and point at the fallback that always works regardless.
```

### companion-app/source/ui_screens.c:546

In the code now:

```
// Each module drawn at a fixed on-screen size rather than stretching to fill some target box -- at...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:546)
```

Original (lines 546-551):

```
// Each module drawn at a fixed on-screen size rather than
// stretching to fill some target box -- at this panel's size a
// module works out to a clean integer pixel count, and a QR
// scanner (especially a phone camera at a shallow angle off the
// TV) reads crisp square edges far more reliably than anything
// that landed on a fractional pixel boundary.
```

### companion-app/source/ui_screens.c:569

In the code now:

```
// QR itself -- a plain white square behind it first (most scanners expect light modules to actually be...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:569)
```

Original (lines 569-571):

```
// QR itself -- a plain white square behind it first (most scanners
// expect light modules to actually be light, not "the panel's own
// dark background showing through"), then the dark modules on top.
```

### companion-app/source/ui_screens.c:605

In the code now:

```
// Settings-screen shell
// More: docs/code-comments.md (companion-app/source/ui_screens.c:605)
```

Original (lines 605-607):

```
// ---------------------------------------------------------------
// Settings-screen shell
// ---------------------------------------------------------------
```

### companion-app/source/ui_screens.c:630

In the code now:

```
// .content-inner padding-bottom: clears the save bar (now 156px, up from 120px, to fit the hint row)...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:630)
```

Original (lines 630-632):

```
// .content-inner padding-bottom: clears the save bar (now 156px,
// up from 120px, to fit the hint row) plus the same margin above
// it the original 190px left for a 120px bar.
```

### companion-app/source/ui_screens.c:688

In the code now:

```
// .save-bar -- shared by Set up and Customization, not Customization-only.
// More: docs/code-comments.md (companion-app/source/ui_screens.c:688)
```

Original (lines 688-693):

```
// .save-bar -- shared by Set up and Customization, not
// Customization-only. Taller than the blueprint's own version
// (120px -> 156px) to fit a hint row the blueprint doesn't
// have at all: L1/R1's value-nudge shortcut has no other
// on-screen indication anywhere, so without this a user would
// only ever find it by accident.
```

### companion-app/source/ui_screens.c:714

In the code now:

```
// .save-btn -- a solid amber fill, same as the blueprint, always, EXCEPT for two states layered on...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:714)
```

Original (lines 714-726):

```
// .save-btn -- a solid amber fill, same as the blueprint,
// always, EXCEPT for two states layered on top, neither of
// which changes that resting look:
//  - focused: a brighter fill (still clearly "the amber
//    button") plus a ring drawn outside it rather than
//    brackets drawn on top of it -- corner brackets sitting
//    directly on an already-amber fill had too little contrast
//    to actually see, which was the original bug here.
//  - just saved: swaps to a green fill, a checkmark, and
//    "Saved!" for a couple of seconds (main.c owns the
//    timing) -- pressing Save previously had no visible effect
//    at all beyond the status line at the very bottom of the
//    screen, easy to miss entirely.
```

### companion-app/source/ui_screens.c:739

In the code now:

```
// Derived from the schema, same as main.c's navigation -- NOT a hardcoded count.
// More: docs/code-comments.md (companion-app/source/ui_screens.c:739)
```

Original (lines 739-743):

```
// Derived from the schema, same as main.c's navigation -- NOT a
// hardcoded count. It used to be SETUP_FIELD_COUNT/CUST_FIELD_COUNT
// constants, which silently broke the Save button's focus ring the
// moment a row was hidden from kMenuItems (main.c moved focus onto
// Save at the real count, this check still waited for the old one).
```

### companion-app/source/ui_screens.c:760

In the code now:

```
// Home
// More: docs/code-comments.md (companion-app/source/ui_screens.c:760)
```

Original (lines 760-762):

```
// ---------------------------------------------------------------
// Home
// ---------------------------------------------------------------
```

### companion-app/source/ui_screens.c:766

In the code now:

```
// Home banners and button (plugin + shell feed) With the shell feed off, Home draws ONE banner (the...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:766)
```

Original (lines 766-771):

```
// ---- Home banners and button (plugin + shell feed) ----------------------------------------
//
// With the shell feed off, Home draws ONE banner (the plugin's) at full width, exactly as it
// always did. With it on, two banners of the same height sit side by side: "Shell feed" on the
// left, the plugin's on the right. Same height means the button below doesn't move, so the
// compact banners trade padding for a second line of description text.
```

### companion-app/source/ui_screens.c:1081

In the code now:

```
// The hints line contains inline-flex .keycap boxes, which are taller than the 14px text strut.
// More: docs/code-comments.md (companion-app/source/ui_screens.c:1081)
```

Original (lines 1081-1086):

```
// The hints line contains inline-flex .keycap boxes, which are
// taller than the 14px text strut. CSS grows the line box
// around them -- and since a keycap aligns its own text
// baseline with the line's, the extra height lands above the
// baseline. Ignoring this put the whole footer 2px high and
// the status line under it 5px high.
```

### companion-app/source/ui_screens.c:1114

In the code now:

```
// "by Shimy", bottom-right of Home.
// More: docs/code-comments.md (companion-app/source/ui_screens.c:1114)
```

Original (lines 1114-1120):

```
// "by Shimy", bottom-right of Home. Right-aligned to the same 144px
// margin as the page content, a small amber bar on its left (the same
// accent every card carries), "by" in the muted label colour and the
// name itself in amber display type so it reads as a signature rather
// than another hint. The Test Strip's edge ticks sit within ~22px of
// the screen edge at the deepest scan setting, so this stays well
// clear of them.
```

### companion-app/source/ui_screens.c:1145

In the code now:

```
// Thin wrapper matching this function's old combined behavior, used by ui_render()'s own dispatch...
// More: docs/code-comments.md (companion-app/source/ui_screens.c:1145)
```

Original (lines 1145-1148):

```
// Thin wrapper matching this function's old combined behavior, used
// by ui_render()'s own dispatch below (which callers like the desktop
// harness still go through -- they have no reason to know about the
// caching split main.c uses).
```

## companion-app/source/ui_screens.h

### companion-app/source/ui_screens.h:1

In the code now:

```
// ui_screens.h -- the three blueprint screens.
// More: docs/code-comments.md (companion-app/source/ui_screens.h:1)
```

Original (lines 1-5):

```
// ui_screens.h -- the three blueprint screens.
//
// Home is laid out at home.html's own absolute coordinates; Set up and
// Customization are built from ui_widgets' card/field model, which
// reproduces setup.html and customization.html's flow layout.
```

### companion-app/source/ui_screens.h:30

In the code now:

```
// Field counts per screen are derived from kMenuItems via ui_screen_item_range() -- don't reintroduce...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:30)
```

Original (lines 30-32):

```
// Field counts per screen are derived from kMenuItems via
// ui_screen_item_range() -- don't reintroduce hardcoded counts here; they
// go stale the moment a row is added or hidden and break Save focus.
```

### companion-app/source/ui_screens.h:42

In the code now:

```
// The shell feed's side of Home: zero-initialised means SHELL_OFF, which draws Home exactly as it was...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:42)
```

Original (lines 42-45):

```
// The shell feed's side of Home: zero-initialised means SHELL_OFF, which draws Home exactly as
// it was before the shell feed existed. What the button does and whether Test strip is offered
// is home_plan(install, shell) (install_plan.h) -- computed from these two on demand, so the
// renderer and main.c can never disagree about it.
```

### companion-app/source/ui_screens.h:47

In the code now:

```
// Whether the Ambilight payload is actually running, read from the shell feed's heartbeat file...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:47)
```

Original (lines 47-49):

```
// Whether the Ambilight payload is actually running, read from the shell feed's heartbeat file
// (shell_feed.c: shell_alive_poll). Running means the heartbeat is changing; starting means the
// payload was sent a moment ago and no heartbeat has shown up yet. Both false = not running.
```

### companion-app/source/ui_screens.h:61

In the code now:

```
// Help screen only: the Triangle-button QR popup (help_qr.c) pointing at this project's README.
// More: docs/code-comments.md (companion-app/source/ui_screens.h:61)
```

Original (lines 61-63):

```
// Help screen only: the Triangle-button QR popup (help_qr.c)
// pointing at this project's README. Independent of scrollY --
// opening it doesn't move or reset the card list underneath.
```

### companion-app/source/ui_screens.h:70

In the code now:

```
// Footer/status line, shown on every screen. statusIsSaved gives the blueprint's two-tone "saved ->...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:70)
```

Original (lines 70-73):

```
// Footer/status line, shown on every screen. statusIsSaved gives
// the blueprint's two-tone "saved -> path -- ..." treatment;
// otherwise the whole line renders in one colour (warn-amber for
// statusIsError, muted grey otherwise).
```

### companion-app/source/ui_screens.h:81

In the code now:

```
// The LED edge frame (led_frame.c) draws exactly these bytes, in wire order -- the literal RGB...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:81)
```

Original (lines 81-87):

```
// The LED edge frame (led_frame.c) draws exactly these bytes, in
// wire order -- the literal RGB triplets main.c last sent over
// DDP, not an approximation of them. NULL/0 means nothing has
// been sent yet (or the last send failed), which the frame shows
// as a dim, unmistakably-placeholder colour rather than a fake
// gradient. Owned by the caller (main.c); this struct only borrows
// the pointer for the duration of one ui_render() call.
```

### companion-app/source/ui_screens.h:91

In the code now:

```
// Screen positions for those same LEDs, in the same order -- main.c's own layout_build() output...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:91)
```

Original (lines 91-94):

```
// Screen positions for those same LEDs, in the same order --
// main.c's own layout_build() output, reused here rather than
// led_frame.c calling layout_build() a second time every frame
// purely to re-derive positions main.c already computed.
```

### companion-app/source/ui_screens.h:98

In the code now:

```
// True for a short window right after settings_save() succeeds -- the Save button shows a distinct...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:98)
```

Original (lines 98-102):

```
// True for a short window right after settings_save() succeeds --
// the Save button shows a distinct "Saved!" state while this is
// set, rather than nothing visibly happening. main.c owns the
// timing (how long the window lasts); this struct just carries
// the current yes/no for one frame.
```

### companion-app/source/ui_screens.h:108

In the code now:

```
// Draws every part of Home except the LED edge frame (led_frame.c's job, driven by...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:108)
```

Original (lines 108-116):

```
// Draws every part of Home except the LED edge frame (led_frame.c's
// job, driven by UiState.ledSlots/ledColors). Exposed separately so
// main.c can cache the result and, while Test Strip's animation keeps
// the LED frame the only thing actually changing, redraw just that
// instead of repainting all of Home's text, icons and background at
// 30fps for content that's identical frame to frame. ui_render()
// itself still draws Home in one combined call for callers (like the
// desktop verification harness) that have no reason to know this
// split exists.
```

### companion-app/source/ui_screens.h:128

In the code now:

```
// The [start, start+count) span of kMenuItems belonging to one MenuScreen -- shared by the card...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:128)
```

Original (lines 128-132):

```
// The [start, start+count) span of kMenuItems belonging to one
// MenuScreen -- shared by the card builder (ui_screens.c) and main.c's
// D-Pad navigation/editing, so both ever agree on which kMenuItems
// index a given on-screen field is. A screen's items are one
// contiguous run (see settings.c's own comment above kMenuItems).
```

### companion-app/source/ui_screens.h:135

In the code now:

```
// Row layout for D-Pad grid navigation: fills rowStart[]/rowCount[] with each row's starting field...
// More: docs/code-comments.md (companion-app/source/ui_screens.h:135)
```

Original (lines 135-141):

```
// Row layout for D-Pad grid navigation: fills rowStart[]/rowCount[]
// with each row's starting field index (0-based within the screen,
// matching UiState.focusField) and field count, in the same row
// splits the card renderer uses (kGroupLayouts in ui_screens.c) --
// main.c's Up/Down/Left/Right navigation is built on this so it can
// never disagree with what's actually drawn. Returns the number of
// rows written (capped at maxRows).
```

## companion-app/source/ui_text.c

### companion-app/source/ui_text.c:1

In the code now:

```
// ui_text.c -- see ui_text.h. FreeType usage is deliberately kept to the same tiny API surface the...
// More: docs/code-comments.md (companion-app/source/ui_text.c:1)
```

Original (lines 1-9):

```
// ui_text.c -- see ui_text.h.
//
// FreeType usage is deliberately kept to the same tiny API surface the
// previous text_render.c proved linkable against this SDK's
// libSceFreeType (FT_Init_FreeType / FT_New_Memory_Face /
// FT_Set_Pixel_Sizes / FT_Load_Char / FT_Done_Face /
// FT_Done_FreeType), plus FT_Get_Char_Index, which is needed to tell
// "this face has no glyph for U+25CB" from "this face rendered a
// blank" so the fallback chain can work at all.
```

### companion-app/source/ui_text.c:22  (kept in code as a summary)

In the code now:

```
// The FreeType sysmodule load must be unconditional: __ORBIS__ is never defined by this toolchain, so gating on it
// compiled the load out and crashed (PRX_NOT_RESOLVED_FUNCTION). The desktop harness defines UI_TEXT_DESKTOP_HARNESS.
```

Original (lines 22-39):

```
// This must be unconditional in the real build. An earlier pass gated
// it behind `#ifdef __ORBIS__` to let the desktop verification harness
// (which has no orbis/* headers) compile -- but __ORBIS__ is not a
// macro this toolchain defines anywhere (grep the whole codebase and
// build.bat: it appears nowhere else), so on the real OpenOrbis clang
// invocation that guard was always false. That silently compiled out
// both this include and the sceSysmoduleLoadModule() call below on
// every real build, while main.c's own unconditional orbis/* includes
// were unaffected -- which is exactly why this went undetected until a
// real crash log: the file still compiled and linked cleanly with the
// FreeType sysmodule never actually loaded, so the first real
// FT_Init_FreeType call (confirmed by reversing its PS4 NID against
// the crash log's "Required Function NID") faulted as
// PRX_NOT_RESOLVED_FUNCTION -- the same failure mode, for the same
// reason, as the ImeDialog bug this project already solved once.
// UI_TEXT_DESKTOP_HARNESS is instead defined explicitly, only by the
// desktop harness's own build command, so the default here -- what a
// plain build.bat invocation sees -- is the real, production include.
```

### companion-app/source/ui_text.c:43

In the code now:

```
// Desktop-harness stand-in for the one orbis call this file makes, so harness builds (which link no...
// More: docs/code-comments.md (companion-app/source/ui_text.c:43)
```

Original (lines 43-46):

```
// Desktop-harness stand-in for the one orbis call this file makes, so
// harness builds (which link no orbis libraries at all) still resolve
// the symbol; real builds never see this, since UI_TEXT_DESKTOP_HARNESS
// is never defined by build.bat.
```

### companion-app/source/ui_text.c:52

In the code now:

```
// --- Workaround for this SDK snapshot's freetype2 headers --- Carried over verbatim in spirit from...
// More: docs/code-comments.md (companion-app/source/ui_text.c:52)
```

Original (lines 52-66):

```
// --- Workaround for this SDK snapshot's freetype2 headers ---
// Carried over verbatim in spirit from the old text_render.c: the
// headers expose the types and constants but leave these base-API
// functions undeclared, even though libSceFreeType exports them under
// exactly these names. Redeclaring with FreeType's stable public
// signatures is harmless on a desktop build too (a duplicate extern
// declaration is not an error in C).
//
// FT_Get_Name_Index is deliberately NOT in this list. A real
// OpenOrbis build (clang, not this file's desktop gcc build) failed
// with "conflicting types for 'FT_Get_Name_Index'" -- this SDK's
// freetype.h, unlike the functions below, already declares it via
// FT_EXPORT(), whose visibility attribute a plain extern redeclaration
// doesn't match. Call it through the real declaration that
// FT_FREETYPE_H already brings in instead of redeclaring it here.
```

### companion-app/source/ui_text.c:76

In the code now:

```
// Standard Latin f-ligatures. Chrome applies the `liga` feature by default, so "offset" is drawn with...
// More: docs/code-comments.md (companion-app/source/ui_text.c:76)
```

Original (lines 76-81):

```
// Standard Latin f-ligatures. Chrome applies the `liga` feature by
// default, so "offset" is drawn with a single f_f glyph that is ~3px
// narrower at 16px than two f's. Without this, every word after an
// "ff" on a line sits a few pixels right of where the blueprint puts
// it. Only these five are needed for the design's copy; anything not
// present in the face falls back to the plain glyph sequence.
```

### companion-app/source/ui_text.c:147  (kept in code as a summary)

In the code now:

```
// Load the FreeType sysmodule before FT_Init_FreeType (linking is not enough) and bail out if it fails;
// skipping this crashed on console. A NULL font just draws no text. [docs/code-comments.md]
```

Original (lines 147-164):

```
// FreeType is a separate runtime sysmodule on PS4 -- linking
// -lSceFreeType is not enough. Confirmed against OpenOrbis's own
// samples/_common/graphics.cpp, which loads this before its first
// FT_Init_FreeType call. Loading twice (once per ui_font_load
// call, one per bundled font) is harmless -- it's the documented
// idempotent pattern for this API.
//
// This call, and the check on its result, must never be skipped:
// a version of this file that compiled it out on the real target
// (via an unverified __ORBIS__ guard meant only for the desktop
// harness) shipped once already and crashed on console with
// PRX_NOT_RESOLVED_FUNCTION on the very first FT_Init_FreeType
// call below, because that call's import was never resolved
// against a loaded module. If this load ever genuinely fails,
// bail out here rather than let that happen again -- draw calls
// against a NULL UiFont* are already safe no-ops everywhere else
// in this file, so returning NULL degrades to "no text", not a
// crash.
```

### companion-app/source/ui_text.c:193

In the code now:

```
// Metrics are computed from the face's design values rather than face->size->metrics, which FreeType...
// More: docs/code-comments.md (companion-app/source/ui_text.c:193)
```

Original (lines 193-197):

```
// Metrics are computed from the face's design values rather than
// face->size->metrics, which FreeType rounds to whole pixels. That
// rounding is enough to move a baseline by a pixel at these sizes,
// and every vertical position in the screens is derived from these
// numbers.
```

### companion-app/source/ui_text.c:261

In the code now:

```
// FT_LOAD_NO_HINTING matters here.
// More: docs/code-comments.md (companion-app/source/ui_text.c:261)
```

Original (lines 261-266):

```
// FT_LOAD_NO_HINTING matters here. With hinting on, FreeType
// snaps stems to the pixel grid and rounds each advance, which
// makes a long line of text drift several pixels away from where
// the browser puts it by the end of the line. Chrome lays this
// design out with unhinted, linearly-scaled advances; matching
// that keeps wrap points and line widths identical.
```

### companion-app/source/ui_text.c:333

In the code now:

```
// Consumes a ligature at *p if one applies, returning its synthetic codepoint, or 0 if the next...
// More: docs/code-comments.md (companion-app/source/ui_text.c:333)
```

Original (lines 333-341):

```
// Consumes a ligature at *p if one applies, returning its synthetic
// codepoint, or 0 if the next character should be handled normally.
// Kerning. The bundled Space Grotesk faces carry a `kern` table
// generated from their own GPOS pair positioning, restricted to the
// glyphs this UI can draw, because FreeType only reads the legacy
// table and the upstream font ships kerning in GPOS only. It is not
// cosmetic: unkerned, the Screen sampling description measures
// 1301.6px against a 1300px max-width and wraps a word earlier than
// the blueprint does.
```

## companion-app/source/ui_text.h

### companion-app/source/ui_text.h:1

In the code now:

```
// ui_text.h -- FreeType text for the canvas.
// More: docs/code-comments.md (companion-app/source/ui_text.h:1)
```

Original (lines 1-20):

```
// ui_text.h -- FreeType text for the canvas.
//
// Differences from the old text_render.h this replaces, all of them
// forced by the blueprint:
//
//  * Many faces, not one. style.css uses Space Grotesk at 400/500/600/
//    700 and JetBrains Mono at 400/500/600. A single bold serif atlas
//    cannot express that, and the mismatch was the most visible reason
//    the old UI didn't read as the same product.
//  * Many sizes. The blueprint spans 12px keycaps to 50px page
//    headings; one atlas per (face, size) is created up front.
//  * UTF-8 with a fallback face, because real strings in the design
//    contain -, →, ○ and ✕, none of which are ASCII.
//  * CSS line-box positioning. ui_text_baseline_for_box() reproduces
//    the browser's half-leading maths so a value like "top: 64px" in
//    the blueprint lands on the same scanline here. Verified against
//    the blueprint screenshots: Set up's h1 baseline falls at y=113 in
//    both.
//  * Draws into UiCanvas with real coverage blending instead of
//    SDL_RenderCopy of a tinted atlas.
```

### companion-app/source/ui_text.h:33

In the code now:

```
// Any codepoint missing from `f` is looked up in `fallback` instead.
// More: docs/code-comments.md (companion-app/source/ui_text.h:33)
```

Original (lines 33-35):

```
// Any codepoint missing from `f` is looked up in `fallback` instead.
// Used to give the Space Grotesk faces access to JetBrains Mono's ○
// and ✕, exactly the way the browser falls back for those two.
```

### companion-app/source/ui_text.h:42

In the code now:

```
// Baseline for a line box whose top edge is at boxTop.
// More: docs/code-comments.md (companion-app/source/ui_text.h:42)
```

Original (lines 42-44):

```
// Baseline for a line box whose top edge is at boxTop. Pass
// lineHeight <= 0 for `line-height: normal`; otherwise pass the used
// line-height in px (e.g. 1.6 * 16 for .card-desc).
```

### companion-app/source/ui_text.h:52

In the code now:

```
// Convenience: draws with the line-box semantics above, returning the advance width.
// More: docs/code-comments.md (companion-app/source/ui_text.h:52)
```

Original (lines 52-54):

```
// Convenience: draws with the line-box semantics above, returning the
// advance width. Most call sites in the screens use this so the
// numbers in the code are the same numbers that appear in style.css.
```

## companion-app/source/ui_theme.c

### companion-app/source/ui_theme.c:1

In the code now:

```
// ui_theme.c -- loads the two families the blueprint imports. style.css pulls Space Grotesk...
// More: docs/code-comments.md (companion-app/source/ui_theme.c:1)
```

Original (lines 1-8):

```
// ui_theme.c -- loads the two families the blueprint imports.
//
// style.css pulls Space Grotesk 400/500/600/700 and JetBrains Mono
// 400/500/600/700 from Google Fonts. Those exact families are bundled
// under assets/fonts (Space Grotesk instanced from the upstream
// variable font at each weight), so the app renders in the same
// typefaces the design was drawn in rather than substituting one
// bundled serif for all of them.
```

### companion-app/source/ui_theme.c:55

In the code now:

```
// Not part of the blueprint's type system, and deliberately optional: if the file's missing the Home...
// More: docs/code-comments.md (companion-app/source/ui_theme.c:55)
```

Original (lines 55-57):

```
// Not part of the blueprint's type system, and deliberately optional:
// if the file's missing the Home credit falls back to Space Grotesk
// (see render_home_credit) rather than failing startup over a signature.
```

### companion-app/source/ui_theme.c:60

In the code now:

```
// Space Grotesk has no glyph for the circle/cross used in the controller hints ("Press ○ to go back"...
// More: docs/code-comments.md (companion-app/source/ui_theme.c:60)
```

Original (lines 60-63):

```
// Space Grotesk has no glyph for the circle/cross used in the
// controller hints ("Press ○ to go back", the ✕ keycap). The
// browser silently falls back for those two characters; do the
// same rather than dropping them.
```

## companion-app/source/ui_theme.h

### companion-app/source/ui_theme.h:1

In the code now:

```
// ui_theme.h -- style.css :root, transcribed.
// More: docs/code-comments.md (companion-app/source/ui_theme.h:1)
```

Original (lines 1-7):

```
// ui_theme.h -- style.css :root, transcribed.
//
// Every colour below is the literal token from the blueprint's
// stylesheet, including the ones written as rgba() -- those stay
// translucent here rather than being flattened to a "solid
// approximation", because a 9%-white hairline over a #15171b panel
// and a #2b2e34 opaque line are visibly different at 1px.
```

### companion-app/source/ui_theme.h:35

In the code now:

```
// A visibly brighter amber for a solid-fill control's focused state (the Save button) -- not a...
// More: docs/code-comments.md (companion-app/source/ui_theme.h:35)
```

Original (lines 35-41):

```
// A visibly brighter amber for a solid-fill control's focused state
// (the Save button) -- not a blueprint token, since the blueprint has
// no console-focus concept at all; needed because corner brackets
// drawn on top of an already-amber fill (ui_focus_brackets' usual
// treatment) have too little contrast to see, which was the actual
// complaint this fixes: the save button looked identical focused or
// not.
```

### companion-app/source/ui_theme.h:54

In the code now:

```
// fonts Named for the rule they serve in style.css, so a call site reads like the stylesheet it came from.
// More: docs/code-comments.md (companion-app/source/ui_theme.h:54)
```

Original (lines 54-56):

```
// ---- fonts ----
// Named for the rule they serve in style.css, so a call site reads
// like the stylesheet it came from.
```

## companion-app/source/ui_widgets.c

### companion-app/source/ui_widgets.c:53

In the code now:

```
// Text wrapping
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:53)
```

Original (lines 53-55):

```
// ---------------------------------------------------------------
// Text wrapping
// ---------------------------------------------------------------
```

### companion-app/source/ui_widgets.c:97

In the code now:

```
// Focus marks style.css draws two 12x12 L-brackets in the accent colour at the top corners of a...
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:97)
```

Original (lines 97-104):

```
// ---------------------------------------------------------------
// Focus marks
// ---------------------------------------------------------------
//
// style.css draws two 12x12 L-brackets in the accent colour at the top
// corners of a focused control (both ::before and ::after sit at
// top:-2px), replacing the glow ring the old UI used. Note both are at
// the TOP -- not opposite corners.
```

### companion-app/source/ui_widgets.c:127

In the code now:

```
// Keycap
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:127)
```

Original (lines 127-129):

```
// ---------------------------------------------------------------
// Keycap
// ---------------------------------------------------------------
```

### companion-app/source/ui_widgets.c:151

In the code now:

```
// A hint row's line box is taller than its own text, because the keycap chips inside it are taller...
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:151)
```

Original (lines 151-154):

```
// A hint row's line box is taller than its own text, because the
// keycap chips inside it are taller than the surrounding 14px text --
// same CSS line-box reasoning ui_text_baseline_for_box() handles for
// plain text, worked out once here so every caller agrees.
```

### companion-app/source/ui_widgets.c:172

In the code now:

```
// Distance from a hint row's line-box top to its baseline -- the piece ui_hint_row_line_height's total...
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:172)
```

Original (lines 172-174):

```
// Distance from a hint row's line-box top to its baseline -- the
// piece ui_hint_row_line_height's total doesn't expose on its own,
// and every caller that positions a row needs.
```

### companion-app/source/ui_widgets.c:196

In the code now:

```
// Field sizing
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:196)
```

Original (lines 196-198):

```
// ---------------------------------------------------------------
// Field sizing
// ---------------------------------------------------------------
```

### companion-app/source/ui_widgets.c:248

In the code now:

```
// Card measurement
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:248)
```

Original (lines 248-250):

```
// ---------------------------------------------------------------
// Card measurement
// ---------------------------------------------------------------
```

### companion-app/source/ui_widgets.c:301

In the code now:

```
// Field drawing
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:301)
```

Original (lines 301-303):

```
// ---------------------------------------------------------------
// Field drawing
// ---------------------------------------------------------------
```

### companion-app/source/ui_widgets.c:398

In the code now:

```
// Card drawing
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:398)
```

Original (lines 398-400):

```
// ---------------------------------------------------------------
// Card drawing
// ---------------------------------------------------------------
```

### companion-app/source/ui_widgets.c:410

In the code now:

```
// border-left: 3px solid var(--panel-accent).
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:410)
```

Original (lines 410-412):

```
// border-left: 3px solid var(--panel-accent). Drawn as a clipped
// rounded rect so it follows the panel's own 8px corners instead of
// sticking out as a square stripe.
```

### companion-app/source/ui_widgets.c:483

In the code now:

```
// Page header
// More: docs/code-comments.md (companion-app/source/ui_widgets.c:483)
```

Original (lines 483-485):

```
// ---------------------------------------------------------------
// Page header
// ---------------------------------------------------------------
```

## companion-app/source/ui_widgets.h

### companion-app/source/ui_widgets.h:1

In the code now:

```
// ui_widgets.h -- the blueprint's components, as components.
// More: docs/code-comments.md (companion-app/source/ui_widgets.h:1)
```

Original (lines 1-13):

```
// ui_widgets.h -- the blueprint's components, as components.
//
// Rather than hard-coding pixel positions per screen, this reproduces
// the small slice of CSS box model the design actually uses: border-box
// sizing, padding, margins, a flex row with `gap` and `flex: 1 1 240px`
// growth, and CSS line boxes for vertical text placement. Card heights
// then fall out of their content the same way they do in the browser.
//
// This was checked against the blueprint screenshots rather than
// assumed. On Set up, the four LED-count inputs land at x = 191, 581,
// 971, 1361 with width 370, the Colour row's six fields come out 240
// wide, and Screen sampling's five come out 292 -- all matching the
// captures pixel for pixel.
```

### companion-app/source/ui_widgets.h:83

In the code now:

```
// Vertical offset (from the card's top edge) and height of one field row, so a caller can scroll a...
// More: docs/code-comments.md (companion-app/source/ui_widgets.h:83)
```

Original (lines 83-85):

```
// Vertical offset (from the card's top edge) and height of one field
// row, so a caller can scroll a focused control into view without
// duplicating the card's layout maths.
```

### companion-app/source/ui_widgets.h:92

In the code now:

```
// A full outline ring drawn just outside a control's own bounding box -- for controls where...
// More: docs/code-comments.md (companion-app/source/ui_widgets.h:92)
```

Original (lines 92-97):

```
// A full outline ring drawn just outside a control's own bounding box
// -- for controls where ui_focus_brackets' corner marks would sit on
// top of a solid fill colour (a big amber button, say) and be hard to
// see there regardless of the bracket colour. Sitting outside the
// control means the ring is always against the page background
// instead, so contrast never depends on the control's own fill.
```

### companion-app/source/ui_widgets.h:104

In the code now:

```
// Line-box height of a hint row (keycap chips are taller than the surrounding text, so CSS-style...
// More: docs/code-comments.md (companion-app/source/ui_widgets.h:104)
```

Original (lines 104-107):

```
// Line-box height of a hint row (keycap chips are taller than the
// surrounding text, so CSS-style line-box math is needed the same way
// ui_text_baseline_for_box() needs it elsewhere) -- callers use this
// to place whatever comes after the row.
```

## companion-app/source/update_check.c

### companion-app/source/update_check.c:1

In the code now:

```
// The background update check, split out of main.c: the worker thread that asks the update source...
// More: docs/code-comments.md (companion-app/source/update_check.c:1)
```

Original (lines 1-3):

```
// The background update check, split out of main.c: the worker thread that asks the update source
// whether the plugin or a shell feed file has a newer build, the flags it publishes through, and
// the once-a-frame poll that turns the answer into Home's state. The code is as it ran on hardware.
```

### companion-app/source/update_check.c:26

In the code now:

```
// update source: GitHub, or a local test server Normally every download resolves through GitHub's...
// More: docs/code-comments.md (companion-app/source/update_check.c:26)
```

Original (lines 26-41):

```
// ---------------- update source: GitHub, or a local test server ----------------
//
// Normally every download resolves through GitHub's release API (net_http.c). For testing builds
// without publishing a release, put this in the ini the plugin already reads:
//
//   [dev]
//   update_base_url=http://192.168.2.117:8000
//
// and every download (the plugin, the shell feed .prx, the Ambilight payload) and its ".sha256" sidecar is
// fetched from <base>/<file name> instead -- e.g. `python tools/serve_local_release.py`, which
// serves a folder with python's http.server. Delete the line to go back to GitHub. Read fresh each
// time, so editing the ini needs no restart. [dev] already exists for dev_ip/dev_logging, and the
// app's own save leaves unknown keys alone, so this survives a Save.
//
// Plain http:// through sceHttpCreateConnectionWithURL works on a console (checked with the update
// check, installs and the checksum failure paths).
```

### companion-app/source/update_check.c:55

In the code now:

```
// Compares what's currently installed against the checksum published for the latest release, reusing...
// More: docs/code-comments.md (companion-app/source/update_check.c:55)
```

Original (lines 55-66):

```
// Compares what's currently installed against the checksum published
// for the latest release, reusing the exact same asset-resolution
// path do_plugin_update() uses to fetch it. Only meaningful once
// plugin_registration_state() has already returned UI_INSTALL_OK --
// NONE/DISABLED take priority in the UI over this, so callers should
// only invoke this when the plugin is both present and registered.
//
// Best-effort/fail-quiet: any network or I/O failure here (no
// connection, GitHub unreachable, asset renamed, ...) just means "no
// update reported this session" rather than an error shown to the
// user -- this runs unprompted on every launch, and a routine offline
// PS4 shouldn't see a scary status message for it.
```

### companion-app/source/update_check.c:69

In the code now:

```
// Hash the real .prx on disk first, not the cached sidecar -- the sidecar only gets written by...
// More: docs/code-comments.md (companion-app/source/update_check.c:69)
```

Original (lines 69-79):

```
// Hash the real .prx on disk first, not the cached sidecar --
// the sidecar only gets written by do_plugin_update() after an
// in-app install, so it goes stale (and silently wrong) the
// moment someone drops a different build in by hand, e.g. FTPing
// an older .prx over it. Falling back to the sidecar only when
// hashing the real file fails means a manual replacement always
// gets compared honestly against the real bytes now sitting at
// PLUGIN_PRX_PATH, instead of whatever this app last installed.
// Re-hashing on every check used to matter when this ran inline
// on the main thread at startup; now that it's a background-
// thread check, that cost isn't worth trading correctness for.
```

### companion-app/source/update_check.c:108

In the code now:

```
// Same question as check_for_plugin_update(), for one of the two shell feed files: is what is on disk...
// More: docs/code-comments.md (companion-app/source/update_check.c:108)
```

Original (lines 108-111):

```
// Same question as check_for_plugin_update(), for one of the two shell feed files: is what is on
// disk what the update source publishes? Returns 1 = newer build available, 0 = up to date,
// -1 = couldn't tell (not installed, no network, source unreachable) -- which callers treat as
// "nothing to report", same fail-quiet rule as the plugin check.
```

### companion-app/source/update_check.c:139  (kept in code as a summary)

In the code now:

```
// The update check runs on its own thread with an explicit 256 KB stack, polled once a frame via an atomic flag.
// SDL_CreateThread's default stack overflowed on console at startup; do not go back to it. [docs/code-comments.md]
```

Original (lines 139-180):

```
// ---------------- background update check ----------------
//
// check_for_plugin_update() above does two sequential blocking HTTPS
// round trips to GitHub. Running it inline on the main thread at
// startup -- as this used to do -- held the whole UI hostage behind
// the network on every single launch, which is what actually produced
// the "app feels slow to open" latency: even on a fast connection
// that's a DNS+TLS+request/response pair twice in a row before the
// home screen becomes interactive, and on a bad connection it's the
// full 10s connect/resolve/send timeout, more than once.
//
// Fixed by kicking the check off on its own thread right after the
// home screen is up, and polling a plain atomic flag once a frame
// (poll_background_update_check(), called from the main loop below)
// instead of blocking anything. SDL_atomic_t is used rather than a
// plain bool specifically so the main thread's read and the
// background thread's write can't tear -- g_state itself is still
// only ever touched from the main thread, in the polling function.
//
// CRASH FIX: the first version of this used SDL_CreateThread(), which
// on a stack size of 0 means "whatever this platform's default is" --
// SDL's own docs warn that default can be "wildly different between
// platforms... a few kilobytes" on some. On real hardware this
// crashed on startup every time: a SIGSEGV write fault at exactly
// rsp-8, with a huge rbp/rsp gap for only a few stack frames rooted at
// a fresh thread-entry trampoline -- the textbook shape of overrunning
// a too-small thread stack, not a bad pointer. github_resolve_asset_
// api_url() above already treats a 64KB buffer as "too large to
// comfortably put on this thread's stack", so a platform default small
// enough to blow past on this call chain isn't a stretch. Switched to
// the plugin side's own scePthreadCreate pattern (see plugin/source/
// main.c's ambient_sample_thread call, confirmed against real
// hardware there) with an explicit stack size instead of trusting an
// unknown SDL-port default. UPDATE_CHECK_THREAD_STACK_SIZE reuses the
// same 256KB this file already trusts sceSslInit() with, rather than
// inventing a new number.
//
// Deliberately defined after check_for_plugin_update() rather than
// forward-declared -- this file has no function forward declarations
// anywhere else and relies entirely on top-to-bottom definition
// order, so this follows that same convention instead of introducing
// a new one.
```

### companion-app/source/update_check.c:208

In the code now:

```
// scePthreadCreate's expected entry-point shape (void *(*)(void *)), same as...
// More: docs/code-comments.md (companion-app/source/update_check.c:208)
```

Original (lines 208-211):

```
// scePthreadCreate's expected entry-point shape (void *(*)(void *)),
// same as plugin/source/sample_thread.c's ambient_sample_thread --
// not SDL_CreateThread's int(*)(void*), since this no longer goes
// through SDL to create the thread.
```

### companion-app/source/update_check.c:238

In the code now:

```
// Fires the check off in the background.
// More: docs/code-comments.md (companion-app/source/update_check.c:238)
```

Original (lines 238-241):

```
// Fires the check off in the background. Safe to call even when
// g_state.install isn't UI_INSTALL_OK -- callers still gate on that
// the same way the old inline call did, this just doesn't block them
// while it runs.
```

### companion-app/source/update_check.c:244

In the code now:

```
// What there is to check right now: the plugin when it is installed and enabled (NONE/DISABLED have...
// More: docs/code-comments.md (companion-app/source/update_check.c:244)
```

Original (lines 244-246):

```
// What there is to check right now: the plugin when it is installed and enabled (NONE/DISABLED
// have their own button and outrank "is there something newer"), and each shell feed file that
// is on disk, but only with the Shell feed switch on -- off means nothing shell-related runs.
```

### companion-app/source/update_check.c:272

In the code now:

```
// scePthreadCreate copies whatever it needs out of attr (same as POSIX pthread_create) -- safe to...
// More: docs/code-comments.md (companion-app/source/update_check.c:272)
```

Original (lines 272-274):

```
// scePthreadCreate copies whatever it needs out of attr (same as
// POSIX pthread_create) -- safe to destroy right after, whether or
// not the create call above actually succeeded.
```

### companion-app/source/update_check.c:277

In the code now:

```
// If thread creation itself failed, g_updateCheckStarted stays false and...
// More: docs/code-comments.md (companion-app/source/update_check.c:277)
```

Original (lines 277-281):

```
// If thread creation itself failed, g_updateCheckStarted stays
// false and poll_background_update_check() below just never finds
// anything to report -- same "fail quiet" behavior
// check_for_plugin_update() already documents for network/GitHub
// failures.
```

### companion-app/source/update_check.c:284

In the code now:

```
// Called once a frame from the main loop.
// More: docs/code-comments.md (companion-app/source/update_check.c:284)
```

Original (lines 284-286):

```
// Called once a frame from the main loop. Cheap when nothing's ready
// yet (one atomic read), and only ever touches g_state/set_status
// from the main thread once the background thread has finished.
```

### companion-app/source/update_check.c:305

In the code now:

```
// Only overwrite a still-neutral "Checking for updates..." -- if something else already changed the...
// More: docs/code-comments.md (companion-app/source/update_check.c:305)
```

Original (lines 305-308):

```
// Only overwrite a still-neutral "Checking for updates..." --
// if something else already changed the status line in the
// meantime (a save, an error, the user starting an install),
// leave it alone rather than stomping on it late.
```

## companion-app/tests/stubs/orbis/ImeDialog.h

### companion-app/tests/stubs/orbis/ImeDialog.h:1

In the code now:

```
// TEST-ONLY stand-in for the OpenOrbis SDK's <orbis/ImeDialog.h> (and the dialog types from...
// More: docs/code-comments.md (companion-app/tests/stubs/orbis/ImeDialog.h:1)
```

Original (lines 1-5):

```
// TEST-ONLY stand-in for the OpenOrbis SDK's <orbis/ImeDialog.h> (and the dialog types from
// CommonDialog.h), so source/ime.c builds on a PC. It declares only what ime.c uses, with the
// SDK's names; tests/test_ime.c supplies fake sceIme* / sceCommonDialog* functions that play the
// part of the system keyboard. It checks ime.c's own logic, not the real SDK's types, which only
// the CI package build and a console exercise.
```

## companion-app/tests/test_action_buttons.c

### companion-app/tests/test_action_buttons.c:1

In the code now:

```
// Run from companion-app/. Build: gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -o...
// More: docs/code-comments.md (companion-app/tests/test_action_buttons.c:1)
```

Original (lines 1-7):

```
// Run from companion-app/. Build:
//   gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -o /tmp/test_action_buttons \
//       tests/test_action_buttons.c source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c
//
// Every FIELD_ACTION button has to say what it does. The payload button once read "Reset Game"
// because the drawing code gave every action the preset reset's text; this keeps that from
// coming back, and keeps the "Ambilight payload" name (not "loader" / "launcher") in the menu.
```

## companion-app/tests/test_app_version.c

### companion-app/tests/test_app_version.c:1

In the code now:

```
// Run from companion-app/. Build: gcc -w -Iinclude -o /tmp/test_app_version tests/test_app_version.c...
// More: docs/code-comments.md (companion-app/tests/test_app_version.c:1)
```

Original (lines 1-5):

```
// Run from companion-app/. Build:
//   gcc -w -Iinclude -o /tmp/test_app_version tests/test_app_version.c && /tmp/test_app_version
//
// APP_VERSION feeds the package's APP_VER and VERSION (CI and build.bat both parse the header
// with a pattern, see app_version.h) and the Help screen. This keeps its form what both expect.
```

## companion-app/tests/test_claim_file.c

### companion-app/tests/test_claim_file.c:1

In the code now:

```
// Run from companion-app/: gcc -Wall -Wextra -Iinclude -I../plugin/include -o /tmp/test_claim_file...
// More: docs/code-comments.md (companion-app/tests/test_claim_file.c:1)
```

Original (lines 1-3):

```
// Run from companion-app/:
//   gcc -Wall -Wextra -Iinclude -I../plugin/include -o /tmp/test_claim_file tests/test_claim_file.c && /tmp/test_claim_file
// The claim record against the plugin's REAL state_format(), and the heartbeat logic on a clock.
```

## companion-app/tests/test_github_release_json.c

### companion-app/tests/test_github_release_json.c:1

In the code now:

```
// Run from companion-app/. Build: gcc -w -Iinclude -o /tmp/test_github_release_json...
// More: docs/code-comments.md (companion-app/tests/test_github_release_json.c:1)
```

Original (lines 1-7):

```
// Run from companion-app/. Build:
//   gcc -w -Iinclude -o /tmp/test_github_release_json tests/test_github_release_json.c && /tmp/test_github_release_json
//
// github_json_find_asset_url() (include/github_release_json.h) pulls one asset's API URL out of
// GitHub's "latest release" JSON with a hand-rolled scan, not a JSON parser. These feed it
// release JSON shaped like GitHub's (an assets array, each asset listing "url" before "name", with
// a nested "uploader" object) and check what it picks, and what it refuses.
```

## companion-app/tests/test_ime.c

### companion-app/tests/test_ime.c:1

In the code now:

```
// Run from companion-app/. Build (needs the /tmp/stub/orbis/libkernel.h from testing.md too): gcc -w...
// More: docs/code-comments.md (companion-app/tests/test_ime.c:1)
```

Original (lines 1-8):

```
// Run from companion-app/. Build (needs the /tmp/stub/orbis/libkernel.h from testing.md too):
//   gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -Itests/stubs -o /tmp/test_ime \
//       tests/test_ime.c source/ime.c source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c
//
// source/ime.c was split out of main.c. This plays the part of the system keyboard with fake
// sceImeDialog* functions (tests/stubs/orbis/ImeDialog.h declares their types) and checks what
// ime.c does with it: what it opens, what it applies, what it refuses. It cannot check the real
// SDK's behaviour, only ime.c's own logic.
```

## companion-app/tests/test_layout.c

### companion-app/tests/test_layout.c:1

In the code now:

```
// test_layout.c -- pure-logic isolation test for layout.c, same convention as...
// More: docs/code-comments.md (companion-app/tests/test_layout.c:1)
```

Original (lines 1-3):

```
// test_layout.c -- pure-logic isolation test for layout.c, same
// convention as test_pipeline.c/test_settings.c: gcc -Iinclude -o
// test_layout test_layout.c source/layout.c && ./test_layout
```

### companion-app/tests/test_layout.c:27

In the code now:

```
// 1) Total count must equal the sum of all 4 sides -- this is the direct regression test for the...
// More: docs/code-comments.md (companion-app/tests/test_layout.c:27)
```

Original (lines 27-29):

```
// 1) Total count must equal the sum of all 4 sides -- this is the
// direct regression test for the original bug: the old preview
// only ever sent 5 swatches no matter what these counts were.
```

### companion-app/tests/test_layout.c:38

In the code now:

```
// 2) Zero-LED sides must not appear, and the four zones must together account for every slot (nothing...
// More: docs/code-comments.md (companion-app/tests/test_layout.c:38)
```

Original (lines 38-40):

```
// 2) Zero-LED sides must not appear, and the four zones must
// together account for every slot (nothing dropped, nothing
// duplicated).
```

### companion-app/tests/test_layout.c:76

In the code now:

```
// 5) ledOffset rotation should be a pure permutation -- same set of positions, just starting at a...
// More: docs/code-comments.md (companion-app/tests/test_layout.c:76)
```

Original (lines 76-78):

```
// 5) ledOffset rotation should be a pure permutation -- same set
// of positions, just starting at a different index -- and offset
// N (== total count) should be a full-circle no-op.
```

### companion-app/tests/test_layout.c:99

In the code now:

```
// 6) Positions must stay roughly within the [padding, dim-padding] box on both axes.
// More: docs/code-comments.md (companion-app/tests/test_layout.c:99)
```

Original (lines 99-108):

```
// 6) Positions must stay roughly within the [padding, dim-padding]
// box on both axes. Tolerance is half a cell's pitch, not zero:
// the upstream Android formula (padding + i*step + step/2, ported
// unchanged here on purpose to match the phone app's own preview)
// centers each LED's box on a fencepost grid, so the very last
// LED on a densely-packed edge can sit up to half a cell-width
// past the nominal edge -- a pre-existing cosmetic quirk in the
// reference implementation, not something introduced by this
// port. A real overflow (positions wildly outside the box) would
// still be caught by this check.
```

### companion-app/tests/test_layout.c:129

In the code now:

```
// 7) BUGFIX regression: real-hardware report was that changing startCorner/direction/ledOffset rotated...
// More: docs/code-comments.md (companion-app/tests/test_layout.c:129)
```

Original (lines 129-145):

```
// 7) BUGFIX regression: real-hardware report was that changing
// startCorner/direction/ledOffset rotated the on-screen preview
// rectangle but produced zero visible change on the real strip.
// Root cause was hue being assigned by array index (post-reorder)
// instead of by the slot's actual (cx,cy,zone) position -- DDP
// offset i is a fixed physical bulb, so a color that only depends
// on i never changes no matter what gets reordered into that
// slot. Two things must now both be true:
//   (a) the SAME physical location gets the SAME hue regardless
//       of startCorner/direction/ledOffset (hue is tied to
//       position, not to where settings happen to place it in
//       the array)
//   (b) wire-offset 0's hue DOES change between two different
//       startCorner settings (this is the actual user-visible
//       fix -- a fixed physical bulb now shows a different color
//       when you change these settings, which is the whole point
//       of a *live* preview for tuning them)
```

### companion-app/tests/test_layout.c:154

In the code now:

```
// (a) find a slot in A and a slot in B that land at (nearly) the same physical position but via...
// More: docs/code-comments.md (companion-app/tests/test_layout.c:154)
```

Original (lines 154-156):

```
// (a) find a slot in A and a slot in B that land at (nearly)
// the same physical position but via different settings, and
// confirm they get the same hue.
```

### companion-app/tests/test_layout.c:176

In the code now:

```
// (b) wire-offset 0 (the very first LED sent, DDP pixel 0 -- a fixed physical bulb on the real strip)...
// More: docs/code-comments.md (companion-app/tests/test_layout.c:176)
```

Original (lines 176-178):

```
// (b) wire-offset 0 (the very first LED sent, DDP pixel 0 --
// a fixed physical bulb on the real strip) must get a
// different hue under the two different settings.
```

## companion-app/tests/test_payload_ini.c

### companion-app/tests/test_payload_ini.c:1

In the code now:

```
// Run from companion-app/: gcc -Wall -Wextra -Iinclude -o /tmp/test_payload_ini...
// More: docs/code-comments.md (companion-app/tests/test_payload_ini.c:1)
```

Original (lines 1-3):

```
// Run from companion-app/: gcc -Wall -Wextra -Iinclude -o /tmp/test_payload_ini tests/test_payload_ini.c && /tmp/test_payload_ini
// The only thing that edits GoldHEN's payload AutoRun list: other entries, comments and
// line endings must come through byte for byte.
```

## companion-app/tests/test_presets.c

### companion-app/tests/test_presets.c:235

In the code now:

```
// golden file: what the app writes for a fresh setup, byte for byte tools/test_plugin_config.c loads...
// More: docs/code-comments.md (companion-app/tests/test_presets.c:235)
```

Original (lines 235-237):

```
// ---- golden file: what the app writes for a fresh setup, byte for byte ----
// tools/test_plugin_config.c loads the same file through the plugin's real
// loader, so the app's writer and the plugin's reader can't drift apart.
```

## companion-app/tests/test_sce_stubs.c

### companion-app/tests/test_sce_stubs.c:1

In the code now:

```
// TEST-ONLY stubs for sceKernel* file I/O, so config.c (which calls these directly, matching the real...
// More: docs/code-comments.md (companion-app/tests/test_sce_stubs.c:1)
```

Original (lines 1-6):

```
// TEST-ONLY stubs for sceKernel* file I/O, so config.c (which calls
// these directly, matching the real plugin's fopen-crash fix) can be
// compiled and tested on this Linux sandbox. NOT part of the shipped
// app -- the real PS4 build gets these from orbis/libkernel.h via the
// actual toolchain, same as every other sceKernel* call in this
// entire project.
```

## companion-app/tests/test_settings.c

### companion-app/tests/test_settings.c:47

In the code now:

```
// The relay_* keys have no UI row and settings_save only writes them back when the file already has...
// More: docs/code-comments.md (companion-app/tests/test_settings.c:47)
```

Original (lines 47-49):

```
// The relay_* keys have no UI row and settings_save only writes them back when the file
// already has them (a hand-added relay setup is kept, a fresh ini never grows one). So seed
// the file with all three, from scratch each run, and check the new values replace them.
```

### companion-app/tests/test_settings.c:101

In the code now:

```
// relay fields are deliberately NOT in kMenuItems (no settings-UI row for them, same as v17)...
// More: docs/code-comments.md (companion-app/tests/test_settings.c:101)
```

Original (lines 101-104):

```
// relay fields are deliberately NOT in kMenuItems (no settings-UI
// row for them, same as v17) -- confirm that's still true, since a
// future menu-schema change accidentally re-adding one of these
// three would be easy to miss otherwise.
```

## companion-app/tests/test_sha256.c

### companion-app/tests/test_sha256.c:23

In the code now:

```
// Standard SHA-256 test vectors (empty string and "abc"; both cross-checked against Python's...
// More: docs/code-comments.md (companion-app/tests/test_sha256.c:23)
```

Original (lines 23-25):

```
// Standard SHA-256 test vectors (empty string and "abc"; both
// cross-checked against Python's hashlib.sha256, not typed from
// memory).
```

### companion-app/tests/test_sha256.c:32

In the code now:

```
// Multi-block input (forces sha256_update across internal 64-byte transform boundaries) -- three...
// More: docs/code-comments.md (companion-app/tests/test_sha256.c:32)
```

Original (lines 32-34):

```
// Multi-block input (forces sha256_update across internal 64-byte
// transform boundaries) -- three concatenated calls, verified
// against Python's hashlib.sha256(rep * 3).
```

## companion-app/tests/test_shell_settings.c

### companion-app/tests/test_shell_settings.c:1

In the code now:

```
// Run from companion-app/ (same flags as test_presets): gcc -w -I. -Iinclude -I../plugin/include...
// More: docs/code-comments.md (companion-app/tests/test_shell_settings.c:1)
```

Original (lines 1-5):

```
// Run from companion-app/ (same flags as test_presets):
//   gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -o /tmp/test_shell_settings tests/test_shell_settings.c \
//       source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c && /tmp/test_shell_settings
// The ini side of the shell feed: [shell] enabled round-trips, other sections (and the new
// [dev] update_base_url) survive a save, and the autoload row never reaches the ini.
```

## companion-app/tools/sce_file_stubs.c

### companion-app/tools/sce_file_stubs.c:1

In the code now:

```
// Desktop stand-ins for sceKernel* file I/O, so config.c (which calls these directly, matching the...
// More: docs/code-comments.md (companion-app/tools/sce_file_stubs.c:1)
```

Original (lines 1-6):

```
// Desktop stand-ins for sceKernel* file I/O, so config.c (which calls
// these directly, matching the real plugin's fopen-crash fix) can be
// compiled on a PC by tools/ui_preview.sh. NOT part of the shipped
// app -- the real PS4 build gets these from orbis/libkernel.h via the
// actual toolchain, same as every other sceKernel* call in this
// entire project.
```

## companion-app/tools/ui_preview.c

### companion-app/tools/ui_preview.c:1

In the code now:

```
// ui_preview.c -- render the companion's real UI code to PNGs on a PC, so layout changes can be looked...
// More: docs/code-comments.md (companion-app/tools/ui_preview.c:1)
```

Original (lines 1-9):

```
// ui_preview.c -- render the companion's real UI code to PNGs on a PC, so layout changes can be
// looked at without a console build. Same sources the PS4 build compiles; only main.c (SDL, pads,
// sockets) and the Orbis FreeType module load are left out.
//
//   tools/ui_preview.sh /tmp/ui     # builds this and writes /tmp/ui/*.png
//
// It does NOT prove the console looks identical (fonts are the same files, but this is a desktop
// FreeType), it proves the layout: what fits, what wraps, whether two banners match, where the
// button sits.
```

## plugin/Makefile

### plugin/Makefile:24

In the code now:

```
# O_FLAG intentionally left undefined by default (matches how this plugin was actually built and...
# More: docs/code-comments.md (plugin/Makefile:24)
```

Original (lines 24-30):

```
# O_FLAG intentionally left undefined by default (matches how this plugin
# was actually built and confirmed crash-free on real hardware) --
# override explicitly on the command line if you want one, e.g.
# `make O_FLAG=-O2`, but be aware this plugin's raw kernel hooks and
# volatile-shared state between the hook and the worker thread have not
# been verified crash-free at any optimization level above the compiler
# default.
```

### plugin/Makefile:72

In the code now:

```
# Native Windows (MSYS2/Git Bash/etc). $(OS) is set to "Windows_NT" by the Windows environment itself...
# More: docs/code-comments.md (plugin/Makefile:72)
```

Original (lines 72-76):

```
# Native Windows (MSYS2/Git Bash/etc). $(OS) is set to "Windows_NT" by the
# Windows environment itself, unlike uname -s (which reports MSYS_NT-...,
# MINGW64_NT-..., or similar depending on shell) -- checking $(OS) instead
# is shell-independent. Requires clang/clang++/ld.lld from an LLVM for
# Windows install (https://github.com/llvm/llvm-project/releases) on PATH.
```

## plugin/include/ambient_internal.h

### plugin/include/ambient_internal.h:1

In the code now:

```
// ambient_internal.h -- shared types, config path, and cross-module externs for ps4_ambient_light...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:1)
```

Original (lines 1-11):

```
// ambient_internal.h -- shared types, config path, and cross-module
// externs for ps4_ambient_light, split out of what used to be one
// 3240-line main.c (see main.c's own top comment for the file's real
// history/scope notes -- this header only exists to let that same
// code compile as separate translation units; it changes no
// behavior). Every struct/enum/global declared here was previously a
// file-local `static` inside main.c -- promoted to extern linkage
// ONLY where a different .c file genuinely needed it, nothing more.
//
// Not a public API: this is glue between this plugin's OWN source
// files, not something another plugin or the companion app includes.
```

### plugin/include/ambient_internal.h:24

In the code now:

```
// [settings] -- AmbientConfig struct, g_config instance lives in settings.c. See settings.c for the...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:24)
```

Original (lines 24-28):

```
// ------------------------------------------------------------
// [settings] -- AmbientConfig struct, g_config instance lives in
// settings.c. See settings.c for the defaults and the full field-by-
// field history (why each default is what it is).
// ------------------------------------------------------------
```

### plugin/include/ambient_internal.h:40

In the code now:

```
// v2.3: separate destination for the external-source on/off signal (see relay_send_external_source...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:40)
```

Original (lines 40-44):

```
// v2.3: separate destination for the external-source on/off signal
// (see relay_send_external_source below) -- the relay that owns
// this (wled-relay's tv_external_source.py) runs on its own host,
// NOT the WLED controller itself, so this can't just reuse
// wledHost/wledPort.
```

### plugin/include/ambient_internal.h:47

In the code now:

```
// v2.3.1: this whole external-source signal is a personal-setup integration with one specific...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:47)
```

Original (lines 47-51):

```
// v2.3.1: this whole external-source signal is a personal-setup
// integration with one specific wled-relay project, not something
// every user of this plugin has (or should have to configure a
// dead host for) -- OFF by default, only sends anything if a user
// explicitly opts in via the ini.
```

### plugin/include/ambient_internal.h:59

In the code now:

```
// v3.0: auto letterbox/black-bar detection is now the ONLY inset applied before sampling -- the old...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:59)
```

Original (lines 59-69):

```
// v3.0: auto letterbox/black-bar detection is now the ONLY inset
// applied before sampling -- the old manual capture_margin_{top,
// right,bottom,left} fields were removed (see CHANGELOG). Almost
// everything plays fullscreen with no letterboxing at all, in
// which case this correctly detects and applies zero margin; the
// cases it exists for -- cutscenes, aspect-ratio-locked menus, a
// status bar along one edge -- are exactly the cases a fixed
// manual number couldn't handle anyway (it's either wrong during
// the bar, or wrong for the other 99% of gameplay that has none).
// On by default for that reason: disabling it now means sampling
// starts at the true pixel 0/edge on every side, unconditionally.
```

### plugin/include/ambient_internal.h:82

In the code now:

```
// v2.2: ported from the Android "inspiration" project's own ColorProcessor.kt.
// More: docs/code-comments.md (plugin/include/ambient_internal.h:82)
```

Original (lines 82-89):

```
// v2.2: ported from the Android "inspiration" project's own
// ColorProcessor.kt. These are ADDITIONS on top
// of the existing brightness/gamma fields above, not replacements
// -- brightness/gamma keep their original 0-255 / fixed-LUT-string
// meaning so nobody's existing ini silently changes behavior.
// Percentages below use Android's own convention (100 = neutral),
// NOT this file's usual "0 = neutral" convention -- documented
// per-field in the generated ini template.
```

### plugin/include/ambient_internal.h:93

In the code now:

```
// before zone-averaging (see sampleZoneAverage) -- unlike every other knob here, which is applied once...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:93)
```

Original (lines 93-97):

```
// before zone-averaging (see sampleZoneAverage) -- unlike every other
// knob here, which is applied once to the already-averaged zone color.
// This matters because gamma is non-linear: correcting-then-averaging
// and averaging-then-correcting are NOT the same operation, and the
// Android source applies its gamma per-pixel, before any downsampling.
```

### plugin/include/ambient_internal.h:98

In the code now:

```
// v2.2.5: opt-in developer telemetry destination.
// More: docs/code-comments.md (plugin/include/ambient_internal.h:98)
```

Original (lines 98-106):

```
// v2.2.5: opt-in developer telemetry destination. Deliberately NOT
// part of ambient_create_default_config()'s generated template
// (see AMBIENT_DEFAULT_INI below) -- a person has to type a [dev]
// section into their own ini by hand for this to ever do anything.
// Empty devIp / false devLoggingEnabled (the zero-init default,
// same as every other field in this struct before a real ini is
// read) means debug_send_raw() sends nothing at all, full stop --
// see its own comment for why this check lives there and not
// scattered across each call site.
```

### plugin/include/ambient_internal.h:109

In the code now:

```
// [notifications] enabled. Optional, default true, not in the shipped ini (hand-added, like [compat])....
// More: docs/code-comments.md (plugin/include/ambient_internal.h:109)
```

Original (lines 109-112):

```
// [notifications] enabled. Optional, default true, not in the shipped ini (hand-added, like [compat]).
// false silences the notification bar messages that are only information; the ones the strip can't
// work without (no WLED address, an ini value ignored, debug logging left on) are sent regardless.
// See notices.c and the shell feed's sh_notify().
```

### plugin/include/ambient_internal.h:124

In the code now:

```
// v3.9.4: force ambient_load_config() to use a specific [preset_*] section regardless of [presets]...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:124)
```

Original (lines 124-133):

```
// v3.9.4: force ambient_load_config() to use a specific [preset_*] section
// regardless of [presets] active= or the title's own movie/game guess, IF
// that section exists in the ini (falls through to the normal pick
// otherwise -- same "use the other one if the first has no keys" rule
// preset_section_pick() already follows). NULL (the default) means "no
// override, behave exactly as before" -- this plugin never sets it itself.
// It exists for a second, in-repo plugin (shell-feed/) that always wants
// [preset_game], never [preset_movie], no matter what the ini's active=
// says, since active=movie would otherwise turn the shell screens movie
// too. See preset_select.h for PRESET_SECTION_GAME/PRESET_SECTION_MOVIE.
```

### plugin/include/ambient_internal.h:136

In the code now:

```
// [gamma] -- kGammaLuts is the fixed 8-preset table (gamma.c); the per-channel g_gammaLutR/G/B are the...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:136)
```

Original (lines 136-140):

```
// [gamma] -- kGammaLuts is the fixed 8-preset table (gamma.c); the
// per-channel g_gammaLutR/G/B are the continuous, config-driven ones
// (also gamma.c) -- see that file for why both exist. Used directly
// by color_processing.c (kGammaLuts) and zones.c's sampleZoneAverage
// (g_gammaLutR/G/B, applied per-sample before averaging).
```

### plugin/include/ambient_internal.h:179

In the code now:

```
// v2.8: called only from sample_thread.c's no-valid-frame branch (unknown pixel format while...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:179)
```

Original (lines 179-181):

```
// v2.8: called only from sample_thread.c's no-valid-frame branch (unknown
// pixel format while foregrounded) -- NOT from the g_isBackgrounded path,
// where going silent is intentional. See network.c for the full comment.
```

### plugin/include/ambient_internal.h:189

In the code now:

```
// v2.4: sceSystemServiceGetStatus's real field layout, confirmed against shadPS4's independently...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:189)
```

Original (lines 189-192):

```
// v2.4: sceSystemServiceGetStatus's real field layout, confirmed
// against shadPS4's independently reverse-engineered source -- see
// network.c for the full verification history/caveats on this struct.
// Resolved by name in main.c's plugin_load, read by sample_thread.c.
```

### plugin/include/ambient_internal.h:218

In the code now:

```
// How many bytes from the buffer base are contiguously CPU-readable right now (set by...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:218)
```

Original (lines 218-221):

```
// How many bytes from the buffer base are contiguously CPU-readable right now (set by
// ambient_buffer_readable). Every pixel read is bounds-checked against this instead of the
// fixed 1088-row ceiling, so a buffer shorter than the padded size skips its last pixels
// instead of faulting or being rejected outright.
```

### plugin/include/ambient_internal.h:234

In the code now:

```
// [report] -- v3.9.1, report.c. The per-title report file a tester sends instead of a capture.
// More: docs/code-comments.md (plugin/include/ambient_internal.h:234)
```

Original (lines 234-237):

```
// [report] -- v3.9.1, report.c. The per-title report file a tester sends instead of a capture.
// AMBIENT_VERSION_STRING is the full version; g_pluginVersion (hooks.c) only holds major.minor,
// because CI compares a tag's major.minor against it. tools/test_report.c fails if the two
// disagree on major.minor.
```

### plugin/include/ambient_internal.h:245

In the code now:

```
// [notices] -- notices.c, plugin only.
// More: docs/code-comments.md (plugin/include/ambient_internal.h:245)
```

Original (lines 245-247):

```
// [notices] -- notices.c, plugin only. System-notification-bar messages for things that otherwise
// fail silently (no WLED host, bad ini values, a title the plugin can't read, ...). The inputs
// below are set by settings.c (shared with the shell feed, so it only sets flags, never calls out).
```

### plugin/include/ambient_internal.h:276

In the code now:

```
// zones.c's detectHdr2200Format calls these two unpack functions directly (not through the...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:276)
```

Original (lines 276-278):

```
// zones.c's detectHdr2200Format calls these two unpack functions
// directly (not through the function-pointer dispatch above) to try
// both hypotheses itself -- see that function for why.
```

### plugin/include/ambient_internal.h:281

In the code now:

```
// v2.8 HDR-vs-SDR auto-detect state for format 0x80002200 -- written by zones.c's detectHdr2200Format...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:281)
```

Original (lines 281-283):

```
// v2.8 HDR-vs-SDR auto-detect state for format 0x80002200 -- written by
// zones.c's detectHdr2200Format, read by pixel_formats.c's
// getUnpackFnForFormat. See pixel_formats.c for the full method.
```

### plugin/include/ambient_internal.h:292

In the code now:

```
// v3.5: 0x88740000 with 8-bit ARGB in the buffer (YouTube, SDR video, HDR on).
// More: docs/code-comments.md (plugin/include/ambient_internal.h:292)
```

Original (lines 292-294):

```
// v3.5: 0x88740000 with 8-bit ARGB in the buffer (YouTube, SDR video, HDR on).
// Set by zones.c's detectPq8bitMisregistration, read by pixel_formats.c's
// getUnpackFnForFormat. The vote itself is in pq8bit_vote.h.
```

### plugin/include/ambient_internal.h:300

In the code now:

```
// Upper bound for scanDepth (config's scan_depth). sampleZoneAverage in zones.c reads...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:300)
```

Original (lines 300-312):

```
// Upper bound for scanDepth (config's scan_depth). sampleZoneAverage in
// zones.c reads (2*scanDepth+1)^2 pixels PER ZONE, PER SAMPLE-THREAD
// PASS, each one a real getTiledElementByteOffset() tile-address
// computation -- not a flat read. At the unclamped old ceiling of 10,
// worst case is 512 zones (MAX_TOTAL_ZONES) x 441 samples x up to 240Hz
// (update_frequency_hz's own max) =~ 54M tiled-offset computations/sec.
// 4 caps that same worst case at 512 x 81 x 240 =~ 10M/sec -- still the
// upper end, but roughly a 5x cut -- while leaving real headroom over
// the default of 1 (9 samples) for anyone who actually wants a
// noticeably smoother average. Enforced here (settings.c's ini loader)
// so this holds regardless of how scan_depth got set -- a hand-edited
// ini, not just the companion app's own UI, which mirrors this same
// cap on its own Edge depth slider.
```

### plugin/include/ambient_internal.h:314

In the code now:

```
// Upper bound for saturation and contrast (both -100..MAX, 0 = unchanged).
// More: docs/code-comments.md (plugin/include/ambient_internal.h:314)
```

Original (lines 314-324):

```
// Upper bound for saturation and contrast (both -100..MAX, 0 = unchanged).
// This used to be 300 (4x), but the result gets clamped to 0-255 per
// channel at the end of the color chain, so most of that range just
// clipped. Simulated over sample colors: at +100 (2x) about 84% of vivid
// colors already have a channel clipped and each further +25 buys about
// a third of the visible change the first steps did; at 300, 99.8% of
// vivid colors clip. For contrast the same 2x factor already clips half
// the tonal range (1 - 1/2), and 4x clips about 75%. Unlike scan_depth,
// settings.c CLAMPS these two on load instead of rejecting an
// out-of-range value: a saved saturation=175 should land on the cap, not
// silently fall back to 0 (no saturation at all).
```

### plugin/include/ambient_internal.h:336

In the code now:

```
// [letterbox] -- auto black-bar detection (letterbox.c), the sole source of capture margin (see...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:336)
```

Original (lines 336-340):

```
// [letterbox] -- auto black-bar detection (letterbox.c), the sole
// source of capture margin (see zones.c's effMargin{Top,Right,Bottom,
// Left} helpers). All four default to 0 (no letterbox found, or the
// feature is off) until ambient_check_letterbox has committed a real
// border.
```

### plugin/include/ambient_internal.h:361

In the code now:

```
// How many flips back the sampler reads. 0 = the slot the hook just reported (can be cleared / half...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:361)
```

Original (lines 361-369):

```
// How many flips back the sampler reads. 0 = the slot the hook just reported
// (can be cleared / half drawn: causes flicker), 1 = previous flip (finished),
// 2 = two flips back. sample_thread.c reads g_prevDisplayBufferIndex or
// g_prevPrevDisplayBufferIndex accordingly -- each level needs its own
// tracked variable, there's no way to derive "N flips back" from just the
// current index. Default 2: at about 60 fps one flip back was not enough for
// the GPU to finish the slot (Red Dead Redemption), and a value above 1 did
// nothing at all before v3.7. Values above 2 behave as 2.
// See docs/debugging/sample-lag-and-boot-flash.md.
```

### plugin/include/ambient_internal.h:398

In the code now:

```
// The actual hook functions (hooks.c) and their Detour_* handles (created by HOOK_INIT in hooks.c)...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:398)
```

Original (lines 398-406):

```
// The actual hook functions (hooks.c) and their Detour_* handles
// (created by HOOK_INIT in hooks.c) both need to be visible from
// main.c's plugin_load/plugin_unload, since that's where the real
// HOOK32(name)/UNHOOK(name) macro invocations live -- HOOK32 expands
// to code referencing Detour_##name and (&name##_hook) directly, by
// textual substitution, so both symbols must be in scope at the call
// site, not just inside hooks.c. HOOK_EXTERN is the SDK's own macro
// for exactly this (Utilities.h) -- same pattern as HOOK_INIT, just
// extern instead of defining.
```

### plugin/include/ambient_internal.h:427

In the code now:

```
// TIMING_ENABLED gates the loop-time telemetry in sample_thread.c -- also referenced (as a plain #if...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:427)
```

Original (lines 427-429):

```
// TIMING_ENABLED gates the loop-time telemetry in sample_thread.c --
// also referenced (as a plain #if, not calling anything) by main.c's
// plugin_load for the still-unset CORE_MASK affinity comment block.
```

### plugin/include/ambient_internal.h:434

In the code now:

```
// g_smoothedRgb itself (the actual per-zone color array) stays private to sample_thread.c -- nothing...
// More: docs/code-comments.md (plugin/include/ambient_internal.h:434)
```

Original (lines 434-438):

```
// g_smoothedRgb itself (the actual per-zone color array) stays private
// to sample_thread.c -- nothing else touches it. Only the validity
// flag crosses files: settings.c's ambient_check_config_reload clears
// it on a live layout change (new zone count/positions shouldn't
// smooth-blend from the old ones).
```

## plugin/include/ambilight_paths.h

### plugin/include/ambilight_paths.h:1

In the code now:

```
// ambilight_paths.h -- the one folder everything this project writes on the console lives in.
// More: docs/code-comments.md (plugin/include/ambilight_paths.h:1)
```

Original (lines 1-15):

```
// ambilight_paths.h -- the one folder everything this project writes on the console lives in.
//
// Config, claim file, version marker, reports, update staging and the shell feed's files all sit in
// AMBILIGHT_DATA_DIR, so finding, backing up or removing the project's files is one folder. Nothing
// of ours goes loose in /data. What stays outside, because other software owns the location:
// /data/GoldHEN/ (plugins, plugins.ini, config.ini, payloads.ini) and /data/payloads/ (where
// GoldHEN looks for payload .bin files).
//
// Every other path macro (AMBIENT_CONFIG_PATH, STATE_PATH, REPORT_PATH_PREFIX, VERSION_MARKER,
// DISABLE_FILE, CLAIM_PATH, ...) is AMBILIGHT_DATA_DIR "/file", so moving the folder is this one
// line. The companion app builds with only its own include/ and keeps a matching copy in
// companion-app/include/app_paths.h; companion-app/tests/test_claim_file.c fails if they differ.
//
// The folder must exist before the first write. The companion app, the plugin (plugin_load) and
// the shell feed's installer each create it; creating an existing folder is not an error.
```

## plugin/include/config.h

### plugin/include/config.h:23

In the code now:

```
// @brief Creates an empty ini_table_s struct for writing new entries to. @return ini_table_s*
// More: docs/code-comments.md (plugin/include/config.h:23)
```

Original (lines 23-26):

```
/**
 * @brief Creates an empty ini_table_s struct for writing new entries to.
 * @return ini_table_s*
 */
```

### plugin/include/config.h:29

In the code now:

```
// @brief Free up all the allocated resources in the ini_table_s struct. @param table
// More: docs/code-comments.md (plugin/include/config.h:29)
```

Original (lines 29-32):

```
/**
 * @brief Free up all the allocated resources in the ini_table_s struct.
 * @param table
 */
```

### plugin/include/config.h:35

In the code now:

```
// @brief Creates an ini_table_s struct filled with data from the specified `file'.
// More: docs/code-comments.md (plugin/include/config.h:35)
```

Original (lines 35-41):

```
/**
 * @brief Creates an ini_table_s struct filled with data from the specified
 *        `file'.  Returns NULL if the file can not be read.
 * @param table
 * @param file
 * @return ini_table_s*
 */
```

### plugin/include/config.h:44

In the code now:

```
// @brief Writes the specified ini_table_s struct to the specified `file'.
// More: docs/code-comments.md (plugin/include/config.h:44)
```

Original (lines 44-51):

```
/**
 * @brief Writes the specified ini_table_s struct to the specified `file'.
 *        Returns false if the file could not be opened for writing, otherwise
 *        true.
 * @param table
 * @param file
 * @return bool
 */
```

### plugin/include/config.h:54

In the code now:

```
// @brief Creates a new entry in the `table' containing the `key' and `value' provided if it does not...
// More: docs/code-comments.md (plugin/include/config.h:54)
```

Original (lines 54-62):

```
/**
 * @brief Creates a new entry in the `table' containing the `key' and `value'
 *        provided if it does not exist.  Otherwise, modifies an exsiting `key'
 *        with the new `value'
 * @param table
 * @param section_name
 * @param key
 * @param value
 */
```

### plugin/include/config.h:65

In the code now:

```
// @brief Checks for the existance of an entry in the specified `table'.
// More: docs/code-comments.md (plugin/include/config.h:65)
```

Original (lines 65-72):

```
/**
 * @brief Checks for the existance of an entry in the specified `table'.  Returns
 *        false if the entry does not exist, otherwise true.
 * @param table
 * @param section_name
 * @param key
 * @return bool
 */
```

### plugin/include/config.h:75

In the code now:

```
// @brief Retrieves the unmodified value of the specified `key' in `section_name'.
// More: docs/code-comments.md (plugin/include/config.h:75)
```

Original (lines 75-83):

```
/**
 * @brief Retrieves the unmodified value of the specified `key' in `section_name'.
 *        Returns NULL if the entry does not exist, otherwise a pointer to the
 *        entry value data.
 * @param table
 * @param section_name
 * @param key
 * @return const char*
 */
```

### plugin/include/config.h:86

In the code now:

```
// @brief Retrieves the value of the specified `key' in `section_name', converted to int.
// More: docs/code-comments.md (plugin/include/config.h:86)
```

Original (lines 86-94):

```
/**
 * @brief Retrieves the value of the specified `key' in `section_name', converted
 *        to int.  Returns false on failure, otherwise true.
 * @param table
 * @param section_name
 * @param key
 * @param [out]value
 * @return int
 */
```

### plugin/include/config.h:97

In the code now:

```
// @brief Retrieves the value of the specified `key' in `section_name', converted to bool.
// More: docs/code-comments.md (plugin/include/config.h:97)
```

Original (lines 97-105):

```
/**
 * @brief Retrieves the value of the specified `key' in `section_name', converted
 *        to bool.  Returns false on failure, true otherwise.
 * @param table
 * @param section_name
 * @param key
 * @param [out]value
 * @return bool
 */
```

## plugin/include/default_ini.h

### plugin/include/default_ini.h:1

In the code now:

```
// default_ini.h -- the ini the plugin writes on first run when none exists.
// More: docs/code-comments.md (plugin/include/default_ini.h:1)
```

Original (lines 1-6):

```
// default_ini.h -- the ini the plugin writes on first run when none exists.
// Kept identical to plugin/config/ps4_ambient_light.ini (the copy people
// download) -- companion-app/tests/test_presets.c compares the two, and
// compares the two preset sections against the app's shipped presets, so a
// change to either side that forgets the other fails a test instead of
// shipping. Own header, plain C string, so a PC can build it.
```

## plugin/include/hdr2200_vote.h

### plugin/include/hdr2200_vote.h:1

In the code now:

```
// hdr2200_vote.h -- per-frame PQ-or-SDR vote for 0x80002200 buffers.
// More: docs/code-comments.md (plugin/include/hdr2200_vote.h:1)
```

Original (lines 1-3):

```
// hdr2200_vote.h -- per-frame PQ-or-SDR vote for 0x80002200 buffers. Its own
// header and nothing but stdint so tools/test_hdr2200_vote.c can build it on a
// PC. Why it works: docs/debugging/hdr2200-alpha-detection.md.
```

### plugin/include/hdr2200_vote.h:14

In the code now:

```
// +1: looks like A2R10G10B10 PQ. -1: looks like 8-bit SDR. 0: keep whatever mode is active (too few...
// More: docs/code-comments.md (plugin/include/hdr2200_vote.h:14)
```

Original (lines 14-32):

```
// +1: looks like A2R10G10B10 PQ. -1: looks like 8-bit SDR. 0: keep whatever
// mode is active (too few words, a mix in between, or a fade frame).
//
// A PQ word has 0b11 in bits 31:30 (opaque 2-bit alpha), so its top byte is
// 0b11RRRRRR. 0xff would need R10 >= 1008, red at 8,700 nits or more, and real
// content doesn't do that. In the captures every HDR-on word had a top byte of
// 0xc0..0xdd and 97% of the HDR-off words had 0xff. Unlike the smoothness
// detector this needs no scene content, so a black frame decides too: 0xc0000000
// is PQ black, 0xff000000 is SDR black. All-zero words are a cleared buffer
// and count for neither side.
//
// Fade guard. Hitman fades to and from black in SDR by writing a frame-wide
// alpha byte into the SDR words (0x11 up to 0xfe over about a second). Alphas
// 0xc1..0xfe pass the PQ test above. The 8 zones of one fade frame differ by a
// few counts (cc cb cb cb ca ca ca c8) because the game is still writing while
// we read, so the guard is a spread, not equality. Real PQ only clusters that
// tightly in 0xc1..0xfe when the scene is dark and flat, so the frame holds.
// Cost: a dark flat PQ frame seen while the mode is SDR holds until a frame
// with more variation arrives. Pure black (0xc0) is exempt and decides.
```

## plugin/include/media_titles.h

### plugin/include/media_titles.h:1

In the code now:

```
// media_titles.h -- title IDs that always run the Movie preset.
// More: docs/code-comments.md (plugin/include/media_titles.h:1)
```

Original (lines 1-15):

```
// media_titles.h -- title IDs that always run the Movie preset.
//
// Anything not listed runs whichever preset [presets] active= names (game by
// default). Add an ID here, rebuild, and that app switches to Movie by itself
// with no trip to the companion app. The ID is the one GoldHEN reports for
// the running process (sys_sdk_proc_info's titleid), CUSA plus five digits.
//
// Only IDs confirmed against independent sources belong here. A listed ID
// forces Movie (sampling depth 4, 200 ms smoothing) on whatever runs under
// it, so one that turns out to be a game is worse than a missing one --
// CUSA05682, for example, looks like a YouTube ID in some lists and is
// Horizon Zero Dawn. tools/test_plugin_config.c guards that one by name.
//
// Known but NOT listed, unconfirmed: YouTube TV (one catalogue gives CUSA18680).
// A YouTube beta build, CUSA06021, also exists and is left out.
```

## plugin/include/plugin_entry_alignment.h

### plugin/include/plugin_entry_alignment.h:1

In the code now:

```
// GoldHEN plugin callbacks can be entered through a loader call path whose x86-64 stack alignment is...
// More: docs/code-comments.md (plugin/include/plugin_entry_alignment.h:1)
```

Original (lines 1-6):

```
// GoldHEN plugin callbacks can be entered through a loader call path whose
// x86-64 stack alignment is not guaranteed. The PS4 libc's vectorized
// printf implementation uses MOVAPS and faults if a nested call inherits
// that misalignment. Keep these exported entry shims tiny and assembly-only:
// save the incoming frame, align RSP to 16 bytes, call the C implementation,
// then restore the exact incoming stack before returning to the loader.
```

## plugin/include/pq8bit_vote.h

### plugin/include/pq8bit_vote.h:1

In the code now:

```
// pq8bit_vote.h -- alpha byte vote for 0x88740000 buffers that actually hold 8-bit ARGB.
// More: docs/code-comments.md (plugin/include/pq8bit_vote.h:1)
```

Original (lines 1-3):

```
// pq8bit_vote.h -- alpha byte vote for 0x88740000 buffers that actually hold
// 8-bit ARGB. Its own header and nothing but stdint so tools/test_pq8bit_vote.c
// can build it on a PC. Why it works: docs/debugging/youtube-hdr-8bit.md.
```

### plugin/include/pq8bit_vote.h:12

In the code now:

```
// +1: looks like 8-bit A8R8G8B8. -1: looks like real A2R10G10B10 PQ. 0: keep whatever mode is active...
// More: docs/code-comments.md (plugin/include/pq8bit_vote.h:12)
```

Original (lines 12-19):

```
// +1: looks like 8-bit A8R8G8B8. -1: looks like real A2R10G10B10 PQ. 0: keep
// whatever mode is active (too few samples, or a mix in between).
//
// A real 10-bit word has alpha in bits 31:30, so an alpha BYTE of 0xff means
// R10 >= 1008, red at 8,700 nits or more. Real content doesn't do that, and
// real PQ black is 0xc0000000 or 0x00000000. 8-bit SDR content is 0xff on
// every pixel. Every check in the captures came out 8 of 8 or 0 of 8, so the
// 25%..75% band is there for a stray overlay pixel and hasn't been hit yet.
```

## plugin/include/preset_select.h

### plugin/include/preset_select.h:1

In the code now:

```
// preset_select.h -- which ini section supplies the preset-owned settings.
// More: docs/code-comments.md (plugin/include/preset_select.h:1)
```

Original (lines 1-12):

```
// preset_select.h -- which ini section supplies the preset-owned settings.
// Own header, string logic only.
//
// The ini holds two complete presets, [preset_game] and [preset_movie], and
// [presets] active= says which one runs. A title in media_titles.h runs movie
// whatever active says, because that is the whole point of having a preset
// per kind of content: Netflix gets its own smoothing without anyone opening
// the app.
//
// If the wanted preset has no section the other one is used, and if neither
// exists the answer is NULL, meaning "an ini from before v3.8, read the old
// flat [layout]/[color]/[timing] keys". So old inis keep working untouched.
```

## plugin/include/report_file.h

### plugin/include/report_file.h:1

In the code now:

```
// report_file.h -- the text of the per-title report file (v3.9.1).
// More: docs/code-comments.md (plugin/include/report_file.h:1)
```

Original (lines 1-11):

```
// report_file.h -- the text of the per-title report file (v3.9.1).
//
// A tester who reports "the strip stays dark in game X" has so far had to run a debug build
// and capture UDP packets. The release build now writes a short text file instead,
// /data/ps4-ambilight/ps4_ambient_report_<TITLEID>.txt, and the tester sends that. Everything in it is
// already kept in plain globals in every build; only the packets that send them are
// debug-only. It holds no personal data: no WLED address, no ini values, nothing typed.
//
// This header is string logic only, with no console calls, so tools/test_report.c can build
// it on a PC (same idea as preset_select.h and pq8bit_vote.h). report.c fills a
// ReportSnapshot from the live globals and does the file I/O.
```

### plugin/include/report_file.h:53

In the code now:

```
// Where the title has got to, from the counters alone.
// More: docs/code-comments.md (plugin/include/report_file.h:53)
```

Original (lines 53-55):

```
// Where the title has got to, from the counters alone. "running" means the sampler loop has
// completed at least one pass. A game that dies on its first pass is left at "flipping",
// which is why the file is written on every change and not only at the end.
```

### plugin/include/report_file.h:92

In the code now:

```
// True when something a person reading the file cares about moved: the stage, the format, or the...
// More: docs/code-comments.md (plugin/include/report_file.h:92)
```

Original (lines 92-94):

```
// True when something a person reading the file cares about moved: the stage, the format,
// or the guard/remap outcome. The per-frame counters (flips, passes, timing) are left out on
// purpose, or this would rewrite the file every second.
```

## plugin/include/smoothing.h

### plugin/include/smoothing.h:1

In the code now:

```
// smoothing.h -- the per-zone exponential smoothing, in one place.
// More: docs/code-comments.md (plugin/include/smoothing.h:1)
```

Original (lines 1-13):

```
// smoothing.h -- the per-zone exponential smoothing, in one place.
// Own header, integer maths only, so tools/test_smoothing.c can build it on a
// PC and prove it matches the code it replaced.
//
// Until the Stage D merge step this lived as two copies: inline in
// sample_thread.c's pass 3, and pasted into the shell feed's main.c (now shell-feed/source/shell_frame.c). Both now
// call these two functions. The state array (previous frame's output per zone)
// and the "valid" flag stay in each caller: this header owns the arithmetic
// only, so each process keeps its own, private smoothing state.
//
// Both functions are static inline on purpose: no new .c file, so neither
// Makefile's source list changes, and the compiler can inline them back into
// the same loop the maths used to sit in.
```

### plugin/include/smoothing.h:20

In the code now:

```
// Blend factor for one frame, 0..256, from the sample interval and the ini's settling_time_ms. 256...
// More: docs/code-comments.md (plugin/include/smoothing.h:20)
```

Original (lines 20-23):

```
// Blend factor for one frame, 0..256, from the sample interval and the ini's
// settling_time_ms. 256 means "snap to the new value". This is a discrete
// integer approximation of RC-style exponential smoothing, NOT a calibrated
// time constant: read settling_time_ms = 200 as "roughly", not as a guarantee.
```

### plugin/include/smoothing.h:32

In the code now:

```
// Move one zone's smoothed colour toward `verified` (the colour-processed, dark-thresholded value).
// More: docs/code-comments.md (plugin/include/smoothing.h:32)
```

Original (lines 32-38):

```
// Move one zone's smoothed colour toward `verified` (the colour-processed,
// dark-thresholded value). `state` is that zone's previous output and is
// updated in place; the caller writes it to the LED buffer.
//
// stateValid == false (first frame, or the frame after a settings reload or a
// letterbox commit) seeds the state straight from `verified`: a hard cut, not
// a fade in from black.
```

### plugin/include/smoothing.h:52  (kept in code as a summary)

In the code now:

```
// Force a minimum step of 1 toward the target: integer division truncates toward zero, so smoothing otherwise
// stalls at a dim residual (about 3) and never reaches black. [docs/code-comments.md]
```

Original (lines 52-62):

```
// v2.7.5: C's / truncates toward zero, not toward -infinity, so once
// |raw-prev| is small enough that |delta| rounds down to 0, prev stops
// moving AT ALL, forever, even though raw != prev. Confirmed by
// simulation with this project's own defaults (update_frequency_hz=30,
// settling_time_ms=100 -> alpha=83): decaying toward 0 from ANY
// starting brightness lands on exactly 3 and sticks there for good --
// a dim residual glow that never reaches black on its own. This is
// what was reported as "a colorful scene's colors are retained dimly"
// after a cut to black. Forcing a minimum step of 1 toward raw
// whenever the proportional step would round to 0 guarantees it
// always converges to the exact target within a few extra frames.
```

## plugin/include/state_file.h

### plugin/include/state_file.h:1

In the code now:

```
// state_file.h -- the text of the cross-process state file (v3.9.2)....
// More: docs/code-comments.md (plugin/include/state_file.h:1)
```

Original (lines 1-15):

```
// state_file.h -- the text of the cross-process state file (v3.9.2).
//
// /data/ps4-ambilight/ps4_ambient_state tells a second, independent process -- a compositor-level feed
// reading the shell's own screens (Home, Settings, the quick menu) -- whether the game title is foregrounded, so the two never fight over the strip. This
// plugin has no code that reads the other side; it only writes its own state,
// unconditionally, the same way it already exposes report.c's counters. Holds no personal data:
// a flag, a counter and a title ID, the same title ID already shown in the report file.
//
// Deliberately NOT like report.c's file: that one is rewritten with O_TRUNC, fine for an
// occasional diagnostic nobody is polling. This file is read by another process several times
// a second, so every record is a FIXED WIDTH and every write lands at offset 0 WITHOUT
// truncating -- a reader can only ever see the old complete record or the new one, never a torn
// write. state.c fills a StateSnapshot from the live globals and does the file I/O; this header
// is string logic only, so tools/test_state.c can build and check it on a PC (same split as
// report_file.h / report.c).
```

### plugin/include/state_file.h:29

In the code now:

```
// "v1 fg=0 seq=000000 title=CUSA00000\n" -- 6+1+5+6+7+9+1 = 35 bytes, constant for every fg (0/1, one...
// More: docs/code-comments.md (plugin/include/state_file.h:29)
```

Original (lines 29-31):

```
// "v1 fg=0 seq=000000 title=CUSA00000\n" -- 6+1+5+6+7+9+1 = 35 bytes, constant for every
// fg (0/1, one digit) and every seq (wrapped mod 1,000,000, always 6 digits). Checked equal
// to this constant by tools/test_state.c, not just hand-counted.
```

### plugin/include/state_file.h:40

In the code now:

```
// Right-pads/truncates to exactly STATE_TITLE_LEN so the record's width never depends on the title...
// More: docs/code-comments.md (plugin/include/state_file.h:40)
```

Original (lines 40-42):

```
// Right-pads/truncates to exactly STATE_TITLE_LEN so the record's width never depends on the
// title ID's own length. Every real PS4 title ID is already this length; this is defense in
// depth for a malformed or empty one, not expected to trigger in practice.
```

### plugin/include/state_file.h:53

In the code now:

```
// Returns STATE_RECORD_LEN on success, 0 if something made the formatted length come out different...
// More: docs/code-comments.md (plugin/include/state_file.h:53)
```

Original (lines 53-55):

```
// Returns STATE_RECORD_LEN on success, 0 if something made the formatted length come out
// different (should not happen; a caller treats 0 as "don't write this turn, retry next tick").
// `cap` must be at least STATE_RECORD_LEN + 1.
```

## plugin/source/buffer_guard.c

### plugin/source/buffer_guard.c:1  (kept in code as a summary)

In the code now:

```
// Never read a display buffer without asking the kernel it is mapped and CPU-readable: Mortal Kombat 11's
// GPU-only buffers (prot 0x30) faulted the sampler. See docs/debugging/gpu-only-buffers.md. [docs/code-comments.md]
```

Original (lines 1-29):

```
// buffer_guard.c -- part of ps4_ambient_light (v3.9).
//
// Why this exists: Mortal Kombat 11 (CUSA11395) crashed the game with a SIGSEGV in
// ambient_sample_thread ("page fault, user read data, page not present", fault address
// 0x49ff38b6d0, plugin offset +0x719c) on the first real sampling pass. The sampler trusted
// g_bufferAddrs[] blindly: sampleZoneAverage() bounds-checked the OFFSET against
// BASE_PADDED_BUFFER_BYTES but never asked whether the ADDRESS was mapped or CPU-readable.
//
// What a capture then showed (docs/debugging/gpu-only-buffers.md has all of it): the address
// was valid and inside the buffer. MK11 maps each display buffer as one direct-memory region
// with protection 0x30 (GPU read/write, no CPU bits) and memory type 3 (write-combined), so
// a CPU load faults. The registered format (0x88740000) was recognized and decoded fine; format
// recognition says nothing about whether the CPU may read the memory.
//
// Three layers, each one a fix for something the layer before it did not cover:
//
//   1. ambient_readable_bytes()/ambient_buffer_readable(): ask the kernel
//      (sceKernelVirtualQuery) how many bytes from the buffer base the CPU may read, keep
//      that in g_readableLimit, and refuse to sample when it is 0. Worst case is the existing
//      keepalive path (liveBufferAddr = 0): the strip holds its color instead of the game
//      dying. Every pixel read elsewhere is bounds-checked against g_readableLimit.
//   2. ambient_remap_cpu_view(): for a buffer layer 1 rejected as "mapped but not
//      CPU-readable" and that is direct memory, make it CPU-readable (see the block below).
//   3. ambient_resolve_readable(): the one entry point the sampler and the debug probe call.
//
// sceKernelVirtualQuery and friends are resolved by name in main.c (same pattern as
// sceSystemServiceGetStatus). If sceKernelVirtualQuery cannot be resolved the guard fails
// OPEN (the behavior before this file existed) so other firmware/titles are not regressed; g_guardAvailable says
// which happened.
```

### plugin/source/buffer_guard.c:37

In the code now:

```
// Layout of OrbisKernelVirtualQueryInfo (OpenOrbis kernel.h).
// More: docs/code-comments.md (plugin/source/buffer_guard.c:37)
```

Original (lines 37-39):

```
// Layout of OrbisKernelVirtualQueryInfo (OpenOrbis kernel.h). Declared locally
// and padded generously so a slightly different real struct size cannot make
// the kernel write past our buffer.
```

### plugin/source/buffer_guard.c:67

In the code now:

```
// Walks contiguous mapped regions from addr, up to len bytes, and returns how many bytes are...
// More: docs/code-comments.md (plugin/source/buffer_guard.c:67)
```

Original (lines 67-70):

```
// Walks contiguous mapped regions from addr, up to len bytes, and returns how many bytes are
// CPU-readable from addr (0 = the very first page is not). Reason codes in g_guardLastRet:
// 0 ok, <0 syscall error, 1 unmapped gap, 2 not CPU-readable. Fails open (returns len) when
// sceKernelVirtualQuery is unavailable.
```

### plugin/source/buffer_guard.c:96

In the code now:

```
// Sampler-facing wrapper. Sets g_readableLimit for the pixel-read bounds checks and returns false only...
// More: docs/code-comments.md (plugin/source/buffer_guard.c:96)
```

Original (lines 96-99):

```
// Sampler-facing wrapper. Sets g_readableLimit for the pixel-read bounds checks and returns
// false only when NOTHING is readable (first page unmapped or GPU-only, e.g. MK11 with
// protection 0x30). A buffer that is readable but shorter than the padded ceiling is still
// used; its unreadable tail is skipped pixel by pixel.
```

### plugin/source/buffer_guard.c:108  (kept in code as a summary)

In the code now:

```
// GPU-only buffers are made CPU-readable by three methods in order (gpu_only_remap, default 3). Keep the second view's
// memory type equal to the original's: a write-back view of write-combined memory goes stale. [docs/code-comments.md]
```

Original (lines 108-144):

```
// ---------------------------------------------------------------------------------------------
// Making a GPU-only display buffer CPU-readable (Mortal Kombat 11 and any title like it).
//
// Three methods, tried in this order, selected by the optional ini key [compat] gpu_only_remap
// (0 off, 1 = method 1 only, 2 = methods 1+2, 3 = all three, the default):
//
//   1. A second, CPU_READ-only view of the same direct-memory offset via sceKernelMapDirectMemory2,
//      created with the SAME memory type the original mapping reports. The type matters: a
//      write-back (type 0) view of write-combined memory would let the CPU cache lines that the
//      GPU, which does not snoop the CPU cache, later overwrites, and the view would freeze on
//      old pixels.
//   2. The same with the plain sceKernelMapDirectMemory (kernel picks the type). That has the
//      stale-pixel risk above, so it is only reached with gpu_only_remap=2 or more.
//   3. sceKernelMprotect on the GAME'S OWN mapping, adding CPU_READ to the bits it already has
//      (0x30 -> 0x31). Memory type is untouched, so it stays write-combined: reads are uncached
//      (about 2.5 ms per sampling pass on MK11) and always see what the GPU wrote.
//
// MEASURED ON HARDWARE (MK11, CUSA11395): methods 1 and 2 are refused, method 3 works.
//   - Method 1 was captured: sceKernelMapDirectMemory2 answered 0x80020010, SCE errno 16 (EBUSY),
//     435 times in a run with gpu_only_remap=1. The range is already mapped, so a second mapping
//     of it is not allowed.
//   - Method 2 was refused too, but that is inferred, not captured: with the default 3 the packet
//     ended on method 3, and method 3 only runs after both mapping methods failed. Its own return
//     code was overwritten by method 3's and is not in any capture.
//   - Method 3 was accepted (return 0, protection verified afterwards) and is what makes MK11
//     work: created 3, failed 0 across 53 s of gameplay.
// Methods 1 and 2 are expected to fail for any buffer the game maps itself. They stay because
// they were part of the tested build and removing them is a second change to verify; they are
// the first thing to delete if nothing ever needs them. See docs/debugging/gpu-only-buffers.md.
//
// This only ever runs on a buffer layer 1 already rejected, so a title whose buffers are
// CPU-readable never reaches it. Every failure leaves the strip holding its last color with the
// game untouched, and is reported in the RMAP debug packet. A region the kernel refused is
// remembered and not retried until its start, size or direct-memory offset changes: a refusal is
// deterministic and each refused call cost about 6 ms of the sampler thread (435 refusals in a
// capture of about 23 s before this was added, pass time 6.2 ms instead of tens of microseconds).
// ---------------------------------------------------------------------------------------------
```

### plugin/source/buffer_guard.c:172

In the code now:

```
// Regions every enabled method already failed on, so they are not retried (see the block above for the...
// More: docs/code-comments.md (plugin/source/buffer_guard.c:172)
```

Original (lines 172-174):

```
// Regions every enabled method already failed on, so they are not retried (see the block above
// for the numbers). Keyed on start, size and direct-memory offset: a re-created buffer at the
// same address has a different offset or size and gets a fresh attempt.
```

### plugin/source/buffer_guard.c:199

In the code now:

```
// Drops aliases whose original mapping is gone, moved, or became CPU-readable on its own.
// More: docs/code-comments.md (plugin/source/buffer_guard.c:199)
```

Original (lines 199-201):

```
// Drops aliases whose original mapping is gone, moved, or became CPU-readable on its own.
// Called every ~30 lookups (about once a second at the sampler rate). Only aliases (methods 1 and 2)
// need this; a method 3 mprotect leaves nothing to clean up.
```

### plugin/source/buffer_guard.c:267

In the code now:

```
// Second mapping refused (EBUSY on MK11), or not possible.
// More: docs/code-comments.md (plugin/source/buffer_guard.c:267)
```

Original (lines 267-269):

```
// Second mapping refused (EBUSY on MK11), or not possible. Method 3 (gpu_only_remap=3, the
// default): add CPU_READ to the game's OWN mapping, keeping its GPU bits and memory type. The
// kernel may still refuse; the result is verified with a fresh query before it is trusted.
```

### plugin/source/buffer_guard.c:293

In the code now:

```
// Single entry point for the sampler and the debug probe: returns an address the CPU may read...
// More: docs/code-comments.md (plugin/source/buffer_guard.c:293)
```

Original (lines 293-295):

```
// Single entry point for the sampler and the debug probe: returns an address the CPU may read
// (bufferAddr itself, or an alias of it) with g_readableLimit set for it, or 0 when the buffer
// cannot be read at all.
```

## plugin/source/color_processing.c

### plugin/source/color_processing.c:1

In the code now:

```
// color_processing.c -- part of ps4_ambient_light, split out of the original single-file main.c....
// More: docs/code-comments.md (plugin/source/color_processing.c:1)
```

Original (lines 1-4):

```
// color_processing.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. gamma->brightness->contrast->saturation->levels->order pipeline, plus dark-threshold hysteresis.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
```

### plugin/source/color_processing.c:21

In the code now:

```
// v2.2 color processing : gamma -> brightness -> contrast -> saturation -> levels -> color order.
// More: docs/code-comments.md (plugin/source/color_processing.c:21)
```

Original (lines 21-42):

```
// ============================================================
// v2.2 color processing : gamma -> brightness ->
// contrast -> saturation -> levels -> color order. Most of this is
// still applied once per zone AFTER averaging, for the same
// cheapness argument as before -- EXCEPT the new per-channel
// gamma_r/g/b, which is applied per-sample BEFORE averaging (see
// sampleZoneAverage), because gamma is non-linear and this project's
// own comment claiming "averaging first then correcting matches the
// Android inspiration project" was simply wrong -- the real Android
// source (ColorProcessor.kt, called from e.g. ScreenEncoder before
// any zone/border cropping) corrects every pixel first, then
// averages. That distinction is invisible for linear ops (brightness,
// levels) but real for gamma, so only gamma moved.
//
// No libm anywhere in this path: the legacy global gamma is a LUT
// lookup (kGammaLuts, generated offline), the new per-channel gammas
// are LUTs too (g_gammaLutR/G/B, built from a libm-free fast-pow
// approximation at config load -- see ambient_rebuild_perchannel_
// gamma_luts above), saturation/contrast are pure integer math
// (verified in Python before porting), brightness is a plain integer
// scale.
// ============================================================
```

### plugin/source/color_processing.c:44

In the code now:

```
// v2.2: brightness/contrast/saturation below all take and return UNCLAMPED int32_t, not uint8_t, and...
// More: docs/code-comments.md (plugin/source/color_processing.c:44)
```

Original (lines 44-60):

```
// ------------------------------------------------------------
// v2.2: brightness/contrast/saturation below all take and return
// UNCLAMPED int32_t, not uint8_t, and NONE of them clamp to 0-255
// internally. This matters, and isn't just a style choice: Android's
// ColorProcessor.kt keeps every intermediate value as an unclamped
// Float and only clamps+rounds once, at the very end of the whole
// pipeline. Clamping to a byte between every stage (the v2.0/v2.1
// behavior) throws away sign/overflow information that a LATER,
// luma-relative stage (saturation) actually needs -- e.g. a channel
// that contrast pushed to -37 is meaningfully different input to
// saturation's luma math than a channel clamped to 0 first. Verified
// in Python before porting: on ordinary test inputs this produced
// final answers up to ~13/255 away from Android's real result, not
// mere rounding noise. The only clamp+round in this
// whole chain is in applyColorProcessing, right before the wire-order
// write.
// ------------------------------------------------------------
```

### plugin/source/color_processing.c:67

In the code now:

```
// v2.2: per-channel brightness multiplier, ported from Android's (brightness/100) *...
// More: docs/code-comments.md (plugin/source/color_processing.c:67)
```

Original (lines 67-71):

```
// v2.2: per-channel brightness multiplier, ported from Android's
// (brightness/100) * (brightnessChannel/100) combined factor. Kept as
// its own integer scale (0-500, 100=neutral) separate from the 0-255
// global `applyBrightness` above rather than trying to unify the two
// scales -- see the AmbientConfig struct comment for why.
```

### plugin/source/color_processing.c:78

In the code now:

```
// v2.2: contrast, ported from Android's `128 + (c-128)*cf` with cf = 1 + contrast/100.
// More: docs/code-comments.md (plugin/source/color_processing.c:78)
```

Original (lines 78-81):

```
// v2.2: contrast, ported from Android's `128 + (c-128)*cf` with
// cf = 1 + contrast/100. Uses this file's usual "0 = unchanged"
// convention (same as saturation), NOT Android's raw "100 = unchanged"
// percentage -- see the AmbientConfig struct comment.
```

### plugin/source/color_processing.c:91

In the code now:

```
// v2.2: input/output are unclamped int32_t now (see the big comment above applyBrightness) -- a...
// More: docs/code-comments.md (plugin/source/color_processing.c:91)
```

Original (lines 91-95):

```
// v2.2: input/output are unclamped int32_t now (see the big comment
// above applyBrightness) -- a channel that arrived negative (from
// contrast) or >255 (from a >100% per-channel brightness) still needs
// to pull the luma calculation in that same, un-clamped direction, or
// this stage disagrees with what Android's real float math does.
```

### plugin/source/color_processing.c:100

In the code now:

```
// Integer luma using the real BT.601 weights (Android uses 0.299/0.587/0.114 as floats; /1000 here...
// More: docs/code-comments.md (plugin/source/color_processing.c:100)
```

Original (lines 100-104):

```
// Integer luma using the real BT.601 weights (Android uses
// 0.299/0.587/0.114 as floats; /1000 here instead of the old
// >>8-and-round-to-256ths approximation, now that this whole path
// carries enough precision to make the more exact weights worth
// using.
```

### plugin/source/color_processing.c:111

In the code now:

```
// Writes the 3 output bytes in the configured wire order (most WS2812B/NeoPixel strips are GRB, not...
// More: docs/code-comments.md (plugin/source/color_processing.c:111)
```

Original (lines 111-113):

```
// Writes the 3 output bytes in the configured wire order (most
// WS2812B/NeoPixel strips are GRB, not RGB
// Android inspiration project's own color_order setting).
```

### plugin/source/color_processing.c:126

In the code now:

```
// v2.1/v2.2: levels adjustment (black_level/white_level) -- a "stretch" of the 0-255 range, verified...
// More: docs/code-comments.md (plugin/source/color_processing.c:126)
```

Original (lines 126-138):

```
// v2.1/v2.2: levels adjustment (black_level/white_level) -- a
// "stretch" of the 0-255 range, verified in Python before porting.
// Applied last in the color pipeline, matching the
// real Android ColorProcessor order (gamma -> brightness -> contrast
// -> saturation -> levels).
//
// This is now ALSO the single final clamp-to-byte for the entire
// v2.2 pipeline (see the big comment above applyBrightness) -- input
// here may be negative or >255 coming out of contrast/saturation, so
// even the "no-op" (blackLevel=0, whiteLevel=100) fast path below
// must still clamp, not just pass through, or an out-of-range
// unclamped value would wrap/truncate incorrectly when cast to
// uint8_t by the caller.
```

### plugin/source/color_processing.c:160

In the code now:

```
// Full per-zone pipeline, v2.2 order -- now matches the Android source exactly: gamma -> brightness ->...
// More: docs/code-comments.md (plugin/source/color_processing.c:160)
```

Original (lines 160-182):

```
// Full per-zone pipeline, v2.2 order -- now matches the Android source
// exactly: gamma -> brightness -> contrast -> saturation -> levels
// (black/white point) -> wire-order bytes. (v2.0/v2.1 had saturation
// BEFORE brightness, contradicting this file's own comment about
// matching Android. Harmless while brightness was
// only ever a single uniform scalar, since a uniform scale commutes
// with saturation's luma-relative math either way, but it stops being
// harmless the moment per-channel brightness enters the picture below,
// so it's fixed here rather than left as a latent trap.)
//
// Note the input (r,g,b) here has ALREADY had the new per-channel
// gamma_r/gamma_g/gamma_b applied, per-sample, before zone-averaging
// (see sampleZoneAverage) -- that part deliberately happens BEFORE
// this function, not in it, because gamma is non-linear and Android
// applies it before any downsampling too. The legacy fixed-LUT
// `gamma=` global below is a SEPARATE, ADDITIONAL curve applied here,
// zone-wide, after averaging -- kept for backward compatibility with
// existing configs. Both default to a no-op, so an unmodified ini
// still behaves exactly like before.
//
// dark_threshold is deliberately NOT applied here -- it needs
// per-zone hysteresis STATE across calls, which lives with the
// smoothing state in the thread loop instead.
```

### plugin/source/color_processing.c:185

In the code now:

```
// --- gamma (legacy global fixed-LUT stage; per-channel gamma_r/g/b already happened per-sample before...
// More: docs/code-comments.md (plugin/source/color_processing.c:185)
```

Original (lines 185-189):

```
// --- gamma (legacy global fixed-LUT stage; per-channel gamma_r/g/b
// already happened per-sample before this call). This is the one
// remaining byte-rounding step before the pipeline goes fully
// unclamped int32_t below -- see the struct comment on why the
// legacy uint8_t LUT itself wasn't also upgraded to float. ---
```

### plugin/source/color_processing.c:212

In the code now:

```
// --- levels (black/white point) -- the ONE clamp+round back to a byte for this whole unclamped chain...
// More: docs/code-comments.md (plugin/source/color_processing.c:212)
```

Original (lines 212-214):

```
// --- levels (black/white point) -- the ONE clamp+round back to a
// byte for this whole unclamped chain, matching Android's own
// single final clamp+round. ---
```

### plugin/source/color_processing.c:221

In the code now:

```
// v2.1 dark_threshold state: per-zone hysteresis flag (see applyDarkThreshold below) -- separate from...
// More: docs/code-comments.md (plugin/source/color_processing.c:221)
```

Original (lines 221-223):

```
// v2.1 dark_threshold state: per-zone hysteresis flag (see
// applyDarkThreshold below) -- separate from g_smoothedRgb since it's
// a decision (am I "dark" right now), not a color value.
```

### plugin/source/color_processing.c:226

In the code now:

```
// Forces (r,g,b) to black if the zone should currently be considered "dark", with hysteresis to avoid...
// More: docs/code-comments.md (plugin/source/color_processing.c:226)
```

Original (lines 226-232):

```
// Forces (r,g,b) to black if the zone should currently be considered
// "dark", with hysteresis to avoid flicker on scenes hovering right at
// the threshold: must drop BELOW darkThreshold to go dark, but must
// rise darkThreshold+10 to come back -- verified in Python against a
// deliberately noisy sequence straddling the threshold before porting.
// Applied to the pre-smoothing target, so smoothing (if
// enabled) naturally fades in/out of black instead of snapping.
```

## plugin/source/config.c

### plugin/source/config.c:103  (kept in code as a summary)

In the code now:

```
// Raw sceKernel* file calls only: libc stdio (fopen, FILE*) is not safe off the plugin-load thread and crashed.
// Keep every read and write here on raw syscalls. [docs/code-comments.md]
```

Original (lines 103-111):

```
// v2.2: reads via sceKernelOpen/sceKernelRead/sceKernelClose into a
// memory buffer instead of fopen()/fgetc(). This function is reachable
// from ambient_check_config_reload() on ambient_sample_thread (a worker
// thread spawned separately from plugin load), and libc stdio (FILE*,
// buffering, locks) isn't safe to touch off the main plugin-load thread
// on this toolchain -- that's what was crashing. Same raw-syscall
// approach game_patch/utils.cpp already uses successfully elsewhere in
// this repo. The parsing state machine below is unchanged; only the
// byte source changed, from fgetc(f) to indexing into filebuf.
```

### plugin/source/config.c:124

In the code now:

```
// malloc(0) is legal-but-implementation-defined (may return NULL even on success), so a genuinely...
// More: docs/code-comments.md (plugin/source/config.c:124)
```

Original (lines 124-126):

```
// malloc(0) is legal-but-implementation-defined (may return NULL even
// on success), so a genuinely empty file gets a 1-byte buffer that
// nothing ever reads into rather than being misread as a malloc failure.
```

## plugin/source/gamma.c

### plugin/source/gamma.c:1

In the code now:

```
// gamma.c -- part of ps4_ambient_light, split out of the original single-file main.c. Fixed 8-preset...
// More: docs/code-comments.md (plugin/source/gamma.c:1)
```

Original (lines 1-4):

```
// gamma.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. Fixed 8-preset gamma LUTs + the continuous per-channel gamma builder.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
```

### plugin/source/gamma.c:21

In the code now:

```
// Gamma correction LUTs -- generated OFFLINE with real pow() (this exact comment mirrors the sRGB/PQ...
// More: docs/code-comments.md (plugin/source/gamma.c:21)
```

Original (lines 21-24):

```
// Gamma correction LUTs -- generated OFFLINE with real pow() (this exact
// comment mirrors the sRGB/PQ LUT precedent above: table lookups only at
// runtime, no libm linked in this build, see Makefile LIBS).
// Index 0 = gamma 1.0 (off/passthrough) ... index 7 = gamma 2.8.
```

### plugin/source/gamma.c:173

In the code now:

```
// v2.2: per-channel gamma at an arbitrary percentage (Android's own convention -- out =...
// More: docs/code-comments.md (plugin/source/gamma.c:173)
```

Original (lines 173-192):

```
// ============================================================
// v2.2: per-channel gamma at an arbitrary percentage (Android's own
// convention -- out = 255*(in/255)^(100/pct), pct=100 == passthrough).
// The fixed kGammaLuts above only cover 8 preset exponents because
// they're baked in at compile time with a real pow() run OFFLINE, on
// a dev machine -- this build links no libm (see Makefile LIBS), so
// there's no pow()/log()/exp() available at runtime to support an
// arbitrary user-chosen percentage the same way.
//
// Rather than either (a) restricting the new per-channel controls to
// the same 8 presets, which defeats the point of porting a
// continuous per-channel control, or (b) linking libm just for this,
// this uses the well-known Ankerl fast log2/exp2 approximation --
// pure integer/float bit manipulation, no transcendental libm calls.
// It's only ever run when the config (re)loads, NOT per-pixel/frame,
// so its handful of extra ULPs of error versus real pow() (verified
// in Python before porting: max off-by-one across a full 0-255 LUT,
// and EXACT agreement across the entire dark end 0-40 that actually
// matters for the black-crush issue
// at runtime and are invisible in the final 8-bit LUT anyway.
```

### plugin/source/gamma.c:214

In the code now:

```
// x assumed in [0,1] here (normalized 0-255 sample); 0 is handled as a special case since log2(0) is...
// More: docs/code-comments.md (plugin/source/gamma.c:214)
```

Original (lines 214-216):

```
// x assumed in [0,1] here (normalized 0-255 sample); 0 is handled as
// a special case since log2(0) is undefined and every real gamma
// curve maps black to black anyway.
```

### plugin/source/gamma.c:223

In the code now:

```
// Per-channel gamma LUTs for the v2.2 gamma_r/gamma_g/gamma_b knobs.
// More: docs/code-comments.md (plugin/source/gamma.c:223)
```

Original (lines 223-237):

```
// Per-channel gamma LUTs for the v2.2 gamma_r/gamma_g/gamma_b knobs.
// Rebuilt on config load/reload (ambient_rebuild_perchannel_gamma_luts
// below), NOT per pixel -- same pattern as everything else in this
// file that could otherwise involve float math on a hot path.
//
// Stored as FLOAT, not uint8_t, deliberately: Android's own gamma LUT
// (buildGammaLut in ColorProcessor.kt) is a FloatArray too, and the
// whole rest of its pipeline stays in Float with no intermediate
// clamp/round until the single final clamp+round back to a byte. An
// 8-bit LUT here would force an early, lossy round-to-byte right
// after gamma, before brightness/contrast/saturation/levels even run
// -- verified in Python before porting that this isn't just
// theoretical: chaining early uint8_t clamps between stages produced
// answers up to ~13/255 off from the real float pipeline on ordinary
// inputs, not merely off-by-one rounding noise.
```

## plugin/source/hooks.c

### plugin/source/hooks.c:1

In the code now:

```
// hooks.c -- part of ps4_ambient_light, split out of the original single-file main.c. Plugin metadata...
// More: docs/code-comments.md (plugin/source/hooks.c:1)
```

Original (lines 1-4):

```
// hooks.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. Plugin metadata + the two real GoldHEN hooks (buffer registration, flip).
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
```

### plugin/source/hooks.c:26  (kept in code as a summary)

In the code now:

```
// g_pluginVersion and the v2.x-v3.x release notes that lived here are in CHANGELOG.md and docs/code-comments.md.
// More: docs/code-comments.md (plugin/source/hooks.c:26)
```

Original (lines 26-209):

```
// The sampler asks the kernel whether it may read a display buffer before it
// touches it, and makes a GPU-only one (Mortal Kombat 11) CPU-readable instead
// of crashing the game -- see buffer_guard.c and
// docs/debugging/gpu-only-buffers.md.
// v3.7 -> v3.8:
// Game and Movie presets. The ini holds two complete presets and the plugin
// reads the one [presets] active= names, or Movie for a title in
// media_titles.h. An ini from before v3.8 reads as it always did -- see
// preset_select.h and settings.c.
// v3.6 -> v3.7:
// AMBIENT_SAMPLE_LAG defaults to 2 (two flips back), which needs a second
// tracked slot, g_prevPrevDisplayBufferIndex below. The sampler no longer
// reads the hook-reported slot while that history fills in, and re-reads
// zones that jump to catch a buffer that changes mid-read -- see
// sample_thread.c and docs/debugging/sample-lag-and-boot-flash.md.
// v3.5 -> v3.6:
// Format 0x80002200 (HITMAN 3, RDR2) is decided per frame from the alpha byte
// of 8 zone words, before the older smoothness check gets a say -- see
// detectHdr2200Fast in zones.c, hdr2200_vote.h and
// docs/debugging/hdr2200-alpha-detection.md.
// v3.4 -> v3.5:
// Format 0x88740000 is decoded as 8-bit A8R8G8B8 on the frames where the
// buffer holds that (YouTube playing SDR video with HDR on), decided every
// pass from the alpha byte -- see detectPq8bitMisregistration in zones.c and
// docs/debugging/youtube-hdr-8bit.md.
// v3.3 -> v3.4:
// Letterbox zones placed at the measured bar depth instead of the
// quantized (rounded-down) one, and only when the axis's two edges
// agree -- see docs/debugging/letterbox-placement.md.
// v3.2 -> v3.3:
// Sampler reads the previous flip's buffer instead of the one the flip
// hook just reported (g_prevDisplayBufferIndex, record_flip_index below),
// fixing flicker from sampling a cleared / half-drawn frame. See
// CHANGELOG.md v3.3 for the capture evidence.
// v3.1 -> v3.2:
// The v3.1 flip hooks above fire in EVERY process GoldHEN injects this
// plugin into -- including this project's own companion app, which is
// just another titleid (SHMY00091) as far as GoldHEN is concerned. Two
// consequences, both real: the companion app's own screen flips got
// captured and streamed to the strip as if it were a game, conflicting
// with whatever real testing/adjustment was already running; and worse,
// ambient_sample_thread then applied this file's game-buffer tiling math
// to the companion app's differently-shaped surface, producing a wild
// address and a real SIGSEGV (crash report: thread
// "ambient_sample_thread", proc "eboot.bin", AppName "PS4 Ambilight",
// TitleID "SHMY00091", page fault on a read from an address matching
// neither this title's real GNM buffer addresses seen in other
// captures). Fixed at the top of plugin_load (main.c), before any
// dlsym/hook/thread work: read procInfo.titleid via sys_sdk_proc_info()
// and bail out entirely if it's SHMY00091. One check fixes both the
// crash and the color conflict, since neither can happen if no hook is
// ever installed in that process to begin with.
//
// v3.0 -> v3.1:
// A title (Shadow of the Tomb Raider, both its SDR and HDR display
// modes -- confirmed the same title, not two separate ones, after an
// earlier capture session got that wrong) never lit up the strip at
// all. Buffer registration looked completely normal --
// sceVideoOutRegisterBuffersPtr_hook fired once, format came back
// recognized both times (0x80000000 A8R8G8B8_SRGB on the SDR run,
// 0x88740000 A2R10G10B10_BT2020_PQ on the HDR run) -- so this wasn't
// another entry for the pixel-format table.
//
// Added real telemetry instead of guessing further:
// g_registerHookCallCount / g_flipHookCallCount (below), sent once a
// second via a new send_flip_diag_packet (network.c), __FINAL__==0-
// gated same as every other diagnostic packet in this file. Captured
// against the failing title: register climbing normally, flip stuck
// at 0 for the entire session, g_currentDisplayBufferIndex never
// leaving its 0xFFFFFFFF sentinel. sceGnmSubmitAndFlipCommandBuffers
// was simply never being called by this title -- not a timing/
// throttle artifact, an unconditional per-second counter that never
// once incremented across an ~11-second capture.
//
// First hypothesis: a second, real flip entrypoint,
// sceVideoOutSubmitFlip (libSceVideoOut) -- confirmed to exist with a
// real, separate NID via shadPS4's independently reverse-engineered
// video_out.cpp (fetched and read directly, not recited from memory,
// same standard of evidence already used for
// sceSystemServiceGetStatus above). Added as a second hook,
// resolved/hooked non-fatally (a title with no use for it must not
// lose the rest of this plugin). Recaptured: resolved fine
// (submit_flip_ptr_resolved=1 in the extended diagnostic packet,
// disambiguating "never resolved" from "resolved but never called"),
// call count still stuck at 0. Wrong hypothesis, but kept hooked --
// harmless, and some other title may yet use it.
//
// Real cause, again found in shadPS4 source rather than guessed:
// gnmdriver.cpp shows sceGnmSubmitAndFlipCommandBuffers (the hook
// this file already had) is itself just a thin wrapper --
//   return sceGnmSubmitAndFlipCommandBuffersForWorkload(count, count, ...);
// -- around a SEPARATELY exported symbol with its own distinct NID. A
// title submitting explicit multi-workload GPU work can call the
// ForWorkload entrypoint directly, skipping the wrapper this plugin
// hooked, entirely. Added as a third hook, same non-fatal pattern.
//
// CONFIRMED on real hardware: gnm_for_workload_hook_call_count
// climbing (0 -> 1 -> 30 -> 95...) over a live capture,
// g_currentDisplayBufferIndex off its sentinel and alternating
// between the title's two swap-chain slots, g_bufferAddrs resolving
// to real addresses, and the existing __FINAL__==0 raw-pixel
// diagnostic firing at its normal ~10x/sec cadence. Strip lit up.
//
// While in here: g_pluginVersion had been sitting at the v2.7.2
// comment above since that release, unbumped through v2.7.3-v3.0.
// Catching it up now that there's a real reason to touch this file
// again -- not attempting to backfill the missing intermediate
// comment entries, see CHANGELOG.md for those.
//
// v2.7.1 -> v2.7.2:
// CONFIRMED, no longer diagnostic-only. The v2.7.1 dev-telemetry
// (still present, still __FINAL__==0-gated -- see detectHdr2200Format's
// own comment) showed the live detection working exactly as the
// offline replica predicted: on the session that originally looked
// broken, sdrTv/hdrTv/streak/isHdr streamed back showing a clean
// accumulate-then-flip at the correct moment (streak 1->2->3, then
// isHdr 0->1 with streak reset), then stayed locked on HDR with no
// reversal. The earlier "doesn't settle" report turned out to be a
// patience issue, not a bug: the flip took ~43s from boot (a black
// loading screen produces no valid checks at all, then several more
// seconds accumulating a real streak) -- watching for less than that
// looks identical to "broken."
//
// Also tested the specific worry raised after that: would a dark
// gameplay scene flip it back to SDR incorrectly? Captured a real
// ~6.5-minute gameplay session (not menus) and decoded all 53
// diagnostic packets -- exactly ONE flip in the whole session, the
// correct initial SDR->HDR one. The HDR/SDR margin narrowed
// substantially in darker/busier scenes (as low as ~2.3x apart,
// versus 15-40x on simple menu content) but never came close to
// reversing, let alone sustaining 4 consecutive reversed checks.
// Not proof for every possible scene, but a real, varied session
// with no failure is real evidence, not just theory.
//
// v2.7 -> v2.7.1 (mechanism unchanged, kept for the full trail): added
// live HDR-vs-SDR auto-detection for format 0x80002200 specifically --
// this format ID is confirmed AMBIGUOUS on this title (see
// unpackA8B8G8R8_to_rgb888's own comment): it means A8B8G8R8_SRGB with
// HDR off, but the SAME registered format ID actually holds
// A2R10G10B10_BT2020_PQ data when HDR is on, with no reliable signal
// available from any hooked API to tell which is currently true --
// hooking sceVideoOutAddBufferHdrPrivilege (the obvious direct signal)
// was investigated and ruled out, since no real reference
// implementation of it exists anywhere (not in this codebase's history,
// not in fpPS4) to safely derive its call signature from, and a wrong
// guess there risks corrupting a call into Sony's own code. Detected
// instead via decode-smoothness: real image content decodes smoothly
// under the correct hypothesis and noisily under the wrong one. Method
// validated offline first (a Python replica in
// decode_verification_dump.py, run blind against real captures, twice)
// before being ported here. Kept deliberately cheap, per explicit
// instruction not to have this plugin compete with the game for CPU:
// reuses zone coordinates already computed for real LED sampling, caps
// samples at HDR2200_DETECT_SAMPLES, throttled to once every
// HDR2200_DETECT_INTERVAL sampling passes, with a hysteresis streak
// (HDR2200_STREAK_THRESHOLD) and a noise floor (HDR2200_NOISE_FLOOR)
// so a single noisy or degenerate frame can't flip the live decision.
//
// v2.6 -> v2.7 (still true): added
// real support for format 0x80002200 (A8B8G8R8_SRGB) -- this format was
// identified by name several sessions ago (against fpPS4's enum table,
// on a different title: HITMAN 3) but was NEVER actually wired into
// getUnpackFnForFormat; a game reporting this format got NULL back,
// same as any other unrecognized format, and this plugin correctly
// sent nothing rather than guess (see g_haveValidFormat). Root-caused
// via detile_verify_probe (a standalone diagnostic plugin, not this
// one) across a long investigation: pad-conflict trigger issues, then
// three addressing hypotheses (tiled math, naive linear math, wrong
// swap-chain slot) that all decoded plausible-but-wrong colors against
// a real screenshot, then a clean flip-rate and stable videoOutHandle
// that ruled out "wrong render pass" -- until the user independently
// tested turning HDR off, which is what actually fixed it. With HDR
// off, the SAME format ID's buffer content decoded correctly with
// simple tiled addressing, once R and B are swapped relative to this
// file's existing unpackA8R8G8B8_to_rgb888. Confirmed two ways: a
// probe capture checked pixel-for-pixel against a real screenshot at 5
// known coordinates (single-digit-to-teens RGB error at all 5 at
// once, the first hypothesis in the whole investigation to do that),
// and then the user confirming colors look correct live, in this
// plugin, on real hardware. See unpackA8B8G8R8_to_rgb888's own comment
// below, and this repo's for the full investigation trail
// (kept as a separate document rather than squeezed into this file's
// existing section-numbered project notes, whose numbering already collides across
// forks.
```

### plugin/source/hooks.c:219

In the code now:

```
// v3.1: second real flip entrypoint.
// More: docs/code-comments.md (plugin/source/hooks.c:219)
```

Original (lines 219-222):

```
// v3.1: second real flip entrypoint. Real signature, confirmed against
// shadPS4's video_out.cpp, not guessed -- see the g_pluginVersion
// comment above for why this exists. Resolved and hooked non-fatally
// (main.c) -- a title with no use for it is unaffected either way.
```

### plugin/source/hooks.c:225

In the code now:

```
// v3.1: third real flip entrypoint, and the one that actually turned out to matter -- see the...
// More: docs/code-comments.md (plugin/source/hooks.c:225)
```

Original (lines 225-229):

```
// v3.1: third real flip entrypoint, and the one that actually turned
// out to matter -- see the g_pluginVersion comment above. Confirmed
// against shadPS4's gnmdriver.cpp: the plain sceGnmSubmitAndFlipCommand-
// Buffers hook above is itself just a wrapper around this, separately
// exported symbol. Same non-fatal resolve/hook pattern.
```

### plugin/source/hooks.c:247

In the code now:

```
// v3.1: diagnostic only, no pipeline effect -- what actually found the "never lights up" bug.
// More: docs/code-comments.md (plugin/source/hooks.c:247)
```

Original (lines 247-252):

```
// v3.1: diagnostic only, no pipeline effect -- what actually found the
// "never lights up" bug. Sent via send_flip_diag_packet (network.c),
// __FINAL__==0-gated same as every other packet in this file; see
// the g_pluginVersion comment above for the investigation these came
// out of. Plain volatile counters, same cost class as
// g_currentDisplayBufferIndex's own single write per hook call.
```

### plugin/source/hooks.c:283

In the code now:

```
// v2.2.1: diagnostic-only telemetry, added to answer a real live question (does this specific game's...
// More: docs/code-comments.md (plugin/source/hooks.c:283)
```

Original (lines 283-304):

```
// v2.2.1: diagnostic-only telemetry, added to answer a real
// live question (does this specific game's HDR mode register
// a format this plugin actually recognizes?) rather than
// guess. Reuses the existing debug_send_raw/DEBUG_IP path
// (same one send_timing_packet uses), a NEW 8-byte payload so
// it dispatches distinctly by length in any listener that
// already keys off len(data) (this project's own convention
// -- see send_timing_packet's comment above). Only fires when
// the format actually CHANGES, not on every re-registration
// (this hook already fires repeatedly during normal play per
// the comment this replaces), so it won't spam a listener
// during a real session -- only right when HDR toggles on/off
// or a title switches formats.
//   [0:4] format     -- raw attribute->format, uint32 LE
//   [4:8] wasRecognized -- 0/1, uint32 LE (matches one of the
//                          getUnpackFnForFormat cases or not)
// v2.2.4: gated behind __FINAL__==0 (make DEBUG=1), same
// idiom frame_logger/force_1080p_display already use in this
// repo. This unconditionally sent a UDP packet to a
// hardcoded DEBUG_IP for every single end user on every real
// release build, which is not something a shipped plugin
// should do.
```

### plugin/source/hooks.c:322

In the code now:

```
// v1.1: the flip hook itself only records which buffer is live now -- no sampling, no network I/O, no...
// More: docs/code-comments.md (plugin/source/hooks.c:322)
```

Original (lines 322-325):

```
// v1.1: the flip hook itself only records which buffer is live now --
// no sampling, no network I/O, no loop. This is the actual fix for
// finding 1. g_currentDisplayBufferIndex is read by the
// worker thread below at its own pace.
```

### plugin/source/hooks.c:328

In the code now:

```
// Slot of the flip BEFORE the current one (sentinel until two different slots have been seen).
// More: docs/code-comments.md (plugin/source/hooks.c:328)
```

Original (lines 328-333):

```
// Slot of the flip BEFORE the current one (sentinel until two different
// slots have been seen). The hooks fire at SUBMIT time, so the slot they
// report is the one the GPU is about to render into: it can be cleared or
// half drawn. The previous flip's slot has finished rendering. Only advanced
// when the index actually changes, so the two flip hooks both firing for one
// real flip (same index twice) don't overwrite it.
```

### plugin/source/hooks.c:336

In the code now:

```
// Slot of the flip before THAT one (two real flips back).
// More: docs/code-comments.md (plugin/source/hooks.c:336)
```

Original (lines 336-342):

```
// Slot of the flip before THAT one (two real flips back). Same sentinel and
// same only-on-a-real-change rule as g_prevDisplayBufferIndex above. Before
// v3.7 AMBIENT_SAMPLE_LAG was only ever tested as ">= 1", so a build set to 2
// compiled to exactly the same behaviour as 1: nothing tracked a second step
// back. Red Dead Redemption (about 58 fps) still flickered at lag 1, and
// three captures taken at "lag 2" were byte-identical to the lag 1 ones.
// See docs/debugging/sample-lag-and-boot-flash.md.
```

### plugin/source/hooks.c:370

In the code now:

```
// v3.1: second flip entrypoint -- see its pointer declaration above for why this is hooked.
// More: docs/code-comments.md (plugin/source/hooks.c:370)
```

Original (lines 370-373):

```
// v3.1: second flip entrypoint -- see its pointer declaration above for
// why this is hooked. Record-only, same discipline as the hook above:
// no sampling or network I/O from inside a hook, just the buffer index
// write the worker thread reads at its own pace.
```

### plugin/source/hooks.c:385

In the code now:

```
// v3.1: third flip entrypoint -- the one that actually turned out to matter, see its pointer...
// More: docs/code-comments.md (plugin/source/hooks.c:385)
```

Original (lines 385-388):

```
// v3.1: third flip entrypoint -- the one that actually turned out to
// matter, see its pointer declaration above. Same record-only
// discipline; extra leading `workload` parameter versus the plain
// variant above is the real, confirmed difference in signature.
```

## plugin/source/letterbox.c

### plugin/source/letterbox.c:1

In the code now:

```
// letterbox.c -- part of ps4_ambient_light.
// More: docs/code-comments.md (plugin/source/letterbox.c:1)
```

Original (lines 1-92):

```
// letterbox.c -- part of ps4_ambient_light. Auto letterbox/pillarbox
// (black bar) detection, ported from the Android "inspiration"
// project's own common/util/BorderProcessor.kt.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
//
// What this ports from BorderProcessor.kt, and why it still applies
// here even though that code works on a decoded RGB byte array and
// this one reads live tiled GPU memory:
//  - Each edge is detected INDEPENDENTLY (not "is there a bar", but
//    "how deep is top's bar / right's bar / ..." separately), so an
//    asymmetric bar -- e.g. a status bar rendered only along the top --
//    is handled correctly instead of being averaged away or ignored.
//  - Each edge is probed at 3 points along the perpendicular axis
//    (25%/50%/75%), not just the center, so a bar with a small non-black
//    HUD element sitting off-center is still detected as a bar rather
//    than mistaken for real picture content at the one point that
//    happens to be dark.
//  - A newly detected border is only committed after
//    autoLetterboxStabilityFrames consecutive re-detections agree,
//    exactly like BorderProcessor's own stabilityDetections gate -- a
//    one-frame flicker (a bright flash, a single corrupted read) can't
//    cause a visible snap in the LED geometry. The very first detection
//    of a session is NOT applied immediately (unlike this plugin's own
//    live config reload, which applies right away) -- it goes through
//    the same stability gate as every later one, matching
//    BorderProcessor's checkNewBorder exactly: startup shouldn't guess
//    from a single frame that could be a loading screen.
//
// What's different from the Android version, and why:
//  - BorderProcessor re-detects on every frame it's handed (it's called
//    from the video encoder's own per-frame callback). This plugin's
//    sample thread runs on its own fixed ~30Hz cadence independent of
//    the game's actual frame delivery, so re-detecting on literally
//    every iteration would mean a full bounded-depth probe pass 30x/sec
//    forever, just to keep re-confirming a border that isn't changing.
//    autoLetterboxCheckIntervalFrames throttles this the same way
//    detectHdr2200Format in zones.c throttles ITS own periodic probe --
//    same idiom, new knob, because this is a different signal.
//  - v3.1: this now probes each candidate row/column with a spread of
//    sample points along the ENTIRE line (isRowBlack/isColBlack below),
//    requiring EDGE_BLACK_PERCENT of them to be black before the scan
//    continues past that line -- this is BorderProcessor.findBorderRgb's
//    majority-vote design (used by every real Android capture path),
//    not findBorderRgba's 3-fixed-point spot check (which, on the
//    Android side, is only ever exercised by unit tests, not live
//    capture -- this plugin had accidentally ported the untested path).
//    The 3-point version stops at the first of its 3 fixed probes
//    (25%/50%/75% of the line) that isn't black, so a dark loading
//    screen with small centered text reads as a giant letterbox: the
//    only non-black pixels are near the middle, so all four edges scan
//    almost all the way to the search limit before "finding" the text
//    and committing a border that crops in around it. Once committed,
//    buildZoneGeometry() moves the edge LED zones in to sit right at
//    that margin (see EDGE_LEFT's x = mLeft, etc. in zones.c) --
//    they're no longer anywhere near the real screen edges. So when
//    real content (e.g. a flash of light) enters from a true edge, nothing
//    reacts until it has physically travelled all the way in to wherever
//    the zones actually got moved to -- a very visible, very real delay,
//    despite the plugin never having gone through a fixed processing lag.
//    Requiring most of a line to be black before treating it as bar
//    means a few dozen pixels of centered text only flip a handful of
//    the sampled points and the scan correctly walks past it to the true
//    edge instead.
//  - v3.1 also bounds the search depth itself (MAX_BAR_DEPTH_V/H) instead
//    of scanning to a full half-screen. On this console, gameplay is
//    always native 16:9 -- the only place a real bar can legitimately
//    appear is a cutscene mastered at a narrower theatrical ratio, and
//    even 2.39:1 scope only asks for ~138px of bar per edge on a 1080p
//    frame (see the constants below for the math). Capping the search
//    well short of height/2 or width/2 does two things: it bounds the
//    worst-case scan cost (see the tiled-offset cost note on
//    probeNonBlack below -- this scan is not cheap per-sample), and it's
//    a second, independent safety net against ever mistaking on-screen
//    content for a border -- even if the majority-vote check somehow
//    still misreads a line, the margin this can commit is capped far
//    short of eating the picture, unlike the old height/2-wide net.
//  - As of v3.0, this behaves exactly like the Android original here:
//    the detected depth IS the margin (see zones.c's effective-margin
//    helpers), full stop. It briefly ADDED to a separate manual
//    capture_margin_{top,right,bottom,left} config in v2.9, when this
//    was introduced alongside those pre-existing fields -- but once
//    this could fully cover what manual margins were for (a fixed
//    number is either wrong during a letterboxed cutscene or wrong for
//    the fullscreen gameplay around it; this isn't), the manual fields
//    were removed rather than carried as unused legacy config. See
//    CHANGELOG for that removal.
//  - No RGBA/stride variant: this plugin only ever has one live pixel
//    access path (getTiledElementByteOffset + a PixelUnpackFn), so
//    there's only one detection function, not BorderProcessor's
//    separate parseBorderRgb/parseBorder split for two different input
//    shapes.
```

### plugin/source/letterbox.c:101

In the code now:

```
// The margin actually applied on each edge -- see zones.c's effMargin{Top,Right,Bottom,Left} helpers...
// More: docs/code-comments.md (plugin/source/letterbox.c:101)
```

Original (lines 101-104):

```
// The margin actually applied on each edge -- see zones.c's
// effMargin{Top,Right,Bottom,Left} helpers, the only readers of these.
// All zero (i.e. sample right up to the true screen edge) until a
// border is actually committed, or whenever the feature is off.
```

### plugin/source/letterbox.c:107

In the code now:

```
// One edge-depth reading. -1 means "every line down to the search limit (MAX_BAR_DEPTH_V/H) still read...
// More: docs/code-comments.md (plugin/source/letterbox.c:107)
```

Original (lines 107-113):

```
// One edge-depth reading. -1 means "every line down to the search limit
// (MAX_BAR_DEPTH_V/H) still read as bar" -- kept distinct from a real 0
// (no bar at all, first row/col was already non-black) so it can be
// treated as "inconclusive on this edge" rather than "the whole search
// range is a border": a black loading/transition screen shouldn't
// collapse the capture area to a sliver just because nothing looked
// like picture content in whatever this one probe pass caught.
```

### plugin/source/letterbox.c:123

In the code now:

```
// Matches BorderRect.isKnown: at least one edge has a real (>0) detected depth.
// More: docs/code-comments.md (plugin/source/letterbox.c:123)
```

Original (lines 123-126):

```
// Matches BorderRect.isKnown: at least one edge has a real (>0) detected
// depth. A rect where every edge is 0 or -1 means "no letterbox found",
// which is a legitimate, common, stable state (most content isn't
// letterboxed) -- not an error.
```

### plugin/source/letterbox.c:148

In the code now:

```
// How many probe points are swept along a candidate line (a full row for top/bottom, a full column for...
// More: docs/code-comments.md (plugin/source/letterbox.c:148)
```

Original (lines 148-158):

```
// How many probe points are swept along a candidate line (a full row
// for top/bottom, a full column for left/right), and what fraction of
// them must read black for that line to still count as "bar" and let
// the scan continue past it. 85% matches BorderProcessor's own
// BLACK_PERCENT. 10 samples is well under Android's 32 -- each sample
// here is a getTiledElementByteOffset() call (real address math), not
// a flat array read, so this is a real per-sample cost, not a free
// knob; 10 across a ~1920px-wide or ~1080px-tall line still needs
// several samples to land on a loading-screen glyph before the line
// stops reading as "mostly black", which is the actual failure case
// this fixes.
```

### plugin/source/letterbox.c:162

In the code now:

```
// How far in from each edge to search before giving up (replaces the previous SCREEN_HEIGHT/2 and...
// More: docs/code-comments.md (plugin/source/letterbox.c:162)
```

Original (lines 162-174):

```
// How far in from each edge to search before giving up (replaces the
// previous SCREEN_HEIGHT/2 and SCREEN_WIDTH/2 limits). Bounded by the
// deepest bar a real theatrical aspect ratio would ever put on a
// 1920x1080 16:9 frame, with headroom:
//   1.85:1 ("flat")  -> content height = 1920/1.85 =~1038px -> ~21px bar per edge
//   2.39:1 ("scope")  -> content height = 1920/2.39 =~ 803px -> ~138px bar per edge
//   4:3 pillarbox     -> content width  = 1080*4/3  =1440px -> ~240px bar per edge
// SCREEN_HEIGHT/6 (180px) clears 2.39:1's ~138px with ~30% headroom;
// SCREEN_WIDTH/6 (320px) clears a 4:3 pillarbox's ~240px with ~33%
// headroom. Gameplay itself is always full 16:9 on this console --
// there's no legitimate case for a deeper bar than a cutscene would
// use, so searching any further was only ever paying for the
// dark-loading-screen worst case, never finding a real bar there.
```

### plugin/source/letterbox.c:178

In the code now:

```
// Reads one live pixel and reports whether it's NOT part of a black bar.
// More: docs/code-comments.md (plugin/source/letterbox.c:178)
```

Original (lines 178-185):

```
// Reads one live pixel and reports whether it's NOT part of a black
// bar. Same bounds check sampleZoneAverage/detectHdr2200Format already
// use for the exact same reason -- a correct offset formula should
// never go out of range for an in-bounds (x,y), but this project has
// already paid once for a bad-memory-access crash from an unguarded
// tiled offset. Out-of-range reads as "black" (the safe default for
// this probe: it just means this one sample doesn't count as evidence
// of real picture content, not that anything is wrong).
```

### plugin/source/letterbox.c:198

In the code now:

```
// Rounds a detected depth down to a band of frameSize/32 pixels, same as BorderProcessor's own quantize().
// More: docs/code-comments.md (plugin/source/letterbox.c:198)
```

Original (lines 198-204):

```
// Rounds a detected depth down to a band of frameSize/32 pixels, same
// as BorderProcessor's own quantize(). Without this, a real capture
// with no letterbox still jitters by a pixel or two between checks
// (sensor/scaler noise right at the true edge), which would make
// rectsEqual() fail forever and the stability gate never reach
// autoLetterboxStabilityFrames. -1 (inconclusive/unknown) and exactly 0
// pass through unchanged -- only a real positive depth gets banded.
```

### plugin/source/letterbox.c:218

In the code now:

```
// Real letterbox/pillarbox bars come from cropping to a wider or taller aspect ratio, so the two bars...
// More: docs/code-comments.md (plugin/source/letterbox.c:218)
```

Original (lines 218-224):

```
// Real letterbox/pillarbox bars come from cropping to a wider or taller
// aspect ratio, so the two bars on an axis are always the same depth --
// unlike a HUD element, a logo, or a loading-screen layout, which can
// independently pass the per-row/column black test near just one edge.
// Reject an axis whose two measured depths don't agree within 2 quantize
// bands, or where only one side found a bar at all. See
// docs/debugging/letterbox-placement.md for the capture this came from.
```

### plugin/source/letterbox.c:232

In the code now:

```
// Sweeps EDGE_NUM_SAMPLES points across the full row y (x = 0..SCREEN_WIDTH-1) and reports whether the...
// More: docs/code-comments.md (plugin/source/letterbox.c:232)
```

Original (lines 232-241):

```
// Sweeps EDGE_NUM_SAMPLES points across the full row y (x = 0..SCREEN_WIDTH-1)
// and reports whether the row still counts as "bar" -- i.e. whether the
// top/bottom scan should keep going past it. Matches
// BorderProcessor.isRowBlack's majority-vote design: a narrow bright
// object (a few dozen pixels of centered loading-screen text in an
// otherwise-black frame) only flips a handful of these samples, so the
// row still reads >=EDGE_BLACK_PERCENT% black and the scan correctly
// walks past it toward the true edge -- unlike a single fixed probe
// point, which anything non-black sitting at exactly that x could fool
// into stopping right there.
```

### plugin/source/letterbox.c:269

In the code now:

```
// One full per-edge probe pass.
// More: docs/code-comments.md (plugin/source/letterbox.c:269)
```

Original (lines 269-275):

```
// One full per-edge probe pass. Scans inward from each edge, up to the
// bounded MAX_BAR_DEPTH_V/H search limit, for the first row/column that
// isRowBlack/isColBlack no longer calls "bar" -- that's this edge's
// border depth. -1 means every line down to the search limit still read
// as bar -- kept distinct from a real 0 so it's "inconclusive on this
// edge", not "the whole search range is a border" (see this file's own
// top comment and rectIsKnown below).
```

### plugin/source/letterbox.c:301

In the code now:

```
// Unquantized depths (first non-bar row/col index; -1 = search limit hit).
// More: docs/code-comments.md (plugin/source/letterbox.c:301)
```

Original (lines 301-304):

```
// Unquantized depths (first non-bar row/col index; -1 = search limit hit).
// The stability gate compares the QUANTIZED rect below, but the margin
// actually applied to the LED zones is built from these -- see the
// commit block in ambient_check_letterbox.
```

### plugin/source/letterbox.c:314

In the code now:

```
// Drops any auto-detected margin and resets all detection state.
// More: docs/code-comments.md (plugin/source/letterbox.c:314)
```

Original (lines 314-317):

```
// Drops any auto-detected margin and resets all detection state. Called
// when the feature is off (so it's a guaranteed no-op) and any time it
// transitions from on to off, so a later re-enable starts clean instead
// of resuming from a stale candidate/committed border from before.
```

### plugin/source/letterbox.c:328

In the code now:

```
// Only rebuild geometry (and break the smoothing continuity below) if this reset actually changes...
// More: docs/code-comments.md (plugin/source/letterbox.c:328)
```

Original (lines 328-332):

```
// Only rebuild geometry (and break the smoothing continuity
// below) if this reset actually changes anything a running
// session already applied -- a no-op disable (never detected
// anything, or already reset) shouldn't force useless work
// every time this function is called while the feature stays off.
```

### plugin/source/letterbox.c:338

In the code now:

```
// Called once per sample_thread.c loop pass, with the exact same (TileParams, bufferAddr, unpack) that...
// More: docs/code-comments.md (plugin/source/letterbox.c:338)
```

Original (lines 338-353):

```
// Called once per sample_thread.c loop pass, with the exact same
// (TileParams, bufferAddr, unpack) that pass is already using to sample
// zones -- see that file's call site right before the zone-sampling
// loop, so a border committed this pass is honored by THIS pass's own
// g_zoneX/g_zoneY (buildZoneGeometry runs synchronously here, on commit,
// before returning).
// Margin the LED zones actually use for one edge, given that edge's
// stability-gated depth and its real measured depth. quantize() rounds
// DOWN to a band (33px vertical / 60px horizontal) so a pixel of jitter
// can't break the stability gate -- right for the gate, wrong for
// placement: a real 112px bar rounds to 99, and zones sitting at exactly
// the margin row then sample 13px INSIDE the black bar. This places them
// at the measured depth instead, plus a small inset so the sample box
// clears the bar. If the measurement itself is shallow, this errs a few
// px into the picture, never into the bar. See
// docs/debugging/letterbox-placement.md.
```

### plugin/source/letterbox.c:377

In the code now:

```
// First read of this enable just establishes a baseline -- see this file's top comment on why this...
// More: docs/code-comments.md (plugin/source/letterbox.c:377)
```

Original (lines 377-380):

```
// First read of this enable just establishes a baseline -- see
// this file's top comment on why this does NOT apply
// immediately, unlike live config reload elsewhere in this
// plugin.
```

### plugin/source/letterbox.c:388

In the code now:

```
// Detection moved -- restart the stability count against this new reading rather than the old one...
// More: docs/code-comments.md (plugin/source/letterbox.c:388)
```

Original (lines 388-390):

```
// Detection moved -- restart the stability count against this
// new reading rather than the old one, exactly like
// checkNewBorder's else branch.
```

### plugin/source/letterbox.c:400

In the code now:

```
// Stable result. "No bars" (isKnown == false) is just as valid and stable a result as a real border...
// More: docs/code-comments.md (plugin/source/letterbox.c:400)
```

Original (lines 400-403):

```
// Stable result. "No bars" (isKnown == false) is just as valid and
// stable a result as a real border -- e.g. the letterbox that was
// there a minute ago just ended -- and needs to actually clear the
// previously committed margin, not leave it stuck.
```

## plugin/source/main.c

### plugin/source/main.c:1

In the code now:

```
// ps4_ambient_light: the real per-frame pipeline.
// More: docs/code-comments.md (plugin/source/main.c:1)
```

Original (lines 1-49):

```
// ps4_ambient_light: the real per-frame pipeline. Everything here is
// proven infrastructure lifted directly from detile_verify_probe.prx
// (v2.2) -- the hooks, the buffer/format tracking, and the tiling
// math are unchanged from what was verified against a real
// screenshot. This file's only new job is to run that pipeline every
// frame instead of once on a button press, and turn the result into
// a live WLED signal instead of a debug dump.
//
// SCOPE (v2.0; the worker-thread architecture described below is
// unchanged by v2.0):
//   - Samples a fixed set of 229 screen zones per frame (not a full
//     frame detile), matching the real measured LED strip layout
//     (42/73/41/73, see buildZoneGeometry below) using ONLY the
//     confirmed-correct base tiling params. The Neo path
//     is dropped here; it served its purpose settling that question in
//     the probe and has no reason to run in the hot path.
//   - Pixel format is read live from sceVideoOutRegisterBuffers, not
//     assumed at compile time -- this fixed a real bug where
//     A2R10G10B10 math was applied to A8R8G8B8 data. If an
//     unknown format shows up, this plugin explicitly stops sending
//     color rather than guessing.
//   - v1.1: the actual zone-sampling + UDP send no longer runs inside
//     the flip hook. It's been moved to a dedicated worker thread
//     (ambient_sample_thread, same pattern as detile_verify_probe's
//     pad_poll_thread) so a ~2000-sample pass can never stall the
//     game's render/submission thread. The flip hook now only records
//     displayBufferIndex -- see the thread's own comment block for the
//     staleness tradeoff this introduces.
//   - v1.1: sampleZoneAverage now bounds-checks the computed tiled
//     offset against the real padded-buffer size before reading, and
//     skips (treats as black) any sample that would land outside it,
//     instead of trusting the offset formula unconditionally.
//   - v1.2: ambient_sample_thread now measures its own loop time
//     (buffer-resolve + 229-zone sampling + UDP send, i.e. everything
//     except usleep) and reports per-window min/max/avg microseconds
//     plus an over-budget count to DEBUG_IP every ~1s, turning "barely
//     noticeable" into an actual number. Gated behind TIMING_ENABLED so
//     it can be compiled out later. Decode with decode_verification_dump.py
//     v3+ (24-byte packets).
//
// STILL OPEN (do not treat this as fully validated):
//   - The worker-thread decoupling in v1.1 has not yet been measured
//     on real hardware for actual frame-time impact of the render-
//     thread-side work that remains (a couple of volatile writes per
//     flip). The v1.2 telemetry below measures the WORKER thread's own
//     loop time, which answers "is 30Hz sampling fast enough to keep
//     up" -- it does NOT measure render-thread-side cost, which is a
//     separate, still-open question if zone count or sample radius
//     increases later.
```

### plugin/source/main.c:70

In the code now:

```
// SPLIT NOTE: this file used to contain the entire pipeline above (~3240 lines, one translation unit).
// More: docs/code-comments.md (plugin/source/main.c:70)
```

Original (lines 70-79):

```
// SPLIT NOTE: this file used to contain the entire pipeline above
// (~3240 lines, one translation unit). It's now split into
// gamma.c, settings.c, network.c, tiling.c, pixel_formats.c, zones.c,
// color_processing.c, hooks.c, and sample_thread.c -- purely a file
// reorganization, no logic changed. main.c now only owns plugin
// lifecycle (plugin_load/plugin_unload/module_start/module_stop),
// which is what actually has to live at a fixed, GoldHEN-visible
// entry point. See ambient_internal.h for the shared types/externs
// tying the split files together, and each file's own top comment
// for what moved there.
```

### plugin/source/main.c:102  (kept in code as a summary)

In the code now:

```
// Bail out before any hook or thread when running inside the companion app (SHMY00091): hooking its flips crashed
// it with game tiling math. Keep the id in sync with companion-app/build.bat and CI.yml. [docs/code-comments.md]
```

Original (lines 102-124):

```
// v3.2: GoldHEN injects every plugin into every title's process,
// including this project's OWN companion app (titleid SHMY00091 --
// hardcoded here to match companion-app/build.bat's PKG_TITLE_ID and
// .github/workflows/CI.yml's TITLE_ID entry, both of which already
// hardcode this same literal rather than deriving it from one shared
// source; not worth adding cross-build-system config plumbing for a
// value that isn't expected to change -- if it ever does, update all
// three). Before this check, the v3.1 flip hooks fired on the
// companion app's own screen flips too: confirmed via a real crash
// report, thread "ambient_sample_thread", proc "eboot.bin", AppName
// "PS4 Ambilight", TitleID "SHMY00091", SIGSEGV in this plugin's own
// .prx applying game-buffer tiling math to the companion app's
// differently-shaped surface -- and, before that crash, the same
// root cause was already visibly sending the companion app's own
// screen colors to the strip, conflicting with whatever real
// testing/adjustment session was already running. Bailing out here,
// before ANY dlsym/hook/thread work, fixes both in one place rather
// than patching either symptom individually downstream.
// procInfo retrieved via sys_sdk_proc_info(). Fails safe: if
// sys_sdk_proc_info itself fails, this check is simply skipped
// rather than blocking plugin_load -- an unknown titleid is not
// evidence we're in the companion app, so proceeding normally is
// the safer default.
```

### plugin/source/main.c:152

In the code now:

```
// v3.1: two more real flip entrypoints -- see hooks.c's g_pluginVersion comment for why.
// More: docs/code-comments.md (plugin/source/main.c:152)
```

Original (lines 152-161):

```
// v3.1: two more real flip entrypoints -- see hooks.c's
// g_pluginVersion comment for why. Both resolved from the SAME
// sprx handles already loaded above (no new sys_dynlib_load_prx
// calls needed), and deliberately NOT fatal if either fails to
// resolve: a title with no use for a given entrypoint just never
// triggers that hook, same reasoning as sceSystemServiceGetStatus
// below. g_submitFlipPtrResolved/g_gnmForWorkloadPtrResolved (set
// here, read by the diagnostic packet) exist specifically so a
// future capture can tell "never resolved" apart from "resolved,
// never called" instead of both reading as a flat 0.
```

### plugin/source/main.c:168

In the code now:

```
// v2.4: resolved separately from the two required loads above, and deliberately NOT fatal if it fails...
// More: docs/code-comments.md (plugin/source/main.c:168)
```

Original (lines 168-176):

```
// v2.4: resolved separately from the two required loads above, and
// deliberately NOT fatal if it fails -- foreground/background
// detection is additive. If this symbol can't be resolved (wrong
// firmware, sprx renamed, etc.), ambient_sample_thread's own check
// below just no-ops (sceSystemServiceGetStatusPtr stays NULL) and
// this plugin falls back to its pre-v2.4 behavior: freezes the
// strip on the last frame when suspended, per the known, pre-
// existing gap -- not a new regression, just not yet
// fixed on whatever system rejected this symbol.
```

### plugin/source/main.c:183

In the code now:

```
// v3.9: resolve sceKernelVirtualQuery for the buffer readability guard (buffer_guard.c).
// More: docs/code-comments.md (plugin/source/main.c:183)
```

Original (lines 183-185):

```
// v3.9: resolve sceKernelVirtualQuery for the buffer readability guard
// (buffer_guard.c). Not fatal if it fails: the guard then fails open and
// the plugin behaves as v3.8 did.
```

### plugin/source/main.c:200  (kept in code as a summary)

In the code now:

```
// Forwarding-stub guard: proven necessary, do not skip. The two optional hook pointers are guarded only if resolved.
// More: docs/code-comments.md (plugin/source/main.c:200)
```

Original (lines 200-204):

```
// Same forwarding-stub guard as detile_verify_probe -- proven
// necessary, do not skip. v3.1: extended to the two new pointers,
// but only when they actually resolved above -- unlike the two
// required ones, a NULL here is an expected, harmless outcome, not
// a reason to abort plugin_load.
```

### plugin/source/main.c:230

In the code now:

```
// --- Resource-isolation status: MEASURE, not guessed --- Confirmed real, from the same...
// More: docs/code-comments.md (plugin/source/main.c:230)
```

Original (lines 230-263):

```
// --- Resource-isolation status: MEASURE, not guessed ---
// Confirmed real, from the same <orbis/libkernel.h> this file already
// includes:
//   scePthreadAttrInit(OrbisPthreadAttr*)
//   scePthreadAttrSetaffinity(OrbisPthreadAttr*, uint64_t mask)  -- must
//     be set on the attr BEFORE scePthreadCreate; the post-create
//     scePthreadSetaffinity() is declared with NO arguments in this
//     header (unresolved stub), so it is not a usable path.
//   scePthreadSetprio(OrbisPthread, int)                        -- set
//     AFTER create, on the returned handle.
//   sceKernelGetCurrentCpu(void)                                -- returns
//     the core index the CALLING thread is on right now.
//
// NOT yet known, and not fabricated here:
//   1) The uint64_t mask's bit-to-core mapping. The toolchain header's
//      own comment on this line says the real type ("OrbisKernelCpumask")
//      still needs porting -- i.e. even upstream hasn't confirmed
//      bit N == core N for this platform. Assumed elsewhere on other
//      platforms, not established here.
//   2) Which PS4 core(s) game threads actually run on / avoid. No public
//      source found documenting a game-vs-system core split for PS4,
//      the way e.g. Xbox 360 docs spell out reserved hardware threads.
//   3) scePthreadSetprio's number scale (does lower mean higher priority
//      here, and by how much) -- signature confirmed, semantics not.
//
// TIMING_ENABLED (already 1 by default) now also samples
// sceKernelGetCurrentCpu() once per telemetry window, so you can see,
// empirically, which core this thread actually lands on across a real
// session -- the same "measure it for real" approach as the loop-time
// work in. Once you've also logged sceKernelGetCurrentCpu
// from a thread you know is the game's own render/submission path (or
// found another reliable way to identify it), fill in CORE_MASK below to
// exclude that core, and verify with the same telemetry that the switch
// actually took effect, before trusting it.
```

### plugin/source/main.c:265  (kept in code as a summary)

In the code now:

```
// Leave thread affinity and priority unset until the CPU mask is measured: an unverified mask is a real risk.
// More: docs/code-comments.md (plugin/source/main.c:265)
```

Original (lines 265-271):

```
// Leave unset until (1) and (2) above are resolved. Setting affinity
// with an unverified mask is a real risk here, not just a style
// preference -- verified-wrong is one of this project's exact
// recurring bug patterns (// format-mismatch bugs,
// scePadOpen frequency bug), all traced back to this same category of
// "looked plausible, wasn't checked."
// #define CORE_MASK (1ULL << N)   // fill in N once measured, not before
```

### plugin/source/main.c:279

In the code now:

```
// v3.9.2: the cross-process state file (state.c), so a second, independent process -- a...
// More: docs/code-comments.md (plugin/source/main.c:279)
```

Original (lines 279-283):

```
// v3.9.2: the cross-process state file (state.c), so a second, independent process -- a
// compositor-level feed over in the research repo -- can tell whether this plugin is
// actively driving the strip right now. Own thread for the same reason as reportThread.
// Always started, unlike reportThread: see state.c's header comment for why this one has
// no opt-in key.
```

### plugin/source/main.c:286

In the code now:

```
// scePthreadAttrSetaffinity(&attr, CORE_MASK) + scePthreadCreate(&thread, &attr, ...) is the confirmed...
// More: docs/code-comments.md (plugin/source/main.c:286)
```

Original (lines 286-289):

```
// scePthreadAttrSetaffinity(&attr, CORE_MASK) + scePthreadCreate(&thread,
// &attr, ...) is the confirmed path once CORE_MASK is real. Same for
// scePthreadSetprio(thread, N) once N's semantics are confirmed against
// scePthreadGetprio()'s (also confirmed) default reading.
```

### plugin/source/main.c:291  (kept in code as a summary)

In the code now:

```
// Tell the relay "on" only after a load that got all the way through, and only if relay_signal_enabled;
// an early return means this plugin never drives the strip and nothing would send the matching "off". [docs/code-comments.md]
```

Original (lines 291-299):

```
// v2.3: only sent here, at the very end of a load that actually got
// this far (hooks installed, sample thread created) -- every early
// `return 0` above means this plugin never actually starts driving
// the strip, so telling the relay "on" from one of those paths
// would be a lie it has no way to detect or recover from on its
// own (nothing would ever send the matching "off").
// v2.6: explicit guard added now that relay_send_external_source()
// no longer gates itself internally -- a user who never opted in
// must not get a real send here.
```

### plugin/source/main.c:307  (kept in code as a summary)

In the code now:

```
// Entry thunk repairs RSP before any C runs: GoldHEN can enter with RSP 8 off 16-byte alignment, which crashed
// vfprintf (SIGBUS on MOVAPS, CUSA15090). Do not call the implementation directly. [docs/code-comments.md]
```

Original (lines 307-312):

```
// GoldHEN is the external caller for these callbacks. The crash dump for
// CUSA15090 recorded a SIGBUS at the first MOVAPS stack store in vfprintf,
// with the destination 8 bytes off a 16-byte boundary. The implementation
// above is ordinary compiler-generated C and assumes the SysV entry ABI;
// this assembly-only shim repairs RSP before any C code or libc formatting
// runs, without depending on how GoldHEN arrived at the export.
```

### plugin/source/main.c:317  (kept in code as a summary)

In the code now:

```
// Release the relay first in unload, even if load returned early, but only if relay_signal_enabled;
// relay_release_external_source() skips the "off" while the shell feed is running. [docs/code-comments.md]
```

Original (lines 317-336):

```
// v2.3: sent first and unconditionally, before any of the teardown
// below -- this must fire even if plugin_load returned early above
// and never actually hooked anything, since the relay's own state
// has no other way to notice this plugin is gone. Harmless if the
// matching "on" was never sent (tv_state.external_source_active is
// idempotent either way -- see tv_external_source.handle_payload).
// v2.6: explicit guard added now that relay_send_external_source()
// no longer gates itself internally. Checking the CURRENT value of
// relaySignalEnabled here is deliberately fine even if it differs
// from whatever it was when "on" was last sent: if true now,
// sending "off" is correct and harmless whether or not an "on"
// actually preceded it (idempotent on the relay
// side). If false now, either it was never enabled
// (no real "on" was ever sent, so none is owed back either), or it
// WAS enabled and got disabled via a live reload -- which already
// sent its own "off" at the moment it changed (see
// ambient_check_config_reload's flip-handler), making a second
// one here redundant, not missing.
// The shell feed, if it is running, takes the strip over from here and keeps the relay stood down:
// relay_release_external_source() only sends "off" when it is not.
```

### plugin/source/main.c:354

In the code now:

```
// Keep teardown under the same explicit ABI contract as plugin_load: the loader is outside this C...
// More: docs/code-comments.md (plugin/source/main.c:354)
```

Original (lines 354-356):

```
// Keep teardown under the same explicit ABI contract as plugin_load: the
// loader is outside this C translation unit, so neither callback may assume
// its incoming RSP already satisfies the 16-byte SysV call-site alignment.
```

## plugin/source/network.c

### plugin/source/network.c:1

In the code now:

```
// network.c -- part of ps4_ambient_light, split out of the original single-file main.c. WLED DDP...
// More: docs/code-comments.md (plugin/source/network.c:1)
```

Original (lines 1-4):

```
// network.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. WLED DDP transport, wled-relay signal, and debug/telemetry UDP sends.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
```

### plugin/source/network.c:23

In the code now:

```
// --- WLED DDP transport (identical pattern used throughout this project) --- v2.0: WLED host is now...
// More: docs/code-comments.md (plugin/source/network.c:23)
```

Original (lines 23-28):

```
// --- WLED DDP transport (identical pattern used throughout this project) ---
// v2.0: WLED host is now g_config.wledHost/wledPort (see the settings
// block above), not a compile-time constant. WLED_PORT is kept as a
// #define solely because debug_send_raw() below (the DEV-PC debug
// telemetry channel, not the production light) still uses a fixed
// port -- that's deliberately NOT user-configurable, it's a dev tool.
```

### plugin/source/network.c:32

In the code now:

```
// Compile-time cap for zone-array/DDP-packet-buffer sizing.
// More: docs/code-comments.md (plugin/source/network.c:32)
```

Original (lines 32-36):

```
// Compile-time cap for zone-array/DDP-packet-buffer sizing. Defined
// here (before first use in wled_send_rgb_zones below) rather than
// down in the geometry section, since C macro expansion needs the
// referenced name already defined by that point in the file, not just
// defined somewhere else in it.
```

### plugin/source/network.c:48

In the code now:

```
// UDP sendto() essentially never blocks (no handshake/ACK), but "essentially never" isn't "never" -- a...
// More: docs/code-comments.md (plugin/source/network.c:48)
```

Original (lines 48-55):

```
// UDP sendto() essentially never blocks (no handshake/ACK), but
// "essentially never" isn't "never" -- a full local send buffer
// or an unusual driver/network state could still stall this
// thread with no telemetry to explain why. A short send timeout
// bounds that to a few ms instead of an indefinite stall. This is
// plain POSIX SO_SNDTIMEO via <sys/socket.h>, the same header
// already included and working in this file -- not a new or
// unverified API surface like the affinity/priority calls above.
```

### plugin/source/network.c:62

In the code now:

```
// --- Debug/telemetry channel, step 2: a real number for ambient_sample_thread's own loop time, not...
// More: docs/code-comments.md (plugin/source/network.c:62)
```

Original (lines 62-71):

```
// --- Debug/telemetry channel, step 2: a real number for
// ambient_sample_thread's own loop time, not just "barely noticeable".
// Deliberately a SEPARATE socket/target from wled_ensure_socket() above --
// that one points at the real WLED light (g_config.wledHost, //); telemetry has no
// business going there. This reuses detile_verify_probe's proven
// wled_send_raw pattern (own socket, dev-PC debug IP, 64-byte payload
// cap, per-call socket()/close() since this is a ~1x/second send, not a
// hot path -- unlike wled_send_rgb_zones above there's no reason to hold
// a persistent socket open for this) rather than routing through the
// production WLED path.
```

### plugin/source/network.c:73

In the code now:

```
// a comment/reference of what this used to be hardcoded to. debug_send_raw now requires g_config.devIp...
// More: docs/code-comments.md (plugin/source/network.c:73)
```

Original (lines 73-76):

```
// a comment/reference of what this used to be hardcoded to.
// debug_send_raw now requires g_config.devIp
// to be set via an explicit [dev] section in the ini; see
// that function and the AmbientConfig struct comment.
```

### plugin/source/network.c:78

In the code now:

```
// Event-only foreground-handoff diagnostics.
// More: docs/code-comments.md (plugin/source/network.c:78)
```

Original (lines 78-80):

```
// Event-only foreground-handoff diagnostics. Reuse the existing opt-in dev UDP path
// (INI [dev] dev_ip + dev_logging=true), not stdout/printf. HOF1 lets the ground
// listener distinguish these ASCII records from the plugin's existing binary telemetry.
```

### plugin/source/network.c:98

In the code now:

```
// v2.2.5: single choke point for ALL debug/diagnostic UDP sends in this file (the v2.2.1 format-change...
// More: docs/code-comments.md (plugin/source/network.c:98)
```

Original (lines 98-105):

```
// v2.2.5: single choke point for ALL debug/diagnostic UDP sends in
// this file (the v2.2.1 format-change packet, the v2.2.2 raw-pixel
// dump, AND this pre-existing config-reload packet) -- requires an
// explicit opt-in via the ini's [dev] section, checked here rather
// than at each call site so there's exactly one place to get this
// right, not three. Neither devIp alone nor devLoggingEnabled alone
// is enough; both are required. See for why this replaced
// a hardcoded always-on DEBUG_IP.
```

### plugin/source/network.c:120  (kept in code as a summary)

In the code now:

```
// The debug packet cap must stay above the largest packet (the config-reload packet is 68 bytes):
// a 64-byte cap silently dropped its last field. [docs/code-comments.md]
```

Original (lines 120-127):

```
// BUGFIX (found via disassembly of a real deployed .prx, not source
// inspection alone -- the compiled binary's call site correctly passed
// 68 for the config-reload packet, but this cap silently truncated it
// to 64 before it ever hit the wire, dropping exactly the last_hash
// field added in v2.1.5). This cap predates that packet's growth past
// 64 bytes and was never updated to match. Raised with headroom rather
// than tuned to exactly 68, so the next packet that grows a few bytes
// doesn't silently reintroduce the same class of bug.
```

### plugin/source/network.c:143  (kept in code as a summary)

In the code now:

```
// sceSystemServiceGetStatus is resolved by name at runtime (the SDK header declares a bare stub) and its status buffer
// is padded generously: the real struct size is unverified, so a longer write must land in padding, not the stack. [docs/code-comments.md]
```

Original (lines 143-201):

```
// v2.3: tells wled-relay's tv_external_source.py whether THIS plugin
// is currently the one driving the TV backlight WLED controller
// directly (see the AmbientConfig relayHost/relayPort comment above
// for why this is a separate destination from the WLED controller
// itself). Deliberately a plain
// raw UDP payload, not wrapped in the DDP header used everywhere else
// in this file -- there's no WLED device on the receiving end here,
// just wled-relay's own tiny listener socket, and it expects the exact
// same bare "on"/"off" ASCII text tv_gate.py's MQTT handler already
// parses for gaming_mode, just delivered over a plain UDP datagram
// instead of an MQTT payload (this plugin has no MQTT client, and
// implementing one here for a single on/off flag isn't worth the
// complexity -- see that module's own docstring).
//
// One-shot, own socket per call (not the persistent g_wledSockfd) --
// this is called at most twice per game session (plugin_load,
// plugin_unload), nowhere near the per-frame hot path, so there's no
// reason to hold a socket open for it. Same pattern debug_send_raw
// already uses for exactly that reason.
// v2.4: real field layout confirmed against shadPS4's own independently
// reverse-engineered implementation (shadps4-emu/shadPS4,
// src/core/libraries/system/systemservice.{h,cpp} -- fetched and read
// directly from the SDK, not recited from memory) because this
// plugin's own OpenOrbis toolchain header (<orbis/SystemService.h>)
// declares sceSystemServiceGetStatus() as a bare, argument-less `void`
// stub -- the exact same "unfixed auto-generated placeholder" problem
// already flagged for sceHttpSetRecvTimeOut (see
// the project history). This plugin does NOT call through
// that broken declaration -- the real symbol is resolved by name at
// runtime instead (sys_dynlib_load_prx + sys_dynlib_dlsym in
// plugin_load), the exact same pattern this file already uses for
// sceVideoOutRegisterBuffers/sceGnmSubmitAndFlipCommandBuffers, with
// its own correct function-pointer type declared here.
//
// shadPS4's own struct only defines 5 known bool/int32 fields plus a
// flexible, UNSIZED `reserved[]` array at the end -- i.e. even that
// project doesn't claim to know this struct's true total size on real
// hardware, only enough of its front to satisfy real games running
// under HLE. Real firmware could plausibly write further into
// `reserved` than either project accounts for.
// AMBIENT_SYS_SERVICE_STATUS_PADDING pads this struct's real buffer
// generously past every known field specifically so an
// unexpectedly-larger real write lands in our own padding rather than
// adjacent stack memory -- this project has already had one full
// crash requiring a console power cycle from an unverified
// system-level assumption (klog -- see the project history
// "do not re-enable without a very good reason and a safety-net
// signal plan"), and this isn't repeating that mistake without a
// safety margin.
//
// NOT verified against Sony's own official SDK (not available to this
// session) or against real hardware -- only against shadPS4's public,
// independently-reverse-engineered source. See
// the project history for the full account and the
// recommended staged hardware-verification plan before trusting this
// for anything beyond an isolated debug-telemetry read-back test.
// (AMBIENT_SYS_SERVICE_STATUS_PADDING and the AmbientSystemServiceStatus
// struct itself now live in ambient_internal.h -- sample_thread.c needs
// the type too, to read status.isInBackgroundExecution.)
```

### plugin/source/network.c:205

In the code now:

```
// v2.4: last known foreground/background state, checked once per ambient_sample_thread iteration...
// More: docs/code-comments.md (plugin/source/network.c:205)
```

Original (lines 205-209):

```
// v2.4: last known foreground/background state, checked once per
// ambient_sample_thread iteration (throttled -- see the poll counter
// there) so both the direct WLED send AND the relay signal react to
// the same real transition instead of two separate, possibly
// disagreeing detectors.
```

### plugin/source/network.c:214  (kept in code as a summary)

In the code now:

```
// relay_send_external_source() must not gate on relaySignalEnabled itself (the true->false "off" would be
// swallowed); every call site guards on it instead. [docs/code-comments.md]
```

Original (lines 214-231):

```
// v2.6: BUGFIX -- this used to gate itself here on
// g_config.relaySignalEnabled, in one place, so no call site could
// accidentally dial someone else's relay_host default. That broke
// the one case that matters most: ambient_check_config_reload's
// own flip-handler (below) calls this exactly when
// relaySignalEnabled itself just transitioned -- including
// true->false. By the time that call happens, g_config.relaySignalEnabled
// is ALREADY false (that's what triggered the call), so this
// gate would silently swallow the "off" send the call exists to
// make, leaving wled-relay believing this plugin is still driving
// the strip until the separate gaming_mode-off backstop (that
// fork's own eventually corrects it as a side effect
// -- not by design. Found while adding this same signal to the
// companion app, reproduced directly in an isolated sandbox test
// before being traced back and fixed here too. The opt-in
// protection this gate existed for is now each call site's own
// responsibility instead -- see the comment at every call site
// below for why each one is safe for a user who never opted in.
```

### plugin/source/network.c:249  (kept in code as a summary)

In the code now:

```
// Send the relay "off" only if the shell feed is not running (g_shellFeedAlive): while it drives the strip,
// an "off" would bring the relay on over its colours. [docs/code-comments.md]
```

Original (lines 249-255):

```
// Hand the strip back to wled-relay -- but only if nothing else still holds it. The shell feed drives
// the strip whenever no game does (Home, Settings, the quick menu), so while it is running, a game that
// backgrounds or exits must NOT tell the relay "off": the relay would come on over the shell feed's colours.
// g_shellFeedAlive is state.c's latest read of the shell feed's heartbeat (kept fresh for as long as
// relay_signal_enabled is on). If the shell feed is not running (switched off, crashed, not installed, or
// the console has no shell feed at all), this is the plain "off" the plugin always sent.
// Callers still guard on relaySignalEnabled themselves, as for relay_send_external_source().
```

### plugin/source/network.c:262

In the code now:

```
// One timing-window packet, 24 bytes -- deliberately a length that collides with neither...
// More: docs/code-comments.md (plugin/source/network.c:262)
```

Original (lines 262-285):

```
// One timing-window packet, 24 bytes -- deliberately a length that
// collides with neither detile_verify_probe's 11-byte pixel packets nor
// its 32-byte registration packets, so decode_verification_dump.py can
// keep dispatching purely on len(data) the way it already does (v3 adds
// the len==24 case; see that script). All fields uint32 LE, matching
// every other multi-byte field in this project's wire formats.
//   [0:4]   min_us            -- fastest loop iteration this window
//   [4:8]   max_us            -- slowest loop iteration this window
//   [8:12]  avg_us            -- mean loop iteration this window
//   [12:16] sample_count      -- iterations folded into this window (window size, or less if just started)
//   [16:20] over_budget_count -- iterations that took longer than SAMPLE_INTERVAL_US (i.e. the thread fell behind its own ~30Hz target)
//   [20:24] window_id         -- increments every window; lets the listener notice a gap (plugin reload, crash, etc.)
// v1.2 was 24 bytes (minUs/maxUs/avgUs/sampleCount/overBudgetCount/windowId).
// This adds 3 more uint32 fields (minCpu/maxCpu/migrationCount) for a new,
// 36-byte length -- same "new packet length -> new dispatch case" pattern
// decode_verification_dump.py already uses to tell packet types apart
//, rather than reusing 24 for a changed layout.
//
// What this answers, using ONLY the confirmed sceKernelGetCurrentCpu()
// (no affinity/priority APIs involved -- this is pure observation):
// which core(s) this thread actually runs on across a session, and how
// often the scheduler moves it mid-session (migrationCount > 0 means it
// isn't pinned, by default, to one core). This is the actual measurement
// needed before CORE_MASK above can be filled in with anything real.
```

### plugin/source/network.c:304

In the code now:

```
// v3.1: what actually found "this title never lights up" -- see hooks.c's g_pluginVersion comment for...
// More: docs/code-comments.md (plugin/source/network.c:304)
```

Original (lines 304-320):

```
// v3.1: what actually found "this title never lights up" -- see
// hooks.c's g_pluginVersion comment for the full investigation. One
// packet per second from sample_thread.c, __FINAL__==0-gated same as
// every other packet in this file. 44 bytes, deliberately distinct
// from this project's other packet lengths (8 format, 36 timing, 68
// config-reload) so decode_verification_dump.py-style dispatch on
// len(data) keeps working without ambiguity.
//   [0:4]   register_hook_call_count            -- cumulative sceVideoOutRegisterBuffersPtr_hook calls this session
//   [4:8]   flip_hook_call_count                 -- cumulative sceGnmSubmitAndFlipCommandBuffersPtr_hook calls (the plain, wrapper entrypoint)
//   [8:12]  display_buffer_index                 -- g_currentDisplayBufferIndex; 0xFFFFFFFF == still the startup sentinel, no flip seen yet by ANY hook
//   [12:20] live_buffer_addr                     -- g_bufferAddrs[display_buffer_index], or 0 if the index above is still the sentinel
//   [20:24] have_valid_format                    -- g_haveValidFormat
//   [24:28] active_format                        -- g_activeFormat
//   [28:32] videout_submit_flip_hook_call_count  -- cumulative sceVideoOutSubmitFlipPtr_hook calls (the second, API-path entrypoint)
//   [32:36] submit_flip_ptr_resolved             -- 1 if dlsym found sceVideoOutSubmitFlip on this title/firmware, 0 if not -- disambiguates "never resolved" from "resolved, never called" when [28:32] reads 0
//   [36:40] gnm_for_workload_hook_call_count     -- cumulative sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr_hook calls (the third entrypoint, the one that turned out to matter)
//   [40:44] gnm_for_workload_ptr_resolved        -- same disambiguation as [32:36], for the third entrypoint
```

### plugin/source/network.c:343

In the code now:

```
// v2.1.3: peek at the first bytes of AMBIENT_CONFIG_PATH from the plugin's own point of view, so a...
// More: docs/code-comments.md (plugin/source/network.c:343)
```

Original (lines 343-353):

```
// v2.1.3: peek at the first bytes of AMBIENT_CONFIG_PATH from the
// plugin's own point of view, so a capture can show directly what's
// there instead of inferring it from size/mtime alone -- e.g. leftover
// test text vs. a truncated fragment of the real ini vs. something else
// entirely. Uses the same sceKernelOpen/sceKernelRead/sceKernelClose
// path as config.c's ini_table_read_from_file (see that file's v2.2
// comment) -- no fopen/FILE* here either, so this stays safe to call
// from ambient_sample_thread. Purely diagnostic: never touches g_config
// or any reload state, and any failure just zero-fills the preview
// rather than showing stack garbage or aborting the check.
// (CONFIG_DEBUG_PREVIEW_LEN now lives in ambient_internal.h -- settings.c needs it too.)
```

### plugin/source/network.c:364

In the code now:

```
// Config-reload diagnostic packet, added to actually SEE why live reload isn't firing on real hardware...
// More: docs/code-comments.md (plugin/source/network.c:364)
```

Original (lines 364-424):

```
// Config-reload diagnostic packet, added to actually SEE why live reload
// isn't firing on real hardware rather than guessing again (the v2.1.1
// mtime-sentinel fix didn't resolve it). One per ambient_check_config_reload()
// call -- reports every branch that function can take, not just the
// interesting one, so a silent "the check never even runs" is visible
// too, not just "it ran and saw no change".
//
// 60 bytes -- deliberately a length that collides with none of this
// project's existing packet types (10/11 pixel, 24 old timing, 32
// registration, 36 current timing, 44 pre-v2.1.3 version of this same
// packet), matching the same "new packet length -> new dispatch case"
// convention decode_verification_dump.py already uses.
//
// mtime/size are sent as full 64-bit values (not truncated to uint32
// like the other telemetry fields) specifically because the bug this is
// chasing is about mtime edge cases -- truncating away the high bits
// here would risk hiding exactly the kind of value this exists to show.
//   [0:4]   event:        0=reload disabled, 1=sceKernelOpen/Lseek
//                         failed (was "stat() failed" pre-v2.1.4),
//                         2=baseline just established, 3=checked, no
//                         change, 4=change detected, reload triggered
//   [4:8]   stat_errno:   negated orbis error code from the failed
//                         open/lseek (event==1 only, 0 otherwise) --
//                         was a libc errno from stat() pre-v2.1.4, now
//                         an orbis kernel error code from
//                         ambient_get_real_config_size; field kept at
//                         the same offset/name for capture compat
//   [8:16]  cur_mtime:    HARDCODED 0 as of v2.1.4 -- st_mtime never
// worked on this filesystem and is no
//                         longer read at all. Field kept at the same
//                         offset so old/new captures still line up
//                         byte-for-byte; not a live value.
//   [16:24] last_mtime:   HARDCODED 0 as of v2.1.4, same reason as
//                         cur_mtime above.
//   [24:32] cur_size:     st_size just read (0 if stat wasn't reached)
//   [32:40] last_size:    g_configLastSize BEFORE this check updated it
//   [40:44] check_count:  increments every call to
//                         ambient_check_config_reload -- if this never
//                         climbs, the sampling thread isn't reaching the
//                         check at all (wrong configReloadCheckSeconds,
//                         thread not running, etc.), which is a
//                         different bug than "the check runs but never
//                         detects a change".
//   [44:60] content_preview: first 16 raw bytes AMBIENT_CONFIG_PATH's own
//                         read path actually sees (v2.1.3, see
//                         ambient_read_content_preview above). Zero-filled
//                         when unavailable -- 16 zero bytes here does NOT
//                         necessarily mean the file is empty/all-zero, it
//                         may mean the open/read itself failed.
//   [60:64] cur_hash:     v2.1.5. FNV-1a/32 over the whole file, from
//                         ambient_get_real_config_size_and_hash. 0 if
//                         that call failed (event==1) or wasn't reached
//                         (event==0) -- note 0 is also a real possible
//                         hash value, same caveat as mtime==0 pre-v2.1.1,
//                         but here it's disambiguated by event/stat_errno
//                         rather than by a separate sentinel, since this
//                         field was never used as its own "have we
//                         initialized yet" flag the way mtime was.
//   [64:68] last_hash:    v2.1.5. g_configLastHash BEFORE this check
//                         updated it. Same 0-is-ambiguous caveat as
//                         cur_hash above.
```

### plugin/source/network.c:446

In the code now:

```
// v2.0: MAX_ZONES is just an alias for MAX_TOTAL_ZONES (defined above, near DDP_HEADER_SIZE) -- kept...
// More: docs/code-comments.md (plugin/source/network.c:446)
```

Original (lines 446-449):

```
// v2.0: MAX_ZONES is just an alias for MAX_TOTAL_ZONES (defined above,
// near DDP_HEADER_SIZE) -- kept as its own name here since it documents
// *why* this buffer is sized the way it is (DDP packet cap), while the
// geometry code below cares about it as "max zone count" instead.
```

### plugin/source/network.c:452

In the code now:

```
// v2.8: WLED heartbeat. WLED falls back to its own realtime-timeout color after a gap in packets...
// More: docs/code-comments.md (plugin/source/network.c:452)
```

Original (lines 452-459):

```
// v2.8: WLED heartbeat. WLED falls back to its own realtime-timeout
// color after a gap in packets -- see the v2.2.3 changelog entry and
// the "deliberately send nothing rather than guess" comment in
// sample_thread.c for the real, still-open silent path this closes
// (an unknown/transitioning pixel format while foregrounded). This
// tracks the last color actually sent so a stale-send check can
// repeat it, without ever inventing a color that wasn't verified by
// the real pipeline.
```

### plugin/source/network.c:481  (kept in code as a summary)

In the code now:

```
// Hold the first send until a non-black frame, but only while the shell feed is running to cover (the first send
// publishes fg=1 and silences it). With no shell feed there is no hold. One-way latch. [docs/code-comments.md]
```

Original (lines 481-489):

```
// Startup hold: until the game feed has produced one non-black frame, send nothing -- but only while the
// shell feed is actually running to cover for us. The first send is what publishes fg=1 (state.c), and
// fg=1 makes the shell feed back off. During a launch picture the game's own buffers can read all zeros
// while the picture is on screen, so sending those frames both blacks out the strip and silences the
// shell feed that still has the right colours. If the shell feed is not running (switched off, crashed or
// not installed: g_shellFeedAlive, kept by state.c from its heartbeat file) there is nobody to cover, and
// holding would leave the strip on WLED's default colour through a genuinely black loading screen, so the
// plugin sends from the first available frame, as it did before the hold existed. One-way latch: once a
// frame has passed, dark scenes are sent normally.
```

### plugin/source/network.c:529

In the code now:

```
// LAUNCH_GAP dark-send tracking: tells "packets stopped" (LG_SEND_GAP) apart from "packets kept coming...
// More: docs/code-comments.md (plugin/source/network.c:529)
```

Original (lines 529-531):

```
// LAUNCH_GAP dark-send tracking: tells "packets stopped" (LG_SEND_GAP) apart from "packets kept
// coming but were black". A run of >= LG_DARK_MIN_FRAMES sends whose brightest channel is <= LG_DARK_MAX
// is reported once when it is confirmed and once when it ends. Event-only, capped.
```

### plugin/source/network.c:573

In the code now:

```
// This counter is deliberately a transport-progress signal, not a flip signal.
// More: docs/code-comments.md (plugin/source/network.c:573)
```

Original (lines 573-575):

```
// This counter is deliberately a transport-progress signal, not a flip signal.
// UDP cannot confirm receipt by WLED, but a complete sendto() result tells the
// ownership writer that this process is still successfully submitting DDP.
```

### plugin/source/network.c:579

In the code now:

```
// v2.8: snapshot what actually went out, so a later stale-send check (wled_send_keepalive_if_stale)...
// More: docs/code-comments.md (plugin/source/network.c:579)
```

Original (lines 579-582):

```
// v2.8: snapshot what actually went out, so a later stale-send check
// (wled_send_keepalive_if_stale) has a real, previously-sent color to
// repeat -- covers both a normal per-frame send and a heartbeat
// resend, so the timer always measures from the last real packet.
```

### plugin/source/network.c:591

In the code now:

```
// v2.8: called once per sample-thread iteration, but ONLY from the no-valid-frame branch...
// More: docs/code-comments.md (plugin/source/network.c:591)
```

Original (lines 591-596):

```
// v2.8: called once per sample-thread iteration, but ONLY from the
// no-valid-frame branch (unknown/transitioning pixel format while still
// foregrounded) -- never from the g_isBackgrounded path, where going
// silent is intentional (v2.4) and a heartbeat here would reintroduce
// the frozen-strip-on-suspend bug that fix removed. Resends the last
// verified color rather than guessing a new one.
```

### plugin/source/network.c:606

In the code now:

```
// v3.9: buffer guard debug packets (debug builds only, sent from sample_thread.c about once a second).
// More: docs/code-comments.md (plugin/source/network.c:606)
```

Original (lines 606-610):

```
// v3.9: buffer guard debug packets (debug builds only, sent from sample_thread.c about once a
// second). Each starts with a 4 byte ASCII tag, like FLK1, PQ8C and HDRV. The tag is how to tell
// them apart: GRDI is 48 bytes after the DDP header, the same length as PQ8C and HDRV, so a
// decoder that goes by length alone will misread one as the other (this happened while decoding
// the first MK11 capture, see docs/debugging/gpu-only-buffers.md).
```

### plugin/source/network.c:612

In the code now:

```
// GRDC -- guard counters, 24 bytes.
// More: docs/code-comments.md (plugin/source/network.c:612)
```

Original (lines 612-620):

```
// GRDC -- guard counters, 24 bytes.
//   [0:4]   "GRDC"
//   [4:8]   guard_available  -- 1 if sceKernelVirtualQuery resolved (0 = the guard is failing open)
//   [8:12]  reject_count     -- cumulative passes skipped because the buffer was not readable
//   [12:16] last_ret         -- 0 ok, <0 VirtualQuery error, 1 unmapped gap, 2 not CPU-readable
//   [16:24] last_bad_addr    -- last address rejected (compare with a crash report's fault address)
// reject_count keeps counting a buffer that is later made readable only by its first rejection,
// so on a fixed title it stops at one per slot; on a title that stays dark it climbs by the sampler
// rate (about 30 a second).
```

### plugin/source/network.c:632

In the code now:

```
// GRDI -- kernel map info for the last region that failed the readability check, 52 bytes.
// More: docs/code-comments.md (plugin/source/network.c:632)
```

Original (lines 632-641):

```
// GRDI -- kernel map info for the last region that failed the readability check, 52 bytes.
// Only sent once reject_count is above 0. It is a snapshot of the LAST failed query, so on a title
// that was fixed by the mprotect it keeps showing the pre-fix protection (0x30) after the buffer
// became readable; read it together with RMAP, not on its own.
//   [0:4]   "GRDI"
//   [4:12]  start  [12:20] end  [20:28] direct-memory offset  [28:36] address queried
//   [36:40] protection (bit0 CPU read, bit1 CPU write, bit4 GPU read, bit5 GPU write)
//   [40:44] memoryType (MK11: 3, write-combined)
//   [44:48] flags byte (bit1 direct, bit4 committed; MK11: 0x12)
//   [48:52] guard reason
```

### plugin/source/network.c:654

In the code now:

```
// RMAP -- GPU-only buffer remap status, 28 bytes.
// More: docs/code-comments.md (plugin/source/network.c:654)
```

Original (lines 654-665):

```
// RMAP -- GPU-only buffer remap status, 28 bytes.
//   [0:4]   "RMAP"
//   [4:8]   flags: bits 0-3 gpu_only_remap setting (default 3), bit4 sceKernelMapDirectMemory2
//           resolved, bit5 sceKernelMapDirectMemory resolved, bit6 sceKernelMunmap resolved,
//           bit7 sceKernelMprotect resolved, bits 8-15 last method tried
//           (1 typed MapDirectMemory2, 2 plain MapDirectMemory, 3 mprotect)
//   [8:12]  created  -- buffers made readable (alias views created, or mprotects that stuck)
//   [12:16] failed   -- regions every enabled method failed on (each is remembered, not retried)
//   [16:20] passes   -- resolve calls served by a remap (an mprotected buffer reads directly from
//                       the next pass on, so this stays small on a title fixed by method 3)
//   [20:24] last_ret -- last method's return code, 0 ok. 0x80020010 = EBUSY (second mapping refused)
//   [24:28] low 32 bits of the last alias address (0 when no alias was made)
```

## plugin/source/notices.c

### plugin/source/notices.c:1

In the code now:

```
// notices.c -- part of ps4_ambient_light (plugin only; settings.c and the other shared files never...
// More: docs/code-comments.md (plugin/source/notices.c:1)
```

Original (lines 1-15):

```
// notices.c -- part of ps4_ambient_light (plugin only; settings.c and the other shared files never
// call into this file, because the shell feed compiles them too and does not link it).
//
// System-notification-bar messages for things that would otherwise fail silently: the strip not
// lighting because there is no WLED address, an ini value the plugin ignored, a title it can't read,
// a plugin update, debug logging left on. Every one is sent at most once per game launch (the plugin
// is injected into every title's process, so "once" here means once per process), apart from the
// update notice, which is gated by a marker file so it shows once per version.
//
// Deliberately NOT here: anything about the network. Whether the strip is reachable is not this
// plugin's to know (a router, a power cut, a sleeping WLED), and a notice that is missing the next
// time would read as "the plugin is fine, so my LEDs are broken" -- or the reverse.
//
// Decisions are plain C over the globals settings.c sets, so tools/test_notices.c can drive them on
// a PC. Messages are one line, under ~150 characters.
```

### plugin/source/notices.c:48

In the code now:

```
// plugin update (marker file) The first run only records the version: a fresh install already knows it...
// More: docs/code-comments.md (plugin/source/notices.c:48)
```

Original (lines 48-50):

```
// ---- plugin update (marker file) ----
// The first run only records the version: a fresh install already knows it just installed. After
// that, a different version in the marker means the .prx was replaced.
```

### plugin/source/notices.c:74

In the code now:

```
// WLED address Only "there is none", never "it doesn't answer".
// More: docs/code-comments.md (plugin/source/notices.c:74)
```

Original (lines 74-76):

```
// ---- WLED address ----
// Only "there is none", never "it doesn't answer". Once an address shows up after a warning, say so,
// so the person who just fixed it isn't left wondering.
```

## plugin/source/pixel_formats.c

### plugin/source/pixel_formats.c:1

In the code now:

```
// pixel_formats.c -- part of ps4_ambient_light, split out of the original single-file main.c. Runtime...
// More: docs/code-comments.md (plugin/source/pixel_formats.c:1)
```

Original (lines 1-4):

```
// pixel_formats.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. Runtime pixel-format dispatch, incl. real PQ/HDR decode.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
```

### plugin/source/pixel_formats.c:21

In the code now:

```
// Runtime pixel-format dispatch -- same principle as ps4_detile_2dthin.c's getUnpackFnForFormat()...
// More: docs/code-comments.md (plugin/source/pixel_formats.c:21)
```

Original (lines 21-27):

```
// ============================================================
// Runtime pixel-format dispatch -- same principle as
// ps4_detile_2dthin.c's getUnpackFnForFormat(): read live off the
// registration hook, never assumed at compile time. Unknown format ->
// NULL -> this plugin explicitly stops sending color instead of
// guessing (see the flip hook below).
// ============================================================
```

### plugin/source/pixel_formats.c:48

In the code now:

```
// A8B8G8R8 / A8B8G8R8_SRGB (format 0x80002200) -- R and B swapped vs. unpackA8R8G8B8_to_rgb888 above.
// More: docs/code-comments.md (plugin/source/pixel_formats.c:48)
```

Original (lines 48-80):

```
// A8B8G8R8 / A8B8G8R8_SRGB (format 0x80002200) -- R and B swapped vs.
// unpackA8R8G8B8_to_rgb888 above.
//
// This format was first identified purely by name, against fpPS4's
// enum table, and the swap this function implies was tried, then
// REVERTED, based on a real hardware report at the time ("red and
// blue are wrong" on a busy in-game scene). That report was later
// confirmed to have been given uncertainly, without HDR controlled
// for as a variable, and without a real screenshot to check specific
// pixels against -- just an impression from a scene lit mostly in
// warm reds and oranges, where a channel swap is genuinely hard to
// eyeball. This function reverses that revert, based on stronger
// evidence gathered afterward: a real screenshot, sampled pixel-for-
// pixel at 5 known coordinates, checked numerically (not by eye)
// against a probe capture with HDR explicitly turned OFF. The swapped
// decode matched the real screen within single-digit-to-teens RGB
// units at all 5 points simultaneously; the no-swap version never
// matched at any point, on any screen, across the whole investigation
// (see detile_verify_probe's history/ for the full trail).
//
// The format ID itself was confirmed UNCHANGED (0x80002200) between
// HDR on and off via a fresh registration event -- so this isn't a
// case of the game switching formats. HDR was corrupting the actual
// buffer content (a garbage-looking alpha byte, among other things),
// not just which channel maps to which byte.
//
// CONFIRMED FOR HDR-OFF ONLY. This plugin has no way to detect the
// console's HDR setting, and HDR-on behavior for this format is
// still completely unverified -- it may need a different unpack
// entirely (the same way 0x88740000's PQ curve needed its own
// function above, not just a channel reorder). If colors look wrong
// again on this format specifically, check HDR status before
// assuming another channel-order bug.
```

### plugin/source/pixel_formats.c:88

In the code now:

```
// HDR (A2R10G10B10_BT2020_PQ) decode -- ported from decode_verification_dump.py's...
// More: docs/code-comments.md (plugin/source/pixel_formats.c:88)
```

Original (lines 88-107):

```
// ------------------------------------------------------------------
// HDR (A2R10G10B10_BT2020_PQ) decode -- ported from
// decode_verification_dump.py's unpack_a2r10g10b10_bt2020_pq, verified
// there against a real capture + ground-truth screenshot (5 test
// points, 3/5 usable for real color -- PQ decode landed 6-12x closer
// to the actual on-screen pixel than plain truncation on the two
// midtone points). This format was PREVIOUSLY routed to
// unpackA2R10G10B10_to_rgb888 above -- the same plain truncation used
// for gamma-encoded SDR -- which is why HDR colors looked wrong: PQ
// (SMPTE ST 2084) is a fundamentally different curve, not a gamma
// variant. Bit LAYOUT (which 10 bits are R/G/B/A) is unchanged from
// SDR A2R10G10B10 -- only the interpretation of the numbers differs.
//
// PQ_REFERENCE_WHITE_NITS / PQ_TONE_MAP_MAX_NITS are the same tunable
// ASSUMPTIONS as the Python script, not measured constants -- if HDR
// colors still look off after this ships, these two are the knobs to
// adjust (empirically, against a real capture, same method as /
// and the screenshot verification). The PQ EOTF and BT.2020->BT.709
// matrix below are fixed standards and should not need touching.
// ------------------------------------------------------------------
```

### plugin/source/pixel_formats.c:113  (kept in code as a summary)

In the code now:

```
// PQ EOTF is a precomputed 1024-entry table: this build does not link libm, so no pow() here.
// Matches the Python reference to within 1 unit. [docs/code-comments.md]
```

Original (lines 113-126):

```
// Precomputed on the dev machine (Python, matching
// decode_verification_dump.py's unpack_a2r10g10b10_bt2020_pq exactly)
// and verified to reproduce identical output to a live pow()-based
// version on 5 real captured pixels (max 1-unit diff,
// from LUT quantization -- well within the photo-comparison noise
// floor already established). Table lookups only, no libm calls --
// see buildZoneGeometry's comment above: this build does not link
// libm, and after the printf/klog crash history in this project,
// nothing here should assume a library "just works" without checking.
//
// kPqEotfNitsLut: indexed by the raw 10-bit code value (0-1023) ->
// linear light in nits (SMPTE ST 2084 inverse EOTF). Domain is exactly
// 1024 discrete values, so a direct 1-to-1 LUT loses zero precision
// versus computing pow() live.
```

### plugin/source/pixel_formats.c:258

In the code now:

```
// kSrgbEncode8Lut: indexed by a linear value in [0,1] quantized to 4096 steps -> final 8-bit...
// More: docs/code-comments.md (plugin/source/pixel_formats.c:258)
```

Original (lines 258-263):

```
// kSrgbEncode8Lut: indexed by a linear value in [0,1] quantized to
// 4096 steps -> final 8-bit sRGB-encoded output. This stage's input is
// continuous (post tone-map, post gamut matrix), so this is a
// quantized approximation rather than an exact table -- 4096 steps
// keeps quantization error under 1 code value almost everywhere,
// same order as the LUT-vs-pow() check above.
```

### plugin/source/pixel_formats.c:530

In the code now:

```
// v2.2.3 PERFORMANCE FIX (not a color-quality change -- see note below): pqToneMap() above is a pure...
// More: docs/code-comments.md (plugin/source/pixel_formats.c:530)
```

Original (lines 530-551):

```
// v2.2.3 PERFORMANCE FIX (not a color-quality change -- see note below):
// pqToneMap() above is a pure function of ONE 10-bit code (it only ever
// gets called as pqToneMap(kPqEotfNitsLut[someCode])), so composing the
// two into a single precomputed 1024-entry table removes two live
// divisions from the hottest path in the plugin: this runs up to
// (2*scanDepth+1)^2 times per zone, times up to ~229 zones, times up to
// 30 times a second -- on the order of 150,000+ calls/sec at this
// project's own default layout. Under CPU pressure (a game's loading
// screen was captured at 97% CPU usage during the investigation that
// led here.
//
// This is IDENTICAL output to calling pqToneMap(kPqEotfNitsLut[code])
// live, not an approximation -- same double-precision formula, same
// kPqEotfNitsLut values, just evaluated once at first use instead of
// on every single sample. Verified bit-for-bit identical against the
// live computation across all 1024 codes before this was wired in
// (see the session's own check, not restated here). This is exactly
// the same kind of "no libm on the hot path" pattern this file already
// uses everywhere else (kGammaLuts, kSrgbEncode8Lut) -- pqToneMap's
// only new since being one degree removed (a LUT of a LUT) rather than
// hardcoded at compile time, since it's genuinely cheap and libm-free
// to build once at runtime, unlike the gamma curves.
```

### plugin/source/pixel_formats.c:554

In the code now:

```
// atomics/locking ONLY because this is read/written from the plugin's single background sampling thread.
// More: docs/code-comments.md (plugin/source/pixel_formats.c:554)
```

Original (lines 554-561):

```
// atomics/locking ONLY because
// this is read/written from
// the plugin's single
// background sampling thread.
// If a future change ever
// calls the PQ unpack fn from
// more than one thread, this
// needs a real guard.
```

### plugin/source/pixel_formats.c:609

In the code now:

```
// v2.8: live HDR-vs-SDR auto-detection for format 0x80002200 specifically -- this format ID is...
// More: docs/code-comments.md (plugin/source/pixel_formats.c:609)
```

Original (lines 609-642):

```
// v2.8: live HDR-vs-SDR auto-detection for format 0x80002200
// specifically -- this format ID is confirmed AMBIGUOUS on this title
// (see unpackA8B8G8R8_to_rgb888's own comment): it means A8B8G8R8_SRGB
// with HDR off, but the SAME registered format ID actually holds
// A2R10G10B10_BT2020_PQ data when HDR is on, with no reliable signal
// available from any hooked API to tell which is currently true --
// hooking sceVideoOutAddBufferHdrPrivilege (the obvious direct signal)
// was investigated and ruled out, since no real reference
// implementation of it exists anywhere (not in this codebase's history,
// not in fpPS4) to safely derive its call signature from, and a wrong
// guess there risks corrupting a call into Sony's own code.
//
// Detected instead via decode-smoothness: real image content decodes
// smoothly under the correct hypothesis and noisily under the wrong
// one. Method validated twice, blind, against real captures in
// decode_verification_dump.py before being ported here -- see that
// script's own comment and this repo's for the full trail.
//
// Kept deliberately cheap, per explicit instruction not to have this
// plugin compete with the game for CPU: reuses zone coordinates
// already computed for real LED sampling (no new coordinate math),
// reads a small fixed subset (HDR2200_DETECT_SAMPLES) rather than
// every configured zone, and only runs once every
// HDR2200_DETECT_INTERVAL sampling passes -- not every pass. A
// hysteresis streak (not a single-shot decision) guards against
// flipping on one noisy/degenerate frame -- exactly the failure mode
// that made this heuristic unreliable on near-black loading screens
// during validation; HDR2200_NOISE_FLOOR skips a check entirely when
// there isn't enough real signal to trust it either way.
//
// UNTESTED WITH HDR ACTUALLY ON. Nothing in this whole investigation
// has run this decision live, watching real LED output, with HDR
// engaged
// been verified before treating this as a finished fix.
```

### plugin/source/pixel_formats.c:651

In the code now:

```
// v3.5: 1 while a 0x88740000 buffer holds 8-bit A8R8G8B8 instead of real PQ.
// More: docs/code-comments.md (plugin/source/pixel_formats.c:651)
```

Original (lines 651-653):

```
// v3.5: 1 while a 0x88740000 buffer holds 8-bit A8R8G8B8 instead of real PQ.
// Decided every pass by detectPq8bitMisregistration() in zones.c. Starts at 0
// (real PQ, what the format ID says) and is cleared on a format change.
```

### plugin/source/pixel_formats.c:662

In the code now:

```
// v3.5: YouTube registers this ID with HDR on but draws plain 8-bit ARGB into the buffer while it...
// More: docs/code-comments.md (plugin/source/pixel_formats.c:662)
```

Original (lines 662-665):

```
// v3.5: YouTube registers this ID with HDR on but draws plain 8-bit
// ARGB into the buffer while it plays SDR video. Which one it is gets
// decided per frame by detectPq8bitMisregistration() (zones.c), called
// just before this from the sampling loop. Real HDR video stays PQ.
```

### plugin/source/pixel_formats.c:671

In the code now:

```
// same registered format ID means two different real byte layouts on this title, decided live by...
// More: docs/code-comments.md (plugin/source/pixel_formats.c:671)
```

Original (lines 671-676):

```
// same registered format ID means two different real byte layouts
// on this title, decided live by detectHdr2200Format() (called
// just before this from the main sampling loop). See that
// function's own comment above g_hdr2200IsHdr for the full trail.
// v3.6: decided per frame from the alpha byte first, see zones.c and
// hdr2200_vote.h. Confirmed live with HDR on and off (HITMAN 3, RDR2).
```

## plugin/source/report.c

### plugin/source/report.c:1

In the code now:

```
// report.c -- part of ps4_ambient_light (v3.9.1).
// More: docs/code-comments.md (plugin/source/report.c:1)
```

Original (lines 1-28):

```
// report.c -- part of ps4_ambient_light (v3.9.1).
//
// Writes /data/ps4-ambilight/ps4_ambient_report_<TITLEID>.txt from a release build, so a tester can send
// one small file instead of running a debug build and capturing UDP. The text itself is
// built by report_file.h (string logic only, host-tested). This file does the two things
// that need the console: reading the plugin's existing globals, and writing the file.
//
// Why a thread of its own and not a call from ambient_sample_thread: a file write takes
// milliseconds on the console's disk, and the sampler's whole budget is 33 ms a pass at
// 30 Hz. The sampler is not touched except for a counter increment per loop and three
// stores once per 30-pass timing window (sample_thread.c). This thread only ever reads.
//
// When it writes. Once at start, then whenever the stage, the format or the guard/remap
// outcome changes (report_changed), and every REPORT_REFRESH_SECS otherwise so the timing
// and counters stay current. The change-driven writes are the point: a title that kills the
// game on its first sampling pass (Mortal Kombat 11 before v3.9) never gets to a "final"
// write, but the file on disk still says which stage it reached and what the guard saw.
//
// Opt-in: OFF unless the ini has [compat] report_file=1. Optional and not in the shipped ini, so a
// normal install never writes the file; a tester adds the key. 0, or no key, writes nothing.
// The thread still runs (a live ini reload can switch it on mid-game) but does nothing while off.
// After 3 failed writes in a row (no /data, disk full) it stops trying for the session.
// A title the plugin never hooks (plugin_load returned early) gets no file at all, and that
// absence is itself a finding, see docs/debugging/report-file.md.
//
// Known gap, accepted: the file is truncated and rewritten in place, so a crash in the
// middle of a write leaves an empty or cut-off file. The "end of report" line marks a
// complete one. Writing to a temporary name and renaming would need sceKernelRename.
```

### plugin/source/report.c:58

In the code now:

```
// Same raw-kernel-call route settings.c uses for the ini (sceKernelOpen, not fopen).
// More: docs/code-comments.md (plugin/source/report.c:58)
```

Original (lines 58-61):

```
// Same raw-kernel-call route settings.c uses for the ini (sceKernelOpen, not fopen). The flags
// are FreeBSD's: O_WRONLY 0x0001, O_CREAT 0x0200, O_TRUNC 0x0400. The default-ini write in
// settings.c leaves O_TRUNC off because the file is new; this one is rewritten in place, and
// without O_TRUNC a shorter report would leave the old tail behind.
```

## plugin/source/sample_thread.c

### plugin/source/sample_thread.c:1

In the code now:

```
// sample_thread.c -- part of ps4_ambient_light, split out of the original single-file main.c. The...
// More: docs/code-comments.md (plugin/source/sample_thread.c:1)
```

Original (lines 1-4):

```
// sample_thread.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. The worker thread: resolves the live buffer, samples zones, applies color, sends UDP -- decoupled from the render/submission path.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
```

### plugin/source/sample_thread.c:22

In the code now:

```
// v1.1: all the actual work -- resolving the live buffer, sampling 229 zones, sending the UDP packet...
// More: docs/code-comments.md (plugin/source/sample_thread.c:22)
```

Original (lines 22-46):

```
// v1.1: all the actual work -- resolving the live buffer, sampling 229
// zones, sending the UDP packet -- now happens here, on its own
// thread, at its own ~30Hz pace, never inside the render/submission
// path. Same scePthreadCreate pattern as detile_verify_probe's
// pad_poll_thread.
//
// Tradeoff this introduces (worth understanding, not a hidden cost):
// this thread can read g_currentDisplayBufferIndex / g_bufferAddrs /
// g_activeFormat at any point in a flip's lifecycle, including
// mid-update on another thread. Every one of those is a single
// word-sized volatile read/write (same pattern already used
// throughout this project without locks), so there's no torn-read risk
// on real hardware, but the *value* read could be up to one flip old.
// At ~30Hz sampling against ~60Hz flips, worst case this thread is
// working from a frame that's already been superseded by a newer one
// by the time it finishes reading -- i.e. the light output can lag
// real screen content by up to roughly one extra frame interval versus
// the old in-hook version. That's the price for never risking a stall
// on the thread the game actually needs to stay smooth, and is a far
// better tradeoff for a hobby ambient-light feature than occasional
// frame hitches during real play.
// v2.0: was a fixed #define (33*1000, ~30Hz). Now computed once at
// thread start from g_config.updateFrequencyHz (validated 1-240 at
// load time, see ambient_load_config). Kept as a plain local rather
// than a macro since it's no longer a compile-time constant.
```

### plugin/source/sample_thread.c:48

In the code now:

```
// step 2: window size for the timing telemetry below. 30 iterations at the ~30Hz default sample rate...
// More: docs/code-comments.md (plugin/source/sample_thread.c:48)
```

Original (lines 48-54):

```
// step 2: window size for the timing telemetry below.
// 30 iterations at the ~30Hz default sample rate is roughly one
// telemetry packet per second of real play -- frequent enough to see
// trends during a session, not so frequent it competes with the actual
// zone-color UDP traffic. TIMING_ENABLED lets this be compiled out
// entirely for a "final" build once the number is in hand, same spirit
// as PROBE_INCLUDE_NEO in detile_verify_probe.
```

### plugin/source/sample_thread.c:60

In the code now:

```
// v2.0 smoothing state: previous frame's post-processed color per zone, so each new sample can be...
// More: docs/code-comments.md (plugin/source/sample_thread.c:60)
```

Original (lines 60-67):

```
// v2.0 smoothing state: previous frame's post-processed color per
// zone, so each new sample can be blended toward instead of snapped
// to. This is a simple discrete integer approximation of an RC-style
// exponential smoothing, NOT a calibrated real time-constant -- alpha
// is derived from settlingTimeMs vs the actual sample interval and
// verified in Python to converge monotonically without oscillation
// across a range of settling times, but "settling_time_ms
// = 200" should be read as "roughly", not as a precise guarantee.
```

### plugin/source/sample_thread.c:71

In the code now:

```
// v3.7 read-verify. A zone whose new read differs a lot from the last accepted one is re-read from the...
// More: docs/code-comments.md (plugin/source/sample_thread.c:71)
```

Original (lines 71-91):

```
// v3.7 read-verify. A zone whose new read differs a lot from the last accepted
// one is re-read from the SAME buffer once the whole pass has finished
// sampling; if the two reads of one buffer disagree, the buffer was still
// being written (or cleared) while we looked, so the new value is discarded
// and the last accepted one is kept.
// This is title-agnostic: it never asks "which buffer / how many flips back",
// only "did this buffer hold still for the length of one pass".
//
// Tried first and dropped: a gate that only trusted a change once it held for
// 2 passes. A stale slot that stays stale repeats, so it "persists" and gets
// trusted, and every real transition paid an extra pass of delay for it (a GOWR
// capture showed the flash get through). Read-verify costs nothing on static
// content (no zone is flagged) and adds no delay to a real change: a stable
// buffer reads the same twice, so it is accepted on the first pass.
//
// Known limit: a stale slot that is genuinely not being touched during the
// pass reads identically twice and is accepted; only a buffer that changes
// under the sampler is caught. The hold is capped (VERIFY_MAX_HOLD passes per
// zone) so a title that rewrites its buffer continuously cannot freeze the
// lights. State resets with g_smoothedRgbValid (first frame, letterbox commit,
// live settings reload) so a deliberate hard cut is accepted immediately.
```

### plugin/source/sample_thread.c:103

In the code now:

```
// LAUNCH_GAP milestones (docs/debugging/launch-gap-test.md).
// More: docs/code-comments.md (plugin/source/sample_thread.c:103)
```

Original (lines 103-106):

```
// LAUNCH_GAP milestones (docs/debugging/launch-gap-test.md). Event-only HOF1 records over the opt-in
// [dev] UDP channel, each sent ONCE per process, from the sampler thread only -- the flip/register hooks
// stay record-only. Resolution is one sampler pass (~33 ms): these are "first seen by the sampler", not
// the exact instant a hook ran. Every record carries bg= so a launch that starts backgrounded is visible.
```

### plugin/source/sample_thread.c:135

In the code now:

```
// Mid-session changes (the milestones above fire once; these catch what happens after).
// More: docs/code-comments.md (plugin/source/sample_thread.c:135)
```

Original (lines 135-137):

```
// Mid-session changes (the milestones above fire once; these catch what happens after). Capped so a
// title that flaps cannot flood the dev channel. Resolution is one sampler pass, so a change that
// flips and flips back inside one pass is not seen.
```

### plugin/source/sample_thread.c:173

In the code now:

```
// sceKernelGetProcessTimeCounter[Frequency] -- same TSC-based timer already proven in this repo...
// More: docs/code-comments.md (plugin/source/sample_thread.c:173)
```

Original (lines 173-176):

```
// sceKernelGetProcessTimeCounter[Frequency] -- same TSC-based timer
// already proven in this repo (frame_logger.prx). Needed
// unconditionally now (not just under TIMING_ENABLED) since the
// config-reload check below uses real elapsed time too.
```

### plugin/source/sample_thread.c:184

In the code now:

```
// Pure observation via the confirmed sceKernelGetCurrentCpu() -- no affinity/priority call involved...
// More: docs/code-comments.md (plugin/source/sample_thread.c:184)
```

Original (lines 184-186):

```
// Pure observation via the confirmed sceKernelGetCurrentCpu() -- no
// affinity/priority call involved, just recording where the scheduler
// is actually placing this thread by default.
```

### plugin/source/sample_thread.c:196

In the code now:

```
// v2.4: throttled to roughly once a second (every ~30 iterations of this thread's own ~30Hz loop)...
// More: docs/code-comments.md (plugin/source/sample_thread.c:196)
```

Original (lines 196-203):

```
// v2.4: throttled to roughly once a second (every ~30
// iterations of this thread's own ~30Hz loop) rather than
// every iteration -- this calls into a symbol resolved by
// name against a real signature, which could only
// corroborate from a third-party reimplementation (see the
// struct's own comment above), not Sony's actual SDK or real
// hardware, so its exposure is kept low while unproven. Once
// a second is already far faster than a human needs for this.
```

### plugin/source/sample_thread.c:217

In the code now:

```
// true ("on") = foregrounded again, this plugin is driving the strip; false ("off") = backgrounded...
// More: docs/code-comments.md (plugin/source/sample_thread.c:217)
```

Original (lines 217-228):

```
// true ("on") = foregrounded again, this plugin
// is driving the strip; false ("off") =
// backgrounded, hand control back to wled-relay.
// v2.6: explicit guard here now that
// relay_send_external_source() no longer gates
// itself internally -- this transition
// (foreground/background) is completely
// independent of relaySignalEnabled, so without
// this check a user who never opted in would
// still get a real send on every suspend/resume.
// Backgrounding releases the relay only when the shell feed is not running
// (the shell feed takes the strip over from a backgrounded game).
```

### plugin/source/sample_thread.c:245

In the code now:

```
// Recompute everything derived from config that this loop otherwise only computes once at thread start...
// More: docs/code-comments.md (plugin/source/sample_thread.c:245)
```

Original (lines 245-247):

```
// Recompute everything derived from config that this
// loop otherwise only computes once at thread start --
// this IS the "live" part of live reload.
```

### plugin/source/sample_thread.c:259

In the code now:

```
// v2.4: closes the real, confirmed gap from -- suspending via the PS button no longer leaves the strip...
// More: docs/code-comments.md (plugin/source/sample_thread.c:259)
```

Original (lines 259-267):

```
// v2.4: closes the real, confirmed gap from --
// suspending via the PS button no longer leaves the strip
// frozen showing the last on-screen frame indefinitely;
// skip the capture/process/send pipeline below while
// backgrounded. Placed AFTER the reload check/smoothing
// recompute above (not before) so live config reload still
// works even while suspended, and this recovers
// immediately, with fresh settings, the instant this
// plugin is foregrounded again.
```

### plugin/source/sample_thread.c:278

In the code now:

```
// Read a slot N flips back, which the GPU has had longer to finish drawing; the slot the hook just...
// More: docs/code-comments.md (plugin/source/sample_thread.c:278)
```

Original (lines 278-293):

```
// Read a slot N flips back, which the GPU has had longer to
// finish drawing; the slot the hook just reported may be
// mid-render. Waits (see the v3.7 note below) until enough history
// is known. A blank/no-frame flip (index sentinel) keeps
// the old path (liveBufferAddr == 0 -> keepalive) instead of
// re-reading a real slot.
//
// AMBIENT_SAMPLE_LAG only selects WHICH tracked variable to read
// -- there is no way to derive "N flips back" from the current
// index alone, so each level needs hooks.c to track its own
// variable (g_prevDisplayBufferIndex for 1,
// g_prevPrevDisplayBufferIndex for 2). A value of 2 used to
// silently read the same slot as 1, since the old `>= 1` check
// is also true for 2 and nothing tracked a second step back --
// confirmed by three RDR1 captures at "lag 2" that were
// byte-identical to lag 1. See docs/debugging/sample-lag-and-boot-flash.md.
```

### plugin/source/sample_thread.c:299  (kept in code as a summary)

In the code now:

```
// Until the lag history exists, do not sample the slot the hook just reported (the GPU is about to render into it):
// that gave a bright first read and a visible flash. No frame is sampled for a few passes instead. [docs/code-comments.md]
```

Original (lines 299-309):

```
// v3.7: until the lag history exists, do NOT fall back to the
// slot the hook just reported -- that is exactly the slot the
// GPU is about to render into. Two GOWR captures (seq 0, flip
// counter 4, i.e. the first pass after game boot) showed a
// bright raw read there while all three buffers re-read black in
// the same pass; that first read also seeds smoothing with no
// prior value to fall back to, so it decayed on the strip as a
// visible flash. No frame is sampled (liveBufferAddr = 0, the
// existing keepalive path) for up to LAG_HISTORY_WAIT_PASSES
// passes; a title that really never changes slot (single
// buffer) then gets the old behaviour instead of staying dark.
```

### plugin/source/sample_thread.c:339  (kept in code as a summary)

In the code now:

```
// Never read a buffer the kernel says is unmapped or not CPU-readable (Mortal Kombat 11 crashed here);
// 0 from ambient_resolve_readable() takes the keepalive path and the strip holds its last colour. [docs/code-comments.md]
```

Original (lines 339-345):

```
// v3.9: never read a buffer the kernel says is not mapped or CPU-readable. Mortal Kombat 11
// crashed here (SIGSEGV, page not present) on its first sampling pass: its display buffers
// are GPU-only (protection 0x30). ambient_resolve_readable() asks the kernel, and for a
// GPU-only direct-memory buffer makes it CPU-readable (buffer_guard.c), returning the
// address to read and setting g_readableLimit for the pixel-read bounds checks. 0 means
// unreadable, which takes the existing keepalive path (liveBufferAddr = 0): the strip
// holds its last color instead of the game dying.
```

### plugin/source/sample_thread.c:356

In the code now:

```
// v3.1: what actually found "this title never lights up" -- see hooks.c's g_pluginVersion comment.
// More: docs/code-comments.md (plugin/source/sample_thread.c:356)
```

Original (lines 356-363):

```
// v3.1: what actually found "this title never lights up" -- see
// hooks.c's g_pluginVersion comment. Sent UNCONDITIONALLY, not
// just from inside the liveBufferAddr!=0 branch below -- the
// whole point is to keep reporting the true hook-call-count
// state even when the pipeline is going silent, same reasoning
// as send_timing_packet's own always-on placement. Throttled to
// roughly once a second, same ~30-iteration pattern already
// used for the sceSystemServiceGetStatus poll above.
```

### plugin/source/sample_thread.c:403

In the code now:

```
// v2.9: throttled internally (autoLetterboxCheckIntervalFrames) and a complete no-op when...
// More: docs/code-comments.md (plugin/source/sample_thread.c:403)
```

Original (lines 403-407):

```
// v2.9: throttled internally (autoLetterboxCheckIntervalFrames)
// and a complete no-op when autoLetterboxEnabled is false --
// see letterbox.c. Runs before this pass's own zone sampling
// below so that a border committed just now is reflected in
// THIS pass's g_zoneX/g_zoneY, not the next one.
```

### plugin/source/sample_thread.c:412

In the code now:

```
// FLK1 flicker/flash probe (v3.3): raw pipeline read for the 6 fixed probe zones, captured here...
// More: docs/code-comments.md (plugin/source/sample_thread.c:412)
```

Original (lines 412-416):

```
// FLK1 flicker/flash probe (v3.3): raw pipeline read for the 6
// fixed probe zones, captured here (before color processing)
// so the debug packet below can show what the pipeline itself
// saw, same as tools/flicker_capture.py expects. Trivial cost:
// 6 comparisons per zone, compiled out entirely in release.
```

### plugin/source/sample_thread.c:421

In the code now:

```
// Pass 1: sample every zone from the chosen buffer.
// More: docs/code-comments.md (plugin/source/sample_thread.c:421)
```

Original (lines 421-423):

```
// Pass 1: sample every zone from the chosen buffer. Nothing
// stateful (color processing, dark threshold) runs yet, so a
// read that later gets rejected leaves no trace.
```

### plugin/source/sample_thread.c:446

In the code now:

```
// Pass 2: only zones that moved a lot are re-read, from the same buffer, now that the rest of the pass...
// More: docs/code-comments.md (plugin/source/sample_thread.c:446)
```

Original (lines 446-450):

```
// Pass 2: only zones that moved a lot are re-read, from the
// same buffer, now that the rest of the pass has elapsed. Two
// reads of one buffer that disagree mean it was mid-write.
// (When a whole scene really changes every zone is re-read
// once -- a stable buffer just agrees with itself.)
```

### plugin/source/sample_thread.c:478

In the code now:

```
// Pass 3: color processing, smoothing and output on the verified value.
// More: docs/code-comments.md (plugin/source/sample_thread.c:478)
```

Original (lines 478-481):

```
// Pass 3: color processing, smoothing and output on the
// verified value. Color processing/dark-threshold hysteresis
// therefore runs exactly once per zone per pass, on a value
// that was actually accepted.
```

### plugin/source/sample_thread.c:506

In the code now:

```
// FLK1 probe (v3.3), decoded by tools/flicker_capture.py.
// More: docs/code-comments.md (plugin/source/sample_thread.c:506)
```

Original (lines 506-518):

```
// FLK1 probe (v3.3), decoded by tools/flicker_capture.py.
// Exists to answer "is this a slot-timing problem, and did
// the AMBIENT_SAMPLE_LAG fix close it" with a capture, not a
// guess: for 6 zones spread around the strip, sends the
// color actually output, the pipeline's raw read (from
// liveBufferAddr -- i.e. AFTER the lag-1 buffer selection
// above), AND the same zone re-read directly from buffer
// slots 0, 1 and 2 right now. If the slot this pass used
// (curIdx, adjusted by AMBIENT_SAMPLE_LAG) still disagrees
// with its own re-read, or a bright raw value has no
// matching bright slot anywhere, the fix did not close it.
// Layout matches flicker_capture.py's HDR_FMT/ZONE_FMT
// exactly; see that file's top comment for the byte map.
```

### plugin/source/sample_thread.c:543

In the code now:

```
// curIdx: the RAW slot the flip hook reported (displayBufferIndex), not the one this pass actually...
// More: docs/code-comments.md (plugin/source/sample_thread.c:543)
```

Original (lines 543-553):

```
// curIdx: the RAW slot the flip hook reported (displayBufferIndex), not the
// one this pass actually sampled. tools/flicker_capture.py's ring-lag math
// is built around that: it walks the ring forward from curIdx to find which
// lag is safe, and separately checks which lag the pipeline's own raw read
// (probeRaw, below) actually matches -- so it can tell "AMBIENT_SAMPLE_LAG is
// active and reading a clean slot" apart from "still reading the risky one",
// exactly the same way it already does for the AC3 menu capture. Recovering
// curIdx from liveBufferAddr instead (i.e. reporting the already-safe slot as
// if it were the raw one) would make every capture with the fix built in look
// identical to lag 0 always being clean, and silently drop the comparison this
// probe exists to make.
```

### plugin/source/sample_thread.c:607

In the code now:

```
// v2.2.4: verified behind __FINAL__==0 (make DEBUG=1), same idiom frame_logger/force_1080p_display...
// More: docs/code-comments.md (plugin/source/sample_thread.c:607)
```

Original (lines 607-648):

```
// v2.2.4: verified behind __FINAL__==0 (make DEBUG=1), same
// idiom frame_logger/force_1080p_display already use in
// this repo. As written through v2.2.3, this ran
// unconditionally on every real release build for every
// end user: 3 extra sampleZoneAverage calls plus a UDP
// send, on an ongoing basis for as long as HDR stayed
// active -- real, avoidable CPU and network cost that
// was still present in the exact build meant to REDUCE
// CPU cost after the WLED-timeout root cause was found.
// It has already served its purpose
// (it's what let root cause get found at all) and has
// no reason to ship active in a release build.
//
// v2.2.2: DIAGNOSTIC ONLY, no color-pipeline change --
// the format-diagnostic in v2.2.1 confirmed this game's
// HDR mode correctly registers as A2R10G10B10_BT2020_PQ
// (recognized=1), and the PQ decode math itself was
// hand-verified against neutral AND slightly-imbalanced
// near-black input with no red bias found either way.
// But the user's own physical observation is that MUCH
// of the strip goes red, not just the zone nearest a
// hypothetical red UI element -- which neither of those
// findings explains, and rules out "it's just real
// content in one corner". This sends the RAW pre-decode
// 32-bit pixel word for 3 zones spread across the strip
// (index 0, middle, last) alongside the already-decoded
// 8-bit RGB pipeline output, so the raw captured value
// itself -- not just the decoded interpretation of it --
// can be checked.
// Throttled to roughly every 3rd time this block runs
// (NOT scaled to update_frequency_hz=30 like the first
// attempt at this was) -- the color packets in the
// user's own capture arrive roughly once per SECOND,
// not 30x/sec, so gating on a 30-iteration threshold
// meant this would take ~30 seconds of real time to
// fire even once, and never showed up in a ~22-second
// capture. Whatever's behind that slower real cadence
// (dedup on unchanged color, or the loop genuinely not
// running at its configured rate) is a separate
// question from the one this packet exists to answer,
// so this just fires on a small fixed count instead of
// trying to match the real cadence.
```

### plugin/source/sample_thread.c:652

In the code now:

```
// v2.2.3: entry grew from 12 to 16 bytes -- ADDS the TRUE zone average (calling the real...
// More: docs/code-comments.md (plugin/source/sample_thread.c:652)
```

Original (lines 652-665):

```
// v2.2.3: entry grew from 12 to 16 bytes -- ADDS the
// TRUE zone average (calling the real
// sampleZoneAverage, the exact same function/inputs
// the real color pipeline uses) alongside the single-
// center-pixel peek v2.2.2 already had. This is not
// a guess or a proxy: it's the literal value that
// gets handed to applyColorProcessing for these
// zones. Needed because hand-modeling a uniform raw
// input through gamma=2.4/saturation=175/black_level=
// 17 shows NO input value reproduces "default clean,
// tuned strongly red" -- gamma/saturation/levels are
// all zero-preserving and gamma=2.4 crushes small
// values further, not less, so guessing at the real
// average's magnitude isn't getting this further.
```

### plugin/source/sample_thread.c:668

In the code now:

```
// once here so memcpy below gets a plain uint32_t*, not a volatile-qualified one (was a harmless but...
// More: docs/code-comments.md (plugin/source/sample_thread.c:668)
```

Original (lines 668-673):

```
// once here so memcpy below
// gets a plain uint32_t*, not
// a volatile-qualified one
// (was a harmless but real
// -Wincompatible-pointer-types
// -discards-qualifiers warning)
```

### plugin/source/sample_thread.c:697

In the code now:

```
// v2.8: unpack == NULL -- format went unknown between the outer check and here.
// More: docs/code-comments.md (plugin/source/sample_thread.c:697)
```

Original (lines 697-700):

```
// v2.8: unpack == NULL -- format went unknown between the
// outer check and here. Still no guessing at a color;
// just keep WLED's realtime override alive with the last
// verified one instead of going silent.
```

### plugin/source/sample_thread.c:704

In the code now:

```
// v2.8: liveBufferAddr == 0 or g_haveValidFormat == 0.
// More: docs/code-comments.md (plugin/source/sample_thread.c:704)
```

Original (lines 704-707):

```
// v2.8: liveBufferAddr == 0 or g_haveValidFormat == 0. Same
// reasoning as above -- we're foregrounded (the
// g_isBackgrounded branch already `continue`d otherwise) but
// have no valid frame to sample this tick.
```

### plugin/source/sample_thread.c:710  (kept in code as a summary)

In the code now:

```
// With an unknown format, send nothing new rather than guess (the original wrong-colour bug); the heartbeat only
// repeats an already verified colour so WLED's realtime timeout does not fire. [docs/code-comments.md]
```

Original (lines 710-718):

```
// If g_haveValidFormat is 0 (unknown/unconfirmed format), we
// deliberately send nothing NEW rather than guess -- this is the
// direct fix for how the original bug happened in the
// first place: an unverified format assumption silently
// producing wrong color instead of visibly doing nothing. The
// heartbeat above only ever repeats a color the real pipeline
// already verified, on the cases above where new/unknown/no-frame
// states would otherwise mean total silence long enough to trip
// WLED's own realtime-timeout fallback (see v2.8 changelog entry).
```

### plugin/source/sample_thread.c:721

In the code now:

```
// Measured window intentionally covers the buffer-resolve + sampling + color processing + UDP send...
// More: docs/code-comments.md (plugin/source/sample_thread.c:721)
```

Original (lines 721-726):

```
// Measured window intentionally covers the buffer-resolve +
// sampling + color processing + UDP send above -- i.e.
// everything this thread does per iteration except the sleep
// itself. That's the number / actually cares about: is
// the real work fast enough to comfortably fit inside
// sampleIntervalUs, not how precisely usleep() is honored.
```

## plugin/source/settings.c

### plugin/source/settings.c:1

In the code now:

```
// settings.c -- part of ps4_ambient_light, split out of the original single-file main.c. AmbientConfig...
// More: docs/code-comments.md (plugin/source/settings.c:1)
```

Original (lines 1-4):

```
// settings.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. AmbientConfig defaults, ini load, and live-reload change detection.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
```

### plugin/source/settings.c:27

In the code now:

```
// Defaults match v1.3's hardcoded behavior exactly -- upgrading from a build with no ini file present...
// More: docs/code-comments.md (plugin/source/settings.c:27)
```

Original (lines 27-31):

```
// Defaults match v1.3's hardcoded behavior exactly -- upgrading from a
// build with no ini file present should look identical to before,
// not silently change anything.
// The running title (e.g. "CUSA00129"), set by plugin_load before the first
// ambient_load_config(); empty if GoldHEN could not say. Only used to pick a preset.
```

### plugin/source/settings.c:35

In the code now:

```
// wledHost intentionally blank, not pre-filled with this project's own controller's IP -- a build...
// More: docs/code-comments.md (plugin/source/settings.c:35)
```

Original (lines 35-39):

```
// wledHost intentionally blank, not pre-filled with this project's
// own controller's IP -- a build anyone else uses shouldn't ship
// with someone else's network address. Blank just leaves DDP send
// targeting nothing until a real ini (or the companion app) sets
// wled_host; see AMBIENT_DEFAULT_INI below for the generated file.
```

### plugin/source/settings.c:42

In the code now:

```
// relayHost blank for the same reason as wledHost above -- also moot either way, since...
// More: docs/code-comments.md (plugin/source/settings.c:42)
```

Original (lines 42-45):

```
// relayHost blank for the same reason as wledHost above -- also
// moot either way, since relaySignalEnabled defaults to false
// below, so this address is never dialed unless a user explicitly
// opts in AND sets their own relay_host in the ini.
```

### plugin/source/settings.c:54

In the code now:

```
// v3.0: on by default -- this is now the only inset applied before sampling (see ambient_internal.h)....
// More: docs/code-comments.md (plugin/source/settings.c:54)
```

Original (lines 54-57):

```
// v3.0: on by default -- this is now the only inset applied before
// sampling (see ambient_internal.h). threshold/stability default
// values are unchanged from when this was introduced in v2.9 as an
// addition on top of manual margins.
```

### plugin/source/settings.c:80

In the code now:

```
// brand-new capability with no prior behavior to preserve, so unlike every other default above it does...
// More: docs/code-comments.md (plugin/source/settings.c:80)
```

Original (lines 80-83):

```
// brand-new capability with no prior behavior to preserve, so unlike every
// other default above it does NOT need to match "what v2.0 did" (v2.0 simply
// couldn't do this at all). Only matters if ambient_create_default_config()
// ever stops matching this value -- keep the two in sync by hand.
```

### plugin/source/settings.c:92

In the code now:

```
// What the last ambient_load_config() found, for the plugin's notifications (notices.c).
// More: docs/code-comments.md (plugin/source/settings.c:92)
```

Original (lines 92-94):

```
// What the last ambient_load_config() found, for the plugin's notifications (notices.c). Set here
// because this is where the ini is read; only the plugin reads them, and settings.c is also compiled
// into the shell feed, so nothing here may call back into plugin-only code.
```

### plugin/source/settings.c:119

In the code now:

```
// Same sceKernelOpen-not-fopen gotcha plugin_loader's own code comment flags ("Does not work, may not...
// More: docs/code-comments.md (plugin/source/settings.c:119)
```

Original (lines 119-121):

```
// Same sceKernelOpen-not-fopen gotcha plugin_loader's own code comment
// flags ("Does not work, may not have write access") -- not
// rediscovering that the hard way here.
```

### plugin/source/settings.c:163

In the code now:

```
// Matched by exact string, not parsed as a float -- deliberately, to avoid any float-parsing code path...
// More: docs/code-comments.md (plugin/source/settings.c:163)
```

Original (lines 163-167):

```
// Matched by exact string, not parsed as a float -- deliberately, to
// avoid any float-parsing code path at runtime in a build with no
// libm linked. Only these 8 values exist as precomputed LUTs anyway
// (see kGammaLuts below), so a free-form value couldn't be honored
// correctly regardless.
```

### plugin/source/settings.c:179

In the code now:

```
// True if `section` holds at least one preset-owned key, i.e. the ini really has that preset.
// More: docs/code-comments.md (plugin/source/settings.c:179)
```

Original (lines 179-181):

```
// True if `section` holds at least one preset-owned key, i.e. the ini really
// has that preset. Checked by key rather than by section name so an empty or
// stray "[preset_movie]" line doesn't count as a preset.
```

### plugin/source/settings.c:207

In the code now:

```
// default -- MUST build the identity LUTs here too, or defaults (gammaR/G/B=100, meant to be a no-op)...
// More: docs/code-comments.md (plugin/source/settings.c:207)
```

Original (lines 207-210):

```
// default -- MUST build the identity LUTs here
// too, or defaults (gammaR/G/B=100, meant to be
// a no-op) would silently crush every sample to
// black instead.
```

### plugin/source/settings.c:226

In the code now:

```
// v3.8: the settings a preset owns (sampling depth, letterbox, colour, smoothing, RGB balance) are...
// More: docs/code-comments.md (plugin/source/settings.c:226)
```

Original (lines 226-233):

```
// v3.8: the settings a preset owns (sampling depth, letterbox, colour,
// smoothing, RGB balance) are read from the chosen [preset_*] section.
// NULL means an ini from before presets, which still has them in
// [layout]/[color]/[timing]; it reads exactly as it always did. Setup
// values (network, LED layout, colour order, update rate, reload check)
// are never part of a preset and always come from their own sections.
// A key missing from the chosen section is treated like any other missing
// key here: the value it already has stands.
```

### plugin/source/settings.c:236

In the code now:

```
// g_presetOverride wins over active=/the title guess, but only if its section actually has keys -- an...
// More: docs/code-comments.md (plugin/source/settings.c:236)
```

Original (lines 236-239):

```
// g_presetOverride wins over active=/the title guess, but only if its
// section actually has keys -- an override pointing at a section the
// ini doesn't define falls through to the normal pick below rather
// than silently losing every preset-owned setting.
```

### plugin/source/settings.c:251

In the code now:

```
// v3.9: [compat] gpu_only_remap.
// More: docs/code-comments.md (plugin/source/settings.c:251)
```

Original (lines 251-255):

```
// v3.9: [compat] gpu_only_remap. OPTIONAL: default 3, not in the shipped ini, nothing needs
// to set it. 0 off, 1 second CPU_READ view only, 2 also the untyped fallback, 3 also
// sceKernelMprotect on the game's own mapping (the only method that has worked, on MK11).
// It only acts on a buffer the guard already rejected as GPU-only, so it cannot change a
// title that reads fine today. See buffer_guard.c and docs/debugging/gpu-only-buffers.md.
```

### plugin/source/settings.c:259

In the code now:

```
// v3.9.1: [compat] report_file.
// More: docs/code-comments.md (plugin/source/settings.c:259)
```

Original (lines 259-261):

```
// v3.9.1: [compat] report_file. OPTIONAL, default 0 (off), not in the shipped ini. 1 makes the
// plugin write /data/ps4-ambilight/ps4_ambient_report_<TITLEID>.txt (report.c): a tester adds it. Anything
// but 0 or 1 is ignored and the value stands, like gpu_only_remap above.
```

### plugin/source/settings.c:318

In the code now:

```
// v2.2: Android-ported per-channel color knobs (see AmbientConfig struct comment for why these use a...
// More: docs/code-comments.md (plugin/source/settings.c:318)
```

Original (lines 318-320):

```
// v2.2: Android-ported per-channel color knobs (see AmbientConfig
// struct comment for why these use a different 0-neutral-point
// convention than the fields just above).
```

### plugin/source/settings.c:337

In the code now:

```
// v2.2.5: [dev] is intentionally undocumented in the generated ini template (AMBIENT_DEFAULT_INI) and...
// More: docs/code-comments.md (plugin/source/settings.c:337)
```

Original (lines 337-346):

```
// v2.2.5: [dev] is intentionally undocumented in the generated ini
// template (AMBIENT_DEFAULT_INI) and absent from both reference
// configs in this folder -- someone has to already know to type
// "[dev]" / "dev_ip=..." / "dev_logging=true" into their own ini
// by hand. That's deliberate: see the struct comment on why this
// shouldn't be one flag away from accidentally-on for a normal
// user. Both keys are required together -- a stray dev_ip with no
// dev_logging=true does nothing, and vice versa (checked again,
// redundantly, right at the one place that actually sends
// anything -- see debug_send_raw).
```

### plugin/source/settings.c:372

In the code now:

```
// v2.1 live config reload: re-reads the ini file if its mtime changed, checked at most once every...
// More: docs/code-comments.md (plugin/source/settings.c:372)
```

Original (lines 372-420):

```
// v2.1 live config reload: re-reads the ini file if its mtime changed,
// checked at most once every configReloadCheckSeconds (0 = never,
// matches v2.0's load-once behavior). Safe without any locking because
// g_config/g_zoneX/g_zoneY/g_numZones are read AND written exclusively
// by this one thread -- plugin_load's initial ambient_load_config()/
// buildZoneGeometry() call happens before this thread is even spawned,
// and the flip hook only ever touches g_currentDisplayBufferIndex, not
// any of this state.
// v2.1.1 BUGFIX (real hardware confirmed live reload never fired --
// ): the original version used g_configLastMtime==0 as
// its own "not yet initialized" sentinel. That's a real, fatal design
// flaw independent of any PS4-specific quirk: 0 is also a value
// st_mtime could legitimately have (unpopulated/unsupported on this
// filesystem, or just a coincidence). If st_mtime is ever actually 0,
// this check would conclude "still establishing baseline" FOREVER,
// on every single call, and reload would silently never fire again --
// exactly the symptom reported. Fixed with a separate boolean flag so
// the sentinel state can never collide with a real mtime value.
//
// Also now tracks file SIZE alongside mtime and reloads if EITHER
// changed -- not because mtime is confirmed broken on this platform
// (not verified from here), but because this is cheap insurance
// against exactly that class of platform-level uncertainty, and this
// project has already spent one whole session finding out an
// assumption about low-level platform behavior was wrong.
// v2.1.4: real hardware showed st_size is just as
// broken as st_mtime on this filesystem -- stuck reporting a fixed
// wrong value (8) for the entire life of the process, never once
// reflecting the real file's actual size even while it was being
// actively edited. Both fields come off the same stat() call, so
// "st_mtime is broken, but surely st_size is fine" was never a safe
// assumption -- it just hadn't been checked directly yet. What IS
// proven reliable on this platform is a raw sceKernelOpen +
// sceKernelLseek(SEEK_END) + sceKernelClose read: that's exactly how
// the initial plugin-load config read (and the v2.1.3 content preview)
// already get real, live data, and both are confirmed correct against
// this exact path by the fact edits DO take effect after a game
// restart. So the reload check now sources its size signal from that
// same syscall path instead of patching around stat() a third time.
// Existence is still checked via stat() in ambient_file_exists() --
// that's a boolean return code, not a numeric field, and nothing here
// suggests that part is unreliable.
// v2.1.5: content hash, added because size alone can't see a
// same-length edit (confirmed on real hardware -- an RGB->RBG swap
// left cur_size/last_size both correctly reading a stable, real 2809
// but never differing, so event stayed 3/"unchanged" forever). FNV-1a
// is used purely as a cheap, dependency-free change signal, not for
// any security property -- no libm, no external library, consistent
// with the existing no-libm constraint (see the PQ/sRGB LUT work above).
```

### plugin/source/settings.c:433

In the code now:

```
// Sanity cap on the hash read, independent of the earlier no-libm constraint -- this is about not...
// More: docs/code-comments.md (plugin/source/settings.c:433)
```

Original (lines 433-437):

```
// Sanity cap on the hash read, independent of the earlier no-libm
// constraint -- this is about not handing malloc() a garbage size if
// AMBIENT_CONFIG_PATH ever resolves to something unexpected. The real
// file is a few KB at most (even a heavily-commented ini); 1 MiB is
// generous headroom, not a real expected size.
```

### plugin/source/settings.c:440

In the code now:

```
// Replaces v2.1.4's ambient_get_real_config_size: still gets size the same proven way (sceKernelOpen +...
// More: docs/code-comments.md (plugin/source/settings.c:440)
```

Original (lines 440-452):

```
// Replaces v2.1.4's ambient_get_real_config_size: still gets size the
// same proven way (sceKernelOpen + sceKernelLseek(SEEK_END)), but now
// also seeks back to the start and reads the whole file through the
// same fd to compute a content hash in the same call, rather than
// opening the file twice per check. Both outputs share one success/
// failure result since they come from the same read; on any failure
// *outSize carries the negative orbis error code (or a sentinel below)
// same as v2.1.4's function did, and *outHash is left untouched.
// v3.9.4: no longer static -- shell-feed's own reload poll (shell_cfg_poll) wants the sister's
// proven size+hash read (raw sceKernelOpen/Lseek/Read, not stat(), see the comment above
// ambient_check_config_reload for why) but needs its own cadence, logging and
// buildZoneGeometry()/g_smoothedRgbValid side effects, not ambient_check_config_reload()'s. This
// function's own body and behaviour are unchanged; only its linkage changed.
```

### plugin/source/settings.c:479

In the code now:

```
// sz fits in AMBIENT_CONFIG_MAX_HASH_READ (checked above), so this is a small, bounded allocation, not...
// More: docs/code-comments.md (plugin/source/settings.c:479)
```

Original (lines 479-481):

```
// sz fits in AMBIENT_CONFIG_MAX_HASH_READ (checked above), so this
// is a small, bounded allocation, not proportional to whatever
// AMBIENT_CONFIG_PATH happens to resolve to.
```

### plugin/source/settings.c:503

In the code now:

```
// mtime is kept in the struct/packet purely for backward-compatible layout with the v2.1.2/v2.1.3...
// More: docs/code-comments.md (plugin/source/settings.c:503)
```

Original (lines 503-510):

```
// mtime is kept in the struct/packet purely for backward-compatible
// layout with the v2.1.2/v2.1.3 captures already on file -- it is
// HARDCODED to 0 as of v2.1.4 and no longer read from stat() or used
// for change-detection at all (confirmed non-functional on this
// filesystem /). Size, sourced from
// ambient_get_real_config_size_and_hash above, is one of two change
// signals as of v2.1.5 -- see g_configLastHash below for the other,
// added because size alone missed a real same-length edit on hardware.
```

### plugin/source/settings.c:513

In the code now:

```
// v2.1.5: content hash (FNV-1a/32) of the whole file, alongside size.
// More: docs/code-comments.md (plugin/source/settings.c:513)
```

Original (lines 513-517):

```
// v2.1.5: content hash (FNV-1a/32) of the whole file, alongside size.
// Needed because a same-length edit (confirmed on hardware: RGB->RBG)
// changes the file's content without changing its byte count, which
// size alone has no way to detect. Reload now fires if EITHER size or
// hash differs from last check.
```

### plugin/source/settings.c:572  (kept in code as a summary)

In the code now:

```
// On a live relay_signal_enabled edit, send "on" or release the relay once (not while backgrounded), so the relay
// is never left waiting on a flag this plugin no longer touches. [docs/code-comments.md]
```

Original (lines 572-594):

```
// v2.4.1: relay_send_external_source(true) was previously only
// ever called once, at the very end of a successful plugin_load --
// if relay_signal_enabled was still false at that moment (e.g. the
// game was already running when the ini was edited to turn it on),
// nothing would ever re-send "on" for the rest of that session,
// even though g_config.relaySignalEnabled below picks up the new
// value correctly. Capturing the value BEFORE reload and comparing
// after closes that gap: a live false->true edit now sends "on"
// immediately (matching plugin_load's own real transition), and a
// live true->false edit sends "off" once, so wled-relay isn't left
// waiting on a flag this plugin will now never touch again.
// Skipped while backgrounded -- matches ambient_sample_thread's own
// condition for what "on" means, so this can't contradict whatever
// that loop decides once it resumes.
// v2.6: the true->false direction described above didn't actually
// work until this version -- relay_send_external_source() used to
// gate itself on g_config.relaySignalEnabled internally, which by
// this point is already false (that's what triggered this call),
// so the "off" send this call exists to make was being silently
// swallowed. That internal gate is gone now (see that function's
// own comment) -- this call site's own "only fires on a real
// transition" condition is what makes it safe for a
// never-opted-in user, not a second gate inside the function.
```

### plugin/source/settings.c:607

In the code now:

```
// Is the shell feed really running?
// More: docs/code-comments.md (plugin/source/settings.c:607)
```

Original (lines 607-612):

```
// Is the shell feed really running? True only while it keeps proving it: it rewrites
// /data/ps4-ambilight/ps4_shell_alive with a changing counter about once a second, and this reports false once that
// value has not changed for SHELL_ALIVE_STALE_MS (file missing, shell feed off, crashed, older build, or not
// installed all look the same: the value never changes). A switch-off the shell feed itself honours (the disable
// file, [shell] enabled=false) is believed immediately. Right after load there is nothing to compare yet, so it
// answers true for the first SHELL_ALIVE_STALE_MS. Stateful; call from one thread, about every 250 ms, not per frame.
```

## plugin/source/state.c

### plugin/source/state.c:1  (kept in code as a summary)

In the code now:

```
// The state file reports only the foreground bit (fg=1 after the first successful send, then follows g_isBackgrounded);
// never infer ownership from output, flips or seq. Fixed-width writes at offset 0, no O_TRUNC. [docs/code-comments.md]
```

Original (lines 1-32):

```
// state.c -- direct foreground/background state writer for the shell-feed gate.
//
// The shell feed must not infer game ownership from successful WLED transmissions,
// output counters, flip counters, sequence freshness, or sampler progress. The rule is
// deliberately simple: while this game plugin reports the title in the foreground,
// publish fg=1; when the title is backgrounded, publish fg=0.
//
// ONE exception, a launch-only latch (docs/debugging/launch-gap-test.md): fg=1 is not published
// until this process has completed its first DDP send. Before that the game is "foreground" but
// cannot drive the strip yet (launch captures: first flip hook ~3.1 s after load), and publishing
// fg=1 early made the shell feed back off into a ~3.2 s silence. The latch is read once, becomes
// true once, and is never cleared; after it, ownership follows g_isBackgrounded exactly as before
// (no per-frame or per-send inference, no heartbeat). A title that never sends (unsupported,
// unreadable buffers) stays fg=0, so the shell feed keeps the strip instead of leaving it dark.
//
// The same thread also keeps g_shellFeedAlive for network.c's startup hold: from load until the first
// send it asks ambient_shell_feed_expected() (settings.c) about every 300 ms whether the shell feed's
// heartbeat file is still changing, then stops, so normal play does no file or ini reads. It also stops
// after about one minute (SHELL_POLL_CAP_TICKS) if the title never sends, so an unsupported title does not
// open files and parse the ini for the whole session; g_shellFeedAlive then keeps its last answer -- unless
// relay_signal_enabled is on (the maintainer's own wled-relay), which keeps the check going at 1 Hz so the relay
// hand-off knows whether the shell feed is up when the game backgrounds. That is
// a read of a different file, not a write of this one; this file still has no heartbeat.
//
// This thread polls the already-maintained g_isBackgrounded flag at 10 Hz and writes
// only the initial state and actual state transitions. File I/O stays off the sampler's
// hot path. Records remain fixed-width and are written at offset zero without O_TRUNC.
//
// Limitation: if the plugin/process exits or crashes without publishing fg=0, the last
// state may remain on disk. The current reader intentionally does not use seq freshness
// as an ownership signal, so stale-state recovery must be handled separately rather than
// silently reintroducing output/heartbeat inference. This prototype does not yet hook unload.
```

### plugin/source/state.c:101

In the code now:

```
// Is the shell feed really running?
// More: docs/code-comments.md (plugin/source/state.c:101)
```

Original (lines 101-105):

```
// Is the shell feed really running? Judged from plugin load until the first send, so the answer is already
// there when the game feed's first frame arrives (network.c's startup hold reads it). About every 300 ms.
// Stops for good once the first send has gone out, and after about one minute if none ever does (an
// unsupported title), so neither normal play nor a title that never sends keeps reading files or the ini;
// g_shellFeedAlive keeps its last answer. Separate from the thread so tools/test_state.c can exercise it.
```

### plugin/source/state.c:109

In the code now:

```
// Relay hand-off (network.c, relay_release_external_source): a game that backgrounds or exits must...
// More: docs/code-comments.md (plugin/source/state.c:109)
```

Original (lines 109-111):

```
// Relay hand-off (network.c, relay_release_external_source): a game that backgrounds or exits must know
// whether the shell feed is running right now, so with relay_signal_enabled on the check carries on after
// the startup window, once a second. Off by default, so for everyone else this still stops as before.
```

## plugin/source/tiling.c

### plugin/source/tiling.c:1

In the code now:

```
// tiling.c -- part of ps4_ambient_light, split out of the original single-file main.c....
// More: docs/code-comments.md (plugin/source/tiling.c:1)
```

Original (lines 1-4):

```
// tiling.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. Confirmed-correct BASE detile math (offset formula), no Neo branch.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
```

### plugin/source/tiling.c:21

In the code now:

```
// Detile math -- confirmed-correct BASE params only (/).
// More: docs/code-comments.md (plugin/source/tiling.c:21)
```

Original (lines 21-29):

```
// ============================================================
// Detile math -- confirmed-correct BASE params only (/).
// Identical formulas to ps4_detile_2dthin.c / detile_verify_probe,
// just without the Neo branch, which has no reason to run every frame
// now that base is the settled answer for this hardware.
// (TileParams itself is declared in ambient_internal.h -- zones.c
// needs it too, for sampleZoneAverage/detectHdr2200Format's calls
// into getTiledElementByteOffset below.)
// ============================================================
```

### plugin/source/tiling.c:35

In the code now:

```
// paddedHeight isn't in TileParams (only paddedWidth is used by the offset math itself), but it's...
// More: docs/code-comments.md (plugin/source/tiling.c:35)
```

Original (lines 35-37):

```
// paddedHeight isn't in TileParams (only paddedWidth is used by the
// offset math itself), but it's needed here for the v1.1 bounds check
// below -- 1088 for base params, per (ceil(1080/64)*64).
```

## plugin/source/zones.c

### plugin/source/zones.c:1

In the code now:

```
// zones.c -- part of ps4_ambient_light, split out of the original single-file main.c. Screen-edge zone...
// More: docs/code-comments.md (plugin/source/zones.c:1)
```

Original (lines 1-4):

```
// zones.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. Screen-edge zone geometry, HDR2200 live format auto-detect, 0x88740000 8-bit check, zone sampling.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.
```

### plugin/source/zones.c:21

In the code now:

```
// Zones: screen-edge points read as a small averaged neighborhood (not a single pixel) for stability.
// More: docs/code-comments.md (plugin/source/zones.c:21)
```

Original (lines 21-34):

```
// ============================================================
// Zones: screen-edge points read as a small averaged neighborhood
// (not a single pixel) for stability. This is NOT a full-frame detile
// -- see the header note on cost.
//
// v2.0: per-edge counts, start corner, direction, index offset, and
// per-edge capture margin are all now config-driven (g_config, see
// the settings block above) instead of fixed #defines. The traversal-
// order logic (which corner/direction maps to which edge visit order
// and per-edge direction) was verified in Python for all 8 corner x
// direction combinations -- including an exact regression check
// against v1.0-v1.3's hardcoded bottom-left/clockwise output -- before
// being ported here. See.
// ============================================================
```

### plugin/source/zones.c:39

In the code now:

```
// (MAX_TOTAL_ZONES is defined earlier, near DDP_HEADER_SIZE -- needed before wled_send_rgb_zones's...
// More: docs/code-comments.md (plugin/source/zones.c:39)
```

Original (lines 39-41):

```
// (MAX_TOTAL_ZONES is defined earlier, near DDP_HEADER_SIZE -- needed
// before wled_send_rgb_zones's first use of it via the MAX_ZONES alias.)
// g_numZones (actual, config-driven count) is always <= MAX_TOTAL_ZONES.
```

### plugin/source/zones.c:48

In the code now:

```
// v3.0: effective margin per edge = whatever letterbox.c's auto- detection currently has committed for...
// More: docs/code-comments.md (plugin/source/zones.c:48)
```

Original (lines 48-64):

```
// v3.0: effective margin per edge = whatever letterbox.c's auto-
// detection currently has committed for that edge (0 when the feature
// is off or hasn't found a bar there -- see that file). This used to
// be a manual g_config.margin* PLUS the auto value; the manual fields
// were removed once auto letterbox could fully cover what they were
// for (see CHANGELOG) -- these are kept as their own named functions,
// rather than reading g_autoLetterboxTop etc. directly below, purely
// for the safety clamp: no edge's margin can reach past the screen's
// own midpoint. That's the same limit BorderProcessor.applyKnownBorderCrop
// itself applies ("never crop more than half of either axis") -- here
// it also protects the unsigned subtraction below (SCREEN_HEIGHT - 1 -
// marginTop - marginBottom, etc.) from underflowing, which in
// principle a bad detection on two opposite edges could still cause
// without it. Detection itself is now bounded far tighter than this
// (MAX_BAR_DEPTH_V/H in letterbox.c, screen/6), so in practice this
// clamp is only a backstop against a bad value, not a limit that
// detection ever gets close to.
```

### plugin/source/zones.c:82

In the code now:

```
// Generates `count` points along one screen edge, in either its canonical clockwise-from-bottom-left...
// More: docs/code-comments.md (plugin/source/zones.c:82)
```

Original (lines 82-106):

```
// Generates `count` points along one screen edge, in either its
// canonical clockwise-from-bottom-left direction (reversed=false) or
// the opposite direction (reversed=true), honoring capture margins.
// Canonical directions: LEFT bottom->top, TOP left->right,
// RIGHT top->bottom, BOTTOM right->left -- matches v1.0-v1.3's
// original (now-default) bottom-left/clockwise behavior exactly.
//
// v2.7.4: denominator changed from (count-1) to count -- the old
// inclusive-both-endpoints spacing put a point at t=0 (this edge's
// start corner) AND a point at t=count-1 landing exactly on the FAR
// corner, which the next edge's own t=0 point also lands on (its own
// start corner is the same physical point). Every one of the 4 screen
// corners therefore got two zones reading the identical pixel --
// confirmed on real hardware: zone 0 and zone (numZones-1) sent
// bit-identical raw pixel values on every single sampled frame,
// exactly matching this math for a bottom-left-start clockwise loop
// (zone 0 and the last zone both land on the bottom-left corner).
// Dividing by `count` instead spaces `count` points evenly across
// [0, edge_length) -- t=0 still lands exactly on the start corner,
// but the last point (t=count-1) now falls one step short of the far
// corner, leaving that corner to be owned solely by the next edge's
// own t=0 point. No more double-sampled corners, and no more (count >
// 1 ? ... : 0) special case needed -- count is always >= 1 here (the
// loop above never runs this function body for count == 0), so
// dividing by count alone is always safe.
```

### plugin/source/zones.c:166

In the code now:

```
// CCW = reverse of the CW-for-this-corner visit order, with every edge's own direction also reversed.
// More: docs/code-comments.md (plugin/source/zones.c:166)
```

Original (lines 166-168):

```
// CCW = reverse of the CW-for-this-corner visit order, with
// every edge's own direction also reversed. Verified in
// Python against all 4 corners before porting.
```

### plugin/source/zones.c:178

In the code now:

```
// led_offset: rotate which physical LED index 0 lands on, without changing the shape/order of the...
// More: docs/code-comments.md (plugin/source/zones.c:178)
```

Original (lines 178-182):

```
// led_offset: rotate which physical LED index 0 lands on, without
// changing the shape/order of the sequence itself. Done as a
// second pass (rotate the filled arrays) rather than folded into
// the generation loop above, so this stays a single, independently
// understandable step.
```

### plugin/source/zones.c:197

In the code now:

```
// v3.6: per-pass alpha byte check for 0x80002200, tried before the smoothness detector below.
// More: docs/code-comments.md (plugin/source/zones.c:197)
```

Original (lines 197-209):

```
// v3.6: per-pass alpha byte check for 0x80002200, tried before the smoothness
// detector below. HITMAN 3 and RDR2 register this ID whether HDR is on or off,
// and the smoothness detector took 53 s to flip on a real HDR-on capture
// (about 46 s of dark frames under its noise floor, then 4 checks 2.2 s
// apart), with the strip decoded as SDR the whole time. The words were PQ from
// the first frame. Vote logic and the numbers behind it are in hdr2200_vote.h
// and docs/debugging/hdr2200-alpha-detection.md.
//
// Same idea as detectPq8bitMisregistration() below, with the sides swapped:
// here 0xff in the alpha byte means SDR and anything that looks like 0b11xxxxxx
// without 0xff means PQ. Returns 1 if the vote was decisive, and then the mode
// is already set for THIS frame and the smoothness detector is skipped. 0 means
// hold, and the caller falls through to it.
```

### plugin/source/zones.c:230

In the code now:

```
// Diagnostic-only, same debug_send_raw idiom as the PQ8C packet below.
// More: docs/code-comments.md (plugin/source/zones.c:230)
```

Original (lines 230-240):

```
// Diagnostic-only, same debug_send_raw idiom as the PQ8C packet below. Sent
// when the mode changes and otherwise about once every 30 checks.
//   [0:4]   "HDRV"
//   [4]     vote, int8: +1 PQ / -1 SDR / 0 hold
//   [5]     words read
//   [6]     of those, how many had the PQ signature (top bits 11, top byte not 0xff)
//   [7]     mode after this check (1 = PQ)
//   [8:40]  the 8 words, uint32 LE (unread slots are 0)
//   [40:44] g_numZones
//   [44:48] low 32 bits of bufferAddr
// Same 48 byte payload as PQ8C, told apart by the magic.
```

### plugin/source/zones.c:265

In the code now:

```
// See the big comment above g_hdr2200IsHdr for the full rationale.
// More: docs/code-comments.md (plugin/source/zones.c:265)
```

Original (lines 265-272):

```
// See the big comment above g_hdr2200IsHdr for the full rationale.
// Cheap by construction: at most HDR2200_DETECT_SAMPLES extra 4-byte
// reads (8, capped regardless of how many zones are configured), only
// every HDR2200_DETECT_INTERVAL sampling passes, and only while the
// active format is the ambiguous 0x80002200 -- for every other format
// (i.e. every other title) this function is never called at all.
// v3.6: the per-pass alpha byte check above runs first, and this smoothness
// check only gets a say on the frames where that one holds.
```

### plugin/source/zones.c:323

In the code now:

```
// Diagnostic-only telemetry, same debug_send_raw/[dev]-ini idiom as the format-change packet above.
// More: docs/code-comments.md (plugin/source/zones.c:323)
```

Original (lines 323-339):

```
// Diagnostic-only telemetry, same debug_send_raw/[dev]-ini idiom as
// the format-change packet above. This whole detection mechanism
// was validated offline (a Python replica of this exact algorithm,
// run against real captures, converged cleanly and plausibly), but
// live behavior didn't settle -- meaning something between "the
// algorithm is sound" and "the strip shows the right color" isn't
// visible from here. Reports every actual check (already throttled
// to once per HDR2200_DETECT_INTERVAL, so not spammy), not just on
// a decision change, so the live sdrTv/hdrTv/streak numbers can be
// compared directly against decode_verification_dump.py's own
// smoothness-heuristic output for the same real capture.
//   [0:4]  sdrTv        -- int32 LE
//   [4:8]  hdrTv        -- int32 LE
//   [8:12] streak       -- int32 LE (post-update, pre-reset)
//   [12:16] isHdr       -- uint32 LE, 0/1, current live decision
// 16-byte length is distinct from the existing 8-byte format-change
// packet, matching this project's own dispatch-by-length convention.
```

### plugin/source/zones.c:353

In the code now:

```
// v3.5: YouTube (the PS4 app) registers 0x88740000 once HDR is on, but while it plays SDR video, or...
// More: docs/code-comments.md (plugin/source/zones.c:353)
```

Original (lines 353-370):

```
// v3.5: YouTube (the PS4 app) registers 0x88740000 once HDR is on, but while
// it plays SDR video, or shows its own UI, the buffer holds plain 8-bit
// A8R8G8B8. It goes back to real 10-bit PQ for HDR video, and it never
// re-registers in between, so the format ID can't be trusted here. The PQ
// decode reads 0xffRRGGBB as R10 >= 1008 (over 8,700 nits) and clips, which
// is where the red/yellow/magenta came from.
//
// The tell is the alpha byte: a real 10-bit word has alpha in bits 31:30, so
// 0xff there would mean red at 8,700 nits or more. 8-bit content has it on
// every pixel. Vote logic and the numbers behind it are in pq8bit_vote.h and
// docs/debugging/youtube-hdr-8bit.md.
//
// Runs on EVERY pass, unthrottled, and before getUnpackFnForFormat() picks
// the decode function, on the same liveBufferAddr the zone loop reads next,
// so the mode is right for the frame it applies to. The first version waited
// for 3 agreeing checks 10 passes apart, which took about 0.75 s to switch
// and showed wrong colors the whole time, and there was no reason for it: it
// is 8 four byte reads, nothing next to the zone loop.
```

### plugin/source/zones.c:390

In the code now:

```
// Diagnostic-only, same debug_send_raw idiom as above.
// More: docs/code-comments.md (plugin/source/zones.c:390)
```

Original (lines 390-401):

```
// Diagnostic-only, same debug_send_raw idiom as above. Sent when the mode
// changes and otherwise about once every 30 checks (a check per pass
// would be ~30 packets a second for nothing).
//   [0:4]   "PQ8C"
//   [4]     vote, int8: +1 / -1 / 0
//   [5]     words read
//   [6]     of those, how many had alpha byte 0xff
//   [7]     mode after this check
//   [8:40]  the 8 words, uint32 LE (unread slots are 0)
//   [40:44] g_numZones
//   [44:48] low 32 bits of bufferAddr
// 48 byte payload (58 on the wire) is a length nothing else uses.
```

### plugin/source/zones.c:425

In the code now:

```
// v2.0: sample radius is g_config.scanDepth (was the fixed ZONE_SAMPLE_RADIUS #define). int32_t cast...
// More: docs/code-comments.md (plugin/source/zones.c:425)
```

Original (lines 425-427):

```
// v2.0: sample radius is g_config.scanDepth (was the fixed
// ZONE_SAMPLE_RADIUS #define). int32_t cast since scanDepth is
// unsigned but used in a signed loop range below.
```

### plugin/source/zones.c:429

In the code now:

```
// v2.2: float accumulators, not uint32_t -- the per-sample gamma LUTs (g_gammaLutR/G/B) now return...
// More: docs/code-comments.md (plugin/source/zones.c:429)
```

Original (lines 429-435):

```
// v2.2: float accumulators, not uint32_t -- the per-sample gamma
// LUTs (g_gammaLutR/G/B) now return unclamped/unrounded float, so
// rounding each sample to a byte before summing would reintroduce
// exactly the precision loss this change is meant to avoid. This
// only touches (2*radius+1)^2 samples per zone -- basic float
// add/multiply, no libm -- so it's the same cost class as the
// uint32_t version it replaces.
```

### plugin/source/zones.c:444  (kept in code as a summary)

In the code now:

```
// Keep the bounds check before touching real memory: an out-of-range tiled offset has crashed a title before.
// More: docs/code-comments.md (plugin/source/zones.c:444)
```

Original (lines 444-449):

```
// v1.1: bounds-check before touching real memory. A correct
// offset formula should never produce something out of
// range for a valid (sx,sy), but this project has already
// paid for bad-memory-access crashes once --
// cheap insurance against a future edge case or a param
// typo, not a sign anything is currently wrong.
```

### plugin/source/zones.c:455

In the code now:

```
// v2.2: per-channel gamma (gamma_r/g/b) applied to THIS sample now, before it's folded into the zone...
// More: docs/code-comments.md (plugin/source/zones.c:455)
```

Original (lines 455-459):

```
// v2.2: per-channel gamma (gamma_r/g/b) applied to THIS
// sample now, before it's folded into the zone sum --
// see the big comment above applyColorProcessing for why
// this one specific step moved here instead of staying in
// the post-average pipeline like everything else.
```

### plugin/source/zones.c:468

In the code now:

```
// This IS the one deliberate round-to-byte boundary in the whole v2.2 pipeline before the final...
// More: docs/code-comments.md (plugin/source/zones.c:468)
```

Original (lines 468-474):

```
// This IS the one deliberate round-to-byte boundary in the whole
// v2.2 pipeline before the final output: the legacy post-average
// stages (applyColorProcessing) take uint8_t in/out to stay a
// drop-in match for the pre-v2.2 function signature and call
// site. Everything from here through applyColorProcessing's
// internals is unclamped int32_t, with only ONE more round+clamp
// at the very end -- see that function.
```

## shell-feed/Makefile

### shell-feed/Makefile:1

In the code now:

```
# Library metadata. Stage C (see shell-feed/README.md and docs/architecture.md): this module's own...
# More: docs/code-comments.md (shell-feed/Makefile:1)
```

Original (lines 1-16):

```
# Library metadata.
#
# Stage C (see shell-feed/README.md and docs/architecture.md): this module's
# own source (shell-feed/source/*.c) is compiled alongside a second source
# list -- the sister's real tiling.c, pixel_formats.c, zones.c, gamma.c,
# color_processing.c, settings.c and config.c, compiled straight from
# ../plugin/source/, not copied -- so SHAREDFILES below. Otherwise this
# follows plugin/Makefile's pattern exactly: same toolchain vars, same
# flags, same link step. This module's own include dir and ../plugin's are
# both on the include path, since shell-feed/include/shell_internal.h (included by every
# shell-feed/source/*.c) pulls in the sister's own ambient_internal.h/config.h/preset_select.h.
#
# NAMING: every file in shell-feed/source/ is shell_*.c (or main.c). The sister files above are
# compiled into the same $(INTDIR), so a shell-feed source named tiling.c, zones.c, settings.c,
# config.c, gamma.c, pixel_formats.c or color_processing.c would write the same .o and one would
# silently replace the other.
```

### shell-feed/Makefile:39

In the code now:

```
# O_FLAG intentionally left undefined by default, same reasoning as plugin/Makefile: this module's...
# More: docs/code-comments.md (shell-feed/Makefile:39)
```

Original (lines 39-43):

```
# O_FLAG intentionally left undefined by default, same reasoning as
# plugin/Makefile: this module's kernel hooks and shared state with the
# sister plugin have only been verified crash-free at the compiler's
# default optimization level. Override explicitly if you want one, e.g.
# `make O_FLAG=-O2`, at your own risk.
```

### shell-feed/Makefile:56

In the code now:

```
# Stage C: the sister's real colour pipeline, compiled from ../plugin/source, not copied.
# More: docs/code-comments.md (shell-feed/Makefile:56)
```

Original (lines 56-59):

```
# Stage C: the sister's real colour pipeline, compiled from ../plugin/source, not copied. See the
# comment above and shell-feed/docs/architecture.md. Their .o files land in this module's own
# $(INTDIR) (same as shell-feed/source/*.c's), which is why the link step's `$(INTDIR)/*.o` glob
# already picks them up with no other change needed below.
```

## shell-feed/include/shell_internal.h

### shell-feed/include/shell_internal.h:1

In the code now:

```
// shell_internal.h -- what the shell feed's source files share.
// More: docs/code-comments.md (shell-feed/include/shell_internal.h:1)
```

Original (lines 1-22):

```
// shell_internal.h -- what the shell feed's source files share. The plugin's counterpart is
// plugin/include/ambient_internal.h.
//
// File map (shell-feed/source/):
//   main.c            module_start / module_stop only
//   shell_modules.c   which modules are loaded, and where their symbols are
//   shell_hooks.c     the RegisterBuffers / SubmitFlip hooks, installing them, the positive-control calls
//   shell_buffers.c   memory-map scan, display-buffer matching, TSC calibration, self-heal
//   shell_config.c    the ini as the shell reads it: load, live reload, the [shell] enabled= poll
//   shell_frame.c     one frame: sample the zones, colour chain, smoothing
//   shell_state.c     the game plugin's foreground bit (read) and the shell's heartbeat file (written)
//   shell_relay.c     the wled-relay on/off signal (personal, opt-in), combining this feed with the game's claim
//   shell_log.c       log lines, the [dev] UDP mirror, console notifications
//   shell_report.c    the compile-time gates, the report file and its thread
//   shell_live.c      live_send(): the send loop
//   shell_thread.c    probe_thread(): the start-up sequence and the standby waits
//   shell_shim.c      the symbols the shared plugin files expect from files this module does not link
//   shell_probe17.c   the probe's original v17 sampler; only build_points() still runs
//
// Every file here starts with shell_ on purpose. The Makefile compiles the plugin's own tiling.c,
// zones.c, settings.c, config.c ... into this module's build/ folder as well, and two sources with the
// same name would write the same .o.
```

### shell-feed/include/shell_internal.h:113

In the code now:

```
// [v21] This probe's zone order is top L->R (73), right T->B (41), bottom R->L (73), left B->T (42)...
// More: docs/code-comments.md (shell-feed/include/shell_internal.h:113)
```

Original (lines 113-117):

```
// [v21] This probe's zone order is top L->R (73), right T->B (41), bottom R->L (73), left B->T (42),
// zone 0 at the top-left corner. The sister plugin's order for the person's ini (bottom_left,
// clockwise, offset 0) starts at the bottom-left, 42 LEDs (one left edge) earlier, so zone z
// belongs at LED (z + LED_ROT) mod NZ. Verified on a PC: docs/2026-10-05_zone_order_check.md.
// Hard-coded for that ini; an ini-driven layout comes later.
```

### shell-feed/include/shell_internal.h:159

In the code now:

```
// Shared between the shell feed's own files.
// More: docs/code-comments.md (shell-feed/include/shell_internal.h:159)
```

Original (lines 159-162):

```
// ---------------------------------------------------------------------------------------------------
// Shared between the shell feed's own files. Hidden: nothing below is exported from the module, the
// same as when all of it was `static` in one main.c.
// ---------------------------------------------------------------------------------------------------
```

## shell-feed/source/main.c

### shell-feed/source/main.c:1

In the code now:

```
// shell_probe v24: v23 plus the four Phase 3 leftovers (docs/integration_plan_v2.md): report file...
// More: docs/code-comments.md (shell-feed/source/main.c:1)
```

Original (lines 1-81):

```
// shell_probe v24: v23 plus the four Phase 3 leftovers (docs/integration_plan_v2.md): report file, [dev] UDP
// mirror, self-heal, one log line per gate. Nothing about v23's behaviour changes; see this overlay's README.
//  1. REPORT FILE. [compat] report_file=1 -> /data/ps4-ambilight/ps4_ambient_report_SHELL.txt, written by its own thread on
//     start, on a change, and every 60 s. No WLED address, no ini values. Last line "end of report".
//  2. [dev] MIRROR. [dev] dev_ip + dev_logging=true (both, like the sister) -> the 5 s status row and the stop,
//     standby, resume, claim and self-heal events go to dev_ip:4048 as DDP-wrapped text starting "SHL1".
//  3. SELF-HEAL. 30 bad buffer checks in a row (~1 s) -> map_scan(0); find_bufs();. At most one re-scan per
//     10 s, doubling to 60 s while re-scans do not help.
//  4. GATES. One "GATE:" line per compile-time gate at start (an undefined gate macro is now a compile error)
//     and one "GATE (runtime ...)" line whenever the ini changes report_file / [dev].
//
// ---- v23 header, unchanged ----
// shell_probe v23: runs until reboot; disable file and [shell] enabled= switch; no crash-loop guard.
//
// Basis (v22, hardware-verified, docs/2026-10-06_probe-v22_A.md + v22_B.md): ini read at start, live reload
// confirmed (two edits, order 0->1->0, strip visibly rotated). What v22 did not do: run past 300 s, or
// offer a way to silence the feed without a reboot.
//
// WHAT'S NEW (v23)
//  1. RUN UNTIL REBOOT. The 300-second LIVE_SECONDS limit is gone. The send loop runs until the process
//     exits (reboot), or until a clean stop fires (items 2-3 below). The LIVE: header says "until reboot"
//     and the LIVE done line reports elapsed seconds.
//  2. DISABLE FILE. If /data/ps4-ambilight/ps4_shell_feed_off exists (create over FTP or the app) the send loop exits
//     cleanly with "DISABLED: disable file found". Delete the file to silence the feed until next reboot.
//  3. [shell] enabled=false in the ini. Same effect as the disable file, picked up by the live-reload poll
//     with no reboot. Default: true (absent key = enabled). "DISABLED: shell disabled in ini" is logged.
//
// NO CRASH-LOOP GUARD. v21 and v22 wrote /data/shell_probe_vXX_ran on first run and refused to re-run if
// it existed; v23 drops this entirely. The guard never actually triggered during testing (v1-v22) because
// the person always rebooted between sessions -- it only would have fired on a second injection in the same
// boot without a reboot, which never happened. Recovery from a crash loop is the same as any other plugin:
// remove the SPRX or disable AutoRun, exactly as a person would do for any bad GoldHEN plugin. The manual
// delete-to-override step was complexity that worked against the automation goal.
//
// v22 note (kept for context): the shell feed runs on YOUR ini -- the same settings, the same zone geometry,
// the same colour chain and the same [preset_game] the PS4-Ambilight plugin uses for a game.
//
// Basis (v21-A, hardware-verified, docs/2026-10-06_probe-v21_A.md): the claim gate, the 42-LED order fix and
// the fast handoff work. What v21 did not do was follow the ini: its order was a hard-coded rotation, its
// sampling was its own (four points per zone in a band), and its colours were raw. v22 replaces that.
//
// WHAT'S NEW
//  1. INI. /data/ps4-ambilight/ps4_ambient_light.ini is read at start and re-checked every [timing]
//     config_reload_check_seconds (the sister's own 2 s), by size AND content hash read through raw
//     sceKernelOpen/Read (stat() is garbage on this filesystem, the sister's hardware-proven lesson). A change
//     reloads the settings, rebuilds the zone geometry and restarts smoothing, with no reboot.
//  2. PRESET. Settings that a preset owns come from [preset_game], whatever [presets] active= says, because
//     active=movie would otherwise turn the shell into "movie" too. If the ini has no game preset the same
//     fallbacks as the sister apply (the movie preset, then the old flat [layout]/[color]/[timing] keys).
//  3. GEOMETRY. buildZoneGeometry() from the sister: led_count_*, led_start_corner, led_direction and
//     led_offset decide where zone i sits and which LED it is. v21's hard-coded LED_ROT is no longer used by
//     the live send (it only remains for the old scene previews).
//  4. SAMPLING AND COLOUR. sampleZoneAverage() with scan_depth, the per-channel gamma, then
//     applyColorProcessing() (gamma -> brightness -> contrast -> saturation -> levels -> colour order),
//     applyDarkThreshold() and the sister's smoothing (settling_time_ms). update_frequency_hz sets the
//     pace, wled_host/wled_port the destination (the compile-time host is only a fallback).
//
// HOW THE SISTER'S CODE GOT IN. Through v24 (shell_probe's own history, as a research-repo probe) it was
// pasted, not rewritten: every region between a "sister-begin" and a "sister-end" marker was cut out of
// the sister's own sources by function name (PS4-Ambilight 89199fe) by tools/make_probe_v22.py, and
// tools/host_ini_check/run.sh cut the same regions out of that file and compared them to the sister's
// files byte for byte. As of the PS4-Ambilight/ps4-shell-capture-research merge's Stage C, this plugin
// (now living in the sister's own repo, as shell-feed/) instead compiles tiling.c, pixel_formats.c,
// zones.c, gamma.c, color_processing.c, settings.c and config.c straight from ../plugin/source/ -- see
// the comment above the real #include block further down for the rest of that story, and
// shell-feed/README.md for the provenance of the one piece that is still local to this file (the
// probe's own v17 tiling math). Stage D then moved the per-frame smoothing out of both this file and
// sample_thread.c into plugin/include/smoothing.h, so there is no "sister-begin" paste left here.
//
// NOT PORTED, on purpose (these exist for a game's process or a game's pixels, not the shell's):
//  - auto letterbox (the shell has no black bars; its margins stay 0), HDR/PQ format detection,
//    the sample-lag buffer choice and read-verify (v21 samples the buffer the compositor just flipped),
//  - [network] relay (the shell feed has its own opt-in relay signal, shell_relay.c), [dev] UDP logging, [compat], the report file,
//  - writing a default ini: if the file is missing v22 keeps the sister's compiled defaults and says so.
//  The values in an INI line in the log are therefore the sister's own, with the shell's choice of preset.
//
// Foreground-bit reader gate; the shell feed polls the state file at 10 Hz.
//
// ENTRY POINT: module_start.
// CRASH-LOOP GUARD: marker /data/shell_probe_v22_ran (delete over FTP to rerun).
// HOOKS STAY INSTALLED after the window and keep counting; never unhooked. Reboot to clear.
```

### shell-feed/source/main.c:83

In the code now:

```
// SOURCE LAYOUT. This file used to hold everything below; it is now split by job (map in...
// More: docs/code-comments.md (shell-feed/source/main.c:83)
```

Original (lines 83-88):

```
//
// SOURCE LAYOUT. This file used to hold everything below; it is now split by job (map in
// shell-feed/include/shell_internal.h): shell_modules.c, shell_hooks.c, shell_buffers.c, shell_config.c,
// shell_frame.c, shell_state.c, shell_log.c, shell_report.c, shell_live.c, shell_thread.c, shell_shim.c and
// the legacy shell_probe17.c. This file keeps the entry point only. The comment above is the module's
// history; names in it such as live_send() and map_scan() now live in those files.
```

### shell-feed/source/main.c:92

In the code now:

```
// [Stage D] Build identity. common/git_ver.h is written by the Makefile on every build (commit...
// More: docs/code-comments.md (shell-feed/source/main.c:92)
```

Original (lines 92-94):

```
// [Stage D] Build identity. common/git_ver.h is written by the Makefile on every build (commit, branch,
// commit count, date) and plugin_common.h includes it; plugin_common.h also defines BUILD_TYPE. The
// fallbacks only matter for a PC test whose stand-in git_ver.h carries less than the real one.
```

### shell-feed/source/main.c:117

In the code now:

```
// Entry point
// More: docs/code-comments.md (shell-feed/source/main.c:117)
```

Original (lines 117-119):

```
// ---------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------
```

## shell-feed/source/shell_buffers.c

### shell-feed/source/shell_buffers.c:1

In the code now:

```
// shell_buffers.c -- finding the compositor's display buffers: the memory-map scan, matching the two...
// More: docs/code-comments.md (shell-feed/source/shell_buffers.c:1)
```

Original (lines 1-3):

```
// shell_buffers.c -- finding the compositor's display buffers: the memory-map scan, matching the two
// 1920x1080 regions, the TSC calibration, and the self-heal that re-runs the scan after a run of bad
// buffer checks. A buffer is trusted only while its region still looks exactly as it did when it was found.
```

### shell-feed/source/shell_buffers.c:6

In the code now:

```
// [v14] Memory-map scan + optional CPU_READ probe (see header)
// More: docs/code-comments.md (shell-feed/source/shell_buffers.c:6)
```

Original (lines 6-8):

```
// =====================================================================
// [v14] Memory-map scan + optional CPU_READ probe (see header)
// =====================================================================
```

## shell-feed/source/shell_config.c

### shell-feed/source/shell_config.c:1

In the code now:

```
// shell_config.c -- the ini as the shell feed reads it.
// More: docs/code-comments.md (shell-feed/source/shell_config.c:1)
```

Original (lines 1-4):

```
// shell_config.c -- the ini as the shell feed reads it. The plugin's own ambient_load_config() (settings.c)
// does the real parsing; this file forces the game preset on top of it, reads the three keys the plugin's
// struct has no field for ([compat] report_file, [dev] dev_ip / dev_logging, [shell] enabled), and follows
// edits live.
```

### shell-feed/source/shell_config.c:32

In the code now:

```
// Reads the ini into g_config.
// More: docs/code-comments.md (shell-feed/source/shell_config.c:32)
```

Original (lines 32-40):

```
// Reads the ini into g_config. Stage C of the PS4-Ambilight/ps4-shell-capture-research merge: this now
// calls the sister's real ambient_load_config() (plugin/source/settings.c, linked, not pasted) for
// everything [compat] gpu_only_remap/report_file included, instead of re-walking the ini key by key
// by hand. The one deliberate difference from a game's own load -- [preset_game] whatever [presets]
// active= says, no media title can turn it into "movie" (the shell is not a title) -- is now expressed
// through g_presetOverride (ambient_internal.h/settings.c, added alongside this refactor) rather than
// by duplicating preset_section_pick()'s own call here.
// Returns 1 if the file was read and parsed; on 0 g_config is untouched and the caller keeps running on
// it. Like the sister: a key that is missing or out of range keeps the value g_config already has.
```

### shell-feed/source/shell_config.c:47

In the code now:

```
// [compat] report_file and [dev] dev_ip/dev_logging: shell-only copies...
// More: docs/code-comments.md (shell-feed/source/shell_config.c:47)
```

Original (lines 47-51):

```
// [compat] report_file and [dev] dev_ip/dev_logging: shell-only copies (g_shReportOn/g_shDevIp/g_shDevLog),
// separate from the fields ambient_load_config() just set on g_config itself. [shell] enabled= is a
// probe-only key the sister's struct has no field for at all. None of these three are in the sister's
// ambient_load_config(), so they still need their own, separate ini read -- the same pattern
// shell_enabled_poll() below already uses on its own cadence.
```

### shell-feed/source/shell_config.c:78

In the code now:

```
// Live reload, the sister's rule: every configReloadCheckSeconds, read the file through the raw...
// More: docs/code-comments.md (shell-feed/source/shell_config.c:78)
```

Original (lines 78-80):

```
// Live reload, the sister's rule: every configReloadCheckSeconds, read the file through the raw syscalls,
// and reload when its size OR its content hash changed. The first check only records a baseline.
// Returns 1 when settings were reloaded (the caller then recomputes its pace and destination).
```

### shell-feed/source/shell_config.c:111  (kept in code as a summary)

In the code now:

```
// [shell] enabled= is polled on its own 1 s cadence, independent of config_reload_check_seconds (0 disabled it);
// it deliberately does not reload the rest of the ini. [docs/code-comments.md]
```

Original (lines 111-120):

```
// [v46] Found in capture v23-C (2026-10-07, person-reported): with config_reload_check_seconds=0,
// shell_cfg_poll() above never runs while live_send() is sending, so [shell] enabled= could only be
// noticed at the very start of a live_send() session -- a real gap, not fixed by v45 (which only
// covers standby, where nothing is being sent and a full reload is free). The person asked for the
// kill switch to work the same way the disable file already does: checked on its own cadence,
// independent of config_reload_check_seconds, while everything else in the ini (brightness, zones,
// host, ...) keeps following config_reload_check_seconds exactly as before -- this deliberately does
// NOT call shell_cfg_load() (which would reload everything) or touch g_config, buildZoneGeometry(),
// or the gamma LUTs. Logged separately from "INI RELOAD" and does not increment g_cfgReloads, since
// nothing but the one key changed.
```

## shell-feed/source/shell_frame.c

### shell-feed/source/shell_frame.c:5

In the code now:

```
// [v22] Glue: this project's own code from here to the end of the block.
// More: docs/code-comments.md (shell-feed/source/shell_frame.c:5)
```

Original (lines 5-7):

```
// ---------------------------------------------------------------------
// [v22] Glue: this project's own code from here to the end of the block.
// ---------------------------------------------------------------------
```

### shell-feed/source/shell_frame.c:19

In the code now:

```
// One frame: every zone sampled with scan_depth, run through the sister's colour chain, dark threshold...
// More: docs/code-comments.md (shell-feed/source/shell_frame.c:19)
```

Original (lines 19-22):

```
// One frame: every zone sampled with scan_depth, run through the sister's colour chain, dark threshold and
// smoothing, written to rgbTriplets in LED order (zone i IS LED i: buildZoneGeometry() already applied the
// start corner, direction and offset). Returns the zone count. This is the sister's sample_thread.c pass 3
// without the game-process-only parts (see the header comment).
```

## shell-feed/source/shell_hooks.c

### shell-feed/source/shell_hooks.c:18

In the code now:

```
// Shared state, written by hooks, read by the reporter thread.
// More: docs/code-comments.md (shell-feed/source/shell_hooks.c:18)
```

Original (lines 18-21):

```
// ---------------------------------------------------------------------
// Shared state, written by hooks, read by the reporter thread. Plain
// volatile counters: diagnostics only, torn reads are acceptable.
// ---------------------------------------------------------------------
```

### shell-feed/source/shell_hooks.c:104  (kept in code as a summary)

In the code now:

```
// HOOK32 patches a 32-bit relative jump: refuse to patch when the hook is over 2 GB from its target,
// an unreachable hook would crash the compositor on its first call. [docs/code-comments.md]
```

Original (lines 104-107):

```
// [v10] HOOK32 patches a 32-bit relative jump, which cannot reach a hook function
// more than 2 GB from its target. v3-v8 only LOGGED that condition (it never occurred
// in the shell). In the compositor an unreachable hook would crash it on the first
// call, so v10 refuses to patch instead. Returns 1 if the hook is reachable.
```

### shell-feed/source/shell_hooks.c:119

In the code now:

```
// Installs both hooks, each with a before/after byte check, and says how many took.
// More: docs/code-comments.md (shell-feed/source/shell_hooks.c:119)
```

Original (lines 119-122):

```
// Installs both hooks, each with a before/after byte check, and says how many took. A target that is a
// forwarding stub (first byte 0xE9) or farther than 2 GB from its hook is left alone. The patched counts go
// back to module_start(), which stops if neither hook went in.
// (Moved out of module_start() unchanged; the hook functions, HOOK_INIT and HOOK32 have to share a file.)
```

### shell-feed/source/shell_hooks.c:158

In the code now:

```
// 5. Positive control: call the PATCHED entry points (raw resolved addresses, not the trampoline...
// More: docs/code-comments.md (shell-feed/source/shell_hooks.c:158)
```

Original (lines 158-160):

```
// 5. Positive control: call the PATCHED entry points (raw resolved addresses,
// not the trampoline pointers) with an invalid handle (-1). Assumed, not
// verified in the compositor: rejected at argument validation, as in the shell.
```

## shell-feed/source/shell_live.c

### shell-feed/source/shell_live.c:1

In the code now:

```
// shell_live.c -- live_send(): the send loop.
// More: docs/code-comments.md (shell-feed/source/shell_live.c:1)
```

Original (lines 1-3):

```
// shell_live.c -- live_send(): the send loop. One socket, TSC-paced at update_frequency_hz, samples the
// compositor's display buffer, sends DDP to WLED. Stops cleanly when the disable file appears or
// [shell] enabled=false, and returns why. The caller (probe_thread) decides what happens next.
```

### shell-feed/source/shell_live.c:7

In the code now:

```
// read; makes every later entry (i.e. every resume out of wait_for_reenable()) log as INI RELOAD and...
// More: docs/code-comments.md (shell-feed/source/shell_live.c:7)
```

Original (lines 7-13):

```
// read; makes every later entry (i.e. every resume out of
// wait_for_reenable()) log as INI RELOAD and count toward
// g_cfgReloads/ini_reloads, instead of logging as a fresh
// LOAD that silently didn't increment the counter. Found in
// capture v23-B (2026-10-06): the resume's own config read
// printed "INI LOAD", not "INI RELOAD", so ini_reloads under-
// counted every resume.
```

### shell-feed/source/shell_live.c:15

In the code now:

```
// [v18] Continuous live send: one socket, LIVE_HZ, TSC-paced.
// More: docs/code-comments.md (shell-feed/source/shell_live.c:15)
```

Original (lines 15-21):

```
// [v18] Continuous live send: one socket, LIVE_HZ, TSC-paced. [v23] runs until reboot or clean stop.
// [v23-fix] live_send() now returns why it stopped, so the caller can tell a user-triggered disable
// (resumable) apart from a configuration/environment problem (not retried automatically):
//   0 = never got to the loop at all (no buffers, no host configured, socket/address setup failed)
//   1 = stopped because the disable file was present
//   2 = stopped because [shell] enabled=false
// ---------------------------------------------------------------------
```

### shell-feed/source/shell_live.c:32

In the code now:

```
// [v22] The ini first: it decides the destination, the pace, the zone count and every colour setting.
// More: docs/code-comments.md (shell-feed/source/shell_live.c:32)
```

Original (lines 32-36):

```
// [v22] The ini first: it decides the destination, the pace, the zone count and every colour setting.
// A missing or unreadable file is not fatal: the sister's compiled defaults stand (WLED_HOST is empty, so with no wled_host the send loop does not start).
// [v23-fix v44] Only the very first call this process lifetime is a real LOAD; every later call is a
// resume out of wait_for_reenable() re-reading the same ini, which is a RELOAD in every sense that
// matters to a reader of the log (and should count toward g_cfgReloads, same as shell_cfg_poll()'s).
```

### shell-feed/source/shell_live.c:117

In the code now:

```
// [v22] The REST of the ini (brightness, zones, host, ...), re-checked on the sister's own schedule...
// More: docs/code-comments.md (shell-feed/source/shell_live.c:117)
```

Original (lines 117-121):

```
// [v22] The REST of the ini (brightness, zones, host, ...), re-checked on the sister's own schedule
// (config_reload_check_seconds, 2 s) -- [shell] enabled= no longer follows this cadence; see
// shell_enabled_poll() above, [v46]. Runs even
// while the sister plugin holds the claim, so an edit made during a game is already in effect afterwards.
// [v23] disable file: /data/ps4-ambilight/ps4_shell_feed_off -- create over FTP to stop cleanly.
```

### shell-feed/source/shell_live.c:126

In the code now:

```
// [v46] [shell] enabled= checked on its own 1 s cadence, independent of config_reload_check_seconds...
// More: docs/code-comments.md (shell-feed/source/shell_live.c:126)
```

Original (lines 126-128):

```
// [v46] [shell] enabled= checked on its own 1 s cadence, independent of config_reload_check_seconds
// (see shell_enabled_poll()'s comment) -- was previously "picked up by the live-reload poll above",
// which meant config_reload_check_seconds=0 silently made this unreachable until the next fresh start.
```

### shell-feed/source/shell_live.c:156

In the code now:

```
// [v20] The sister plugin's claim, checked every tick (cheap: a TSC compare unless it's actually time...
// More: docs/code-comments.md (shell-feed/source/shell_live.c:156)
```

Original (lines 156-159):

```
// [v20] The sister plugin's claim, checked every tick (cheap: a TSC compare unless it's
// actually time to poll the file). While it's fresh, this tick is entirely skipped --
// no buffer read, no send -- and counted separately from both sent and skipped (a claim
// deferral is not a failure of anything on this side).
```

## shell-feed/source/shell_log.c

### shell-feed/source/shell_log.c:5

In the code now:

```
// --- [dev] UDP mirror: the sister's telemetry idiom (network.c debug_send_raw): DDP header, port...
// More: docs/code-comments.md (shell-feed/source/shell_log.c:5)
```

Original (lines 5-7):

```
// --- [dev] UDP mirror: the sister's telemetry idiom (network.c debug_send_raw): DDP header, port 4048,
// payload capped at 192 bytes, own short-lived socket, and BOTH dev_ip and dev_logging=true required. The
// payload starts with the 4-byte magic "SHL1" so a listener can tell it from the plugin's binary packets. ---
```

### shell-feed/source/shell_log.c:62

In the code now:

```
// [v47] A line in the console's own notification bar, for the few shell-feed states a person has to...
// More: docs/code-comments.md (shell-feed/source/shell_log.c:62)
```

Original (lines 62-65):

```
// [v47] A line in the console's own notification bar, for the few shell-feed states a person has to know
// about without a monitor or the [dev] mirror. Used only where the strip stops or comes back: not per frame,
// not per re-scan (the heal cooldown alone doesn't make those worth a message), and nothing about whether
// WLED answers. [notifications] enabled=false silences all but the two "can't start: wled_host" ones.
```

## shell-feed/source/shell_modules.c

### shell-feed/source/shell_modules.c:5

In the code now:

```
// Helpers
// More: docs/code-comments.md (shell-feed/source/shell_modules.c:5)
```

Original (lines 5-7):

```
// ---------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------
```

### shell-feed/source/shell_modules.c:65

In the code now:

```
// [v8] ONE consolidated scan: walks the handle range ONCE per phase and answers every question this...
// More: docs/code-comments.md (shell-feed/source/shell_modules.c:65)
```

Original (lines 65-72):

```
// [v8] ONE consolidated scan: walks the handle range ONCE per phase and
// answers every question this probe used to run a SEPARATE full table scan
// for (Piglet, ScreenShot, VideoRecording, Mono discovery, four guessed
// Mono filenames -- up to ten full passes total in v7, HANDOFF 7.15's
// "repeated-full-scan design issue"). Still scans 0..SCAN_START_CAP
// unconditionally, then keeps going until SCAN_MISS_RUN consecutive misses
// or SCAN_HARD_CAP, exactly as v5-v7 (HANDOFF 7.9's fix, unchanged). All
// name matching is case-insensitive (fixes v7-A's missed Mono modules).
```

### shell-feed/source/shell_modules.c:116

In the code now:

```
// Prefer a "monosgen" match (the confirmed real runtime, 7.15) over any earlier non-sgen match (e.g....
// More: docs/code-comments.md (shell-feed/source/shell_modules.c:116)
```

Original (lines 116-119):

```
// Prefer a "monosgen" match (the confirmed real runtime, 7.15)
// over any earlier non-sgen match (e.g. libmono-btls-shared.sprx,
// a TLS/crypto helper, not the VM itself); otherwise keep the
// first match found.
```

## shell-feed/source/shell_probe17.c

### shell-feed/source/shell_probe17.c:1

In the code now:

```
// shell_probe17.c -- the probe's original v17 sampler: its own tiling math, a fixed 229-zone layout...
// More: docs/code-comments.md (shell-feed/source/shell_probe17.c:1)
```

Original (lines 1-7):

```
// shell_probe17.c -- the probe's original v17 sampler: its own tiling math, a fixed 229-zone layout, and the
// scene / burst / dump code built on them. LEGACY. v18 stopped calling scene_run(), and the live feed samples
// with the plugin's shared zones.c / tiling.c instead (shell_frame.c). In this build the only function in
// here that runs is build_points(), once, from probe_thread(), for its "SAMPLER ... points built" log line.
// scene_run(), run_burst(), ddp_preview() and dump_bufs() are never called (the compiler says so for
// scene_run and ddp_preview). They are kept as they were, together, so removing them later is removing
// this one file and the build_points() call.
```

### shell-feed/source/shell_probe17.c:10

In the code now:

```
// [v17] Sampler core. The block between the markers is plain C with no SDK dependency: it is compiled...
// More: docs/code-comments.md (shell-feed/source/shell_probe17.c:10)
```

Original (lines 10-15):

```
// =====================================================================
// [v17] Sampler core. The block between the markers is plain C with no SDK
// dependency: it is compiled and tested on a PC against the real v16-A dumps.
// Tiling formulas: verbatim from the sister project's plugin/source/tiling.c
// (BASE params only, paddedHeight 1088), which decoded all four v16-A buffers.
// =====================================================================
```

### shell-feed/source/shell_probe17.c:96

In the code now:

```
// Zone order: 0..72 top (left->right), 73..113 right (top->bottom), 114..186 bottom (right->left)...
// More: docs/code-comments.md (shell-feed/source/shell_probe17.c:96)
```

Original (lines 96-98):

```
// Zone order: 0..72 top (left->right), 73..113 right (top->bottom),
// 114..186 bottom (right->left), 187..228 left (bottom->top). Physical LED
// mapping (start corner / direction / offset) is the sister project's config job.
```

## shell-feed/source/shell_relay.c

### shell-feed/source/shell_relay.c:1  (kept in code as a summary)

In the code now:

```
// wled-relay hand-off, personal and opt-in (relay_signal_enabled + relay_host): "on" while the loop runs, repeated every 5 s;
// "off" on a clean stop unless a game holds fg=1, in which case the plugin sends it later. [docs/code-comments.md]
```

Original (lines 1-20):

```
// shell_relay.c -- the shell feed's half of the wled-relay hand-off. Personal, opt-in: it does nothing unless the
// ini has relay_signal_enabled=true AND a relay_host (both default to off/blank, so nobody else's console ever
// sends a relay packet from here).
//
// Who holds the strip, as the relay should see it. The relay is told "on" (something else is driving the strip,
// stand down) or "off" (nobody is, take over). Two things can drive it:
//   - the shell feed, from the moment its send loop starts until that loop stops (the heartbeat file proves this
//     to the game plugin: it is written from the same loop), and
//   - a game (or the companion app), whose fg=1 claim in ps4_ambient_state this feed already reads.
// The relay stands down while EITHER holds, so "off" goes out only when the last holder lets go:
//   - this feed starts or resumes sending                    -> "on"
//   - this feed stops cleanly (disable file, [shell] enabled=false) -> "off", unless a game claims fg=1; the
//     game then holds it, and the plugin sends the "off" itself when it backgrounds or exits, because by then it
//     sees this feed's heartbeat is gone (plugin/source/network.c, relay_release_external_source()).
//   - a game backgrounding while this feed is running            -> nothing here: the plugin sends no "off"
//     while it sees the heartbeat, so the relay stays down.
// While held, "on" is repeated every RELAY_RESEND_SEC: the relay clears its flag itself when HA turns gaming
// mode off (tv_gate.py's backstop), and a one-shot "on" would never be asserted again. The relay treats a
// repeat as a no-op.
// A crash or a power-off cannot send anything; the relay's own gaming_mode backstop has to cover that.
```

### shell-feed/source/shell_relay.c:42

In the code now:

```
// Call with shellActive=1 when the send loop starts and after every ini reload, 0 when it stops.
// More: docs/code-comments.md (shell-feed/source/shell_relay.c:42)
```

Original (lines 42-44):

```
// Call with shellActive=1 when the send loop starts and after every ini reload, 0 when it stops. Sends only on
// a change, so calling it often is fine. Reads relay_signal_enabled / relay_host / relay_port live, so editing
// them while running does the right thing (off to the old address, on to the new one).
```

## shell-feed/source/shell_report.c

### shell-feed/source/shell_report.c:1

In the code now:

```
// shell_report.c -- the compile-time gate table, and the report file (/data/ps4-ambilight/...
// More: docs/code-comments.md (shell-feed/source/shell_report.c:1)
```

Original (lines 1-3):

```
// shell_report.c -- the compile-time gate table, and the report file (/data/ps4-ambilight/
// ps4_ambient_report_SHELL.txt): its text, when it is due, and the thread that writes it. live_send()
// publishes its counters here; the report thread only reads them, so a file write never lands inside a tick.
```

### shell-feed/source/shell_report.c:152

In the code now:

```
// [v24] Report-file side: live_send() publishes its counters into these globals at each 5 s row and at...
// More: docs/code-comments.md (shell-feed/source/shell_report.c:152)
```

Original (lines 152-156):

```
// ---------------------------------------------------------------------
// [v24] Report-file side: live_send() publishes its counters into these globals at each 5 s row and at
// every state change; the report thread (its own thread, like the sister's report.c, so a file write
// never lands inside a 33 ms tick) only ever reads them.
// ---------------------------------------------------------------------
```

## shell-feed/source/shell_shim.c

### shell-feed/source/shell_shim.c:1

In the code now:

```
// shell_shim.c -- the symbols the 7 shared plugin files (tiling.c, pixel_formats.c, zones.c, gamma.c...
// More: docs/code-comments.md (shell-feed/source/shell_shim.c:1)
```

Original (lines 1-3):

```
// shell_shim.c -- the symbols the 7 shared plugin files (tiling.c, pixel_formats.c, zones.c, gamma.c,
// color_processing.c, settings.c, config.c) expect from plugin files this module does not link:
// network.c, buffer_guard.c, report.c. Each is a real value or a safe no-op here; the comment on each says why.
```

### shell-feed/source/shell_shim.c:6

In the code now:

```
// [v3.9.4 / Stage C merge] PS4-Ambilight's settings, zone geometry and colour chain -- shared code...
// More: docs/code-comments.md (shell-feed/source/shell_shim.c:6)
```

Original (lines 6-22):

```
// =====================================================================
// [v3.9.4 / Stage C merge] PS4-Ambilight's settings, zone geometry and colour chain -- shared code,
// not pasted. Through v24 (the shell-capture-research project's own history) this section was a
// byte-for-byte paste of the sister's source (PS4-Ambilight 89199fe), kept honest by
// tools/host_ini_check/run.sh diffing the two byte for byte. That stopped being possible the moment
// the sister itself changed underneath the paste (v3.9.4 added g_presetOverride to settings.c,
// specifically so this file could stop re-implementing preset selection by hand) -- a paste cannot
// both match its source and diverge from it to add new behaviour. So this merge's Stage C replaces
// the paste with the real thing: tiling.c, pixel_formats.c, zones.c, gamma.c, color_processing.c,
// settings.c and config.c are compiled from ../plugin/source/ (see shell-feed/Makefile's second
// source list) and declared through the sister's own real headers, included below. What stays
// local to this file: the probe's own v17 tiling math just above this comment (kept for the
// buffer-discovery/self-heal code that has always used it, unrelated to the zone sampler), and the
// small shim just below (11 externs the shared files need from sister files this project
// does not link -- network.c, buffer_guard.c, report.c -- each a safe default or a no-op here; see
// each one's own comment for why).
// =====================================================================
```

### shell-feed/source/shell_shim.c:24

In the code now:

```
// shim: 11 symbols the 7 shared files above reference from sister files this project doesn't link...
// More: docs/code-comments.md (shell-feed/source/shell_shim.c:24)
```

Original (lines 24-26):

```
// ---- shim: 11 symbols the 7 shared files above reference from sister files this project doesn't
// link (network.c, buffer_guard.c, report.c). Confirmed to be exactly these 11 by grepping all 7
// shared .c files for every extern ambient_internal.h declares; see shell-feed/docs/findings.md.
```

### shell-feed/source/shell_shim.c:28

In the code now:

```
// real, not a stub: the shell has no letterbox detection, so these stay 0 (zones.c's effMargin*()...
// More: docs/code-comments.md (shell-feed/source/shell_shim.c:28)
```

Original (lines 28-30):

```
// real, not a stub: the shell has no letterbox detection, so these
// stay 0 (zones.c's effMargin*() helpers), same as before this refactor --
// just no longer `static`, since zones.c now reads them as its own extern.
```

### shell-feed/source/shell_shim.c:32

In the code now:

```
// real, not a stub: sampleZoneAverage()'s bounds check (no read past one display buffer), same...
// More: docs/code-comments.md (shell-feed/source/shell_shim.c:32)
```

Original (lines 32-35):

```
// real, not a stub: sampleZoneAverage()'s bounds check (no read past
// one display buffer), same BUF_BYTES17 this probe's own buffer
// discovery already uses and already confirmed on real captures --
// just no longer `static`, since zones.c now reads it as its own extern.
```

## shell-feed/source/shell_state.c

### shell-feed/source/shell_state.c:1

In the code now:

```
// shell_state.c -- the foreground handshake with the game plugin.
// More: docs/code-comments.md (shell-feed/source/shell_state.c:1)
```

Original (lines 1-3):

```
// shell_state.c -- the foreground handshake with the game plugin. Reads the plugin's state file
// (plugin/source/state.c writes it; docs/development/shell-feed-coordination.md) and writes the shell
// feed's own heartbeat file, which the plugin reads to tell a running shell feed from a missing one.
```

### shell-feed/source/shell_state.c:6

In the code now:

```
// Heartbeat for the game plugin: a fixed-width, ever-changing counter rewritten about once a second...
// More: docs/code-comments.md (shell-feed/source/shell_state.c:6)
```

Original (lines 6-9):

```
// Heartbeat for the game plugin: a fixed-width, ever-changing counter rewritten about once a second, so the plugin can
// tell a shell feed that is really running from one that is switched off, crashed, or not installed (it sees the value
// stop changing). Deliberately the same tiny open/write/close the state file uses, without O_TRUNC, and on the same
// 1 s cadence as the ini poll next to it, so it adds nothing the loop was not already doing every second.
```

### shell-feed/source/shell_state.c:24

In the code now:

```
// [v25] Simple foreground-bit gate.
// More: docs/code-comments.md (shell-feed/source/shell_state.c:24)
```

Original (lines 24-29):

```
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
// [v25] Simple foreground-bit gate. The shell reader does not infer ownership from seq
// changes, heartbeat freshness, or WLED send activity. A readable fg=1 backs shell feed off;
// a readable fg=0 releases it. Before any valid record, allow shell feed. After a valid record,
// retain the last foreground bit on read errors rather than failing open.
```

## shell-feed/source/shell_thread.c

### shell-feed/source/shell_thread.c:1

In the code now:

```
// shell_thread.c -- probe_thread(): the start-up sequence (calibrate, read the aux ini keys, find the...
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:1)
```

Original (lines 1-3):

```
// shell_thread.c -- probe_thread(): the start-up sequence (calibrate, read the aux ini keys, find the display
// buffers) and the loop around live_send(), plus the two standby waits it uses: one for a clean stop (disable
// file / [shell] enabled=false), one for a missing or unusable wled_host.
```

### shell-feed/source/shell_thread.c:6

In the code now:

```
// Reporter thread (probe_thread) [v23] The CUE table and the timed reporter loop have been removed...
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:6)
```

Original (lines 6-11):

```
// ---------------------------------------------------------------------
// Reporter thread (probe_thread)
// [v23] The CUE table and the timed reporter loop have been removed: live_send() now runs until the
// process exits and never returns, so the post-send loop was dead code. Status rows (LIVE t=Xs ...) are
// printed by the send loop itself every 5 s and carry all the progress information that was in the CUEs.
// ---------------------------------------------------------------------
```

### shell-feed/source/shell_thread.c:40  (kept in code as a summary)

In the code now:

```
// Standby re-reads the ini every second regardless of config_reload_check_seconds: with 0 it used to deadlock
// (enabled=false could never be cleared without a reboot). [docs/code-comments.md]
```

Original (lines 40-58):

```
// [v23-fix] Entered only after a clean, user-triggered stop (live_send() returned 1 or 2), never after a
// configuration/environment failure (0). Polls for both kill-switches to clear -- the disable file gone AND
// [shell] enabled= back to true -- then returns so probe_thread() can call live_send() again. Does not touch
// hooks, map_scan() or find_bufs(): the buffers already found stay as they are, same as a running live_send()
// never re-scans them either. The disable file is checked every poll regardless of the ini's own cadence,
// since it carries no cadence of its own.
//
// [v45] The ini itself used to be re-read on the SAME cadence as live_send()'s own reload poll
// (config_reload_check_seconds) -- which meant a value of 0 (meaning "never poll" while actively
// sending, a deliberate way to skip the file-stat overhead at 30 Hz) also meant "never poll" here,
// where nothing is being sent and there is no overhead to avoid. The result, confirmed on hardware
// (HANDOFF v44): a permanent deadlock -- `[shell] enabled=false` with `config_reload_check_seconds=0`
// could not be cleared by `enabled=true`, nor by raising `config_reload_check_seconds` itself, since
// that value can only be learned by a read this never performed. Only a reboot cleared it.
//
// Fixed by giving standby its own cadence, independent of config_reload_check_seconds: the ini is now
// re-read every STANDBY_RELOAD_MS regardless of that setting. config_reload_check_seconds still governs
// how often live_send() re-reads the ini WHILE SENDING (unchanged, 0 still means "never poll" there,
// same deliberate opt-out as before) -- this only changes the no-cost, not-sending-anything wait here.
```

### shell-feed/source/shell_thread.c:61

In the code now:

```
// config_reload_check_seconds, so a value of 0 there can no longer block recovery.
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:61)
```

Original (lines 61-66):

```
// config_reload_check_seconds, so a value of 0 there can no longer
// block recovery. Deliberately == STANDBY_POLL_MS (checked every
// wake): standby already wakes once a second to poll the disable
// file, and sceKernelOpen+a config_size_and_hash stat/read is cheap
// next to that, so there is no reason to wait any longer than this
// loop already does.
```

### shell-feed/source/shell_thread.c:70

In the code now:

```
// [v47] Told once per pause (this function is entered once per clean stop, not per poll).
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:70)
```

Original (lines 70-73):

```
// [v47] Told once per pause (this function is entered once per clean stop, not per poll). The disable
// file is always worth saying: forgotten, it leaves a dark strip with no explanation. [shell] enabled=false
// is a setting the person chose, so it is only announced when the feed was already running and just got
// turned off -- not at every boot of a console where it has simply been off all along.
```

### shell-feed/source/shell_thread.c:105

In the code now:

```
// Standby for a missing or unusable wled_host.
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:105)
```

Original (lines 105-110):

```
// Standby for a missing or unusable wled_host. With no compiled-in fallback address, a first-run shell feed has
// nowhere to send; this used to stop the thread for good (a console restart to recover). Instead wait here, re-reading
// the ini once a second (the same cadence wait_for_reenable() uses), and return as soon as wled_host holds something
// other than the value that failed, so setting it in the ini or the companion app starts the feed on its own, the same
// way a destination change is already followed live once the feed is running. No heartbeat is written while waiting
// (there is no loop), so the game plugin correctly treats the shell feed as not running.
```

### shell-feed/source/shell_thread.c:150

In the code now:

```
// [v19] FIX 2: g_nBuf is only ever set by find_bufs(), which v17 reached through scene_run(1,..). v18...
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:150)
```

Original (lines 150-152):

```
// [v19] FIX 2: g_nBuf is only ever set by find_bufs(), which v17 reached through scene_run(1,..).
// v18 stopped calling scene_run, so g_nBuf stayed 0 and live_send() would have returned
// "no display buffers found". Scan and match the buffers here, then send.
```

### shell-feed/source/shell_thread.c:155

In the code now:

```
// [v23] live_send() runs until a clean stop fires (disable file, [shell] enabled=false) or a...
// More: docs/code-comments.md (shell-feed/source/shell_thread.c:155)
```

Original (lines 155-163):

```
// [v23] live_send() runs until a clean stop fires (disable file, [shell] enabled=false) or a
// configuration/environment problem stops it before it ever starts sending. The post-send reporter
// and watch loops that followed here in v22 are removed: they were dead code from v18 onward
// (live_send already ran for LIVE_SECONDS, leaving no time) and are doubly dead now.
// [v23-fix] A clean stop used to end probe_thread() for good ("Reboot to clear"), which meant
// [shell] enabled=true back in the ini, or removing the disable file, did not actually resume
// anything -- found on hardware in capture v23-A. Loop here instead: wait in wait_for_reenable()
// for both kill-switches to clear, then call live_send() again. Buffers found earlier are not
// re-scanned; a stop with reason 0 (buffers/socket) is not retried automatically (a missing or unusable host now waits in wait_for_destination()).
```

## shell-feed/tools/run_stagec_core_test.sh

### shell-feed/tools/run_stagec_core_test.sh:2

In the code now:

```
# Stage C verification (merge plan step 13): compiles the shell feed's own modules (shell_config.c...
# More: docs/code-comments.md (shell-feed/tools/run_stagec_core_test.sh:2)
```

Original (lines 2-16):

```
# Stage C verification (merge plan step 13): compiles the shell feed's own modules (shell_config.c,
# shell_frame.c, shell_probe17.c, shell_log.c, shell_shim.c, pulled in by tools/test_stagec_core.c) together
# with the sister's real, linked tiling.c/pixel_formats.c/zones.c/gamma.c/color_processing.c/settings.c/
# config.c against a real test ini, and runs it. (Before the main.c split this extracted a marked span of
# main.c; the modules are now included directly.)
#
# This is this project's own equivalent of the research repo's tools/host_zone_check and
# tools/host_ini_check for the post-Stage-C shape: those two assume a self-contained single-file
# probe (Stage A/B's shape) and can no longer run their own harness step against shell-feed's main.c
# now that it legitimately #includes the sister's real headers via a proper Makefile -- their
# provenance check (which compared the two sample_thread.c smoothing blocks, since removed by
# Stage D) has nothing left to compare. This script covers everything Stage C actually changed;
# the shared smoothing's own equivalence proof is tools/test_smoothing.c.
#
# Usage: sh shell-feed/tools/run_stagec_core_test.sh   (run from the repo root)
```

## shell-feed/tools/test_stagec_core.c

### shell-feed/tools/test_stagec_core.c:1

In the code now:

```
// Stage C verification: compiles the EXACT extracted span of shell-feed/source/main.c (v17 tiling math...
// More: docs/code-comments.md (shell-feed/tools/test_stagec_core.c:1)
```

Original (lines 1-8):

```
// Stage C verification: compiles the EXACT extracted span of shell-feed/source/main.c (v17 tiling
// math through shell_process_frame -- lines 616-952 of the real file as of this test) against the
// real sister files (plugin/source/{tiling,pixel_formats,zones,gamma,color_processing,settings,
// config}.c), the same way host_zone_check/host_ini_check already prove the sister files link
// together, but now proving THIS project's own post-refactor code links against them too and
// behaves as designed: g_presetOverride forces preset_game despite active=movie, buildZoneGeometry()
// produces the known-correct geometry, and a synthetic frame samples/colour-processes/smooths
// through the REAL linked functions, not a pasted copy.
```

### shell-feed/tools/test_stagec_core.c:23

In the code now:

```
// Signatures match this repo's own tools/host_stubs/orbis/libkernel.h and tools/test_plugin_config.c...
// More: docs/code-comments.md (shell-feed/tools/test_stagec_core.c:23)
```

Original (lines 23-25):

```
// Signatures match this repo's own tools/host_stubs/orbis/libkernel.h and tools/test_plugin_config.c
// exactly (ssize_t for Read/Write, not int32_t -- the research repo's own stub differs here, so this
// is deliberately repo-native rather than copied from there).
```

### shell-feed/tools/test_stagec_core.c:34

In the code now:

```
// The shell feed is split into modules (shell-feed/source/shell_*.c).
// More: docs/code-comments.md (shell-feed/tools/test_stagec_core.c:34)
```

Original (lines 34-36):

```
// The shell feed is split into modules (shell-feed/source/shell_*.c). Include the ones this test exercises into
// this translation unit (textual inclusion, so their file-static state is visible to the checks below), plus
// stand-ins for what lives in modules that need the console (buffer scan, hooks, timers).
```

### shell-feed/tools/test_stagec_core.c:57

In the code now:

```
// paint a flat-colour rect into a raw (non-tiled) ARGB buffer, then we'll let the real...
// More: docs/code-comments.md (shell-feed/tools/test_stagec_core.c:57)
```

Original (lines 57-62):

```
// paint a flat-colour rect into a raw (non-tiled) ARGB buffer, then we'll let the real
// getTiledElementByteOffset() read it back through the tiled addressing -- since we don't have
// a real tiled buffer, this test paints a FLAT buffer instead and checks sampling doesn't crash
// and produces a stable, deterministic value, not that it matches a specific untiled picture
// (that part -- the tiling math itself -- is what host_zone_check and the probe's own v16/v17
// capture history already separately proved correct on real hardware dumps).
```

### shell-feed/tools/test_stagec_core.c:98

In the code now:

```
// 3. shell_process_frame(): samples a synthetic buffer through the REAL sampleZoneAverage()...
// More: docs/code-comments.md (shell-feed/tools/test_stagec_core.c:98)
```

Original (lines 98-100):

```
// 3. shell_process_frame(): samples a synthetic buffer through the REAL sampleZoneAverage(),
//    applyColorProcessing(), applyDarkThreshold() (zones.c/color_processing.c/gamma.c, linked),
//    plus this file's own smoothing block, exactly as it will run on hardware.
```

## tools/decode_verification_dump.py

### tools/decode_verification_dump.py:88

In the code now:

```
# Paste captured payload_hex values here (from udp_ground_truth_listener.py's "payload_hex=..."...
# More: docs/code-comments.md (tools/decode_verification_dump.py:88)
```

Original (lines 88-93):

```
# Paste captured payload_hex values here (from udp_ground_truth_listener.py's
# "payload_hex=..." lines), one string per packet. Each should decode to
# 11 bytes (22 hex chars) from the v2.1 probe, 10 bytes (20 hex chars)
# from an older capture, or 32 bytes (64 hex chars) for a v2.2
# registration packet. Order doesn't matter -- the script sorts by point
# index / call index automatically.
```

### tools/decode_verification_dump.py:99

In the code now:

```
# Per-format unpack functions.
# More: docs/code-comments.md (tools/decode_verification_dump.py:99)
```

Original (lines 99-102):

```
# ---------------------------------------------------------------------------
# Per-format unpack functions. Each takes the raw 4 bytes (as read off the
# wire, little-endian uint32) and returns (alpha_bits_or_None, (r8, g8, b8)).
# ---------------------------------------------------------------------------
```

### tools/decode_verification_dump.py:156

In the code now:

```
# HDR (A2R10G10B10_BT2020_PQ) decode -- added when HDR colors were reported wrong.
# More: docs/code-comments.md (tools/decode_verification_dump.py:156)
```

Original (lines 156-181):

```
# ---------------------------------------------------------------------------
# HDR (A2R10G10B10_BT2020_PQ) decode -- added when HDR colors were reported
# wrong. Root cause: this format was previously routed to the SAME plain
# 10-bit truncation as SDR A2R10G10B10 (see unpack_a2r10g10b10 above) --
# defensible for gamma-encoded SDR content per that function's own
# docstring, but PQ (SMPTE ST 2084) is a fundamentally different, much
# steeper curve. Truncating raw PQ code values straight to 8-bit RGB has
# no meaningful relationship to the actual displayed color.
#
# Pipeline: PQ code value -> linear light (nits) -> tone-map down from
# PQ's 10,000-nit range -> BT.2020 -> BT.709/sRGB primaries -> sRGB gamma
# encode -> 8-bit. Bit LAYOUT (which 10 bits are R/G/B/A, MSB-first,
# blue at LSB) is unchanged from unpack_a2r10g10b10 -- HDR doesn't change
# how the tiling/channel packing works, only how the numbers should be
# interpreted once unpacked.
#
# PQ_REFERENCE_WHITE_NITS / PQ_TONE_MAP_MAX_NITS below are ASSUMPTIONS,
# not measured constants. The PQ EOTF and BT.2020->BT.709 matrix are
# fixed standards (SMPTE ST 2084 / ITU-R BT.2087) and shouldn't need
# tuning, but where to set "reference white" and how hard to roll off
# highlights depends on what THIS console/title is actually doing with
# the format, which nothing here has measured yet -- tune these against
# a real capture + known on-screen color, the exact same empirical method
# that settled base vs Neo. main() also prints the OLD naive
# truncation next to this decode when this format is active, so you can
# compare both against the screen the same way base vs Neo were compared.
```

### tools/decode_verification_dump.py:269

In the code now:

```
# Maps the raw format enum value (as read off the wire, masked to 32 bits) to (display name, unpack...
# More: docs/code-comments.md (tools/decode_verification_dump.py:269)
```

Original (lines 269-272):

```
# Maps the raw format enum value (as read off the wire, masked to 32 bits)
# to (display name, unpack function). Formats with no unpack function are
# known/named but not yet supported for auto-decode (e.g. float formats
# need a different raw4 size / decode path entirely).
```

### tools/decode_verification_dump.py:285

In the code now:

```
# v3.2: runtime format auto-detection via decode smoothness, added after a real, confirmed dead end...
# More: docs/code-comments.md (tools/decode_verification_dump.py:285)
```

Original (lines 285-312):

```
# v3.2: runtime format auto-detection via decode smoothness, added after
# a real, confirmed dead end trying to detect HDR state directly. The
# obvious approach -- hook sceVideoOutAddBufferHdrPrivilege, the PS4
# API a game presumably calls when HDR engages -- turned out to have
# no real reference implementation anywhere: not in this repo's own
# probe/plugin history, not in fpPS4 (checked its full, real
# ps4_libscevideoout.pas -- the function isn't implemented there at
# all, only present as a bare name-to-NID lookup entry with zero type
# info). Guessing a signature for a hook GoldHEN's HOOK_CONTINUE would
# need to forward calls through exactly right is a real crash risk
# with no way to verify it beforehand, so that path was dropped.
#
# This sidesteps detection entirely: since both real formats seen on
# this title are now independently confirmed correct decoders (SDR:
# A8B8G8R8_SRGB with R/B swapped, confirmed against a real screenshot
# and independently against fpPS4's own enum comment; HDR:
# A2R10G10B10_BT2020_PQ, confirmed against a real screenshot at all 5
# points), the two candidate decodes can just be tried against each
# other and scored by which one looks more like real image content.
#
# The scoring signal: real photographic/rendered content is spatially
# coherent -- neighboring pixels are usually close in value, even
# across hard edges the change is bounded. Decoding the WRONG bit
# layout effectively randomizes the bit pattern, which tends to
# produce much larger, noisier swings between adjacent samples. Each
# capture already includes 4 consecutive 32bpp words per test point
# (RAW_DUMP_BYTES=16, from v2.9) -- exactly the adjacent-sample data
# this needs, with no probe/firmware changes required at all.
```

### tools/decode_verification_dump.py:397

In the code now:

```
# Legacy case this script's error message still allows for -- no real packet from this probe has ever...
# More: docs/code-comments.md (tools/decode_verification_dump.py:397)
```

Original (lines 397-401):

```
# Legacy case this script's error message still allows for --
# no real packet from this probe has ever actually been this
# short (main.c always sends the trailing displayBufferIndex
# byte), but keep the old fixed-4-byte behavior here rather
# than let data[-1] silently eat a real raw byte.
```

### tools/decode_verification_dump.py:677

In the code now:

```
# v3.2: format 0x80002200 is now a known special case for this title -- confirmed to mean two...
# More: docs/code-comments.md (tools/decode_verification_dump.py:677)
```

Original (lines 677-685):

```
# v3.2: format 0x80002200 is now a known special case for this title --
# confirmed to mean two DIFFERENT real byte layouts depending on HDR
# state (SDR: A8B8G8R8_SRGB swapped; HDR: A2R10G10B10_BT2020_PQ), with
# no reliable way to tell which from the registration event alone (see
# this script's own v3.2 comment above detect_format_via_smoothness for
# why a direct HDR-state hook was ruled out). When the registration-
# based pick lands on this format, run the smoothness heuristic and let
# it override the choice for the table below, rather than trusting the
# ambiguous format ID at face value.
```

### tools/decode_verification_dump.py:704

In the code now:

```
# When the active format is the HDR PQ format, also show the OLD naive truncation decode side by side...
# More: docs/code-comments.md (tools/decode_verification_dump.py:704)
```

Original (lines 704-709):

```
# When the active format is the HDR PQ format, also show the OLD naive
# truncation decode side by side -- this is what "colors weren't
# accurate" was actually looking at before this fix, and printing both
# lets you visually confirm the PQ decode is the one that now matches
# the real screen, the same empirical comparison method that settled
# base vs Neo.
```

### tools/decode_verification_dump.py:741

In the code now:

```
# v2.9: if more than 4 raw bytes came through, also show what each following 4-byte word would decode...
# More: docs/code-comments.md (tools/decode_verification_dump.py:741)
```

Original (lines 741-748):

```
# v2.9: if more than 4 raw bytes came through, also show what
# each following 4-byte word would decode to as its own pixel
# -- if this really is contiguous 32bpp pixel data, these
# should look like a plausible, smoothly-varying continuation
# of whatever's actually next to that point on screen. If they
# look like noise or don't relate to nearby real colors at
# all, that's evidence against "this memory is 32bpp pixels"
# entirely, not just against the current offset formula.
```

### tools/decode_verification_dump.py:769

In the code now:

```
# Sanity check for the v2.1 probe fix: every REAL test point in one dump should share the same...
# More: docs/code-comments.md (tools/decode_verification_dump.py:769)
```

Original (lines 769-774):

```
# Sanity check for the v2.1 probe fix: every REAL test point in one
# dump should share the same displayBufferIndex, since one capture
# = one live frame = one buffer. v2.7's dump_buffer_scan() rows
# (point_idx 254) are deliberately excluded -- they're tagged with
# their own slot index on purpose, always differing from each
# other and often from the live points, and that's not a bug.
```

## tools/flicker_capture.py

### tools/flicker_capture.py:404

In the code now:

```
# Which ring position did the PIPELINE actually read?
# More: docs/code-comments.md (tools/flicker_capture.py:404)
```

Original (lines 404-406):

```
# Which ring position did the PIPELINE actually read? Compare its raw read to each lag's buffer.
# (After a fix that samples an older slot, the plugin no longer reads lag 0 -- but the probe still
# measures every lag relative to the slot the hook reports.)
```

## tools/host_stubs/Common.h

### tools/host_stubs/Common.h:1

In the code now:

```
// PC test stand-in for the GoldHEN SDK's <Common.h>.
// More: docs/code-comments.md (tools/host_stubs/Common.h:1)
```

Original (lines 1-3):

```
// PC test stand-in for the GoldHEN SDK's <Common.h>. Only what the plugin's
// own headers need to parse: HOOK_EXTERN normally declares a typed hook
// pointer, here it just has to be a valid declaration.
```

## tools/ps4_detile_2dthin.c

### tools/ps4_detile_2dthin.c:1

In the code now:

```
// ps4_detile_2dthin.c Standalone C port of sce::GpuAddress::Tiler2d::detileSurface /...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:1)
```

Original (lines 1-31):

```
/*
 * ps4_detile_2dthin.c
 *
 * Standalone C port of sce::GpuAddress::Tiler2d::detileSurface /
 * getTiledElementBitOffset, from GPCS4 (Inori/GPCS4,
 * Graphics/Gnm/GpuAddress/GnmTiler.cpp + GnmTilemodes.cpp), hardcoded for
 * the exact surface this project captured from a live PS4 game via
 * videoout_probe:
 *
 *   width       = 1920
 *   height      = 1080
 *   format      = SCE_VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10_SRGB (32bpp)
 *   tmode       = TILE (0)
 *   tileMode    = kTileModeDisplay_2dThin (register 0x92000310)
 *   arrayMode   = kArrayMode2dTiledThin
 *   microTileMode = kMicroTileModeDisplay
 *
 * All tiling parameters below were derived by hand-evaluating the exact
 * same functions GPCS4 uses (computeSurfaceTileMode, computeSurfaceMacroTileMode,
 * Tiler2d::init) against these inputs -- not guessed, not from general GCN
 * knowledge. See the project's tiling-math notes and the
 * follow-up derivation for the full trail.
 *
 * NOT YET CONFIRMED: whether the console is actually running in Neo
 * (PS4 Pro / boost) mode. That changes bankHeight/numBanks/macroTileAspect
 * and therefore macroTileHeight and the padded buffer height. Both
 * parameter sets are included below (kParamsBase / kParamsNeo) -- run
 * detile against a real captured tile dump with each, and whichever one
 * produces a visually correct image is the right one. This *is* itself
 * indirect confirmation of Neo mode.
 */
```

### tools/ps4_detile_2dthin.c:77

In the code now:

```
// Derived by hand from computeSurfaceTileMode / computeSurfaceMacroTileMode / Tiler2d::init for...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:77)
```

Original (lines 77-79):

```
/* Derived by hand from computeSurfaceTileMode / computeSurfaceMacroTileMode /
 * Tiler2d::init for tileMode=kTileModeDisplay_2dThin, bitsPerElement=32,
 * numFragmentsPerPixel=1, width=1920, height=1080. */
```

### tools/ps4_detile_2dthin.c:120

In the code now:

```
// 32bpp display-microtile element swizzle (kMicroTileModeDisplay, bpp=32 case of getElementIndex...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:120)
```

Original (lines 120-122):

```
/* 32bpp display-microtile element swizzle (kMicroTileModeDisplay, bpp=32 case
 * of getElementIndex -- the other bpp cases were dropped since our surface
 * is always 32bpp). */
```

### tools/ps4_detile_2dthin.c:198

In the code now:

```
// core address-swizzle function, ported from Tiler2d::getTiledElementBitOffset (GnmTiler.cpp lines...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:198)
```

Original (lines 198-205):

```
/* ---- core address-swizzle function, ported from
 * Tiler2d::getTiledElementBitOffset (GnmTiler.cpp lines 1090-1231),
 * specialized: bitsPerElement=32, numFragmentsPerPixel=1, tileThickness=1,
 * arrayMode=kArrayMode2dTiledThin, microTileMode=Display, z=0, arraySlice=0,
 * bankSwizzleMask=0, pipeSwizzleMask=0 (all true for a plain scanout buffer,
 * no swizzle/no array/no volume). Dropping those terms removes a lot of
 * dead branches from the original without changing behavior for this case.
 * ---- */
```

### tools/ps4_detile_2dthin.c:266

In the code now:

```
// Detile a full 1920x1080 32bpp A2R10G10B10_SRGB surface. tiledPixels: pointer to the full tiled...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:266)
```

Original (lines 266-276):

```
/*
 * Detile a full 1920x1080 32bpp A2R10G10B10_SRGB surface.
 *
 * tiledPixels: pointer to the full tiled buffer, sized for paddedWidth x
 *              paddedHeight x 4 bytes (use p->paddedWidth/paddedHeight,
 *              NOT 1920x1080, when allocating/reading the source buffer --
 *              the GPU always allocates the padded size).
 * outLinearPixels: caller-allocated buffer sized 1920*1080*4 bytes,
 *                   row-major, top-to-bottom, 4 bytes/pixel matching the
 *                   original A2R10G10B10_SRGB packing.
 */
```

### tools/ps4_detile_2dthin.c:297

In the code now:

```
// Detile a single pixel -- useful for the empirical-verification step (via the ground-truth capture...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:297)
```

Original (lines 297-302):

```
/*
 * Detile a single pixel -- useful for the empirical-verification step
 * (via the ground-truth capture flow): pull a small chunk of real tiled bytes via
 * wled_send_raw, and check a handful of known pixel locations by hand
 * before committing to a full-frame detile in the hook.
 */
```

### tools/ps4_detile_2dthin.c:312

In the code now:

```
// Unpack A2R10G10B10 (MSB-first, blue at LSB) into 8-bit RGB for the WLED DDP path. sRGB gamma is left...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:312)
```

Original (lines 312-315):

```
/* Unpack A2R10G10B10 (MSB-first, blue at LSB) into 8-bit RGB for the WLED
 * DDP path. sRGB gamma is left as-is here (raw values); apply a
 * gamma-decode curve before sending to the strip if display-accurate
 * color matters more than "looks reasonable enough for ambient light." */
```

### tools/ps4_detile_2dthin.c:326

In the code now:

```
// Unpack A8R8G8B8 (MSB-first: A,R,G,B; little-endian uint32 = 0xAARRGGBB) into 8-bit RGB.
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:326)
```

Original (lines 326-332):

```
/* Unpack A8R8G8B8 (MSB-first: A,R,G,B; little-endian uint32 = 0xAARRGGBB)
 * into 8-bit RGB. No shifting math needed, just byte extraction -- this
 * is the format the real capture confirmed: the
 * A2R10G10B10 function above was the wrong assumption for this title/
 * buffer, not a wrong formula. Applying 10-bit math to 8-bit-per-channel
 * data is exactly what produced the "everything pinned near max"
 * symptom that looked like a tiling bug for most of this project. */
```

### tools/ps4_detile_2dthin.c:340

In the code now:

```
// Runtime format dispatch -- takes the raw format value straight off a live sceVideoOutRegisterBuffers...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:340)
```

Original (lines 340-347):

```
/* Runtime format dispatch -- takes the raw format value straight off a
 * live sceVideoOutRegisterBuffers capture (attribute->format) rather
 * than assuming one at compile time. This is the fix for the actual
 * root cause: a format assumption baked in once,
 * never re-checked against what the title is really doing. Add a case
 * here (plus the matching unpack function) for any new format a
 * registration-event capture turns up -- do not fall back to guessing
 * A2R10G10B10 again. */
```

### tools/ps4_detile_2dthin.c:365

In the code now:

```
// Quick sanity check: print the tiled byte offset for a handful of pixel coordinates under both...
// More: docs/code-comments.md (tools/ps4_detile_2dthin.c:365)
```

Original (lines 365-367):

```
/* Quick sanity check: print the tiled byte offset for a handful of pixel
 * coordinates under both parameter sets, so results can be eyeballed
 * against manually-decoded raw dumps from wled_send_raw. */
```

## tools/test_buffer_guard.c

### tools/test_buffer_guard.c:1

In the code now:

```
// test_buffer_guard.c -- host test for plugin/source/buffer_guard.c (v3.9).
// More: docs/code-comments.md (tools/test_buffer_guard.c:1)
```

Original (lines 1-25):

```
// test_buffer_guard.c -- host test for plugin/source/buffer_guard.c (v3.9).
//
// The guard is the code that decides whether the sampler may read a display buffer at all, and
// what to do about a GPU-only one (Mortal Kombat 11). It talks to the kernel through function
// pointers that main.c resolves by name, so a PC can stand in for the kernel: this file provides
// a fake address space and the fake sceKernelVirtualQuery / MapDirectMemory2 / MapDirectMemory /
// Munmap / Mprotect that read and change it, then includes buffer_guard.c itself.
//
// What the fake kernel is built from is what real captures showed, not guesses:
//   - MK11's display buffer: one region, protection 0x30, memory type 3, flags 0x12 (direct +
//     committed), 0x7EC000 bytes (shorter than the plugin's 1920 x 1088 x 4 = 0x7F8000 ceiling).
//   - the kernel refusing a second mapping of it with 0x80020010 (EBUSY).
//   - the kernel accepting sceKernelMprotect that adds CPU_READ (0x30 -> 0x31).
// What it cannot show: whether a real kernel does those things for another title, or how
// GPU-written write-combined memory behaves under CPU reads. Those need a console, see
// docs/debugging/gpu-only-buffers.md.
//
// Build and run (from the repo root; the two stub headers stand in for the GoldHEN SDK):
//
//   mkdir -p /tmp/stubbg
//   echo '#include <stdint.h>' > /tmp/stubbg/plugin_common.h
//   printf '#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n#define BASE_PADDED_BUFFER_BYTES ((uint64_t)1920 * 1088 * 4)\n' > /tmp/stubbg/ambient_internal.h
//   gcc -Wall -I/tmp/stubbg -o /tmp/test_buffer_guard tools/test_buffer_guard.c && /tmp/test_buffer_guard
//
// Exits 0 if every check passed.
```

### tools/test_buffer_guard.c:147

In the code now:

```
// 6. MK11 as captured, default mode 3.
// More: docs/code-comments.md (tools/test_buffer_guard.c:147)
```

Original (lines 147-149):

```
// 6. MK11 as captured, default mode 3. Kernel refuses the second mapping (EBUSY) and accepts the
//    mprotect (mode 3 tries the plain map too, also refused). The ORIGINAL address is returned, readable, with the region's real length. The
//    next pass sees a readable buffer and costs no further calls.
```

### tools/test_buffer_guard.c:199

In the code now:

```
// 11. If a kernel ever DOES accept the typed second view: it is created with the original's memory...
// More: docs/code-comments.md (tools/test_buffer_guard.c:199)
```

Original (lines 199-201):

```
// 11. If a kernel ever DOES accept the typed second view: it is created with the original's
//     memory type, returned as an offset into the view, reused on the next pass, and dropped
//     once the game's mapping changes (checked about once per 30 lookups).
```

## tools/test_hdr2200_vote.c

### tools/test_hdr2200_vote.c:1

In the code now:

```
// Host test for hdr2200Vote() in plugin/include/hdr2200_vote.h. gcc -Wall -o /tmp/test_hdr2200_vote...
// More: docs/code-comments.md (tools/test_hdr2200_vote.c:1)
```

Original (lines 1-13):

```
// Host test for hdr2200Vote() in plugin/include/hdr2200_vote.h.
//
//   gcc -Wall -o /tmp/test_hdr2200_vote tools/test_hdr2200_vote.c
//   /tmp/test_hdr2200_vote                       (run from the repo root)
//   /tmp/test_hdr2200_vote path/to/hdr2200_frames.csv
//
// Two kinds of input. Hand-written cases cover the vote's edges: SDR alpha
// 0xff or 0x00, PQ black, the hold band, cleared buffers, fade frames. The
// recorded ones are tools/data/hdr2200_frames.csv, 281 real 8-word frames the
// plugin read on a PS4 (HDRV telemetry, debug build) in HITMAN 3 and RDR2 with
// the console's HDR on and off. Neither game applies an HDR change until it is
// restarted, so every frame in a capture has the mode its file name says.
// See docs/debugging/hdr2200-alpha-detection.md.
```

### tools/test_hdr2200_vote.c:34

In the code now:

```
// Replays each capture the way zones.c uses the vote: the mode starts at SDR, a decisive vote sets it...
// More: docs/code-comments.md (tools/test_hdr2200_vote.c:34)
```

Original (lines 34-36):

```
// Replays each capture the way zones.c uses the vote: the mode starts at SDR, a decisive vote
// sets it, a hold leaves it. A frame is "wrong mode" if it had real (non-zero) words and the
// mode at that point wasn't the truth. A cleared-buffer frame before the first real one isn't.
```

## tools/test_notices.c

### tools/test_notices.c:1

In the code now:

```
// Host test for the plugin's notifications -- the real plugin/source/notices.c, driven with made-up...
// More: docs/code-comments.md (tools/test_notices.c:1)
```

Original (lines 1-6):

```
// Host test for the plugin's notifications -- the real plugin/source/notices.c, driven with made-up
// config state. Only the console calls are stand-ins (below). Checks what is said, and that each
// message is said once.
//
// Build and run from the repo root (one line):
//   gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_notices tools/test_notices.c plugin/source/notices.c && /tmp/test_notices
```

## tools/test_plugin_config.c

### tools/test_plugin_config.c:1

In the code now:

```
// Host test for the plugin's ini loading -- the real ambient_load_config() from...
// More: docs/code-comments.md (tools/test_plugin_config.c:1)
```

Original (lines 1-6):

```
// Host test for the plugin's ini loading -- the real ambient_load_config()
// from plugin/source/settings.c, run against sample inis on a PC. Only the
// hardware-facing calls are stand-ins (below).
//
// Build and run from the repo root (one line):
//   gcc -Wall -Wl,--wrap=stat -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_plugin_config tools/test_plugin_config.c plugin/source/settings.c plugin/source/config.c && /tmp/test_plugin_config
```

### tools/test_plugin_config.c:262

In the code now:

```
// v3.9.4: g_presetOverride (shell-feed).
// More: docs/code-comments.md (tools/test_plugin_config.c:262)
```

Original (lines 262-264):

```
// v3.9.4: g_presetOverride (shell-feed). NULL is the default and every assert above this
// point ran with it NULL, so this plugin's own behaviour (all 13 cases above) is unaffected
// by the override's mere existence -- these new cases are additional, not a replacement.
```

### tools/test_plugin_config.c:273

In the code now:

```
// 14c. override pointing at a section the ini doesn't define falls through to the normal pick, same...
// More: docs/code-comments.md (tools/test_plugin_config.c:273)
```

Original (lines 273-275):

```
// 14c. override pointing at a section the ini doesn't define falls through to the normal
// pick, same "use the other one" rule preset_section_pick() already follows -- an override
// never produces a worse result than no override at all.
```

### tools/test_plugin_config.c:283

In the code now:

```
// 14e. override=preset_game on a title that would normally force movie (Netflix): override wins.
// More: docs/code-comments.md (tools/test_plugin_config.c:283)
```

Original (lines 283-285):

```
// 14e. override=preset_game on a title that would normally force movie (Netflix): override wins.
// This is the exact situation shell-feed needs -- the shell is never "Netflix", but this proves
// the override outranks the media-title check, not just active=.
```

## tools/test_plugin_entry_alignment.c

### tools/test_plugin_entry_alignment.c:1

In the code now:

```
// Regression test for the GoldHEN plugin entry stack-alignment thunk.
// More: docs/code-comments.md (tools/test_plugin_entry_alignment.c:1)
```

Original (lines 1-5):

```
// Regression test for the GoldHEN plugin entry stack-alignment thunk.
// A naked caller deliberately calls the exported thunk with RSP%16 == 8
// immediately before CALL (the bad case seen in the PS4 crash context).
// The thunk must realign before it calls the implementation, whose entry
// therefore observes the ABI-required RSP%16 == 8.
```

## tools/test_pq8bit_vote.c

### tools/test_pq8bit_vote.c:1

In the code now:

```
// Host test for pq8bitVote() in plugin/include/pq8bit_vote.h. gcc -Wall -o /tmp/test_pq8bit_vote...
// More: docs/code-comments.md (tools/test_pq8bit_vote.c:1)
```

Original (lines 1-12):

```
// Host test for pq8bitVote() in plugin/include/pq8bit_vote.h.
//
//   gcc -Wall -o /tmp/test_pq8bit_vote tools/test_pq8bit_vote.c -lm
//   /tmp/test_pq8bit_vote
//
// Two kinds of input. The recorded ones are real 8-word reads out of PQ8C
// telemetry packets (debug build), copied from captures while YouTube played
// SDR video and then HDR video with the console's HDR on. The synthetic ones
// are real-PQ words built from nits with the ST 2084 curve, because no real
// PQ capture of a title that ISN'T lying about its format is in the repo, so
// the false-positive side is only tested against what PQ should look like.
// See docs/debugging/youtube-hdr-8bit.md.
```

## tools/test_report.c

### tools/test_report.c:1

In the code now:

```
// test_report.c -- host test for the per-title report file (v3.9.1): plugin/include/report_file.h (the...
// More: docs/code-comments.md (tools/test_report.c:1)
```

Original (lines 1-14):

```
// test_report.c -- host test for the per-title report file (v3.9.1): plugin/include/report_file.h
// (the text) and plugin/source/report.c (when it writes, and the write itself).
//
// report.c is included, not linked, so this runs the real ambient_report_tick() against made-up
// counters. What stands in for the console is small: the three raw file calls (redirected from
// /data to a scratch directory, and honoring the flags report.c really passes, so a missing
// O_TRUNC shows up as a stale tail) and the plugin globals report.c reads.
//
// Build and run from the repo root (one line):
//   gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_report tools/test_report.c && /tmp/test_report
//
// Exits 0 if every check passed. What this can't show: that a real console lets a game process
// write /data, which is the same thing settings.c already relies on for the default ini, or what
// the counters read on a title nobody has tried.
```

## tools/test_smoothing.c

### tools/test_smoothing.c:1

In the code now:

```
// test_smoothing.c -- host test for plugin/include/smoothing.h. Stage D of the shell-feed merge...
// More: docs/code-comments.md (tools/test_smoothing.c:1)
```

Original (lines 1-10):

```
// test_smoothing.c -- host test for plugin/include/smoothing.h.
//
// Stage D of the shell-feed merge replaced two copies of the per-zone smoothing
// (inline in plugin/source/sample_thread.c, pasted into shell-feed/source/main.c)
// with the two functions in smoothing.h. This test keeps a verbatim copy of the
// OLD code below as the reference and checks that the new functions give
// bit-identical results on every input that can reach them, then checks the
// property the v2.7.5 fix exists for (a fade always reaches its target).
//
//   gcc -Wall -Iplugin/include -o /tmp/test_smoothing tools/test_smoothing.c && /tmp/test_smoothing
```

### tools/test_smoothing.c:66

In the code now:

```
// 2. step: every previous value x every target x every alpha 0..256, both valid and not-yet-valid.
// More: docs/code-comments.md (tools/test_smoothing.c:66)
```

Original (lines 66-68):

```
// 2. step: every previous value x every target x every alpha 0..256, both
//    valid and not-yet-valid. Channels are independent, so one channel is
//    swept exhaustively and the other two ride along with other values.
```

## tools/test_state.c

### tools/test_state.c:1

In the code now:

```
// Host regression test for the direct foreground/background state writer.
// More: docs/code-comments.md (tools/test_state.c:1)
```

Original (lines 1-4):

```
// Host regression test for the direct foreground/background state writer.
// Build from the kit root:
//   gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_state tools/test_state.c && /tmp/test_state
// This tests the real ambient_state_tick() and file record format, not PS4 foreground detection.
```

## tools/udp_ground_truth_listener.py

### tools/udp_ground_truth_listener.py:20

In the code now:

```
# If the payload is a repeated solid color (what wled_signal sends), the first 3 bytes (6 hex chars)...
# More: docs/code-comments.md (tools/udp_ground_truth_listener.py:20)
```

Original (lines 20-22):

```
# If the payload is a repeated solid color (what wled_signal sends),
# the first 3 bytes (6 hex chars) of the payload are the R,G,B value
# repeated for every LED — show that explicitly.
```

## tools/update_payload_checksum.sh

### tools/update_payload_checksum.sh:2

In the code now:

```
# Keeps payloads/CHECKSUMS.md in step with payloads/ps4-ambilight.bin, the Ambilight payload. sh...
# More: docs/code-comments.md (tools/update_payload_checksum.sh:2)
```

Original (lines 2-10):

```
# Keeps payloads/CHECKSUMS.md in step with payloads/ps4-ambilight.bin, the Ambilight payload.
#
#   sh tools/update_payload_checksum.sh           rewrite the hash and size in CHECKSUMS.md from the file
#   sh tools/update_payload_checksum.sh --check   change nothing; exit 1 if CHECKSUMS.md is stale
#
# It rewrites three things and nothing else: the hash in the `sha256sum -c` block, and the size
# and hash in the table row. The row's last column (the note about which build this is) is yours
# to edit by hand. Needs only sh, sed, sha256sum (or shasum) and wc, so it runs on a PC, Git Bash
# and the CI runner. CI runs --check on every push and again before a release is published.
```
