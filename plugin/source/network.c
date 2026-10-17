// network.c -- part of ps4_ambient_light, split out of the original single-file main.c. WLED DDP...
// More: docs/code-comments.md (plugin/source/network.c:1)

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
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
#include "output_activity.h"

// --- WLED DDP transport (identical pattern used throughout this project) --- v2.0: WLED host is now...
// More: docs/code-comments.md (plugin/source/network.c:23)
#define WLED_PORT       4048
#define DDP_HEADER_SIZE 10

// Compile-time cap for zone-array/DDP-packet-buffer sizing.
// More: docs/code-comments.md (plugin/source/network.c:32)
#define MAX_TOTAL_ZONES 512

int g_wledSockfd = -1; // one socket, held open -- avoid a socket()/close() per send at frame rate
volatile uint32_t g_wledOutputCount = 0; // observed by state.c to arbitrate ownership with shell-feed
volatile int      g_shellFeedAlive = 1;  // 1 until state.c's thread has evidence the shell feed is not running; read by the startup hold in wled_send_rgb_zones

static int wled_ensure_socket(void)
{
    if (g_wledSockfd >= 0) return g_wledSockfd;
    g_wledSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_wledSockfd >= 0) {
        // UDP sendto() essentially never blocks (no handshake/ACK), but "essentially never" isn't "never" -- a...
        // More: docs/code-comments.md (plugin/source/network.c:48)
        struct timeval sndTimeout = { .tv_sec = 0, .tv_usec = 5000 }; // 5ms
        setsockopt(g_wledSockfd, SOL_SOCKET, SO_SNDTIMEO, &sndTimeout, sizeof(sndTimeout));
    }
    return g_wledSockfd;
}

// --- Debug/telemetry channel, step 2: a real number for ambient_sample_thread's own loop time, not...
// More: docs/code-comments.md (plugin/source/network.c:62)
#define DEBUG_IP "192.168.2.117"   // v2.2.5: NO LONGER USED as a send target -- kept only as
                                    // a comment/reference of what this used to be hardcoded to. debug_send_raw now requires g_config.devIp...
                                    // More: docs/code-comments.md (plugin/source/network.c:73)

