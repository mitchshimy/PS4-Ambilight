// Run from companion-app/. Build:
//   gcc -w -Iinclude -o /tmp/test_app_version tests/test_app_version.c && /tmp/test_app_version
//
// APP_VERSION feeds the package's APP_VER and VERSION (CI and build.bat both parse the header
// with a pattern, see app_version.h) and the Help screen. This keeps its form what both expect.
#include "app_version.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *v = APP_VERSION;
    size_t n = strlen(v);
    assert(n >= 3 && n <= 8);                 // the SFO field holds 8
    const char *dot = strchr(v, '.');
    assert(dot && dot != v && dot[1] != 0);   // N.NN: digits, one dot, digits
    assert(strchr(dot + 1, '.') == NULL);
    for (size_t i = 0; i < n; i++)
        assert(isdigit((unsigned char)v[i]) || &v[i] == dot);
    printf("APP_VERSION \"%s\": PASSED\n", v);
    printf("ALL APP VERSION CHECKS PASSED\n");
    return 0;
}
