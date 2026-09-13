// ps4_ambient_light: the real per-frame pipeline (handoff §1 / §21 step
// 4). Everything here is proven infrastructure lifted directly from
// detile_verify_probe.prx (v2.2) -- the hooks, the buffer/format
// tracking, and the tiling math are unchanged from what was verified
// against a real screenshot this session. This file's only new job is
// to run that pipeline every frame instead of once on a button press,
// and turn the result into a live WLED signal instead of a debug dump.
//
// SCOPE (v2.0 -- see handoff §43 for the settings-system addition;
// v1.1/§26/§27 below still describes the worker-thread architecture,
// unchanged by v2.0):
//   - Samples a fixed set of 229 screen zones per frame (not a full
//     frame detile), matching the real measured LED strip layout
//     (42/73/41/73, see buildZoneGeometry below) using ONLY the
//     confirmed-correct base tiling params (handoff §22). The Neo path
//     is dropped here; it served its purpose settling that question in
//     the probe and has no reason to run in the hot path.
//   - Pixel format is read live from sceVideoOutRegisterBuffers, not
//     assumed at compile time -- this is the actual bug an earlier
//     session found (A2R10G10B10 math applied to A8R8G8B8 data). If an
//     unknown format shows up, this plugin explicitly stops sending
//     color rather than guessing.
//   - v1.1: the actual zone-sampling + UDP send no longer runs inside
//     the flip hook. It's been moved to a dedicated worker thread
//     (ambient_sample_thread, same pattern as detile_verify_probe's
//     pad_poll_thread) so a ~2000-sample pass can never stall the
//     game's render/submission thread. The flip hook now only records
//     displayBufferIndex -- see the thread's own comment block for the
//     staleness tradeoff this introduces.
//   - v1.1: sampleZoneAverage now bounds-checks the computed tiled
//     offset against the real padded-buffer size before reading, and
//     skips (treats as black) any sample that would land outside it,
//     instead of trusting the offset formula unconditionally.

//   - v1.2: ambient_sample_thread now measures its own loop time
//     (buffer-resolve + 229-zone sampling + UDP send, i.e. everything
//     except usleep) and reports per-window min/max/avg microseconds
//     plus an over-budget count to DEBUG_IP every ~1s (handoff §30
//     step 2 -- turning "barely noticeable" into an actual number).
//     Gated behind TIMING_ENABLED so it can be compiled out later.
//     Decode with decode_verification_dump.py v3+ (24-byte packets).

// STILL OPEN (do not treat this as fully validated -- see handoff):
//   - §25 (handoff v6) point-1 static-read question: RESOLVED, not a
//     bug -- see handoff §28. Left here only so this comment block
//     doesn't repeat the exact staleness mistake §26 called out.
//   - The worker-thread decoupling in v1.1 has not yet been measured
//     on real hardware for actual frame-time impact of the render-
//     thread-side work that remains (a couple of volatile writes per
//     flip). The v1.2 telemetry below measures the WORKER thread's own
//     loop time, which answers "is 30Hz sampling fast enough to keep
//     up" -- it does NOT measure render-thread-side cost, which is a
//     separate, still-open question if zone count or sample radius
//     increases later.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> // v2.1.5: malloc/free, for ambient_get_real_config_size_and_hash's bounded read buffer -- not needed anywhere else in this TU before now
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // struct timeval, for the SO_SNDTIMEO socket hardening below
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h> // no longer read directly as of v2.1.4 (ambient_check_config_reload's size signal now comes from sceKernelOpen/Lseek, not stat()) -- left included, harmless, in case anything else in this TU ever needs it

#include <orbis/libkernel.h>
#include <orbis/_types/video.h>

#include "plugin_common.h"
#include "config.h"

// Gamma correction LUTs -- generated OFFLINE with real pow() (this exact
// comment mirrors the sRGB/PQ LUT precedent above: table lookups only at
// runtime, no libm linked in this build, see Makefile LIBS).
// Index 0 = gamma 1.0 (off/passthrough) ... index 7 = gamma 2.8.
#define NUM_GAMMA_LUTS 8
static const uint8_t kGammaLuts[NUM_GAMMA_LUTS][256] = {
    { // gamma 1.0
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
        96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
    },
    { // gamma 1.4
        0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5,
        5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 12, 12, 13, 13,
        14, 15, 15, 16, 16, 17, 18, 18, 19, 20, 20, 21, 22, 22, 23, 24,
        25, 25, 26, 27, 28, 28, 29, 30, 31, 31, 32, 33, 34, 34, 35, 36,
        37, 38, 38, 39, 40, 41, 42, 43, 43, 44, 45, 46, 47, 48, 49, 49,
        50, 51, 52, 53, 54, 55, 56, 57, 57, 58, 59, 60, 61, 62, 63, 64,
        65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96,
        97, 98, 99, 100, 101, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
        115, 116, 117, 118, 119, 120, 121, 122, 124, 125, 126, 127, 128, 129, 130, 132,
        133, 134, 135, 136, 137, 139, 140, 141, 142, 143, 145, 146, 147, 148, 149, 151,
        152, 153, 154, 155, 157, 158, 159, 160, 161, 163, 164, 165, 166, 168, 169, 170,
        171, 173, 174, 175, 176, 178, 179, 180, 181, 183, 184, 185, 187, 188, 189, 190,
        192, 193, 194, 196, 197, 198, 200, 201, 202, 203, 205, 206, 207, 209, 210, 211,
        213, 214, 215, 217, 218, 219, 221, 222, 223, 225, 226, 227, 229, 230, 232, 233,
        234, 236, 237, 238, 240, 241, 242, 244, 245, 247, 248, 249, 251, 252, 254, 255,
    },
    { // gamma 1.8
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2,
        2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6,
        6, 6, 7, 7, 8, 8, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12,
        13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 21,
        21, 22, 22, 23, 24, 24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31,
        32, 32, 33, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 41, 42, 43,
        44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 53, 54, 55, 56, 57,
        58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
        74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 86, 87, 88, 89, 90,
        91, 92, 93, 95, 96, 97, 98, 99, 100, 102, 103, 104, 105, 107, 108, 109,
        110, 111, 113, 114, 115, 116, 118, 119, 120, 122, 123, 124, 126, 127, 128, 129,
        131, 132, 134, 135, 136, 138, 139, 140, 142, 143, 145, 146, 147, 149, 150, 152,
        153, 154, 156, 157, 159, 160, 162, 163, 165, 166, 168, 169, 171, 172, 174, 175,
        177, 178, 180, 181, 183, 184, 186, 188, 189, 191, 192, 194, 195, 197, 199, 200,
        202, 204, 205, 207, 208, 210, 212, 213, 215, 217, 218, 220, 222, 224, 225, 227,
        229, 230, 232, 234, 236, 237, 239, 241, 243, 244, 246, 248, 250, 251, 253, 255,
    },
    { // gamma 2.0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
        4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9,
        9, 9, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16,
        16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 23, 23, 24, 24,
        25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35, 35,
        36, 37, 38, 38, 39, 40, 41, 42, 42, 43, 44, 45, 46, 47, 47, 48,
        49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 84, 85, 86, 87, 88, 89, 91, 92, 93, 94, 95, 97, 98, 99,
        100, 102, 103, 104, 105, 107, 108, 109, 111, 112, 113, 115, 116, 117, 119, 120,
        121, 123, 124, 126, 127, 128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 143,
        145, 146, 148, 149, 151, 152, 154, 155, 157, 158, 160, 162, 163, 165, 166, 168,
        170, 171, 173, 175, 176, 178, 180, 181, 183, 185, 186, 188, 190, 192, 193, 195,
        197, 199, 200, 202, 204, 206, 207, 209, 211, 213, 215, 217, 218, 220, 222, 224,
        226, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255,
    },
    { // gamma 2.2
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6,
        6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12,
        12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
        20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29,
        30, 30, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 39, 39, 40, 41,
        42, 43, 43, 44, 45, 46, 47, 48, 49, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
        73, 74, 75, 76, 77, 78, 79, 81, 82, 83, 84, 85, 87, 88, 89, 90,
        91, 93, 94, 95, 97, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 111,
        113, 114, 116, 117, 119, 120, 121, 123, 124, 126, 127, 129, 130, 132, 133, 135,
        137, 138, 140, 141, 143, 145, 146, 148, 149, 151, 153, 154, 156, 158, 159, 161,
        163, 165, 166, 168, 170, 172, 173, 175, 177, 179, 181, 182, 184, 186, 188, 190,
        192, 194, 196, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
        223, 225, 227, 229, 231, 234, 236, 238, 240, 242, 244, 246, 248, 251, 253, 255,
    },
    { // gamma 2.4
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
        2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9,
        9, 10, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 14, 15, 15,
        16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24,
        24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35,
        35, 36, 37, 38, 39, 39, 40, 41, 42, 43, 43, 44, 45, 46, 47, 48,
        49, 50, 51, 52, 53, 53, 54, 55, 56, 57, 58, 59, 60, 62, 63, 64,
        65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 80, 81, 82,
        83, 85, 86, 87, 88, 90, 91, 92, 94, 95, 96, 98, 99, 100, 102, 103,
        105, 106, 108, 109, 111, 112, 114, 115, 117, 118, 120, 121, 123, 124, 126, 127,
        129, 131, 132, 134, 136, 137, 139, 141, 142, 144, 146, 148, 149, 151, 153, 155,
        156, 158, 160, 162, 164, 166, 167, 169, 171, 173, 175, 177, 179, 181, 183, 185,
        187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 210, 212, 214, 216, 218,
        220, 223, 225, 227, 229, 232, 234, 236, 239, 241, 243, 246, 248, 250, 253, 255,
    },
    { // gamma 2.6
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3,
        3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7,
        7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12,
        13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20,
        20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29,
        30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 38, 38, 39, 40, 41, 42,
        42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
        58, 59, 60, 61, 62, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73, 75,
        76, 77, 78, 80, 81, 82, 84, 85, 86, 88, 89, 90, 92, 93, 94, 96,
        97, 99, 100, 102, 103, 105, 106, 108, 109, 111, 112, 114, 115, 117, 119, 120,
        122, 124, 125, 127, 129, 130, 132, 134, 136, 137, 139, 141, 143, 145, 146, 148,
        150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180,
        182, 184, 186, 188, 191, 193, 195, 197, 199, 202, 204, 206, 209, 211, 213, 215,
        218, 220, 223, 225, 227, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252, 255,
    },
    { // gamma 2.8
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
        2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
        5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
        10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
        17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
        25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
        37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
        51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
        69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
        90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
        115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
        144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
        177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
        215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255,
    },
};
// ============================================================
// User-editable settings (v2.0 -- previously everything below was a
// compile-time #define; see handoff §43). Read once, at plugin_load,
// from an INI file at a fixed path -- same file format, same
// ini_table_s API, and the same auto-create-a-commented-template
// behavior as plugin_loader/plugins.ini (config.c/config.h in this
// folder are copied verbatim from plugin_loader, not reimplemented).
//
// Deliberately NOT using GOLDHEN_PATH ("/data/GoldHEN") for this file
// -- that directory is GoldHEN's own, shared across every plugin.
// This plugin gets its own file so editing it can't be confused with
// plugin_loader's plugins.ini, and so uninstalling this plugin has an
// obvious single file to also remove.
#define AMBIENT_CONFIG_PATH "/data/ps4_ambient_light.ini"

typedef enum { CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT, CORNER_TOP_LEFT, CORNER_TOP_RIGHT } StartCorner;
typedef enum { DIR_CLOCKWISE, DIR_COUNTERCLOCKWISE } LedDirection;
typedef enum { ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR } ColorOrder;

typedef struct {
    // [network]
    char wledHost[64];
    uint16_t wledPort;
    // [layout]
    uint32_t ledCountTop, ledCountRight, ledCountBottom, ledCountLeft;
    StartCorner startCorner;
    LedDirection direction;
    int32_t ledOffset;          // rotates which physical LED index 0 lands on
    uint32_t marginTop, marginRight, marginBottom, marginLeft; // pixels inset from the true screen edge before sampling
    uint32_t scanDepth;         // sample radius: (2*scanDepth+1)^2 pixels averaged per zone
    // [color]
    uint32_t brightness;        // 0-255 global scale, applied after gamma
    uint32_t gammaLutIndex;     // index into kGammaLuts -- see NUM_GAMMA_LUTS below
    int32_t saturation;         // -100 (grayscale) .. 0 (unchanged) .. 300 (4x boost, mostly clipped by then)
    ColorOrder colorOrder;
    uint32_t blackLevel;        // v2.1: 0-100, percent of 255 below which output clips to 0
    uint32_t whiteLevel;        // v2.1: 0-100, percent of 255 at/above which output clips to 255 (100 = no change)
    uint32_t darkThreshold;     // v2.1: 0-255, max(R,G,B) below this forces a zone fully black (0 = disabled)
    // v2.2: ported from the Android "inspiration" project's own
    // ColorProcessor.kt (see handoff §49). These are ADDITIONS on top
    // of the existing brightness/gamma fields above, not replacements
    // -- brightness/gamma keep their original 0-255 / fixed-LUT-string
    // meaning so nobody's existing ini silently changes behavior.
    // Percentages below use Android's own convention (100 = neutral),
    // NOT this file's usual "0 = neutral" convention -- documented
    // per-field in the generated ini template.
    int32_t contrast;           // -100..300, 0 = unchanged (100=neutral Android pct minus 100, same convention as saturation above)
    uint32_t brightnessR, brightnessG, brightnessB; // 0-500, 100 = unchanged. Multiplies with the global brightness above.
    uint32_t gammaR, gammaG, gammaB;                // 10-500, 100 = unchanged. Independent per-channel curves, applied PER SAMPLE
                                                     // before zone-averaging (see sampleZoneAverage) -- unlike every other
                                                     // knob here, which is applied once to the already-averaged zone color.
                                                     // This matters because gamma is non-linear: correcting-then-averaging
                                                     // and averaging-then-correcting are NOT the same operation, and the
                                                     // Android source applies its gamma per-pixel, before any downsampling.
    // [timing]
    uint32_t updateFrequencyHz;
    int smoothingEnabled;
    uint32_t settlingTimeMs;
    uint32_t configReloadCheckSeconds; // v2.1: 0 = load once at start only (v2.0 behavior), like before
} AmbientConfig;

// Defaults match v1.3's hardcoded behavior exactly -- upgrading from a
// build with no ini file present should look identical to before,
// not silently change anything.
static AmbientConfig g_config = {
    .wledHost = "192.168.2.110",
    .wledPort = 4048,
    .ledCountTop = 73, .ledCountRight = 41, .ledCountBottom = 73, .ledCountLeft = 42,
    .startCorner = CORNER_BOTTOM_LEFT,
    .direction = DIR_CLOCKWISE,
    .ledOffset = 0,
    .marginTop = 0, .marginRight = 0, .marginBottom = 0, .marginLeft = 0,
    .scanDepth = 1,
    .brightness = 255,
    .gammaLutIndex = 0, // gamma 1.0 == passthrough, matches v1.3 (no color processing existed)
    .saturation = 0,
    .colorOrder = ORDER_RGB,
    .blackLevel = 0,     // no-op -- matches "no black_level existed before v2.1" exactly
    .whiteLevel = 100,   // no-op
    .darkThreshold = 0,  // disabled -- matches "no dark_threshold existed before v2.1" exactly
    .contrast = 0,       // no-op -- new in v2.2, matches "didn't exist before" like the rest of this block
    .brightnessR = 100, .brightnessG = 100, .brightnessB = 100, // no-op (100 = unchanged, Android convention)
    .gammaR = 100, .gammaG = 100, .gammaB = 100,                 // no-op (100 = unchanged, Android convention)
    .updateFrequencyHz = 30,
    .smoothingEnabled = 0, // off by default -- v1.3 had no smoothing, don't change behavior silently
    .settlingTimeMs = 200,
    .configReloadCheckSeconds = 2, // matches the auto-generated template's default (2s) -- this is a
                                    // brand-new capability with no prior behavior to preserve, so unlike every
                                    // other default above it does NOT need to match "what v2.0 did" (v2.0 simply
                                    // couldn't do this at all). Only matters if ambient_create_default_config()
                                    // ever stops matching this value -- keep the two in sync by hand.
};

