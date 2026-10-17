// The companion app's version: the one place to change it.
// More: docs/code-comments.md (companion-app/include/app_version.h:1)
#pragma once

#define APP_VERSION "1.01"

_Static_assert(sizeof(APP_VERSION) <= 9, "APP_VERSION must fit the 8 character SFO field");

