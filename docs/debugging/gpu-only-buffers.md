# Mortal Kombat 11: display buffers the CPU can't read (v3.9)

Mortal Kombat 11 (CUSA11395) took the game down with the plugin loaded and ran fine
without it. The cause was the sampler reading a display buffer that the CPU is not
allowed to read. This is what the captures showed, the fix, and the paths that did
not work, in the order they were tried.

## The crash

The crash report, from the console's log:

- `SIGSEGV` on thread `ambient_sample_thread`, in the game's `eboot.bin`.
- Reason: page fault, user read data, page not present, at address `0x49ff38b6d0`.
- `rip` was `0x80082719c`. The plugin was mapped at `0x800820000`, so the fault is at
  plugin offset `+0x719c`. The caller is at `+0x43e8`.
- The game itself was fine and still early in boot (attract mode). No other thread had
  crashed. The plugin disabled: no crash. Enabled: crash.

It has the same signature as the v3.2 companion-app crash (sampler thread, page-fault
read of an address that isn't a real buffer, [architecture](../architecture.md)), but a
different cause, and the underlying weakness is the same: `sampleZoneAverage()` checked
the read **offset** against `BASE_PADDED_BUFFER_BYTES` and never asked whether the
**address** was mapped or readable by the CPU.

The first captures were from a release build, which sends none of the flip or format
telemetry, so they said what crashed but not why. The timing packets did show that the
sampler had only been idling on its keepalive path for the first seconds (passes of 4 to
11 µs, where a real pass reads thousands of pixels), so the crash was its first real
sampling pass and not something that built up.

Three explanations were ranked before there was more data:

1. MK11's buffers aren't CPU-readable (GPU-only mappings).
2. A stale or unmapped slot: the game registered buffers, freed or re-created them, and
   the plugin kept the old address.
3. The console was in HDR10 (`A2R10G10B10` PQ in the boot log). Thought unlikely, since
   the fault is about mapping and not about decoding.

## The guard, and what it found

The first fix (`buffer_guard.c`) asks the kernel, through `sceKernelVirtualQuery`,
whether the buffer is mapped and CPU-readable before every pass, and treats an
unreadable one like a missing one: the existing keepalive path, so the strip holds its
last color. That stopped the crash, and its debug packets showed why the read faulted.

| What | Value |
|---|---|
| Registered format | `0x88740000` (A2R10G10B10 PQ), recognized |
| Swap chain | 3 slots, `0x49fe208000`, `0x49fec08000`, `0x49ff608000`, 8 MB apart |
| Region per slot | exactly the buffer: `0x7EC000` bytes, start of region = start of buffer |
| Direct-memory offsets | `0x64a08000`, `0x65208000`, `0x65a08000` |
| Protection | `0x30`: GPU read and GPU write. **No CPU bits.** |
| Memory type | 3 (write-combined) |
| Flags | `0x12`: direct memory, committed |
| Guard reason | 2 (mapped, not CPU-readable), on every pass, about 30 a second |

The old fault address `0x49ff38b6d0` sits about 7.9 MB into the middle slot's range, so
the pointer was valid and inside the buffer. That rules out explanations 2 and 3: it
isn't stale, it isn't too short, and the format decode never ran. Explanation 1 was it.

Format recognition and readability are separate questions. Recognizing `0x88740000` says
how to decode a pixel. It says nothing about whether the CPU may load one. A working
title that was captured beside MK11 registers the same `0x88740000` and samples fine,
because its buffers are CPU-readable.

## Wrong turns

**The first guard rejected too much (v3.9 first build).** It required the whole
`BASE_PADDED_BUFFER_BYTES` (`0x7F8000`, the 1920 by 1088 by 4 ceiling) to be readable.
MK11's region is `0x7EC000`, `0xC000` bytes short of that, so a title whose buffers are
CPU-readable but end at that size would have been rejected outright: a dark strip on a
game that used to work. It never showed on MK11 because MK11 was rejected for its
protection first, and it was found while decoding the capture, not by testing a working
title. The guard now measures how many bytes from the base are readable
(`g_readableLimit`) and rejects only when it is 0. Every pixel read in `zones.c`,
`letterbox.c` and the debug probe is checked against that limit, so a short buffer skips
its unreadable tail instead of faulting or being refused. Zones that sit entirely in the
skipped tail read as black. No captured title has hit this.

**The `sceKernelVirtualQuery` struct was written from memory.** The first version gave
`flags` four bytes. The capture read `0x12` as the flag byte, and the bytes after it were
the start of the region's name string, so `flags` is one byte. The protection and memory
type positions were right. Fixed. The OpenOrbis header has the same call as
`sceKernelVirtualQuery(const void*, int32_t, OrbisKernelVirtualQueryInfo*, size_t)`, and
the plugin resolves it by name in `main.c`, like `sceSystemServiceGetStatus`.

**Decoding the packets by length.** `GRDI` (48 bytes after the DDP header in the first
build) has the same length as `PQ8C` and `HDRV`, and a first pass that classified
packets by length read the wrong ones. The packets now start with a 4 byte tag, and
`tools/decode_guard_packets.py` goes by the tag. The listener also prints "first LED =
RGB(...)" for every packet, which only means something for a solid color; for a `FLK1`
packet it is the ASCII of the tag (`F`, `L`, `K` = 70, 76, 75), not a color.

**Mapping a second, CPU-readable view (methods 1 and 2).** The memory is ordinary direct
memory, so the plan was to ask the kernel for a second `CPU_READ` view of the same
offset and sample from that, leaving the game's own mapping alone. Type mattered: a
write-back view of write-combined memory would let the CPU cache lines the GPU later
overwrites without snooping, and the view would freeze on old pixels, so the view was
created with the type the original reports (`sceKernelMapDirectMemory2`). With the ini
key `gpu_only_remap=1`:

- The kernel refused it every time: `0x80020010`, SCE errno 16, `EBUSY`. The range is
  already mapped, and a second mapping of it isn't allowed.
- It was tried 435 times in a capture of about 23 s, and each refused call cost about
  6 ms of the sampler thread. Pass time went from tens of microseconds to a steady
  6.2 ms. A refusal is deterministic, so a refused region is now remembered and not
  retried until its start, size or offset changes. After that fix the same run showed 3
  refusals (one per slot) and pass times of 40 to 70 µs.
- The plain `sceKernelMapDirectMemory` (kernel picks the type, so the stale-pixel risk
  above applies) is method 2. It was refused too, but that is inferred: with the default
  3 the packet ended on method 3, which only runs after both mapping methods failed, and
  method 3's return code overwrote method 2's. Its own code isn't in any capture.

**`sceKernelMprotect` on the game's own mapping (method 3). This worked.** Add `CPU_READ`
to the protection the mapping already has (`0x30` to `0x31`). Nothing else changes, so
it stays write-combined: reads are uncached and always see what the GPU wrote, with none
of the stale-cache risk. The plugin verifies with a fresh query that the bit actually
stuck before trusting it. It was the last resort, because the kernel might well have refused to add a bit the
mapping was created without. It didn't.

## Results

| Capture | Setting | Result |
|---|---|---|
| Crash (release build) | no guard | SIGSEGV at the first real pass |
| Guard only, about 14 s | guard | no crash, 30 rejections a second, reason 2, strip dark |
| Guard plus info packet | guard | the table above |
| `gpu_only_remap=1` | typed second view | 435 refusals (EBUSY), 6.2 ms passes, strip dark, no crash |
| same, after the no-retry fix | 1 | 3 refusals, 40 to 70 µs passes, strip dark, no crash |
| `gpu_only_remap=3`, about 37 s | mprotect | created 3, failed 0, rejects stop at 3, all 3 slots readable, about 2.6 ms average pass (3.8 ms worst), no crash |
| `gpu_only_remap=3`, gameplay, about 53 s | mprotect | created 3, failed 0 the whole time, 1,422 probe packets all with 3 slots readable, zone output up to 210 of 255, median window 2.9 ms, no crash |

The colors on the strip were confirmed by eye on real MK11 content. They were not
measured against a reference. The first capture at 3 sampled only near-black pixels (raw
channels 8 or below, from a scene fading in), so it could not have shown a wrong decode;
the gameplay one has real content.

The comparison title (not identified in the capture) had 0 rejections across about 30 s
and about 2.0 to 2.5 ms average passes at the same format, so a readable buffer costs
about the same as MK11 does now. It ran on the v3.9 guard with the short-buffer fix.

## What it does now

`ambient_resolve_readable()` in `buffer_guard.c` is the one entry point, called by the
sampler right after the lag logic and by the debug probe for each slot it reads.

1. Ask the kernel how many bytes from the buffer base the CPU may read. If some, that is
   `g_readableLimit` and the buffer is used as it is. A working title never goes further.
2. If it was refused as reason 2 and the region is direct memory, try the methods in
   order, up to what `gpu_only_remap` allows: a typed second view, the untyped one,
   `mprotect`. The default is 3, all of them. The ini key is optional and nothing needs
   to set it. 0 turns the whole thing off.
3. If none works, the buffer is unreadable and the pass takes the keepalive path. A
   refused region is remembered.

Only step 1 runs for a buffer that is already readable, so a title that works today
never reaches steps 2 and 3. If `sceKernelVirtualQuery` can't be resolved (other
firmware), the guard fails open and behaves as v3.8 did, before the guard existed.