// ============================================================
// v2.2: per-channel gamma at an arbitrary percentage (Android's own
// convention -- out = 255*(in/255)^(100/pct), pct=100 == passthrough).
// The fixed kGammaLuts above only cover 8 preset exponents because
// they're baked in at compile time with a real pow() run OFFLINE, on
// a dev machine -- this build links no libm (see Makefile LIBS), so
// there's no pow()/log()/exp() available at runtime to support an
// arbitrary user-chosen percentage the same way.
//
// Rather than either (a) restricting the new per-channel controls to
// the same 8 presets, which defeats the point of porting a
// continuous per-channel control, or (b) linking libm just for this,
// this uses the well-known Ankerl fast log2/exp2 approximation --
// pure integer/float bit manipulation, no transcendental libm calls.
// It's only ever run when the config (re)loads, NOT per-pixel/frame,
// so its handful of extra ULPs of error versus real pow() (verified
// in Python before porting: max off-by-one across a full 0-255 LUT,
// and EXACT agreement across the entire dark end 0-40 that actually
// matters for the black-crush issue -- see handoff §49) cost nothing
// at runtime and are invisible in the final 8-bit LUT anyway.
static float ambient_fast_log2(float x)
{
    union { float f; uint32_t i; } vx = { x };
    union { uint32_t i; float f; } mx = { (vx.i & 0x007FFFFFu) | 0x3f000000u };
    float y = (float)vx.i;
    y *= 1.0f / (1 << 23);
    return y - 124.22551499f - 1.498030302f * mx.f - 1.72587999f / (0.3520887068f + mx.f);
}

static float ambient_fast_exp2(float p)
{
    float offset = (p < 0.0f) ? 1.0f : 0.0f;
    float clipp = (p < -126.0f) ? -126.0f : p;
    int32_t w = (int32_t)clipp;
    float z = clipp - (float)w + offset;
    union { uint32_t i; float f; } v = {
        (uint32_t)((1 << 23) * (clipp + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z))
    };
    return v.f;
}

// x assumed in [0,1] here (normalized 0-255 sample); 0 is handled as
// a special case since log2(0) is undefined and every real gamma
// curve maps black to black anyway.
static float ambient_fast_pow01(float x, float p)
{
    if (x <= 0.0f) return 0.0f;
    return ambient_fast_exp2(p * ambient_fast_log2(x));
}

// Per-channel gamma LUTs for the v2.2 gamma_r/gamma_g/gamma_b knobs.
// Rebuilt on config load/reload (ambient_rebuild_perchannel_gamma_luts
// below), NOT per pixel -- same pattern as everything else in this
// file that could otherwise involve float math on a hot path.
//
// Stored as FLOAT, not uint8_t, deliberately: Android's own gamma LUT
// (buildGammaLut in ColorProcessor.kt) is a FloatArray too, and the
// whole rest of its pipeline stays in Float with no intermediate
// clamp/round until the single final clamp+round back to a byte. An
// 8-bit LUT here would force an early, lossy round-to-byte right
// after gamma, before brightness/contrast/saturation/levels even run
// -- verified in Python before porting that this isn't just
// theoretical: chaining early uint8_t clamps between stages produced
// answers up to ~13/255 off from the real float pipeline on ordinary
// inputs, not merely off-by-one rounding noise (handoff §49).
static float g_gammaLutR[256], g_gammaLutG[256], g_gammaLutB[256];

static void ambient_build_one_gamma_lut(uint32_t pct, float *outLut)
{
    if (pct == 100) { // passthrough -- skip the float math entirely
        for (uint32_t i = 0; i < 256; i++) outLut[i] = (float)i;
        return;
    }
    float invGamma = 100.0f / (float)pct;
    for (uint32_t i = 0; i < 256; i++) {
        float norm = (float)i / 255.0f;
        outLut[i] = ambient_fast_pow01(norm, invGamma) * 255.0f; // left unclamped/unrounded on purpose
    }
}

static void ambient_rebuild_perchannel_gamma_luts(void)
{
    ambient_build_one_gamma_lut(g_config.gammaR, g_gammaLutR);
    ambient_build_one_gamma_lut(g_config.gammaG, g_gammaLutG);
    ambient_build_one_gamma_lut(g_config.gammaB, g_gammaLutB);
}

static bool ambient_file_exists(const char *filename)
{
    struct stat buff;
    return stat(filename, &buff) == 0;
}

// Same sceKernelOpen-not-fopen gotcha plugin_loader's own code comment
// flags ("Does not work, may not have write access") -- not
// rediscovering that the hard way here.
static void ambient_create_default_config(void)
{
    #define AMBIENT_DEFAULT_INI \
        "[network]\n" \
        "; The real WLED controller's IP -- NOT this PC's own IP.\n" \
        "wled_host=192.168.2.110\n" \
        "wled_port=4048\n" \
        "\n" \
        "[layout]\n" \
        "; Physical LED counts per screen edge. Defaults match this\n" \
        "; project's own measured strip (73/41/73/42 = 229 total).\n" \
        "led_count_top=73\n" \
        "led_count_right=41\n" \
        "led_count_bottom=73\n" \
        "led_count_left=42\n" \
        "; Which corner physical LED index 0 sits at, and which way the\n" \
        "; strip runs from there. Valid led_start_corner: bottom_left,\n" \
        "; bottom_right, top_left, top_right. Valid led_direction:\n" \
        "; clockwise, counterclockwise.\n" \
        "led_start_corner=bottom_left\n" \
        "led_direction=clockwise\n" \
        "; If the light show is correct but rotated around the border\n" \
        "; (e.g. everything is one LED off from where it should be),\n" \
        "; adjust this instead of led_start_corner/led_direction.\n" \
        "led_offset=0\n" \
        "; Pixels to inset sampling from the true screen edge, per side.\n" \
        "; Raise these if overscan/black bars are being sampled instead\n" \
        "; of real picture content.\n" \
        "capture_margin_top=0\n" \
        "capture_margin_right=0\n" \
        "capture_margin_bottom=0\n" \
        "capture_margin_left=0\n" \
        "; Sample radius per zone: (2*scan_depth+1)^2 pixels averaged.\n" \
        "; Higher = smoother/less noisy but more CPU per frame.\n" \
        "scan_depth=1\n" \
        "\n" \
        "[color]\n" \
        "; Global brightness scale, 0-255. 255 = no change.\n" \
        "brightness=255\n" \
        "; Must be exactly one of: 1.0 1.4 1.8 2.0 2.2 2.4 2.6 2.8\n" \
        "; (precomputed lookup tables -- no other value is accepted).\n" \
        "gamma=1.0\n" \
        "; -100 (grayscale) to 300 (4x color boost, mostly clipped by\n" \
        "; then -- verified overflow-safe well beyond this, the cap is\n" \
        "; just where it stops looking meaningfully different).\n" \
        "; 0 = unchanged.\n" \
        "saturation=0\n" \
        "; Match your strip's actual wiring. Valid values: RGB, RBG,\n" \
        "; GRB, GBR, BRG, BGR. Most WS2812B/NeoPixel strips are GRB.\n" \
        "color_order=RGB\n" \
        "; Levels adjustment (0-100, percent of the 0-255 range).\n" \
        "; Anything at/below black_level becomes 0; anything at/above\n" \
        "; white_level becomes 255; the rest stretches to fill the gap.\n" \
        "; Defaults (0, 100) are a no-op.\n" \
        "black_level=0\n" \
        "white_level=100\n" \
        "; If a zone's brightest channel drops below this (0-255), that\n" \
        "; zone is forced fully black instead of showing a faint/noisy\n" \
        "; near-black color. Has built-in hysteresis (must rise 10 above\n" \
        "; this value again before turning back on) so it won't flicker\n" \
        "; on scenes hovering right at the threshold. 0 = disabled.\n" \
        "dark_threshold=0\n" \
        "; --- Below this line: ported from the Android version's own\n" \
        "; per-channel color engine. NOTE THE DIFFERENT CONVENTION: these\n" \
        "; use Android's \"100 = unchanged\" percent scale, NOT this file's\n" \
        "; usual \"0 = unchanged\" scale used by saturation/brightness above.\n" \
        "; Contrast: -100..300, 0 = unchanged (stretches/shrinks around\n" \
        "; mid-grey 128, same math as saturation but around brightness\n" \
        "; instead of hue).\n" \
        "contrast=0\n" \
        "; Per-channel brightness, 0-500, 100 = unchanged. Multiplies with\n" \
        "; the single [color] brightness above rather than replacing it.\n" \
        "brightness_r=100\n" \
        "brightness_g=100\n" \
        "brightness_b=100\n" \
        "; Per-channel gamma, 10-500, 100 = unchanged. Unlike the fixed\n" \
        "; gamma= list above (8 preset curves, shared across all 3\n" \
        "; channels), these accept ANY value in range and are independent\n" \
        "; per channel. Applied PER SAMPLE PIXEL before zone-averaging,\n" \
        "; not to the already-averaged zone color -- matches how the\n" \
        "; Android version does it, and avoids a single stray bright\n" \
        "; pixel in an otherwise-dark zone getting averaged in BEFORE\n" \
        "; being gamma-crushed.\n" \
        "gamma_r=100\n" \
        "gamma_g=100\n" \
        "gamma_b=100\n" \
        "\n" \
        "[timing]\n" \
        "; How many times per second to sample and send color.\n" \
        "update_frequency_hz=30\n" \
        "; Blend each new sample with the previous one over roughly\n" \
        "; settling_time_ms, instead of snapping instantly -- reduces\n" \
        "; flicker on fast scene cuts. false = send raw samples as-is.\n" \
        "smoothing_enabled=false\n" \
        "settling_time_ms=200\n" \
        "; How often (seconds) to check this file for changes WHILE\n" \
        "; RUNNING and apply them live -- no need to close/reopen the\n" \
        "; game. 0 = only read this file once, at plugin load (the\n" \
        "; original v2.0 behavior).\n" \
        "config_reload_check_seconds=2\n"

    int32_t f = sceKernelOpen(AMBIENT_CONFIG_PATH, 0x200 | 0x001, 0777);
    if (f < 0) return; // no write access or path issue -- defaults above still apply in memory
    sceKernelWrite(f, AMBIENT_DEFAULT_INI, strlen(AMBIENT_DEFAULT_INI));
    sceKernelClose(f);
    #undef AMBIENT_DEFAULT_INI
}

static ColorOrder parse_color_order(const char *s, ColorOrder fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "RGB")) return ORDER_RGB;
    if (!strcmp(s, "RBG")) return ORDER_RBG;
    if (!strcmp(s, "GRB")) return ORDER_GRB;
    if (!strcmp(s, "GBR")) return ORDER_GBR;
    if (!strcmp(s, "BRG")) return ORDER_BRG;
    if (!strcmp(s, "BGR")) return ORDER_BGR;
    return fallback; // unrecognized -- keep default rather than guess
}

static StartCorner parse_start_corner(const char *s, StartCorner fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "bottom_left"))  return CORNER_BOTTOM_LEFT;
    if (!strcmp(s, "bottom_right")) return CORNER_BOTTOM_RIGHT;
    if (!strcmp(s, "top_left"))     return CORNER_TOP_LEFT;
    if (!strcmp(s, "top_right"))    return CORNER_TOP_RIGHT;
    return fallback;
}

static LedDirection parse_direction(const char *s, LedDirection fallback)
{
    if (!s) return fallback;
    if (!strcmp(s, "clockwise")) return DIR_CLOCKWISE;
    if (!strcmp(s, "counterclockwise")) return DIR_COUNTERCLOCKWISE;
    return fallback;
}

// Matched by exact string, not parsed as a float -- deliberately, to
// avoid any float-parsing code path at runtime in a build with no
// libm linked. Only these 8 values exist as precomputed LUTs anyway
// (see kGammaLuts below), so a free-form value couldn't be honored
// correctly regardless.
static uint32_t parse_gamma_index(const char *s, uint32_t fallback)
{
    static const char *kGammaStrings[NUM_GAMMA_LUTS] = {"1.0","1.4","1.8","2.0","2.2","2.4","2.6","2.8"};
    if (!s) return fallback;
    for (uint32_t i = 0; i < NUM_GAMMA_LUTS; i++) {
        if (!strcmp(s, kGammaStrings[i])) return i;
    }
    return fallback; // unrecognized string -- keep default rather than guess
}

static void ambient_load_config(void)
{
    if (!ambient_file_exists(AMBIENT_CONFIG_PATH)) {
        ambient_create_default_config();
        ambient_rebuild_perchannel_gamma_luts(); // g_gammaLut{R,G,B} are static, zero-init by
                                                  // default -- MUST build the identity LUTs here
                                                  // too, or defaults (gammaR/G/B=100, meant to be
                                                  // a no-op) would silently crush every sample to
                                                  // black instead.
        return; // g_config's compile-time defaults (== v1.3 behavior) stand otherwise
    }

    ini_table_s *table = ini_table_create();
    if (table == NULL || !ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) {
        if (table) ini_table_destroy(table);
        return; // parse failure -- keep defaults rather than run with a half-read config
    }

    const char *v;
    int iv; bool bv;

    if ((v = ini_table_get_entry(table, "network", "wled_host")) != NULL) {
        strncpy(g_config.wledHost, v, sizeof(g_config.wledHost) - 1);
        g_config.wledHost[sizeof(g_config.wledHost) - 1] = '\0';
    }
    if (ini_table_get_entry_as_int(table, "network", "wled_port", &iv) && iv > 0 && iv <= 65535)
        g_config.wledPort = (uint16_t)iv;

    if (ini_table_get_entry_as_int(table, "layout", "led_count_top", &iv) && iv > 0)
        g_config.ledCountTop = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_right", &iv) && iv > 0)
        g_config.ledCountRight = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_bottom", &iv) && iv > 0)
        g_config.ledCountBottom = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "led_count_left", &iv) && iv > 0)
        g_config.ledCountLeft = (uint32_t)iv;
    g_config.startCorner = parse_start_corner(ini_table_get_entry(table, "layout", "led_start_corner"), g_config.startCorner);
    g_config.direction = parse_direction(ini_table_get_entry(table, "layout", "led_direction"), g_config.direction);
    if (ini_table_get_entry_as_int(table, "layout", "led_offset", &iv))
        g_config.ledOffset = iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_top", &iv) && iv >= 0)
        g_config.marginTop = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_right", &iv) && iv >= 0)
        g_config.marginRight = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_bottom", &iv) && iv >= 0)
        g_config.marginBottom = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "capture_margin_left", &iv) && iv >= 0)
        g_config.marginLeft = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "layout", "scan_depth", &iv) && iv >= 0)
        g_config.scanDepth = (uint32_t)iv;

    if (ini_table_get_entry_as_int(table, "color", "brightness", &iv) && iv >= 0 && iv <= 255)
        g_config.brightness = (uint32_t)iv;
    g_config.gammaLutIndex = parse_gamma_index(ini_table_get_entry(table, "color", "gamma"), g_config.gammaLutIndex);
    if (ini_table_get_entry_as_int(table, "color", "saturation", &iv) && iv >= -100 && iv <= 300)
        g_config.saturation = iv;
    g_config.colorOrder = parse_color_order(ini_table_get_entry(table, "color", "color_order"), g_config.colorOrder);
    if (ini_table_get_entry_as_int(table, "color", "black_level", &iv) && iv >= 0 && iv <= 100)
        g_config.blackLevel = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "white_level", &iv) && iv >= 0 && iv <= 100)
        g_config.whiteLevel = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "dark_threshold", &iv) && iv >= 0 && iv <= 255)
        g_config.darkThreshold = (uint32_t)iv;

    // v2.2: Android-ported per-channel color knobs (see AmbientConfig
    // struct comment for why these use a different 0-neutral-point
    // convention than the fields just above).
    if (ini_table_get_entry_as_int(table, "color", "contrast", &iv) && iv >= -100 && iv <= 300)
        g_config.contrast = iv;
    if (ini_table_get_entry_as_int(table, "color", "brightness_r", &iv) && iv >= 0 && iv <= 500)
        g_config.brightnessR = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "brightness_g", &iv) && iv >= 0 && iv <= 500)
        g_config.brightnessG = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "brightness_b", &iv) && iv >= 0 && iv <= 500)
        g_config.brightnessB = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "gamma_r", &iv) && iv >= 10 && iv <= 500)
        g_config.gammaR = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "gamma_g", &iv) && iv >= 10 && iv <= 500)
        g_config.gammaG = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "color", "gamma_b", &iv) && iv >= 10 && iv <= 500)
        g_config.gammaB = (uint32_t)iv;
    ambient_rebuild_perchannel_gamma_luts();

    if (ini_table_get_entry_as_int(table, "timing", "update_frequency_hz", &iv) && iv > 0 && iv <= 240)
        g_config.updateFrequencyHz = (uint32_t)iv;
    if (ini_table_get_entry_as_bool(table, "timing", "smoothing_enabled", &bv))
        g_config.smoothingEnabled = bv ? 1 : 0;
    if (ini_table_get_entry_as_int(table, "timing", "settling_time_ms", &iv) && iv >= 0)
        g_config.settlingTimeMs = (uint32_t)iv;
    if (ini_table_get_entry_as_int(table, "timing", "config_reload_check_seconds", &iv) && iv >= 0)
        g_config.configReloadCheckSeconds = (uint32_t)iv;

    ini_table_destroy(table);
}

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

