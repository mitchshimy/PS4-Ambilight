# CI and releases

The workflow is `.github/workflows/CI.yml`. It builds the plugin and the companion
app, and on a tag it publishes a release.

## Jobs

| Job | Runner | What it does |
|---|---|---|
| `build_prx` | ubuntu-latest | checks out the GoldHEN SDK, patches it, builds the plugin in Release and Debug, uploads both |
| `build_pkg` | ubuntu-22.04 | compiles the companion app against the OpenOrbis toolchain and builds a `.pkg` |
| `publish` | ubuntu-latest | only on `refs/tags/*`, waits on both builds, computes a checksum, creates the release |

It runs on push, pull request and manual dispatch, and skips pushes that only touch
`*.md` files. Docs-only commits don't trigger a build.

A tagged release publishes exactly three things: `ps4_ambient_light.prx`,
`ps4_ambient_light.prx.sha256`, and the `.pkg`. The companion app's updater looks
for those first two by name, see
[companion-app-updater](companion-app-updater.md). If you rename them, the updater
breaks.

The release is its own job so that the two builds can run in parallel without
racing each other to create the same release. Only `publish` touches
`softprops/action-gh-release`.

## The GoldHEN SDK patch

CI pulls `GoldHEN/GoldHEN_Plugins_SDK` fresh every run, with no pin and no fork. That
meant every release built up to Sep 25 was linked against the unpatched SDK, and the
local fixes made to get the plugin stable on hardware weren't in it. Diffing a
known-good local SDK against a fresh checkout turned up three real bugs in the 64-bit
detour path, plus one unsafe macro. `patches/goldhen-sdk-detour64-fix.patch` fixes
them and CI applies it right after the checkout, before anything builds.

What it changes (its header has the full breakdown):

- `Detour_GetInstructionSize()` was sized with `sizeof(JumpInstructions32)` on the
  path that installs a 64-bit jump, so it read the wrong number of bytes for what it
  was about to overwrite. Now `JumpInstructions64`.
- `TrampolinePtr` was `malloc()`'d. That's ordinary heap memory, not executable under
  DEP/W^X, so jumping into it either crashes or is silently blocked. Now
  `sceKernelMmap` with executable protection, and the matching `sceKernelMunmap`
  on cleanup, which upstream never had (it leaked the mapping every time a hook was
  removed).
- `Detour_WriteJump32()` was called on a path that had just allocated a 64-bit sized
  trampoline, which wrote the wrong jump encoding. Now `Detour_WriteJump64()`.
- `klog` is now a no-op instead of `printf`. It gets called from inside hook code,
  where calling into libc's `printf` is a crash or hang risk.

The trampoline and instruction size bugs are almost certainly the real cause of a
hardware crash that had been blamed on optimization level. That's why an `-O2`
default was added and then reverted (`98a01fb`) before the SDK was fixed. It had
crept in with an unrelated Makefile fix, so the plugin that was tested crash free on
hardware had actually been built at the compiler's default level (effectively `-O0`)
the whole time. It still builds that way unless you pass `make O_FLAG=-O2`, and the
Makefile has a comment warning about it, since the raw hooks and the state shared with
the worker thread are exactly the code an optimizer can trip up.

A local build only gets these fixes if you patch your own `GOLDHEN_SDK` copy the same
way. The README's "Building the plugin" section has the commands. The patch was
verified with `patch -p1 --dry-run` against a fresh clone of the exact repo and ref CI
uses, and the patched result diffs byte-identical to the local copy it came from.

## Companion app build

`build_pkg` is the Linux equivalent of `companion-app/build.bat`. It compiles every
`source/*.c` with clang for `x86_64-pc-freebsd12-elf`, links, creates the self and
`param.sfo`, and builds the package with the toolchain's Linux `create-fself`,
`create-gp4` and `PkgTool.Core` binaries instead of the Windows ones.

`build.bat` had a `-I` for `include\freetype2`. On this toolchain snapshot that flag
does nothing, since `ft2build.h` resolves from the top level include directory, so
it was dropped from the Linux version instead of carried over.

### Why ubuntu-22.04

`PkgTool.Core`'s `pkg_build` needs `libssl1.1`, which `ubuntu-latest` (24.04) no
longer ships. The first real run aborted with "No usable version of libssl was
found". The job is pinned to 22.04 because of that.

### A bug CI found

`build_pkg` was the first time the companion app was ever build tested on a clean
machine. `ui_icons.c` used `CUR`, a "currentColor" sentinel, as a `static const UiColor`
object 84 times across its icon tables. Clang hard errors on reading a named const
object into another file scope initializer ("initializer element is not a
compile-time constant"), and GCC's default GNU mode had been quietly allowing it
locally. `CUR` is now a compound literal macro, matching how `CHAN_RED`,
`CHAN_GREEN` and `CHAN_BLUE` in the same file already worked. It was a real bug, not a
CI quirk.

## Cutting a release

1. Bump `g_pluginVersion` in `plugin/source/hooks.c` and add the changelog entry.
   The value is `0xMMmm`, major in the high byte and minor in the low one, so v3.5
   is `0x00000305`. (Before v3.1 the number was an internal counter, `0x212` for
   v2.7.2, and it sat at that value through v3.0. That is why the check below exists.)
2. Commit and push.
3. Tag it and push the tag:
   ```
   git tag v3.5.0
   git push origin v3.5.0
   ```
4. CI builds both artifacts and publishes. Check that the release has the `.prx`,
   the `.sha256` and the `.pkg`.

Don't move or re-push an old tag to re-release. Tag the next version instead. Every
tag stays as its own release, and the companion app's updater just follows `latest`.

### The tag check

The first step of `build_prx` runs on tag pushes and compares the tag with
`g_pluginVersion`. Only major and minor are compared, so `v3.4.0` and `v3.4.1` both
pass while the plugin says 3.4. The tag has to look like `v3.4` or `v3.4.0`, so
`v3.4.0-rc1` or `test` fail on purpose rather than publish something.

It fails before the SDK checkout and builds, and `publish` needs `build_prx`, so a
mismatch produces no release. The error names both versions. To fix it, correct
whichever side is wrong, then:

```
git tag -d v3.5.0
git push origin :refs/tags/v3.5.0
git tag v3.5.0
git push origin v3.5.0
```

The check only exists in the workflow file at the tagged commit. Tags on commits
from before it was added, and manual runs of the workflow on those tags, don't have
it and are unaffected.
