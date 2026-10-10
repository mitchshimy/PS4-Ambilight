// Small file helpers and checksums over the SDK's sceKernelOpen/Read/Write, split out of main.c.
// The code is as it ran on hardware (stat() and fopen() are not trusted on this SDK, see the note
// on http_download() in net_http.c).

#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include "sha256.h"
#include <stdbool.h>
#include <stddef.h>

long file_size_of(const char *path);
char *read_small_file(const char *path, size_t *outLen);
bool write_file_atomic(const char *path, const char *data, size_t len);
bool sha256_hex_of_file(const char *path, char *outHex, size_t outHexSize);
bool read_installed_checksum(char *outHex, size_t outHexSize);
void write_installed_checksum(const char *actualHex);

#endif