static int g_wledSockfd = -1; // one socket, held open -- avoid a socket()/close() per send at frame rate

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
#define DEBUG_IP "192.168.2.117"   // dev-PC debug listener -- confirm this still matches (handoff §2, changes over time)

static void debug_send_raw(const uint8_t *data, int len)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return;

    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(WLED_PORT);
    if (inet_pton(AF_INET, DEBUG_IP, &destAddr.sin_addr) != 1) {
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
static void send_timing_packet(uint32_t minUs, uint32_t maxUs, uint32_t avgUs,
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
#define CONFIG_DEBUG_PREVIEW_LEN 16
static void ambient_read_content_preview(uint8_t *out, size_t previewLen)
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
static void send_config_reload_debug_packet(uint32_t event, uint32_t statErrno,
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

static void wled_send_rgb_zones(const uint8_t *rgbTriplets, int numZones)
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

// ============================================================
// Detile math -- confirmed-correct BASE params only (§21/§22).
// Identical formulas to ps4_detile_2dthin.c / detile_verify_probe,
// just without the Neo branch, which has no reason to run every frame
// now that base is the settled answer for this hardware.
// ============================================================

typedef struct {
    uint32_t pipeConfig, numPipes, bankWidth, bankHeight, numBanks;
    uint32_t macroTileWidth, macroTileHeight, paddedWidth, tileSplitBytes;
    uint32_t pipeInterleaveBits, pipeBits, bankBits;
} TileParams;

static const TileParams kParamsBase = {
    0xc, 8, 1, 1, 16, 128, 64, 1920, 512, 8, 3, 4
};

// paddedHeight isn't in TileParams (only paddedWidth is used by the
// offset math itself), but it's needed here for the v1.1 bounds check
// below -- 1088 for base params, per handoff §12 (ceil(1080/64)*64).
#define BASE_PADDED_HEIGHT 1088
#define BASE_PADDED_BUFFER_BYTES ((uint64_t)1920 * BASE_PADDED_HEIGHT * 4)

static uint32_t getElementIndex32(uint32_t x, uint32_t y)
{
    uint32_t elem = 0;
    elem |= ((x >> 0) & 0x1) << 0;
    elem |= ((x >> 1) & 0x1) << 1;
    elem |= ((y >> 0) & 0x1) << 2;
    elem |= ((x >> 2) & 0x1) << 3;
    elem |= ((y >> 1) & 0x1) << 4;
    elem |= ((y >> 2) & 0x1) << 5;
    return elem;
}

static uint32_t getPipeIndex(uint32_t x, uint32_t y) // base only -> P8_32x32_16x16
{
    uint32_t pipe = 0;
    pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
    pipe |= (((x >> 4) ^ (y >> 4))            & 0x1) << 1;
    pipe |= (((x >> 5) ^ (y >> 5))            & 0x1) << 2;
    return pipe;
}

static uint32_t fastIntLog2_(uint32_t i)
{
    uint32_t log2 = 0;
    while ((i >>= 1) != 0) log2++;
    return log2;
}

static uint32_t getBankIndex(uint32_t x, uint32_t y, const TileParams *p) // base only -> numBanks=16
{
    const uint32_t x_shift_offset = fastIntLog2_(p->bankWidth * p->numPipes);
    const uint32_t y_shift_offset = fastIntLog2_(p->bankHeight);
    const uint32_t xs = x >> x_shift_offset;
    const uint32_t ys = y >> y_shift_offset;
    uint32_t bank = 0;
    bank |= (((xs >> 3) ^ (ys >> 6))             & 0x1) << 0;
    bank |= (((xs >> 4) ^ (ys >> 5) ^ (ys >> 6)) & 0x1) << 1;
    bank |= (((xs >> 5) ^ (ys >> 4))             & 0x1) << 2; // confirmed against GnmTiler.cpp:310, see main.c v2.1 fix
    bank |= (((xs >> 6) ^ (ys >> 3))             & 0x1) << 3;
    return bank;
}

static uint64_t getTiledElementByteOffset(const TileParams *p, uint32_t x, uint32_t y)
{
    uint64_t element_index = getElementIndex32(x, y);
    uint64_t pipe = getPipeIndex(x, y);
    uint64_t bank = getBankIndex(x, y, p);

    uint32_t tile_bytes = (8 * 8 * 1 * 32 * 1 + 7) / 8; // 256, our fixed 32bpp case
    uint64_t element_offset = element_index * 32;

    uint64_t macro_tile_bytes = (p->macroTileWidth / 8) * (p->macroTileHeight / 8)
                                 * tile_bytes / (p->numPipes * p->numBanks);
    uint64_t macro_tiles_per_row = p->paddedWidth / p->macroTileWidth;
    uint64_t macro_tile_row_index = y / p->macroTileHeight;
    uint64_t macro_tile_column_index = x / p->macroTileWidth;
    uint64_t macro_tile_index = (macro_tile_row_index * macro_tiles_per_row) + macro_tile_column_index;
    uint64_t macro_tile_offset = macro_tile_index * macro_tile_bytes;

    uint64_t tile_row_index = (y / 8) % p->bankHeight;
    uint64_t tile_column_index = ((x / 8) / p->numPipes) % p->bankWidth;
    uint64_t tile_index = (tile_row_index * p->bankWidth) + tile_column_index;
    uint64_t tile_offset = tile_index * tile_bytes;

    uint64_t total_offset = (macro_tile_offset + tile_offset) * 8 + element_offset;
    uint64_t bitOffset = total_offset & 0x7;
    total_offset /= 8;

    uint32_t pipeInterleaveMask = (1u << p->pipeInterleaveBits) - 1;
    uint64_t pipe_interleave_offset = total_offset & pipeInterleaveMask;
    uint64_t offset = total_offset >> p->pipeInterleaveBits;

    uint64_t finalByteOffset = pipe_interleave_offset |
        (pipe   << (p->pipeInterleaveBits)) |
        (bank   << (p->pipeInterleaveBits + p->pipeBits)) |
        (offset << (p->pipeInterleaveBits + p->pipeBits + p->bankBits));

    return ((finalByteOffset << 3) | bitOffset) / 8;
}

// ============================================================
// Runtime pixel-format dispatch -- same principle as
// ps4_detile_2dthin.c's getUnpackFnForFormat(): read live off the
// registration hook, never assumed at compile time. Unknown format ->
// NULL -> this plugin explicitly stops sending color instead of
// guessing (see the flip hook below).
// ============================================================

typedef void (*PixelUnpackFn)(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b);

static void unpackA2R10G10B10_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint32_t r10 = (px >> 20) & 0x3FF;
    uint32_t g10 = (px >> 10) & 0x3FF;
    uint32_t b10 = (px >> 0)  & 0x3FF;
    *r = (uint8_t)(r10 >> 2);
    *g = (uint8_t)(g10 >> 2);
    *b = (uint8_t)(b10 >> 2);
}

static void unpackA8R8G8B8_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((px >> 16) & 0xFF);
    *g = (uint8_t)((px >> 8)  & 0xFF);
    *b = (uint8_t)(px & 0xFF);
}

// ------------------------------------------------------------------
// HDR (A2R10G10B10_BT2020_PQ) decode -- ported from
// decode_verification_dump.py's unpack_a2r10g10b10_bt2020_pq, verified
// there against a real capture + ground-truth screenshot (5 test
// points, 3/5 usable for real color -- PQ decode landed 6-12x closer
// to the actual on-screen pixel than plain truncation on the two
// midtone points). This format was PREVIOUSLY routed to
// unpackA2R10G10B10_to_rgb888 above -- the same plain truncation used
// for gamma-encoded SDR -- which is why HDR colors looked wrong: PQ
// (SMPTE ST 2084) is a fundamentally different curve, not a gamma
// variant. Bit LAYOUT (which 10 bits are R/G/B/A) is unchanged from
// SDR A2R10G10B10 -- only the interpretation of the numbers differs.
//
// PQ_REFERENCE_WHITE_NITS / PQ_TONE_MAP_MAX_NITS are the same tunable
// ASSUMPTIONS as the Python script, not measured constants -- if HDR
// colors still look off after this ships, these two are the knobs to
// adjust (empirically, against a real capture, same method as §21/§22
// and the screenshot verification). The PQ EOTF and BT.2020->BT.709
// matrix below are fixed standards and should not need touching.
// ------------------------------------------------------------------

#define PQ_REFERENCE_WHITE_NITS 203.0
#define PQ_TONE_MAP_MAX_NITS    1000.0
#define SRGB_LUT_SIZE           4096

// Precomputed on the dev machine (Python, matching
// decode_verification_dump.py's unpack_a2r10g10b10_bt2020_pq exactly)
// and verified to reproduce identical output to a live pow()-based
// version on this session's 5 real captured pixels (max 1-unit diff,
// from LUT quantization -- well within the photo-comparison noise
// floor already established). Table lookups only, no libm calls --
// see buildZoneGeometry's comment above: this build does not link
// libm, and after the printf/klog crash history in this project,
// nothing here should assume a library "just works" without checking.
//
// kPqEotfNitsLut: indexed by the raw 10-bit code value (0-1023) ->
// linear light in nits (SMPTE ST 2084 inverse EOTF). Domain is exactly
// 1024 discrete values, so a direct 1-to-1 LUT loses zero precision
// versus computing pow() live.
static const double kPqEotfNitsLut[1024] = {
    0.000000, 0.000040, 0.000131, 0.000262, 0.000432, 0.000637, 0.000880, 0.001159,
    0.001474, 0.001826, 0.002215, 0.002642, 0.003108, 0.003612, 0.004156, 0.004741,
    0.005366, 0.006032, 0.006742, 0.007494, 0.008290, 0.009131, 0.010018, 0.010951,
    0.011931, 0.012959, 0.014036, 0.015163, 0.016340, 0.017569, 0.018850, 0.020185,
    0.021574, 0.023019, 0.024519, 0.026077, 0.027693, 0.029367, 0.031103, 0.032899,
    0.034757, 0.036679, 0.038664, 0.040715, 0.042833, 0.045017, 0.047271, 0.049593,
    0.051987, 0.054452, 0.056991, 0.059603, 0.062291, 0.065055, 0.067897, 0.070818,
    0.073818, 0.076900, 0.080065, 0.083313, 0.086646, 0.090065, 0.093572, 0.097168,
    0.100854, 0.104631, 0.108501, 0.112465, 0.116525, 0.120681, 0.124936, 0.129291,
    0.133746, 0.138304, 0.142966, 0.147734, 0.152608, 0.157591, 0.162684, 0.167888,
    0.173205, 0.178637, 0.184185, 0.189850, 0.195635, 0.201540, 0.207568, 0.213720,
    0.219998, 0.226404, 0.232938, 0.239604, 0.246402, 0.253334, 0.260403, 0.267609,
    0.274956, 0.282443, 0.290074, 0.297850, 0.305774, 0.313846, 0.322069, 0.330445,
    0.338975, 0.347663, 0.356508, 0.365515, 0.374684, 0.384018, 0.393519, 0.403188,
    0.413028, 0.423042, 0.433230, 0.443596, 0.454142, 0.464869, 0.475780, 0.486877,
    0.498163, 0.509639, 0.521309, 0.533174, 0.545236, 0.557499, 0.569964, 0.582634,
    0.595511, 0.608598, 0.621898, 0.635412, 0.649143, 0.663094, 0.677268, 0.691667,
    0.706294, 0.721151, 0.736241, 0.751566, 0.767131, 0.782936, 0.798986, 0.815282,
    0.831828, 0.848626, 0.865680, 0.882992, 0.900566, 0.918404, 0.936509, 0.954884,
    0.973533, 0.992458, 1.011662, 1.031149, 1.050923, 1.070985, 1.091339, 1.111989,
    1.132938, 1.154189, 1.175745, 1.197610, 1.219788, 1.242281, 1.265093, 1.288228,
    1.311690, 1.335481, 1.359606, 1.384067, 1.408870, 1.434017, 1.459512, 1.485359,
    1.511562, 1.538125, 1.565051, 1.592344, 1.620009, 1.648049, 1.676469, 1.705272,
    1.734463, 1.764045, 1.794023, 1.824401, 1.855184, 1.886375, 1.917979, 1.950001,
    1.982444, 2.015313, 2.048613, 2.082348, 2.116523, 2.151142, 2.186210, 2.221732,
    2.257713, 2.294156, 2.331068, 2.368452, 2.406314, 2.444660, 2.483492, 2.522818,
    2.562642, 2.602968, 2.643803, 2.685152, 2.727019, 2.769410, 2.812330, 2.855786,
    2.899781, 2.944323, 2.989416, 3.035066, 3.081278, 3.128059, 3.175414, 3.223349,
    3.271871, 3.320983, 3.370694, 3.421008, 3.471933, 3.523473, 3.575635, 3.628426,
    3.681852, 3.735919, 3.790634, 3.846002, 3.902031, 3.958728, 4.016098, 4.074148,
    4.132886, 4.192318, 4.252451, 4.313292, 4.374848, 4.437126, 4.500134, 4.563877,
    4.628365, 4.693603, 4.759600, 4.826362, 4.893898, 4.962215, 5.031320, 5.101222,
    5.171928, 5.243446, 5.315784, 5.388950, 5.462952, 5.537799, 5.613497, 5.690057,
    5.767485, 5.845791, 5.924983, 6.005069, 6.086059, 6.167961, 6.250783, 6.334535,
    6.419226, 6.504865, 6.591460, 6.679022, 6.767559, 6.857081, 6.947597, 7.039116,
    7.131649, 7.225205, 7.319794, 7.415426, 7.512110, 7.609857, 7.708678, 7.808581,
    7.909577, 8.011678, 8.114892, 8.219232, 8.324707, 8.431329, 8.539108, 8.648056,
    8.758182, 8.869499, 8.982018, 9.095750, 9.210706, 9.326899, 9.444339, 9.563039,
    9.683010, 9.804264, 9.926814, 10.050671, 10.175848, 10.302358, 10.430212, 10.559424,
    10.690005, 10.821970, 10.955330, 11.090099, 11.226290, 11.363916, 11.502991, 11.643529,
    11.785542, 11.929044, 12.074050, 12.220574, 12.368629, 12.518230, 12.669390, 12.822126,
    12.976450, 13.132379, 13.289926, 13.449107, 13.609937, 13.772431, 13.936604, 14.102473,
    14.270052, 14.439357, 14.610405, 14.783211, 14.957792, 15.134164, 15.312344, 15.492347,
    15.674191, 15.857893, 16.043470, 16.230938, 16.420316, 16.611621, 16.804871, 17.000083,
    17.197275, 17.396465, 17.597672, 17.800915, 18.006211, 18.213579, 18.423039, 18.634609,
    18.848310, 19.064159, 19.282177, 19.502383, 19.724798, 19.949442, 20.176334, 20.405495,
    20.636946, 20.870708, 21.106801, 21.345248, 21.586068, 21.829284, 22.074917, 22.322990,
    22.573523, 22.826541, 23.082064, 23.340117, 23.600721, 23.863899, 24.129676, 24.398074,
    24.669117, 24.942829, 25.219234, 25.498356, 25.780220, 26.064850, 26.352272, 26.642511,
    26.935591, 27.231539, 27.530380, 27.832140, 28.136846, 28.444524, 28.755200, 29.068902,
    29.385657, 29.705492, 30.028435, 30.354513, 30.683755, 31.016188, 31.351843, 31.690747,
    32.032930, 32.378420, 32.727248, 33.079444, 33.435036, 33.794057, 34.156536, 34.522504,
    34.891993, 35.265034, 35.641659, 36.021899, 36.405786, 36.793355, 37.184636, 37.579664,
    37.978471, 38.381091, 38.787559, 39.197909, 39.612174, 40.030390, 40.452593, 40.878816,
    41.309097, 41.743472, 42.181976, 42.624646, 43.071520, 43.522634, 43.978027, 44.437736,
    44.901799, 45.370256, 45.843145, 46.320506, 46.802377, 47.288800, 47.779814, 48.275460,
    48.775780, 49.280814, 49.790604, 50.305193, 50.824622, 51.348936, 51.878176, 52.412386,
    52.951611, 53.495895, 54.045282, 54.599817, 55.159547, 55.724517, 56.294773, 56.870362,
    57.451330, 58.037727, 58.629598, 59.226994, 59.829962, 60.438551, 61.052812, 61.672794,
    62.298548, 62.930124, 63.567574, 64.210950, 64.860304, 65.515688, 66.177156, 66.844761,
    67.518558, 68.198600, 68.884944, 69.577643, 70.276755, 70.982336, 71.694443, 72.413132,
    73.138462, 73.870492, 74.609280, 75.354886, 76.107370, 76.866792, 77.633213, 78.406695,
    79.187300, 79.975090, 80.770129, 81.572480, 82.382207, 83.199376, 84.024052, 84.856300,
    85.696187, 86.543781, 87.399148, 88.262358, 89.133479, 90.012580, 90.899732, 91.795005,
    92.698470, 93.610200, 94.530267, 95.458744, 96.395705, 97.341224, 98.295376, 99.258238,
    100.229886, 101.210396, 102.199846, 103.198315, 104.205882, 105.222627, 106.248630, 107.283972,
    108.328736, 109.383004, 110.446858, 111.520384, 112.603667, 113.696790, 114.799842, 115.912909,
    117.036078, 118.169439, 119.313080, 120.467092, 121.631566, 122.806594, 123.992267, 125.188679,
    126.395925, 127.614099, 128.843297, 130.083615, 131.335152, 132.598006, 133.872275, 135.158060,
    136.455461, 137.764581, 139.085523, 140.418389, 141.763284, 143.120314, 144.489586, 145.871205,
    147.265282, 148.671924, 150.091242, 151.523348, 152.968352, 154.426369, 155.897513, 157.381897,
    158.879639, 160.390856, 161.915666, 163.454187, 165.006541, 166.572848, 168.153231, 169.747813,
    171.356719, 172.980074, 174.618005, 176.270640, 177.938108, 179.620539, 181.318065, 183.030816,
    184.758928, 186.502536, 188.261773, 190.036779, 191.827692, 193.634650, 195.457796, 197.297270,
    199.153216, 201.025780, 202.915105, 204.821341, 206.744635, 208.685137, 210.642998, 212.618371,
    214.611409, 216.622268, 218.651104, 220.698074, 222.763339, 224.847060, 226.949397, 229.070515,
    231.210579, 233.369755, 235.548212, 237.746119, 239.963646, 242.200967, 244.458256, 246.735687,
    249.033439, 251.351690, 253.690620, 256.050411, 258.431247, 260.833313, 263.256796, 265.701884,
    268.168768, 270.657640, 273.168692, 275.702121, 278.258124, 280.836899, 283.438647, 286.063570,
    288.711873, 291.383762, 294.079445, 296.799130, 299.543031, 302.311360, 305.104334, 307.922168,
    310.765084, 313.633301, 316.527044, 319.446537, 322.392008, 325.363686, 328.361803, 331.386591,
    334.438288, 337.517130, 340.623357, 343.757211, 346.918937, 350.108780, 353.326990, 356.573816,
    359.849513, 363.154336, 366.488542, 369.852391, 373.246145, 376.670070, 380.124433, 383.609502,
    387.125550, 390.672851, 394.251683, 397.862324, 401.505056, 405.180165, 408.887936, 412.628660,
    416.402628, 420.210137, 424.051483, 427.926966, 431.836890, 435.781560, 439.761286, 443.776377,
    447.827149, 451.913918, 456.037005, 460.196732, 464.393424, 468.627412, 472.899025, 477.208600,
    481.556473, 485.942985, 490.368482, 494.833309, 499.337817, 503.882359, 508.467292, 513.092977,
    517.759775, 522.468054, 527.218184, 532.010538, 536.845493, 541.723428, 546.644727, 551.609779,
    556.618972, 561.672702, 566.771366, 571.915367, 577.105108, 582.341000, 587.623454, 592.952888,
    598.329721, 603.754378, 609.227287, 614.748879, 620.319592, 625.939864, 631.610141, 637.330870,
    643.102503, 648.925497, 654.800312, 660.727415, 666.707273, 672.740360, 678.827154, 684.968139,
    691.163799, 697.414628, 703.721120, 710.083776, 716.503102, 722.979606, 729.513804, 736.106215,
    742.757363, 749.467777, 756.237991, 763.068543, 769.959978, 776.912844, 783.927695, 791.005092,
    798.145597, 805.349781, 812.618219, 819.951490, 827.350182, 834.814884, 842.346194, 849.944713,
    857.611051, 865.345819, 873.149639, 881.023134, 888.966936, 896.981682, 905.068014, 913.226582,
    921.458040, 929.763050, 938.142279, 946.596400, 955.126093, 963.732045, 972.414948, 981.175502,
    990.014412, 998.932391, 1007.930158, 1017.008440, 1026.167968, 1035.409483, 1044.733731, 1054.141466,
    1063.633450, 1073.210449, 1082.873239, 1092.622604, 1102.459333, 1112.384223, 1122.398080, 1132.501716,
    1142.695952, 1152.981616, 1163.359544, 1173.830580, 1184.395576, 1195.055392, 1205.810897, 1216.662967,
    1227.612486, 1238.660348, 1249.807456, 1261.054718, 1272.403055, 1283.853395, 1295.406673, 1307.063835,
    1318.825837, 1330.693642, 1342.668223, 1354.750562, 1366.941651, 1379.242491, 1391.654093, 1404.177477,
    1416.813674, 1429.563724, 1442.428677, 1455.409593, 1468.507542, 1481.723606, 1495.058875, 1508.514451,
    1522.091445, 1535.790982, 1549.614194, 1563.562227, 1577.636235, 1591.837386, 1606.166858, 1620.625840,
    1635.215533, 1649.937150, 1664.791915, 1679.781064, 1694.905846, 1710.167520, 1725.567359, 1741.106648,
    1756.786684, 1772.608777, 1788.574249, 1804.684437, 1820.940687, 1837.344362, 1853.896837, 1870.599500,
    1887.453752, 1904.461009, 1921.622699, 1938.940267, 1956.415169, 1974.048877, 1991.842877, 2009.798668,
    2027.917766, 2046.201702, 2064.652018, 2083.270277, 2102.058053, 2121.016936, 2140.148534, 2159.454468,
    2178.936376, 2198.595913, 2218.434749, 2238.454572, 2258.657084, 2279.044006, 2299.617075, 2320.378046,
    2341.328690, 2362.470797, 2383.806174, 2405.336645, 2427.064053, 2448.990260, 2471.117145, 2493.446606,
    2515.980560, 2538.720944, 2561.669711, 2584.828838, 2608.200317, 2631.786164, 2655.588412, 2679.609116,
    2703.850351, 2728.314212, 2753.002815, 2777.918298, 2803.062820, 2828.438562, 2854.047725, 2879.892533,
    2905.975234, 2932.298096, 2958.863411, 2985.673494, 3012.730682, 3040.037337, 3067.595843, 3095.408611,
    3123.478073, 3151.806686, 3180.396934, 3209.251323, 3238.372387, 3267.762684, 3297.424797, 3327.361338,
    3357.574941, 3388.068271, 3418.844018, 3449.904898, 3481.253658, 3512.893068, 3544.825930, 3577.055074,
    3609.583356, 3642.413664, 3675.548913, 3708.992051, 3742.746052, 3776.813922, 3811.198699, 3845.903450,
    3880.931273, 3916.285300, 3951.968692, 3987.984646, 4024.336388, 4061.027178, 4098.060311, 4135.439114,
    4173.166949, 4211.247212, 4249.683334, 4288.478781, 4327.637056, 4367.161695, 4407.056274, 4447.324404,
    4487.969732, 4528.995944, 4570.406765, 4612.205957, 4654.397320, 4696.984696, 4739.971964, 4783.363044,
    4827.161898, 4871.372527, 4915.998975, 4961.045326, 5006.515710, 5052.414295, 5098.745296, 5145.512971,
    5192.721622, 5240.375596, 5288.479284, 5337.037125, 5386.053602, 5435.533247, 5485.480639, 5535.900403,
    5586.797213, 5638.175795, 5690.040919, 5742.397411, 5795.250142, 5848.604038, 5902.464075, 5956.835283,
    6011.722742, 6067.131588, 6123.067010, 6179.534252, 6236.538613, 6294.085448, 6352.180168, 6410.828244,
    6470.035200, 6529.806623, 6590.148156, 6651.065504, 6712.564430, 6774.650761, 6837.330384, 6900.609248,
    6964.493368, 7028.988821, 7094.101747, 7159.838356, 7226.204919, 7293.207779, 7360.853342, 7429.148085,
    7498.098554, 7567.711365, 7637.993204, 7708.950830, 7780.591074, 7852.920838, 7925.947102, 7999.676919,
    8074.117416, 8149.275800, 8225.159354, 8301.775439, 8379.131497, 8457.235047, 8536.093693, 8615.715119,
    8696.107093, 8777.277466, 8859.234175, 8941.985242, 9025.538776, 9109.902976, 9195.086128, 9281.096608,
    9367.942884, 9455.633515, 9544.177155, 9633.582551, 9723.858546, 9815.014078, 9907.058184, 10000.000000,
};

// kSrgbEncode8Lut: indexed by a linear value in [0,1] quantized to
// 4096 steps -> final 8-bit sRGB-encoded output. This stage's input is
// continuous (post tone-map, post gamut matrix), so this is a
// quantized approximation rather than an exact table -- 4096 steps
// keeps quantization error under 1 code value almost everywhere,
// same order as the LUT-vs-pow() check above.
static const uint8_t kSrgbEncode8Lut[SRGB_LUT_SIZE] = {
    0, 1, 2, 2, 3, 4, 5, 6, 6, 7, 8, 9, 10, 10, 11, 12,
    13, 13, 14, 15, 15, 16, 16, 17, 18, 18, 19, 19, 20, 20, 21, 21,
    22, 22, 23, 23, 23, 24, 24, 25, 25, 25, 26, 26, 27, 27, 27, 28,
    28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32, 32, 32, 33, 33, 33,
    34, 34, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 37, 38, 38,
    38, 38, 39, 39, 39, 40, 40, 40, 40, 41, 41, 41, 41, 42, 42, 42,
    42, 43, 43, 43, 43, 43, 44, 44, 44, 44, 45, 45, 45, 45, 46, 46,
    46, 46, 46, 47, 47, 47, 47, 48, 48, 48, 48, 48, 49, 49, 49, 49,
    49, 50, 50, 50, 50, 50, 51, 51, 51, 51, 51, 52, 52, 52, 52, 52,
    53, 53, 53, 53, 53, 54, 54, 54, 54, 54, 55, 55, 55, 55, 55, 55,
    56, 56, 56, 56, 56, 57, 57, 57, 57, 57, 57, 58, 58, 58, 58, 58,
    58, 59, 59, 59, 59, 59, 59, 60, 60, 60, 60, 60, 60, 61, 61, 61,
    61, 61, 61, 62, 62, 62, 62, 62, 62, 63, 63, 63, 63, 63, 63, 64,
    64, 64, 64, 64, 64, 64, 65, 65, 65, 65, 65, 65, 66, 66, 66, 66,
    66, 66, 66, 67, 67, 67, 67, 67, 67, 67, 68, 68, 68, 68, 68, 68,
    68, 69, 69, 69, 69, 69, 69, 69, 70, 70, 70, 70, 70, 70, 70, 71,
    71, 71, 71, 71, 71, 71, 72, 72, 72, 72, 72, 72, 72, 72, 73, 73,
    73, 73, 73, 73, 73, 74, 74, 74, 74, 74, 74, 74, 74, 75, 75, 75,
    75, 75, 75, 75, 75, 76, 76, 76, 76, 76, 76, 76, 77, 77, 77, 77,
    77, 77, 77, 77, 78, 78, 78, 78, 78, 78, 78, 78, 78, 79, 79, 79,
    79, 79, 79, 79, 79, 80, 80, 80, 80, 80, 80, 80, 80, 81, 81, 81,
    81, 81, 81, 81, 81, 81, 82, 82, 82, 82, 82, 82, 82, 82, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 84, 84, 84, 84, 84, 84, 84, 84, 84,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 86, 86, 86, 86, 86, 86, 86,
    86, 86, 87, 87, 87, 87, 87, 87, 87, 87, 87, 88, 88, 88, 88, 88,
    88, 88, 88, 88, 88, 89, 89, 89, 89, 89, 89, 89, 89, 89, 90, 90,
    90, 90, 90, 90, 90, 90, 90, 90, 91, 91, 91, 91, 91, 91, 91, 91,
    91, 91, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 93, 93, 93, 93,
    93, 93, 93, 93, 93, 93, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94,
    95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 96, 96, 96, 96, 96, 96,
    96, 96, 96, 96, 96, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 102, 102, 102, 102, 102,
    102, 102, 102, 102, 102, 102, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103,
    103, 103, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 105, 105, 105,
    105, 105, 105, 105, 105, 105, 105, 105, 105, 106, 106, 106, 106, 106, 106, 106,
    106, 106, 106, 106, 106, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
    107, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 110, 110, 110, 110, 110, 110, 110,
    110, 110, 110, 110, 110, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
    111, 111, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 113, 113,
    113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 114, 114, 114, 114, 114,
    114, 114, 114, 114, 114, 114, 114, 114, 115, 115, 115, 115, 115, 115, 115, 115,
    115, 115, 115, 115, 115, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116,
    116, 116, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
    118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 119, 119, 119,
    119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 120, 120, 120, 120, 120,
    120, 120, 120, 120, 120, 120, 120, 120, 120, 121, 121, 121, 121, 121, 121, 121,
    121, 121, 121, 121, 121, 121, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
    122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,
    123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124,
    124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125,
    126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 129, 129, 129, 129,
    129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 131, 131, 131, 131, 131, 131,
    131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 133, 133, 133, 133, 133, 133, 133,
    133, 133, 133, 133, 133, 133, 133, 133, 133, 134, 134, 134, 134, 134, 134, 134,
    134, 134, 134, 134, 134, 134, 134, 134, 134, 135, 135, 135, 135, 135, 135, 135,
    135, 135, 135, 135, 135, 135, 135, 135, 135, 136, 136, 136, 136, 136, 136, 136,
    136, 136, 136, 136, 136, 136, 136, 136, 136, 137, 137, 137, 137, 137, 137, 137,
    137, 137, 137, 137, 137, 137, 137, 137, 137, 138, 138, 138, 138, 138, 138, 138,
    138, 138, 138, 138, 138, 138, 138, 138, 138, 139, 139, 139, 139, 139, 139, 139,
    139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 140, 140, 140, 140, 140, 140,
    140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 141, 141, 141, 141, 141,
    141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 142, 142, 142, 142,
    142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 143, 143, 143,
    143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 144, 144,
    144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 145,
    145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
    145, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
    146, 146, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
    147, 147, 147, 147, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
    148, 148, 148, 148, 148, 148, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
    149, 149, 149, 149, 149, 149, 149, 149, 150, 150, 150, 150, 150, 150, 150, 150,
    150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 151, 151, 151, 151, 151,
    151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 152, 152, 152,
    152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152,
    153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153,
    153, 153, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
    154, 154, 154, 154, 154, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155,
    155, 155, 155, 155, 155, 155, 155, 155, 156, 156, 156, 156, 156, 156, 156, 156,
    156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 157, 157, 157, 157,
    157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 158,
    158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
    158, 158, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159,
    159, 159, 159, 159, 159, 159, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160,
    160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 161, 161, 161, 161, 161, 161,
    161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 162, 162,
    162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162,
    162, 162, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164,
    164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 165, 165, 165, 165, 165,
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165,
    166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166,
    166, 166, 166, 166, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167,
    167, 167, 167, 167, 167, 167, 167, 167, 167, 168, 168, 168, 168, 168, 168, 168,
    168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 169,
    169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169,
    169, 169, 169, 169, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170,
    170, 170, 170, 170, 170, 170, 170, 170, 170, 171, 171, 171, 171, 171, 171, 171,
    171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 172,
    172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
    172, 172, 172, 172, 172, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
    173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 174, 174, 174, 174, 174,
    174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
    174, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175,
    175, 175, 175, 175, 175, 175, 175, 176, 176, 176, 176, 176, 176, 176, 176, 176,
    176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 177, 177,
    177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177,
    177, 177, 177, 177, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178,
    178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 179, 179, 179, 179, 179,
    179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179,
    179, 179, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
    180, 180, 180, 180, 180, 180, 180, 180, 180, 181, 181, 181, 181, 181, 181, 181,
    181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
    182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
    182, 182, 182, 182, 182, 182, 182, 182, 183, 183, 183, 183, 183, 183, 183, 183,
    183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 184,
    184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
    184, 184, 184, 184, 184, 184, 184, 185, 185, 185, 185, 185, 185, 185, 185, 185,
    185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 186,
    186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186,
    186, 186, 186, 186, 186, 186, 186, 187, 187, 187, 187, 187, 187, 187, 187, 187,
    187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187,
    188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188,
    188, 188, 188, 188, 188, 188, 188, 188, 189, 189, 189, 189, 189, 189, 189, 189,
    189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189,
    189, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190,
    190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 191, 191, 191, 191, 191, 191,
    191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191,
    191, 191, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
    192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 193, 193, 193, 193,
    193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193,
    193, 193, 193, 193, 193, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194,
    194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 195, 195,
    195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195,
    195, 195, 195, 195, 195, 195, 195, 195, 196, 196, 196, 196, 196, 196, 196, 196,
    196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196,
    196, 196, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197,
    197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 198, 198, 198, 198,
    198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
    198, 198, 198, 198, 198, 198, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
    199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
    200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200,
    200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 201, 201, 201, 201, 201,
    201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201,
    201, 201, 201, 201, 201, 201, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202,
    202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202,
    202, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203,
    203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 204, 204, 204, 204,
    204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204,
    204, 204, 204, 204, 204, 204, 204, 205, 205, 205, 205, 205, 205, 205, 205, 205,
    205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205,
    205, 205, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206,
    206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 207, 207,
    207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
    207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 208, 208, 208, 208, 208, 208,
    208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208,
    208, 208, 208, 208, 208, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209,
    209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209,
    209, 209, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
    210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 211, 211,
    211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
    211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 212, 212, 212, 212, 212, 212,
    212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
    212, 212, 212, 212, 212, 212, 212, 213, 213, 213, 213, 213, 213, 213, 213, 213,
    213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
    213, 213, 213, 213, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
    214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
    214, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
    215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 216, 216,
    216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216,
    216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 217, 217, 217, 217, 217,
    217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217,
    217, 217, 217, 217, 217, 217, 217, 217, 217, 218, 218, 218, 218, 218, 218, 218,
    218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218,
    218, 218, 218, 218, 218, 218, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219,
    219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219,
    219, 219, 219, 219, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
    220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
    220, 220, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
    221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
    221, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222,
    222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 223,
    223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223,
    223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 224, 224,
    224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
    224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 225, 225, 225, 225,
    225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
    225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 226, 226, 226, 226, 226,
    226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
    226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 227, 227, 227, 227, 227, 227,
    227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
    227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 228, 228, 228, 228, 228, 228,
    228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
    228, 228, 228, 228, 228, 228, 228, 228, 228, 229, 229, 229, 229, 229, 229, 229,
    229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
    229, 229, 229, 229, 229, 229, 229, 229, 229, 230, 230, 230, 230, 230, 230, 230,
    230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
    230, 230, 230, 230, 230, 230, 230, 230, 230, 231, 231, 231, 231, 231, 231, 231,
    231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
    231, 231, 231, 231, 231, 231, 231, 231, 231, 232, 232, 232, 232, 232, 232, 232,
    232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
    232, 232, 232, 232, 232, 232, 232, 232, 232, 233, 233, 233, 233, 233, 233, 233,
    233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
    233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 234, 234, 234, 234, 234, 234,
    234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
    234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 235, 235, 235, 235, 235, 235,
    235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
    235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 236, 236, 236, 236, 236,
    236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
    236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 237, 237, 237, 237,
    237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
    237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 238, 238, 238,
    238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
    238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 239, 239,
    239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
    239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
    240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
    240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
    240, 240, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
    241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
    241, 241, 241, 241, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
    242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
    242, 242, 242, 242, 242, 242, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
    243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
    243, 243, 243, 243, 243, 243, 243, 243, 244, 244, 244, 244, 244, 244, 244, 244,
    244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
    244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 245, 245, 245, 245, 245, 245,
    245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245,
    245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 246, 246, 246,
    246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
    246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
    247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
    247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
    247, 247, 247, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
    248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
    248, 248, 248, 248, 248, 248, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
    249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
    249, 249, 249, 249, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 251, 251, 251,
    251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
    251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
    251, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
    252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};

static double pqToneMap(double nits)
{
    double x = nits / PQ_REFERENCE_WHITE_NITS;
    double peak = PQ_TONE_MAP_MAX_NITS / PQ_REFERENCE_WHITE_NITS;
    return x * (1.0 + x / (peak * peak)) / (1.0 + x);
}

static void bt2020ToBt709Linear(double r, double g, double b,
                                 double *r2, double *g2, double *b2)
{
    *r2 =  1.6605 * r + -0.5876 * g + -0.0728 * b;
    *g2 = -0.1246 * r +  1.1329 * g + -0.0083 * b;
    *b2 = -0.0182 * r + -0.1006 * g +  1.1187 * b;
}

static uint8_t srgbEncodeLutLookup(double linear)
{
    double c = linear < 0.0 ? 0.0 : (linear > 1.0 ? 1.0 : linear);
    int idx = (int)(c * (double)(SRGB_LUT_SIZE - 1) + 0.5); // round, no libm
    if (idx < 0) idx = 0;
    if (idx > SRGB_LUT_SIZE - 1) idx = SRGB_LUT_SIZE - 1;
    return kSrgbEncode8Lut[idx];
}

static void unpackA2R10G10B10_BT2020_PQ_to_rgb888(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint32_t r10 = (px >> 20) & 0x3FF;
    uint32_t g10 = (px >> 10) & 0x3FF;
    uint32_t b10 = (px >> 0)  & 0x3FF;

    double rNits = kPqEotfNitsLut[r10];
    double gNits = kPqEotfNitsLut[g10];
    double bNits = kPqEotfNitsLut[b10];

    double rTm = pqToneMap(rNits);
    double gTm = pqToneMap(gNits);
    double bTm = pqToneMap(bNits);

    double r709, g709, b709;
    bt2020ToBt709Linear(rTm, gTm, bTm, &r709, &g709, &b709);

    *r = srgbEncodeLutLookup(r709);
    *g = srgbEncodeLutLookup(g709);
    *b = srgbEncodeLutLookup(b709);
}

static PixelUnpackFn getUnpackFnForFormat(uint32_t format)
{
    switch (format) {
    case 0x88000000: case 0x88060000: // SDR A2R10G10B10 / A2R10G10B10_SRGB
        return unpackA2R10G10B10_to_rgb888;
    case 0x88740000: // A2R10G10B10_BT2020_PQ -- real PQ decode, not SDR truncation (was the bug)
        return unpackA2R10G10B10_BT2020_PQ_to_rgb888;
    case 0x80000000: // A8R8G8B8_SRGB -- confirmed live format this session, §21/§22
        return unpackA8R8G8B8_to_rgb888;
    default:
        return NULL;
    }
}

// ============================================================
// Zones: screen-edge points read as a small averaged neighborhood
// (not a single pixel) for stability. This is NOT a full-frame detile
// -- see the header note on cost.
//
// v2.0: per-edge counts, start corner, direction, index offset, and
// per-edge capture margin are all now config-driven (g_config, see
// the settings block above) instead of fixed #defines. The traversal-
// order logic (which corner/direction maps to which edge visit order
// and per-edge direction) was verified in Python for all 8 corner x
// direction combinations -- including an exact regression check
// against v1.0-v1.3's hardcoded bottom-left/clockwise output -- before
// being ported here. See handoff §43.
// ============================================================

#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

// (MAX_TOTAL_ZONES is defined earlier, near DDP_HEADER_SIZE -- needed
// before wled_send_rgb_zones's first use of it via the MAX_ZONES alias.)
// g_numZones (actual, config-driven count) is always <= MAX_TOTAL_ZONES.
static uint32_t g_zoneX[MAX_TOTAL_ZONES];
static uint32_t g_zoneY[MAX_TOTAL_ZONES];
static uint32_t g_numZones = 0; // set by buildZoneGeometry(), 0 until then

typedef enum { EDGE_LEFT, EDGE_TOP, EDGE_RIGHT, EDGE_BOTTOM } Edge;

// Generates `count` points along one screen edge, in either its
// canonical clockwise-from-bottom-left direction (reversed=false) or
// the opposite direction (reversed=true), honoring capture margins.
// Canonical directions: LEFT bottom->top, TOP left->right,
// RIGHT top->bottom, BOTTOM right->left -- matches v1.0-v1.3's
// original (now-default) bottom-left/clockwise behavior exactly.
static void generateEdgePoints(Edge edge, bool reversed, uint32_t count, uint32_t *outIdx)
{
    for (uint32_t i = 0; i < count && *outIdx < MAX_TOTAL_ZONES; i++) {
        uint32_t t = reversed ? (count - 1 - i) : i;
        uint32_t x, y;
        switch (edge) {
        case EDGE_LEFT:
            x = g_config.marginLeft;
            y = (SCREEN_HEIGHT - 1 - g_config.marginBottom) -
                (count > 1 ? (t * (SCREEN_HEIGHT - 1 - g_config.marginTop - g_config.marginBottom)) / (count - 1) : 0);
            break;
        case EDGE_TOP:
            x = g_config.marginLeft +
                (count > 1 ? (t * (SCREEN_WIDTH - 1 - g_config.marginLeft - g_config.marginRight)) / (count - 1) : 0);
            y = g_config.marginTop;
            break;
        case EDGE_RIGHT:
            x = SCREEN_WIDTH - 1 - g_config.marginRight;
            y = g_config.marginTop +
                (count > 1 ? (t * (SCREEN_HEIGHT - 1 - g_config.marginTop - g_config.marginBottom)) / (count - 1) : 0);
            break;
        default: // EDGE_BOTTOM
            x = (SCREEN_WIDTH - 1 - g_config.marginRight) -
                (count > 1 ? (t * (SCREEN_WIDTH - 1 - g_config.marginLeft - g_config.marginRight)) / (count - 1) : 0);
            y = SCREEN_HEIGHT - 1 - g_config.marginBottom;
            break;
        }
        g_zoneX[*outIdx] = x;
        g_zoneY[*outIdx] = y;
        (*outIdx)++;
    }
}

// Fills g_zoneX/g_zoneY (and g_numZones) at load time using only
// integer math (no libm linked in this build, see Makefile LIBS).
static void buildZoneGeometry(void)
{
    static const Edge kCwFromBL[4] = { EDGE_LEFT, EDGE_TOP, EDGE_RIGHT, EDGE_BOTTOM };
    uint32_t counts[4] = {
        [EDGE_LEFT] = g_config.ledCountLeft, [EDGE_TOP] = g_config.ledCountTop,
        [EDGE_RIGHT] = g_config.ledCountRight, [EDGE_BOTTOM] = g_config.ledCountBottom,
    };

    // Rotate the canonical CW-from-bottom-left order to start at the
    // edge that begins at the configured start corner.
    int startEdgeIdx;
    switch (g_config.startCorner) {
        case CORNER_BOTTOM_LEFT:  startEdgeIdx = 0; break; // LEFT
        case CORNER_TOP_LEFT:     startEdgeIdx = 1; break; // TOP
        case CORNER_TOP_RIGHT:    startEdgeIdx = 2; break; // RIGHT
        default: /* BOTTOM_RIGHT */ startEdgeIdx = 3; break; // BOTTOM
    }

    Edge order[4];
    bool edgeReversed[4];
    if (g_config.direction == DIR_CLOCKWISE) {
        for (int i = 0; i < 4; i++) { order[i] = kCwFromBL[(startEdgeIdx + i) % 4]; edgeReversed[i] = false; }
    } else {
        // CCW = reverse of the CW-for-this-corner visit order, with
        // every edge's own direction also reversed. Verified in
        // Python against all 4 corners before porting (handoff §43).
        for (int i = 0; i < 4; i++) { order[i] = kCwFromBL[(startEdgeIdx + (3 - i)) % 4]; edgeReversed[i] = true; }
    }

    uint32_t idx = 0;
    for (int i = 0; i < 4; i++) {
        generateEdgePoints(order[i], edgeReversed[i], counts[order[i]], &idx);
    }
    g_numZones = idx;

    // led_offset: rotate which physical LED index 0 lands on, without
    // changing the shape/order of the sequence itself. Done as a
    // second pass (rotate the filled arrays) rather than folded into
    // the generation loop above, so this stays a single, independently
    // understandable step.
    int32_t offset = g_config.ledOffset % (int32_t)g_numZones;
    if (offset < 0) offset += (int32_t)g_numZones; // proper modulo for negative offsets
    if (offset != 0) {
        uint32_t tmpX[MAX_TOTAL_ZONES], tmpY[MAX_TOTAL_ZONES];
        memcpy(tmpX, g_zoneX, g_numZones * sizeof(uint32_t));
        memcpy(tmpY, g_zoneY, g_numZones * sizeof(uint32_t));
        for (uint32_t i = 0; i < g_numZones; i++) {
            uint32_t src = (i + (uint32_t)offset) % g_numZones;
            g_zoneX[i] = tmpX[src];
            g_zoneY[i] = tmpY[src];
        }
    }
}

static void sampleZoneAverage(const TileParams *p, uint64_t bufferAddr, PixelUnpackFn unpack,
                               uint32_t cx, uint32_t cy, uint8_t *outR, uint8_t *outG, uint8_t *outB)
{
    // v2.0: sample radius is g_config.scanDepth (was the fixed
    // ZONE_SAMPLE_RADIUS #define). int32_t cast since scanDepth is
    // unsigned but used in a signed loop range below.
    int32_t radius = (int32_t)g_config.scanDepth;
    // v2.2: float accumulators, not uint32_t -- the per-sample gamma
    // LUTs (g_gammaLutR/G/B) now return unclamped/unrounded float, so
    // rounding each sample to a byte before summing would reintroduce
    // exactly the precision loss this change is meant to avoid. This
    // only touches (2*radius+1)^2 samples per zone -- basic float
    // add/multiply, no libm -- so it's the same cost class as the
    // uint32_t version it replaces.
    float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
    uint32_t n = 0;
    for (int32_t dy = -radius; dy <= radius; dy++) {
        for (int32_t dx = -radius; dx <= radius; dx++) {
            int32_t sx = (int32_t)cx + dx;
            int32_t sy = (int32_t)cy + dy;
            if (sx < 0 || sy < 0 || sx >= SCREEN_WIDTH || sy >= SCREEN_HEIGHT) continue;
            uint64_t off = getTiledElementByteOffset(p, (uint32_t)sx, (uint32_t)sy);
            // v1.1: bounds-check before touching real memory. A correct
            // offset formula should never produce something out of
            // range for a valid (sx,sy), but this project has already
            // paid for bad-memory-access crashes once (handoff §3) --
            // cheap insurance against a future edge case or a param
            // typo, not a sign anything is currently wrong.
            if (off + 4 > BASE_PADDED_BUFFER_BYTES) continue;
            uint32_t px;
            memcpy(&px, (const void*)(bufferAddr + off), 4);
            uint8_t r, g, b;
            unpack(px, &r, &g, &b);
            // v2.2: per-channel gamma (gamma_r/g/b) applied to THIS
            // sample now, before it's folded into the zone sum --
            // see the big comment above applyColorProcessing for why
            // this one specific step moved here instead of staying in
            // the post-average pipeline like everything else.
            sumR += g_gammaLutR[r];
            sumG += g_gammaLutG[g];
            sumB += g_gammaLutB[b];
            n++;
        }
    }
    if (n == 0) n = 1;
    float avgR = sumR / (float)n, avgG = sumG / (float)n, avgB = sumB / (float)n;
    // This IS the one deliberate round-to-byte boundary in the whole
    // v2.2 pipeline before the final output: the legacy post-average
    // stages (applyColorProcessing) take uint8_t in/out to stay a
    // drop-in match for the pre-v2.2 function signature and call
    // site. Everything from here through applyColorProcessing's
    // internals is unclamped int32_t, with only ONE more round+clamp
    // at the very end -- see that function.
    #define CLAMPF(v) ((v) < 0.0f ? 0.0f : ((v) > 255.0f ? 255.0f : (v)))
    *outR = (uint8_t)(CLAMPF(avgR) + 0.5f);
    *outG = (uint8_t)(CLAMPF(avgG) + 0.5f);
    *outB = (uint8_t)(CLAMPF(avgB) + 0.5f);
    #undef CLAMPF
}

// ============================================================
// v2.2 color processing (see handoff §49): gamma -> brightness ->
// contrast -> saturation -> levels -> color order. Most of this is
// still applied once per zone AFTER averaging, for the same
// cheapness argument as before -- EXCEPT the new per-channel
// gamma_r/g/b, which is applied per-sample BEFORE averaging (see
// sampleZoneAverage), because gamma is non-linear and this project's
// own comment claiming "averaging first then correcting matches the
// Android inspiration project" was simply wrong -- the real Android
// source (ColorProcessor.kt, called from e.g. ScreenEncoder before
// any zone/border cropping) corrects every pixel first, then
// averages. That distinction is invisible for linear ops (brightness,
// levels) but real for gamma, so only gamma moved.
//
// No libm anywhere in this path: the legacy global gamma is a LUT
// lookup (kGammaLuts, generated offline), the new per-channel gammas
// are LUTs too (g_gammaLutR/G/B, built from a libm-free fast-pow
// approximation at config load -- see ambient_rebuild_perchannel_
// gamma_luts above), saturation/contrast are pure integer math
// (verified in Python before porting), brightness is a plain integer
// scale.
// ============================================================

// ------------------------------------------------------------
// v2.2: brightness/contrast/saturation below all take and return
// UNCLAMPED int32_t, not uint8_t, and NONE of them clamp to 0-255
// internally. This matters, and isn't just a style choice: Android's
// ColorProcessor.kt keeps every intermediate value as an unclamped
// Float and only clamps+rounds once, at the very end of the whole
// pipeline. Clamping to a byte between every stage (the v2.0/v2.1
// behavior) throws away sign/overflow information that a LATER,
// luma-relative stage (saturation) actually needs -- e.g. a channel
// that contrast pushed to -37 is meaningfully different input to
// saturation's luma math than a channel clamped to 0 first. Verified
// in Python before porting: on ordinary test inputs this produced
// final answers up to ~13/255 away from Android's real result, not
// mere rounding noise (handoff §49). The only clamp+round in this
// whole chain is in applyColorProcessing, right before the wire-order
// write.
// ------------------------------------------------------------

static int32_t applyBrightness(int32_t c, uint32_t brightness)
{
    return (c * (int32_t)brightness) / 255;
}

// v2.2: per-channel brightness multiplier, ported from Android's
// (brightness/100) * (brightnessChannel/100) combined factor. Kept as
// its own integer scale (0-500, 100=neutral) separate from the 0-255
// global `applyBrightness` above rather than trying to unify the two
// scales -- see the AmbientConfig struct comment for why.
static int32_t applyChannelBrightnessPct(int32_t c, uint32_t pct)
{
    if (pct == 100) return c; // no-op fast path
    return (c * (int32_t)pct) / 100;
}

// v2.2: contrast, ported from Android's `128 + (c-128)*cf` with
// cf = 1 + contrast/100. Uses this file's usual "0 = unchanged"
// convention (same as saturation), NOT Android's raw "100 = unchanged"
// percentage -- see the AmbientConfig struct comment.
static void applyContrast(int32_t r, int32_t g, int32_t b, int32_t contrast,
                           int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (contrast == 0) { *outR = r; *outG = g; *outB = b; return; }
    *outR = 128 + ((r - 128) * (100 + contrast)) / 100;
    *outG = 128 + ((g - 128) * (100 + contrast)) / 100;
    *outB = 128 + ((b - 128) * (100 + contrast)) / 100;
}

// v2.2: input/output are unclamped int32_t now (see the big comment
// above applyBrightness) -- a channel that arrived negative (from
// contrast) or >255 (from a >100% per-channel brightness) still needs
// to pull the luma calculation in that same, un-clamped direction, or
// this stage disagrees with what Android's real float math does.
static void applySaturation(int32_t r, int32_t g, int32_t b, int32_t sat,
                             int32_t *outR, int32_t *outG, int32_t *outB)
{
    if (sat == 0) { *outR = r; *outG = g; *outB = b; return; }
    // Integer luma using the real BT.601 weights (Android uses
    // 0.299/0.587/0.114 as floats; /1000 here instead of the old
    // >>8-and-round-to-256ths approximation, now that this whole path
    // carries enough precision to make the more exact weights worth
    // using -- see handoff §49).
    int32_t luma = (r * 299 + g * 587 + b * 114) / 1000;
    *outR = luma + ((r - luma) * (100 + sat)) / 100;
    *outG = luma + ((g - luma) * (100 + sat)) / 100;
    *outB = luma + ((b - luma) * (100 + sat)) / 100;
}

// Writes the 3 output bytes in the configured wire order (most
// WS2812B/NeoPixel strips are GRB, not RGB -- see handoff §43/the
// Android inspiration project's own color_order setting).
static void writeColorOrdered(uint8_t r, uint8_t g, uint8_t b, ColorOrder order, uint8_t *out3)
{
    switch (order) {
        case ORDER_RGB: out3[0]=r; out3[1]=g; out3[2]=b; break;
        case ORDER_RBG: out3[0]=r; out3[1]=b; out3[2]=g; break;
        case ORDER_GRB: out3[0]=g; out3[1]=r; out3[2]=b; break;
        case ORDER_GBR: out3[0]=g; out3[1]=b; out3[2]=r; break;
        case ORDER_BRG: out3[0]=b; out3[1]=r; out3[2]=g; break;
        default: /* ORDER_BGR */ out3[0]=b; out3[1]=g; out3[2]=r; break;
    }
}

// v2.1/v2.2: levels adjustment (black_level/white_level) -- a
// "stretch" of the 0-255 range, verified in Python before porting
// (handoff §45). Applied last in the color pipeline, matching the
// real Android ColorProcessor order (gamma -> brightness -> contrast
// -> saturation -> levels).
//
// This is now ALSO the single final clamp-to-byte for the entire
// v2.2 pipeline (see the big comment above applyBrightness) -- input
// here may be negative or >255 coming out of contrast/saturation, so
// even the "no-op" (blackLevel=0, whiteLevel=100) fast path below
// must still clamp, not just pass through, or an out-of-range
// unclamped value would wrap/truncate incorrectly when cast to
// uint8_t by the caller.
static void applyLevels(int32_t r, int32_t g, int32_t b, uint32_t blackLevel, uint32_t whiteLevel,
                         uint8_t *outR, uint8_t *outG, uint8_t *outB)
{
    #define CLAMP255(c) (uint8_t)((c) < 0 ? 0 : ((c) > 255 ? 255 : (c)))
    if (blackLevel == 0 && whiteLevel == 100) {
        *outR = CLAMP255(r); *outG = CLAMP255(g); *outB = CLAMP255(b);
        return;
    }
    int32_t blackThresh = (int32_t)(blackLevel * 255 / 100);
    int32_t whiteThresh = (int32_t)(whiteLevel * 255 / 100);
    int32_t range = whiteThresh - blackThresh;
    if (range <= 0) { *outR = 0; *outG = 0; *outB = 0; return; } // degenerate config -- fail to black, not undefined
    #define STRETCH(c) CLAMP255(((c) < blackThresh) ? 0 : ((c) >= whiteThresh) ? 255 : \
                                  (((c) - blackThresh) * 255) / range)
    *outR = STRETCH(r);
    *outG = STRETCH(g);
    *outB = STRETCH(b);
    #undef STRETCH
    #undef CLAMP255
}

