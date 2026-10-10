#include "net_http.h"
#include "github_release_json.h"
#include "plugin_common.h" // ORBIS_O_CREAT_TRUNC_WRONLY -- see its BUG FIX comment

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>

// HTTP downloads and the release lookup. See net_http.h.
//
// Verified real APIs, not guessed: SDK's samples/net_http/main.c is a
// real, working https://www.google.com download demo this sequence is
// copied from -- sceSysmoduleLoadModuleInternal for NET/HTTP/SSL,
// sceNetInit/PoolCreate, sceSslInit, sceHttpInit, then the
// template/connection/request chain below.

static int g_libnetMemId = 0, g_libhttpCtxId = 0, g_libsslCtxId = 0;
static bool g_httpReady = false;

static int skip_ssl_callback(int libsslId, unsigned int verifyErr, void *const sslCert[], int certNum, void *userArg)
{
    (void)libsslId; (void)verifyErr; (void)sslCert; (void)certNum; (void)userArg;
    return 1; // accept -- matches the sample; tighten this if certificate pinning matters later
}

static bool http_init(void)
{
    if (g_httpReady) return true;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0) return false;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0) return false;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0) return false;

    sceNetInit();
    int ret = sceNetPoolCreate("ambientAppNetPool", 4 * 1024, 0);
    if (ret < 0) return false;
    g_libnetMemId = ret;

    ret = sceSslInit(256 * 1024);
    if (ret < 0) return false;
    g_libsslCtxId = ret;

    ret = sceHttpInit(g_libnetMemId, g_libsslCtxId, 4 * 1024 * 1024);
    if (ret < 0) return false;
    g_libhttpCtxId = ret;

    g_httpReady = true;
    return true;
}

// Downloads full_url to local_dst. Returns true on a clean 200 +
// complete read. If outDigest is non-NULL, it's filled with the
// SHA-256 of exactly the bytes written to local_dst, computed
// incrementally as each chunk arrives -- no separate re-read of the
// file afterwards.
bool http_download(const char *full_url, const char *local_dst, uint8_t outDigest[SHA256_DIGEST_SIZE],
                           const char *acceptHeader)
{
    if (!http_init()) return false;

    int tpl = sceHttpCreateTemplate(g_libhttpCtxId, "Mozilla/5.0 (PLAYSTATION 4; 1.00)", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return false;
    sceHttpsSetSslCallback(tpl, skip_ssl_callback, NULL);

    // Follows the small-header 302 from an api.github.com asset URL
    // (or, previously, straight from github.com's web app -- see the
    // BUG FIX comment on GITHUB_RELEASES_LATEST_API_URL in
    // net_http.h for why that direct route no longer works) on to
    // the signed objects.githubusercontent.com URL the asset actually
    // lives at.
    sceHttpSetAutoRedirect(tpl, 1);

    // Bounds how long a hung/unreachable host can freeze the app.
    // Confirmed real 2-arg signatures; NOT confirmed against a
    // working call site in this SDK (no sample calls these), so treat
    // as a real, compilable improvement over no timeout at all, not a
    // confirmed-correct one.
    sceHttpSetConnectTimeOut(tpl, 10 * 1000 * 1000);  // 10s
    sceHttpSetResolveTimeOut(tpl, 10 * 1000 * 1000);  // 10s
    sceHttpSetSendTimeOut(tpl, 10 * 1000 * 1000);     // 10s
    sceHttpSetRecvTimeOut(tpl, 10 * 1000 * 1000);     // 10s -- was missing; a connection that
                                                       // opens then stalls mid-response had no
                                                       // bound at all without this

    bool ok = false;
    int req = -1;
    int conn = sceHttpCreateConnectionWithURL(tpl, full_url, 1);
    if (conn >= 0) {
        req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, full_url, 0);
        if (req >= 0) {
            // NOT YET VERIFIED (same status as sceHttpSetConnectTimeOut
            // et al. above): sceHttpAddRequestHeader's signature is
            // taken from the usual OpenOrbis/orbis-sdk shape
            // (id, name, value, mode) with mode=1 meaning "overwrite
            // any existing value for this header" -- not exercised
            // against a real call site in this SDK snapshot. This is
            // what asks the GitHub asset API for the raw asset (a
            // small-header 302 to objects.githubusercontent.com)
            // instead of its default JSON metadata body.
            if (acceptHeader) sceHttpAddRequestHeader(req, "Accept", acceptHeader, 1);

            if (sceHttpSendRequest(req, NULL, 0) >= 0) {
                int32_t statusCode = 0;
                sceHttpGetStatusCode(req, &statusCode);
                if (statusCode == 200) {
                    // sceKernelOpen/Write/Close, not fopen -- fopen
                    // from an unusual thread context is not reliably
                    // safe in this environment (same fix the plugin
                    // itself needed).
                    int32_t fd = sceKernelOpen(local_dst, ORBIS_O_CREAT_TRUNC_WRONLY /* see plugin_common.h's BUG FIX comment */, 0777);
                    if (fd >= 0) {
                        uint8_t buf[64 * 1024];
                        Sha256Ctx shaCtx;
                        if (outDigest) sha256_init(&shaCtx);
                        ok = true;
                        for (;;) {
                            int n = sceHttpReadData(req, buf, sizeof(buf));
                            if (n < 0) { ok = false; break; }
                            if (n == 0) break;
                            if (sceKernelWrite(fd, buf, n) != n) { ok = false; break; }
                            if (outDigest) sha256_update(&shaCtx, buf, (size_t)n);
                        }
                        sceKernelClose(fd);
                        if (ok && outDigest) sha256_final(&shaCtx, outDigest);
                    }
                }
            }
        }
    }
    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return ok;
}

