// shell_probe17.c -- the probe's original v17 sampler: its own tiling math, a fixed 229-zone layout...
// More: docs/code-comments.md (shell-feed/source/shell_probe17.c:1)
#include "shell_internal.h"

// [v17] Sampler core. The block between the markers is plain C with no SDK dependency: it is compiled...
// More: docs/code-comments.md (shell-feed/source/shell_probe17.c:10)

typedef struct {
    uint32_t pipeConfig, numPipes, bankWidth, bankHeight, numBanks;
    uint32_t macroTileWidth, macroTileHeight, paddedWidth, tileSplitBytes;
    uint32_t pipeInterleaveBits, pipeBits, bankBits;
} TileParams17;
static const TileParams17 kParamsBase17 = { 0xc, 8, 1, 1, 16, 128, 64, 1920, 512, 8, 3, 4 };

static uint32_t t17_elem(uint32_t x, uint32_t y)
{
    uint32_t e = 0;
    e |= ((x >> 0) & 1) << 0; e |= ((x >> 1) & 1) << 1; e |= ((y >> 0) & 1) << 2;
    e |= ((x >> 2) & 1) << 3; e |= ((y >> 1) & 1) << 4; e |= ((y >> 2) & 1) << 5;
    return e;
}
static uint32_t t17_pipe(uint32_t x, uint32_t y)
{
    uint32_t p = 0;
    p |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 1) << 0;
    p |= (((x >> 4) ^ (y >> 4)) & 1) << 1;
    p |= (((x >> 5) ^ (y >> 5)) & 1) << 2;
    return p;
}
static uint32_t t17_log2(uint32_t i) { uint32_t l = 0; while ((i >>= 1) != 0) l++; return l; }
static uint32_t t17_bank(uint32_t x, uint32_t y, const TileParams17 *p)
{
    const uint32_t xs = x >> t17_log2(p->bankWidth * p->numPipes);
    const uint32_t ys = y >> t17_log2(p->bankHeight);
    uint32_t b = 0;
    b |= (((xs >> 3) ^ (ys >> 6)) & 1) << 0;
    b |= (((xs >> 4) ^ (ys >> 5) ^ (ys >> 6)) & 1) << 1;
    b |= (((xs >> 5) ^ (ys >> 4)) & 1) << 2;
    b |= (((xs >> 6) ^ (ys >> 3)) & 1) << 3;
    return b;
}
static uint64_t t17_offset(const TileParams17 *p, uint32_t x, uint32_t y)
{
    uint64_t element_index = t17_elem(x, y);
    uint64_t pipe = t17_pipe(x, y);
    uint64_t bank = t17_bank(x, y, p);
    uint32_t tile_bytes = (8 * 8 * 1 * 32 * 1 + 7) / 8;   // 256
    uint64_t element_offset = element_index * 32;
    uint64_t macro_tile_bytes = (p->macroTileWidth / 8) * (p->macroTileHeight / 8) * tile_bytes / (p->numPipes * p->numBanks);
    uint64_t macro_tiles_per_row = p->paddedWidth / p->macroTileWidth;
    uint64_t macro_tile_index = ((uint64_t)(y / p->macroTileHeight) * macro_tiles_per_row) + (x / p->macroTileWidth);
    uint64_t macro_tile_offset = macro_tile_index * macro_tile_bytes;
    uint64_t tile_row_index = (y / 8) % p->bankHeight;
    uint64_t tile_column_index = ((x / 8) / p->numPipes) % p->bankWidth;
    uint64_t tile_offset = ((tile_row_index * p->bankWidth) + tile_column_index) * tile_bytes;
    uint64_t total_offset = (macro_tile_offset + tile_offset) * 8 + element_offset;
    uint64_t bitOffset = total_offset & 0x7;
    total_offset /= 8;
    uint32_t pipeInterleaveMask = (1u << p->pipeInterleaveBits) - 1;
    uint64_t pipe_interleave_offset = total_offset & pipeInterleaveMask;
    uint64_t offset = total_offset >> p->pipeInterleaveBits;
    uint64_t finalByteOffset = pipe_interleave_offset
        | (pipe << (p->pipeInterleaveBits))
        | (bank << (p->pipeInterleaveBits + p->pipeBits))
        | (offset << (p->pipeInterleaveBits + p->pipeBits + p->bankBits));
    return ((finalByteOffset << 3) | bitOffset) / 8;
}

