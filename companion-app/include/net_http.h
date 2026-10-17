// Downloads over HTTP(S) and the lookup of where a release asset lives.
// More: docs/code-comments.md (companion-app/include/net_http.h:1)
#ifndef NET_HTTP_H
#define NET_HTTP_H

#include "sha256.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Never GET github.com's /releases/latest/download directly: its huge redirect headers overflow sceHttp's header
// buffer (0x80431073). Resolve the asset via api.github.com first, then follow its small 302. [docs/code-comments.md]
#define GITHUB_REPO_OWNER "mitchshimy"
#define GITHUB_REPO_NAME  "PS4-Ambilight"
#define GITHUB_RELEASES_LATEST_API_URL \
    "https://api.github.com/repos/" GITHUB_REPO_OWNER "/" GITHUB_REPO_NAME "/releases/latest"

// Downloads full_url to local_dst.
// More: docs/code-comments.md (companion-app/include/net_http.h:35)
bool http_download(const char *full_url, const char *local_dst, uint8_t outDigest[SHA256_DIGEST_SIZE],
                   const char *acceptHeader);

// Downloads full_url into outBuf (NUL-terminated on success), for small text such as a checksum
// sidecar. Returns false on any HTTP failure or if the response would not fit outBufSize.
bool http_download_text(const char *full_url, char *outBuf, size_t outBufSize, const char *acceptHeader);

// Where to fetch release asset `assetName` from, and which Accept header to send with it (GitHub's
// asset API needs one; a plain file server must not get one). Uses the dev override when one is set.
bool resolve_asset_url(const char *assetName, char *outUrl, size_t outUrlSize, const char **outAccept);

// The [dev] update_base_url override, cached. main.c reads it from the ini on the MAIN thread only...
// More: docs/code-comments.md (companion-app/include/net_http.h:49)
void net_set_dev_base_url(const char *url);
const char *net_dev_base_url(void);

#endif
