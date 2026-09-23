// network.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. WLED DDP transport, wled-relay signal, and debug/telemetry UDP sends.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.

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

// --- WLED DDP transport (identical pattern used throughout this project) ---
// v2.0: WLED host is now g_config.wledHost/wledPort (see the settings
// block above), not a compile-time constant. WLED_PORT is kept as a
// #define solely because debug_send_raw() below (the DEV-PC debug
// telemetry channel, not the production light) still uses a fixed
// port -- that's deliberately NOT user-configurable, it's a dev tool.
#define WLED_PORT       4048
#define DDP_HEADER_SIZE 10

// Compile-time cap for zone-array/DDP-packet-buffer sizing. Defined
// here (before first use in wled_send_rgb_zones below) rather than
// down in the geometry section, since C macro expansion needs the
// referenced name already defined by that point in the file, not just
// defined somewhere else in it.
#define MAX_TOTAL_ZONES 512

int g_wledSockfd = -1; // one socket, held open -- avoid a socket()/close() per send at frame rate

static int wled_ensure_socket(void)
{
    if (g_wledSockfd >= 0) return g_wledSockfd;
    g_wledSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_wledSockfd >= 0) {
        // UDP sendto() essentially never blocks (no handshake/ACK), but
        // "essentially never" isn't "never" -- a full local send buffer
        // or an unusual driver/network state could still stall this
        // thread with no telemetry to explain why. A short send timeout
        // bounds that to a few ms instead of an indefinite stall. This is
        // plain POSIX SO_SNDTIMEO via <sys/socket.h>, the same header
        // already included and working in this file -- not a new or
        // unverified API surface like the affinity/priority calls above.
        struct timeval sndTimeout = { .tv_sec = 0, .tv_usec = 5000 }; // 5ms
        setsockopt(g_wledSockfd, SOL_SOCKET, SO_SNDTIMEO, &sndTimeout, sizeof(sndTimeout));
    }
    return g_wledSockfd;
}

// --- Debug/telemetry channel, handoff §30 step 2: a real number for
// ambient_sample_thread's own loop time, not just "barely noticeable".
// Deliberately a SEPARATE socket/target from wled_ensure_socket() above --
// that one points at the real WLED light (g_config.wledHost, §2/§21/§43); telemetry has no
// business going there. This reuses detile_verify_probe's proven
// wled_send_raw pattern (own socket, dev-PC debug IP, 64-byte payload
// cap, per-call socket()/close() since this is a ~1x/second send, not a
// hot path -- unlike wled_send_rgb_zones above there's no reason to hold
// a persistent socket open for this) rather than routing through the
// production WLED path.
#define DEBUG_IP "192.168.2.117"   // v2.2.5: NO LONGER USED as a send target -- kept only as
                                    // a comment/reference of what this used to be hardcoded to
                                    // (handoff §2). debug_send_raw now requires g_config.devIp
                                    // to be set via an explicit [dev] section in the ini; see
                                    // that function and the AmbientConfig struct comment.

