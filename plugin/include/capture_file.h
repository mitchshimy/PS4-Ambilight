// capture_file.h -- v4.0.1: the debug packets, saved to a file on the console. Debug builds, [dev] capture_file=1 only.
// More: docs/code-comments.md (plugin/include/capture_file.h:1)
#ifndef CAPTURE_FILE_H
#define CAPTURE_FILE_H

#include <stdint.h>

#define CAPTURE_FILE_BUF_BYTES   (256u * 1024u)   // waits here until written, about once a second
#define CAPTURE_FILE_MAX_BYTES   (8u * 1024u * 1024u)   // the file stops growing here (about 30 minutes of probe packets)
#define CAPTURE_FILE_MAX_PACKET  192            // same cap debug_send_raw applies
#define CAPTURE_FILE_REC_HDR     10u            // u64 tsUs + u16 length
#define CAPTURE_FILE_HDR_BYTES   16u

// ini [dev] capture_file (optional, debug builds only): 1 on, default 0. Set by settings.c.
extern volatile uint32_t g_captureFileEnabled;

#if (__FINAL__) == 0
// Called from debug_send_raw() for every debug packet. Cheap, never blocks, does nothing when capture_file is off.
void capture_file_record(const uint8_t *data, int len);
// Called once a second from the report thread: writes what was recorded since the last call.
void capture_file_tick(uint32_t uptimeSecs);
#else
static inline void capture_file_record(const uint8_t *data, int len) { (void)data; (void)len; }
static inline void capture_file_tick(uint32_t uptimeSecs) { (void)uptimeSecs; }
#endif

#endif // CAPTURE_FILE_H
