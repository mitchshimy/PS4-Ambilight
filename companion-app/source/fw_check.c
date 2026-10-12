// fw_check.c -- reads the console firmware once at startup and adds a notice when the current
// standalone shell-feed loader is untested on that version, or has no entry for it (it will most
// likely refuse). The notice never blocks anything; the payload decides at runtime. The old loader's
// offset table no longer describes the shipped payload.
// The game plugin's firmware behavior is separate from the shell-feed loader.

// The app reads kern.sdk_version through sysctlbyname. If the call fails or the value is not valid
// BCD, fw_decode() rejects it and nothing is shown rather than guessing a firmware version.
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
