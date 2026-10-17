// network.c -- part of ps4_ambient_light, split out of the original
// single-file main.c. WLED DDP transport, wled-relay signal, and debug/telemetry UDP sends.
// See ambient_internal.h for the shared types/externs this file relies
// on, and main.c's own top comment for this plugin's overall history.

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
volatile uint32_t g_wledOutputCount = 0; // observed by state.c to arbitrate ownership with shell-feed
volatile int      g_shellFeedAlive = 1;  // 1 until state.c's thread has evidence the shell feed is not running; read by the startup hold in wled_send_rgb_zones

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

// --- Debug/telemetry channel, step 2: a real number for
// ambient_sample_thread's own loop time, not just "barely noticeable".
// Deliberately a SEPARATE socket/target from wled_ensure_socket() above --
// that one points at the real WLED light (g_config.wledHost, //); telemetry has no
// business going there. This reuses detile_verify_probe's proven
// wled_send_raw pattern (own socket, dev-PC debug IP, 64-byte payload
// cap, per-call socket()/close() since this is a ~1x/second send, not a
// hot path -- unlike wled_send_rgb_zones above there's no reason to hold
// a persistent socket open for this) rather than routing through the
// production WLED path.
#define DEBUG_IP "192.168.2.117"   // v2.2.5: NO LONGER USED as a send target -- kept only as
                                    // a comment/reference of what this used to be hardcoded to.
                                    // debug_send_raw now requires g_config.devIp
                                    // to be set via an explicit [dev] section in the ini; see
                                    // that function and the AmbientConfig struct comment.

// Event-only foreground-handoff diagnostics. Reuse the existing opt-in dev UDP path
// (INI [dev] dev_ip + dev_logging=true), not stdout/printf. HOF1 lets the ground
// listener distinguish these ASCII records from the plugin's existing binary telemetry.
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
    // v2.2.5: single choke point for ALL debug/diagnostic UDP sends in
    // this file (the v2.2.1 format-change packet, the v2.2.2 raw-pixel
    // dump, AND this pre-existing config-reload packet) -- requires an
    // explicit opt-in via the ini's [dev] section, checked here rather
    // than at each call site so there's exactly one place to get this
    // right, not three. Neither devIp alone nor devLoggingEnabled alone
    // is enough; both are required. See for why this replaced
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
// directly from the SDK, not recited from memory) because this
// plugin's own OpenOrbis toolchain header (<orbis/SystemService.h>)
// declares sceSystemServiceGetStatus() as a bare, argument-less `void`
// stub -- the exact same "unfixed auto-generated placeholder" problem
// already flagged for sceHttpSetRecvTimeOut (see
// the project history). This plugin does NOT call through
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
// system-level assumption (klog -- see the project history
// "do not re-enable without a very good reason and a safety-net
// signal plan"), and this isn't repeating that mistake without a
// safety margin.
//
// NOT verified against Sony's own official SDK (not available to this
// session) or against real hardware -- only against shadPS4's public,
// independently-reverse-engineered source. See
// the project history for the full account and the
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
    // fork's own eventually corrects it as a side effect
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