// Copies NZ zone colours into a DDP payload in LED order.
static void zones_to_leds(uint8_t *dst, const uint8_t (*rgb)[3])
{
    for (int z = 0; z < NZ; z++) {
        int led = (z + LED_ROT) % NZ;
        dst[3 * led + 0] = rgb[z][0];
        dst[3 * led + 1] = rgb[z][1];
        dst[3 * led + 2] = rgb[z][2];
    }
}

static uint32_t g_ptOff[NZ * ZPTS];
static uint16_t g_ptX[NZ * ZPTS], g_ptY[NZ * ZPTS];
static uint32_t g_scOff[SC_PTS];
static uint16_t g_scX[SC_PTS], g_scY[SC_PTS];

int      g_badPts = 0;

// Zone order: 0..72 top (left->right), 73..113 right (top->bottom), 114..186 bottom (right->left)...
// More: docs/code-comments.md (shell-feed/source/shell_probe17.c:96)
static void zone_rect(int z, int *x0, int *y0, int *x1, int *y1)
{
    if (z < LED_TOP) {
        int k = z;
        *x0 = k * SCR_W / LED_TOP; *x1 = (k + 1) * SCR_W / LED_TOP; *y0 = 0; *y1 = BAND_TB;
    } else if (z < LED_TOP + LED_RIGHT) {
        int k = z - LED_TOP;
        *y0 = k * SCR_H / LED_RIGHT; *y1 = (k + 1) * SCR_H / LED_RIGHT; *x0 = SCR_W - BAND_LR; *x1 = SCR_W;
    } else if (z < LED_TOP + LED_RIGHT + LED_BOTTOM) {
        int c = LED_BOTTOM - 1 - (z - LED_TOP - LED_RIGHT);
        *x0 = c * SCR_W / LED_BOTTOM; *x1 = (c + 1) * SCR_W / LED_BOTTOM; *y0 = SCR_H - BAND_TB; *y1 = SCR_H;
    } else {
        int c = LED_LEFT - 1 - (z - LED_TOP - LED_RIGHT - LED_BOTTOM);
        *y0 = c * SCR_H / LED_LEFT; *y1 = (c + 1) * SCR_H / LED_LEFT; *x0 = 0; *x1 = BAND_LR;
    }
}

void build_points(void)
{
    for (int z = 0; z < NZ; z++) {
        int x0, y0, x1, y1;
        zone_rect(z, &x0, &y0, &x1, &y1);
        for (int j = 0; j < ZPTS; j++) {
            int fx = (j & 1) ? 3 : 1, fy = (j >> 1) ? 3 : 1;
            int x = x0 + (x1 - x0) * fx / 4, y = y0 + (y1 - y0) * fy / 4;
            if (x > SCR_W - 1) x = SCR_W - 1;
            if (y > SCR_H - 1) y = SCR_H - 1;
            uint64_t off = t17_offset(&kParamsBase17, (uint32_t)x, (uint32_t)y);
            if (off + 4 > BUF_BYTES17) { off = 0; g_badPts++; }
            g_ptX[z * ZPTS + j] = (uint16_t)x; g_ptY[z * ZPTS + j] = (uint16_t)y;
            g_ptOff[z * ZPTS + j] = (uint32_t)off;
        }
    }
    uint32_t s = 0x1234567u;
    for (int i = 0; i < SC_PTS; i++) {
        s = s * 1664525u + 1013904223u; int x = (int)((s >> 8) % SCR_W);
        s = s * 1664525u + 1013904223u; int y = (int)((s >> 8) % SCR_H);
        uint64_t off = t17_offset(&kParamsBase17, (uint32_t)x, (uint32_t)y);
        if (off + 4 > BUF_BYTES17) { off = 0; g_badPts++; }
        g_scX[i] = (uint16_t)x; g_scY[i] = (uint16_t)y; g_scOff[i] = (uint32_t)off;
    }
}

static inline uint32_t rd32_17(uint64_t a) { return *(volatile uint32_t *)a; }

// Word layout A8R8G8B8: 0xAARRGGBB. Average of ZPTS points per zone.
static void sample_zones(uint64_t base, uint8_t rgb[][3])
{
    for (int z = 0; z < NZ; z++) {
        uint32_t r = 0, g = 0, b = 0;
        for (int j = 0; j < ZPTS; j++) {
            uint32_t w = rd32_17(base + g_ptOff[z * ZPTS + j]);
            r += (w >> 16) & 255; g += (w >> 8) & 255; b += w & 255;
        }
        rgb[z][0] = (uint8_t)(r / ZPTS); rgb[z][1] = (uint8_t)(g / ZPTS); rgb[z][2] = (uint8_t)(b / ZPTS);
    }
}

