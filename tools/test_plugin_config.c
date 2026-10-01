// Host test for the plugin's ini loading -- the real ambient_load_config()
// from plugin/source/settings.c, run against sample inis on a PC. Only the
// hardware-facing calls are stand-ins (below).
//
// Build and run from the repo root (one line):
//   gcc -Wall -Wl,--wrap=stat -Itools/host_stubs -Iplugin/include -Icommon -o /tmp/test_plugin_config tools/test_plugin_config.c plugin/source/settings.c plugin/source/config.c && /tmp/test_plugin_config
#include "plugin_common.h"
#include "ambient_internal.h"
#include "media_titles.h"
#include "default_ini.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// ---- stand-ins for what the console provides ----
#define TMP_INI "/tmp/test_plugin_config.ini"
static const char *remap(const char *p) { return strcmp(p, AMBIENT_CONFIG_PATH) == 0 ? TMP_INI : p; }
int32_t sceKernelOpen(const char *path, int flags, int mode) {
    return open(remap(path), flags == 0 ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC), mode);
}
int64_t sceKernelLseek(int32_t fd, int64_t off, int wh) { return lseek(fd, off, wh); }
ssize_t sceKernelRead(int32_t fd, void *b, size_t n)    { return read(fd, b, n); }
ssize_t sceKernelWrite(int32_t fd, const void *b, size_t n) { return write(fd, b, n); }
int sceKernelClose(int32_t fd) { return close(fd); }
int __real_stat(const char *p, struct stat *b);
int __wrap_stat(const char *p, struct stat *b) { return __real_stat(remap(p), b); }
void klog(const char *fmt, ...) { (void)fmt; }
static int g_lutRebuilds;
void ambient_rebuild_perchannel_gamma_luts(void) { g_lutRebuilds++; }
void ambient_read_content_preview(uint8_t *o, size_t n) { memset(o, 0, n); }
void buildZoneGeometry(void) {}
bool g_isBackgrounded; bool g_smoothedRgbValid;
volatile uint32_t g_gpuOnlyRemap = 3;   // lives in buffer_guard.c (v3.9), not linked here; same default
volatile uint32_t g_reportEnabled = 0;  // lives in report.c (v3.9.1), not linked here; same default (off)
void relay_send_external_source(bool on) { (void)on; }
void send_config_reload_debug_packet(uint32_t event, uint32_t statErrno, uint64_t curMtime, uint64_t lastMtime,
                                     uint64_t curSize, uint64_t lastSize, uint32_t checkCount,
                                     const uint8_t *contentPreview, uint32_t curHash, uint32_t lastHash)
{ (void)event;(void)statErrno;(void)curMtime;(void)lastMtime;(void)curSize;(void)lastSize;
  (void)checkCount;(void)contentPreview;(void)curHash;(void)lastHash; }

static void write_ini(const char *text) { FILE *f = fopen(TMP_INI, "w"); assert(f); fputs(text, f); fclose(f); }

// The state ambient_load_config() starts a plugin run with.
static AmbientConfig g_pristine;
static void fresh(const char *ini, const char *title)
{
    g_config = g_pristine;
    strncpy(g_titleId, title, sizeof(g_titleId) - 1);
    write_ini(ini);
    ambient_load_config();
}

#define PRESET_BODY_GAME  "scan_depth=2\nbrightness=255\nsaturation=35\ngamma=2.2\nsmoothing_enabled=true\nsettling_time_ms=50\n"
#define PRESET_BODY_MOVIE "scan_depth=4\nbrightness=204\nsaturation=25\ngamma=2.4\nblack_level=1\nsmoothing_enabled=true\nsettling_time_ms=200\n"

