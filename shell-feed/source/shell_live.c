// shell_live.c -- live_send(): the send loop.
// More: docs/code-comments.md (shell-feed/source/shell_live.c:1)
#include "shell_internal.h"

static int      g_liveSendEverStarted = 0; // [v23-fix v44] set after live_send()'s first-ever config
                                            // read; makes every later entry (i.e. every resume out of wait_for_reenable()) log as INI RELOAD and...
                                            // More: docs/code-comments.md (shell-feed/source/shell_live.c:7)

// [v18] Continuous live send: one socket, LIVE_HZ, TSC-paced.
// More: docs/code-comments.md (shell-feed/source/shell_live.c:15)
#if DO_LIVE_DDP

int live_send(void)
{
    if (g_nBuf == 0) {
        printf(TAG " LIVE: no display buffers found, nothing to send\n");
        snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "no display buffers");
        return 0;
    }

    // [v22] The ini first: it decides the destination, the pace, the zone count and every colour setting.
    // More: docs/code-comments.md (shell-feed/source/shell_live.c:32)
    int resuming = g_liveSendEverStarted;
    g_liveSendEverStarted = 1;
    int cfgOk = shell_cfg_load();
    snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "%s", ""); // [v24] we got past the buffer check
    buildZoneGeometry();
    g_smoothedRgbValid = false;
    {
        int64_t sz = 0; uint32_t h = 0;
        if (ambient_get_real_config_size_and_hash(&sz, &h)) { g_cfgLastSize = sz; g_cfgLastHash = h; g_cfgHaveBaseline = 1; }
        if (resuming && cfgOk) g_cfgReloads++;
        const char *why = cfgOk ? (resuming ? "RELOAD" : "LOAD")
                                 : (resuming ? "RELOAD FAILED (keeping prior settings)" : "LOAD FAILED (compiled defaults in use)");
        shell_cfg_log(why, cfgOk ? sz : 0, cfgOk ? h : 0);
    }
    sh_runtime_gates_note(); // [v24]

    char curHost[64]; int curPort;
    strncpy(curHost, g_config.wledHost[0] != '\0' ? g_config.wledHost : WLED_HOST, sizeof(curHost) - 1);
    curHost[sizeof(curHost) - 1] = '\0';
    curPort = g_config.wledPort != 0 ? (int)g_config.wledPort : (int)WLED_PORT;
    if (curHost[0] == '\0') {
        printf(TAG " LIVE: no wled_host in the ini (and no compiled-in default), nothing to send\n");
        snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "no destination host");
        g_liveSendEverStarted = 0; // never actually started: the start after wait_for_destination() is a real first LOAD
        return 0;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { printf(TAG " LIVE: socket() failed (%d)\n", sock); snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "socket() failed"); return 0; }
    struct timeval to = { 0, 5000 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
    struct sockaddr_in d; memset(&d, 0, sizeof(d));
    d.sin_family = AF_INET; d.sin_port = htons((uint16_t)curPort);
    if (inet_pton(AF_INET, curHost, &d.sin_addr) != 1) {
        printf(TAG " LIVE: inet_pton failed for host \"%s\"\n", curHost);
        snprintf(g_shNotStartedWhy, sizeof(g_shNotStartedWhy), "destination is not an address");
        g_liveSendEverStarted = 0; // same: not started yet
        close(sock);
        return 0;
    }
    printf(TAG " LIVE: sending to %s:%d, %u Hz, until reboot, buffers found=%d, zones=%u\n",
           curHost, curPort, (unsigned)g_config.updateFrequencyHz, g_nBuf, (unsigned)g_numZones);
    g_shLiveSessions++; g_shState = SH_LIVE; g_shStopReason = 0; // [v24]
    if (!resuming) sh_notify(false, "shell feed running (sending to %s).", curHost); // [v47] first start this boot; a resume says so itself
    { char m[100]; snprintf(m, sizeof(m), "LIVE start zones=%u hz=%u buffers=%d session=%u",
                            (unsigned)g_numZones, (unsigned)g_config.updateFrequencyHz, g_nBuf, (unsigned)g_shLiveSessions); sh_dev_send(m); }

    shell_relay_update(1); // wled-relay: this feed now holds the strip (no-op unless relay_signal_enabled and relay_host are set)

    static uint8_t pk[10 + 3 * MAX_TOTAL_ZONES];
    static uint8_t rgbTriplets[3 * MAX_TOTAL_ZONES];
    pk[0] = 0x41; pk[1] = 0; pk[2] = 0x0B; pk[3] = 0x01; pk[4] = pk[5] = pk[6] = pk[7] = 0;

    uint32_t hz = g_config.updateFrequencyHz;
    uint64_t periodTsc = (g_tscHz > 0) ? (g_tscHz / hz) : 0;
    uint64_t nextTick = rdtsc17();
    g_liveStartTsc = nextTick;   // Foreground-gate transition lines print ms since here
    // [v23] no endTsc: the loop runs until a clean stop (disable file, shell enabled=false) or process exit.
    uint64_t nextRowTsc = nextTick + 5 * g_tscHz, rows = 0;
    uint64_t nextEnabledPollTsc = nextTick; // [v46] due immediately on the first tick
    uint64_t nextBeatTsc = nextTick;        // heartbeat for the game plugin, due immediately
    uint64_t beatCounter = 0;
    uint64_t k = 0, sent = 0, skipped = 0, buf0 = 0, buf1 = 0, gameOwned = 0;
    uint64_t sumUs = 0, maxUs = 0, minUs = 0xFFFFFFFFu, rowTicks = 0;
    int stopReason = 0; // [v23-fix] set just before each break below; 0 here only guards against a logic error

    for (;; k++) {

        if (periodTsc > 0) {
            uint64_t now = rdtsc17();
            if (now < nextTick) {
                uint64_t waitTsc = nextTick - now;
                uint32_t waitUs = (uint32_t)(waitTsc * 1000000ULL / g_tscHz);
                if (waitUs > 0 && waitUs < 1000000) usleep(waitUs);
            }
            nextTick += periodTsc;
        } else {
            usleep(1000000 / hz); // [v18] fallback if TSC calibration failed: fixed-rate, will drift
        }

        // [v22] The REST of the ini (brightness, zones, host, ...), re-checked on the sister's own schedule...
        // More: docs/code-comments.md (shell-feed/source/shell_live.c:117)
        {
            int32_t dfd = sceKernelOpen(DISABLE_FILE, 0 /* O_RDONLY */, 0);
            if (dfd >= 0) { sceKernelClose(dfd); printf(TAG " DISABLED: disable file found (" DISABLE_FILE ").\n"); sh_dev_send("DISABLED: disable file"); stopReason = 1; break; }
        }
        // [v46] [shell] enabled= checked on its own 1 s cadence, independent of config_reload_check_seconds...
        // More: docs/code-comments.md (shell-feed/source/shell_live.c:126)
        shell_enabled_poll(rdtsc17(), g_tscHz, &nextEnabledPollTsc);
        shell_relay_tick(rdtsc17(), g_tscHz); // wled-relay: repeat "on" while held (no-op unless relay_signal_enabled)
        shell_heartbeat(rdtsc17(), g_tscHz, &nextBeatTsc, ++beatCounter); // runs every tick, writes once a second, including while the game owns the strip
        if (!g_shellEnabled) { printf(TAG " DISABLED: shell disabled in ini ([shell] enabled=false).\n"); sh_dev_send("DISABLED: [shell] enabled=false"); stopReason = 2; break; }

        if (shell_cfg_poll(rdtsc17(), g_tscHz)) {
            sh_runtime_gates_note(); // [v24]
            shell_relay_update(1);   // relay_* keys may have changed in this reload
            hz = g_config.updateFrequencyHz;
            periodTsc = (g_tscHz > 0) ? (g_tscHz / hz) : 0;
            nextTick = rdtsc17() + periodTsc;
            const char *nh = g_config.wledHost[0] != '\0' ? g_config.wledHost : WLED_HOST;
            int np = g_config.wledPort != 0 ? (int)g_config.wledPort : (int)WLED_PORT;
            if (strcmp(nh, curHost) != 0 || np != curPort) {
                struct sockaddr_in nd; memset(&nd, 0, sizeof(nd));
                nd.sin_family = AF_INET; nd.sin_port = htons((uint16_t)np);
                if (inet_pton(AF_INET, nh, &nd.sin_addr) == 1) {
                    d = nd; strncpy(curHost, nh, sizeof(curHost) - 1); curHost[sizeof(curHost) - 1] = '\0'; curPort = np;
                    printf(TAG " LIVE: destination is now %s:%d\n", curHost, curPort);
                } else {
                    printf(TAG " LIVE: new host \"%s\" is not an address; still sending to %s:%d\n", nh, curHost, curPort);
                }
            }
        }

        int rowDue = (g_tscHz > 0 && rdtsc17() >= nextRowTsc);

        // [v20] The sister plugin's claim, checked every tick (cheap: a TSC compare unless it's actually time...
        // More: docs/code-comments.md (shell-feed/source/shell_live.c:156)
        if (claim_poll(rdtsc17(), g_tscHz)) {
            gameOwned++;
            if (rowDue) {
                rows++; nextRowTsc += 5 * g_tscHz;
                printf(TAG " LIVE t=%llus: sent=%llu skipped=%llu gameOwned=%llu buf0=%llu buf1=%llu (foreground gate active)\n",
                       (unsigned long long)(rows * 5), (unsigned long long)sent, (unsigned long long)skipped,
                       (unsigned long long)gameOwned, (unsigned long long)buf0, (unsigned long long)buf1);
                sh_publish(sent, skipped, gameOwned); // [v24]
                { char m[110]; snprintf(m, sizeof(m), "LIVE t=%llus sent=%llu skipped=%llu gameOwned=%llu (foreground gate active)",
                                        (unsigned long long)(rows * 5), (unsigned long long)sent, (unsigned long long)skipped, (unsigned long long)gameOwned); sh_dev_send(m); }
                sumUs = 0; maxUs = 0; minUs = 0xFFFFFFFFu; rowTicks = 0;
            }
            continue;
        }

        int32_t idx = g_lastFlipIndex;
        int b = (g_nBuf == 2 && idx == 1) ? 1 : 0; // default buf0 for idx==-1/0/anything else
        if (b == 0) buf0++; else buf1++;

        uint64_t t0 = rdtsc17();
        int bufReady = (g_nBuf > 0) && buf_ok(&g_buf[b]); // [v24] g_nBuf can be 0 after a failed re-scan
#if SHELL_SELF_HEAL
        {
            uint64_t nowMs = g_tscHz > 1000 ? rdtsc17() / (g_tscHz / 1000u) : (k * 1000u / (hz > 0 ? hz : 30u));
            if (heal_tick(&g_heal, bufReady, nowMs)) {
                sh_do_heal();
                nextTick = rdtsc17() + periodTsc; // the re-scan took a while: re-pace rather than burst
                skipped++; continue;
            }
        }
#endif
        if (!bufReady) { skipped++; continue; }
        {
            static int handoffDiagFirstBuffer = 0;
            if (!handoffDiagFirstBuffer) {
                handoffDiagFirstBuffer = 1;
                shell_event("HANDOFF_DIAG SHELL_FIRST_READY_BUFFER index=%d buffers=%d tsc=%llu hz=%llu", b, g_nBuf,
                       (unsigned long long)sceKernelGetProcessTimeCounter(),
                       (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
            }
        }
        uint32_t sampleIntervalUs = 1000000u / hz;
        uint32_t nz = shell_process_frame(g_buf[b].start, sampleIntervalUs, rgbTriplets);
        int dataSize = (int)(3 * nz);
        pk[8] = (uint8_t)((dataSize >> 8) & 255); pk[9] = (uint8_t)(dataSize & 255);
        memcpy(pk + 10, rgbTriplets, (size_t)dataSize);
        ssize_t handoffSendBytes = sendto(sock, pk, 10 + dataSize, 0, (struct sockaddr *)&d, sizeof(d));
        {
            static int handoffDiagFirstSend = 0;
            if (!handoffDiagFirstSend) {
                handoffDiagFirstSend = 1;
                shell_event("HANDOFF_DIAG SHELL_FIRST_DDP_RESULT bytes=%lld expected=%d zones=%u tsc=%llu hz=%llu",
                       (long long)handoffSendBytes, 10 + dataSize, (unsigned)nz,
                       (unsigned long long)sceKernelGetProcessTimeCounter(),
                       (unsigned long long)sceKernelGetProcessTimeCounterFrequency());
            }
        }
        uint64_t elapsedTsc = rdtsc17() - t0;
        uint32_t elapsedUs = (g_tscHz > 0) ? (uint32_t)(elapsedTsc * 1000000ULL / g_tscHz) : 0;
        sumUs += elapsedUs; if (elapsedUs > maxUs) maxUs = elapsedUs; if (elapsedUs < minUs) minUs = elapsedUs;
        sent++; rowTicks++;

        if (rowDue) { // every 5 s
            rows++; nextRowTsc += 5 * g_tscHz;
            uint32_t za = 0, zb = nz / 3, zc = (2 * nz) / 3;
            printf(TAG " LIVE t=%llus: sent=%llu skipped=%llu gameOwned=%llu buf0=%llu buf1=%llu avg=%lluus min=%lluus max=%lluus"
                   " first3=%02x%02x%02x/%02x%02x%02x/%02x%02x%02x zones=%u hz=%u reloads=%u\n",
                   (unsigned long long)(rows * 5), (unsigned long long)sent, (unsigned long long)skipped,
                   (unsigned long long)gameOwned, (unsigned long long)buf0, (unsigned long long)buf1,
                   rowTicks ? (unsigned long long)(sumUs / rowTicks) : 0ULL, (unsigned long long)minUs, (unsigned long long)maxUs,
                   rgbTriplets[za*3], rgbTriplets[za*3+1], rgbTriplets[za*3+2],
                   rgbTriplets[zb*3], rgbTriplets[zb*3+1], rgbTriplets[zb*3+2],
                   rgbTriplets[zc*3], rgbTriplets[zc*3+1], rgbTriplets[zc*3+2],
                   (unsigned)nz, (unsigned)hz, (unsigned)g_cfgReloads);
            sh_publish(sent, skipped, gameOwned); // [v24]
            { char m[150]; snprintf(m, sizeof(m), "LIVE t=%llus sent=%llu skipped=%llu gameOwned=%llu avg=%lluus max=%lluus zones=%u hz=%u reloads=%u heals=%u",
                                    (unsigned long long)(rows * 5), (unsigned long long)sent, (unsigned long long)skipped, (unsigned long long)gameOwned,
                                    rowTicks ? (unsigned long long)(sumUs / rowTicks) : 0ULL, (unsigned long long)maxUs,
                                    (unsigned)nz, (unsigned)hz, (unsigned)g_cfgReloads, (unsigned)g_heal.heals); sh_dev_send(m); }
            sumUs = 0; maxUs = 0; minUs = 0xFFFFFFFFu; rowTicks = 0;
        }
    }

    close(sock);
    shell_relay_update(0); // wled-relay: this feed lets go; "off" only if no game holds the strip
    sh_publish(sent, skipped, gameOwned); // [v24]
    g_shStopReason = stopReason;
    {
        uint64_t elapsedSec = g_tscHz > 0 ? (rdtsc17() - g_liveStartTsc) / g_tscHz : k / (hz > 0 ? hz : 30);
        printf(TAG " LIVE done. elapsed=%llus ticks=%llu sent=%llu skipped=%llu (%.1f%%) gameOwned=%llu (%.1f%%) buf0=%llu buf1=%llu"
               " sister_ever_seen=%d ini_reloads=%u\n",
               (unsigned long long)elapsedSec,
               (unsigned long long)k, (unsigned long long)sent, (unsigned long long)skipped,
               k ? 100.0 * (double)skipped / (double)k : 0.0,
               (unsigned long long)gameOwned, k ? 100.0 * (double)gameOwned / (double)k : 0.0,
               (unsigned long long)buf0, (unsigned long long)buf1, g_claimEverSeen, (unsigned)g_cfgReloads);
        { char m[100]; snprintf(m, sizeof(m), "LIVE done elapsed=%llus sent=%llu skipped=%llu stop=%d",
                                (unsigned long long)elapsedSec, (unsigned long long)sent, (unsigned long long)skipped, stopReason); sh_dev_send(m); }
    }
    return stopReason; // [v23-fix] 1 (disable file) or 2 (enabled=false); 0 would mean the loop was left without
                        // taking either break above, which should not happen -- left as a visible 0 rather than
                        // asserting, so a future bug here shows as "stopped permanently" instead of a crash.
}

#endif