## What isn't known

- **One title.** Only MK11 was captured with a GPU-only buffer. Whether `mprotect` is
  accepted for another title's buffers is untested, though MK11's are ordinary display
  buffers.
- **Not direct memory.** A GPU-only region that isn't direct memory (flexible memory,
  say) can't be handled by any of the three methods, and would give a dark strip, not a
  crash. Nobody has seen one.
- **Two pauses.** In the gameplay capture two single passes took 94 ms and 123 ms (one
  over-budget pass in each of two 30-pass windows), at 12:51:23 and 12:51:30. The other 45
  windows averaged under 4 ms. The cause is unknown. Uncached reads competing with the GPU
  under load is one possibility and nothing here tests it. The strip would hold its
  color for about a tenth of a second at those moments. The earlier, quieter capture
  never went above 3.8 ms.
- **A buffer freed mid-pass** can still fault. The check is before each pass, so the
  window is small and not zero.
- **Re-created buffers.** If a game frees and re-creates a buffer, the new region is
  rejected again, gets a new `mprotect`, and is verified again. That logic is exercised
  by the host test, not seen on a console. A full match, menus and scene changes were
  not captured.
- **What changing the mapping does to the game.** The game ran for the whole of both
  captures without a fault or an obvious change, and adding a CPU read bit shouldn't
  affect the GPU. That is the whole of the evidence.
