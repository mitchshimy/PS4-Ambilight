// shell_hooks.c -- the two VideoOut hooks that count the compositor's buffer registrations and flips, and
// the code that installs them. The counters are plain volatile values: diagnostics, torn reads are fine.
#include "shell_internal.h"

// Copy 8 bytes of code at p into out (reads are plain loads; code pages are
// readable, as first_byte() already relies on).
static void read8(void *p, uint8_t out[8])
{
    for (int i = 0; i < 8; i++) out[i] = ((volatile uint8_t *)p)[i];
}

static void print8(const char *label, const uint8_t b[8])
{
    printf(TAG "   %s %02x %02x %02x %02x %02x %02x %02x %02x\n", label,
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
}

// ---------------------------------------------------------------------
// Shared state, written by hooks, read by the reporter thread. Plain
// volatile counters: diagnostics only, torn reads are acceptable.
// ---------------------------------------------------------------------

volatile uint32_t g_regCount  = 0;
volatile uint32_t g_flipCount = 0;

static volatile int32_t  g_lastFlipHandle = -1;

volatile int32_t  g_lastFlipIndex  = -1;

static volatile int32_t  g_lastFlipMode   = -1;

struct RegEvent {
    int32_t  handle, startIndex, bufferNum;
    uint32_t format, tmode, width, height, pixelPitch;
    uint64_t addr[MAX_ADDRS];
};
static struct RegEvent   g_ring[RING_SIZE];
static volatile uint32_t g_ringHead = 0;   // total events recorded

int32_t (*sceVideoOutRegisterBuffersPtr)(int32_t handle, int32_t startIndex,
                                          void *const *addresses, int32_t bufferNum,
                                          const OrbisVideoOutBufferAttribute *attribute);
int32_t (*sceVideoOutSubmitFlipPtr)(int32_t handle, int32_t bufferIndex,
                                     int32_t flipMode, int64_t flipArg);

HOOK_INIT(sceVideoOutRegisterBuffersPtr);
HOOK_INIT(sceVideoOutSubmitFlipPtr);

int32_t sceVideoOutRegisterBuffersPtr_hook(int32_t handle, int32_t startIndex,
                                            void *const *addresses, int32_t bufferNum,
                                            const OrbisVideoOutBufferAttribute *attribute)
{
    uint32_t slot = __sync_fetch_and_add(&g_ringHead, 1) % RING_SIZE;
    struct RegEvent *e = &g_ring[slot];
    e->handle = handle; e->startIndex = startIndex; e->bufferNum = bufferNum;
    if (attribute != NULL) {
        e->format = (uint32_t)attribute->format;
        e->tmode = (uint32_t)attribute->tmode;
        e->width = (uint32_t)attribute->width;
        e->height = (uint32_t)attribute->height;
        e->pixelPitch = (uint32_t)attribute->pixelPitch;
    } else {
        e->format = e->tmode = e->width = e->height = e->pixelPitch = 0;
    }
    for (int i = 0; i < MAX_ADDRS; i++) {
        e->addr[i] = (addresses != NULL && i < bufferNum) ? (uint64_t)addresses[i] : 0;
    }
    __sync_fetch_and_add(&g_regCount, 1);

    return HOOK_CONTINUE(sceVideoOutRegisterBuffersPtr,
                          int32_t(*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *),
                          handle, startIndex, addresses, bufferNum, attribute);
}

int32_t sceVideoOutSubmitFlipPtr_hook(int32_t handle, int32_t bufferIndex,
                                       int32_t flipMode, int64_t flipArg)
{
    g_lastFlipHandle = handle;
    g_lastFlipIndex = bufferIndex;
    g_lastFlipMode = flipMode;
    __sync_fetch_and_add(&g_flipCount, 1);

    return HOOK_CONTINUE(sceVideoOutSubmitFlipPtr,
                          int32_t(*)(int32_t, int32_t, int32_t, int64_t),
                          handle, bufferIndex, flipMode, flipArg);
}

// Log what a hook did to its target. Returns 1 if the target bytes changed.
static int report_patch(const char *what, void *target, void *hookfn,
                        const uint8_t before[8])
{
    uint8_t after[8];
    read8(target, after);
    int changed = memcmp(before, after, 8) != 0;
    int64_t delta = (int64_t)((uintptr_t)hookfn - (uintptr_t)target);
    printf(TAG " patch %s: target=%p hookfn=%p delta=%lld bytes_changed=%s%s\n", what,
           target, hookfn, (long long)delta, changed ? "YES" : "NO",
           (delta > 0x7fffffffLL || delta < -0x80000000LL) ? " (hook farther than 2GB)" : "");
    print8("before:", before);
    print8("after: ", after);
    return changed;
}

// [v10] HOOK32 patches a 32-bit relative jump, which cannot reach a hook function
// more than 2 GB from its target. v3-v8 only LOGGED that condition (it never occurred
// in the shell). In the compositor an unreachable hook would crash it on the first
// call, so v10 refuses to patch instead. Returns 1 if the hook is reachable.
static int hook_reachable(const char *what, void *target, void *hookfn)
{
    int64_t delta = (int64_t)((uintptr_t)hookfn - (uintptr_t)target);
    if (delta > 0x7fffffffLL || delta < -0x80000000LL) {
        printf(TAG " SKIP patch %s: hook function %p is %lld bytes from target %p (beyond 32-bit jump range); NOT patching\n",
               what, hookfn, (long long)delta, target);
        return 0;
    }
    return 1;
}

// Installs both hooks, each with a before/after byte check, and says how many took. A target that is a
// forwarding stub (first byte 0xE9) or farther than 2 GB from its hook is left alone. The patched counts go
// back to module_start(), which stops if neither hook went in.
// (Moved out of module_start() unchanged; the hook functions, HOOK_INIT and HOOK32 have to share a file.)
void shell_hooks_install(void *pReg, void *pFlip, int *outPatchedReg, int *outPatchedFlip)
{

    // 4. Hooks, each with a before/after byte check. Skip forwarding stubs (0xE9).
    sceVideoOutRegisterBuffersPtr = (int32_t (*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *))pReg;
    sceVideoOutSubmitFlipPtr = (int32_t (*)(int32_t, int32_t, int32_t, int64_t))pFlip;

    int patchedReg = 0, patchedFlip = 0;
    uint8_t b[8];

    if (pReg != NULL && first_byte(pReg) != 0xE9 &&
        hook_reachable("RegisterBuffers", pReg, (void *)sceVideoOutRegisterBuffersPtr_hook)) {
        read8(pReg, b);
        printf(TAG " about to patch sceVideoOutRegisterBuffers at %p (a hang or black screen right after this line = the patch itself faulted)\n", pReg);
        HOOK32(sceVideoOutRegisterBuffersPtr);
        patchedReg = report_patch("RegisterBuffers", pReg, (void *)sceVideoOutRegisterBuffersPtr_hook, b);
    }
    if (pFlip != NULL && first_byte(pFlip) != 0xE9 &&
        hook_reachable("SubmitFlip", pFlip, (void *)sceVideoOutSubmitFlipPtr_hook)) {
        read8(pFlip, b);
        printf(TAG " about to patch sceVideoOutSubmitFlip at %p (a hang or black screen right after this line = the patch itself faulted)\n", pFlip);
        HOOK32(sceVideoOutSubmitFlipPtr);
        patchedFlip = report_patch("SubmitFlip", pFlip, (void *)sceVideoOutSubmitFlipPtr_hook, b);
    }

    *outPatchedReg = patchedReg;
    *outPatchedFlip = patchedFlip;
}

#if DO_CONTROL_CALLS
// Calls the two patched entry points once with an invalid handle, to see the hooks fire, then puts the
// counters and the ring back to zero. (Moved out of module_start() unchanged.)
void shell_hooks_control_calls(void *pReg, void *pFlip, int patchedReg, int patchedFlip)
{

    // 5. Positive control: call the PATCHED entry points (raw resolved addresses,
    // not the trampoline pointers) with an invalid handle (-1). Assumed, not
    // verified in the compositor: rejected at argument validation, as in the shell.
    if (patchedFlip && pFlip != NULL) {
        uint32_t c0 = g_flipCount;
        printf(TAG " control: about to call SubmitFlip(handle=-1) via %p\n", pFlip);
        int32_t r = ((int32_t (*)(int32_t, int32_t, int32_t, int64_t))pFlip)(-1, 0, 0, 0);
        printf(TAG " control: SubmitFlip returned 0x%x, flip count %u -> %u => hook %s\n",
               (unsigned)r, (unsigned)c0, (unsigned)g_flipCount,
               (g_flipCount > c0) ? "FIRES" : "DID NOT FIRE");
    }
    if (patchedReg && pReg != NULL) {
        uint32_t c0 = g_regCount;
        printf(TAG " control: about to call RegisterBuffers(handle=-1) via %p\n", pReg);
        int32_t r = ((int32_t (*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *))pReg)(-1, 0, NULL, 0, NULL);
        printf(TAG " control: RegisterBuffers returned 0x%x, register count %u -> %u => hook %s\n",
               (unsigned)r, (unsigned)c0, (unsigned)g_regCount,
               (g_regCount > c0) ? "FIRES" : "DID NOT FIRE");
    }
    // Remove the control calls from the counts and the ring.
    g_flipCount = 0; g_regCount = 0; g_ringHead = 0;
    g_lastFlipHandle = -1; g_lastFlipIndex = -1; g_lastFlipMode = -1;
    printf(TAG " control done, counters reset to 0\n");

}
#endif
