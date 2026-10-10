// Run from companion-app/. Build:
//   gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -o /tmp/test_action_buttons \
//       tests/test_action_buttons.c source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c
//
// Every FIELD_ACTION button has to say what it does. The payload button once read "Reset Game"
// because the drawing code gave every action the preset reset's text; this keeps that from
// coming back, and keeps the "Ambilight payload" name (not "loader" / "launcher") in the menu.
#include "settings.h"
#include "presets.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int icontains(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    for (; hay && *hay; hay++) {
        size_t i = 0;
        while (i < n && hay[i] && tolower((unsigned char)hay[i]) == needle[i]) i++;
        if (i == n) return 1;
    }
    return 0;
}

int main(void)
{
    char t[64];
    int actions = 0, sawPayload = 0, sawReset = 0;

    for (int i = 0; i < kMenuItemCount; i++) {
        const MenuItem *it = &kMenuItems[i];
        if (it->type != FIELD_ACTION) continue;
        actions++;
        for (int armed = 0; armed < 2; armed++) {
            for (int p = 0; p < PRESET_COUNT; p++) {
                // Every action in the menu must have its own text, short enough for the UI's
                // 40 byte button buffer. A new action with no entry fails here.
                assert(settings_action_text(it, armed != 0, p, t, sizeof(t)));
                assert(t[0] != 0 && strlen(t) < 40);
                if (strcmp(it->key, "preset_reset") != 0)
                    assert(strncmp(t, "Reset", 5) != 0);   // only the preset reset says Reset
            }
        }
        if (!strcmp(it->key, "shell_run_now")) {
            sawPayload = 1;
            for (int armed = 0; armed < 2; armed++)
                for (int p = 0; p < PRESET_COUNT; p++) {
                    assert(settings_action_text(it, armed != 0, p, t, sizeof(t)));
                    assert(!strcmp(t, "Launch Ambilight payload"));   // never depends on the preset or armed state
                }
            assert(!strcmp(it->label, "Ambilight payload"));
        }
        if (!strcmp(it->key, "preset_reset")) {
            sawReset = 1;
            assert(settings_action_text(it, false, PRESET_GAME,  t, sizeof(t)) && !strcmp(t, "Reset Game"));
            assert(settings_action_text(it, false, PRESET_MOVIE, t, sizeof(t)) && !strcmp(t, "Reset Movie"));
            assert(settings_action_text(it, true,  PRESET_GAME,  t, sizeof(t)) && !strcmp(t, "Press again to confirm"));
            assert(settings_action_text(it, true,  PRESET_MOVIE, t, sizeof(t)) && !strcmp(t, "Press again to confirm"));
        }
    }
    assert(actions == 2 && sawPayload && sawReset);   // adding a third action means adding it here too
    printf("every action button has its own text: PASSED (%d buttons)\n", actions);

    // An action key the function doesn't know is refused (the UI then shows the row's label),
    // and non-actions are refused too.
    MenuItem fake = { "Something new", "ui", "something_new", FIELD_ACTION };
    assert(!settings_action_text(&fake, false, PRESET_GAME, t, sizeof(t)));
    for (int i = 0; i < kMenuItemCount; i++)
        if (kMenuItems[i].type != FIELD_ACTION) assert(!settings_action_text(&kMenuItems[i], false, PRESET_GAME, t, sizeof(t)));
    assert(!settings_action_text(NULL, false, PRESET_GAME, t, sizeof(t)));
    // A short buffer truncates, it doesn't overflow.
    char small[8];
    for (int i = 0; i < kMenuItemCount; i++)
        if (kMenuItems[i].type == FIELD_ACTION) {
            assert(settings_action_text(&kMenuItems[i], false, PRESET_GAME, small, sizeof(small)));
            assert(strlen(small) < sizeof(small));
        }
    printf("unknown actions and short buffers: PASSED\n");

    // The payload is "Ambilight payload" in everything the menu shows; "loader" / "launcher"
    // are GoldHEN's and don't belong in the app's own rows.
    for (int i = 0; i < kMenuItemCount; i++) {
        const MenuItem *it = &kMenuItems[i];
        assert(!icontains(it->label, "launcher") && !icontains(it->label, "loader"));
        assert(!it->groupDesc || (!icontains(it->groupDesc, "launcher") && !icontains(it->groupDesc, "loader")));
    }
    printf("menu text uses \"Ambilight payload\": PASSED\n");

    printf("ALL ACTION BUTTON CHECKS PASSED\n");
    return 0;
}
