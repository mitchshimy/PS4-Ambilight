// install_plan.h -- what Home shows, and what its one button does, once the shell feed is
// part of the picture. Pure decision logic with no console calls, so
// tests/test_install_plan.c can check every combination on a PC.
//
// Three files can be pending: the plugin, the shell feed's .prx and the loader payload.
// The plugin keeps the states it always had (UiInstallState). The other two are folded
// into one "Shell feed" unit, because nobody using the app cares which of the two a
// problem is in -- they get one banner and one state.
//
// Shell feed OFF is the default and means "behave exactly as before": shell_summarize()
// returns SHELL_OFF, home_plan() ignores it, and the Test strip rule is the old one.
#ifndef INSTALL_PLAN_H
#define INSTALL_PLAN_H

#include <stdbool.h>

// The plugin's own states (moved here from ui_screens.h unchanged).
typedef enum {
    UI_INSTALL_NONE,     // "Plugin not installed"
    UI_INSTALL_UPDATE,   // "Update available"
    UI_INSTALL_DISABLED, // .prx is present, but its plugins.ini entry is
                          // commented out or missing entirely -- reuses
                          // the same CTA slot as Install/Update
    UI_INSTALL_OK        // "Plugin installed"
} UiInstallState;

// The three downloadable files. ASSET_SHELL_PRX and ASSET_PAYLOAD together are "the shell feed".
typedef enum {
    ASSET_PLUGIN = 0,
    ASSET_SHELL_PRX,
    ASSET_PAYLOAD,
    ASSET_COUNT
} AssetId;

// What is known about one file. ASSET_UPDATE needs a successful remote check; offline, a
// file that is there is simply ASSET_PRESENT (same fail-quiet rule the plugin check has).
typedef enum {
    ASSET_MISSING = 0,   // not on disk, or zero bytes
    ASSET_PRESENT,
    ASSET_UPDATE         // on disk, and the published checksum differs
} AssetStatus;

typedef enum {
    SHELL_OFF,              // Shell feed switched off in Set up: nothing shell-related is shown or checked
    SHELL_NOT_INSTALLED,    // both files missing
    SHELL_INCOMPLETE,       // exactly one of the two missing
    SHELL_UPDATE_AVAILABLE, // both there, at least one has a newer published build
    SHELL_OK
} ShellFeedState;

#define SHELL_BIT_PRX      1   // bit in missingMask / updateMask
#define SHELL_BIT_PAYLOAD  2

typedef struct {
    ShellFeedState state;
    int missing;      // 0..2
    int updates;      // 0..2 (only counts files that are present)
    int missingMask;  // SHELL_BIT_*
    int updateMask;   // SHELL_BIT_*
} ShellSummary;

static inline ShellSummary shell_summarize(bool shellOn, AssetStatus prx, AssetStatus payload)
{
    ShellSummary s = { SHELL_OFF, 0, 0, 0, 0 };
    if (!shellOn) return s;

    if (prx == ASSET_MISSING)          { s.missing++; s.missingMask |= SHELL_BIT_PRX; }
    else if (prx == ASSET_UPDATE)      { s.updates++; s.updateMask  |= SHELL_BIT_PRX; }
    if (payload == ASSET_MISSING)      { s.missing++; s.missingMask |= SHELL_BIT_PAYLOAD; }
    else if (payload == ASSET_UPDATE)  { s.updates++; s.updateMask  |= SHELL_BIT_PAYLOAD; }

    if (s.missing == 2)      s.state = SHELL_NOT_INSTALLED;
    else if (s.missing == 1) s.state = SHELL_INCOMPLETE;
    else if (s.updates > 0)  s.state = SHELL_UPDATE_AVAILABLE;
    else                     s.state = SHELL_OK;
    return s;
}

// What the big button on Home does. Everything except CTA_TEST_STRIP is "do the pending work".
typedef enum {
    CTA_TEST_STRIP = 0,
    CTA_INSTALL_PLUGIN,
    CTA_UPDATE_PLUGIN,
    CTA_ENABLE_PLUGIN,
    CTA_INSTALL_SHELL,
    CTA_REPAIR_SHELL,
    CTA_UPDATE_SHELL,
    CTA_INSTALL_ALL,
    CTA_UPDATE_ALL
} HomeCta;

typedef struct {
    HomeCta cta;
    int  pending;        // files that will be touched (plugin counts 1, even for "enable")
    bool anyMissing;     // something is not installed at all (decides "Install" vs "Update" wording)
    bool pluginAction;
    bool shellAction;
    bool testAvailable;  // Test strip may be offered: nothing at all is pending
} HomePlan;

static inline bool home_plan_equal(const HomePlan *a, const HomePlan *b)
{
    return a->cta == b->cta && a->pending == b->pending && a->anyMissing == b->anyMissing &&
           a->pluginAction == b->pluginAction && a->shellAction == b->shellAction &&
           a->testAvailable == b->testAvailable;
}

static inline bool shell_summary_equal(const ShellSummary *a, const ShellSummary *b)
{
    return a->state == b->state && a->missing == b->missing && a->updates == b->updates &&
           a->missingMask == b->missingMask && a->updateMask == b->updateMask;
}

static inline HomePlan home_plan(UiInstallState plugin, ShellSummary shell)
{
    HomePlan p = { CTA_TEST_STRIP, 0, false, false, false, false };

    if (plugin != UI_INSTALL_OK) {
        p.pluginAction = true;
        p.pending += 1;
        if (plugin == UI_INSTALL_NONE) p.anyMissing = true;
    }
    if (shell.state != SHELL_OFF && shell.state != SHELL_OK) {
        p.shellAction = true;
        p.pending += shell.missing + shell.updates;
        if (shell.missing > 0) p.anyMissing = true;
    }

    if (p.pending == 0) {
        p.testAvailable = true;
        return p;
    }

    if (p.pluginAction && p.shellAction) {
        p.cta = p.anyMissing ? CTA_INSTALL_ALL : CTA_UPDATE_ALL;
    } else if (p.pluginAction) {
        p.cta = (plugin == UI_INSTALL_NONE)     ? CTA_INSTALL_PLUGIN
              : (plugin == UI_INSTALL_DISABLED) ? CTA_ENABLE_PLUGIN
              :                                   CTA_UPDATE_PLUGIN;
    } else {
        p.cta = (shell.state == SHELL_NOT_INSTALLED) ? CTA_INSTALL_SHELL
              : (shell.state == SHELL_INCOMPLETE)    ? CTA_REPAIR_SHELL
              :                                        CTA_UPDATE_SHELL;
    }
    return p;
}

#endif // INSTALL_PLAN_H