- **A release build wasn't captured.** Everything above is from debug builds, which is
  where the telemetry is.
- **The capture tables are from the earlier builds.** The guard logic was first built and
  run on the console as overlay builds over v3.7 (before the Game and Movie presets of v3.8
  existed), with untagged debug packets. Everything quoted above comes from those. It was
  then merged onto v3.8 (`settings.c`, `hooks.c`, `main.c` and `ambient_internal.h` were all
  touched by the presets too; no conflict was resolved by dropping a line of either side),
  the debug packets gained their 4 byte tags (`GRDC`, `GRDI`, `RMAP`, and the packet sizes
  grew by 4), the comments were rewritten to one story and the version became v3.9. The
  merged build was built and tested on a console and works.

## Telemetry (debug builds)

Three packets, sent about once a second from the sampler, each starting with a 4 byte
ASCII tag (layouts in `network.c`, decoder in `tools/decode_guard_packets.py`).

| Tag | Bytes after the DDP header | Contents |
|---|---|---|
| `GRDC` | 24 | guard available, reject count, last reason, last rejected address |
| `GRDI` | 52 | start, end, direct-memory offset, protection, memory type, flags of the last rejected region. Only sent once there has been a rejection. It is a snapshot of that last failed query, so it keeps showing protection `0x30` after `mprotect` fixed the buffer. |
| `RMAP` | 28 | the `gpu_only_remap` setting, which functions resolved, last method tried, created, failed, passes, last return code, alias address (low 32 bits) |

Reading them: on a fixed title `GRDC` rejects stop at 1 per slot and `RMAP` shows created
above 0. On a title that stays dark the rejects climb by about 30 a second. `RMAP` last
return `0x80020010` means a second mapping was refused. The captures quoted above were
made before the tags were added: same fields, the first 4 bytes are the tag now.

## If another title has a dark strip or crashes

1. Debug build, `[dev]` logging on, capture with `tools/udp_ground_truth_listener.py`,
   decode with `tools/decode_guard_packets.py`.
2. `GRDC` guard `FAILING OPEN` means `sceKernelVirtualQuery` didn't resolve, so none of
   this applies.
3. Rejects climbing and `GRDI` showing flags without the direct bit (`0x2`): not direct
   memory, none of the methods can help. Note the protection, type and flags, and add
   the title to [title-compatibility](title-compatibility.md).
4. Rejects climbing, direct memory, `RMAP` failed above 0: the kernel refused all three.
   The last return code says how. `0x8002000d` or similar from `mprotect` would mean it
   won't add the bit for that mapping.
5. No rejects but a wrong or dark strip: not this, see
   [hdr-pixel-format](hdr-pixel-format.md).

## Tests

`tools/test_buffer_guard.c` includes `buffer_guard.c` and runs it against a fake kernel
built from what the captures showed, on a PC (27 checks): a readable buffer is left
alone and never reaches the remap path, a short readable buffer is used with its real
limit, MK11 as captured ends with protection `0x31` and the type unchanged, every refused
method is tried once per region and not again, a re-created region gets a new try, an
`mprotect` that reports success without the bit sticking is treated as a failure, and
each `gpu_only_remap` value does what it says. What it can't show is a real kernel's
answers for another title. See [tools](../development/tools.md#test_buffer_guardc).