// The offline-measured bar colour class (v16-A): R ~= G, B a little higher, mid brightness.
static int is_lav17(uint32_t w)
{
    int r = (w >> 16) & 255, g = (w >> 8) & 255, b = w & 255;
    int dg = r > g ? r - g : g - r;
    return r >= 0x60 && dg <= 4 && (b - r) >= 6 && (b - r) <= 40;
}

#define BURST_TICKS 300
struct TickRec { uint64_t t0, t1; int32_t idx0, idx1; uint32_t zt[2]; uint32_t sc[2][SC_PTS]; };
static struct TickRec g_rec[BURST_TICKS];
struct BurstHdr { uint32_t magic, ver, nBuf, ticks, scPts, recSize, nz, zpts; uint64_t tscHz; uint64_t bufStart[2]; };

static void run_burst(int scene)
{
    if (g_nBuf == 0 || g_tscHz == 0) { printf(TAG " BURST s%d: skipped (no buffers or no TSC calibration)\n", scene); return; }
    static uint8_t zc[2][NZ][3];
    int done = 0;
    printf(TAG " BURST s%d: about to run %d ticks (reads only; both regions re-verified every tick)\n", scene, BURST_TICKS);
    for (int k = 0; k < BURST_TICKS; k++) {
        int ok = buf_ok(&g_buf[0]);
        for (int b = 1; b < g_nBuf; b++) ok = ok && buf_ok(&g_buf[b]);
        if (!ok) { printf(TAG " BURST s%d: a display buffer changed or vanished at tick %d; stopped\n", scene, k); break; }
        struct TickRec *r = &g_rec[k];
        memset(r->sc, 0, sizeof(r->sc)); r->zt[0] = r->zt[1] = 0;
        r->t0 = rdtsc17(); r->idx0 = g_lastFlipIndex;
        for (int b = 0; b < g_nBuf; b++) {
            uint64_t za = rdtsc17();
            for (int i = 0; i < SC_PTS; i++) r->sc[b][i] = rd32_17(g_buf[b].start + g_scOff[i]);
            sample_zones(g_buf[b].start, zc[b]);
            r->zt[b] = (uint32_t)(rdtsc17() - za);
        }
        r->idx1 = g_lastFlipIndex; r->t1 = rdtsc17();
        done++;
        usleep(1000);
    }
    if (done == 0) return;

    // ---- summary for the log ----
    uint64_t sumTick = 0, maxTick = 0, minTick = ~0ULL, sumZt[2] = {0, 0};
    uint32_t stable = 0, idxMoved = 0, lav[2][2] = {{0, 0}, {0, 0}}, lavTot[2][2] = {{0, 0}, {0, 0}}, chg[2] = {0, 0};
    for (int k = 0; k < done; k++) {
        struct TickRec *r = &g_rec[k];
        uint64_t d = r->t1 - r->t0; sumTick += d; if (d > maxTick) maxTick = d; if (d < minTick) minTick = d;
        sumZt[0] += r->zt[0]; sumZt[1] += r->zt[1];
        if (r->idx0 != r->idx1) idxMoved++;
        if (r->idx0 == r->idx1 && (r->idx0 == 0 || r->idx0 == 1)) {
            stable++;
            for (int b = 0; b < g_nBuf; b++) {
                int m = (r->idx0 == b) ? 1 : 0;        // m=1: this buffer's index is the one just flipped to
                for (int i = 0; i < SC_PTS; i++) { lavTot[b][m]++; if (is_lav17(r->sc[b][i])) lav[b][m]++; }
            }
        }
        if (k > 0) for (int b = 0; b < g_nBuf; b++)
            for (int i = 0; i < SC_PTS; i++) if (r->sc[b][i] != g_rec[k - 1].sc[b][i]) chg[b]++;
    }
    double us = 1e6 / (double)g_tscHz;
    printf(TAG " BURST s%d: %d ticks, tick us avg/min/max = %.0f/%.0f/%.0f; full 229-zone+128-point pass us per buffer avg = %.0f / %.0f; flip idx moved inside %u ticks; %u ticks had a steady idx 0/1\n",
           scene, done, (double)sumTick / done * us, (double)minTick * us, (double)maxTick * us,
           (double)sumZt[0] / done * us, g_nBuf > 1 ? (double)sumZt[1] / done * us : 0.0, (unsigned)idxMoved, (unsigned)stable);
    for (int b = 0; b < g_nBuf; b++)
        printf(TAG " BURST s%d: buf%d grey-lavender scatter hits: when idx==%d: %u/%u | when idx!=%d: %u/%u | scatter words changed tick-to-tick: %u of %d\n",
               scene, b, b, (unsigned)lav[b][1], (unsigned)lavTot[b][1], b, (unsigned)lav[b][0], (unsigned)lavTot[b][0],
               (unsigned)chg[b], (done > 1 ? (done - 1) * SC_PTS : 0));

    // ---- raw record file for offline analysis ----
    char path[64];
    snprintf(path, sizeof(path), AMBILIGHT_DATA_DIR "/shell_probe_v17_s%d_burst.bin", scene);
    int32_t fd = sceKernelOpen(path, 0x200 | 0x400 | 0x001, 0777);
    if (fd < 0) { printf(TAG " BURST s%d: cannot create %s (%d)\n", scene, path, (int)fd); return; }
    struct BurstHdr h; memset(&h, 0, sizeof(h));
    h.magic = 0x42373156u; h.ver = 1; h.nBuf = (uint32_t)g_nBuf; h.ticks = (uint32_t)done; h.scPts = SC_PTS;
    h.recSize = sizeof(struct TickRec); h.nz = NZ; h.zpts = ZPTS; h.tscHz = g_tscHz;
    for (int b = 0; b < g_nBuf; b++) h.bufStart[b] = g_buf[b].start;
    int64_t w1 = sceKernelWrite(fd, &h, sizeof(h));
    int64_t w2 = sceKernelWrite(fd, g_scX, sizeof(g_scX));
    int64_t w3 = sceKernelWrite(fd, g_scY, sizeof(g_scY));
    int64_t w4 = sceKernelWrite(fd, g_rec, (uint64_t)done * sizeof(struct TickRec));
    sceKernelClose(fd);
    printf(TAG " BURST s%d: wrote %s (hdr %lld, x %lld, y %lld, recs %lld of %llu bytes)\n", scene, path,
           (long long)w1, (long long)w2, (long long)w3, (long long)w4, (unsigned long long)done * sizeof(struct TickRec));
}

