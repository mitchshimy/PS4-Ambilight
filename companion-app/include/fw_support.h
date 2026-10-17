// fw_support.h -- firmware status for the current standalone shell-feed loader...
// More: docs/code-comments.md (common/fw_support.h:1)
#ifndef FW_SUPPORT_H
#define FW_SUPPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    FW_UNKNOWN = 0,  // couldn't read/decode the version: say nothing
    FW_SUPPORTED,    // confirmed on hardware with the current payload and shell feed
    FW_UNTESTED,     // in the loader's table, no end-to-end test yet
    FW_NOT_LISTED    // not in the loader's table: it will most likely refuse
} FwSupport;

// Decodes the SDK version as BCD: major in bits 24-31, minor in bits 16-23.
// A failed decode means "unknown" rather than guessing and showing a potentially false warning.
static inline bool fw_decode(uint32_t sdk, int *major, int *minor)
{
    uint32_t hi = (sdk >> 24) & 0xFFu, lo = (sdk >> 16) & 0xFFu;
    if (sdk == 0) return false;
    if ((hi >> 4) > 9 || (hi & 0xF) > 9 || (lo >> 4) > 9 || (lo & 0xF) > 9) return false;
    *major = (int)((hi >> 4) * 10 + (hi & 0xF));
    *minor = (int)((lo >> 4) * 10 + (lo & 0xF));
    return true;
}

// Exact lookups, not ranges. Values are major*100 + minor.
static inline FwSupport fw_classify(int major, int minor)
{
    static const int kListed[] = {
        505, 672, 702, 755, 803, 852, 900, 904, 960, 1002,
        1071, 1100, 1102, 1152, 1202, 1252, 1300, 1304, 1350, 1352
    };
    int v = major * 100 + minor;
    if (v == 900 || v == 1252) return FW_SUPPORTED;
    for (unsigned i = 0; i < sizeof(kListed) / sizeof(kListed[0]); i++)
        if (kListed[i] == v) return FW_UNTESTED;
    return FW_NOT_LISTED;
}

// A one-line title and a one-line explanation for the Home warning panel and the status line after
// the payload is sent. Confirmed and unknown say nothing.
static inline bool fw_notice(FwSupport s, int major, int minor, char *title, size_t tn, char *body, size_t bn)
{
    if (tn) title[0] = '\0';
    if (bn) body[0] = '\0';
    if (s == FW_UNTESTED) {
        snprintf(title, tn, "Firmware %d.%02d is not yet verified with the shell loader.", major, minor);
        snprintf(body, bn, "The loader has an entry for it, but only 9.00 and 12.52 are confirmed.");
        return true;
    }
    if (s == FW_NOT_LISTED) {
        snprintf(title, tn, "Firmware %d.%02d is not in the shell loader's firmware list.", major, minor);
        snprintf(body, bn, "The payload will most likely report unsupported firmware and not start.");
        return true;
    }
    return false;
}

#endif // FW_SUPPORT_H
