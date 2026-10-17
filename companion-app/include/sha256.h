// sha256.h -- minimal streaming SHA-256, no dependencies beyond stdint.h/stddef.h. Public domain...
// More: docs/code-comments.md (companion-app/include/sha256.h:1)
#ifndef PS4_AMBILIGHT_SHA256_H
#define PS4_AMBILIGHT_SHA256_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SHA256_DIGEST_SIZE 32 // bytes
#define SHA256_HEX_SIZE 65    // 64 hex chars + NUL

typedef struct {
    uint8_t  data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} Sha256Ctx;

void sha256_init(Sha256Ctx *ctx);
void sha256_update(Sha256Ctx *ctx, const uint8_t *data, size_t len);
// Writes the 32-byte digest to outDigest. ctx must not be reused after.
void sha256_final(Sha256Ctx *ctx, uint8_t outDigest[SHA256_DIGEST_SIZE]);

// Formats a 32-byte digest as 64 lowercase hex chars + NUL into outHex
// (must be at least SHA256_HEX_SIZE bytes).
void sha256_to_hex(const uint8_t digest[SHA256_DIGEST_SIZE], char outHex[SHA256_HEX_SIZE]);

// Case-insensitive compare of two hex digest strings.
// More: docs/code-comments.md (companion-app/include/sha256.h:36)
bool sha256_hex_matches(const char *hexA, const char *hexWithMaybeSuffix);

#endif // PS4_AMBILIGHT_SHA256_H
