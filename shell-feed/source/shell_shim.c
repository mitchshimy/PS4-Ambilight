// shell_shim.c -- the symbols the 7 shared plugin files (tiling.c, pixel_formats.c, zones.c, gamma.c...
// More: docs/code-comments.md (shell-feed/source/shell_shim.c:1)
#include "shell_internal.h"

// [v3.9.4 / Stage C merge] PS4-Ambilight's settings, zone geometry and colour chain -- shared code...
// More: docs/code-comments.md (shell-feed/source/shell_shim.c:6)

// shim: 11 symbols the 7 shared files above reference from sister files this project doesn't link...
// More: docs/code-comments.md (shell-feed/source/shell_shim.c:24)
uint32_t g_autoLetterboxTop = 0, g_autoLetterboxRight = 0, g_autoLetterboxBottom = 0, g_autoLetterboxLeft = 0;
                                   // real, not a stub: the shell has no letterbox detection, so these stay 0 (zones.c's effMargin*()...
                                   // More: docs/code-comments.md (shell-feed/source/shell_shim.c:28)
volatile uint64_t g_readableLimit = BUF_BYTES17;
                                   // real, not a stub: sampleZoneAverage()'s bounds check (no read past one display buffer), same...
                                   // More: docs/code-comments.md (shell-feed/source/shell_shim.c:32)
volatile uint32_t g_gpuOnlyRemap = 0;       // GPU-only buffer remap is a game-process concern (buffer_guard.c); unused here
volatile uint32_t g_reportEnabled = 0;      // the sister's own [compat] report_file flag (report.c); this probe tracks its
                                             // OWN report_file setting separately as g_shReportOn (see sh_parse_aux below)
bool g_isBackgrounded = false;              // set by the sister's sample_thread.c/network.c, read inside
                                             // ambient_check_config_reload() (settings.c), which this probe never calls --
                                             // shell_cfg_poll() below is its own, separate poll function
void relay_send_external_source(bool active) { (void)active; } // settings.c's reload path references these two; this module never calls that path.
void relay_release_external_source(void) {}                    // The shell feed's own relay signal is shell_relay.c.
void send_config_reload_debug_packet(uint32_t event, uint32_t statErrno, uint64_t curMtime, uint64_t lastMtime,
                                      uint64_t curSize, uint64_t lastSize, uint32_t checkCount,
                                      const uint8_t *contentPreview, uint32_t curHash, uint32_t lastHash)
{ (void)event; (void)statErrno; (void)curMtime; (void)lastMtime; (void)curSize; (void)lastSize;
  (void)checkCount; (void)contentPreview; (void)curHash; (void)lastHash; } // diagnostic-only, part of
                                             // ambient_check_config_reload(), which this probe never calls
void ambient_read_content_preview(uint8_t *out, size_t previewLen) { memset(out, 0, previewLen); } // feeds the no-op packet above
void debug_send_raw(const uint8_t *data, int len) { (void)data; (void)len; } // zones.c's own [dev]-gated diagnostic
                                             // packets (HDR2200 detection etc.), separate from this probe's [dev] mirror
