// Run from companion-app/: gcc -Wall -Wextra -Iinclude -o /tmp/test_goldhen_config tests/test_goldhen_config.c && /tmp/test_goldhen_config
// Reading the one "Enable Plugins Loader" value in GoldHEN's config.ini (read-only: see goldhen_config.h).
#include "goldhen_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// The two real files from a console differed only in this one value; this is their shape.
#define CFG(loader) \
    "[BinLoader]\nEnabled = 1\n\n[FTPServer]\nEnabled = 1\n\n[Cheat]\nShow_TitleId = 1\nMenu_Enabled = 1\n\n" \
    "[PluginLoader]\nPluginLoader_Enabled = " loader "\nGame_Patch_Enabled = 0\nAIO_Fix_Enabled = 0\n\n" \
    "[DebugSettings]\nEnabled = 0\n\n[Date_Time]\nAuto_Update_Enabled = 0\n"

static PluginLoaderState st(const char *t) { return goldhen_plugin_loader_state(t, strlen(t)); }

int main(void)
{
    // ---- reading ----
    assert(st(CFG("0")) == LOADER_OFF);
    assert(st(CFG("1")) == LOADER_ON);
    printf("real-file shape, off and on: ok\n");

    assert(st("") == LOADER_UNKNOWN);
    assert(goldhen_plugin_loader_state(NULL, 0) == LOADER_UNKNOWN);
    assert(st("[BinLoader]\nEnabled = 1\n") == LOADER_UNKNOWN);                      // no section
    assert(st("[PluginLoader]\nGame_Patch_Enabled = 0\n") == LOADER_UNKNOWN);        // no key
    assert(st("[PluginLoader]\nPluginLoader_Enabled = maybe\n") == LOADER_UNKNOWN);  // not 0 or 1
    assert(st("[PluginLoader]\nPluginLoader_Enabled =\n") == LOADER_UNKNOWN);        // empty
    assert(st("[PluginLoader]\nPluginLoader_Enabled = 10\n") == LOADER_UNKNOWN);     // not exactly 1
    assert(st("[PluginLoader]\nPluginLoader_Enabled = 2\n") == LOADER_UNKNOWN);
    printf("anything unexpected is unknown (never blocks): ok\n");

    // the key only counts inside its own section
    assert(st("[Other]\nPluginLoader_Enabled = 0\n[PluginLoader]\nGame_Patch_Enabled = 1\n") == LOADER_UNKNOWN);
    assert(st("[PluginLoader]\nGame_Patch_Enabled = 1\n[Other]\nPluginLoader_Enabled = 0\n") == LOADER_UNKNOWN);
    assert(st("[PluginLoader]\nx = 1\n[Other]\ny = 1\n[PluginLoader]\nPluginLoader_Enabled = 0\n") == LOADER_OFF);
    // a commented line is not the setting
    assert(st("[PluginLoader]\n; PluginLoader_Enabled = 0\nGame_Patch_Enabled = 1\n") == LOADER_UNKNOWN);
    assert(st("[PluginLoader]\n# PluginLoader_Enabled = 0\nPluginLoader_Enabled = 1\n") == LOADER_ON);
    // spacing, case, CRLF, BOM, no trailing newline
    assert(st("[PluginLoader]\nPluginLoader_Enabled=0") == LOADER_OFF);
    assert(st("  [ pluginloader ]  \n\tpluginloader_enabled\t=\t0  \n") == LOADER_OFF);
    assert(st("[PluginLoader]\r\nPluginLoader_Enabled = 0\r\nGame_Patch_Enabled = 1\r\n") == LOADER_OFF);
    assert(st("\xEF\xBB\xBF[PluginLoader]\nPluginLoader_Enabled = 1\n") == LOADER_ON);
    printf("sections, comments, spacing, CRLF, BOM: ok\n");

    // ---- the file is only ever read: this header has no way to write it (see its comment) ----
    printf("ALL PASSED\n");
    return 0;
}
