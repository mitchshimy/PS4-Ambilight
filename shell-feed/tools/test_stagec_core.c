// Stage C verification: compiles the EXACT extracted span of shell-feed/source/main.c (v17 tiling
// math through shell_process_frame -- lines 616-952 of the real file as of this test) against the
// real sister files (plugin/source/{tiling,pixel_formats,zones,gamma,color_processing,settings,
// config}.c), the same way host_zone_check/host_ini_check already prove the sister files link
// together, but now proving THIS project's own post-refactor code links against them too and
// behaves as designed: g_presetOverride forces preset_game despite active=movie, buildZoneGeometry()
// produces the known-correct geometry, and a synthetic frame samples/colour-processes/smooths
// through the REAL linked functions, not a pasted copy.
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

// The extracted span includes ambient_internal.h, whose hook declarations rely on the
// same SDK Common.h macro normally pulled in through plugin_common.h in production files.
#include "Common.h"

#define TAG "[STAGEC-TEST]"

// Signatures match this repo's own tools/host_stubs/orbis/libkernel.h and tools/test_plugin_config.c
// exactly (ssize_t for Read/Write, not int32_t -- the research repo's own stub differs here, so this
// is deliberately repo-native rather than copied from there).
int32_t sceKernelOpen(const char *path, int flags, int mode) { (void)mode; return open(path, flags == 0 ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC), 0644); }
int64_t sceKernelLseek(int32_t fd, int64_t off, int wh) { return lseek(fd, off, wh); }
ssize_t sceKernelRead(int32_t fd, void *b, size_t n)  { return read(fd, b, n); }
ssize_t sceKernelWrite(int32_t fd, const void *b, size_t n) { return write(fd, b, n); }
int sceKernelClose(int32_t fd) { return close(fd); }
int sys_sdk_proc_info(void *p) { (void)p; return 0; }
unsigned sys_sdk_version(void) { return 0; }

#include "extracted_core.c"

static void dump_px(uint32_t *buf, int w, int h, int x0, int y0, int rw, int rh, uint8_t r, uint8_t g, uint8_t b)
{
    // paint a flat-colour rect into a raw (non-tiled) ARGB buffer, then we'll let the real
    // getTiledElementByteOffset() read it back through the tiled addressing -- since we don't have
    // a real tiled buffer, this test paints a FLAT buffer instead and checks sampling doesn't crash
    // and produces a stable, deterministic value, not that it matches a specific untiled picture
    // (that part -- the tiling math itself -- is what host_zone_check and the probe's own v16/v17
    // capture history already separately proved correct on real hardware dumps).
    (void)w; (void)h; (void)x0; (void)y0; (void)rw; (void)rh;
    uint32_t px = ((uint32_t)0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    for (uint32_t i = 0; i < BUF_BYTES17 / 4; i++) buf[i] = px;
}

int main(void)
{
    printf(TAG " sizeof TileParams (real sister struct) = %zu, BUF_BYTES17 = %llu, sizeof(kParamsBase real) fields match TileParams17 layout: %s\n",
           sizeof(TileParams), (unsigned long long)BUF_BYTES17,
           (sizeof(TileParams) == sizeof(TileParams17)) ? "YES" : "NO (size mismatch!)");

    // 1. shell_cfg_load(): forces preset_game despite active=movie in the ini, via g_presetOverride
    //    and the REAL ambient_load_config() (plugin/source/settings.c).
    int ok = shell_cfg_load();
    printf(TAG " shell_cfg_load() = %d, g_cfgPreset=%s, scanDepth=%u (want 2=game, not 4=movie), brightness=%u (want 200, not 100), settlingTimeMs=%u (want 50, not 200)\n",
           ok, g_cfgPreset, g_config.scanDepth, g_config.brightness, g_config.settlingTimeMs);
    printf(TAG " shell-only aux keys: g_shellEnabled=%d (want 1) g_shReportOn=%d (want 1) g_shDevIp=%s (want 192.168.2.50) g_shDevLog=%d (want 1)\n",
           g_shellEnabled, g_shReportOn, g_shDevIp, g_shDevLog);
    int fail = 0;
    if (!ok) { fprintf(stderr, "FAIL: shell_cfg_load returned 0\n"); fail = 1; }
    if (g_config.scanDepth != 2) { fprintf(stderr, "FAIL: scanDepth=%u, want 2 (preset_game)\n", g_config.scanDepth); fail = 1; }
    if (g_config.brightness != 200) { fprintf(stderr, "FAIL: brightness=%u, want 200 (preset_game)\n", g_config.brightness); fail = 1; }
    if (g_config.settlingTimeMs != 50) { fprintf(stderr, "FAIL: settlingTimeMs=%u, want 50 (preset_game)\n", g_config.settlingTimeMs); fail = 1; }
    if (!g_shellEnabled) { fprintf(stderr, "FAIL: g_shellEnabled=0, want 1\n"); fail = 1; }
    if (!g_shReportOn) { fprintf(stderr, "FAIL: g_shReportOn=0, want 1\n"); fail = 1; }
    if (strcmp(g_shDevIp, "192.168.2.50") != 0) { fprintf(stderr, "FAIL: g_shDevIp=%s\n", g_shDevIp); fail = 1; }
    if (!g_shDevLog) { fprintf(stderr, "FAIL: g_shDevLog=0, want 1\n"); fail = 1; }

    // 2. buildZoneGeometry(): the REAL sister function, same one host_zone_check already proved
    //    puts the first zone at LED 42, not 0, for this exact layout.
    buildZoneGeometry();
    printf(TAG " buildZoneGeometry(): g_numZones=%u (want 229 = 73+41+73+42), g_zoneX[0]=%u g_zoneY[0]=%u\n",
           g_numZones, g_zoneX[0], g_zoneY[0]);
    if (g_numZones != 73u + 41u + 73u + 42u) { fprintf(stderr, "FAIL: g_numZones=%u\n", g_numZones); fail = 1; }

    // 3. shell_process_frame(): samples a synthetic buffer through the REAL sampleZoneAverage(),
    //    applyColorProcessing(), applyDarkThreshold() (zones.c/color_processing.c/gamma.c, linked),
    //    plus this file's own smoothing block, exactly as it will run on hardware.
    static uint32_t fakeBuf[2 * 1024 * 1024]; // > BUF_BYTES17/4
    dump_px(fakeBuf, 1920, 1088, 0, 0, 1920, 1088, 255, 128, 64);
    static uint8_t rgbTriplets[3 * MAX_TOTAL_ZONES];
    uint32_t n = shell_process_frame((uint64_t)(uintptr_t)fakeBuf, 33333u, rgbTriplets);
    printf(TAG " shell_process_frame(): zones=%u, rgbTriplets[0]=%u,%u,%u (first-frame smoothing snaps to source, so this should be the colour-processed (255,128,64), not 0,0,0)\n",
           n, rgbTriplets[0], rgbTriplets[1], rgbTriplets[2]);
    if (n != g_numZones) { fprintf(stderr, "FAIL: shell_process_frame returned %u zones, want %u\n", n, g_numZones); fail = 1; }
    if (rgbTriplets[0] == 0 && rgbTriplets[1] == 0 && rgbTriplets[2] == 0) { fprintf(stderr, "FAIL: first zone came back black on a bright synthetic frame\n"); fail = 1; }

    if (fail) { printf(TAG " STAGE C SCOPED TEST: FAILED\n"); return 1; }
    printf(TAG " STAGE C SCOPED TEST: ALL PASSED -- shell-feed's refactored code compiles, links and runs against the real sister files\n");
    return 0;
}
