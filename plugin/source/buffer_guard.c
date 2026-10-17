// Never read a display buffer without asking the kernel it is mapped and CPU-readable: Mortal Kombat 11's
// GPU-only buffers (prot 0x30) faulted the sampler. See docs/debugging/gpu-only-buffers.md. [docs/code-comments.md]

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "plugin_common.h"
#include "ambient_internal.h"

// Layout of OrbisKernelVirtualQueryInfo (OpenOrbis kernel.h).
// More: docs/code-comments.md (plugin/source/buffer_guard.c:37)
typedef struct {
    void    *start;
    void    *end;
    int64_t  offset;
    int32_t  protection;   // bit0 CPU_READ, bit1 CPU_WRITE, bit4 GPU_READ, bit5 GPU_WRITE
    int32_t  memoryType;
    uint8_t  flags;        // 1 byte: bit0 flexible, bit1 direct, bit2 stack, bit3 pooled, bit4 committed
                           // (confirmed from an MK11 capture: 0x12 = direct + committed, then the name follows)
    char     name[31];
    uint8_t  pad[64];
} GuardVqInfo;

#define GUARD_PROT_CPU_READ 0x1
#define GUARD_MAX_REGIONS   8   // contiguous regions walked per check

int32_t (*sceKernelVirtualQueryPtr)(const void *addr, int32_t flags, void *info, uint64_t infoSize) = NULL;
volatile uint32_t g_guardAvailable = 0;     // 1 once sceKernelVirtualQuery resolved
volatile uint32_t g_guardRejectCount = 0;   // passes skipped because the buffer was not readable
volatile uint64_t g_guardLastBadAddr = 0;   // last rejected address (for a debug capture)
volatile int32_t  g_guardLastRet = 0;       // last VirtualQuery return / reason code
// Region info from the most recent query that FAILED the check (debug packet only).
volatile uint64_t g_guardInfoStart = 0, g_guardInfoEnd = 0, g_guardInfoOffset = 0, g_guardInfoAddr = 0;
volatile int32_t  g_guardInfoProt = 0, g_guardInfoMemType = 0;
volatile uint32_t g_guardInfoFlags = 0;

volatile uint64_t g_readableLimit = 0; // bytes from the buffer base that are contiguously CPU-readable (see header)

// Walks contiguous mapped regions from addr, up to len bytes, and returns how many bytes are...
// More: docs/code-comments.md (plugin/source/buffer_guard.c:67)
uint64_t ambient_readable_bytes(uint64_t addr, uint64_t len)
{
    if (sceKernelVirtualQueryPtr == NULL) return len;
    if (addr == 0 || len == 0) return 0;

    uint64_t cur = addr, endAddr = addr + len;
    if (endAddr < addr) return 0; // wrapped
    for (int i = 0; i < GUARD_MAX_REGIONS && cur < endAddr; i++) {
        GuardVqInfo info;
        memset(&info, 0, sizeof(info));
        // flags 0: cur must lie INSIDE a mapped region (no "find next"), so an
        // unmapped hole is reported as an error instead of skipped.
        int32_t ret = sceKernelVirtualQueryPtr((const void *)cur, 0, &info, 72);
        if (ret != 0) { g_guardLastRet = ret; return cur - addr; }
        g_guardInfoAddr = cur; g_guardInfoStart = (uint64_t)info.start; g_guardInfoEnd = (uint64_t)info.end;
        g_guardInfoOffset = (uint64_t)info.offset; g_guardInfoProt = info.protection;
        g_guardInfoMemType = info.memoryType; g_guardInfoFlags = info.flags;
        if ((uint64_t)info.start > cur || (uint64_t)info.end <= cur) { g_guardLastRet = 1; return cur - addr; }
        if (!(info.protection & GUARD_PROT_CPU_READ))                { g_guardLastRet = 2; return cur - addr; }
        cur = (uint64_t)info.end;
    }
    g_guardLastRet = 0;
    return (cur >= endAddr ? endAddr : cur) - addr;
}

// Sampler-facing wrapper. Sets g_readableLimit for the pixel-read bounds checks and returns false only...
// More: docs/code-comments.md (plugin/source/buffer_guard.c:96)
bool ambient_buffer_readable(uint64_t bufferAddr)
{
    uint64_t n = ambient_readable_bytes(bufferAddr, BASE_PADDED_BUFFER_BYTES);
    g_readableLimit = n;
    if (n == 0) { g_guardRejectCount++; g_guardLastBadAddr = bufferAddr; return false; }
    return true;
}

// GPU-only buffers are made CPU-readable by three methods in order (gpu_only_remap, default 3). Keep the second view's
// memory type equal to the original's: a write-back view of write-combined memory goes stale. [docs/code-comments.md]

