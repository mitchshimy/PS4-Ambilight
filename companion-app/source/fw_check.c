// fw_check.c -- reads the console's system SDK version once at startup and, when the shell feed
// has no complete offsets for it, fills g_state.fwTitle/fwBody. Home shows them as a warning panel
// (only while the "Shell feed" switch is on: the game plugin works on every firmware, only the shell
// feed needs this check), and launching the payload says it again. A warning only, never a block:
// the payload itself refuses cleanly on a firmware it doesn't know.
//
// The homebrew app can't call GoldHEN's sys_sdk_version(), so it reads kern.sdk_version through
// sysctlbyname. NOT yet confirmed on a console: if the call fails or the value isn't valid BCD,
// fw_decode() rejects it and nothing is shown -- a missing warning, never a wrong one.
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "app_state.h"
#include "fw_support.h"
#include "fw_check.h"

// From libkernel (already linked). Declared here rather than via <sys/sysctl.h>, which this SDK
// snapshot may not ship.
extern int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, const void *newp, size_t newlen);

void firmware_check(void)
{
    g_state.fwTitle[0] = '\0';
    g_state.fwBody[0] = '\0';
    uint32_t sdk = 0;
    size_t len = sizeof(sdk);
    int major = 0, minor = 0;
    if (sysctlbyname("kern.sdk_version", &sdk, &len, NULL, 0) != 0 || len != sizeof(sdk)) return;
    if (!fw_decode(sdk, &major, &minor)) return;
    fw_notice(fw_classify(major, minor), major, minor,
              g_state.fwTitle, sizeof(g_state.fwTitle), g_state.fwBody, sizeof(g_state.fwBody));
    g_dirty = true;
}