// Full per-zone pipeline, v2.2 order -- now matches the Android source
// exactly: gamma -> brightness -> contrast -> saturation -> levels
// (black/white point) -> wire-order bytes. (v2.0/v2.1 had saturation
// BEFORE brightness, contradicting this file's own comment about
// matching Android -- see handoff §49. Harmless while brightness was
// only ever a single uniform scalar, since a uniform scale commutes
// with saturation's luma-relative math either way, but it stops being
// harmless the moment per-channel brightness enters the picture below,
// so it's fixed here rather than left as a latent trap.)
//
// Note the input (r,g,b) here has ALREADY had the new per-channel
// gamma_r/gamma_g/gamma_b applied, per-sample, before zone-averaging
// (see sampleZoneAverage) -- that part deliberately happens BEFORE
// this function, not in it, because gamma is non-linear and Android
// applies it before any downsampling too. The legacy fixed-LUT
// `gamma=` global below is a SEPARATE, ADDITIONAL curve applied here,
// zone-wide, after averaging -- kept for backward compatibility with
// existing configs. Both default to a no-op, so an unmodified ini
// still behaves exactly like before.
//
// dark_threshold is deliberately NOT applied here -- it needs
// per-zone hysteresis STATE across calls, which lives with the
// smoothing state in the thread loop instead (handoff §45).
static void applyColorProcessing(uint8_t r8, uint8_t g8, uint8_t b8, uint8_t *out3)
{
    // --- gamma (legacy global fixed-LUT stage; per-channel gamma_r/g/b
    // already happened per-sample before this call). This is the one
    // remaining byte-rounding step before the pipeline goes fully
    // unclamped int32_t below -- see the struct comment on why the
    // legacy uint8_t LUT itself wasn't also upgraded to float. ---
    uint32_t gi = g_config.gammaLutIndex < NUM_GAMMA_LUTS ? g_config.gammaLutIndex : 0;
    int32_t r = kGammaLuts[gi][r8];
    int32_t g = kGammaLuts[gi][g8];
    int32_t b = kGammaLuts[gi][b8];

    // --- brightness: global (0-255 scale) then per-channel (Android's
    // 0-500pct scale) on top of it. Unclamped from here on. ---
    r = applyBrightness(r, g_config.brightness);
    g = applyBrightness(g, g_config.brightness);
    b = applyBrightness(b, g_config.brightness);
    r = applyChannelBrightnessPct(r, g_config.brightnessR);
    g = applyChannelBrightnessPct(g, g_config.brightnessG);
    b = applyChannelBrightnessPct(b, g_config.brightnessB);

    // --- contrast ---
    int32_t cr, cg, cb;
    applyContrast(r, g, b, g_config.contrast, &cr, &cg, &cb);

    // --- saturation ---
    int32_t sr, sg, sb;
    applySaturation(cr, cg, cb, g_config.saturation, &sr, &sg, &sb);

    // --- levels (black/white point) -- the ONE clamp+round back to a
    // byte for this whole unclamped chain, matching Android's own
    // single final clamp+round. ---
    uint8_t lr, lg, lb;
    applyLevels(sr, sg, sb, g_config.blackLevel, g_config.whiteLevel, &lr, &lg, &lb);

    writeColorOrdered(lr, lg, lb, g_config.colorOrder, out3);
}

