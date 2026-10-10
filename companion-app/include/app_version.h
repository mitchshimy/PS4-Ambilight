// The companion app's version: the one place to change it.
//
// It is shown on the Help screen and becomes APP_VER and VERSION in the package's param.sfo. Both
// build paths read this line: .github/workflows/CI.yml (sed) and build.bat (findstr). Keep it a
// single "#define APP_VERSION" line in the form "N.NN", at most 8 characters (the SFO field).
// tests/test_app_version.c checks the form.
// #pragma once, not an include guard named APP_VERSION_*: build.bat finds the version line by
// its "#define APP_VERSION" prefix, and a guard would match it too.
#pragma once

#define APP_VERSION "1.00"

_Static_assert(sizeof(APP_VERSION) <= 9, "APP_VERSION must fit the 8 character SFO field");