int main(void)
{
    g_pristine = g_config;

    // 1. The ini the plugin itself writes on first run: Game, with Setup values from their own sections.
    fresh(AMBIENT_DEFAULT_INI, "CUSA00001");
    assert(g_config.scanDepth == 2 && g_config.brightness == 255 && g_config.saturation == 35);
    assert(g_config.gammaLutIndex == 4);                       // 2.2
    assert(g_config.smoothingEnabled == 1 && g_config.settlingTimeMs == 50);
    assert(g_config.blackLevel == 0 && g_config.whiteLevel == 100 && g_config.contrast == 0);
    assert(g_config.brightnessR == 100 && g_config.gammaB == 100);
    assert(g_config.colorOrder == ORDER_RGB && g_config.updateFrequencyHz == 30 && g_config.configReloadCheckSeconds == 2);
    assert(g_config.ledCountTop == 73 && g_config.startCorner == CORNER_BOTTOM_LEFT);
    printf("generated default ini reads as Game: PASSED\n");

    // 2. active=movie picks the Movie section.
    {
        char ini[16384];
        const char *p = strstr(AMBIENT_DEFAULT_INI, "active=game");
        assert(p);
        snprintf(ini, sizeof(ini), "%.*sactive=movie%s", (int)(p - AMBIENT_DEFAULT_INI), AMBIENT_DEFAULT_INI, p + strlen("active=game"));
        fresh(ini, "CUSA00001");
        assert(g_config.scanDepth == 4 && g_config.brightness == 204 && g_config.saturation == 25);
        assert(g_config.gammaLutIndex == 5 && g_config.blackLevel == 1);
        assert(g_config.smoothingEnabled == 1 && g_config.settlingTimeMs == 200);
        assert(g_config.updateFrequencyHz == 30);                    // Setup value unaffected
    }
    printf("active=movie reads the Movie section: PASSED\n");

    // 3. A listed media title runs Movie even though active=game.
    static const char *const kMustBeMovie[] = { "CUSA00129", "CUSA00127", "CUSA02988",   // Netflix US, EU, JP
                                                "CUSA01015", "CUSA01116",                // YouTube US, EU
                                                "CUSA01065", "CUSA01034" };              // YouTube JP, Asia
    for (unsigned i = 0; i < sizeof(kMustBeMovie) / sizeof(kMustBeMovie[0]); i++) {
        fresh(AMBIENT_DEFAULT_INI, kMustBeMovie[i]);
        assert(g_config.scanDepth == 4 && g_config.settlingTimeMs == 200 && g_config.brightness == 204);
    }
    // Games must never match. CUSA05682 is Horizon Zero Dawn and shows up in some lists as a YouTube ID.
    static const char *const kMustBeGame[] = { "CUSA05682", "CUSA00001", "CUSA18680", "SHMY00091", "" };
    for (unsigned i = 0; i < sizeof(kMustBeGame) / sizeof(kMustBeGame[0]); i++) {
        fresh(AMBIENT_DEFAULT_INI, kMustBeGame[i]);
        assert(g_config.scanDepth == 2 && g_config.settlingTimeMs == 50);
    }
    // The list is exactly the IDs above. A new ID has to be added in both places, on purpose.
    {
        int n = 0; while (kMovieTitleIds[n] != NULL) n++;
        assert(n == (int)(sizeof(kMustBeMovie) / sizeof(kMustBeMovie[0])));
    }
    // No ID appears twice, and every one has the shape GoldHEN reports.
    for (int i = 0; kMovieTitleIds[i] != NULL; i++) {
        assert(strlen(kMovieTitleIds[i]) == 9 && strncmp(kMovieTitleIds[i], "CUSA", 4) == 0);
        for (int j = i + 1; kMovieTitleIds[j] != NULL; j++) assert(strcmp(kMovieTitleIds[i], kMovieTitleIds[j]) != 0);
    }
    printf("media titles force Movie, games and unknowns don't: PASSED\n");

    // 4. An ini from before presets: flat keys, read as they always were.
    fresh("[color]\nbrightness=180\nsaturation=-20\ngamma=2.4\ncolor_order=GRB\n"
          "[layout]\nscan_depth=3\nauto_letterbox_enabled=false\n"
          "[timing]\nsmoothing_enabled=true\nsettling_time_ms=120\nupdate_frequency_hz=60\n", "CUSA00001");
    assert(g_config.brightness == 180 && g_config.saturation == -20 && g_config.gammaLutIndex == 5);
    assert(g_config.scanDepth == 3 && g_config.autoLetterboxEnabled == false);
    assert(g_config.smoothingEnabled == 1 && g_config.settlingTimeMs == 120);
    assert(g_config.colorOrder == ORDER_GRB && g_config.updateFrequencyHz == 60);
    printf("pre-preset ini still reads as before: PASSED\n");

    // 5. Once a preset section exists the old flat preset keys are ignored, but Setup keys still count.
    fresh("[color]\nbrightness=10\ncolor_order=BGR\n[layout]\nscan_depth=1\n[timing]\nsettling_time_ms=999\nupdate_frequency_hz=45\n"
          "[presets]\nactive=game\n[preset_game]\n" PRESET_BODY_GAME, "CUSA00001");
    assert(g_config.brightness == 255 && g_config.scanDepth == 2 && g_config.settlingTimeMs == 50);
    assert(g_config.colorOrder == ORDER_BGR && g_config.updateFrequencyHz == 45);
    printf("preset sections win over leftover flat keys: PASSED\n");

    // 6. Wanted preset missing -> the other one; movie wanted, only game present.
    fresh("[presets]\nactive=movie\n[preset_game]\n" PRESET_BODY_GAME, "CUSA00001");
    assert(g_config.scanDepth == 2 && g_config.settlingTimeMs == 50);
    fresh("[presets]\nactive=game\n[preset_movie]\n" PRESET_BODY_MOVIE, "CUSA00001");
    assert(g_config.scanDepth == 4);
    printf("missing preset falls back to the other: PASSED\n");

    // 7. No [presets] at all but a preset section: defaults to game.
    fresh("[preset_game]\n" PRESET_BODY_GAME "[preset_movie]\n" PRESET_BODY_MOVIE, "CUSA00001");
    assert(g_config.scanDepth == 2);
    printf("no [presets] section defaults to game: PASSED\n");

    // 8. Live reload: same process, ini edited from game to movie.
    fresh("[presets]\nactive=game\n[preset_game]\n" PRESET_BODY_GAME "[preset_movie]\n" PRESET_BODY_MOVIE, "CUSA00001");
    assert(g_config.settlingTimeMs == 50);
    write_ini("[presets]\nactive=movie\n[preset_game]\n" PRESET_BODY_GAME "[preset_movie]\n" PRESET_BODY_MOVIE);
    ambient_load_config();
    assert(g_config.settlingTimeMs == 200 && g_config.scanDepth == 4 && g_config.brightness == 204);
    write_ini("[presets]\nactive=game\n[preset_game]\n" PRESET_BODY_GAME "[preset_movie]\n" PRESET_BODY_MOVIE);
    ambient_load_config();
    assert(g_config.settlingTimeMs == 50 && g_config.scanDepth == 2 && g_config.brightness == 255);
    printf("live reload switches preset both ways: PASSED\n");

    // 9. Bad values in a preset are rejected like bad flat keys always were.
    fresh("[preset_game]\nscan_depth=9\nbrightness=999\ngamma=3.3\nblack_level=150\nsaturation=900\n", "CUSA00001");
    assert(g_config.scanDepth == 1);                                  // rejected: default stands
    assert(g_config.brightness == 255 && g_config.gammaLutIndex == 0 && g_config.blackLevel == 0);
    assert(g_config.saturation == SATURATION_MAX);                    // clamped, as before
    printf("out-of-range preset values handled like flat keys: PASSED\n");

    // 10. A key missing from a preset keeps the value it already had.
    fresh("[preset_game]\nscan_depth=3\n", "CUSA00001");
    assert(g_config.scanDepth == 3 && g_config.settlingTimeMs == 200 && g_config.smoothingEnabled == 0);   // compiled defaults
    printf("missing key keeps existing value: PASSED\n");

    // 11. An empty [preset_movie] line is not a preset.
    fresh("[presets]\nactive=movie\n[preset_movie]\n[preset_game]\n" PRESET_BODY_GAME, "CUSA00001");
    assert(g_config.scanDepth == 2);
    printf("empty preset section ignored: PASSED\n");

    // 12. Per-channel gamma LUTs are rebuilt on every load.
    int before = g_lutRebuilds;
    fresh(AMBIENT_DEFAULT_INI, "CUSA00001");
    assert(g_lutRebuilds == before + 1);
    printf("gamma LUTs rebuilt after load: PASSED\n");

    // 13. The file the companion app actually writes (tools/data/preset_ini_golden.ini, kept in
    // step with the app by companion-app/tests/test_presets.c), read by the plugin's real loader.
    {
        FILE *gf = fopen("tools/data/preset_ini_golden.ini", "rb");
        assert(gf && "run from the repo root");
        char golden[8192]; size_t gn = fread(golden, 1, sizeof(golden) - 1, gf); golden[gn] = 0; fclose(gf);

        fresh(golden, "CUSA00001");                                  // any game: the active preset, Game
        assert(strcmp(g_config.wledHost, "192.168.2.110") == 0 && g_config.wledPort == 4048);
        assert(g_config.ledCountTop == 73 && g_config.ledCountRight == 41 && g_config.ledCountBottom == 73 && g_config.ledCountLeft == 42);
        assert(g_config.startCorner == CORNER_BOTTOM_LEFT && g_config.direction == DIR_CLOCKWISE && g_config.ledOffset == 0);
        assert(g_config.colorOrder == ORDER_RGB && g_config.updateFrequencyHz == 30 && g_config.configReloadCheckSeconds == 2);
        assert(g_config.scanDepth == 2 && g_config.autoLetterboxEnabled && g_config.autoLetterboxThreshold == 18);
        assert(g_config.brightness == 255 && g_config.saturation == 35 && g_config.gammaLutIndex == 4);
        assert(g_config.blackLevel == 0 && g_config.whiteLevel == 100 && g_config.contrast == 0 && g_config.darkThreshold == 0);
        assert(g_config.smoothingEnabled == 1 && g_config.settlingTimeMs == 50);
        assert(g_config.brightnessR == 100 && g_config.brightnessG == 100 && g_config.brightnessB == 100);
        assert(g_config.gammaR == 100 && g_config.gammaG == 100 && g_config.gammaB == 100);
        assert(g_config.autoLetterboxStabilityFrames == 3 && g_config.autoLetterboxCheckIntervalFrames == 15);   // omitted keys = defaults

        fresh(golden, "CUSA00129");                                  // Netflix US: Movie, same file
        assert(g_config.scanDepth == 4 && g_config.brightness == 204 && g_config.saturation == 25 && g_config.gammaLutIndex == 5);
        assert(g_config.blackLevel == 1 && g_config.smoothingEnabled == 1 && g_config.settlingTimeMs == 200);
        assert(strcmp(g_config.wledHost, "192.168.2.110") == 0 && g_config.ledCountTop == 73);   // Setup unchanged by the preset
    }
    printf("plugin reads the ini the app writes: PASSED\n");

    remove(TMP_INI);
    // v3.9: [compat] gpu_only_remap (buffer_guard.c). Optional: the shipped ini doesn't have it and the
    // default is 3, so nothing has to set it. It lives outside g_config, so it is reset by hand here.
    g_gpuOnlyRemap = 3;
    fresh(AMBIENT_DEFAULT_INI, "CUSA00001");
    assert(strstr(AMBIENT_DEFAULT_INI, "gpu_only_remap") == NULL);
    assert(g_gpuOnlyRemap == 3);
    for (int want = 0; want <= 3; want++) {
        char ini[16384];
        g_gpuOnlyRemap = 3;
        snprintf(ini, sizeof(ini), "%s\n[compat]\ngpu_only_remap=%d\n", AMBIENT_DEFAULT_INI, want);
        fresh(ini, "CUSA00001");
        assert(g_gpuOnlyRemap == (uint32_t)want);
        assert(g_config.scanDepth == 2 && g_config.brightness == 255);   // the preset still reads as before
    }
    g_gpuOnlyRemap = 2; fresh("[compat]\ngpu_only_remap=4\n", "CUSA00001"); assert(g_gpuOnlyRemap == 2);    // out of range: value stands
    g_gpuOnlyRemap = 2; fresh("[compat]\ngpu_only_remap=-1\n", "CUSA00001"); assert(g_gpuOnlyRemap == 2);
    // Text that is not a number reads as 0 through ini_table_get_entry_as_int, as for every int key here, so it turns the remap off.
    g_gpuOnlyRemap = 3; fresh("[compat]\ngpu_only_remap=abc\n", "CUSA00001"); assert(g_gpuOnlyRemap == 0);
    g_gpuOnlyRemap = 2; fresh("[color]\nbrightness=180\n", "CUSA00001"); assert(g_gpuOnlyRemap == 2);     // key absent: value stands
    g_gpuOnlyRemap = 3;
    printf("[compat] gpu_only_remap is optional, in range 0 to 3, and independent of presets: PASSED\n");

    // v3.9.1: [compat] report_file (report.c). Optional and OFF by default: a tester turns it on by adding
    // the key. Only 0 and 1 mean anything.
    g_reportEnabled = 0;
    fresh(AMBIENT_DEFAULT_INI, "CUSA00001");
    assert(strstr(AMBIENT_DEFAULT_INI, "report_file") == NULL);      // not in the shipped ini
    assert(g_reportEnabled == 0);                                    // so a normal install never writes the file
    g_reportEnabled = 0; fresh("[compat]\nreport_file=1\n", "CUSA00001"); assert(g_reportEnabled == 1);   // the tester's line
    g_reportEnabled = 1; fresh("[compat]\nreport_file=0\n", "CUSA00001"); assert(g_reportEnabled == 0);
    g_reportEnabled = 0; fresh("[compat]\nreport_file=2\n", "CUSA00001"); assert(g_reportEnabled == 0);    // out of range: value stands
    g_reportEnabled = 1; fresh("[compat]\nreport_file=-1\n", "CUSA00001"); assert(g_reportEnabled == 1);
    g_reportEnabled = 0; fresh("[color]\nbrightness=180\n", "CUSA00001"); assert(g_reportEnabled == 0);    // key absent: value stands
    g_reportEnabled = 1; fresh("[color]\nbrightness=180\n", "CUSA00001"); assert(g_reportEnabled == 1);
    {
        char ini[16384];
        g_reportEnabled = 0; g_gpuOnlyRemap = 2;
        snprintf(ini, sizeof(ini), "%s\n[compat]\nreport_file=1\n", AMBIENT_DEFAULT_INI);
        fresh(ini, "CUSA00001");
        assert(g_reportEnabled == 1 && g_gpuOnlyRemap == 2);         // neither [compat] key disturbs the other
        assert(g_config.scanDepth == 2 && g_config.brightness == 255);   // nor the preset
        g_reportEnabled = 0; g_gpuOnlyRemap = 3;
        snprintf(ini, sizeof(ini), "%s\n[compat]\ngpu_only_remap=1\nreport_file=1\n", AMBIENT_DEFAULT_INI);
        fresh(ini, "CUSA00001");
        assert(g_reportEnabled == 1 && g_gpuOnlyRemap == 1);         // both keys together, as a tester would have them
    }
    g_reportEnabled = 0; g_gpuOnlyRemap = 3;
    printf("[compat] report_file is optional, off by default, 0 or 1, and independent of gpu_only_remap and presets: PASSED\n");

    printf("ALL PLUGIN CONFIG CHECKS PASSED\n");
    return 0;
}
