// lease.h -- loopback TCP lease: who owns the WLED strip, without files. Pure logic, host-tested; the app's copy must stay byte-identical.
// Wire (v1), client to shell feed: "C <title>\n" claims, "R\n" releases, EOF releases. Design: docs/development/shell-feed-coordination.md
#ifndef AMBILIGHT_LEASE_H
#define AMBILIGHT_LEASE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LEASE_PORT          47200        // 127.0.0.1 only
#define LEASE_TITLE_LEN     9            // same width as a CUSA title id
#define LEASE_LINE_MAX      32
#define LEASE_MAX_SLOTS     8
#define LEASE_RETRY_MS      500u         // client: wait between connect attempts while nothing listens (a refused loopback connect costs nothing)

// The SDK's sys/socket.h carries Linux values for MSG_*; the console kernel is FreeBSD. These are the FreeBSD ones.
// (#ifndef so a host test on Linux can map them to its own values.)
#ifndef LEASE_MSG_DONTWAIT
#define LEASE_MSG_DONTWAIT  0x00000080
#endif
#ifndef LEASE_MSG_NOSIGNAL
#define LEASE_MSG_NOSIGNAL  0x00020000
#endif
#ifndef LEASE_SO_NOSIGPIPE
#define LEASE_SO_NOSIGPIPE  0x00000800
#endif
// struct sockaddr_in has a length byte on FreeBSD/Orbis and none on Linux (host tests).
#if defined(__FreeBSD__)
#define LEASE_SET_SIN_LEN(a) ((a).sin_len = sizeof(a))
#else
#define LEASE_SET_SIN_LEN(a) ((void)0)
#endif

// ---------------------------------------------------------------- server side (shell feed)

typedef struct {
    int      fd;                          // -1 = free slot
    int      claimed;
    char     title[LEASE_TITLE_LEN + 1];
    char     buf[LEASE_LINE_MAX];
    int      len;
    uint32_t order;                       // age: lower = older
} LeaseSlot;

typedef struct {
    LeaseSlot slot[LEASE_MAX_SLOTS];
    uint32_t  counter;
} LeaseServer;

static inline void lease_srv_init(LeaseServer *s)
{
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < LEASE_MAX_SLOTS; i++) s->slot[i].fd = -1;
}

// Takes a new connection. Returns its slot, or -1 if every slot is held by a claiming owner (the caller closes fd).
// When full of idle connections it evicts the oldest idle one and reports its fd in *evictedFd (the caller closes it); else *evictedFd = -1.
static inline int lease_srv_add(LeaseServer *s, int fd, int *evictedFd)
{
    int idx = -1;
    *evictedFd = -1;
    for (int i = 0; i < LEASE_MAX_SLOTS; i++) if (s->slot[i].fd < 0) { idx = i; break; }
    if (idx < 0) {
        for (int i = 0; i < LEASE_MAX_SLOTS; i++)
            if (!s->slot[i].claimed && (idx < 0 || s->slot[i].order < s->slot[idx].order)) idx = i;
        if (idx < 0) return -1;
        *evictedFd = s->slot[idx].fd;
    }
    memset(&s->slot[idx], 0, sizeof(s->slot[idx]));
    s->slot[idx].fd = fd;
    s->slot[idx].order = ++s->counter;
    return idx;
}

static inline void lease_srv_remove(LeaseServer *s, int idx)
{
    memset(&s->slot[idx], 0, sizeof(s->slot[idx]));
    s->slot[idx].fd = -1;
}

// Feeds bytes read from slot idx. Returns 0 if all fine, -1 on a protocol error (the caller drops the connection:
// the owner is then treated as released). *changed is set when this slot's claim state or title changed.
static inline int lease_srv_feed(LeaseServer *s, int idx, const char *data, size_t n, int *changed)
{
    LeaseSlot *sl = &s->slot[idx];
    *changed = 0;
    for (size_t i = 0; i < n; i++) {
        char c = data[i];
        if (c != '\n') {
            if (sl->len >= LEASE_LINE_MAX - 1) return -1;
            sl->buf[sl->len++] = c;
            continue;
        }
        sl->buf[sl->len] = '\0';
        int len = sl->len;
        sl->len = 0;
        if (len == 1 && sl->buf[0] == 'R') {
            if (sl->claimed) *changed = 1;
            sl->claimed = 0;
        } else if (len >= 3 && sl->buf[0] == 'C' && sl->buf[1] == ' ') {
            char t[LEASE_TITLE_LEN + 1];
            int tn = len - 2 > LEASE_TITLE_LEN ? LEASE_TITLE_LEN : len - 2;
            for (int k = 0; k < tn; k++) { char ch = sl->buf[2 + k]; t[k] = (ch >= 0x20 && ch < 0x7f) ? ch : '?'; }
            t[tn] = '\0';
            if (!sl->claimed || strcmp(sl->title, t) != 0) *changed = 1;
            sl->claimed = 1;
            memcpy(sl->title, t, (size_t)tn + 1);
        } else {
            return -1;
        }
    }
    return 0;
}

