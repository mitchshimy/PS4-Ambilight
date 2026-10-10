// Downloads over HTTP(S) and the lookup of where a release asset lives. Split out of main.c; the
// code is as it ran on hardware. Everything here may be called from the update-check thread except
// net_set_dev_base_url(), which main.c calls from the main thread only (see below).
#ifndef NET_HTTP_H
#define NET_HTTP_H

#include "sha256.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// BUG FIX (this file's own install log, 0x80431073 == SCE_HTTP_ERROR_
// TOO_LARGE_RESPONSE_HEADER): a "/releases/latest/download/..." link
// is served by github.com's web app, and the redirect response it
// sends back carries that app's full header set (a several-KB
// content-security-policy header alone, plus strict-transport-
// security, vary, x-github-request-id, ...). sceHttp has a small
// fixed header-parsing buffer and rejects that response outright,
// before sceHttpSetAutoRedirect(tpl, 1) in net_http.c ever gets a chance to
// act on the redirect -- which is why this always worked in a browser
// (no such fixed buffer) and always failed here. api.github.com is a
// plain JSON API, not the web app, and doesn't send those headers, so
// the fix is the standard two-hop pattern instead of hitting
// github.com directly: (1) look up the release's asset list here,
// (2) GET the resolved asset's own small api.github.com URL with
// Accept: application/octet-stream, which 302s (small headers) to a
// signed, short-lived objects.githubusercontent.com URL that
// sceHttpSetAutoRedirect follows the rest of the way. See
// github_resolve_asset_api_url() in net_http.c and do_plugin_update() in main.c.
#define GITHUB_REPO_OWNER "mitchshimy"
#define GITHUB_REPO_NAME  "PS4-Ambilight"
#define GITHUB_RELEASES_LATEST_API_URL \
    "https://api.github.com/repos/" GITHUB_REPO_OWNER "/" GITHUB_REPO_NAME "/releases/latest"

// Downloads full_url to local_dst. Returns true on a clean 200 + complete read. If outDigest is
// non-NULL it is filled with the SHA-256 of exactly the bytes written to local_dst, computed as each
// chunk arrives. acceptHeader may be NULL.
bool http_download(const char *full_url, const char *local_dst, uint8_t outDigest[SHA256_DIGEST_SIZE],
                   const char *acceptHeader);

// Downloads full_url into outBuf (NUL-terminated on success), for small text such as a checksum
// sidecar. Returns false on any HTTP failure or if the response would not fit outBufSize.
bool http_download_text(const char *full_url, char *outBuf, size_t outBufSize, const char *acceptHeader);

// Where to fetch release asset `assetName` from, and which Accept header to send with it (GitHub's
// asset API needs one; a plain file server must not get one). Uses the dev override when one is set.
bool resolve_asset_url(const char *assetName, char *outUrl, size_t outUrlSize, const char **outAccept);

// The [dev] update_base_url override, cached. main.c reads it from the ini on the MAIN thread only
// (config.c reads the file with fopen(), which this codebase does not trust from a worker thread,
// see the note on http_download() in net_http.c) and hands it over with net_set_dev_base_url(); the
// update-check thread only reads it, through resolve_asset_url() and net_dev_base_url().
// net_dev_base_url() is "" when downloads come from GitHub.
void net_set_dev_base_url(const char *url);
const char *net_dev_base_url(void);

#endif
