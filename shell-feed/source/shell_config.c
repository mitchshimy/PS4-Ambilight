// shell_config.c -- the ini as the shell feed reads it.
// More: docs/code-comments.md (shell-feed/source/shell_config.c:1)
#include "shell_internal.h"

static const char *g_cfgPreset = "defaults"; // which ini section supplied the preset-owned settings, for the log

int      g_cfgHaveBaseline = 0;
int64_t  g_cfgLastSize = 0;
uint32_t g_cfgLastHash = 0;
int      g_cfgLastReadOk = 1;
static uint64_t g_cfgNextPollTsc = 0;
uint32_t g_cfgReloads = 0;
volatile int g_shReportOn = 0; // [v24] [compat] report_file=1 (sister's rule: 0/1 only, else the value stands)
int      g_shellEnabled = 1;  // [v23] [shell] enabled=; default true; updated on every reload

// [v24] The three keys the shell reads that are not in the sister's config struct: [compat] report_file and
// [dev] dev_ip / dev_logging, with the sister's own rules (a missing or out-of-range key leaves the value as it was).

void sh_parse_aux(ini_table_s *table)
{
    const char *v; int iv; bool bv;
    if (ini_table_get_entry_as_int(table, "compat", "report_file", &iv) && (iv == 0 || iv == 1)) g_shReportOn = iv;
    if ((v = ini_table_get_entry(table, "dev", "dev_ip")) != NULL) {
        strncpy(g_shDevIp, v, sizeof(g_shDevIp) - 1);
        g_shDevIp[sizeof(g_shDevIp) - 1] = '\0';
    }
    if (ini_table_get_entry_as_bool(table, "dev", "dev_logging", &bv)) g_shDevLog = bv ? 1 : 0;
}

// Reads the ini into g_config.
// More: docs/code-comments.md (shell-feed/source/shell_config.c:32)
int shell_cfg_load(void)
{
    g_presetOverride = PRESET_SECTION_GAME; // wins over active=/media title inside ambient_load_config(); see its comment
    ambient_load_config();
    g_cfgPreset = PRESET_SECTION_GAME; // ambient_load_config() doesn't report back which section it used; this is what we forced

    // [compat] report_file and [dev] dev_ip/dev_logging: shell-only copies...
    // More: docs/code-comments.md (shell-feed/source/shell_config.c:47)
    ini_table_s *table = ini_table_create();
    if (table == NULL || !ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) return 0; // config read via ambient_load_config() above still stands; only these three fall back
    sh_parse_aux(table); // [v24]
    bool bv;
    g_shellEnabled = ini_table_get_entry_as_bool(table, "shell", "enabled", &bv) ? (bv ? 1 : 0) : 1; // absent = enabled
    ini_table_destroy(table);
    return 1;
}

// One line with every value the shell is running on, printed after each (re)load and geometry rebuild, so a
// log shows exactly which ini the strip followed.
void shell_cfg_log(const char *why, int64_t bytes, uint32_t hash)
{
    printf(TAG " INI %s: bytes=%lld hash=%08x preset=%s | host=%s:%u zones=%u (T%u R%u B%u L%u) corner=%d dir=%d offset=%d order=%d |"
           " scan_depth=%u brightness=%u saturation=%d gamma_idx=%u contrast=%d black=%u white=%u dark=%u smoothing=%d settle_ms=%u hz=%u |"
           " bright_rgb=%u/%u/%u gamma_rgb=%u/%u/%u\n",
           why, (long long)bytes, (unsigned)hash, g_cfgPreset, g_config.wledHost, (unsigned)g_config.wledPort, (unsigned)g_numZones,
           (unsigned)g_config.ledCountTop, (unsigned)g_config.ledCountRight, (unsigned)g_config.ledCountBottom, (unsigned)g_config.ledCountLeft,
           (int)g_config.startCorner, (int)g_config.direction, (int)g_config.ledOffset, (int)g_config.colorOrder,
           (unsigned)g_config.scanDepth, (unsigned)g_config.brightness, (int)g_config.saturation, (unsigned)g_config.gammaLutIndex,
           (int)g_config.contrast, (unsigned)g_config.blackLevel, (unsigned)g_config.whiteLevel, (unsigned)g_config.darkThreshold,
           (int)g_config.smoothingEnabled, (unsigned)g_config.settlingTimeMs, (unsigned)g_config.updateFrequencyHz,
           (unsigned)g_config.brightnessR, (unsigned)g_config.brightnessG, (unsigned)g_config.brightnessB,
           (unsigned)g_config.gammaR, (unsigned)g_config.gammaG, (unsigned)g_config.gammaB);
}