// Downloads full_url straight into outBuf (NUL-terminated on success),
// for small text responses -- just the checksum sidecar file, which
// is a couple dozen bytes. Same template/connection/request machinery
// as http_download() above, just written into memory instead of a
// file since there's no reason to touch the filesystem for this much
// data. Returns false on any HTTP failure OR if the response would
// overflow outBufSize (including the NUL) -- a checksum file should
// never be anywhere close to outBufSize, so an overflow here almost
// certainly means the URL served something else (an HTML error page,
// a redirect-to-login, etc.), which must not be silently truncated
// and treated as a hash.
bool http_download_text(const char *full_url, char *outBuf, size_t outBufSize, const char *acceptHeader)
{
    if (!http_init() || outBufSize == 0) return false;

    int tpl = sceHttpCreateTemplate(g_libhttpCtxId, "Mozilla/5.0 (PLAYSTATION 4; 1.00)", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return false;
    sceHttpsSetSslCallback(tpl, skip_ssl_callback, NULL);
    // Same redirect fix as http_download() above.
    sceHttpSetAutoRedirect(tpl, 1);
    sceHttpSetConnectTimeOut(tpl, 10 * 1000 * 1000);
    sceHttpSetResolveTimeOut(tpl, 10 * 1000 * 1000);
    sceHttpSetSendTimeOut(tpl, 10 * 1000 * 1000);
    sceHttpSetRecvTimeOut(tpl, 10 * 1000 * 1000);  // was missing -- see http_download()'s copy of this

    bool ok = false;
    int req = -1;
    int conn = sceHttpCreateConnectionWithURL(tpl, full_url, 1);
    if (conn >= 0) {
        req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, full_url, 0);
        if (req >= 0) {
            // See the matching sceHttpAddRequestHeader comment in
            // http_download() above.
            if (acceptHeader) sceHttpAddRequestHeader(req, "Accept", acceptHeader, 1);

            if (sceHttpSendRequest(req, NULL, 0) >= 0) {
                int32_t statusCode = 0;
                sceHttpGetStatusCode(req, &statusCode);
                if (statusCode == 200) {
                    size_t total = 0;
                    ok = true;
                    for (;;) {
                        int n = sceHttpReadData(req, outBuf + total, outBufSize - 1 - total);
                        if (n < 0) { ok = false; break; }
                        if (n == 0) break;
                        total += (size_t)n;
                        if (total >= outBufSize - 1) {
                            // Buffer's full but the server might still
                            // have more to send -- that's the overflow
                            // case described above, so treat it as a
                            // failure rather than guess we got it all.
                            uint8_t probe[1];
                            int extra = sceHttpReadData(req, probe, sizeof(probe));
                            if (extra != 0) ok = false;
                            break;
                        }
                    }
                    if (ok) outBuf[total] = '\0';
                }
            }
        }
    }
    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return ok;
}

// First hop of the api.github.com fix described on
// GITHUB_RELEASES_LATEST_API_URL in net_http.h: fetches
// the latest release's metadata and resolves assetName to that
// asset's own small api.github.com URL, ready to be GETted with
// Accept: application/octet-stream by http_download()/
// http_download_text() to reach the real bytes.
static bool github_resolve_asset_api_url(const char *assetName, char *outUrl, size_t outUrlSize)
{
    // Release JSON is dominated by the (markdown) release-notes body
    // and can run to several KB -- generous vs. the couple-dozen-byte
    // checksum sidecar http_download_text() is more typically used
    // for. Heap-allocated: too large to comfortably put on this
    // thread's stack.
    const size_t kReleaseJsonBufSize = 64 * 1024;
    char *releaseJson = (char *)malloc(kReleaseJsonBufSize);
    if (!releaseJson) return false;

    bool ok = http_download_text(GITHUB_RELEASES_LATEST_API_URL, releaseJson, kReleaseJsonBufSize,
                                  "application/vnd.github+json");
    if (!ok) {
        free(releaseJson);
        return false;
    }

    ok = github_json_find_asset_url(releaseJson, assetName, outUrl, outUrlSize);
    free(releaseJson);
    return ok;
}

// The override: see net_set_dev_base_url() in net_http.h.
static char g_devBaseUrl[200];

void net_set_dev_base_url(const char *url)
{
    snprintf(g_devBaseUrl, sizeof(g_devBaseUrl), "%s", url ? url : "");
}

const char *net_dev_base_url(void) { return g_devBaseUrl; }

// Where to fetch release asset `assetName` from, and which Accept header to send with it
// (GitHub's asset API needs one; a plain file server must not get one).
bool resolve_asset_url(const char *assetName, char *outUrl, size_t outUrlSize, const char **outAccept)
{
    if (g_devBaseUrl[0]) {
        int n = snprintf(outUrl, outUrlSize, "%s/%s", g_devBaseUrl, assetName);
        *outAccept = NULL;
        return n > 0 && (size_t)n < outUrlSize;
    }
    *outAccept = "application/octet-stream";
    return github_resolve_asset_api_url(assetName, outUrl, outUrlSize);
}
