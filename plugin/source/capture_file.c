// capture_file.c -- part of ps4_ambient_light (v4.0.1). What it is and the file format: capture_file.h.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "plugin_common.h"
#include "ambient_internal.h"
#include "ambilight_paths.h"
#include "capture_file.h"

volatile uint32_t g_captureFileEnabled = 0;   // ini [dev] capture_file (optional): 1 on, default 0

#if (__FINAL__) == 0

#define CAPTURE_PATH_PREFIX  AMBILIGHT_DATA_DIR "/ps4_ambient_capture_"
#define CAPTURE_PATH_SUFFIX  ".bin"
#define CAPTURE_PATH_MAX     96
#define CAPTURE_FLUSH_MIN    4096u   // write when this much is waiting, or every 5 s, or when the buffer filled up

// Static, so it stays off every thread's stack.
static uint8_t           s_buf[CAPTURE_FILE_BUF_BYTES];
static uint32_t          s_head;      // bytes recorded; guarded by s_lock when written
static uint32_t          s_flushed;   // bytes of the buffer already on disk (0 after each flush); report thread only
static uint32_t          s_fileBytes; // size of the file so far; report thread only
static uint32_t          s_records;
static volatile uint32_t s_dropped;
static volatile uint8_t  s_full;
static volatile uint8_t  s_lock;      // 0 free; taken with a single try, never waited for
static uint64_t          s_hz;        // process time counter frequency, read once
static int               s_started;   // the file was created (and its header written)
static int               s_closed;    // the closing CAPX record was written
static char              s_path[CAPTURE_PATH_MAX];
static uint32_t          s_lastWriteSecs;

static inline void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static inline void put_u64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }

static uint64_t capture_now_us(void)
{
    if (s_hz == 0) {
        uint64_t hz = sceKernelGetProcessTimeCounterFrequency();
        s_hz = hz ? hz : 1;   // a racing second reader stores the same value
    }
    uint64_t t = sceKernelGetProcessTimeCounter();
    return (t / s_hz) * 1000000ull + ((t % s_hz) * 1000000ull) / s_hz;
}

// Appends one record. The caller holds the lock. Returns 0 if it did not fit.
static int capture_append_locked(uint64_t tsUs, const uint8_t *data, uint32_t len)
{
    if (s_head + CAPTURE_FILE_REC_HDR + len > CAPTURE_FILE_BUF_BYTES) return 0;
    uint8_t *p = s_buf + s_head;
    put_u64(p, tsUs);
    put_u16(p + 8, (uint16_t)len);
    memcpy(p + CAPTURE_FILE_REC_HDR, data, len);
    s_head += CAPTURE_FILE_REC_HDR + len;
    s_records++;
    return 1;
}

void capture_file_record(const uint8_t *data, int len)
{
    if (!g_captureFileEnabled || data == NULL || len <= 0) return;
    if (len > CAPTURE_FILE_MAX_PACKET) len = CAPTURE_FILE_MAX_PACKET;
    if (s_full) return;

    uint64_t ts = capture_now_us();
    if (__atomic_test_and_set(&s_lock, __ATOMIC_ACQUIRE)) {     // someone else is in: drop, never wait
        __atomic_fetch_add(&s_dropped, 1u, __ATOMIC_RELAXED);
        return;
    }
    if (s_full) { __atomic_clear(&s_lock, __ATOMIC_RELEASE); return; }   // the file is at its limit: nothing more is kept
    if (!capture_append_locked(ts, data, (uint32_t)len))
        __atomic_fetch_add(&s_dropped, 1u, __ATOMIC_RELAXED);   // only if the disk stalled for about a minute
    __atomic_clear(&s_lock, __ATOMIC_RELEASE);
}

