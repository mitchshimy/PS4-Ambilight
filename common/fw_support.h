// fw_support.h -- which PS4 firmwares the shell feed payload has offsets for, and the text to show
// when the console isn't on one of them. Pure logic, header-only, no console calls, so
// tools/test_fw_support.c can check it on a PC. Used by the companion app (warning on Home, and again
// when the payload is launched). The game plugin works on every firmware and doesn't use it.
//
// The table is shell-feed/docs/support-matrix.md, by hand. When a firmware moves between tables
// there, move it here too. companion-app/include/fw_support.h is a copy (that app only has
// -Iinclude): keep the two identical.
//
// Version encoding: the system SDK version is read as BCD, one byte per field -- major in bits
// 24-31, minor in bits 16-23, each byte two decimal digits written as hex (12.50 is 0x1250xxxx,
// 9.00 is 0x0900xxxx). That matches how Sony prints it ("12.50"), but it has NOT been checked on a
// console from this repo. fw_decode() therefore rejects anything that isn't valid BCD, and every
// caller treats a failed decode as "don't know, say nothing", so a wrong guess about the encoding
// shows up as a missing warning, never as a false one.
#ifndef FW_SUPPORT_H
#define FW_SUPPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    FW_UNKNOWN = 0,   // couldn't read or decode the version: say nothing
    FW_SUPPORTED,     // offsets complete (support-matrix.md "Fully supported")
    FW_PARTIAL,       // some offsets still TODO; the payload refuses at runtime
    FW_UNSUPPORTED    // no offset data at all
} FwSupport;

// Decodes a BCD system SDK version into major/minor. False for 0 or any byte with a non-decimal digit.
static inline bool fw_decode(uint32_t sdk, int *major, int *minor)
{
    uint32_t hi = (sdk >> 24) & 0xFFu, lo = (sdk >> 16) & 0xFFu;
    if (sdk == 0) return false;
    if ((hi >> 4) > 9 || (hi & 0xF) > 9 || (lo >> 4) > 9 || (lo & 0xF) > 9) return false;
    *major = (int)((hi >> 4) * 10 + (hi & 0xF));
    *minor = (int)((lo >> 4) * 10 + (lo & 0xF));
    return true;
}

// Exact lookup, not ranges: 6.00 or 9.20 sit "between" supported versions and are NOT supported.
// Values are major*100 + minor.
static inline FwSupport fw_classify(int major, int minor)
{
    static const int supported[] = {
        505, 671, 672, 750, 751, 755, 800, 801, 803, 850, 852, 900, 903, 904, 950, 951, 960,
        1000, 1001, 1050, 1070, 1071, 1100, 1102, 1150, 1152, 1200, 1202
    };
    static const int partial[] = { 1250, 1252, 1300, 1302, 1304, 1350, 1352, 1400 };
    int v = major * 100 + minor;
    for (unsigned i = 0; i < sizeof(supported) / sizeof(supported[0]); i++)
        if (supported[i] == v) return FW_SUPPORTED;
    for (unsigned i = 0; i < sizeof(partial) / sizeof(partial[0]); i++)
        if (partial[i] == v) return FW_PARTIAL;
    return FW_UNSUPPORTED;
}

// Fills a one-line title and a one-line explanation. Returns false (and leaves both empty) when
// there is nothing to say: supported, or unknown. Short enough for one system notification and for
// the two lines of the companion app's warning panel.
static inline bool fw_notice(FwSupport s, int major, int minor, char *title, size_t tn, char *body, size_t bn)
{
    if (tn) title[0] = '\0';
    if (bn) body[0] = '\0';
    if (s == FW_PARTIAL) {
        snprintf(title, tn, "Firmware %d.%02d: shell feed offsets incomplete.", major, minor);
        snprintf(body, bn, "The shell feed payload will refuse to load. See shell-feed/docs/support-matrix.md.");
        return true;
    }
    if (s == FW_UNSUPPORTED) {
        snprintf(title, tn, "Firmware %d.%02d is not in the shell feed support list.", major, minor);
        snprintf(body, bn, "The shell feed payload will refuse to load. See shell-feed/docs/support-matrix.md.");
        return true;
    }
    return false;
}

#endif // FW_SUPPORT_H
