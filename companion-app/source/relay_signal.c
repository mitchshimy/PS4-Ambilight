// relay_signal.c -- same one-shot-UDP-datagram approach ddp.c already uses for the real strip data...
// More: docs/code-comments.md (companion-app/source/relay_signal.c:1)

#include "relay_signal.h"
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int g_relaySockfd = -1;

static int ensure_relay_socket(void)
{
    if (g_relaySockfd >= 0) return g_relaySockfd;
    g_relaySockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_relaySockfd >= 0) {
        struct timeval sndTimeout = { .tv_sec = 0, .tv_usec = 5000 };
        setsockopt(g_relaySockfd, SOL_SOCKET, SO_SNDTIMEO, &sndTimeout, sizeof(sndTimeout));
    }
    return g_relaySockfd;
}

bool relay_send_external_source(const char *host, uint16_t port, bool active)
{
    int sockfd = ensure_relay_socket();
    if (sockfd < 0) return false;

    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &destAddr.sin_addr) != 1) return false;

    const char *payload = active ? "on" : "off";
    sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
    return true;
}
