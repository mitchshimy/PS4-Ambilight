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

// HTTP downloads and the release lookup.
// More: docs/code-comments.md (companion-app/source/net_http.c:15)

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

// Downloads full_url to local_dst.
// More: docs/code-comments.md (companion-app/source/net_http.c:56)
bool http_download(const char *full_url, const char *local_dst, uint8_t outDigest[SHA256_DIGEST_SIZE],
                           const char *acceptHeader)
{
    if (!http_init()) return false;

    int tpl = sceHttpCreateTemplate(g_libhttpCtxId, "Mozilla/5.0 (PLAYSTATION 4; 1.00)", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return false;
    sceHttpsSetSslCallback(tpl, skip_ssl_callback, NULL);

    // Follows the small-header 302 from an api.github.com asset URL (or, previously, straight from...
    // More: docs/code-comments.md (companion-app/source/net_http.c:70)
    sceHttpSetAutoRedirect(tpl, 1);

    // Bounds how long a hung/unreachable host can freeze the app.
    // More: docs/code-comments.md (companion-app/source/net_http.c:78)
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
            // NOT YET VERIFIED (same status as sceHttpSetConnectTimeOut et al. above): sceHttpAddRequestHeader's...
            // More: docs/code-comments.md (companion-app/source/net_http.c:96)
            if (acceptHeader) sceHttpAddRequestHeader(req, "Accept", acceptHeader, 1);

            if (sceHttpSendRequest(req, NULL, 0) >= 0) {
                int32_t statusCode = 0;
                sceHttpGetStatusCode(req, &statusCode);
                if (statusCode == 200) {
                    // sceKernelOpen/Write/Close, not fopen -- fopen from an unusual thread context is not reliably safe in...
                    // More: docs/code-comments.md (companion-app/source/net_http.c:111)
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

// Downloads full_url straight into outBuf (NUL-terminated on success), for small text responses...
// More: docs/code-comments.md (companion-app/source/net_http.c:141)
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
                            // Buffer's full but the server might still have more to send -- that's the overflow case described...
                            // More: docs/code-comments.md (companion-app/source/net_http.c:188)
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

// First hop of the api.github.com fix described on GITHUB_RELEASES_LATEST_API_URL in net_http.h...
// More: docs/code-comments.md (companion-app/source/net_http.c:209)
static bool github_resolve_asset_api_url(const char *assetName, char *outUrl, size_t outUrlSize)
{
    // Release JSON is dominated by the (markdown) release-notes body and can run to several KB -- generous...
    // More: docs/code-comments.md (companion-app/source/net_http.c:217)
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
