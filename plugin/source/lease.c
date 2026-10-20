// lease.c -- the plugin's socket half of the loopback lease (logic and wire format: lease.h). State thread only; every call is non-blocking or bounded
// (a refused loopback connect is instant, a few-byte send never waits, the liveness check is a zero-timeout select).
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdbool.h>
#include "plugin_common.h"
#include "ambient_internal.h"
#include "lease.h"

static int lease_open(void *ctx)
{
    (void)ctx;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int on = 1;
    setsockopt(fd, SOL_SOCKET, LEASE_SO_NOSIGPIPE, &on, sizeof(on)); // a send to a closed peer must not raise SIGPIPE in a game
    struct timeval to = { 0, 100000 };                                // bounds send() only; a connect to a local listener completes in the kernel
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    LEASE_SET_SIN_LEN(a);
    a.sin_family = AF_INET;
    a.sin_port = htons(LEASE_PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) { close(fd); return -1; }
    return fd;
}

static int lease_send(void *ctx, int fd, const char *msg, size_t n)
{
    (void)ctx;
    return send(fd, msg, n, LEASE_MSG_NOSIGNAL) == (ssize_t)n ? 0 : -1;
}

// 1 = connected and quiet, 0 = the peer closed, -1 = error. The shell feed never sends, so any readable event is a close (or noise to drain).
static int lease_peek(void *ctx, int fd)
{
    (void)ctx;
    if (fd < 0 || fd >= FD_SETSIZE) return -1;
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(fd, &rs);
    struct timeval tv = { 0, 0 };
    int r = select(fd + 1, &rs, NULL, NULL, &tv);
    if (r < 0) return -1;
    if (r == 0 || !FD_ISSET(fd, &rs)) return 1;
    char sink[16];
    ssize_t n = recv(fd, sink, sizeof(sink), LEASE_MSG_DONTWAIT);
    if (n > 0) return 1;
    return n == 0 ? 0 : -1;
}

static void lease_close(void *ctx, int fd) { (void)ctx; close(fd); }

const LeaseOps *ambient_lease_ops(void)
{
    static const LeaseOps ops = { lease_open, lease_send, lease_peek, lease_close, NULL };
    return &ops;
}
