// Run from companion-app/: gcc -Wall -Wextra -Iinclude -o /tmp/test_install_plan tests/test_install_plan.c && /tmp/test_install_plan
// Every combination of plugin state x shell state, and what Home's button and Test strip do.
#include "install_plan.h"
#include <assert.h>
#include <stdio.h>

static HomePlan plan(UiInstallState pl, bool shellOn, AssetStatus prx, AssetStatus bin)
{
    return home_plan(pl, shell_summarize(shellOn, prx, bin), LOADER_UNKNOWN);
}

int main(void)
{
    // ---- Shell off: exactly the old behaviour, whatever the files look like ----
    for (int a = ASSET_MISSING; a <= ASSET_UPDATE; a++)
        for (int b = ASSET_MISSING; b <= ASSET_UPDATE; b++) {
            ShellSummary s = shell_summarize(false, (AssetStatus)a, (AssetStatus)b);
            assert(s.state == SHELL_OFF && s.missing == 0 && s.updates == 0);
            assert(plan(UI_INSTALL_OK, false, (AssetStatus)a, (AssetStatus)b).testAvailable);
            assert(plan(UI_INSTALL_OK, false, (AssetStatus)a, (AssetStatus)b).cta == CTA_TEST_STRIP);
        }
    assert(plan(UI_INSTALL_NONE,     false, ASSET_MISSING, ASSET_MISSING).cta == CTA_INSTALL_PLUGIN);
    assert(plan(UI_INSTALL_UPDATE,   false, ASSET_MISSING, ASSET_MISSING).cta == CTA_UPDATE_PLUGIN);
    assert(plan(UI_INSTALL_DISABLED, false, ASSET_MISSING, ASSET_MISSING).cta == CTA_ENABLE_PLUGIN);
    assert(!plan(UI_INSTALL_NONE,    false, ASSET_PRESENT, ASSET_PRESENT).testAvailable);
    printf("shell off == old behaviour: ok\n");

    // ---- Shell summary ----
    ShellSummary s = shell_summarize(true, ASSET_MISSING, ASSET_MISSING);
    assert(s.state == SHELL_NOT_INSTALLED && s.missing == 2 && s.missingMask == (SHELL_BIT_PRX | SHELL_BIT_PAYLOAD));
    s = shell_summarize(true, ASSET_MISSING, ASSET_PRESENT);
    assert(s.state == SHELL_INCOMPLETE && s.missing == 1 && s.missingMask == SHELL_BIT_PRX);
    s = shell_summarize(true, ASSET_PRESENT, ASSET_MISSING);
    assert(s.state == SHELL_INCOMPLETE && s.missingMask == SHELL_BIT_PAYLOAD);
    s = shell_summarize(true, ASSET_UPDATE, ASSET_PRESENT);
    assert(s.state == SHELL_UPDATE_AVAILABLE && s.updates == 1 && s.updateMask == SHELL_BIT_PRX);
    s = shell_summarize(true, ASSET_UPDATE, ASSET_UPDATE);
    assert(s.state == SHELL_UPDATE_AVAILABLE && s.updates == 2);
    s = shell_summarize(true, ASSET_UPDATE, ASSET_MISSING); // a missing file outranks an update
    assert(s.state == SHELL_INCOMPLETE && s.missing == 1 && s.updates == 1);
    s = shell_summarize(true, ASSET_PRESENT, ASSET_PRESENT);
    assert(s.state == SHELL_OK && s.missing == 0 && s.updates == 0);
    printf("shell summary: ok\n");

    // ---- Test strip only when NOTHING is pending ----
    assert(plan(UI_INSTALL_OK, true, ASSET_PRESENT, ASSET_PRESENT).testAvailable);
    assert(plan(UI_INSTALL_OK, true, ASSET_PRESENT, ASSET_PRESENT).cta == CTA_TEST_STRIP);
    assert(!plan(UI_INSTALL_OK, true, ASSET_MISSING, ASSET_PRESENT).testAvailable);   // folder exists but empty / one file gone
    assert(!plan(UI_INSTALL_OK, true, ASSET_PRESENT, ASSET_MISSING).testAvailable);
    assert(!plan(UI_INSTALL_OK, true, ASSET_MISSING, ASSET_MISSING).testAvailable);
    assert(!plan(UI_INSTALL_OK, true, ASSET_UPDATE,  ASSET_PRESENT).testAvailable);   // same rule the plugin's own update has
    assert(!plan(UI_INSTALL_NONE, true, ASSET_PRESENT, ASSET_PRESENT).testAvailable);
    printf("test strip gating: ok\n");

    // ---- Button wording ----
    HomePlan p = plan(UI_INSTALL_OK, true, ASSET_MISSING, ASSET_MISSING);
    assert(p.cta == CTA_INSTALL_SHELL && p.pending == 2 && p.shellAction && !p.pluginAction && p.anyMissing);
    p = plan(UI_INSTALL_OK, true, ASSET_PRESENT, ASSET_MISSING);
    assert(p.cta == CTA_REPAIR_SHELL && p.pending == 1);
    p = plan(UI_INSTALL_OK, true, ASSET_UPDATE, ASSET_UPDATE);
    assert(p.cta == CTA_UPDATE_SHELL && p.pending == 2 && !p.anyMissing);
    // more than one thing pending -> one combined button
    p = plan(UI_INSTALL_NONE, true, ASSET_MISSING, ASSET_MISSING);
    assert(p.cta == CTA_INSTALL_ALL && p.pending == 3);
    p = plan(UI_INSTALL_UPDATE, true, ASSET_UPDATE, ASSET_PRESENT);
    assert(p.cta == CTA_UPDATE_ALL && p.pending == 2 && !p.anyMissing);
    p = plan(UI_INSTALL_UPDATE, true, ASSET_MISSING, ASSET_PRESENT);
    assert(p.cta == CTA_INSTALL_ALL && p.pending == 2 && p.anyMissing);
    p = plan(UI_INSTALL_DISABLED, true, ASSET_UPDATE, ASSET_PRESENT);
    assert(p.cta == CTA_UPDATE_ALL && p.pending == 2);
    p = plan(UI_INSTALL_UPDATE, true, ASSET_PRESENT, ASSET_PRESENT);
    assert(p.cta == CTA_UPDATE_PLUGIN && p.pending == 1 && p.pluginAction && !p.shellAction);
    printf("button wording: ok\n");

    // ---- Exhaustive: pending == 0 <=> test strip, and pending never 0 with an action ----
    for (int pl = 0; pl <= UI_INSTALL_OK; pl++)
        for (int on = 0; on <= 1; on++)
            for (int a = 0; a <= 2; a++)
                for (int b = 0; b <= 2; b++) {
                    HomePlan q = plan((UiInstallState)pl, on, (AssetStatus)a, (AssetStatus)b);
                    assert((q.pending == 0) == q.testAvailable);
                    assert((q.cta == CTA_TEST_STRIP) == q.testAvailable);
                    assert(q.pending <= 3);
                }
    printf("exhaustive: ok\n");

    // ---- GoldHEN's plugin loader: a definite OFF outranks every other state; UNKNOWN is the same as ON ----
    for (int pl = 0; pl <= UI_INSTALL_OK; pl++)
        for (int on = 0; on <= 1; on++)
            for (int a = 0; a <= 2; a++)
                for (int b = 0; b <= 2; b++) {
                    ShellSummary sh = shell_summarize(on, (AssetStatus)a, (AssetStatus)b);
                    HomePlan off = home_plan((UiInstallState)pl, sh, LOADER_OFF);
                    assert(off.cta == CTA_LOADER_OFF && off.loaderAction && !off.testAvailable);
                    assert(off.pending == 1 && !off.pluginAction && !off.shellAction);
                    HomePlan un = home_plan((UiInstallState)pl, sh, LOADER_UNKNOWN);
                    HomePlan yes = home_plan((UiInstallState)pl, sh, LOADER_ON);
                    assert(home_plan_equal(&un, &yes) && !un.loaderAction);   // never blocks on a guess
                    assert(!home_plan_equal(&off, &yes));
                }
    {   // Everything fine except the loader: no Test strip until it is on.
        HomePlan off = home_plan(UI_INSTALL_OK, shell_summarize(false, ASSET_PRESENT, ASSET_PRESENT), LOADER_OFF);
        assert(off.cta == CTA_LOADER_OFF && !off.testAvailable);
        HomePlan on  = home_plan(UI_INSTALL_OK, shell_summarize(false, ASSET_PRESENT, ASSET_PRESENT), LOADER_ON);
        assert(on.cta == CTA_TEST_STRIP && on.testAvailable);
        // Plugin missing AND loader off: the loader notice first, then the install button once it is on.
        assert(home_plan(UI_INSTALL_NONE, shell_summarize(false, ASSET_PRESENT, ASSET_PRESENT), LOADER_OFF).cta == CTA_LOADER_OFF);
        assert(home_plan(UI_INSTALL_NONE, shell_summarize(false, ASSET_PRESENT, ASSET_PRESENT), LOADER_ON).cta == CTA_INSTALL_PLUGIN);
    }
    printf("plugin loader outranks everything: ok\nALL PASSED\n");
    return 0;
}
