// hooks.c -- part of ps4_ambient_light, split out of the original single-file main.c. Plugin metadata...
// More: docs/code-comments.md (plugin/source/hooks.c:1)

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "plugin_common.h"
#include "ambient_internal.h"

// --- Plugin metadata ---
attr_public const char *g_pluginName = "ps4_ambient_light";
attr_public const char *g_pluginDesc = "Live per-frame ambient light: detiles the real scanout buffer and streams zone colors to WLED";
attr_public const char *g_pluginAuth = "(null)";
attr_public uint32_t g_pluginVersion = 0x00000309; // v3.8 -> v3.9:
// g_pluginVersion and the v2.x-v3.x release notes that lived here are in CHANGELOG.md and docs/code-comments.md.
// More: docs/code-comments.md (plugin/source/hooks.c:26)

int32_t (*sceVideoOutRegisterBuffersPtr)(int32_t handle, int32_t startIndex,
                                          void *const *addresses, int32_t bufferNum,
                                          const OrbisVideoOutBufferAttribute *attribute);
int32_t (*sceGnmSubmitAndFlipCommandBuffersPtr)(uint32_t count, void *dcbGpuAddrs[],
                                                 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                 uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                 uint32_t displayBufferIndex, uint32_t flipMode,
                                                 int64_t flipArg);
// v3.1: second real flip entrypoint.
// More: docs/code-comments.md (plugin/source/hooks.c:219)
int32_t (*sceVideoOutSubmitFlipPtr)(int32_t handle, int32_t bufferIndex,
                                     int32_t flipMode, int64_t flipArg);
// v3.1: third real flip entrypoint, and the one that actually turned out to matter -- see the...
// More: docs/code-comments.md (plugin/source/hooks.c:225)
int32_t (*sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr)(uint32_t workload, uint32_t count,
                                                             void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes,
                                                             void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
                                                             uint32_t videoOutHandle, uint32_t displayBufferIndex,
                                                             uint32_t flipMode, int64_t flipArg);

HOOK_INIT(sceVideoOutRegisterBuffersPtr);
HOOK_INIT(sceGnmSubmitAndFlipCommandBuffersPtr);
HOOK_INIT(sceVideoOutSubmitFlipPtr);
HOOK_INIT(sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr);

#define MAX_TRACKED_BUFFERS 16
volatile uint64_t g_bufferAddrs[MAX_TRACKED_BUFFERS] = {0};
volatile int32_t  g_bufferCount = 0;
volatile uint32_t g_activeFormat = 0; // live format, NOT hardcoded
volatile int      g_haveValidFormat = 0;    // 0 until a registration event gives us a known format

// v3.1: diagnostic only, no pipeline effect -- what actually found the "never lights up" bug.
// More: docs/code-comments.md (plugin/source/hooks.c:247)
volatile uint32_t g_registerHookCallCount = 0;
volatile uint32_t g_flipHookCallCount = 0;
volatile uint32_t g_videoOutSubmitFlipHookCallCount = 0;
volatile uint32_t g_submitFlipPtrResolved = 0;          // set once in main.c, right after that hook's dlsym call
volatile uint32_t g_gnmForWorkloadHookCallCount = 0;
volatile uint32_t g_gnmForWorkloadPtrResolved = 0;      // set once in main.c, right after that hook's dlsym call

int32_t sceVideoOutRegisterBuffersPtr_hook(int32_t handle, int32_t startIndex,
                                            void *const *addresses, int32_t bufferNum,
                                            const OrbisVideoOutBufferAttribute *attribute)
{
    g_registerHookCallCount++; // v3.1: diagnostic only -- see its declaration above
    if (addresses != NULL && bufferNum > 0) {
        int32_t n = bufferNum > MAX_TRACKED_BUFFERS ? MAX_TRACKED_BUFFERS : bufferNum;
        for (int32_t i = 0; i < n; i++) {
            int32_t slot = startIndex + i;
            if (slot >= 0 && slot < MAX_TRACKED_BUFFERS) {
                g_bufferAddrs[slot] = (uint64_t)addresses[i];
            }
        }
        if (n > g_bufferCount) g_bufferCount = n;

        if (attribute != NULL) {
            uint32_t fmt = (uint32_t)attribute->format;
            bool wasValid = g_haveValidFormat;
            uint32_t prevFormat = g_activeFormat;
            if (fmt != prevFormat) g_pq8bitMode = 0; // v3.5: HDR toggled or a new title, decide again from the pixels
            g_activeFormat = fmt;
            g_haveValidFormat = (getUnpackFnForFormat(fmt) != NULL);
#if (__FINAL__) == 0
            // v2.2.1: diagnostic-only telemetry, added to answer a real live question (does this specific game's...
            // More: docs/code-comments.md (plugin/source/hooks.c:283)
            if (fmt != prevFormat || g_haveValidFormat != wasValid) {
                uint8_t fmtPacket[8];
                memcpy(fmtPacket + 0, &fmt, 4);
                uint32_t recognized = g_haveValidFormat ? 1u : 0u;
                memcpy(fmtPacket + 4, &recognized, 4);
                debug_send_raw(fmtPacket, sizeof(fmtPacket));
            }
#else
            (void)wasValid; (void)prevFormat; // avoid unused-variable warnings in release builds
#endif
        }
    }
    return HOOK_CONTINUE(sceVideoOutRegisterBuffersPtr,
                          int32_t(*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *),
                          handle, startIndex, addresses, bufferNum, attribute);
}

