// notices.c -- part of ps4_ambient_light (plugin only; settings.c and the other shared files never...
// More: docs/code-comments.md (plugin/source/notices.c:1)
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <orbis/libkernel.h>

#include "plugin_common.h"
#include "ambient_internal.h"

#define VERSION_MARKER   AMBILIGHT_DATA_DIR "/ps4_ambient_version"  // g_pluginVersion as 8 hex digits, one line
#define NOTICE_DWELL_SEC 5   // how long a title has to stay unusable before we say so (loading screens come and go)

extern uint32_t g_pluginVersion;   // hooks.c, 0xMMmm

static bool     s_hostWarned;      // "no WLED address" was sent and no address has appeared since
static bool     s_debugSaid;       // "debug logging is on" was sent and it has not been turned off since
static bool     s_fmtTiming, s_fmtSaid, s_bufTiming, s_bufSaid;
static uint64_t s_fmtSince, s_bufSince;

// essential = the strip can't work, or the person is being misled, without it: sent whatever
// [notifications] enabled says. Everything else is information and respects the switch.
static void say(bool essential, const char *fmt, ...)
{
    if (!essential && !g_config.notificationsEnabled) return;
    char msg[176];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    Notify(TEX_ICON_SYSTEM, "PS4 Ambilight: %s", msg);
}

// plugin update (marker file) The first run only records the version: a fresh install already knows it...
// More: docs/code-comments.md (plugin/source/notices.c:48)
static void version_notice(void)
{
    uint32_t cur = g_pluginVersion, old = 0;
    bool had = false;
    char buf[16] = {0};
    int32_t fd = sceKernelOpen(VERSION_MARKER, 0 /* O_RDONLY */, 0);
    if (fd >= 0) {
        ssize_t n = sceKernelRead(fd, buf, sizeof(buf) - 1);
        sceKernelClose(fd);
        if (n > 0) { buf[n] = '\0'; old = (uint32_t)strtoul(buf, NULL, 16); had = true; }
    }
    if (had && old == cur) return;
    char out[16];
    int len = snprintf(out, sizeof(out), "%08x\n", (unsigned)cur);
    // O_WRONLY|O_CREAT|O_TRUNC, the BSD values (see state.c); TRUNC so a shorter value leaves no tail.
    fd = sceKernelOpen(VERSION_MARKER, 0x0001 | 0x0200 | 0x0400, 0777);
    if (fd >= 0) { sceKernelWrite(fd, out, (size_t)len); sceKernelClose(fd); }
    if (had)
        say(false, "plugin %s v%u.%u (was v%u.%u).", cur > old ? "updated to" : "changed to",
            (unsigned)((cur >> 8) & 0xFF), (unsigned)(cur & 0xFF),
            (unsigned)((old >> 8) & 0xFF), (unsigned)(old & 0xFF));
}

// WLED address Only "there is none", never "it doesn't answer".
// More: docs/code-comments.md (plugin/source/notices.c:74)
static void host_check(bool suppressWarning)
{
    if (g_config.wledHost[0] == '\0') {
        if (!s_hostWarned && !suppressWarning) {
            say(true, "no WLED address is set, so the strip won't light. Set wled_host in " AMBIENT_CONFIG_PATH " or in the companion app.");
            s_hostWarned = true;
        }
    } else if (s_hostWarned) {
        say(true, "WLED address set (%s). Sending to the strip.", g_config.wledHost);
        s_hostWarned = false;
    }
}

// ---- debug logging ----
// UDP debug packets or the report file: both are tester tools that cost a little and are easy to leave on.
static void debug_check(void)
{
    bool udp = g_config.devLoggingEnabled && g_config.devIp[0] != '\0';
    bool rep = g_reportEnabled != 0;
    if (!udp && !rep) { s_debugSaid = false; return; }
    if (s_debugSaid) return;
    s_debugSaid = true;
    say(true, "debug logging is on (%s%s%s). Turn it off when you're done testing.",
        udp ? "UDP to " : "", udp ? g_config.devIp : "", rep ? (udp ? " + report file" : "report file") : "");
}

// What the load that just ran said about the ini. Returns true if it sent a message about the ini
// itself (so a caller can avoid piling a second one on top).
static bool config_problem_notice(void)
{
    if (g_configParseFailed) {
        say(true, "couldn't read " AMBIENT_CONFIG_PATH ", so it is using defaults. Fix the file or delete it to get a fresh one.");
        return true;
    }
    if (g_configBadCount > 0) {
        if (g_configBadCount == 1)
            say(true, "ignoring an invalid value for %s in the config; its default is used.", g_configBadKey);
        else
            say(true, "ignoring %u invalid values in the config, starting with %s; defaults are used.",
                (unsigned)g_configBadCount, g_configBadKey);
        return true;
    }
    return false;
}

void ambient_notices_on_load(void)
{
    version_notice();
    // The "created" message also covers the missing address, so no second message for the same moment --
    // but only if it is actually shown: with notifications off it isn't, and the address warning stands.
    bool created = g_configDefaultCreated != 0 && g_config.notificationsEnabled;
    if (g_configDefaultCreated) {
        say(false, "created a default config at " AMBIENT_CONFIG_PATH ". Set wled_host in it, or in the companion app, to light your strip.");
        s_hostWarned = created && (g_config.wledHost[0] == '\0');
    } else {
        config_problem_notice();
    }
    host_check(created);
    debug_check();
}

void ambient_notices_on_reload(void)
{
    if (g_configDefaultCreated) {
        say(false, "the config file was missing; wrote a default at " AMBIENT_CONFIG_PATH ".");
    } else if (g_configParseFailed) {
        say(true, "config changed but couldn't be read; keeping the previous settings.");
    } else if (g_configBadCount > 0) {
        config_problem_notice();   // names the bad key; this stands in for "config reloaded"
    } else {
        say(false, "config reloaded.");
    }
    host_check(false);
    debug_check();
}

void ambient_notice_hook_failed(const char *reason)
{
    if (g_titleId[0]) say(false, "couldn't start on this title (%s): %s.", g_titleId, reason);
    else              say(false, "couldn't start on this title: %s.", reason);
}

// Called every sampler pass. A state has to hold for NOTICE_DWELL_SEC before it is reported, so a
// loading screen or a format switch doesn't produce a message; each is reported once per launch.
void ambient_notices_tick(uint64_t nowTicks, uint64_t tscFreq, bool formatUnknown, uint32_t format, bool bufferUnreadable)
{
    uint64_t dwell = tscFreq * (uint64_t)NOTICE_DWELL_SEC;

    if (!formatUnknown) {
        s_fmtTiming = false;
    } else if (!s_fmtTiming) {
        s_fmtTiming = true; s_fmtSince = nowTicks;
    } else if (!s_fmtSaid && nowTicks - s_fmtSince >= dwell) {
        s_fmtSaid = true;
        say(false, "this title uses a screen format the plugin doesn't know (0x%08x), so no colours are sent.", (unsigned)format);
    }

    if (!bufferUnreadable) {
        s_bufTiming = false;
    } else if (!s_bufTiming) {
        s_bufTiming = true; s_bufSince = nowTicks;
    } else if (!s_bufSaid && nowTicks - s_bufSince >= dwell) {
        s_bufSaid = true;
        say(false, "can't read this title's screen buffer, so the strip can't follow it.");
    }
}