// --- Plugin metadata ---
attr_public const char *g_pluginName = "ps4_ambient_light";
attr_public const char *g_pluginDesc = "Live per-frame ambient light: detiles the real scanout buffer and streams zone colors to WLED";
attr_public const char *g_pluginAuth = "(null)";
attr_public uint32_t g_pluginVersion = 0x00000209; // v2.2.2: DIAGNOSTIC ONLY, no color-pipeline change -- v2.2.1's format packet confirmed this game's HDR mode correctly registers and decodes via A2R10G10B10_BT2020_PQ (recognized=1), and this session hand-verified the PQ decode math against both neutral and slightly-imbalanced near-black input with no red bias in either case. But the user then made two sharp observations that neither of those findings explains: (1) SDR shows nothing at all in the same spot, which is odd if it were a persistent real UI element, and (2) MUCH of the physical strip goes red, not just the one LED whose zone would cover a small icon -- ruling out "it's just real content in one corner of the screen". Since a systemic effect across many zones simultaneously is a different failure mode than anything checked so far, this adds a ~1x/sec throttled packet (over the same debug_send_raw/DEBUG_IP path) carrying the RAW pre-decode 32-bit pixel word plus the already-decoded 8-bit RGB for 3 zones spread across the strip (index 0, middle, last) -- so the actual captured memory content can be checked directly instead of continuing to reason about it secondhand. NOT diagnosed further by this session -- next step is reading this packet's values during the red phase.