// Live reload, the sister's rule: every configReloadCheckSeconds, read the file through the raw...
// More: docs/code-comments.md (shell-feed/source/shell_config.c:78)
int shell_cfg_poll(uint64_t nowTsc, uint64_t tscHz)
{
    if (g_config.configReloadCheckSeconds == 0 || tscHz == 0) return 0;
    if (nowTsc < g_cfgNextPollTsc) return 0;
    g_cfgNextPollTsc = nowTsc + tscHz * (uint64_t)g_config.configReloadCheckSeconds;

    int64_t sz = 0; uint32_t h = 0;
    if (!ambient_get_real_config_size_and_hash(&sz, &h)) {
        if (g_cfgLastReadOk) printf(TAG " INI unreadable (%lld): keeping the current settings\n", (long long)sz);
        g_cfgLastReadOk = 0;
        return 0;
    }
    if (!g_cfgLastReadOk) printf(TAG " INI readable again\n");
    g_cfgLastReadOk = 1;
    if (!g_cfgHaveBaseline) { g_cfgLastSize = sz; g_cfgLastHash = h; g_cfgHaveBaseline = 1; return 0; }
    if (sz == g_cfgLastSize && h == g_cfgLastHash) return 0;
    g_cfgLastSize = sz; g_cfgLastHash = h;

    if (!shell_cfg_load()) {
        printf(TAG " INI RELOAD: file changed (bytes=%lld hash=%08x) but could not be parsed; keeping the current settings\n",
               (long long)sz, (unsigned)h);
        return 0;
    }
    buildZoneGeometry();            // layout settings may have changed
    g_smoothedRgbValid = false;     // a hard cut between the old and new settings, not a blend
    g_cfgReloads++;
    shell_cfg_log("RELOAD", sz, h);
    return 1;
}

// [shell] enabled= is polled on its own 1 s cadence, independent of config_reload_check_seconds (0 disabled it);
// it deliberately does not reload the rest of the ini. [docs/code-comments.md]
#define ENABLED_POLL_MS 1000u

void shell_enabled_poll(uint64_t nowTsc, uint64_t tscHz, uint64_t *nextPollTsc)
{
    if (tscHz == 0 || nowTsc < *nextPollTsc) return;
    *nextPollTsc = nowTsc + tscHz * ENABLED_POLL_MS / 1000u;

    ini_table_s *table = ini_table_create();
    if (table == NULL || !ini_table_read_from_file(table, AMBIENT_CONFIG_PATH)) return; // unreadable: leave g_shellEnabled as it was, same rule as shell_cfg_load()

    int was = g_shellEnabled;
    bool bv;
    g_shellEnabled = ini_table_get_entry_as_bool(table, "shell", "enabled", &bv) ? (bv ? 1 : 0) : 1; // absent = enabled
    ini_table_destroy(table);

    if (g_shellEnabled != was)
        printf(TAG " SHELL ENABLED POLL: [shell] enabled= now %s (checked every %us, independent of config_reload_check_seconds)\n",
               g_shellEnabled ? "true" : "false", (unsigned)(ENABLED_POLL_MS / 1000u));
}