static void ddp_preview(int scene, int c, uint8_t rgb[][3])
{
    uint8_t pk[10 + 3 * NZ];
    int dataSize = 3 * NZ;
    pk[0] = 0x41; pk[1] = 0; pk[2] = 0x0B; pk[3] = 0x01; pk[4] = pk[5] = pk[6] = pk[7] = 0;
    pk[8] = (uint8_t)((dataSize >> 8) & 255); pk[9] = (uint8_t)(dataSize & 255);
    zones_to_leds(pk + 10, (const uint8_t (*)[3])rgb);   // [v21] LED order, not zone order
    printf(TAG " DDP s%d c%d: %d-byte packet built (header %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x, %d zones); first zones rgb: %02x%02x%02x %02x%02x%02x %02x%02x%02x [send %s]\n",
           scene, c, 10 + dataSize, pk[0], pk[1], pk[2], pk[3], pk[4], pk[5], pk[6], pk[7], pk[8], pk[9], NZ,
           rgb[0][0], rgb[0][1], rgb[0][2], rgb[1][0], rgb[1][1], rgb[1][2], rgb[2][0], rgb[2][1], rgb[2][2],
           DO_WLED_SEND ? "ON" : "compiled out");
#if DO_WLED_SEND
    // Mirrors the sister project's network.c (one UDP packet, 5 ms send timeout). Off by default.
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct timeval to = { 0, 5000 };
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
        struct sockaddr_in d; memset(&d, 0, sizeof(d));
        d.sin_family = AF_INET; d.sin_port = htons(WLED_PORT);
        if (inet_pton(AF_INET, WLED_HOST, &d.sin_addr) == 1)
            sendto(sock, pk, 10 + dataSize, 0, (struct sockaddr *)&d, sizeof(d));
        close(sock);
    }
#endif
}

#if DO_DUMP_BUFFERS
static uint8_t g_dumpChunk[64 * 1024];

