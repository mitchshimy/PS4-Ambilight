// shell_log.c -- how the shell feed says things: klog lines, the [dev] UDP mirror ("SHL1" packets to
// dev_ip:4048), and the few lines in the console's own notification bar.
#include "shell_internal.h"

// --- [dev] UDP mirror: the sister's telemetry idiom (network.c debug_send_raw): DDP header, port 4048,
// payload capped at 192 bytes, own short-lived socket, and BOTH dev_ip and dev_logging=true required. The
// payload starts with the 4-byte magic "SHL1" so a listener can tell it from the plugin's binary packets. ---
#define SH_DEV_PORT         4048
#define SH_DEV_PAYLOAD_MAX  192
#define SH_DEV_MAGIC        "SHL1"

int sh_dev_active(int devLogging, const char *devIp) { return devLogging && devIp != NULL && devIp[0] != '\0'; }

static size_t sh_dev_pack(uint8_t *out, size_t cap, const char *text)
{
    size_t tl = strlen(text), room = SH_DEV_PAYLOAD_MAX - 4;
    if (tl > room) tl = room;
    while (tl > 0 && (text[tl - 1] == '\n' || text[tl - 1] == '\r')) tl--;
    size_t plen = 4 + tl;
    if (cap < 10 + plen) return 0;
    out[0] = 0x41; out[1] = 0; out[2] = 0x0B; out[3] = 0x01; out[4] = out[5] = out[6] = out[7] = 0;
    out[8] = (uint8_t)((plen >> 8) & 0xFF); out[9] = (uint8_t)(plen & 0xFF);
    memcpy(out + 10, SH_DEV_MAGIC, 4);
    memcpy(out + 14, text, tl);
    return 10 + plen;
}

int  g_shDevLog = 0;     // [dev] dev_logging, set by sh_parse_aux() (defined with the ini globals)
char g_shDevIp[16] = ""; // [dev] dev_ip

// Fire-and-forget: one socket per call (events and 5 s rows only, never per tick), 5 ms send timeout.
void sh_dev_send(const char *text)
{
#if SHELL_DEV_MIRROR && DO_WLED_SEND_INCLUDES
    if (!sh_dev_active(g_shDevLog, g_shDevIp)) return;
    uint8_t pkt[10 + SH_DEV_PAYLOAD_MAX];
    size_t n = sh_dev_pack(pkt, sizeof(pkt), text);
    if (n == 0) return;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return;
    struct timeval to = { 0, 5000 };
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_port = htons(SH_DEV_PORT);
    if (inet_pton(AF_INET, g_shDevIp, &a.sin_addr) == 1)
        sendto(s, pkt, n, 0, (struct sockaddr *)&a, sizeof(a));
    close(s);
#else
    (void)text;
#endif
}

// A new log line: klog AND the [dev] mirror. No trailing newline in fmt.
void shell_event(const char *fmt, ...)
{
    char line[SH_DEV_PAYLOAD_MAX + 64];
    va_list ap; va_start(ap, fmt); vsnprintf(line, sizeof(line), fmt, ap); va_end(ap);
    printf(TAG " %s\n", line);
    sh_dev_send(line);
}

// [v47] A line in the console's own notification bar, for the few shell-feed states a person has to know
// about without a monitor or the [dev] mirror. Used only where the strip stops or comes back: not per frame,
// not per re-scan (the heal cooldown alone doesn't make those worth a message), and nothing about whether
// WLED answers. [notifications] enabled=false silences all but the two "can't start: wled_host" ones.
void sh_notify(bool essential, const char *fmt, ...)
{
    // essential: the feed can't work without the person knowing (no WLED address). Sent whatever
    // [notifications] enabled says; the rest respects it (g_config is the shared ini the sister reads).
    if (!essential && !g_config.notificationsEnabled) return;
    char msg[176];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    Notify(TEX_ICON_SYSTEM, "PS4 Ambilight: %s", msg);
}
