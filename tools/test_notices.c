// Host test for the plugin's notifications -- the real plugin/source/notices.c, driven with made-up
// config state. Only the console calls are stand-ins (below). Checks what is said, and that each
// message is said once.
//
// Build and run from the repo root (one line):
//   gcc -Wall -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_notices tools/test_notices.c plugin/source/notices.c && /tmp/test_notices
#include "plugin_common.h"
#include "ambient_internal.h"
#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ---- stand-ins for what the console provides ----
#define TMP_VER "/tmp/test_notices.version"
static const char *remap(const char *p) { return strcmp(p, "/data/ps4_ambient_version") == 0 ? TMP_VER : p; }
int32_t sceKernelOpen(const char *path, int flags, int mode) {
    return open(remap(path), flags == 0 ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC), mode);
}
ssize_t sceKernelRead(int32_t fd, void *b, size_t n)    { return read(fd, b, n); }
ssize_t sceKernelWrite(int32_t fd, const void *b, size_t n) { return write(fd, b, n); }
int sceKernelClose(int32_t fd) { return close(fd); }
void klog(const char *fmt, ...) { (void)fmt; }

static int  g_sent;
static char g_last[512];
void Notify(const char *icon, const char *fmt, ...) {
    (void)icon;
    va_list ap; va_start(ap, fmt); vsnprintf(g_last, sizeof(g_last), fmt, ap); va_end(ap);
    g_sent++;
}

AmbientConfig g_config;
char g_titleId[16] = "";
uint32_t g_pluginVersion = 0x0309;
volatile uint32_t g_reportEnabled = 0;
volatile uint32_t g_configLoadCount, g_configDefaultCreated, g_configParseFailed, g_configBadCount;
char g_configBadKey[48] = "";

static void reset_load_state(void) {
    g_configDefaultCreated = g_configParseFailed = g_configBadCount = 0; g_configBadKey[0] = '\0';
}
#define SAID(sub) (g_sent == 1 && strstr(g_last, sub) != NULL)
#define QUIET()   (g_sent == 0)

