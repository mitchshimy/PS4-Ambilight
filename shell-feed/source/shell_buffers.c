// shell_buffers.c -- finding the compositor's display buffers: the memory-map scan, matching the two...
// More: docs/code-comments.md (shell-feed/source/shell_buffers.c:1)
#include "shell_internal.h"

// [v14] Memory-map scan + optional CPU_READ probe (see header)
// More: docs/code-comments.md (shell-feed/source/shell_buffers.c:6)
typedef struct {
    void    *start;
    void    *end;
    int64_t  offset;
    int32_t  protection;   // bit0 CPU_READ, bit1 CPU_WRITE, bit4 GPU_READ, bit5 GPU_WRITE
    int32_t  memoryType;
    uint8_t  flags;        // bit1 = direct (sister project, confirmed on MK11)
    char     name[31];
    uint8_t  pad[64];      // generous: the kernel is told 72 bytes, as in the sister project
} VqInfo;

#define VQ_FIND_NEXT      1            // Inferred (see header)
#define VQ_PROT_CPU_READ  0x1
#define VQ_FLAG_DIRECT    0x2
#define MAP_MAX_ITER      4096
#define MAP_PRINT_CAP     80
#define CAND_MIN          (6ULL  << 20)
#define CAND_MAX          (20ULL << 20)
#define CAND_CAP          8
#define PROT_TRY_CAP      4
#define SAMPLE_WORDS      4096

static int32_t (*g_pVq)(const void *, int32_t, void *, uint64_t) = NULL;
static int32_t (*g_pMprot)(const void *, uint64_t, int32_t) = NULL;

struct Cand { uint64_t start, end; int64_t off; int32_t prot, mtype; uint8_t flags; };
static struct Cand g_cand[CAND_CAP];
static int g_nCand = 0;

static void *find_kernel_sym(const char *sym, int32_t *hOut)
{
    // Step 1: resolve-only over known handle ranges.
    for (int32_t h = 0; h <= 0x2010; h++) {
        if (h == 65) h = 0x2000;          // 0..64, then 0x2000..0x2010
        void *a = NULL;
        int r = sys_dynlib_dlsym(h, sym, &a);
        if (r == 0 && a != NULL) { printf(TAG " MAP: %s found via dlsym on handle 0x%x\n", sym, (unsigned)h); *hOut = h; return a; }
    }
    printf(TAG " MAP: %s not found by dlsym over handles 0..64 and 0x2000..0x2010\n", sym);
#if ALLOW_KERNEL_PRX_LOAD
    // Step 2: ask the loader for libkernel by name (returns the existing handle if resident).
    static const char *const names[] = { "libkernel_sys.sprx", "libkernel.sprx" };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        int32_t h = 0;
        int lr = sys_dynlib_load_prx(names[i], &h);
        void *a = NULL;
        int r = (h != 0) ? sys_dynlib_dlsym(h, sym, &a) : -1;
        printf(TAG " MAP: load_prx(\"%s\") ret=0x%x handle=0x%x -> dlsym(%s) ret=0x%x addr=%p\n",
               names[i], (unsigned)lr, (unsigned)h, sym, (unsigned)r, a);
        if (h != 0 && r == 0 && a != NULL) { *hOut = h; return a; }
    }
#endif
    return NULL;
}

static int vq_at(uint64_t addr, int32_t flags, VqInfo *vi)
{
    memset(vi, 0, sizeof(*vi));
    return g_pVq((const void *)addr, flags, vi, 72);
}

