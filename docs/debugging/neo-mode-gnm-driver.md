# PS4 Pro: titles in Neo mode use a different Gnm driver

On a PS4 Pro the plugin lit the strip in some games and in others stopped before it hooked anything.
The [load log](report-file.md#the-load-log) showed where it stopped, and the module list in it showed
why: a title that runs in Pro (Neo) mode does not load `libSceGnmDriver`, it loads
`libSceGnmDriverForNeoMode`. The plugin only looked for the first. This write-up is the evidence and
the change. The strip now follows the picture in every Pro-mode title that loads only the Neo
driver (below); one title that also loads the normal driver still sees no flips. All of it is from
load logs and reports from a Pro, none from a base console.

## What the logs showed

Load logs from a PS4 Pro (firmware 11.02, GoldHEN 2.4b18.12) across several titles. The split was by
`sceKernelIsNeoMode`, not by game:

| Title | `sceKernelIsNeoMode` | `libSceGnmDriver` | What the log shows |
|---|---|---|---|
| CUSA01830 (Assetto Corsa) | 0 | loads | hooks installed, format `0x80002200` recognized, flips counted, no guard rejects |
| CUSA00900 | 0 | loads | hooks installed, format `0x80000000` recognized, flips counted |
| CUSA23264 (NFS Hot Pursuit Remastered), CUSA27790 (WRC 10), CUSA36843 (Red Dead Redemption), CUSA08161 (Devil May Cry 5) | 1 | rc -1, handle 0 | `STOP: a video library handle is 0`; `libSceVideoOut` loaded fine |

In each of the four stopped titles the module list had `libSceGnmDriverForNeoMode.sprx` (handle 56,
and 52 in one title) and the `gnmdriver candidate` line said both `sceGnmSubmitAndFlipCommandBuffers`
and `sceGnmSubmitAndFlipCommandBuffersForWorkload` were found in it. Those are the two Gnm functions
the plugin hooks. So the strip stayed dark because the plugin never hooked those titles, and nothing
about the picture or its tiling was involved at that point.

The module is not a surprise on the console side: emulator projects that reimplement the PS4
firmware list it as a separate module next to `libSceGnmDriver` and give it the same flip exports
(for example [AnyPS5 #16238](https://github.com/boykopovar/AnyPS5/pull/16238) and
[#11971](https://github.com/boykopovar/AnyPS5/pull/11971)). Nothing found says which titles load
it. The logs above are what ties it to Neo mode here.

## What changed

When `libSceVideoOut` loads and `libSceGnmDriver` does not, `plugin_load` now loads
`libSceGnmDriverForNeoMode.sprx` by name, and if that gives no handle takes the first module with
`gnmdriver` in its name that has `sceGnmSubmitAndFlipCommandBuffers`. It then does the same symbol
lookups, forwarding-stub check and hooks as for the normal driver. A title that loads the normal
driver never gets there. A by-name load of a module that is already loaded returns its handle (the
`libSceVideoOut` handle on the load line matched its handle in the module list), which is why this
should work, but it was not run on a Pro when it was written.

It runs only with `[compat] report_file=1`.

## It works on a Pro, for these titles

With that build the Pro hooked the Neo driver in Red Dead Redemption, Devil May Cry 5 and
NFS Hot Pursuit Remastered: `trying libSceGnmDriverForNeoMode: load rc 0` (handle 56, 52 in Devil May
Cry 5), `hooks installed`, the format recognized (`0x80002200`, and `0x88740000` in Devil May Cry 5),
1,700 to 3,400 flips by T+60 and no guard rejects. YouTube (CUSA01015) and Netflix (CUSA00127) did
the same, with `0x88740000`. Colours looked right in all of these, so the base tiling reads
them; that is by eye, not a measurement, and the `tiling_check:` line in the per-title
report is still the way to settle it. Assetto Corsa, in base mode, is unchanged.

## Tekken 7: the normal driver loads, and nothing flips

Tekken 7 (CUSA06014) runs in Neo mode, but there `libSceGnmDriver` did load by name (rc 0, handle
135, with `libSceVideoOut` at 50), so the Neo step never ran. The hooks installed and one buffer
registration was seen (2 buffers, `0x80000000`), but `flip_calls` stayed 0 on all three entry points
and the strip stayed dark. In the titles that work the Neo driver sits next to `libSceVideoOut`
(55 and 56, 51 and 52), so handle 135 looks like a copy our own load created rather than the
title's driver. Two guesses, not yet told apart:

1. The title runs on the Neo driver and our by-name load of the normal one made a copy it never
   calls, so the flip hooks sit on a copy while the `libSceVideoOut` hook, which is shared, still
   sees the registration.
2. The title flips through a function the plugin does not hook.

So the plugin now looks at the module list before it loads anything by name (`snapshot (before our
loads)`). If the title never had the normal driver but has the Neo one, it hooks the Neo driver
even though its own load returned a handle. If the title has both, it keeps the normal one and says
so (hooking both needs a separate trampoline per target, which a second `HOOK32` would not give).
When a title registers buffers but shows no flips for 20 s, the `WAITING` line is followed by the
handles that were hooked and a module census with the address of each Gnm candidate's flip
functions. Both only with `report_file=1`. Not run on a Pro when it was written.

## Not known

- Whether the Tekken 7 change above makes it flip. If it does not, the census at `WAITING` is what
  to read next.
- Whether other formats and titles decode with the base tiling. Only `0x80002200`, `0x88740000`
  and `0x80000000` have been seen on a Pro, and the colours were checked by eye.
- Why `libSceGnmDriver.sprx` cannot be loaded by name in a Neo-mode process, and why it can in
  Tekken 7.
- Street Fighter 6 lights the strip but flickers. No log yet.

## Wrong turns

- The shell report's `claim: ever_seen yes` and `game_owned` counts were first read as the game
  plugin running in the failing games. They most likely came from titles where the plugin does hook
  (the plugin claims the strip only after its hooks are in), so they proved nothing about the others.
- Apollo Save Tool lit the strip on the Pro, which looked like the plugin working. Homebrew loads no
  `libSceVideoOut` or `libSceGnmDriver`, so the plugin stops there on every console (a base PS4
  gives the identical `STOP`). The shell feed lights homebrew; see
  [shell-feed-foreground-gate](../shell-feed-foreground-gate.md).
- Other plugins in `/data/GoldHEN/plugins/` (a frame-rate plugin among them) looked like
  a cause for hooks that never landed. They were not registered in `plugins.ini`, so they never
  loaded.

## Reading a Pro's log now

| Lines after `libSceVideoOut: load rc 0 ... libSceGnmDriver: load rc -1` | Meaning |
|---|---|
| `trying libSceGnmDriverForNeoMode: load rc 0, handle N`, then `using the Neo-mode Gnm driver` | the Neo driver was found; read on like any title (`hooks installed`, `register`, `flips seen`, `sampler running`) |
| `... load rc -1, handle 0`, then `found a Gnm driver module in the module list instead` | the by-name load failed, the module list had it |
| `... handle 0` and then `STOP:` with `no module with "gnmdriver" in its name` | the title has no Gnm driver module at all; the module list shows what it has instead |
| `snapshot (before our loads): ... loaded by the title: VideoOut A, GnmDriver B, GnmDriverForNeoMode C` | what the title itself had loaded (0 = not loaded). The `snapshot` lines above it name the graphics modules |
| `the title had no libSceGnmDriver (our load made handle N) but has the Neo driver at handle M; hooking the Neo driver` | the Tekken 7 case: our by-name load made a copy, and the hooks go on the Neo driver instead. Check that flips are now counted |
| `the title has BOTH Gnm drivers loaded (normal N, Neo M); keeping the normal one` | it cannot be told which one the title flips with; the next change has to hook both |
| `the title had neither Gnm driver loaded before our load` | our `libSceGnmDriver` is the only copy, so zero flips points at a flip path the plugin does not hook |
| `hooked VideoOut handle N, Gnm driver handle M; module list now:` after a `WAITING` line | the census taken while the title is registered but not flipping: compare each candidate's handle and flip-function addresses with what was hooked |