// v16's dump, extended: while copying, pick up the zone points that fall in each chunk so the
// zone colours are computed from the SAME copy that lands in the file.
static void dump_bufs(int scene)
{
    for (int b = 0; b < g_nBuf; b++) {
        const uint64_t start = g_buf[b].start, len = g_buf[b].len;
        if (!buf_ok(&g_buf[b])) { printf(TAG " DUMP s%d buf%d: skipped (region changed or not readable)\n", scene, b); continue; }
        char path[64];
        snprintf(path, sizeof(path), AMBILIGHT_DATA_DIR "/shell_probe_v17_s%d_c%d.raw", scene, b);
        int32_t fd = sceKernelOpen(path, 0x200 | 0x400 | 0x001, 0777);
        if (fd < 0) { printf(TAG " DUMP s%d: cannot create %s (%d)\n", scene, path, (int)fd); return; }
        static uint32_t zs[NZ][3];
        memset(zs, 0, sizeof(zs));
        int32_t idx0 = g_lastFlipIndex;
        printf(TAG " DUMP s%d: about to copy %llx len=%llu -> %s (flip idx %d)\n", scene,
               (unsigned long long)start, (unsigned long long)len, path, (int)idx0);
        uint64_t done = 0; int bad = 0;
        while (done < len) {
            uint64_t chunk = len - done;
            if (chunk > sizeof(g_dumpChunk)) chunk = sizeof(g_dumpChunk);
            volatile uint32_t *src = (volatile uint32_t *)(start + done);
            uint32_t *dst = (uint32_t *)g_dumpChunk;
            for (uint64_t w = 0; w < chunk / 4; w++) dst[w] = src[w];
            for (int i = 0; i < NZ * ZPTS; i++) {
                uint32_t off = g_ptOff[i];
                if (off >= done && off < done + chunk) {
                    uint32_t w = dst[(off - done) >> 2]; int z = i / ZPTS;
                    zs[z][0] += (w >> 16) & 255; zs[z][1] += (w >> 8) & 255; zs[z][2] += w & 255;
                }
            }
            int64_t wr = sceKernelWrite(fd, g_dumpChunk, chunk);
            if (wr != (int64_t)chunk) { bad = 1; break; }
            done += chunk;
        }
        sceKernelClose(fd);
        printf(TAG " DUMP s%d: %s %s, %llu of %llu bytes, flip idx %d->%d, region %llx\n", scene, path,
               bad ? "WRITE SHORT" : "ok", (unsigned long long)done, (unsigned long long)len,
               (int)idx0, (int)g_lastFlipIndex, (unsigned long long)start);
        if (bad) continue;
        static uint8_t rgb[NZ][3];
        for (int z = 0; z < NZ; z++) for (int c = 0; c < 3; c++) rgb[z][c] = (uint8_t)(zs[z][c] / ZPTS);
        for (int z0 = 0; z0 < NZ; z0 += 29) {
            char line[29 * 7 + 8]; int n = 0;
            for (int z = z0; z < z0 + 29 && z < NZ; z++) n += snprintf(line + n, sizeof(line) - (size_t)n, "%02x%02x%02x ", rgb[z][0], rgb[z][1], rgb[z][2]);
            printf(TAG " ZONES s%d c%d [%d..%d]: %s\n", scene, b, z0, (z0 + 28 < NZ ? z0 + 28 : NZ - 1), line);
        }
        ddp_preview(scene, b, rgb);
    }
}
#endif

static void scene_run(int scene, int do_dump)
{
    static uint32_t lastFlips = 0;
    uint32_t f = g_flipCount;
    printf(TAG " SCENE %d begin: compositor flips=%u (+%u since the previous scene), last idx=%d, register=%u\n",
           scene, (unsigned)f, (unsigned)(f - lastFlips), (int)g_lastFlipIndex, (unsigned)g_regCount);
    lastFlips = f;
    map_scan(scene == 1);
    find_bufs();
    if (g_nBuf == 0) { printf(TAG " SCENE %d: no display buffers matched -- nothing sampled (compare the candidates above with scene 1)\n", scene); return; }
    run_burst(scene);
#if DO_DUMP_BUFFERS
    if (do_dump) dump_bufs(scene);
#else
    (void)do_dump;
#endif
    printf(TAG " SCENE %d done: compositor flips=%u\n", scene, (unsigned)g_flipCount);
}
