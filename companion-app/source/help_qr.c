// help_qr.c -- wraps qrcodegen.c (vendored, see its own header) into
// the one thing ui_screens.c's QR modal actually needs: "is module
// (x, y) dark, in a grid that already includes the quiet-zone
// border". Everything else -- picking an ECC level, a version range,
// a mask, the quiet zone width -- is decided once, here, so the
// renderer never has to know qrcodegen's API at all.

#include "help_qr.h"
#include "qrcodegen.h"

#include <string.h>

// Standard-recommended quiet zone is 4 modules; anything less risks a
// scanner that expects full margin failing to lock onto the finder
// patterns, especially photographed off a TV/monitor rather than
// printed.
#define QUIET_ZONE_MODULES 4

static uint8_t s_qr[qrcodegen_BUFFER_LEN_MAX];
static uint8_t s_tempBuffer[qrcodegen_BUFFER_LEN_MAX];
static bool s_ready = false;
static int s_rawSize = 0; // qrcodegen's own size, i.e. without the quiet zone

bool help_qr_init(void)
{
    if (s_ready) return true; // HELP_QR_URL is a compile-time constant -- nothing to redo

    // MEDIUM: matches this app's own on-console rendering (modules
    // land on exact pixel-aligned rects, no photographic blur to fight,
    // so LOW would scan fine too) while still tolerating a stray dead
    // pixel or two and a phone camera at a shallow angle. boostEcl=true
    // lets qrcodegen raise this further for free if the fixed version
    // range below has spare capacity at MEDIUM already.
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