// Hand the strip back to wled-relay -- but only if nothing else still holds it. The shell feed drives
// the strip whenever no game does (Home, Settings, the quick menu), so while it is running, a game that
// backgrounds or exits must NOT tell the relay "off": the relay would come on over the shell feed's colours.
// g_shellFeedAlive is state.c's latest read of the shell feed's heartbeat (kept fresh for as long as
// relay_signal_enabled is on). If the shell feed is not running (switched off, crashed, not installed, or
// the console has no shell feed at all), this is the plain "off" the plugin always sent.
// Callers still guard on relaySignalEnabled themselves, as for relay_send_external_source().
void relay_release_external_source(void)
{
    if (g_shellFeedAlive) return;
    relay_send_external_source(false);
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
//, rather than reusing 24 for a changed layout.
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

// v3.1: what actually found "this title never lights up" -- see
// hooks.c's g_pluginVersion comment for the full investigation. One
// packet per second from sample_thread.c, __FINAL__==0-gated same as
// every other packet in this file. 44 bytes, deliberately distinct
// from this project's other packet lengths (8 format, 36 timing, 68
// config-reload) so decode_verification_dump.py-style dispatch on
// len(data) keeps working without ambiguity.
//   [0:4]   register_hook_call_count            -- cumulative sceVideoOutRegisterBuffersPtr_hook calls this session
//   [4:8]   flip_hook_call_count                 -- cumulative sceGnmSubmitAndFlipCommandBuffersPtr_hook calls (the plain, wrapper entrypoint)
//   [8:12]  display_buffer_index                 -- g_currentDisplayBufferIndex; 0xFFFFFFFF == still the startup sentinel, no flip seen yet by ANY hook
//   [12:20] live_buffer_addr                     -- g_bufferAddrs[display_buffer_index], or 0 if the index above is still the sentinel
//   [20:24] have_valid_format                    -- g_haveValidFormat
//   [24:28] active_format                        -- g_activeFormat
//   [28:32] videout_submit_flip_hook_call_count  -- cumulative sceVideoOutSubmitFlipPtr_hook calls (the second, API-path entrypoint)
//   [32:36] submit_flip_ptr_resolved             -- 1 if dlsym found sceVideoOutSubmitFlip on this title/firmware, 0 if not -- disambiguates "never resolved" from "resolved, never called" when [28:32] reads 0
//   [36:40] gnm_for_workload_hook_call_count     -- cumulative sceGnmSubmitAndFlipCommandBuffersForWorkloadPtr_hook calls (the third entrypoint, the one that turned out to matter)
//   [40:44] gnm_for_workload_ptr_resolved        -- same disambiguation as [32:36], for the third entrypoint
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
// convention decode_verification_dump.py already uses.
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
// worked on this filesystem and is no
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

// v2.8: WLED heartbeat. WLED falls back to its own realtime-timeout
// color after a gap in packets -- see the v2.2.3 changelog entry and
// the "deliberately send nothing rather than guess" comment in
// sample_thread.c for the real, still-open silent path this closes
// (an unknown/transitioning pixel format while foregrounded). This
// tracks the last color actually sent so a stale-send check can
// repeat it, without ever inventing a color that wasn't verified by
// the real pipeline.
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

    // Startup hold: until the game feed has produced one non-black frame, send nothing -- but only while the
    // shell feed is actually running to cover for us. The first send is what publishes fg=1 (state.c), and
    // fg=1 makes the shell feed back off. During a launch picture the game's own buffers can read all zeros
    // while the picture is on screen, so sending those frames both blacks out the strip and silences the
    // shell feed that still has the right colours. If the shell feed is not running (switched off, crashed or
    // not installed: g_shellFeedAlive, kept by state.c from its heartbeat file) there is nobody to cover, and
    // holding would leave the strip on WLED's default colour through a genuinely black loading screen, so the
    // plugin sends from the first available frame, as it did before the hold existed. One-way latch: once a
    // frame has passed, dark scenes are sent normally.
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
    // LAUNCH_GAP dark-send tracking: tells "packets stopped" (LG_SEND_GAP) apart from "packets kept
    // coming but were black". A run of >= LG_DARK_MIN_FRAMES sends whose brightest channel is <= LG_DARK_MAX
    // is reported once when it is confirmed and once when it ends. Event-only, capped.
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
    // UDP cannot confirm receipt by WLED, but a complete sendto() result tells the
    // ownership writer that this process is still successfully submitting DDP.
    (void)wled_output_note_send(&g_wledOutputCount, sentBytes,
                                (size_t)(DDP_HEADER_SIZE + dataSize));

    // v2.8: snapshot what actually went out, so a later stale-send check
    // (wled_send_keepalive_if_stale) has a real, previously-sent color to
    // repeat -- covers both a normal per-frame send and a heartbeat
    // resend, so the timer always measures from the last real packet.
    if (numZones > 0) {
        memcpy(g_lastSentRgb, rgbTriplets, (size_t)dataSize);
        g_lastSentNumZones = numZones;
        g_haveLastSent = true;
    }
    g_lastSendTicks = sceKernelGetProcessTimeCounter();
}