// Event-only foreground-handoff diagnostics.
// More: docs/code-comments.md (plugin/source/network.c:78)
void ambient_handoff_diag(const char *fmt, ...)
{
    char payload[192];
    int prefix = snprintf(payload, sizeof(payload), "HOF1 ");
    if (prefix < 0 || (size_t)prefix >= sizeof(payload)) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(payload + prefix, sizeof(payload) - (size_t)prefix, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    size_t used = (size_t)prefix + (size_t)n;
    if (used >= sizeof(payload)) used = sizeof(payload) - 1;
    debug_send_raw((const uint8_t *)payload, (int)used);
}

void debug_send_raw(const uint8_t *data, int len)
{
    // v2.2.5: single choke point for ALL debug/diagnostic UDP sends in this file (the v2.2.1 format-change...
    // More: docs/code-comments.md (plugin/source/network.c:98)
    if (!g_config.devLoggingEnabled || g_config.devIp[0] == '\0') return;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return;

    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(WLED_PORT);
    if (inet_pton(AF_INET, g_config.devIp, &destAddr.sin_addr) != 1) {
        close(sockfd);
        return;
    }

    // The debug packet cap must stay above the largest packet (the config-reload packet is 68 bytes):
    // a 64-byte cap silently dropped its last field. [docs/code-comments.md]
    uint8_t packet[DDP_HEADER_SIZE + 192];
    if (len > 192) len = 192; // was 128: the 132-byte FLK1 probe packet (sample_thread.c) would have been truncated, same bug class as the v2.7.6 zone-echo truncation
    packet[0] = 0x40 | 0x01;
    packet[1] = 0;
    packet[2] = 0x0B;
    packet[3] = 0x01;
    packet[4] = 0; packet[5] = 0; packet[6] = 0; packet[7] = 0;
    packet[8] = (uint8_t)((len >> 8) & 0xFF);
    packet[9] = (uint8_t)(len & 0xFF);
    memcpy(packet + DDP_HEADER_SIZE, data, len);

    sendto(sockfd, packet, DDP_HEADER_SIZE + len, 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
    close(sockfd);
}

// sceSystemServiceGetStatus is resolved by name at runtime (the SDK header declares a bare stub) and its status buffer
// is padded generously: the real struct size is unverified, so a longer write must land in padding, not the stack. [docs/code-comments.md]

int32_t (*sceSystemServiceGetStatusPtr)(AmbientSystemServiceStatus *status);

// v2.4: last known foreground/background state, checked once per ambient_sample_thread iteration...
// More: docs/code-comments.md (plugin/source/network.c:205)
bool g_isBackgrounded = false;

void relay_send_external_source(bool active)
{
    // relay_send_external_source() must not gate on relaySignalEnabled itself (the true->false "off" would be
    // swallowed); every call site guards on it instead. [docs/code-comments.md]
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return;

    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(g_config.relayPort);
    if (inet_pton(AF_INET, g_config.relayHost, &destAddr.sin_addr) != 1) {
        close(sockfd);
        return;
    }

    const char *payload = active ? "on" : "off";
    sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
    close(sockfd);
}

// Send the relay "off" only if the shell feed is not running (g_shellFeedAlive): while it drives the strip,
// an "off" would bring the relay on over its colours. [docs/code-comments.md]
void relay_release_external_source(void)
{
    if (g_shellFeedAlive) return;
    relay_send_external_source(false);
}

// One timing-window packet, 24 bytes -- deliberately a length that collides with neither...
// More: docs/code-comments.md (plugin/source/network.c:262)
void send_timing_packet(uint32_t minUs, uint32_t maxUs, uint32_t avgUs,
                                uint32_t sampleCount, uint32_t overBudgetCount,
                                uint32_t windowId, uint32_t minCpu,
                                uint32_t maxCpu, uint32_t migrationCount)
{
    uint8_t packet[36];
    memcpy(packet +  0, &minUs,          4);
    memcpy(packet +  4, &maxUs,          4);
    memcpy(packet +  8, &avgUs,          4);
    memcpy(packet + 12, &sampleCount,    4);
    memcpy(packet + 16, &overBudgetCount,4);
    memcpy(packet + 20, &windowId,       4);
    memcpy(packet + 24, &minCpu,         4);
    memcpy(packet + 28, &maxCpu,         4);
    memcpy(packet + 32, &migrationCount, 4);
    debug_send_raw(packet, sizeof(packet));
}

// v3.1: what actually found "this title never lights up" -- see hooks.c's g_pluginVersion comment for...
// More: docs/code-comments.md (plugin/source/network.c:304)
void send_flip_diag_packet(uint32_t registerHookCallCount, uint32_t flipHookCallCount,
                            uint32_t displayBufferIndex, uint64_t liveBufferAddr,
                            uint32_t haveValidFormat, uint32_t activeFormat,
                            uint32_t videoOutSubmitFlipHookCallCount,
                            uint32_t submitFlipPtrResolved,
                            uint32_t gnmForWorkloadHookCallCount,
                            uint32_t gnmForWorkloadPtrResolved)
{
    uint8_t packet[44];
    memcpy(packet +  0, &registerHookCallCount, 4);
    memcpy(packet +  4, &flipHookCallCount,     4);
    memcpy(packet +  8, &displayBufferIndex,    4);
    memcpy(packet + 12, &liveBufferAddr,        8);
    memcpy(packet + 20, &haveValidFormat,       4);
    memcpy(packet + 24, &activeFormat,          4);
    memcpy(packet + 28, &videoOutSubmitFlipHookCallCount, 4);
    memcpy(packet + 32, &submitFlipPtrResolved, 4);
    memcpy(packet + 36, &gnmForWorkloadHookCallCount, 4);
    memcpy(packet + 40, &gnmForWorkloadPtrResolved, 4);
    debug_send_raw(packet, sizeof(packet));
}

// v2.1.3: peek at the first bytes of AMBIENT_CONFIG_PATH from the plugin's own point of view, so a...
// More: docs/code-comments.md (plugin/source/network.c:343)
void ambient_read_content_preview(uint8_t *out, size_t previewLen)
{
    memset(out, 0, previewLen);
    int32_t fd = sceKernelOpen(AMBIENT_CONFIG_PATH, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return; // leave zero-filled -- open failure is itself informative (event/stat_errno already cover it)
    ssize_t nread = sceKernelRead(fd, out, previewLen);
    sceKernelClose(fd);
    if (nread < 0) memset(out, 0, previewLen); // read failed -- keep the zero-fill, don't show garbage
}

// Config-reload diagnostic packet, added to actually SEE why live reload isn't firing on real hardware...
// More: docs/code-comments.md (plugin/source/network.c:364)
void send_config_reload_debug_packet(uint32_t event, uint32_t statErrno,
                                             uint64_t curMtime, uint64_t lastMtime,
                                             uint64_t curSize, uint64_t lastSize,
                                             uint32_t checkCount,
                                             const uint8_t *contentPreview,
                                             uint32_t curHash, uint32_t lastHash)
{
    uint8_t packet[44 + CONFIG_DEBUG_PREVIEW_LEN + 8];
    memcpy(packet +  0, &event,      4);
    memcpy(packet +  4, &statErrno,  4);
    memcpy(packet +  8, &curMtime,   8);
    memcpy(packet + 16, &lastMtime,  8);
    memcpy(packet + 24, &curSize,    8);
    memcpy(packet + 32, &lastSize,   8);
    memcpy(packet + 40, &checkCount, 4);
    memcpy(packet + 44, contentPreview, CONFIG_DEBUG_PREVIEW_LEN);
    memcpy(packet + 60, &curHash,    4);
    memcpy(packet + 64, &lastHash,   4);
    debug_send_raw(packet, sizeof(packet));
}

// v2.0: MAX_ZONES is just an alias for MAX_TOTAL_ZONES (defined above, near DDP_HEADER_SIZE) -- kept...
// More: docs/code-comments.md (plugin/source/network.c:446)
#define MAX_ZONES MAX_TOTAL_ZONES

// v2.8: WLED heartbeat. WLED falls back to its own realtime-timeout color after a gap in packets...
// More: docs/code-comments.md (plugin/source/network.c:452)
#define WLED_HEARTBEAT_INTERVAL_US 1000000u // 1s -- comfortably inside WLED's realtime-timeout margin

static uint8_t  g_lastSentRgb[MAX_TOTAL_ZONES * 3];
static int      g_lastSentNumZones = 0;
static bool     g_haveLastSent = false;
static uint64_t g_lastSendTicks = 0;

void wled_send_rgb_zones(const uint8_t *rgbTriplets, int numZones)
{
    int sockfd = wled_ensure_socket();
    if (sockfd < 0) return;

    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(g_config.wledPort);
    if (inet_pton(AF_INET, g_config.wledHost, &destAddr.sin_addr) != 1) return;

    if (numZones > MAX_ZONES) numZones = MAX_ZONES;
    int dataSize = numZones * 3;

    // Hold the first send until a non-black frame, but only while the shell feed is running to cover (the first send
    // publishes fg=1 and silences it). With no shell feed there is no hold. One-way latch. [docs/code-comments.md]
    {
        static bool sawLiveFrame = false;
        if (!sawLiveFrame) {
            if (!g_shellFeedAlive) {
                sawLiveFrame = true; // no shell feed to hand over from: no hold
            } else {
                uint8_t holdMax = 0;
                for (int i = 0; i < dataSize; i++) if (rgbTriplets[i] > holdMax) holdMax = rgbTriplets[i];
                if (holdMax <= 2) return;
                sawLiveFrame = true;
            }
        }
    }

    uint8_t packet[DDP_HEADER_SIZE + 3 * MAX_ZONES];
    packet[0] = 0x40 | 0x01;
    packet[1] = 0;
    packet[2] = 0x0B;
    packet[3] = 0x01;
    packet[4] = 0; packet[5] = 0; packet[6] = 0; packet[7] = 0; // pixel offset 0
    packet[8] = (uint8_t)((dataSize >> 8) & 0xFF);
    packet[9] = (uint8_t)(dataSize & 0xFF);
    memcpy(packet + DDP_HEADER_SIZE, rgbTriplets, dataSize);

    ssize_t sentBytes = sendto(sockfd, packet, DDP_HEADER_SIZE + dataSize, 0,
                               (struct sockaddr*)&destAddr, sizeof(destAddr));
    // LAUNCH_GAP: any silence between two game-feed submissions longer than LG_GAP_MS (heartbeat resends
    // included, since they come through here). Event-only, once per gap, never per frame.
    {
        static uint64_t lgLastSendTsc = 0;
        uint64_t nowTsc = sceKernelGetProcessTimeCounter();
        uint64_t hzTsc  = sceKernelGetProcessTimeCounterFrequency();
        if (lgLastSendTsc != 0 && hzTsc != 0 && (nowTsc - lgLastSendTsc) * 1000ULL / hzTsc >= 250ULL) {
            ambient_handoff_diag("HANDOFF_DIAG LG_SEND_GAP ms=%llu bg=%d tsc=%llu hz=%llu",
                                 (unsigned long long)((nowTsc - lgLastSendTsc) * 1000ULL / hzTsc),
                                 g_isBackgrounded ? 1 : 0, (unsigned long long)nowTsc, (unsigned long long)hzTsc);
        }
        lgLastSendTsc = nowTsc;
    }
    // LAUNCH_GAP dark-send tracking: tells "packets stopped" (LG_SEND_GAP) apart from "packets kept coming...
    // More: docs/code-comments.md (plugin/source/network.c:529)
    {
        enum { LG_DARK_MAX = 2, LG_DARK_MIN_FRAMES = 5, LG_DARK_CAP = 16 };
        static uint32_t darkFrames = 0, darkEvents = 0;
        static uint64_t darkStartTsc = 0;
        static bool darkReported = false;
        uint8_t maxCh = 0;
        for (int i = 0; i < dataSize; i++) if (rgbTriplets[i] > maxCh) maxCh = rgbTriplets[i];
        uint64_t nowTsc = sceKernelGetProcessTimeCounter();
        uint64_t hzTsc  = sceKernelGetProcessTimeCounterFrequency();
        if (maxCh <= LG_DARK_MAX) {
            if (darkFrames == 0) darkStartTsc = nowTsc;
            darkFrames++;
            if (!darkReported && darkFrames >= LG_DARK_MIN_FRAMES && darkEvents < LG_DARK_CAP) {
                darkReported = true; darkEvents++;
                ambient_handoff_diag("HANDOFF_DIAG LG_DARK_START frames=%u maxch=%u bg=%d tsc=%llu hz=%llu",
                                     (unsigned)darkFrames, (unsigned)maxCh, g_isBackgrounded ? 1 : 0,
                                     (unsigned long long)darkStartTsc, (unsigned long long)hzTsc);
            }
        } else {
            if (darkReported && hzTsc != 0) {
                ambient_handoff_diag("HANDOFF_DIAG LG_DARK_END frames=%u ms=%llu maxch=%u bg=%d tsc=%llu hz=%llu",
                                     (unsigned)darkFrames, (unsigned long long)((nowTsc - darkStartTsc) * 1000ULL / hzTsc),
                                     (unsigned)maxCh, g_isBackgrounded ? 1 : 0,
                                     (unsigned long long)nowTsc, (unsigned long long)hzTsc);
            }
            darkFrames = 0; darkReported = false;
        }
    }
    // HANDOFF_DIAG: first game-feed DDP submission only; UDP send success is not WLED receipt.
    {
        static bool handoffDiagFirstSend = false;
        if (!handoffDiagFirstSend) {
            handoffDiagFirstSend = true;
            uint8_t firstMaxCh = 0;
            for (int i = 0; i < dataSize; i++) if (rgbTriplets[i] > firstMaxCh) firstMaxCh = rgbTriplets[i];
            ambient_handoff_diag("HANDOFF_DIAG GAME_FIRST_DDP_RESULT bytes=%lld expected=%u zones=%d maxch=%u tsc=%llu hz=%llu",
                   (long long)sentBytes, (unsigned)(DDP_HEADER_SIZE + dataSize), numZones, (unsigned)firstMaxCh,
                   (unsigned long long)sceKernelGetProcessTimeCounter(),
                   (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
        }
    }
    // This counter is deliberately a transport-progress signal, not a flip signal.
    // More: docs/code-comments.md (plugin/source/network.c:573)
    (void)wled_output_note_send(&g_wledOutputCount, sentBytes,
                                (size_t)(DDP_HEADER_SIZE + dataSize));

    // v2.8: snapshot what actually went out, so a later stale-send check (wled_send_keepalive_if_stale)...
    // More: docs/code-comments.md (plugin/source/network.c:579)
    if (numZones > 0) {
        memcpy(g_lastSentRgb, rgbTriplets, (size_t)dataSize);
        g_lastSentNumZones = numZones;
        g_haveLastSent = true;
    }
    g_lastSendTicks = sceKernelGetProcessTimeCounter();
}

// v2.8: called once per sample-thread iteration, but ONLY from the no-valid-frame branch...
// More: docs/code-comments.md (plugin/source/network.c:591)
void wled_send_keepalive_if_stale(uint64_t nowTicks, uint64_t tscFreq)
{
    if (!g_haveLastSent) return; // nothing sent yet this session
    uint64_t elapsed = nowTicks - g_lastSendTicks;
    if (elapsed < (tscFreq * (uint64_t)WLED_HEARTBEAT_INTERVAL_US) / 1000000ULL) return;
    wled_send_rgb_zones(g_lastSentRgb, g_lastSentNumZones); // also resets g_lastSendTicks
}


// v3.9: buffer guard debug packets (debug builds only, sent from sample_thread.c about once a second).
// More: docs/code-comments.md (plugin/source/network.c:606)

// GRDC -- guard counters, 24 bytes.
// More: docs/code-comments.md (plugin/source/network.c:612)
void send_guard_diag_packet(uint32_t available, uint32_t rejects, int32_t lastRet, uint64_t lastBadAddr)
{
    uint8_t packet[24];
    memcpy(packet + 0, "GRDC", 4);
    memcpy(packet + 4, &available, 4);
    memcpy(packet + 8, &rejects, 4);
    memcpy(packet + 12, &lastRet, 4);
    memcpy(packet + 16, &lastBadAddr, 8);
    debug_send_raw(packet, sizeof(packet));
}

// GRDI -- kernel map info for the last region that failed the readability check, 52 bytes.
// More: docs/code-comments.md (plugin/source/network.c:632)
void send_guard_info_packet(uint64_t start, uint64_t end, uint64_t offset, uint64_t addr,
                             int32_t prot, int32_t memType, uint32_t flags, int32_t ret)
{
    uint8_t packet[52];
    memcpy(packet + 0, "GRDI", 4);
    memcpy(packet + 4, &start, 8);   memcpy(packet + 12, &end, 8);
    memcpy(packet + 20, &offset, 8); memcpy(packet + 28, &addr, 8);
    memcpy(packet + 36, &prot, 4);   memcpy(packet + 40, &memType, 4);
    memcpy(packet + 44, &flags, 4);  memcpy(packet + 48, &ret, 4);
    debug_send_raw(packet, sizeof(packet));
}

// RMAP -- GPU-only buffer remap status, 28 bytes.
// More: docs/code-comments.md (plugin/source/network.c:654)
void send_remap_diag_packet(uint32_t flags, uint32_t created, uint32_t failed, uint32_t passes, int32_t lastRet, uint64_t lastAlias)
{
    uint8_t packet[28];
    uint32_t aliasLo = (uint32_t)lastAlias;
    memcpy(packet + 0, "RMAP", 4);
    memcpy(packet + 4, &flags, 4);   memcpy(packet + 8, &created, 4);
    memcpy(packet + 12, &failed, 4); memcpy(packet + 16, &passes, 4);
    memcpy(packet + 20, &lastRet, 4); memcpy(packet + 24, &aliasLo, 4);
    debug_send_raw(packet, sizeof(packet));
}
