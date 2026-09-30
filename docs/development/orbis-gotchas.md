# Orbis and GoldHEN gotchas

Things that behaved differently on the console than on a PC or in the docs, each
one found the hard way. Most are from the commit history. If you're writing your own
plugin or homebrew, this list may save you a session.

## In the plugin

**`fopen()` from a worker thread crashed the game (v2.1.5).** The ini reader used
`fopen`/`fgetc`. From `ambient_sample_thread` (not the thread that runs
`plugin_load`) it took the game down. The reader in `config.c` now uses
`sceKernelOpen`, `sceKernelRead` and `sceKernelClose` into a heap buffer, with the
same parsing state machine. 87 reload checks over about 3 minutes, no crashes.

**`stat()` fields are not trustworthy (v2.1.1 to v2.1.5).** `st_mtime` came back as
0 and `st_size` was stuck at 8 for the life of the process. Live reload was built on
those, so it never fired. The current code gets the size from `sceKernelOpen` plus
`sceKernelLseek(SEEK_END)` and compares an FNV-1a hash of the content as well, since
a same-length edit (RGB to RBG) leaves the size unchanged. There is also a
dedicated `g_haveConfigBaseline` flag instead of using mtime == 0 as "no baseline
yet", which collided with a real value.

**No libm.** The plugin isn't linked against libm, so gamma uses precomputed lookup
tables, geometry is integer math, and the PQ tone map is a 1024 entry LUT built
lazily and only if a title uses PQ. The LUT was checked bit for bit against the live
computation for all 1024 codes, so it is caching, not an approximation.

**Don't call `printf` from hook code.** The stock GoldHEN SDK defines `klog` as
`printf`. That's an unsafe context, so the SDK patch makes it a no-op. See
[ci-and-releases](ci-and-releases.md#the-goldhen-sdk-patch).

**GoldHEN loads your plugin into every process.** Including your own homebrew, see
[architecture](../architecture.md#goldhen-loads-the-plugin-into-the-app-too).

**Flip hooks run at submit time.** [buffer-selection](../debugging/buffer-selection.md).

**Keep the flip hook light.** It only records which slot was flipped. All sampling
lives on its own thread, so a slow pass can't hold up the game.

**A recognized format doesn't mean the buffer is readable (v3.9).** Mortal Kombat 11
registers a format the plugin decodes, and its display buffers are mapped GPU-only
(protection `0x30`, memory type 3, write-combined), so a CPU load page-faults and takes
the game down with it. `sceKernelVirtualQuery` tells you the protection and how far the
region extends. Ask before every pass, don't assume because the same format worked on
another title. [gpu-only-buffers](../debugging/gpu-only-buffers.md).

**A second mapping of already-mapped direct memory is refused (v3.9).**
`sceKernelMapDirectMemory2` for a range the game has mapped returns `0x80020010`. The
`0x8002xxxx` codes are SCE errors with the errno in the low bits, and 16 is `EBUSY`. The
refusal is deterministic and each refused call costs about 6 ms, so a failed region has
to be remembered, not retried every pass.

**`sceKernelMprotect` can add `CPU_READ` to a GPU-only mapping (v3.9).** `0x30` to
`0x31`, memory type unchanged, so it stays write-combined (uncached reads, about 2.5 ms
per sampling pass on MK11, always current). Seen on one title. Verify with a fresh
`sceKernelVirtualQuery` that the bit stuck, a call that returns 0 isn't proof.

**The `sceKernelVirtualQuery` info struct's `flags` is one byte (v3.9).** Written from
memory as four bytes at first, the capture showed the next bytes were the start of the
region name.

## In the companion app

**`O_TRUNC` is `0x0400` on Orbis.** The app opened files with `0x200 | 0x001` and a
comment calling it `O_TRUNC|O_CREAT`. On Orbis (FreeBSD derived) that is really
`O_CREAT|O_WRONLY`. Against a brand new path it works, but on a path with leftover
bytes a shorter rewrite keeps a stale tail. The staged `.new` plugin file could end
up longer than the SHA-256 that was verified while downloading, which is why the
update check said "update available" on every launch. `plugin_common.h` now defines
`ORBIS_O_WRONLY` (0x0001), `ORBIS_O_CREAT` (0x0200), `ORBIS_O_TRUNC` (0x0400) and
`ORBIS_O_CREAT_TRUNC_WRONLY`, and every create or overwrite call uses it.

**Default thread stack is too small.** A background update-check thread created
with `SDL_CreateThread()` (stack size 0, "platform default") crashed on every launch
with a SIGSEGV write fault at exactly `rsp-8`. It now uses `scePthreadCreate()` with
an explicit 256 KB stack, the same pattern the plugin uses for its sampler.

**Don't fall off the end of `main()`.** After `SDL_Quit()` the process crashed with
a SIGSYS inside `libkernel.sprx` when launched through `LoadExec`. The app exits
with `sceSystemServiceLoadExec("exit", NULL)`, the same pattern Apollo Save Tool and
ItemzFlow use.

**Flush config on the way out.** Unsaved settings were lost on close until the app
started flushing the in-memory config before shutdown.

**Leave `SDL_INIT_JOYSTICK` off.** It contends with a separately opened `scePad`
handle for the same DualShock and `scePadReadState` reports stale buttons. Input
uses `scePad` directly.

**No SDL2_ttf.** It isn't a compiled library in the toolchain snapshot the app
targets, so text goes through FreeType (`-lSceFreeType`) in `text_render.c` with its
own glyph atlas.

**`sceHttp` does not follow redirects on its own.** See
[companion-app-updater](companion-app-updater.md).

**HTTP requests need a receive timeout.** Without `sceHttpSetRecvTimeOut()` a
connection that opened and then stalled mid response had no bound at all.

**Clang, not just GCC.** Something GCC's default GNU mode accepts can be a hard
error on the toolchain's clang. See
[ci-and-releases](ci-and-releases.md#a-bug-ci-found).