int main(void)
{
    unlink(TMP_VER);
    g_config.notificationsEnabled = true;   // the default; section 8 turns it off
    strcpy(g_config.wledHost, "192.168.1.50");

    // 1. Update notice: the first run only records the version, the same version stays quiet,
    //    a new one says so (once), and the marker follows.
    g_sent = 0; ambient_notices_on_load();             assert(QUIET());
    g_sent = 0; ambient_notices_on_load();             assert(QUIET());
    g_pluginVersion = 0x030A;
    g_sent = 0; ambient_notices_on_load();             assert(SAID("updated to v3.10 (was v3.9)"));
    g_sent = 0; ambient_notices_on_load();             assert(QUIET());
    g_pluginVersion = 0x0309;
    g_sent = 0; ambient_notices_on_load();             assert(SAID("changed to v3.9"));
    printf("update notice: first run silent, new version once: PASSED\n");

    // 2. No WLED address: warn once, say when one appears, warn again if it goes away again.
    g_config.wledHost[0] = '\0';
    g_sent = 0; ambient_notices_on_load();             assert(SAID("no WLED address"));
    g_sent = 0; ambient_notices_on_reload();           assert(strstr(g_last, "config reloaded") && g_sent == 1); // no second warning
    strcpy(g_config.wledHost, "10.0.0.7");
    g_sent = 0; ambient_notices_on_reload();           assert(g_sent == 2 && strstr(g_last, "WLED address set (10.0.0.7)"));
    g_sent = 0; ambient_notices_on_reload();           assert(g_sent == 1 && strstr(g_last, "config reloaded"));
    g_config.wledHost[0] = '\0';
    g_sent = 0; ambient_notices_on_reload();           assert(g_sent == 2 && strstr(g_last, "no WLED address"));
    strcpy(g_config.wledHost, "10.0.0.7");
    ambient_notices_on_reload();                        // settle: address back, flag cleared
    printf("no WLED address: once, and the fix is acknowledged: PASSED\n");

    // 3. Default config created: one message that also covers the missing address.
    reset_load_state(); g_configDefaultCreated = 1; g_config.wledHost[0] = '\0';
    g_sent = 0; ambient_notices_on_load();             assert(SAID("created a default config"));
    strcpy(g_config.wledHost, "10.0.0.7"); reset_load_state();
    g_sent = 0; ambient_notices_on_reload();           assert(g_sent == 2); // "WLED address set" follows: the person did fix it
    reset_load_state(); g_configDefaultCreated = 1;
    g_sent = 0; ambient_notices_on_reload();           assert(SAID("config file was missing"));
    printf("default config created: PASSED\n");

    // 4. Ini problems: read failure, one bad value, several; the reload message names them instead of "reloaded".
    reset_load_state(); g_configParseFailed = 1;
    g_sent = 0; ambient_notices_on_load();             assert(SAID("couldn't read"));
    g_sent = 0; ambient_notices_on_reload();           assert(SAID("keeping the previous settings"));
    reset_load_state(); g_configBadCount = 1; strcpy(g_configBadKey, "[network] wled_port");
    g_sent = 0; ambient_notices_on_load();             assert(SAID("[network] wled_port"));
    g_sent = 0; ambient_notices_on_reload();           assert(SAID("[network] wled_port") && !strstr(g_last, "config reloaded"));
    reset_load_state(); g_configBadCount = 3; strcpy(g_configBadKey, "[color] gamma");
    g_sent = 0; ambient_notices_on_load();             assert(SAID("3 invalid values") && strstr(g_last, "[color] gamma"));
    reset_load_state();
    printf("ini problems named, not just \"reloaded\": PASSED\n");

    // 5. Debug logging: needs both keys for UDP, or the report file; said once, again after being turned off and on.
    g_sent = 0; ambient_notices_on_load();             assert(QUIET());
    strcpy(g_config.devIp, "192.168.1.9");
    g_sent = 0; ambient_notices_on_load();             assert(QUIET());               // dev_ip alone does nothing
    g_config.devLoggingEnabled = true;
    g_sent = 0; ambient_notices_on_load();             assert(SAID("UDP to 192.168.1.9"));
    g_sent = 0; ambient_notices_on_load();             assert(QUIET());
    g_reportEnabled = 1; g_config.devLoggingEnabled = false;
    g_sent = 0; ambient_notices_on_load();             assert(g_sent == 0);           // still "said": it was on and still is
    g_reportEnabled = 0;
    g_sent = 0; ambient_notices_on_load();             assert(QUIET());               // now off, flag reset
    g_reportEnabled = 1;
    g_sent = 0; ambient_notices_on_load();             assert(SAID("report file"));
    g_reportEnabled = 0; ambient_notices_on_load();
    printf("debug logging notice: PASSED\n");

    // 6. Hook failure names the title when it has one.
    g_sent = 0; ambient_notice_hook_failed("the video functions weren't found");
    assert(SAID("couldn't start on this title: the video functions"));
    strcpy(g_titleId, "CUSA00001");
    g_sent = 0; ambient_notice_hook_failed("x");       assert(SAID("CUSA00001"));
    printf("hook failure: PASSED\n");

    // 7. Unusable title: only after 5 s of the same state, once, and a recovery restarts the clock.
    uint64_t hz = 1000, t = 1000;
    g_sent = 0;
    ambient_notices_tick(t, hz, true, 0x12345678, false);                         // starts timing
    ambient_notices_tick(t + 4 * hz, hz, true, 0x12345678, false);                // 4 s: not yet
    assert(QUIET());
    ambient_notices_tick(t + 6 * hz, hz, true, 0x12345678, false);                // past 5 s
    assert(SAID("0x12345678"));
    ambient_notices_tick(t + 60 * hz, hz, true, 0x12345678, false);               // never again
    assert(g_sent == 1);
    g_sent = 0;
    ambient_notices_tick(t, hz, false, 0, true);
    ambient_notices_tick(t + 3 * hz, hz, false, 0, false);                        // recovered: clock resets
    ambient_notices_tick(t + 4 * hz, hz, false, 0, true);
    ambient_notices_tick(t + 8 * hz, hz, false, 0, true);                         // only 4 s since the restart
    assert(QUIET());
    ambient_notices_tick(t + 10 * hz, hz, false, 0, true);
    assert(SAID("screen buffer"));
    printf("unusable title: after a dwell, once: PASSED\n");

    // 8. [notifications] enabled=false: information goes quiet, what the strip can't work without stays.
    g_config.notificationsEnabled = false;
    strcpy(g_config.wledHost, "10.0.0.7"); g_config.devLoggingEnabled = false; g_reportEnabled = 0;
    reset_load_state();
    ambient_notices_on_reload(); g_sent = 0;                                        // settle any state
    g_pluginVersion = 0x030B;
    ambient_notices_on_load();                         assert(QUIET());               // update notice: silenced (marker still moves)
    g_sent = 0; ambient_notices_on_reload();           assert(QUIET());               // "config reloaded": silenced
    g_sent = 0; ambient_notice_hook_failed("x");       assert(QUIET());
    ambient_notices_tick(t, hz, true, 1, true); ambient_notices_tick(t + 9 * hz, hz, true, 1, true);
    assert(QUIET());                                                                  // unusable-title notices: silenced
    reset_load_state(); g_configDefaultCreated = 1; g_config.wledHost[0] = '\0';
    g_sent = 0; ambient_notices_on_load();             assert(SAID("no WLED address")); // "created" is silenced, so the address warning stands
    strcpy(g_config.wledHost, "10.0.0.7"); reset_load_state();
    g_sent = 0; ambient_notices_on_reload();           assert(SAID("WLED address set")); // and its acknowledgement
    reset_load_state(); g_configBadCount = 1; strcpy(g_configBadKey, "[color] gamma");
    g_sent = 0; ambient_notices_on_reload();           assert(SAID("[color] gamma"));  // an ignored value is still named
    reset_load_state(); g_configParseFailed = 1;
    g_sent = 0; ambient_notices_on_load();             assert(SAID("couldn't read"));
    reset_load_state(); g_config.devLoggingEnabled = true; strcpy(g_config.devIp, "192.168.1.9");
    g_sent = 0; ambient_notices_on_load();             assert(SAID("debug logging is on")); // left on by mistake: always said
    g_config.notificationsEnabled = true;
    printf("[notifications] enabled=false: information silenced, essential notices stay: PASSED\n");

    unlink(TMP_VER);
    printf("ALL NOTICE CHECKS PASSED\n");
    return 0;
}