#define REMAP_MAX_ALIASES   6
#define REMAP_PROT_CPU_READ 0x1
#define REMAP_FLAG_DIRECT   0x2    // GuardVqInfo.flags bit1
#define REMAP_ALIGN         0x4000

int32_t (*sceKernelMapDirectMemoryPtr)(void **addr, uint64_t len, int32_t prot, int32_t flags, int64_t offset, uint64_t align) = NULL;
int32_t (*sceKernelMapDirectMemory2Ptr)(void **addr, uint64_t len, int32_t type, int32_t prot, int32_t flags, int64_t offset, uint64_t align) = NULL;
int32_t (*sceKernelMunmapPtr)(void *addr, uint64_t len) = NULL;
int32_t (*sceKernelMprotectPtr)(const void *addr, uint64_t len, int32_t prot) = NULL;

volatile uint32_t g_gpuOnlyRemap = 3;        // ini [compat] gpu_only_remap (optional): 0 off, 1 typed alias (Map2), 2 also plain Map, 3 (default) also mprotect
volatile uint32_t g_remapCreated = 0;        // buffers made readable (alias views created, or mprotects that stuck)
volatile uint32_t g_remapFailed = 0;         // create attempts that failed
volatile uint32_t g_remapPasses = 0;         // sampling passes served from an alias
volatile int32_t  g_remapLastRet = 0;        // last map return (0 ok)
volatile uint32_t g_remapLastMethod = 0;     // 1 = MapDirectMemory2 typed, 2 = plain MapDirectMemory, 3 = mprotect
volatile uint64_t g_remapLastAlias = 0;

typedef struct {
    uint64_t origStart;   // start of the game's region
    uint64_t origLen;     // end - start
    int64_t  offset;      // direct-memory offset of the region start
    uint64_t alias;       // our CPU_READ view of [offset, offset+origLen)
} RemapEntry;
static RemapEntry s_remap[REMAP_MAX_ALIASES];

// Regions every enabled method already failed on, so they are not retried (see the block above for the...
// More: docs/code-comments.md (plugin/source/buffer_guard.c:172)
#define REMAP_MAX_DENIED 8
static RemapEntry s_denied[REMAP_MAX_DENIED];
static uint32_t s_deniedNext = 0;
static bool remap_is_denied(uint64_t start, uint64_t len, int64_t offset)
{
    for (int i = 0; i < REMAP_MAX_DENIED; i++)
        if (s_denied[i].origStart == start && s_denied[i].origLen == len && s_denied[i].offset == offset) return true;
    return false;
}
static void remap_deny(uint64_t start, uint64_t len, int64_t offset)
{
    for (int i = 0; i < REMAP_MAX_DENIED; i++) {            // same address, changed region: replace its old record
        if (s_denied[i].origStart == start) { s_denied[i].origLen = len; s_denied[i].offset = offset; return; }
    }
    RemapEntry *d = &s_denied[s_deniedNext++ % REMAP_MAX_DENIED];
    d->origStart = start; d->origLen = len; d->offset = offset; d->alias = 0;
}

static void remap_drop(RemapEntry *e)
{
    if (e->alias != 0 && sceKernelMunmapPtr != NULL) sceKernelMunmapPtr((void *)e->alias, e->origLen);
    memset(e, 0, sizeof(*e));
}

// Drops aliases whose original mapping is gone, moved, or became CPU-readable on its own.
// More: docs/code-comments.md (plugin/source/buffer_guard.c:199)
static void remap_sweep(void)
{
    for (int i = 0; i < REMAP_MAX_ALIASES; i++) {
        RemapEntry *e = &s_remap[i];
        if (e->alias == 0) continue;
        GuardVqInfo info;
        memset(&info, 0, sizeof(info));
        int32_t ret = sceKernelVirtualQueryPtr((const void *)e->origStart, 0, &info, 72);
        if (ret != 0 || (uint64_t)info.start != e->origStart ||
            (uint64_t)info.end - (uint64_t)info.start != e->origLen ||
            info.offset != e->offset || (info.protection & GUARD_PROT_CPU_READ)) {
            remap_drop(e);
        }
    }
}

