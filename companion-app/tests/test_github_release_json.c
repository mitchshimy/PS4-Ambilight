// Run from companion-app/. Build:
//   gcc -w -Iinclude -o /tmp/test_github_release_json tests/test_github_release_json.c && /tmp/test_github_release_json
//
// github_json_find_asset_url() (include/github_release_json.h) pulls one asset's API URL out of
// GitHub's "latest release" JSON with a hand-rolled scan, not a JSON parser. These feed it
// release JSON shaped like GitHub's (an assets array, each asset listing "url" before "name", with
// a nested "uploader" object) and check what it picks, and what it refuses.
#include "github_release_json.h"
#include <assert.h>
#include <string.h>

#define A1 "https://api.github.com/repos/o/r/releases/assets/101"
#define A2 "https://api.github.com/repos/o/r/releases/assets/102"
#define A3 "https://api.github.com/repos/o/r/releases/assets/103"
#define A4 "https://api.github.com/repos/o/r/releases/assets/104"

#define ASSET(url, id, name) \
    "{\"url\":\"" url "\",\"id\":" id ",\"name\":\"" name "\",\"label\":null," \
    "\"uploader\":{\"login\":\"x\",\"url\":\"https://api.github.com/users/x\"}," \
    "\"content_type\":\"application/octet-stream\",\"size\":10}"

static const char *kRelease =
    "{\"url\":\"https://api.github.com/repos/o/r/releases/1\",\"tag_name\":\"v1\","
    "\"body\":\"notes with { braces } and \\\"quotes\\\"\",\"assets\":["
    ASSET(A1, "101", "ps4_ambient_light.prx.sha256") ","
    ASSET(A2, "102", "ps4_ambient_light.prx") ","
    ASSET(A3, "103", "ps4-ambilight-900.bin") ","
    ASSET(A4, "104", "ps4-ambilight-900.bin.sha256")
    "]}";

static bool find(const char *json, const char *name, char *out, size_t n)
{
    return github_json_find_asset_url(json, name, out, n);
}

int main(void)
{
    char url[160];

    // Each name resolves to its own asset's url, never a neighbour's (the uploader's url sits
    // between this asset's "url" and the next one's).
    assert(find(kRelease, "ps4_ambient_light.prx", url, sizeof(url)) && !strcmp(url, A2));
    assert(find(kRelease, "ps4_ambient_light.prx.sha256", url, sizeof(url)) && !strcmp(url, A1));
    assert(find(kRelease, "ps4-ambilight-900.bin", url, sizeof(url)) && !strcmp(url, A3));
    assert(find(kRelease, "ps4-ambilight-900.bin.sha256", url, sizeof(url)) && !strcmp(url, A4));
    printf("each asset name resolves to its own url: PASSED\n");

    // A name that is only a prefix of another asset's name is not a match.
    assert(!find("{\"assets\":[" ASSET(A1, "101", "ps4_ambient_light.prx.sha256") "]}",
                 "ps4_ambient_light.prx", url, sizeof(url)));
    assert(!find(kRelease, "ps4_shell_feed.prx", url, sizeof(url)));
    assert(!find("", "ps4_ambient_light.prx", url, sizeof(url)));
    assert(!find("{}", "ps4_ambient_light.prx", url, sizeof(url)));
    printf("prefixes, missing assets and empty input are refused: PASSED\n");

    // The spaced form ("name": "x") works too, for both fields.
    const char *spaced =
        "{\"assets\": [{\"url\": \"" A1 "\", \"id\": 1, \"name\": \"ps4-ambilight-900.bin\"}]}";
    assert(find(spaced, "ps4-ambilight-900.bin", url, sizeof(url)) && !strcmp(url, A1));
    printf("the spaced JSON form works: PASSED\n");

    // The output buffer is respected: an exact fit works, one byte short fails and writes nothing
    // beyond what it says.
    size_t need = strlen(A2) + 1;
    char exact[sizeof(A2)];
    assert(need == sizeof(exact));
    assert(find(kRelease, "ps4_ambient_light.prx", exact, sizeof(exact)) && !strcmp(exact, A2));
    char tight[sizeof(A2) - 1];
    assert(!find(kRelease, "ps4_ambient_light.prx", tight, sizeof(tight)));
    printf("output size is respected: PASSED\n");

    // An asset whose object has no "url" before its "name" is refused, not matched to another's.
    assert(!find("{\"assets\":[{\"name\":\"ps4-ambilight-900.bin\",\"id\":1}]}",
                 "ps4-ambilight-900.bin", url, sizeof(url)));
    printf("an asset with no url is refused: PASSED\n");

    printf("ALL GITHUB RELEASE JSON CHECKS PASSED\n");
    return 0;
}