void debug_send_raw(const uint8_t *data, int len)
{
    // v2.2.5: single choke point for ALL debug/diagnostic UDP sends in
    // this file (the v2.2.1 format-change packet, the v2.2.2 raw-pixel
    // dump, AND this pre-existing config-reload packet) -- requires an
    // explicit opt-in via the ini's [dev] section, checked here rather
    // than at each call site so there's exactly one place to get this
    // right, not three. Neither devIp alone nor devLoggingEnabled alone
    // is enough; both are required. See handoff for why this replaced
    // a hardcoded always-on DEBUG_IP.
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

    // BUGFIX (found via disassembly of a real deployed .prx, not source
    // inspection alone -- the compiled binary's call site correctly passed
    // 68 for the config-reload packet, but this cap silently truncated it
    // to 64 before it ever hit the wire, dropping exactly the last_hash
    // field added in v2.1.5). This cap predates that packet's growth past
    // 64 bytes and was never updated to match. Raised with headroom rather
    // than tuned to exactly 68, so the next packet that grows a few bytes
    // doesn't silently reintroduce the same class of bug.
    uint8_t packet[DDP_HEADER_SIZE + 128];
    if (len > 128) len = 128;
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

// v2.3: tells wled-relay's tv_external_source.py whether THIS plugin
// is currently the one driving the TV backlight WLED controller
// directly (see the AmbientConfig relayHost/relayPort comment above
// for why this is a separate destination from the WLED controller
// itself). Deliberately a plain
// raw UDP payload, not wrapped in the DDP header used everywhere else
// in this file -- there's no WLED device on the receiving end here,
// just wled-relay's own tiny listener socket, and it expects the exact
// same bare "on"/"off" ASCII text tv_gate.py's MQTT handler already
// parses for gaming_mode, just delivered over a plain UDP datagram
// instead of an MQTT payload (this plugin has no MQTT client, and
// implementing one here for a single on/off flag isn't worth the
// complexity -- see that module's own docstring).
//
// One-shot, own socket per call (not the persistent g_wledSockfd) --
// this is called at most twice per game session (plugin_load,
// plugin_unload), nowhere near the per-frame hot path, so there's no
// reason to hold a socket open for it. Same pattern debug_send_raw
// already uses for exactly that reason.
// v2.4: real field layout confirmed against shadPS4's own independently
// reverse-engineered implementation (shadps4-emu/shadPS4,
// src/core/libraries/system/systemservice.{h,cpp} -- fetched and read
// directly this session, not recited from memory) because this
// plugin's own OpenOrbis toolchain header (<orbis/SystemService.h>)
// declares sceSystemServiceGetStatus() as a bare, argument-less `void`
// stub -- the exact same "unfixed auto-generated placeholder" problem
// already flagged for sceHttpSetRecvTimeOut (see
// ps4-ambient-light-handoff-v9.md). This plugin does NOT call through
// that broken declaration -- the real symbol is resolved by name at
// runtime instead (sys_dynlib_load_prx + sys_dynlib_dlsym in
// plugin_load), the exact same pattern this file already uses for
// sceVideoOutRegisterBuffers/sceGnmSubmitAndFlipCommandBuffers, with
// its own correct function-pointer type declared here.
//
// shadPS4's own struct only defines 5 known bool/int32 fields plus a
// flexible, UNSIZED `reserved[]` array at the end -- i.e. even that
// project doesn't claim to know this struct's true total size on real
// hardware, only enough of its front to satisfy real games running
// under HLE. Real firmware could plausibly write further into
// `reserved` than either project accounts for.
// AMBIENT_SYS_SERVICE_STATUS_PADDING pads this struct's real buffer
// generously past every known field specifically so an
// unexpectedly-larger real write lands in our own padding rather than
// adjacent stack memory -- this project has already had one full
// crash requiring a console power cycle from an unverified
// system-level assumption (klog -- see ps4-ambient-light-handoff-v10.md
// §2's "do not re-enable without a very good reason and a safety-net
// signal plan"), and this isn't repeating that mistake without a
// safety margin.
//
// NOT verified against Sony's own official SDK (not available to this
// session) or against real hardware -- only against shadPS4's public,
// independently-reverse-engineered source. See
// ps4-ambient-light-handoff-v22.md for the full account and the
// recommended staged hardware-verification plan before trusting this
// for anything beyond an isolated debug-telemetry read-back test.
// (AMBIENT_SYS_SERVICE_STATUS_PADDING and the AmbientSystemServiceStatus
// struct itself now live in ambient_internal.h -- sample_thread.c needs
// the type too, to read status.isInBackgroundExecution.)

int32_t (*sceSystemServiceGetStatusPtr)(AmbientSystemServiceStatus *status);

// v2.4: last known foreground/background state, checked once per
// ambient_sample_thread iteration (throttled -- see the poll counter
// there) so both the direct WLED send AND the relay signal react to
// the same real transition instead of two separate, possibly
// disagreeing detectors.
bool g_isBackgrounded = false;

void relay_send_external_source(bool active)
{
    // v2.6: BUGFIX -- this used to gate itself here on
    // g_config.relaySignalEnabled, in one place, so no call site could
    // accidentally dial someone else's relay_host default. That broke
    // the one case that matters most: ambient_check_config_reload's
    // own flip-handler (below) calls this exactly when
    // relaySignalEnabled itself just transitioned -- including
    // true->false. By the time that call happens, g_config.relaySignalEnabled
    // is ALREADY false (that's what triggered the call), so this
    // gate would silently swallow the "off" send the call exists to
    // make, leaving wled-relay believing this plugin is still driving
    // the strip until the separate gaming_mode-off backstop (that
    // fork's own handoff v21) eventually corrects it as a side effect
    // -- not by design. Found while adding this same signal to the
    // companion app, reproduced directly in an isolated sandbox test
    // before being traced back and fixed here too. The opt-in
    // protection this gate existed for is now each call site's own
    // responsibility instead -- see the comment at every call site
    // below for why each one is safe for a user who never opted in.
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

// One timing-window packet, 24 bytes -- deliberately a length that
// collides with neither detile_verify_probe's 11-byte pixel packets nor
// its 32-byte registration packets, so decode_verification_dump.py can
// keep dispatching purely on len(data) the way it already does (v3 adds
// the len==24 case; see that script). All fields uint32 LE, matching
// every other multi-byte field in this project's wire formats.
//   [0:4]   min_us            -- fastest loop iteration this window
//   [4:8]   max_us            -- slowest loop iteration this window
//   [8:12]  avg_us            -- mean loop iteration this window
//   [12:16] sample_count      -- iterations folded into this window (window size, or less if just started)
//   [16:20] over_budget_count -- iterations that took longer than SAMPLE_INTERVAL_US (i.e. the thread fell behind its own ~30Hz target)
//   [20:24] window_id         -- increments every window; lets the listener notice a gap (plugin reload, crash, etc.)
// v1.2 was 24 bytes (minUs/maxUs/avgUs/sampleCount/overBudgetCount/windowId).
// This adds 3 more uint32 fields (minCpu/maxCpu/migrationCount) for a new,
// 36-byte length -- same "new packet length -> new dispatch case" pattern
// decode_verification_dump.py already uses to tell packet types apart
// (§31), rather than reusing 24 for a changed layout.
//
// What this answers, using ONLY the confirmed sceKernelGetCurrentCpu()
// (no affinity/priority APIs involved -- this is pure observation):
// which core(s) this thread actually runs on across a session, and how
// often the scheduler moves it mid-session (migrationCount > 0 means it
// isn't pinned, by default, to one core). This is the actual measurement
// needed before CORE_MASK above can be filled in with anything real.
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

// v2.1.3: peek at the first bytes of AMBIENT_CONFIG_PATH from the
// plugin's own point of view, so a capture can show directly what's
// there instead of inferring it from size/mtime alone -- e.g. leftover
// test text vs. a truncated fragment of the real ini vs. something else
// entirely. Uses the same sceKernelOpen/sceKernelRead/sceKernelClose
// path as config.c's ini_table_read_from_file (see that file's v2.2
// comment) -- no fopen/FILE* here either, so this stays safe to call
// from ambient_sample_thread. Purely diagnostic: never touches g_config
// or any reload state, and any failure just zero-fills the preview
// rather than showing stack garbage or aborting the check.
// (CONFIG_DEBUG_PREVIEW_LEN now lives in ambient_internal.h -- settings.c needs it too.)
void ambient_read_content_preview(uint8_t *out, size_t previewLen)
{
    memset(out, 0, previewLen);
    int32_t fd = sceKernelOpen(AMBIENT_CONFIG_PATH, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return; // leave zero-filled -- open failure is itself informative (event/stat_errno already cover it)
    ssize_t nread = sceKernelRead(fd, out, previewLen);
    sceKernelClose(fd);
    if (nread < 0) memset(out, 0, previewLen); // read failed -- keep the zero-fill, don't show garbage
}

// Config-reload diagnostic packet, added to actually SEE why live reload
// isn't firing on real hardware rather than guessing again (the v2.1.1
// mtime-sentinel fix didn't resolve it). One per ambient_check_config_reload()
// call -- reports every branch that function can take, not just the
// interesting one, so a silent "the check never even runs" is visible
// too, not just "it ran and saw no change".
//
// 60 bytes -- deliberately a length that collides with none of this
// project's existing packet types (10/11 pixel, 24 old timing, 32
// registration, 36 current timing, 44 pre-v2.1.3 version of this same
// packet), matching the same "new packet length -> new dispatch case"
// convention decode_verification_dump.py already uses (§31).
//
// mtime/size are sent as full 64-bit values (not truncated to uint32
// like the other telemetry fields) specifically because the bug this is
// chasing is about mtime edge cases -- truncating away the high bits
// here would risk hiding exactly the kind of value this exists to show.
//   [0:4]   event:        0=reload disabled, 1=sceKernelOpen/Lseek
//                         failed (was "stat() failed" pre-v2.1.4),
//                         2=baseline just established, 3=checked, no
//                         change, 4=change detected, reload triggered
//   [4:8]   stat_errno:   negated orbis error code from the failed
//                         open/lseek (event==1 only, 0 otherwise) --
//                         was a libc errno from stat() pre-v2.1.4, now
//                         an orbis kernel error code from
//                         ambient_get_real_config_size; field kept at
//                         the same offset/name for capture compat
//   [8:16]  cur_mtime:    HARDCODED 0 as of v2.1.4 -- st_mtime never
//                         worked on this filesystem (§46) and is no
//                         longer read at all. Field kept at the same
//                         offset so old/new captures still line up
//                         byte-for-byte; not a live value.
//   [16:24] last_mtime:   HARDCODED 0 as of v2.1.4, same reason as
//                         cur_mtime above.
//   [24:32] cur_size:     st_size just read (0 if stat wasn't reached)
//   [32:40] last_size:    g_configLastSize BEFORE this check updated it
//   [40:44] check_count:  increments every call to
//                         ambient_check_config_reload -- if this never
//                         climbs, the sampling thread isn't reaching the
//                         check at all (wrong configReloadCheckSeconds,
//                         thread not running, etc.), which is a
//                         different bug than "the check runs but never
//                         detects a change".
//   [44:60] content_preview: first 16 raw bytes AMBIENT_CONFIG_PATH's own
//                         read path actually sees (v2.1.3, see
//                         ambient_read_content_preview above). Zero-filled
//                         when unavailable -- 16 zero bytes here does NOT
//                         necessarily mean the file is empty/all-zero, it
//                         may mean the open/read itself failed.
//   [60:64] cur_hash:     v2.1.5. FNV-1a/32 over the whole file, from
//                         ambient_get_real_config_size_and_hash. 0 if
//                         that call failed (event==1) or wasn't reached
//                         (event==0) -- note 0 is also a real possible
//                         hash value, same caveat as mtime==0 pre-v2.1.1,
//                         but here it's disambiguated by event/stat_errno
//                         rather than by a separate sentinel, since this
//                         field was never used as its own "have we
//                         initialized yet" flag the way mtime was.
//   [64:68] last_hash:    v2.1.5. g_configLastHash BEFORE this check
//                         updated it. Same 0-is-ambiguous caveat as
//                         cur_hash above.
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

// v2.0: MAX_ZONES is just an alias for MAX_TOTAL_ZONES (defined above,
// near DDP_HEADER_SIZE) -- kept as its own name here since it documents
// *why* this buffer is sized the way it is (DDP packet cap), while the
// geometry code below cares about it as "max zone count" instead.
#define MAX_ZONES MAX_TOTAL_ZONES

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
    uint8_t packet[DDP_HEADER_SIZE + 3 * MAX_ZONES];
    packet[0] = 0x40 | 0x01;
    packet[1] = 0;
    packet[2] = 0x0B;
    packet[3] = 0x01;
    packet[4] = 0; packet[5] = 0; packet[6] = 0; packet[7] = 0; // pixel offset 0
    packet[8] = (uint8_t)((dataSize >> 8) & 0xFF);
    packet[9] = (uint8_t)(dataSize & 0xFF);
    memcpy(packet + DDP_HEADER_SIZE, rgbTriplets, dataSize);

    sendto(sockfd, packet, DDP_HEADER_SIZE + dataSize, 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
}