// How many connections hold a claim; the first claimant's title goes to out (LEASE_TITLE_LEN+1 bytes) when out is not NULL.
static inline int lease_srv_claims(const LeaseServer *s, char *out)
{
    int n = 0;
    if (out) out[0] = '\0';
    for (int i = 0; i < LEASE_MAX_SLOTS; i++)
        if (s->slot[i].fd >= 0 && s->slot[i].claimed) {
            if (n == 0 && out) memcpy(out, s->slot[i].title, LEASE_TITLE_LEN + 1);
            n++;
        }
    return n;
}

// ---------------------------------------------------------------- client side (game plugin, companion app)

typedef struct {
    int  (*open)(void *ctx);                                   // connect to 127.0.0.1:LEASE_PORT; fd, or -1
    int  (*send)(void *ctx, int fd, const char *msg, size_t n); // 0 = all sent
    int  (*peek)(void *ctx, int fd);                           // 1 = still connected, 0 = the shell feed closed it, -1 = error (treated as closed)
    void (*close)(void *ctx, int fd);
    void *ctx;
} LeaseOps;

typedef struct {
    int      fd;
    int      wantFg;                    // what the owner wants the shell feed to believe
    char     title[LEASE_TITLE_LEN + 1];
    int      sentValid;                 // the shell feed has been told wantFg/title on THIS connection
    int      sentFg;
    char     sentTitle[LEASE_TITLE_LEN + 1];
    int      tried;                     // at least one connect attempt has been made
    uint32_t nextTryMs;
} LeaseClient;

static inline void lease_client_init(LeaseClient *c)
{
    memset(c, 0, sizeof(*c));
    c->fd = -1;
}

static inline void lease_client_set(LeaseClient *c, int fg, const char *title)
{
    c->wantFg = fg ? 1 : 0;
    const char *src = (title && title[0]) ? title : "-";
    size_t n = strlen(src);
    if (n > LEASE_TITLE_LEN) n = LEASE_TITLE_LEN;
    int k = 0;
    while (k < (int)n && src[k] != ' ') { c->title[k] = (src[k] >= 0x20 && src[k] < 0x7f) ? src[k] : '?'; k++; } // titles are space-padded elsewhere
    if (k == 0) c->title[k++] = '-';
    c->title[k] = '\0';
}

static inline int lease_client_connected(const LeaseClient *c) { return c->fd >= 0; }

static inline void lease_client_drop(LeaseClient *c, const LeaseOps *ops, uint32_t nowMs)
{
    if (c->fd >= 0) ops->close(ops->ctx, c->fd);
    c->fd = -1;
    c->sentValid = 0;
    c->nextTryMs = nowMs + LEASE_RETRY_MS;
}

// One look. needConnect = connect even with nothing to claim, to learn whether a shell feed runs; a claim always connects, and the connection is kept.
// Cheap when idle: one zero-timeout readability check, and nothing at all while disconnected and not needed.
static inline void lease_client_tick(LeaseClient *c, uint32_t nowMs, const LeaseOps *ops, int needConnect)
{
    if (c->fd >= 0 && ops->peek(ops->ctx, c->fd) <= 0) lease_client_drop(c, ops, nowMs); // the shell feed went away
    if (c->fd < 0) {
        if (!needConnect && !c->wantFg) return;
        if (c->tried && (int32_t)(nowMs - c->nextTryMs) < 0) return;
        c->tried = 1;
        c->fd = ops->open(ops->ctx);
        if (c->fd < 0) { c->fd = -1; c->nextTryMs = nowMs + LEASE_RETRY_MS; return; }
        c->sentValid = 0;
    }
    if (c->sentValid && c->sentFg == c->wantFg && (!c->wantFg || strcmp(c->sentTitle, c->title) == 0)) return;
    if (!c->sentValid && !c->wantFg) { c->sentValid = 1; c->sentFg = 0; c->sentTitle[0] = '\0'; return; } // a new connection starts released
    char msg[LEASE_LINE_MAX];
    size_t n = 0;
    if (c->wantFg) { msg[n++] = 'C'; msg[n++] = ' '; for (const char *t = c->title; *t && n < sizeof(msg) - 2; t++) msg[n++] = *t; }
    else msg[n++] = 'R';
    msg[n++] = '\n';
    if (ops->send(ops->ctx, c->fd, msg, n) != 0) { lease_client_drop(c, ops, nowMs); return; }
    c->sentValid = 1;
    c->sentFg = c->wantFg;
    memcpy(c->sentTitle, c->wantFg ? c->title : "", c->wantFg ? strlen(c->title) + 1 : 1);
}

// Owner is exiting cleanly: say so, then close. (An owner that is killed needs none of this: the kernel closes the socket.)
static inline void lease_client_shutdown(LeaseClient *c, const LeaseOps *ops)
{
    if (c->fd >= 0) {
        if (c->sentValid && c->sentFg) ops->send(ops->ctx, c->fd, "R\n", 2);
        ops->close(ops->ctx, c->fd);
    }
    c->fd = -1;
    c->sentValid = 0;
}

#endif // AMBILIGHT_LEASE_H
