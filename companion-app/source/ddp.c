// ddp.c -- copied/adapted from ps4_ambient_light's wled_send_rgb_zones.
// The packet header bytes (0x41, 0x00, 0x0B, 0x01, offset=0) are NOT
// arbitrary -- they're the exact DDP header this whole project has
// used since detile_verify_probe's very first capture, verified
// against real WLED behavior throughout. Copied here unchanged rather
// than re-derived from the DDP spec, same "reuse, don't reinvent"
// rule this project has followed since the Makefile/config.c reuse
// in the plugin itself.

#include "ddp.h"
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static int g_sockfd = -1;

static int ensure_socket(void)
{
    if (g_sockfd >= 0) return g_sockfd;
    g_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sockfd >= 0) {
        struct timeval sndTimeout = { .tv_sec = 0, .tv_usec = 5000 };
        setsockopt(g_sockfd, SOL_SOCKET, SO_SNDTIMEO, &sndTimeout, sizeof(sndTimeout));
    }
    return g_sockfd;
}

bool ddp_send_rgb_zones(const char *host, uint16_t port, const uint8_t *rgbTriplets, int numZones)
{
    int sockfd = ensure_socket();
    if (sockfd < 0) return false;

    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &destAddr.sin_addr) != 1) return false;

    if (numZones > DDP_MAX_ZONES) numZones = DDP_MAX_ZONES;
    int dataSize = numZones * 3;
    uint8_t packet[DDP_HEADER_SIZE + 3 * DDP_MAX_ZONES];
    packet[0] = 0x40 | 0x01;
    packet[1] = 0;
    packet[2] = 0x0B;
    packet[3] = 0x01;
    packet[4] = 0; packet[5] = 0; packet[6] = 0; packet[7] = 0; // pixel offset 0
    packet[8] = (uint8_t)((dataSize >> 8) & 0xFF);
    packet[9] = (uint8_t)(dataSize & 0xFF);
    memcpy(packet + DDP_HEADER_SIZE, rgbTriplets, dataSize);

    sendto(sockfd, packet, DDP_HEADER_SIZE + dataSize, 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
    return true;
}
