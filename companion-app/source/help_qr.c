// help_qr.c -- wraps qrcodegen.c (vendored, see its own header) into the one thing ui_screens.c's QR...
// More: docs/code-comments.md (companion-app/source/help_qr.c:1)

#include "help_qr.h"
#include "qrcodegen.h"

#include <string.h>

// Standard-recommended quiet zone is 4 modules; anything less risks a scanner that expects full margin...
// More: docs/code-comments.md (companion-app/source/help_qr.c:13)
#define QUIET_ZONE_MODULES 4

static uint8_t s_qr[qrcodegen_BUFFER_LEN_MAX];
static uint8_t s_tempBuffer[qrcodegen_BUFFER_LEN_MAX];
static bool s_ready = false;
static int s_rawSize = 0; // qrcodegen's own size, i.e. without the quiet zone

bool help_qr_init(void)
{
    if (s_ready) return true; // HELP_QR_URL is a compile-time constant -- nothing to redo

    // MEDIUM: matches this app's own on-console rendering (modules land on exact pixel-aligned rects, no...
    // More: docs/code-comments.md (companion-app/source/help_qr.c:28)
    bool ok = qrcodegen_encodeText(
        HELP_QR_URL,
        s_tempBuffer,
        s_qr,
        qrcodegen_Ecc_MEDIUM,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true);

    if (!ok) return false;

    s_rawSize = qrcodegen_getSize(s_qr);
    s_ready = true;
    return true;
}

int help_qr_size(void)
{
    if (!s_ready) return 0;
    return s_rawSize + 2 * QUIET_ZONE_MODULES;
}

bool help_qr_module(int x, int y)
{
    if (!s_ready) return false;

    int rx = x - QUIET_ZONE_MODULES;
    int ry = y - QUIET_ZONE_MODULES;
    if (rx < 0 || ry < 0 || rx >= s_rawSize || ry >= s_rawSize)
        return false; // quiet zone itself is light, not an error case

    return qrcodegen_getModule(s_qr, rx, ry);
}
