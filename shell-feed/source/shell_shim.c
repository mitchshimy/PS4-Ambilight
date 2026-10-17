// shell_shim.c -- the symbols the 7 shared plugin files (tiling.c, pixel_formats.c, zones.c, gamma.c,
// color_processing.c, settings.c, config.c) expect from plugin files this module does not link:
// network.c, buffer_guard.c, report.c. Each is a real value or a safe no-op here; the comment on each says why.
#include "shell_internal.h"

// =====================================================================
// [v3.9.4 / Stage C merge] PS4-Ambilight's settings, zone geometry and colour chain -- shared code,
// not pasted. Through v24 (the shell-capture-research project's own history) this section was a
// byte-for-byte paste of the sister's source (PS4-Ambilight 89199fe), kept honest by
// tools/host_ini_check/run.sh diffing the two byte for byte. That stopped being possible the moment
// the sister itself changed underneath the paste (v3.9.4 added g_presetOverride to settings.c,
// specifically so this file could stop re-implementing preset selection by hand) -- a paste cannot
// both match its source and diverge from it to add new behaviour. So this merge's Stage C replaces
// the paste with the real thing: tiling.c, pixel_formats.c, zones.c, gamma.c, color_processing.c,
// settings.c and config.c are compiled from ../plugin/source/ (see shell-feed/Makefile's second
// source list) and declared through the sister's own real headers, included below. What stays
// local to this file: the probe's own v17 tiling math just above this comment (kept for the
// buffer-discovery/self-heal code that has always used it, unrelated to the zone sampler), and the
// small shim just below (11 externs the shared files need from sister files this project
// does not link -- network.c, buffer_guard.c, report.c -- each a safe default or a no-op here; see
// each one's own comment for why).
// =====================================================================

// ---- shim: 11 symbols the 7 shared files above reference from sister files this project doesn't
// link (network.c, buffer_guard.c, report.c). Confirmed to be exactly these 11 by grepping all 7
// shared .c files for every extern ambient_internal.h declares; see shell-feed/docs/findings.md.
uint32_t g_autoLetterboxTop = 0, g_autoLetterboxRight = 0, g_autoLetterboxBottom = 0, g_autoLetterboxLeft = 0;
                                   // real, not a stub: the shell has no letterbox detection, so these
                                   // stay 0 (zones.c's effMargin*() helpers), same as before this refactor --
                                   // just no longer `static`, since zones.c now reads them as its own extern.
volatile uint64_t g_readableLimit = BUF_BYTES17;
                                   // real, not a stub: sampleZoneAverage()'s bounds check (no read past
                                   // one display buffer), same BUF_BYTES17 this probe's own buffer
                                   // discovery already uses and already confirmed on real captures --
                                   // just no longer `static`, since zones.c now reads it as its own extern.
volatile uint32_t g_gpuOnlyRemap = 0;       // GPU-only buffer remap is a game-process concern (buffer_guard.c); unused here
volatile uint32_t g_reportEnabled = 0;      // the sister's own [compat] report_file flag (report.c); this probe tracks its
                                             // OWN report_file setting separately as g_shReportOn (see sh_parse_aux below)
bool g_isBackgrounded = false;              // set by the sister's sample_thread.c/network.c, read inside
                                             // ambient_check_config_reload() (settings.c), which this probe never calls --
                                             // shell_cfg_poll() below is its own, separate poll function
void relay_send_external_source(bool active) { (void)active; } // [network] relay: not part of this design -- see shell-feed/docs/architecture.md "Decisions in force"
void send_config_reload_debug_packet(uint32_t event, uint32_t statErrno, uint64_t curMtime, uint64_t lastMtime,
                                      uint64_t curSize, uint64_t lastSize, uint32_t checkCount,
                                      const uint8_t *contentPreview, uint32_t curHash, uint32_t lastHash)
{ (void)event; (void)statErrno; (void)curMtime; (void)lastMtime; (void)curSize; (void)lastSize;
  (void)checkCount; (void)contentPreview; (void)curHash; (void)lastHash; } // diagnostic-only, part of
                                             // ambient_check_config_reload(), which this probe never calls
void ambient_read_content_preview(uint8_t *out, size_t previewLen) { memset(out, 0, previewLen); } // feeds the no-op packet above
void debug_send_raw(const uint8_t *data, int len) { (void)data; (void)len; } // zones.c's own [dev]-gated diagnostic
                                             // packets (HDR2200 detection etc.), separate from this probe's [dev] mirror
