// Small file helpers and checksums over the SDK's sceKernelOpen/Read/Write, split out of main.c. The...
// More: docs/code-comments.md (companion-app/source/file_util.c:1)

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <orbis/libkernel.h>

#include "app_paths.h"
#include "app_state.h"
#include "sha256.h"
#include "plugin_common.h" // ORBIS_O_CREAT_TRUNC_WRONLY -- see its BUG FIX comment
#include "file_util.h"

// Size in bytes, or -1 if it can't be opened. Same open/lseek idiom as the plugins.ini reader below
// (stat() is not trusted on this SDK -- see the plugin side).
long file_size_of(const char *path)
{
    int32_t fd = sceKernelOpen(path, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return -1;
    long size = sceKernelLseek(fd, 0, SEEK_END);
    sceKernelClose(fd);
    return size;
}

// Whole file into a malloc'd, NUL-terminated buffer (caller frees). NULL if it can't be opened,
// is empty, or is implausibly large for the small ini files this is used on.
char *read_small_file(const char *path, size_t *outLen)
{
    *outLen = 0;
    int32_t fd = sceKernelOpen(path, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return NULL;
    long size = sceKernelLseek(fd, 0, SEEK_END);
    sceKernelLseek(fd, 0, SEEK_SET);
    if (size <= 0 || size > 256 * 1024) { sceKernelClose(fd); return NULL; }
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { sceKernelClose(fd); return NULL; }
    long n = sceKernelRead(fd, buf, (size_t)size);
    sceKernelClose(fd);
    if (n <= 0) { free(buf); return NULL; }
    buf[n] = '\0';
    *outLen = (size_t)n;
    return buf;
}

// Writes `len` bytes to `path` by way of "<path>.new" + fsync + rename, so a failure part way
// never leaves a half-written file where GoldHEN (or anything else) will read it.
bool write_file_atomic(const char *path, const char *data, size_t len)
{
    char staging[256];
    if (snprintf(staging, sizeof(staging), "%s.new", path) >= (int)sizeof(staging)) return false;
    int32_t fd = sceKernelOpen(staging, ORBIS_O_CREAT_TRUNC_WRONLY, 0777);
    if (fd < 0) return false;
    bool ok = (len == 0) || (sceKernelWrite(fd, data, len) == (long)len);
    if (ok && fsync(fd) < 0) ok = false;
    sceKernelClose(fd);
    if (!ok) { sceKernelUnlink(staging); return false; }
    if (sceKernelRename(staging, path) < 0) { sceKernelUnlink(staging); return false; }
    return true;
}

// Hashes an on-disk file with the same buffered-read pattern used throughout this file...
// More: docs/code-comments.md (companion-app/source/file_util.c:66)
bool sha256_hex_of_file(const char *path, char *outHex, size_t outHexSize)
{
    if (outHexSize < SHA256_HEX_SIZE) return false;
    int32_t fd = sceKernelOpen(path, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return false;

    Sha256Ctx ctx;
    sha256_init(&ctx);
    uint8_t buf[64 * 1024];
    bool ok = true;
    for (;;) {
        long n = sceKernelRead(fd, buf, sizeof(buf));
        if (n < 0) { ok = false; break; }
        if (n == 0) break;
        sha256_update(&ctx, buf, (size_t)n);
    }
    sceKernelClose(fd);
    if (!ok) return false;

    uint8_t digest[SHA256_DIGEST_SIZE];
    sha256_final(&ctx, digest);
    sha256_to_hex(digest, outHex);
    return true;
}

// Reads the plain 64-hex-char PLUGIN_INSTALLED_CHECKSUM_PATH sidecar (no sha256sum-style " filename"...
// More: docs/code-comments.md (companion-app/source/file_util.c:97)
bool read_installed_checksum(char *outHex, size_t outHexSize)
{
    if (outHexSize < SHA256_HEX_SIZE) return false;
    int32_t fd = sceKernelOpen(PLUGIN_INSTALLED_CHECKSUM_PATH, 0 /* O_RDONLY */, 0777);
    if (fd < 0) return false;
    long n = sceKernelRead(fd, outHex, outHexSize - 1);
    sceKernelClose(fd);
    if (n < 64) return false; // too short to be a real digest
    outHex[n] = '\0';
    return true;
}

// Writes actualHex -- the plain, already-verified digest computed in do_plugin_update() -- to...
// More: docs/code-comments.md (companion-app/source/file_util.c:115)
void write_installed_checksum(const char *actualHex)
{
    int32_t fd = sceKernelOpen(PLUGIN_INSTALLED_CHECKSUM_PATH, ORBIS_O_CREAT_TRUNC_WRONLY /* see plugin_common.h's BUG FIX comment */, 0777);
    if (fd < 0) return;
    sceKernelWrite(fd, actualHex, strlen(actualHex));
    sceKernelClose(fd);
}