// v2.8: called once per sample-thread iteration, but ONLY from the
// no-valid-frame branch (unknown/transitioning pixel format while still
// foregrounded) -- never from the g_isBackgrounded path, where going
// silent is intentional (v2.4) and a heartbeat here would reintroduce
// the frozen-strip-on-suspend bug that fix removed. Resends the last
// verified color rather than guessing a new one.
void wled_send_keepalive_if_stale(uint64_t nowTicks, uint64_t tscFreq)
{
    if (!g_haveLastSent) return; // nothing sent yet this session
    uint64_t elapsed = nowTicks - g_lastSendTicks;
    if (elapsed < (tscFreq * (uint64_t)WLED_HEARTBEAT_INTERVAL_US) / 1000000ULL) return;
    wled_send_rgb_zones(g_lastSentRgb, g_lastSentNumZones); // also resets g_lastSendTicks
}


// v3.9: buffer guard debug packets (debug builds only, sent from sample_thread.c about once a
// second). Each starts with a 4 byte ASCII tag, like FLK1, PQ8C and HDRV. The tag is how to tell
// them apart: GRDI is 48 bytes after the DDP header, the same length as PQ8C and HDRV, so a
// decoder that goes by length alone will misread one as the other (this happened while decoding
// the first MK11 capture, see docs/debugging/gpu-only-buffers.md).

// GRDC -- guard counters, 24 bytes.
//   [0:4]   "GRDC"
//   [4:8]   guard_available  -- 1 if sceKernelVirtualQuery resolved (0 = the guard is failing open)
//   [8:12]  reject_count     -- cumulative passes skipped because the buffer was not readable
//   [12:16] last_ret         -- 0 ok, <0 VirtualQuery error, 1 unmapped gap, 2 not CPU-readable
//   [16:24] last_bad_addr    -- last address rejected (compare with a crash report's fault address)
// reject_count keeps counting a buffer that is later made readable only by its first rejection,
// so on a fixed title it stops at one per slot; on a title that stays dark it climbs by the sampler
// rate (about 30 a second).
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
// Only sent once reject_count is above 0. It is a snapshot of the LAST failed query, so on a title
// that was fixed by the mprotect it keeps showing the pre-fix protection (0x30) after the buffer
// became readable; read it together with RMAP, not on its own.
//   [0:4]   "GRDI"
//   [4:12]  start  [12:20] end  [20:28] direct-memory offset  [28:36] address queried
//   [36:40] protection (bit0 CPU read, bit1 CPU write, bit4 GPU read, bit5 GPU write)
//   [40:44] memoryType (MK11: 3, write-combined)
//   [44:48] flags byte (bit1 direct, bit4 committed; MK11: 0x12)
//   [48:52] guard reason
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
//   [0:4]   "RMAP"
//   [4:8]   flags: bits 0-3 gpu_only_remap setting (default 3), bit4 sceKernelMapDirectMemory2
//           resolved, bit5 sceKernelMapDirectMemory resolved, bit6 sceKernelMunmap resolved,
//           bit7 sceKernelMprotect resolved, bits 8-15 last method tried
//           (1 typed MapDirectMemory2, 2 plain MapDirectMemory, 3 mprotect)
//   [8:12]  created  -- buffers made readable (alias views created, or mprotects that stuck)
//   [12:16] failed   -- regions every enabled method failed on (each is remembered, not retried)
//   [16:20] passes   -- resolve calls served by a remap (an mprotected buffer reads directly from
//                       the next pass on, so this stays small on a title fixed by method 3)
//   [20:24] last_ret -- last method's return code, 0 ok. 0x80020010 = EBUSY (second mapping refused)
//   [24:28] low 32 bits of the last alias address (0 when no alias was made)
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