static void capture_build_path(void)
{
    size_t n = strlen(CAPTURE_PATH_PREFIX);
    memcpy(s_path, CAPTURE_PATH_PREFIX, n);
    size_t o = n;
    for (const char *c = g_titleId; *c && o < CAPTURE_PATH_MAX - sizeof(CAPTURE_PATH_SUFFIX) - 1; c++) {
        char ch = *c;
        if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) s_path[o++] = ch;
    }
    if (o == n) { memcpy(s_path + o, "UNKNOWN", 7); o += 7; }
    memcpy(s_path + o, CAPTURE_PATH_SUFFIX, sizeof(CAPTURE_PATH_SUFFIX));   // includes the NUL
}

// Same raw-kernel-call route report.c and load_trace.c use, with the BSD flag values:
// O_WRONLY 0x0001, O_APPEND 0x0008, O_CREAT 0x0200, O_TRUNC 0x0400.
static int capture_write(const uint8_t *buf, size_t len, int truncate)
{
    int32_t flags = truncate ? (0x0001 | 0x0200 | 0x0400) : (0x0001 | 0x0008);
    int32_t fd = sceKernelOpen(s_path, flags, 0777);
    if (fd < 0) return -1;
    size_t done = 0;
    while (done < len) {
        ssize_t w = sceKernelWrite(fd, buf + done, len - done);
        if (w <= 0) { sceKernelClose(fd); return -1; }
        done += (size_t)w;
    }
    sceKernelClose(fd);
    return 0;
}

void capture_file_tick(uint32_t uptimeSecs)
{
    if (!g_captureFileEnabled || s_closed) return;

    while (__atomic_test_and_set(&s_lock, __ATOMIC_ACQUIRE)) { /* held for a memcpy only */ }
    uint32_t head = s_head;
    int full = s_full;
    uint32_t records = s_records;
    __atomic_clear(&s_lock, __ATOMIC_RELEASE);

    uint32_t waiting = head - s_flushed;
    int due = full || waiting >= CAPTURE_FLUSH_MIN || (waiting > 0 && uptimeSecs - s_lastWriteSecs >= 5);
    if (!due) return;
    if (s_path[0] == '\0') capture_build_path();

    if (!s_started) {
        uint8_t hdr[CAPTURE_FILE_HDR_BYTES];
        memset(hdr, 0, sizeof(hdr));
        memcpy(hdr, "AMBCAP01", 8);
        put_u32(hdr + 8, 1u);
        if (capture_write(hdr, sizeof(hdr), 1) != 0) return;   // try again next second
        s_started = 1;
        s_fileBytes = CAPTURE_FILE_HDR_BYTES;
    }
    if (waiting > 0) {
        // Data below `head` is complete (records are copied under the lock). Once written it is moved out of the buffer.
        if (capture_write(s_buf + s_flushed, waiting, 0) != 0) return;
        s_fileBytes += waiting;
        s_lastWriteSecs = uptimeSecs;
        while (__atomic_test_and_set(&s_lock, __ATOMIC_ACQUIRE)) { /* held for a memmove only */ }
        memmove(s_buf, s_buf + head, s_head - head);
        s_head -= head;
        s_flushed = 0;
        if (!full && s_fileBytes >= CAPTURE_FILE_MAX_BYTES) s_full = 1;   // the file is at its limit: the next tick closes it
        __atomic_clear(&s_lock, __ATOMIC_RELEASE);
    }
    if (full) {
        uint8_t meta[4 + 12];
        memcpy(meta, "CAPX", 4);
        put_u32(meta + 4, records);
        put_u32(meta + 8, s_dropped);
        put_u32(meta + 12, 1u);
        uint8_t rec[CAPTURE_FILE_REC_HDR + sizeof(meta)];
        put_u64(rec, capture_now_us());
        put_u16(rec + 8, (uint16_t)sizeof(meta));
        memcpy(rec + CAPTURE_FILE_REC_HDR, meta, sizeof(meta));
        if (capture_write(rec, sizeof(rec), 0) == 0) s_closed = 1;
    }
}

#endif // (__FINAL__) == 0