// Returns a CPU-readable address equivalent to bufferAddr, or 0. Only meaningful after the
// guard rejected bufferAddr for "not CPU-readable" (reason 2).
uint64_t ambient_remap_cpu_view(uint64_t bufferAddr)
{
    if (g_gpuOnlyRemap == 0 || sceKernelVirtualQueryPtr == NULL) return 0;
    bool canAlias = (sceKernelMunmapPtr != NULL) &&
                    (sceKernelMapDirectMemory2Ptr != NULL || (g_gpuOnlyRemap >= 2 && sceKernelMapDirectMemoryPtr != NULL));
    bool canProtect = (g_gpuOnlyRemap >= 3 && sceKernelMprotectPtr != NULL);
    if (!canAlias && !canProtect) return 0;

    static uint32_t s_calls = 0;
    if ((++s_calls % 30) == 0) remap_sweep();

    GuardVqInfo info;
    memset(&info, 0, sizeof(info));
    if (sceKernelVirtualQueryPtr((const void *)bufferAddr, 0, &info, 72) != 0) return 0;
    uint64_t start = (uint64_t)info.start, end = (uint64_t)info.end;
    if (start > bufferAddr || end <= bufferAddr) return 0;
    if (!(info.flags & REMAP_FLAG_DIRECT)) return 0;            // only direct memory can be aliased
    if (info.protection & GUARD_PROT_CPU_READ) return 0;        // already readable, nothing to do
    uint64_t len = end - start;
    if (len == 0 || len > 0x4000000ULL) return 0;               // sanity: display buffer regions are ~8 MB
    if (remap_is_denied(start, len, info.offset)) return 0;     // already refused, don't pay for another try

    RemapEntry *free_slot = NULL;
    for (int i = 0; i < REMAP_MAX_ALIASES; i++) {
        RemapEntry *e = &s_remap[i];
        if (e->alias == 0) { if (!free_slot) free_slot = e; continue; }
        if (e->origStart == start) {
            if (e->origLen == len && e->offset == info.offset) return e->alias + (bufferAddr - start);
            remap_drop(e);                                       // same address re-created elsewhere: stale
            if (!free_slot) free_slot = e;
        }
    }
    if (free_slot == NULL) { remap_drop(&s_remap[0]); free_slot = &s_remap[0]; } // full: recycle oldest

    void *view = NULL;
    int32_t ret = -1;
    uint32_t method = 0;
    if (canAlias && sceKernelMapDirectMemory2Ptr != NULL) {
        method = 1;
        ret = sceKernelMapDirectMemory2Ptr(&view, len, info.memoryType, REMAP_PROT_CPU_READ, 0, info.offset, REMAP_ALIGN);
    }
    if (canAlias && (ret != 0 || view == NULL) && g_gpuOnlyRemap >= 2 && sceKernelMapDirectMemoryPtr != NULL) {
        method = 2;
        view = NULL;
        ret = sceKernelMapDirectMemoryPtr(&view, len, REMAP_PROT_CPU_READ, 0, info.offset, REMAP_ALIGN);
    }
    if ((ret != 0 || view == NULL) && canProtect) {
        // Second mapping refused (EBUSY on MK11), or not possible.
        // More: docs/code-comments.md (plugin/source/buffer_guard.c:267)
        method = 3;
        int32_t pr = sceKernelMprotectPtr((const void *)start, len, info.protection | REMAP_PROT_CPU_READ);
        g_remapLastRet = pr; g_remapLastMethod = method;
        if (pr == 0) {
            GuardVqInfo chk; memset(&chk, 0, sizeof(chk));
            if (sceKernelVirtualQueryPtr((const void *)start, 0, &chk, 72) == 0 && (chk.protection & GUARD_PROT_CPU_READ)) {
                g_remapCreated++;
                return bufferAddr;                               // the original address is now readable itself
            }
            g_remapLastRet = -1000;                              // call succeeded but the bit did not stick
        }
        g_remapFailed++; remap_deny(start, len, info.offset);
        return 0;
    }
    g_remapLastRet = ret; g_remapLastMethod = method;
    if (ret != 0 || view == NULL) { g_remapFailed++; remap_deny(start, len, info.offset); return 0; }

    free_slot->origStart = start; free_slot->origLen = len; free_slot->offset = info.offset;
    free_slot->alias = (uint64_t)view;
    g_remapCreated++; g_remapLastAlias = (uint64_t)view;
    return free_slot->alias + (bufferAddr - start);
}

// Single entry point for the sampler and the debug probe: returns an address the CPU may read...
// More: docs/code-comments.md (plugin/source/buffer_guard.c:293)
uint64_t ambient_resolve_readable(uint64_t bufferAddr)
{
    if (bufferAddr == 0) return 0;
    if (ambient_buffer_readable(bufferAddr)) return bufferAddr;
    if (g_guardLastRet != 2) return 0;                           // unmapped etc.: nothing to alias
    uint64_t alias = ambient_remap_cpu_view(bufferAddr);
    if (alias != 0 && ambient_buffer_readable(alias)) { g_remapPasses++; return alias; }
    g_readableLimit = 0;
    return 0;
}
