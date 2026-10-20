#!/bin/sh
# companion-app/include/lease.h must be a byte-for-byte copy of plugin/include/lease.h (the app's build only has its own include folder).
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
cmp "$root/plugin/include/lease.h" "$root/companion-app/include/lease.h" && echo "lease.h copies match"