void map_scan(int verbose)
{
    g_nCand = 0;                                   // [v17] re-entrant: scenes 2-4 rescan
    if (g_pVq == NULL) {
        int32_t hq = -1, hm = -1;
        g_pVq = (int32_t (*)(const void *, int32_t, void *, uint64_t))find_kernel_sym("sceKernelVirtualQuery", &hq);
        g_pMprot = (int32_t (*)(const void *, uint64_t, int32_t))find_kernel_sym("sceKernelMprotect", &hm);
        printf(TAG " MAP: sceKernelVirtualQuery -> %p (handle %d, firstbyte=%d) | sceKernelMprotect -> %p (handle %d)\n",
               (void *)g_pVq, (int)hq, g_pVq ? first_byte((void *)g_pVq) : -1, (void *)g_pMprot, (int)hm);
    }
    if (g_pVq == NULL) { printf(TAG " MAP: VirtualQuery did not resolve; nothing to scan.\n"); return; }

    uint64_t cur = 0;
    int total = 0, printed = 0;
    for (int i = 0; i < MAP_MAX_ITER; i++) {
        VqInfo vi;
        int32_t r = vq_at(cur, VQ_FIND_NEXT, &vi);
        if (r != 0) {
            if (i == 0) printf(TAG " MAP: first FIND_NEXT query returned 0x%x (flag value may be wrong); stopping.\n", (unsigned)r);
            break;
        }
        uint64_t s = (uint64_t)vi.start, e = (uint64_t)vi.end;
        if (e <= s || s < cur) break;               // no forward progress
        total++;
        uint64_t sz = e - s;
        if (sz >= (4ULL << 20) && printed < MAP_PRINT_CAP && (verbose || ((vi.flags & VQ_FLAG_DIRECT) && sz >= CAND_MIN && sz <= CAND_MAX))) {
            printed++;
            printf(TAG " MAP: [%d] %llx-%llx size=%llu (%.2f MB) off=%llx prot=0x%x mtype=%d flags=0x%x name=%.31s%s\n",
                   total, (unsigned long long)s, (unsigned long long)e, (unsigned long long)sz, (double)sz / 1048576.0,
                   (unsigned long long)vi.offset, (unsigned)vi.protection, (int)vi.memoryType, (unsigned)vi.flags, vi.name,
                   ((vi.flags & VQ_FLAG_DIRECT) && sz >= CAND_MIN && sz <= CAND_MAX) ? "  <== CANDIDATE" : "");
        }
        if ((vi.flags & VQ_FLAG_DIRECT) && sz >= CAND_MIN && sz <= CAND_MAX && g_nCand < CAND_CAP) {
            struct Cand *c = &g_cand[g_nCand++];
            c->start = s; c->end = e; c->off = vi.offset; c->prot = vi.protection; c->mtype = vi.memoryType; c->flags = vi.flags;
        }
        cur = e;
    }
    printf(TAG " MAP: scan done. regions=%d, printed(>=4MB)=%d, candidates(direct 6-20MB)=%d\n", total, printed, g_nCand);
}