int32_t (*sceVideoOutRegisterBuffersPtr)(int32_t handle, int32_t startIndex,
                                          void *const *addresses, int32_t bufferNum,
                                          const OrbisVideoOutBufferAttribute *attribute);
int32_t (*sceGnmSubmitAndFlipCommandBuffersPtr)(uint32_t count, void *dcbGpuAddrs[],
                                                 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                 uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                 uint32_t displayBufferIndex, uint32_t flipMode,
                                                 int64_t flipArg);

HOOK_INIT(sceVideoOutRegisterBuffersPtr);
HOOK_INIT(sceGnmSubmitAndFlipCommandBuffersPtr);

#define MAX_TRACKED_BUFFERS 16
static volatile uint64_t g_bufferAddrs[MAX_TRACKED_BUFFERS] = {0};
static volatile int32_t  g_bufferCount = 0;
static volatile uint32_t g_activeFormat = 0;       // live format, NOT hardcoded -- the actual fix from this session
static volatile int      g_haveValidFormat = 0;    // 0 until a registration event gives us a known format

int32_t sceVideoOutRegisterBuffersPtr_hook(int32_t handle, int32_t startIndex,
                                            void *const *addresses, int32_t bufferNum,
                                            const OrbisVideoOutBufferAttribute *attribute)
{
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
            g_activeFormat = fmt;
            g_haveValidFormat = (getUnpackFnForFormat(fmt) != NULL);
            // v2.2.1: diagnostic-only telemetry, added to answer a real
            // live question (does this specific game's HDR mode register
            // a format this plugin actually recognizes?) rather than
            // guess. Reuses the existing debug_send_raw/DEBUG_IP path
            // (same one send_timing_packet uses), a NEW 8-byte payload so
            // it dispatches distinctly by length in any listener that
            // already keys off len(data) (this project's own convention
            // -- see send_timing_packet's comment above). Only fires when
            // the format actually CHANGES, not on every re-registration
            // (this hook already fires repeatedly during normal play per
            // the comment this replaces), so it won't spam a listener
            // during a real session -- only right when HDR toggles on/off
            // or a title switches formats.
            //   [0:4] format     -- raw attribute->format, uint32 LE
            //   [4:8] wasRecognized -- 0/1, uint32 LE (matches one of the
            //                          getUnpackFnForFormat cases or not)
            if (fmt != prevFormat || g_haveValidFormat != wasValid) {
                uint8_t fmtPacket[8];
                memcpy(fmtPacket + 0, &fmt, 4);
                uint32_t recognized = g_haveValidFormat ? 1u : 0u;
                memcpy(fmtPacket + 4, &recognized, 4);
                debug_send_raw(fmtPacket, sizeof(fmtPacket));
            }
        }
    }
    return HOOK_CONTINUE(sceVideoOutRegisterBuffersPtr,
                          int32_t(*)(int32_t, int32_t, void *const *, int32_t, const OrbisVideoOutBufferAttribute *),
                          handle, startIndex, addresses, bufferNum, attribute);
}

