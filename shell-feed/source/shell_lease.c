// shell_lease.c -- the shell feed's end of the loopback TCP lease (wire format and slot logic: plugin/include/lease.h); runs on the I/O worker.
// shell_lease_open() runs once from the send loop before the worker starts; the loop only reads shell_lease_claimed() (atomic), never a socket.
#include "shell_internal.h"
#include "lease.h"
#include <sys/select.h>

volatile int g_leaseUp = 0;                  // the listener is open: claims arrive over the lease and the claim FILE is not read
static int g_leaseListen = -1;
static LeaseServer g_leaseSrv;
static volatile int g_leaseClaims = 0;       // published for the send loop

int shell_lease_claimed(void) { return __atomic_load_n(&g_leaseClaims, __ATOMIC_ACQUIRE); }

static void lease_publish(const char *why, int idx)
{
    char title[LEASE_TITLE_LEN + 1];
    int n = lease_srv_claims(&g_leaseSrv, title);
    int prev = __atomic_exchange_n(&g_leaseClaims, n, __ATOMIC_ACQ_REL);
    if (n != prev) shell_event("LEASE %s slot=%d claims=%d title=%s", why, idx, n, n ? title : "-");
}

// 1 = listening, 0 = could not (the claim file stays the channel, as before).
int shell_lease_open(void)
{
    if (g_leaseUp) return 1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { shell_event("LEASE socket() failed (%d); claim file stays the handoff channel", fd); return 0; }
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    LEASE_SET_SIN_LEN(a);
    a.sin_family = AF_INET;
    a.sin_port = htons(LEASE_PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);      // loopback only: nothing on the LAN can take the strip
    int rb = bind(fd, (struct sockaddr *)&a, sizeof(a));
    int rl = (rb == 0) ? listen(fd, 4) : -1;
    if (rb != 0 || rl != 0) {
        shell_event("LEASE bind/listen failed (bind=%d listen=%d); claim file stays the handoff channel", rb, rl);
        close(fd);
        return 0;
    }
    lease_srv_init(&g_leaseSrv);
    __atomic_store_n(&g_leaseClaims, 0, __ATOMIC_RELEASE);
    g_leaseListen = fd;
    g_leaseUp = 1;
    shell_event("LEASE listening on 127.0.0.1:%d", LEASE_PORT);
    return 1;
}

// Closes the listener and every connection. The plugins see the EOF, know the shell feed is gone, and fall back to retrying.
void shell_lease_close(void)
{
    if (!g_leaseUp) return;
    for (int i = 0; i < LEASE_MAX_SLOTS; i++) if (g_leaseSrv.slot[i].fd >= 0) close(g_leaseSrv.slot[i].fd);
    close(g_leaseListen);
    g_leaseListen = -1;
    lease_srv_init(&g_leaseSrv);
    __atomic_store_n(&g_leaseClaims, 0, __ATOMIC_RELEASE);
    g_leaseUp = 0;
    shell_event("LEASE closed");
}

static void lease_drop(int idx, const char *why)
{
    close(g_leaseSrv.slot[idx].fd);
    lease_srv_remove(&g_leaseSrv, idx);
    lease_publish(why, idx);
}

// Blocks up to timeoutUs for a connection or a message, handles whatever arrived, and returns. It is the worker's sleep: with
// nothing happening it wakes only for the timeout (the worker's housekeeping), never for the lease itself.
void shell_lease_wait(uint32_t timeoutUs)
{
    if (!g_leaseUp) return;
    fd_set rs;
    FD_ZERO(&rs);
    int maxfd = g_leaseListen;
    FD_SET(g_leaseListen, &rs);
    for (int i = 0; i < LEASE_MAX_SLOTS; i++) {
        int fd = g_leaseSrv.slot[i].fd;
        if (fd >= 0) { FD_SET(fd, &rs); if (fd > maxfd) maxfd = fd; }
    }
    struct timeval tv = { (time_t)(timeoutUs / 1000000u), (suseconds_t)(timeoutUs % 1000000u) };
    int r = select(maxfd + 1, &rs, NULL, NULL, &tv);
    if (r < 0) { usleep(20000); return; }   // never spin on an error
    if (r == 0) return;

    if (FD_ISSET(g_leaseListen, &rs)) {
        int cfd = accept(g_leaseListen, NULL, NULL);
        if (cfd >= 0) {
            int on = 1;
            setsockopt(cfd, SOL_SOCKET, LEASE_SO_NOSIGPIPE, &on, sizeof(on));
            int evicted = -1;
            int idx = (cfd < FD_SETSIZE) ? lease_srv_add(&g_leaseSrv, cfd, &evicted) : -1;
            if (evicted >= 0) close(evicted);
            if (idx < 0) { close(cfd); shell_event("LEASE refused a connection: every slot holds a claim"); }
            else lease_publish("connect", idx);
        }
    }
    for (int i = 0; i < LEASE_MAX_SLOTS; i++) {
        int fd = g_leaseSrv.slot[i].fd;
        if (fd < 0 || !FD_ISSET(fd, &rs)) continue;
        char buf[64];
        ssize_t n = recv(fd, buf, sizeof(buf), LEASE_MSG_DONTWAIT);
        if (n <= 0) { lease_drop(i, n == 0 ? "released(EOF)" : "released(error)"); continue; }
        int changed = 0;
        if (lease_srv_feed(&g_leaseSrv, i, buf, (size_t)n, &changed) != 0) { lease_drop(i, "released(protocol error)"); continue; }
        if (changed) lease_publish(g_leaseSrv.slot[i].claimed ? "claim" : "release", i);
    }
}
