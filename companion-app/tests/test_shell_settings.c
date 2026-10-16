// Run from companion-app/ (same flags as test_presets):
//   gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -o /tmp/test_shell_settings tests/test_shell_settings.c \
//       source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c && /tmp/test_shell_settings
// The ini side of the shell feed: [shell] enabled round-trips, other sections (and the new
// [dev] update_base_url) survive a save, and the autoload row never reaches the ini.
#include "settings.h"
#include "config.h"
#include "update_source.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void write_text(const char *path, const char *t) { FILE *f = fopen(path, "w"); assert(f); fputs(t, f); fclose(f); }
static char *slurp(const char *path) { FILE *f = fopen(path, "rb"); assert(f); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1); assert(fread(b, 1, n, f) == (size_t)n); b[n] = 0; fclose(f); return b; }

int main(void)
{
    const char *ini = "/tmp/test_shell_settings.ini";
    AmbientConfig cfg;

    // default: OFF, and nothing about the autoload row is "on"
    settings_set_defaults(&cfg);
    assert(cfg.shellEnabled == 0 && cfg.shellAutoload == 0);

    // load: key absent -> off (the app's default), true/false/1/0 honoured
    write_text(ini, "[wled]\nhost=10.0.0.5\n");
    memset(&cfg, 0, sizeof(cfg)); settings_set_defaults(&cfg);
    assert(settings_load(&cfg, ini) && cfg.shellEnabled == 0);
    write_text(ini, "[wled]\nhost=10.0.0.5\n[shell]\nenabled=true\n");
    settings_set_defaults(&cfg); assert(settings_load(&cfg, ini) && cfg.shellEnabled == 1);
    write_text(ini, "[shell]\nenabled=false\n");
    settings_set_defaults(&cfg); cfg.shellEnabled = 1; assert(settings_load(&cfg, ini) && cfg.shellEnabled == 0);
    write_text(ini, "[shell]\nenabled=1\n");
    settings_set_defaults(&cfg); assert(settings_load(&cfg, ini) && cfg.shellEnabled == 1);
    printf("load: ok\n");

    // save: written explicitly both ways (the shell feed reads a missing key as ON)
    write_text(ini, "[wled]\nhost=10.0.0.5\n");
    settings_set_defaults(&cfg); settings_load(&cfg, ini);
    cfg.shellEnabled = 0; cfg.shellAutoload = 1;                 // autoload must NOT leak into the file
    assert(settings_save(&cfg, ini));
    char *t = slurp(ini);
    assert(!strstr(t, "autoload") && !strstr(t, "[ui]"));
    free(t);
    ini_table_s *chk = ini_table_create(); assert(ini_table_read_from_file(chk, ini));
    assert(!strcmp(ini_table_get_entry(chk, "shell", "enabled"), "false"));
    ini_table_destroy(chk);
    cfg.shellEnabled = 1; assert(settings_save(&cfg, ini));
    chk = ini_table_create(); assert(ini_table_read_from_file(chk, ini));
    assert(!strcmp(ini_table_get_entry(chk, "shell", "enabled"), "true"));
    ini_table_destroy(chk);
    settings_set_defaults(&cfg); assert(settings_load(&cfg, ini) && cfg.shellEnabled == 1);
    printf("save/round trip: ok\n");

    // other sections survive a save: the plugin's [dev]/[compat], the dev update source, and an
    // unknown key inside [shell] itself
    write_text(ini, "[wled]\nhost=10.0.0.5\n[dev]\ndev_ip=192.168.2.117\ndev_logging=true\nupdate_base_url=http://192.168.2.117:8000\n"
                    "[compat]\nreport_file=1\n[shell]\nenabled=true\nfuture_key=42\n");
    settings_set_defaults(&cfg); assert(settings_load(&cfg, ini) && cfg.shellEnabled == 1);
    cfg.shellEnabled = 0;
    assert(settings_save(&cfg, ini));
    ini_table_s *tab = ini_table_create(); assert(ini_table_read_from_file(tab, ini));
    assert(!strcmp(ini_table_get_entry(tab, "dev", "update_base_url"), "http://192.168.2.117:8000"));
    assert(!strcmp(ini_table_get_entry(tab, "dev", "dev_ip"), "192.168.2.117"));
    assert(!strcmp(ini_table_get_entry(tab, "dev", "dev_logging"), "true"));
    assert(!strcmp(ini_table_get_entry(tab, "compat", "report_file"), "1"));
    assert(!strcmp(ini_table_get_entry(tab, "shell", "future_key"), "42"));
    assert(!strcmp(ini_table_get_entry(tab, "shell", "enabled"), "false"));
    ini_table_destroy(tab);
    printf("other sections preserved: ok\n");

    // Set up rows: the two new rows exist, in the right place, on the right screen
    int iShell = -1, iAuto = -1, iLayoutLast = -1, iLive = -1;
    for (int i = 0; i < kMenuItemCount; i++) {
        if (!strcmp(kMenuItems[i].section, "shell") && !strcmp(kMenuItems[i].key, "enabled")) iShell = i;
        if (!strcmp(kMenuItems[i].key, "shell_autoload")) iAuto = i;
        if (!strcmp(kMenuItems[i].key, "color_order")) iLayoutLast = i;
        if (!strcmp(kMenuItems[i].key, "config_reload_check_seconds")) iLive = i;
    }
    assert(iShell >= 0 && iAuto == iShell + 1 && iLayoutLast >= 0 && iLive >= 0);
    assert(iLayoutLast < iShell && iAuto < iLive);                       // after LED strip layout, before Live reload
    assert(kMenuItems[iShell].screen == MENU_SCREEN_SETUP && kMenuItems[iAuto].screen == MENU_SCREEN_SETUP);
    assert(kMenuItems[iShell].type == FIELD_BOOL && kMenuItems[iAuto].type == FIELD_BOOL);
    assert(!strcmp(kMenuItems[iAuto].section, "ui"));                    // never an ini key
    assert(!strcmp(kMenuItems[iShell].group, kMenuItems[iAuto].group));  // one card
    printf("menu rows: ok\n");

    // [notifications] enabled: on by default, absent means on, a save only writes it once it is off
    // or the file already has it, and the Set up row sits right after the Shell feed card.
    {
        settings_set_defaults(&cfg);
        assert(cfg.notificationsEnabled == 1);
        write_text(ini, "[network]\nwled_host=10.0.0.5\n");
        settings_set_defaults(&cfg); assert(settings_load(&cfg, ini) && cfg.notificationsEnabled == 1);
        assert(settings_save(&cfg, ini));
        char *n1 = slurp(ini); assert(!strstr(n1, "[notifications]")); free(n1);       // on + absent: not introduced
        cfg.notificationsEnabled = 0; assert(settings_save(&cfg, ini));
        n1 = slurp(ini); assert(strstr(n1, "[notifications]")); free(n1);               // off: written
        settings_set_defaults(&cfg); assert(settings_load(&cfg, ini) && cfg.notificationsEnabled == 0);
        cfg.notificationsEnabled = 1; assert(settings_save(&cfg, ini));                 // back on: replaces the false
        settings_set_defaults(&cfg); cfg.notificationsEnabled = 0; assert(settings_load(&cfg, ini) && cfg.notificationsEnabled == 1);
        int iN = -1, iAutoN = -1, iRun = -1, iLiveN = -1;
        for (int i = 0; i < kMenuItemCount; i++) {
            if (!strcmp(kMenuItems[i].section, "notifications") && !strcmp(kMenuItems[i].key, "enabled")) iN = i;
            if (!strcmp(kMenuItems[i].key, "shell_run_now")) iRun = i;
            if (!strcmp(kMenuItems[i].key, "shell_autoload")) iAutoN = i;
            if (!strcmp(kMenuItems[i].key, "config_reload_check_seconds")) iLiveN = i;
        }
        assert(iN >= 0 && iRun >= 0 && iAutoN >= 0 && iLiveN >= 0);
        assert(iN > iRun && iN > iAutoN && iN < iLiveN);                                // below Shell feed, own card
        assert(kMenuItems[iN].screen == MENU_SCREEN_SETUP && kMenuItems[iN].type == FIELD_BOOL);
        assert(strcmp(kMenuItems[iN].group, kMenuItems[iRun].group) != 0);
        printf("notifications: ok\n");
    }

    // [dev] update_base_url cleanup
    char out[64];
    assert(update_source_normalize("http://192.168.2.117:8000", out, sizeof(out)) && !strcmp(out, "http://192.168.2.117:8000"));
    assert(update_source_normalize("  http://192.168.2.117:8000/  \r\n", out, sizeof(out)) && !strcmp(out, "http://192.168.2.117:8000"));
    assert(update_source_normalize("https://example.org/dl//", out, sizeof(out)) && !strcmp(out, "https://example.org/dl"));
    assert(!update_source_normalize("", out, sizeof(out)) && out[0] == 0);
    assert(!update_source_normalize(NULL, out, sizeof(out)));
    assert(!update_source_normalize("192.168.2.117:8000", out, sizeof(out)));          // no scheme
    assert(!update_source_normalize("ftp://192.168.2.117", out, sizeof(out)));
    assert(!update_source_normalize("http://", out, sizeof(out)));
    assert(!update_source_normalize("http://a.example.org/a/very/long/path/that/does/not/fit/in/the/buffer", out, 16));
    printf("update source: ok\nALL PASSED\n");
    return 0;
}
