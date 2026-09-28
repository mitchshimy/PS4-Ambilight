# VideoOut and flip hooks

What the plugin hooks, why there are three flip hooks, and how the third one was
found. The last part is the useful bit for the next title that doesn't light up.

## What is hooked

| Hook | Library | Used for |
|---|---|---|
| `sceVideoOutRegisterBuffersPtr` | VideoOut | learn the display buffer addresses, pixel format and attributes |
| `sceGnmSubmitAndFlipCommandBuffers` | Gnm | flip: records which slot was flipped |
| `sceVideoOutSubmitFlip` | VideoOut | flip, same idea |
| `sceGnmSubmitAndFlipCommandBuffersForWorkload` | Gnm | flip, for titles that skip the wrapper above |

All three flip hooks do the same thing: call `record_flip_index()` and pass
through to the real function. The two extra ones are resolved and hooked
non-fatally, so a title that doesn't use them doesn't lose the plugin.

The flip hooks fire when the game submits a flip, not at scanout. The slot they
report is therefore the one the GPU is about to render into. That matters for
[buffer selection](buffer-selection.md).

## v3.1: a title that never lit up

Shadow of the Tomb Raider never lit the strip, on either its SDR or its HDR run.
(It is the same title in both modes. An earlier capture session got that wrong and
treated them as two.)

Registration looked completely normal both times. The register hook fired once,
and the format was recognized both times (`0x80000000` on SDR, `0x88740000`
A2R10G10B10_BT2020_PQ on HDR). So it was not another gap in the pixel format
table.

### Counters instead of guessing

Added `g_registerHookCallCount` and `g_flipHookCallCount`, sent once a second in a
44-byte `send_flip_diag_packet` (debug builds only, `__FINAL__==0`). First capture
against the failing title: register climbing normally, flip count stuck at 0 for
the whole session, and `g_currentDisplayBufferIndex` never left its `0xFFFFFFFF`
sentinel. That's an unconditional per-second counter that didn't move across ~11
seconds, so it isn't throttling. `sceGnmSubmitAndFlipCommandBuffers` just wasn't
being called by this title.

### Guess 1: `sceVideoOutSubmitFlip`

A second real flip entry point in libSceVideoOut. Confirmed it exists with its own
NID by reading shadPS4's `video_out.cpp` rather than going from memory. Hooked it,
recaptured. It resolved fine (`submit_flip_ptr_resolved=1` in the extended
packet, so not a symbol resolution problem either) and the call count was still 0
the whole session. Wrong guess. I left the hook in anyway. It's harmless and some
other title may use it.

### Guess 2: `sceVideoOutSubmitEopFlip`

Checked and rejected before writing any hook code. It exists in shadPS4's
`video_out.cpp`, but it isn't registered with `LIB_FUNCTION`/an NID there, and it
lacks the `PS4_SYSV_ABI` tag every real exported hook target in that file has. It
reads as shadPS4's internal EOP interrupt bookkeeping, not something a game (or
this plugin's `sys_dynlib_dlsym`) can reach.

### The actual cause

`gnmdriver.cpp` in shadPS4 shows that `sceGnmSubmitAndFlipCommandBuffers` is a thin
wrapper:

```c
return sceGnmSubmitAndFlipCommandBuffersForWorkload(count, count, ...);
```

around a separately exported symbol with its own NID. A title submitting explicit
multi-workload GPU work can call the `ForWorkload` entry point directly and skip
the wrapper the plugin was hooking. Added it as the third hook, same non-fatal
resolve pattern.

Confirmed on real hardware: `gnm_for_workload_hook_call_count` climbing (0, 1, 30,
95...), `g_currentDisplayBufferIndex` off the sentinel and alternating between the
title's two swap-chain slots, `g_bufferAddrs` resolving to real addresses, and the
raw-pixel diagnostic packet firing at its normal rate again. The strip lit up.

## If another title never lights up

1. Check registration first. Did the register hook fire, and did
   `getUnpackFnForFormat` recognize the format? If not, it's a pixel format gap.
2. If registration is fine, look at the flip counters. A flip count stuck at 0
   with `g_currentDisplayBufferIndex` on its sentinel means the title is using a
   flip path that isn't hooked.
3. Before writing a hook for a candidate symbol, check that it exists with its own
   NID in a reference source. shadPS4's `video_out.cpp` and `gnmdriver.cpp` settled
   all three questions above, and one of them ruled a symbol out before any hook
   code was written.

## Other things to know

- `g_pluginVersion` had been stuck at the v2.7.2 value through v2.7.3 to v3.0. It
  is `0x00000303` now, and worth bumping alongside changelog entries so it doesn't
  drift again.
- The double-firing note in [framebuffer-flicker.md](framebuffer-flicker.md) (flip
  counter reading about twice the frame rate) is an inference from counts, not
  something confirmed in the hook code.
