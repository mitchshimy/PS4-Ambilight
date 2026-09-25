// sha256.h -- minimal streaming SHA-256, no dependencies beyond
// stdint.h/stddef.h. Public domain implementation (Brad Conte's
// crypto-algorithms, adapted): https://github.com/B-Con/crypto-algorithms
//
// Added for verifying a downloaded plugin .prx against a published
// checksum before it's ever staged/installed (see do_plugin_update()
// in main.c) -- neither OpenOrbis nor the GoldHEN SDK expose a hash
// primitive we could find, so this is self-contained rather than
// pulled from libSceSsl/libSceHttp.
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

// Case-insensitive compare of two hex digest strings. Returns true if
// they represent the same 32 bytes. Tolerant of a trailing filename
// after the hex (the common "<hash>  filename" sha256sum format) --
// only the first 64 hex chars of `hexWithMaybeSuffix` are considered,
// so callers don't need to pre-trim a downloaded checksum file.
bool sha256_hex_matches(const char *hexA, const char *hexWithMaybeSuffix);

#endif // PS4_AMBILIGHT_SHA256_H
