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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../plugin/source/buffer_guard.c"

// ---- fake kernel ------------------------------------------------------------------------

// Must match GuardVqInfo in buffer_guard.c field for field (the real struct is only needed up to
// the flags byte).
typedef struct { void *start, *end; int64_t offset; int32_t protection, memoryType; uint8_t flags; char name[31]; uint8_t pad[64]; } FakeInfo;
typedef struct { uint64_t start, end; int64_t offset; int prot, type, flags; } Region;

static Region R[32];
static int nR;
static int vqCalls, map2Calls, mapCalls, mprotectCalls, munmapCalls;
static int32_t map2Ret, mapRet, mprotectRet;   // what the fake kernel answers; 0 = accept
static bool mprotectSticks;                     // false: returns 0 but leaves the protection alone
static uint64_t nextView = 0x7000000000ULL;

static int32_t fakeVq(const void *a, int32_t flags, void *info, uint64_t size)
{
    (void)flags; (void)size; vqCalls++;
    uint64_t x = (uint64_t)a;
    for (int k = 0; k < nR; k++) if (R[k].start <= x && x < R[k].end) {
        FakeInfo *i = info;
        i->start = (void *)R[k].start; i->end = (void *)R[k].end; i->offset = R[k].offset;
        i->protection = R[k].prot; i->memoryType = R[k].type; i->flags = (uint8_t)R[k].flags;
        return 0;
    }
    return (int32_t)0x80020016; // EINVAL: not mapped
}
static int32_t fakeMap2(void **a, uint64_t len, int32_t type, int32_t prot, int32_t fl, int64_t off, uint64_t al)
{
    (void)fl; (void)al; map2Calls++;
    if (map2Ret) return map2Ret;
    *a = (void *)nextView; R[nR++] = (Region){nextView, nextView + len, off, prot, type, 0x12}; nextView += 0x1000000; return 0;
}
static int32_t fakeMap(void **a, uint64_t len, int32_t prot, int32_t fl, int64_t off, uint64_t al)
{
    (void)fl; (void)al; mapCalls++;
    if (mapRet) return mapRet;
    *a = (void *)nextView; R[nR++] = (Region){nextView, nextView + len, off, prot, 0, 0x12}; nextView += 0x1000000; return 0;
}
static int32_t fakeMunmap(void *a, uint64_t len)
{
    (void)len; munmapCalls++;
    for (int k = 0; k < nR; k++) if (R[k].start == (uint64_t)a) { R[k] = R[--nR]; break; }
    return 0;
}
static int32_t fakeMprotect(const void *a, uint64_t len, int32_t prot)
{
    (void)len; mprotectCalls++;
    if (mprotectRet) return mprotectRet;
    if (mprotectSticks) for (int k = 0; k < nR; k++) if (R[k].start == (uint64_t)a) R[k].prot = prot;
    return 0;
}

// ---- helpers ----------------------------------------------------------------------------

static int passed, failed;
#define CHECK(cond, ...) do { if (cond) passed++; else { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define MK11_LEN   0x7EC000ULL
#define FULL_LEN   BASE_PADDED_BUFFER_BYTES

static void reset(uint32_t mode)
{
    memset(R, 0, sizeof(R)); nR = 0;
    vqCalls = map2Calls = mapCalls = mprotectCalls = munmapCalls = 0;
    map2Ret = mapRet = mprotectRet = 0; mprotectSticks = true;
    sceKernelVirtualQueryPtr = fakeVq; sceKernelMapDirectMemory2Ptr = fakeMap2; sceKernelMapDirectMemoryPtr = fakeMap;
    sceKernelMunmapPtr = fakeMunmap; sceKernelMprotectPtr = fakeMprotect;
    g_gpuOnlyRemap = mode; g_readableLimit = 0;
    memset(s_remap, 0, sizeof(s_remap)); memset(s_denied, 0, sizeof(s_denied)); s_deniedNext = 0;
    g_remapCreated = g_remapFailed = g_remapPasses = 0;
}
static void addRegion(uint64_t start, uint64_t len, int64_t off, int prot, int type, int flags)
{
    R[nR++] = (Region){start, start + len, off, prot, type, flags};
}

