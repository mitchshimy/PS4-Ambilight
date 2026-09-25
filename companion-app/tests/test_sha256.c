#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "include/sha256.h"

static void check(const char *label, const uint8_t *data, size_t len, const char *expectedHex)
{
    Sha256Ctx ctx;
    uint8_t digest[SHA256_DIGEST_SIZE];
    char hex[SHA256_HEX_SIZE];

    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
    sha256_to_hex(digest, hex);

    printf("%s: %s\n", label, hex);
    assert(strcmp(hex, expectedHex) == 0);
}

int main(void)
{
    // Standard SHA-256 test vectors (empty string and "abc"; both
    // cross-checked against Python's hashlib.sha256, not typed from
    // memory).
    check("sha256(\"\")", (const uint8_t *)"", 0,
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    check("sha256(\"abc\")", (const uint8_t *)"abc", 3,
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // Multi-block input (forces sha256_update across internal 64-byte
    // transform boundaries) -- three concatenated calls, verified
    // against Python's hashlib.sha256(rep * 3).
    const char *rep = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    Sha256Ctx ctx;
    uint8_t digest[SHA256_DIGEST_SIZE];
    char hex[SHA256_HEX_SIZE];
    sha256_init(&ctx);
    for (int i = 0; i < 3; ++i) sha256_update(&ctx, (const uint8_t *)rep, strlen(rep));
    sha256_final(&ctx, digest);
    sha256_to_hex(digest, hex);
    printf("sha256(3x multi-block): %s\n", hex);
    assert(strcmp(hex, "50ea825d9684f4229ca29f1fec511593e281e46a140d81e0005f8f688669a06c") == 0);

    // sha256_hex_matches: case-insensitivity and sha256sum-style
    // "<hex>  filename" suffix tolerance.
    const char *lower = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    const char *upper = "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";
    const char *withSuffix = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  ps4_ambient_light.prx\n";
    const char *wrong = "0000000000000000000000000000000000000000000000000000000000000000000000000000";
    assert(sha256_hex_matches(lower, upper));
    assert(sha256_hex_matches(lower, withSuffix));
    assert(!sha256_hex_matches(lower, wrong));
    printf("sha256_hex_matches: PASSED\n");

    printf("All sha256 tests PASSED\n");
    return 0;
}