// v1.1: the flip hook itself only records which buffer is live now --
// no sampling, no network I/O, no loop. This is the actual fix for
// handoff §26 finding 1. g_currentDisplayBufferIndex is read by the
// worker thread below at its own pace.
static volatile uint32_t g_currentDisplayBufferIndex = 0xFFFFFFFFu; // sentinel: no flip seen yet

int32_t sceGnmSubmitAndFlipCommandBuffersPtr_hook(uint32_t count, void *dcbGpuAddrs[],
                                                   uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[],
                                                   uint32_t *ccbSizesInBytes, uint32_t videoOutHandle,
                                                   uint32_t displayBufferIndex, uint32_t flipMode,
                                                   int64_t flipArg)
{
    g_currentDisplayBufferIndex = displayBufferIndex; // single volatile write, near-zero cost

    return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffersPtr,
                          int32_t(*)(uint32_t, void **, uint32_t *, void **, uint32_t *, uint32_t, uint32_t, uint32_t, int64_t),
                          count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes,
                          videoOutHandle, displayBufferIndex, flipMode, flipArg);
}

// v1.1: all the actual work -- resolving the live buffer, sampling 229
// zones, sending the UDP packet -- now happens here, on its own
// thread, at its own ~30Hz pace, never inside the render/submission
// path. Same scePthreadCreate pattern as detile_verify_probe's
// pad_poll_thread.
//
// Tradeoff this introduces (worth understanding, not a hidden cost):
// this thread can read g_currentDisplayBufferIndex / g_bufferAddrs /
// g_activeFormat at any point in a flip's lifecycle, including
// mid-update on another thread. Every one of those is a single
// word-sized volatile read/write (same pattern already used
// throughout this project without locks), so there's no torn-read risk
// on real hardware, but the *value* read could be up to one flip old.
// At ~30Hz sampling against ~60Hz flips, worst case this thread is
// working from a frame that's already been superseded by a newer one
// by the time it finishes reading -- i.e. the light output can lag
// real screen content by up to roughly one extra frame interval versus
// the old in-hook version. That's the price for never risking a stall
// on the thread the game actually needs to stay smooth, and is a far
// better tradeoff for a hobby ambient-light feature than occasional
// frame hitches during real play.
// v2.0: was a fixed #define (33*1000, ~30Hz). Now computed once at
// thread start from g_config.updateFrequencyHz (validated 1-240 at
// load time, see ambient_load_config). Kept as a plain local rather
// than a macro since it's no longer a compile-time constant.

// handoff §30 step 2: window size for the timing telemetry below.
// 30 iterations at the ~30Hz default sample rate is roughly one
// telemetry packet per second of real play -- frequent enough to see
// trends during a session, not so frequent it competes with the actual
// zone-color UDP traffic. TIMING_ENABLED lets this be compiled out
// entirely for a "final" build once the number is in hand, same spirit
// as PROBE_INCLUDE_NEO in detile_verify_probe.
#ifndef TIMING_ENABLED
#define TIMING_ENABLED 1
#endif
#define TIMING_WINDOW_SIZE 30

// v2.0 smoothing state: previous frame's post-processed color per
// zone, so each new sample can be blended toward instead of snapped
// to. This is a simple discrete integer approximation of an RC-style
// exponential smoothing, NOT a calibrated real time-constant -- alpha
// is derived from settlingTimeMs vs the actual sample interval and
// verified in Python to converge monotonically without oscillation
// across a range of settling times (handoff §43), but "settling_time_ms
// = 200" should be read as "roughly", not as a precise guarantee.
static uint8_t g_smoothedRgb[MAX_TOTAL_ZONES][3];
static bool g_smoothedRgbValid = false; // false until the first real frame, so startup doesn't fade in from black
static uint32_t s_rawDiagFrameCounter = 0; // v2.2.2: throttles the raw-pixel diagnostic packet to ~1x/sec

// v2.1 dark_threshold state: per-zone hysteresis flag (see
// applyDarkThreshold below) -- separate from g_smoothedRgb since it's
// a decision (am I "dark" right now), not a color value.
static bool g_zoneIsDark[MAX_TOTAL_ZONES];

// Forces (r,g,b) to black if the zone should currently be considered
// "dark", with hysteresis to avoid flicker on scenes hovering right at
// the threshold: must drop BELOW darkThreshold to go dark, but must
// rise darkThreshold+10 to come back -- verified in Python against a
// deliberately noisy sequence straddling the threshold before porting
// (handoff §45). Applied to the pre-smoothing target, so smoothing (if
// enabled) naturally fades in/out of black instead of snapping.
#define DARK_HYSTERESIS 10
static void applyDarkThreshold(uint32_t zoneIdx, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (g_config.darkThreshold == 0) return; // disabled
    uint8_t luma = *r > *g ? (*r > *b ? *r : *b) : (*g > *b ? *g : *b);
    if (g_zoneIsDark[zoneIdx]) {
        if (luma >= g_config.darkThreshold + DARK_HYSTERESIS) g_zoneIsDark[zoneIdx] = false;
    } else if (luma < g_config.darkThreshold) {
        g_zoneIsDark[zoneIdx] = true;
    }
    if (g_zoneIsDark[zoneIdx]) { *r = 0; *g = 0; *b = 0; }
}

// v2.1 live config reload: re-reads the ini file if its mtime changed,
// checked at most once every configReloadCheckSeconds (0 = never,
// matches v2.0's load-once behavior). Safe without any locking because
// g_config/g_zoneX/g_zoneY/g_numZones are read AND written exclusively
// by this one thread -- plugin_load's initial ambient_load_config()/
// buildZoneGeometry() call happens before this thread is even spawned,
// and the flip hook only ever touches g_currentDisplayBufferIndex, not
// any of this state (handoff §45).
// v2.1.1 BUGFIX (real hardware confirmed live reload never fired --
// see handoff §46): the original version used g_configLastMtime==0 as
// its own "not yet initialized" sentinel. That's a real, fatal design
// flaw independent of any PS4-specific quirk: 0 is also a value
// st_mtime could legitimately have (unpopulated/unsupported on this
// filesystem, or just a coincidence). If st_mtime is ever actually 0,
// this check would conclude "still establishing baseline" FOREVER,
// on every single call, and reload would silently never fire again --
// exactly the symptom reported. Fixed with a separate boolean flag so
// the sentinel state can never collide with a real mtime value.
//
// Also now tracks file SIZE alongside mtime and reloads if EITHER
// changed -- not because mtime is confirmed broken on this platform
// (not verified from here), but because this is cheap insurance
// against exactly that class of platform-level uncertainty, and this
// project has already spent one whole session finding out an
// assumption about low-level platform behavior was wrong (§17-22).
// v2.1.4: real hardware (see handoff §47/§48) showed st_size is just as
// broken as st_mtime on this filesystem -- stuck reporting a fixed
// wrong value (8) for the entire life of the process, never once
// reflecting the real file's actual size even while it was being
// actively edited. Both fields come off the same stat() call, so
// "st_mtime is broken, but surely st_size is fine" was never a safe
// assumption -- it just hadn't been checked directly yet. What IS
// proven reliable on this platform is a raw sceKernelOpen +
// sceKernelLseek(SEEK_END) + sceKernelClose read: that's exactly how
// the initial plugin-load config read (and the v2.1.3 content preview)
// already get real, live data, and both are confirmed correct against
// this exact path by the fact edits DO take effect after a game
// restart. So the reload check now sources its size signal from that
// same syscall path instead of patching around stat() a third time.
// Existence is still checked via stat() in ambient_file_exists() --
// that's a boolean return code, not a numeric field, and nothing here
// suggests that part is unreliable.
// v2.1.5: content hash, added because size alone can't see a
// same-length edit (confirmed on real hardware -- an RGB->RBG swap
// left cur_size/last_size both correctly reading a stable, real 2809
// but never differing, so event stayed 3/"unchanged" forever). FNV-1a
// is used purely as a cheap, dependency-free change signal, not for
// any security property -- no libm, no external library, consistent
// with the existing no-libm constraint (§ PQ/sRGB LUT work).
#define AMBIENT_FNV1A_OFFSET_BASIS 0x811c9dc5u
#define AMBIENT_FNV1A_PRIME        0x01000193u
static uint32_t ambient_fnv1a32(const uint8_t *data, size_t len)
{
    uint32_t hash = AMBIENT_FNV1A_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= AMBIENT_FNV1A_PRIME;
    }
    return hash;
}

// Sanity cap on the hash read, independent of the earlier no-libm
// constraint -- this is about not handing malloc() a garbage size if
// AMBIENT_CONFIG_PATH ever resolves to something unexpected. The real
// file is a few KB at most (even a heavily-commented ini); 1 MiB is
// generous headroom, not a real expected size.
#define AMBIENT_CONFIG_MAX_HASH_READ (1024 * 1024)

// Replaces v2.1.4's ambient_get_real_config_size: still gets size the
// same proven way (sceKernelOpen + sceKernelLseek(SEEK_END)), but now
// also seeks back to the start and reads the whole file through the
// same fd to compute a content hash in the same call, rather than
// opening the file twice per check. Both outputs share one success/
// failure result since they come from the same read; on any failure
// *outSize carries the negative orbis error code (or a sentinel below)
// same as v2.1.4's function did, and *outHash is left untouched.
static bool ambient_get_real_config_size_and_hash(int64_t *outSize, uint32_t *outHash)
{
    int32_t fd = sceKernelOpen(AMBIENT_CONFIG_PATH, 0 /* O_RDONLY */, 0777);
    if (fd < 0) {
        *outSize = fd; // negative orbis error code, reused as the packet's "errno" field below
        return false;
    }
    int64_t sz = sceKernelLseek(fd, 0, SEEK_END);
    if (sz < 0) {
        sceKernelClose(fd);
        *outSize = sz;
        return false;
    }
    if (sz > AMBIENT_CONFIG_MAX_HASH_READ) {
        sceKernelClose(fd);
        *outSize = -1; // sentinel, not a real orbis error code -- "file unexpectedly huge", not an open/lseek failure
        return false;
    }

    int64_t seekBack = sceKernelLseek(fd, 0, SEEK_SET);
    if (seekBack < 0) {
        sceKernelClose(fd);
        *outSize = seekBack;
        return false;
    }

    // sz fits in AMBIENT_CONFIG_MAX_HASH_READ (checked above), so this
    // is a small, bounded allocation, not proportional to whatever
    // AMBIENT_CONFIG_PATH happens to resolve to.
    uint8_t *buf = (sz > 0) ? (uint8_t *)malloc((size_t)sz) : NULL;
    if (sz > 0 && buf == NULL) {
        sceKernelClose(fd);
        *outSize = -1; // sentinel -- malloc failure, not an orbis error code
        return false;
    }

    ssize_t nread = (sz > 0) ? sceKernelRead(fd, buf, (size_t)sz) : 0;
    sceKernelClose(fd);
    if (nread < 0) {
        free(buf);
        *outSize = nread;
        return false;
    }

    *outSize = sz;
    *outHash = ambient_fnv1a32(buf, (size_t)nread);
    free(buf);
    return true;
}

// mtime is kept in the struct/packet purely for backward-compatible
// layout with the v2.1.2/v2.1.3 captures already on file -- it is
// HARDCODED to 0 as of v2.1.4 and no longer read from stat() or used
// for change-detection at all (confirmed non-functional on this
// filesystem, see handoff §46/§47). Size, sourced from
// ambient_get_real_config_size_and_hash above, is one of two change
// signals as of v2.1.5 -- see g_configLastHash below for the other,
// added because size alone missed a real same-length edit on hardware.
static bool g_haveConfigBaseline = false;
static off_t g_configLastSize = 0;
// v2.1.5: content hash (FNV-1a/32) of the whole file, alongside size.
// Needed because a same-length edit (confirmed on hardware: RGB->RBG)
// changes the file's content without changing its byte count, which
// size alone has no way to detect. Reload now fires if EITHER size or
// hash differs from last check.
static uint32_t g_configLastHash = 0;
// Increments on every call, independent of what the call finds -- see
// send_config_reload_debug_packet's check_count field above for why.
static uint32_t g_configCheckCount = 0;