int main(void)
{
    const uint64_t B = 0x49ff608000ULL;   // an MK11 slot address from a capture
    uint64_t r;

    // 0. Default. The ini key is optional, so the code default is what ships.
    CHECK(g_gpuOnlyRemap == 3, "default gpu_only_remap is %u, want 3", g_gpuOnlyRemap);

    // 1. Fails OPEN when sceKernelVirtualQuery could not be resolved (other firmware): the old
    //    behavior, full ceiling, no remap attempt.
    reset(3); sceKernelVirtualQueryPtr = NULL;
    r = ambient_resolve_readable(B);
    CHECK(r == B && g_readableLimit == FULL_LEN, "fail-open: r=%#llx limit=%#llx", (unsigned long long)r, (unsigned long long)g_readableLimit);

    // 2. A normal CPU-readable, full-size buffer (what every working title looks like): returned
    //    unchanged, full limit, no remap call of any kind. This is the "no regression" case.
    reset(3); addRegion(B, FULL_LEN, 0x1000, 0x33, 0, 0x12);
    r = ambient_resolve_readable(B);
    CHECK(r == B && g_readableLimit == FULL_LEN, "readable: r=%#llx limit=%#llx", (unsigned long long)r, (unsigned long long)g_readableLimit);
    CHECK(map2Calls + mapCalls + mprotectCalls + munmapCalls == 0, "readable buffer touched the remap path");

    // 3. Readable but SHORTER than the ceiling, hole after it: still used, limit is what is there.
    //    (v3.9 first demanded the whole ceiling and would have rejected this outright.)
    reset(3); addRegion(B, MK11_LEN, 0x1000, 0x33, 0, 0x12);
    r = ambient_resolve_readable(B);
    CHECK(r == B && g_readableLimit == MK11_LEN, "short: r=%#llx limit=%#llx", (unsigned long long)r, (unsigned long long)g_readableLimit);

    // 4. Two contiguous readable regions covering the ceiling: the walk follows them.
    reset(3); addRegion(B, 0x400000, 0x1000, 0x33, 0, 0x12); addRegion(B + 0x400000, FULL_LEN - 0x400000, 0x401000, 0x33, 0, 0x12);
    r = ambient_resolve_readable(B);
    CHECK(r == B && g_readableLimit == FULL_LEN, "contiguous: limit=%#llx", (unsigned long long)g_readableLimit);

    // 5. Unmapped address: unreadable, and nothing to remap.
    reset(3);
    r = ambient_resolve_readable(0x11110000ULL);
    CHECK(r == 0 && g_readableLimit == 0 && map2Calls + mprotectCalls == 0, "unmapped: r=%#llx", (unsigned long long)r);

    // 6. MK11 as captured, default mode 3. Kernel refuses the second mapping (EBUSY) and accepts the
    //    mprotect (mode 3 tries the plain map too, also refused). The ORIGINAL address is returned, readable, with the region's real length. The
    //    next pass sees a readable buffer and costs no further calls.
    reset(3); map2Ret = mapRet = (int32_t)0x80020010; addRegion(B, MK11_LEN, 0x64a08000, 0x30, 3, 0x12);
    r = ambient_resolve_readable(B);
    CHECK(r == B && g_readableLimit == MK11_LEN, "mk11: r=%#llx limit=%#llx", (unsigned long long)r, (unsigned long long)g_readableLimit);
    CHECK(map2Calls == 1 && mapCalls == 1 && mprotectCalls == 1, "mk11: map2 %d map %d mprotect %d, want 1 1 1", map2Calls, mapCalls, mprotectCalls);
    CHECK(R[0].prot == 0x31, "mk11: protection is %#x, want 0x31 (GPU bits kept, CPU_READ added)", R[0].prot);
    CHECK(R[0].type == 3, "mk11: memory type changed to %d", R[0].type);
    r = ambient_resolve_readable(B);
    CHECK(r == B && map2Calls == 1 && mapCalls == 1 && mprotectCalls == 1, "mk11 second pass made new calls");
    CHECK(g_remapCreated == 1 && g_remapLastMethod == 3, "mk11: created %u method %u", g_remapCreated, g_remapLastMethod);

    // 7. Everything refused. The v3.9 first build retried this every pass: 435 refusals in about
    //    20 s at about 6 ms each. Each method must be tried exactly once per region.
    reset(3); map2Ret = (int32_t)0x80020010; mapRet = (int32_t)0x80020010; mprotectRet = (int32_t)0x8002000D;
    addRegion(B, MK11_LEN, 0x64a08000, 0x30, 3, 0x12);
    for (int i = 0; i < 100; i++) r = ambient_resolve_readable(B);
    CHECK(r == 0, "refused: r=%#llx", (unsigned long long)r);
    CHECK(map2Calls == 1 && mapCalls == 1 && mprotectCalls == 1, "refused x100: map2 %d map %d mprotect %d, want 1 1 1", map2Calls, mapCalls, mprotectCalls);
    CHECK(g_remapFailed == 1, "refused: failed count %u, want 1", g_remapFailed);
    //    The same address re-created with a different offset is a new region and gets a new try.
    R[0].offset = 0x65208000;
    ambient_resolve_readable(B);
    CHECK(map2Calls == 2 && mprotectCalls == 2, "re-created region not retried: map2 %d mprotect %d", map2Calls, mprotectCalls);

    // 8. mprotect returns success but the CPU bit does not stick: verified with a fresh query, so
    //    it is treated as a failure, not trusted.
    reset(3); map2Ret = mapRet = (int32_t)0x80020010; mprotectSticks = false; addRegion(B, MK11_LEN, 0x64a08000, 0x30, 3, 0x12);
    r = ambient_resolve_readable(B);
    CHECK(r == 0 && g_remapLastRet == -1000, "not-sticking mprotect: r=%#llx lastRet=%d", (unsigned long long)r, g_remapLastRet);

    // 9. Modes. 1 never uses mprotect, 0 does nothing at all, 2 adds the untyped map.
    reset(1); map2Ret = mapRet = (int32_t)0x80020010; addRegion(B, MK11_LEN, 0x64a08000, 0x30, 3, 0x12);
    r = ambient_resolve_readable(B);
    CHECK(r == 0 && mprotectCalls == 0 && mapCalls == 0, "mode 1: r=%#llx mprotect %d map %d", (unsigned long long)r, mprotectCalls, mapCalls);
    reset(0); addRegion(B, MK11_LEN, 0x64a08000, 0x30, 3, 0x12);
    r = ambient_resolve_readable(B);
    CHECK(r == 0 && map2Calls + mapCalls + mprotectCalls == 0, "mode 0 made calls");
    reset(2); map2Ret = (int32_t)0x80020010; addRegion(B, MK11_LEN, 0x64a08000, 0x30, 3, 0x12);
    r = ambient_resolve_readable(B);
    CHECK(r != 0 && r != B && mapCalls == 1 && mprotectCalls == 0, "mode 2: r=%#llx map %d mprotect %d", (unsigned long long)r, mapCalls, mprotectCalls);
    //    Mode 3 with no sceKernelMprotect resolved: falls back to the alias methods, no crash.
    reset(3); sceKernelMprotectPtr = NULL; map2Ret = (int32_t)0x80020010; mapRet = (int32_t)0x80020010; addRegion(B, MK11_LEN, 0x64a08000, 0x30, 3, 0x12);
    r = ambient_resolve_readable(B);
    CHECK(r == 0 && mprotectCalls == 0, "mode 3 without mprotect resolved: r=%#llx", (unsigned long long)r);

    // 10. GPU-only but NOT direct memory: cannot be aliased or protected this way; left alone.
    reset(3); addRegion(B, MK11_LEN, 0, 0x30, 3, 0x01);
    r = ambient_resolve_readable(B);
    CHECK(r == 0 && map2Calls + mapCalls + mprotectCalls == 0, "non-direct: r=%#llx calls %d", (unsigned long long)r, map2Calls + mapCalls + mprotectCalls);

    // 11. If a kernel ever DOES accept the typed second view: it is created with the original's
    //     memory type, returned as an offset into the view, reused on the next pass, and dropped
    //     once the game's mapping changes (checked about once per 30 lookups).
    reset(3); addRegion(B, MK11_LEN, 0x64a08000, 0x30, 3, 0x12);
    r = ambient_resolve_readable(B + 0x1000);
    CHECK(r != 0 && r != B + 0x1000 && (r & 0xFFF) == 0, "alias: r=%#llx", (unsigned long long)r);
    CHECK(R[nR - 1].type == 3 && R[nR - 1].prot == 0x1, "alias: type %d prot %#x, want 3 and 0x1", R[nR - 1].type, R[nR - 1].prot);
    uint64_t r2 = ambient_resolve_readable(B + 0x1000);
    CHECK(r2 == r && map2Calls == 1, "alias not reused: map2 %d", map2Calls);
    R[0].offset = 0x65208000;                                   // the game re-created its buffer
    for (int i = 0; i < 40; i++) ambient_resolve_readable(B);
    CHECK(munmapCalls >= 1, "stale alias never unmapped");

    printf("%d checks passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