// v1.1: the flip hook itself only records which buffer is live now -- no sampling, no network I/O, no...
// More: docs/code-comments.md (plugin/source/hooks.c:322)
volatile uint32_t g_currentDisplayBufferIndex = 0xFFFFFFFFu; // sentinel: no flip seen yet

// Slot of the flip BEFORE the current one (sentinel until two different slots have been seen).
// More: docs/code-comments.md (plugin/source/hooks.c:328)
volatile uint32_t g_prevDisplayBufferIndex = 0xFFFFFFFFu;

// Slot of the flip before THAT one (two real flips back).
// More: docs/code-comments.md (plugin/source/hooks.c:336)
volatile uint32_t g_prevPrevDisplayBufferIndex = 0xFFFFFFFFu;

static inline void record_flip_index(uint32_t idx)
{
    uint32_t cur = g_currentDisplayBufferIndex;
    if (idx != cur) {
        g_prevPrevDisplayBufferIndex = g_prevDisplayBufferIndex;
        g_prevDisplayBufferIndex = cur; // may be the sentinel on the first flip; the sampler waits for history (see sample_thread.c)
        g_currentDisplayBufferIndex = idx;
    }
}

int32_t sceGnmSubmitAndFlipCommandBuffersPtr_hook(uint32_t count, void *dcbGpuAddrs[],
                                                   uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                   uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                   uint32_t displayBufferIndex, uint32_t flipMode,
                                                   int64_t flipArg)
{
    g_flipHookCallCount++; // v3.1: diagnostic only -- see its declaration above
    record_flip_index(displayBufferIndex); // near-zero cost

    return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffersPtr,
                          int32_t(*)(uint32_t, void **, uint32_t *, void **, uint32_t *, uint32_t, uint32_t, uint32_t, int64_t),
                          count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes,
                          videoOutHandle, displayBufferIndex, flipMode, flipArg);
}

// v3.1: second flip entrypoint -- see its pointer declaration above for why this is hooked.
// More: docs/code-comments.md (plugin/source/hooks.c:370)
int32_t sceVideoOutSubmitFlipPtr_hook(int32_t handle, int32_t bufferIndex,
                                       int32_t flipMode, int64_t flipArg)
{
    g_videoOutSubmitFlipHookCallCount++; // diagnostic only -- see its declaration above
    record_flip_index((uint32_t)bufferIndex); // (uint32_t)(-1) == the existing sentinel

    return HOOK_CONTINUE(sceVideoOutSubmitFlipPtr,
                          int32_t(*)(int32_t, int32_t, int32_t, int64_t),
                          handle, bufferIndex, flipMode, flipArg);
}

// v3.1: third flip entrypoint -- the one that actually turned out to matter, see its pointer...
// More: docs/code-comments.md (plugin/source/hooks.c:385)
int32_t sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr_hook(uint32_t workload, uint32_t count,
                                                               void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes,
                                                               void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
                                                               uint32_t videoOutHandle, uint32_t displayBufferIndex,
                                                               uint32_t flipMode, int64_t flipArg)
{
    g_gnmForWorkloadHookCallCount++; // diagnostic only -- see its declaration above
    record_flip_index(displayBufferIndex); // near-zero cost

    return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr,
                          int32_t(*)(uint32_t, uint32_t, void **, uint32_t *, void **, uint32_t *, uint32_t, uint32_t, uint32_t, int64_t),
                          workload, count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes,
                          videoOutHandle, displayBufferIndex, flipMode, flipArg);
}