static void ambient_check_config_reload(void)
{
    g_configCheckCount++;

    if (g_config.configReloadCheckSeconds == 0) {
        uint8_t preview[CONFIG_DEBUG_PREVIEW_LEN];
        ambient_read_content_preview(preview, sizeof(preview));
        send_config_reload_debug_packet(0, 0, 0, 0,
                                         0, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         0, g_configLastHash);
        return;
    }

    int64_t realSize;
    uint32_t realHash;
    if (!ambient_get_real_config_size_and_hash(&realSize, &realHash)) {
        uint8_t preview[CONFIG_DEBUG_PREVIEW_LEN];
        ambient_read_content_preview(preview, sizeof(preview));
        send_config_reload_debug_packet(1, (uint32_t)(-realSize), 0, 0,
                                         0, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         0, g_configLastHash);
        return; // file gone/unreadable/unexpectedly huge -- keep running on current config
    }

    uint8_t preview[CONFIG_DEBUG_PREVIEW_LEN];
    ambient_read_content_preview(preview, sizeof(preview));

    if (!g_haveConfigBaseline) {
        send_config_reload_debug_packet(2, 0, 0, 0,
                                         (uint64_t)realSize, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         realHash, g_configLastHash);
        g_configLastSize = (off_t)realSize;
        g_configLastHash = realHash;
        g_haveConfigBaseline = true;
        return; // first check just establishes a baseline, nothing to compare against yet
    }
    if ((off_t)realSize == g_configLastSize && realHash == g_configLastHash) {
        send_config_reload_debug_packet(3, 0, 0, 0,
                                         (uint64_t)realSize, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                         realHash, g_configLastHash);
        return; // unchanged -- same size AND same content hash
    }

    send_config_reload_debug_packet(4, 0, 0, 0,
                                     (uint64_t)realSize, (uint64_t)g_configLastSize, g_configCheckCount, preview,
                                     realHash, g_configLastHash);

    g_configLastSize = (off_t)realSize;
    g_configLastHash = realHash;
    ambient_load_config();  // re-reads the file; unset/removed keys keep their CURRENT g_config value, not the compiled default (see note below)
    buildZoneGeometry();    // layout settings may have changed -- rebuild g_zoneX/g_zoneY/g_numZones
    g_smoothedRgbValid = false; // avoid smoothing a hard cut between old and new zone counts/positions
}

void *ambient_sample_thread(void *args)
{
    uint32_t sampleIntervalUs = 1000000u / g_config.updateFrequencyHz;
    uint32_t smoothingAlpha = 256u; // recomputed every iteration below (cheap), so live reload picks up changes
    // sceKernelGetProcessTimeCounter[Frequency] -- same TSC-based timer
    // already proven in this repo (frame_logger.prx). Needed
    // unconditionally now (not just under TIMING_ENABLED) since the
    // config-reload check below uses real elapsed time too.
    uint64_t tscFreq = sceKernelGetProcessTimeCounterFrequency();
    uint64_t lastReloadCheckTicks = sceKernelGetProcessTimeCounter();

#if TIMING_ENABLED
    uint64_t budgetTicks = (tscFreq * (uint64_t)sampleIntervalUs) / 1000000ULL;
    uint64_t minTicks = UINT64_MAX, maxTicks = 0, sumTicks = 0;
    uint32_t windowSamples = 0, overBudgetCount = 0, windowId = 0;
    // Pure observation via the confirmed sceKernelGetCurrentCpu() -- no
    // affinity/priority call involved, just recording where the scheduler
    // is actually placing this thread by default.
    int32_t minCpuSeen = INT32_MAX, maxCpuSeen = -1, lastCpu = -1;
    uint32_t migrationCount = 0;
#endif

    for (;;) {
        // Needed unconditionally now, not just under TIMING_ENABLED --
        // the config-reload check below also uses real elapsed time.
        uint64_t t0 = sceKernelGetProcessTimeCounter();

        if (g_config.configReloadCheckSeconds > 0) {
            uint64_t elapsedTicks = t0 - lastReloadCheckTicks;
            if (elapsedTicks >= tscFreq * (uint64_t)g_config.configReloadCheckSeconds) {
                ambient_check_config_reload();
                lastReloadCheckTicks = t0;
                // Recompute everything derived from config that this
                // loop otherwise only computes once at thread start --
                // this IS the "live" part of live reload.
                sampleIntervalUs = 1000000u / g_config.updateFrequencyHz;
#if TIMING_ENABLED
                budgetTicks = (tscFreq * (uint64_t)sampleIntervalUs) / 1000000ULL;
#endif
            }
        }
        uint32_t smoothingIntervalMs = sampleIntervalUs / 1000u;
        smoothingAlpha = (smoothingIntervalMs * 256u) / (g_config.settlingTimeMs + 1u);
        if (smoothingAlpha > 256u) smoothingAlpha = 256u;

        uint32_t displayBufferIndex = g_currentDisplayBufferIndex;
        uint64_t liveBufferAddr = (displayBufferIndex != 0xFFFFFFFFu &&
                                    displayBufferIndex < (uint32_t)MAX_TRACKED_BUFFERS)
                                       ? g_bufferAddrs[displayBufferIndex] : 0;

        if (liveBufferAddr != 0 && g_haveValidFormat) {
            PixelUnpackFn unpack = getUnpackFnForFormat(g_activeFormat);
            if (unpack != NULL) { // re-check -- format could have gone unknown since the last read
                uint8_t rgbTriplets[MAX_TOTAL_ZONES * 3];
                for (uint32_t i = 0; i < g_numZones; i++) {
                    uint8_t r, g, b;
                    sampleZoneAverage(&kParamsBase, liveBufferAddr, unpack,
                                       g_zoneX[i], g_zoneY[i], &r, &g, &b);

                    uint8_t processed[3];
                    applyColorProcessing(r, g, b, processed);
                    applyDarkThreshold(i, &processed[0], &processed[1], &processed[2]);

                    if (g_config.smoothingEnabled) {
                        if (!g_smoothedRgbValid) {
                            g_smoothedRgb[i][0] = processed[0];
                            g_smoothedRgb[i][1] = processed[1];
                            g_smoothedRgb[i][2] = processed[2];
                        } else {
                            for (int c = 0; c < 3; c++) {
                                int32_t prev = g_smoothedRgb[i][c];
                                int32_t raw = processed[c];
                                g_smoothedRgb[i][c] = (uint8_t)(prev + ((raw - prev) * (int32_t)smoothingAlpha) / 256);
                            }
                        }
                        rgbTriplets[i*3+0] = g_smoothedRgb[i][0];
                        rgbTriplets[i*3+1] = g_smoothedRgb[i][1];
                        rgbTriplets[i*3+2] = g_smoothedRgb[i][2];
                    } else {
                        rgbTriplets[i*3+0] = processed[0];
                        rgbTriplets[i*3+1] = processed[1];
                        rgbTriplets[i*3+2] = processed[2];
                    }
                }
                g_smoothedRgbValid = true;
                wled_send_rgb_zones(rgbTriplets, (int)g_numZones);

                // v2.2.2: DIAGNOSTIC ONLY, no color-pipeline change --
                // the format-diagnostic in v2.2.1 confirmed this game's
                // HDR mode correctly registers as A2R10G10B10_BT2020_PQ
                // (recognized=1), and the PQ decode math itself was
                // hand-verified against neutral AND slightly-imbalanced
                // near-black input with no red bias found either way.
                // But the user's own physical observation is that MUCH
                // of the strip goes red, not just the zone nearest a
                // hypothetical red UI element -- which neither of those
                // findings explains, and rules out "it's just real
                // content in one corner". This sends the RAW pre-decode
                // 32-bit pixel word for 3 zones spread across the strip
                // (index 0, middle, last) alongside the already-decoded
                // 8-bit RGB this session already has visibility into,
                // so the raw captured value itself -- not just this
                // session's interpretation of it -- can be checked.
                // Throttled to ~1x/sec (not gated on content changing)
                // so it samples steadily through both the "stable red"
                // and "oscillating" phases already observed, without
                // flooding the listener at update_frequency_hz.
                if (++s_rawDiagFrameCounter >= g_config.updateFrequencyHz) {
                    s_rawDiagFrameCounter = 0;
                    uint32_t diagZones[3] = { 0, g_numZones / 2, g_numZones - 1 };
                    uint8_t diagPacket[4 + 3 * 12];
                    memcpy(diagPacket, &g_activeFormat, 4);
                    for (int k = 0; k < 3; k++) {
                        uint32_t zi = diagZones[k];
                        uint64_t off = getTiledElementByteOffset(&kParamsBase, g_zoneX[zi], g_zoneY[zi]);
                        uint32_t rawPx = 0;
                        if (off + 4 <= BASE_PADDED_BUFFER_BYTES)
                            memcpy(&rawPx, (const void*)(liveBufferAddr + off), 4);
                        uint8_t dr, dg, db;
                        unpack(rawPx, &dr, &dg, &db);
                        uint8_t *entry = diagPacket + 4 + k * 12;
                        memcpy(entry + 0, &zi, 4);
                        memcpy(entry + 4, &rawPx, 4);
                        entry[8] = dr; entry[9] = dg; entry[10] = db; entry[11] = 0;
                    }
                    debug_send_raw(diagPacket, sizeof(diagPacket));
                }
            }
        }
        // If g_haveValidFormat is 0 (unknown/unconfirmed format), we
        // deliberately send nothing rather than guess -- this is the
        // direct fix for how the original §22 bug happened in the
        // first place: an unverified format assumption silently
        // producing wrong color instead of visibly doing nothing.

#if TIMING_ENABLED
        // Measured window intentionally covers the buffer-resolve +
        // sampling + color processing + UDP send above -- i.e.
        // everything this thread does per iteration except the sleep
        // itself. That's the number §26/§30 actually cares about: is
        // the real work fast enough to comfortably fit inside
        // sampleIntervalUs, not how precisely usleep() is honored.
        uint64_t t1 = sceKernelGetProcessTimeCounter();
        uint64_t elapsedTicks = t1 - t0;
        if (elapsedTicks < minTicks) minTicks = elapsedTicks;
        if (elapsedTicks > maxTicks) maxTicks = elapsedTicks;
        sumTicks += elapsedTicks;
        if (elapsedTicks > budgetTicks) overBudgetCount++;
        windowSamples++;

        int32_t curCpu = sceKernelGetCurrentCpu();
        if (curCpu < minCpuSeen) minCpuSeen = curCpu;
        if (curCpu > maxCpuSeen) maxCpuSeen = curCpu;
        if (lastCpu != -1 && curCpu != lastCpu) migrationCount++;
        lastCpu = curCpu;

        if (windowSamples >= TIMING_WINDOW_SIZE) {
            uint32_t minUs = (uint32_t)((minTicks * 1000000ULL) / tscFreq);
            uint32_t maxUs = (uint32_t)((maxTicks * 1000000ULL) / tscFreq);
            uint32_t avgUs = (uint32_t)(((sumTicks / windowSamples) * 1000000ULL) / tscFreq);
            send_timing_packet(minUs, maxUs, avgUs, windowSamples, overBudgetCount, windowId++,
                                (uint32_t)minCpuSeen, (uint32_t)maxCpuSeen, migrationCount);
            minTicks = UINT64_MAX; maxTicks = 0; sumTicks = 0;
            windowSamples = 0; overBudgetCount = 0;
            minCpuSeen = INT32_MAX; maxCpuSeen = -1; migrationCount = 0;
        }
#endif

        usleep(sampleIntervalUs);
    }
    return NULL;
}

int32_t attr_public plugin_load(int32_t argc, const char* argv[])
{
    ambient_load_config(); // v2.0: read /data/ps4_ambient_light.ini, or write a default template if absent
    buildZoneGeometry();   // fill g_zoneX/g_zoneY (now config-driven counts/corner/direction/offset/margins)

    int32_t hVideoOut = 0, hGnm = 0;
    sys_dynlib_load_prx("libSceVideoOut.sprx", &hVideoOut);
    sys_dynlib_load_prx("libSceGnmDriver.sprx", &hGnm);
    if (hVideoOut == 0 || hGnm == 0) return 0;

    sys_dynlib_dlsym(hVideoOut, "sceVideoOutRegisterBuffers", (void**)&sceVideoOutRegisterBuffersPtr);
    sys_dynlib_dlsym(hGnm, "sceGnmSubmitAndFlipCommandBuffers", (void**)&sceGnmSubmitAndFlipCommandBuffersPtr);
    if (sceVideoOutRegisterBuffersPtr == NULL || sceGnmSubmitAndFlipCommandBuffersPtr == NULL) return 0;

    // Same forwarding-stub guard as detile_verify_probe -- proven
    // necessary, do not skip.
    if (*((uint8_t*)sceVideoOutRegisterBuffersPtr) == 0xE9 ||
        *((uint8_t*)sceGnmSubmitAndFlipCommandBuffersPtr) == 0xE9) {
        return 0;
    }

    HOOK32(sceVideoOutRegisterBuffersPtr);
    HOOK32(sceGnmSubmitAndFlipCommandBuffersPtr);

    OrbisPthread thread;

    // --- Resource-isolation status: MEASURE, not guessed (see handoff) ---
    // Confirmed real, from the same <orbis/libkernel.h> this file already
    // includes:
    //   scePthreadAttrInit(OrbisPthreadAttr*)
    //   scePthreadAttrSetaffinity(OrbisPthreadAttr*, uint64_t mask)  -- must
    //     be set on the attr BEFORE scePthreadCreate; the post-create
    //     scePthreadSetaffinity() is declared with NO arguments in this
    //     header (unresolved stub), so it is not a usable path.
    //   scePthreadSetprio(OrbisPthread, int)                        -- set
    //     AFTER create, on the returned handle.
    //   sceKernelGetCurrentCpu(void)                                -- returns
    //     the core index the CALLING thread is on right now.
    //
    // NOT yet known, and not fabricated here:
    //   1) The uint64_t mask's bit-to-core mapping. The toolchain header's
    //      own comment on this line says the real type ("OrbisKernelCpumask")
    //      still needs porting -- i.e. even upstream hasn't confirmed
    //      bit N == core N for this platform. Assumed elsewhere on other
    //      platforms, not established here.
    //   2) Which PS4 core(s) game threads actually run on / avoid. No public
    //      source found documenting a game-vs-system core split for PS4,
    //      the way e.g. Xbox 360 docs spell out reserved hardware threads.
    //   3) scePthreadSetprio's number scale (does lower mean higher priority
    //      here, and by how much) -- signature confirmed, semantics not.
    //
    // TIMING_ENABLED (already 1 by default) now also samples
    // sceKernelGetCurrentCpu() once per telemetry window, so you can see,
    // empirically, which core this thread actually lands on across a real
    // session -- the same "measure it for real" approach as the loop-time
    // work in handoff §31. Once you've also logged sceKernelGetCurrentCpu()
    // from a thread you know is the game's own render/submission path (or
    // found another reliable way to identify it), fill in CORE_MASK below to
    // exclude that core, and verify with the same telemetry that the switch
    // actually took effect, before trusting it.
#if TIMING_ENABLED
    // Leave unset until (1) and (2) above are resolved. Setting affinity
    // with an unverified mask is a real risk here, not just a style
    // preference -- verified-wrong is one of this project's exact
    // recurring bug patterns (§7/§22/§38's format-mismatch bugs, §29's
    // scePadOpen frequency bug), all traced back to this same category of
    // "looked plausible, wasn't checked."
    // #define CORE_MASK (1ULL << N)   // fill in N once measured, not before
#endif

    scePthreadCreate(&thread, NULL, ambient_sample_thread, NULL, "ambient_sample_thread");
    // scePthreadAttrSetaffinity(&attr, CORE_MASK) + scePthreadCreate(&thread,
    // &attr, ...) is the confirmed path once CORE_MASK is real. Same for
    // scePthreadSetprio(thread, N) once N's semantics are confirmed against
    // scePthreadGetprio()'s (also confirmed) default reading.

    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char* argv[])
{
    UNHOOK(sceVideoOutRegisterBuffersPtr);
    UNHOOK(sceGnmSubmitAndFlipCommandBuffersPtr);
    if (g_wledSockfd >= 0) { close(g_wledSockfd); g_wledSockfd = -1; }
    return 0;
}

s32 attr_module_hidden module_start(s64 argc, const void *args) { return 0; }
s32 attr_module_hidden module_stop(s64 argc, const void *args) { return 0; }