#if DO_MPROTECT_READ
static void cand_probe(void)
{
    int tried = 0;
    for (int k = 0; k < g_nCand; k++) {
        struct Cand *c = &g_cand[k];
        uint64_t len = c->end - c->start;
        VqInfo vi;
        if (vq_at(c->start, 0, &vi) != 0 || (uint64_t)vi.start != c->start || (uint64_t)vi.end != c->end) {
            printf(TAG " CAND#%d %llx: no longer matches its region, skipped\n", k, (unsigned long long)c->start);
            continue;
        }
        if (!(vi.protection & VQ_PROT_CPU_READ)) {
            if (g_pMprot == NULL || tried >= PROT_TRY_CAP) {
                printf(TAG " CAND#%d %llx: not CPU-readable (prot=0x%x) and no mprotect attempt (%s)\n", k,
                       (unsigned long long)c->start, (unsigned)vi.protection, g_pMprot ? "try cap" : "unresolved");
                continue;
            }
            tried++;
            printf(TAG " CAND#%d about to sceKernelMprotect(%llx, %llu, 0x%x) (last line before a crash = this call)\n", k,
                   (unsigned long long)c->start, (unsigned long long)len, (unsigned)(vi.protection | VQ_PROT_CPU_READ));
            int32_t pr = g_pMprot((const void *)c->start, len, vi.protection | VQ_PROT_CPU_READ);
            VqInfo chk;
            int cr = vq_at(c->start, 0, &chk);
            printf(TAG " CAND#%d mprotect returned 0x%x; re-query ret=%d prot=0x%x\n", k, (unsigned)pr, cr, (unsigned)chk.protection);
            if (pr != 0 || cr != 0 || !(chk.protection & VQ_PROT_CPU_READ)) continue;
        }

        // Bounded read-only sample. Re-verify readability immediately before reading.
        for (int pass = 0; pass < 2; pass++) {
            static uint32_t snap[2][SAMPLE_WORDS];
            if (vq_at(c->start, 0, &vi) != 0 || !(vi.protection & VQ_PROT_CPU_READ) || (uint64_t)vi.end != c->end) {
                printf(TAG " CAND#%d became unreadable before pass %d, stopping its sample\n", k, pass);
                break;
            }
            int32_t idx0 = g_lastFlipIndex;
            uint64_t stride = (len / SAMPLE_WORDS) & ~3ULL;
            if (stride == 0) stride = 4;
            uint32_t nz = 0, aff = 0;
            for (int w = 0; w < SAMPLE_WORDS; w++) {
                uint64_t off = (uint64_t)w * stride;
                if (off + 4 > len) { snap[pass][w] = 0; continue; }
                uint32_t v = *(volatile uint32_t *)(c->start + off);
                snap[pass][w] = v;
                if (v) nz++;
                if ((v >> 24) == 0xFF) aff++;
            }
            int32_t idx1 = g_lastFlipIndex;
            printf(TAG " CAND#%d pass%d size=%llu prot=0x%x mtype=%d | sampled %d words: nonzero=%u top-byte-FF=%u | flip idx %d->%d\n",
                   k, pass, (unsigned long long)len, (unsigned)vi.protection, (int)vi.memoryType, SAMPLE_WORDS,
                   (unsigned)nz, (unsigned)aff, (int)idx0, (int)idx1);
            if (pass == 0) {
                for (int q = 0; q < 8; q++) {
                    uint64_t off = ((len / 8) * (uint64_t)q) & ~0xFULL;
                    volatile uint32_t *p = (volatile uint32_t *)(c->start + off);
                    printf(TAG " CAND#%d   +0x%llx: %08x %08x %08x %08x\n", k, (unsigned long long)off,
                           (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);
                }
                usleep(16000);
            } else {
                uint32_t diff = 0;
                for (int w = 0; w < SAMPLE_WORDS; w++) if (snap[0][w] != snap[1][w]) diff++;
                printf(TAG " CAND#%d changed words between passes (~16 ms apart): %u / %d\n", k, (unsigned)diff, SAMPLE_WORDS);
            }
        }
    }
    printf(TAG " CAND: probe done.\n");
}
#endif

struct Buf g_buf[2];
int        g_nBuf = 0;
uint64_t   g_tscHz = 0;

int buf_ok(const struct Buf *b)
{
    VqInfo vi;
    return vq_at(b->start, 0, &vi) == 0 && (uint64_t)vi.start == b->start && (uint64_t)vi.end == b->start + b->len
        && (vi.protection & VQ_PROT_CPU_READ) && (vi.flags & VQ_FLAG_DIRECT);
}

// Display buffers = direct, already CPU-readable, named *compositor*, exactly 8,355,840 bytes.
void find_bufs(void)
{
    g_nBuf = 0;
    int others = 0;
    for (int k = 0; k < g_nCand; k++) {
        VqInfo vi;
        uint64_t len = g_cand[k].end - g_cand[k].start;
        if (vq_at(g_cand[k].start, 0, &vi) != 0 || !(vi.flags & VQ_FLAG_DIRECT) || !(vi.protection & VQ_PROT_CPU_READ)
            || !name_has_ci(vi.name, "compositor")) continue;
        if (len == BUF_BYTES17 && g_nBuf < 2) { g_buf[g_nBuf].start = g_cand[k].start; g_buf[g_nBuf].len = len; g_nBuf++; }
        else others++;
    }
    if (g_nBuf == 2 && g_buf[1].start < g_buf[0].start) { struct Buf t = g_buf[0]; g_buf[0] = g_buf[1]; g_buf[1] = t; }
    printf(TAG " BUFS: %d display buffer(s) matched (compositor, direct, CPU-readable, %llu B); other compositor-named 6-20MB regions: %d\n",
           g_nBuf, (unsigned long long)BUF_BYTES17, others);
    for (int b = 0; b < g_nBuf; b++)
        printf(TAG " BUFS: buf%d start=%llx len=%llu\n", b, (unsigned long long)g_buf[b].start, (unsigned long long)g_buf[b].len);
}

void calibrate_tsc(void)
{
    uint32_t f0 = g_flipCount; uint64_t a = rdtsc17();
    usleep(1000000);
    uint64_t b = rdtsc17(); uint32_t f1 = g_flipCount;
    uint32_t df = f1 - f0;
    if (df >= 30) g_tscHz = (uint64_t)((double)(b - a) * 59.94 / (double)df);   // output is 59.94 Hz per VideoOutStatus
    else          g_tscHz = (b - a);                                            // fallback: assumes usleep(1 s) was accurate
    printf(TAG " TSC: %llu ticks in ~1 s over %u compositor flips -> %llu Hz (%s)\n",
           (unsigned long long)(b - a), (unsigned)df, (unsigned long long)g_tscHz, df >= 30 ? "flip-rate based" : "usleep based, flips<30");
}

// --- self-heal: when does a run of bad buffer checks justify re-running map_scan()+find_bufs()? ---
#define HEAL_FAIL_THRESHOLD   30u     // consecutive ticks that needed a buffer and found it bad (~1 s at 30 Hz)
#define HEAL_COOLDOWN_MIN_MS  10000u  // never re-scan more often than this ...
#define HEAL_COOLDOWN_MAX_MS  60000u  // ... doubling after each re-scan that did not help, up to this

HealState g_heal;

static uint32_t heal_cooldown_ms(const HealState *h)
{
    uint32_t c = HEAL_COOLDOWN_MIN_MS;
    for (uint32_t i = 1; i < h->unrecovered && c < HEAL_COOLDOWN_MAX_MS; i++) c *= 2u;
    return c > HEAL_COOLDOWN_MAX_MS ? HEAL_COOLDOWN_MAX_MS : c;
}

// One call per tick that needed a buffer (ticks the sister's claim skipped never get here).
// Returns 1 when the caller should now re-scan.
int heal_tick(HealState *h, int bufOk, uint64_t nowMs)
{
    if (bufOk) {
        if (h->awaiting) { h->recovered++; h->awaiting = 0; }
        h->consecFail = 0; h->unrecovered = 0;
        return 0;
    }
    if (h->consecFail < 0xFFFFFFFFu) h->consecFail++;
    if (h->consecFail < HEAL_FAIL_THRESHOLD) return 0;
    if (h->heals > 0 && nowMs >= h->lastHealMs && nowMs - h->lastHealMs < heal_cooldown_ms(h)) return 0;
    h->heals++; h->unrecovered++; h->lastHealMs = nowMs; h->awaiting = 1;
    return 1;
}

static void heal_done(HealState *h, int buffersFound) { if (buffersFound <= 0) h->failed++; }

#if DO_LIVE_DDP

// [v24] One re-scan, run from live_send()'s own thread. The caller skips the current tick and re-paces.
void sh_do_heal(void)
{
    int before = g_nBuf;
    uint32_t n = g_heal.heals;
    map_scan(0);
    find_bufs();
    heal_done(&g_heal, g_nBuf);
    shell_event("SELFHEAL #%u: re-scan found %d display buffer(s) (was %d); next re-scan not before %us",
                (unsigned)n, g_nBuf, before, (unsigned)(heal_cooldown_ms(&g_heal) / 1000u));
}

#endif
