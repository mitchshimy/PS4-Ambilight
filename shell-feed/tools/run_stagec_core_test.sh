#!/bin/sh
# Stage C verification (merge plan step 13): extracts shell-feed/source/main.c's v17 tiling math
# through shell_process_frame() -- the span that calls into the sister's real, linked
# tiling.c/pixel_formats.c/zones.c/gamma.c/color_processing.c/settings.c/config.c -- by marker (not
# a frozen line-number snapshot, so this stays accurate as main.c changes), compiles it together
# with tools/test_stagec_core.c against those real sister files and a real test ini, and runs it.
#
# This is this project's own equivalent of the research repo's tools/host_zone_check and
# tools/host_ini_check for the post-Stage-C shape: those two assume a self-contained single-file
# probe (Stage A/B's shape) and can no longer run their own harness step against shell-feed's main.c
# now that it legitimately #includes the sister's real headers via a proper Makefile -- their
# provenance check (now comparing only the two sample_thread.c smoothing blocks still intentionally
# pasted, Stage D's concern) still works and should still be run from the research repo for that
# part. This script covers everything Stage C actually changed.
#
# Usage: sh shell-feed/tools/run_stagec_core_test.sh   (run from the repo root)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT

MAIN_C="$REPO/shell-feed/source/main.c"
START_MARKER='// ---- v17-math-begin ----'
END_FUNC='static uint32_t shell_process_frame'

START_LINE=$(grep -n "$START_MARKER" "$MAIN_C" | head -1 | cut -d: -f1)
FUNC_LINE=$(grep -n "^$END_FUNC" "$MAIN_C" | head -1 | cut -d: -f1)
if [ -z "$START_LINE" ] || [ -z "$FUNC_LINE" ]; then
    echo "FAIL: could not find the v17-math-begin marker or shell_process_frame in $MAIN_C" >&2
    exit 1
fi
# end = the first line at or after FUNC_LINE+1 that is exactly "}" at column 0
END_LINE=$(awk -v start="$FUNC_LINE" 'NR>start && $0=="}" {print NR; exit}' "$MAIN_C")
if [ -z "$END_LINE" ]; then
    echo "FAIL: could not find shell_process_frame's closing brace" >&2
    exit 1
fi
sed -n "${START_LINE},${END_LINE}p" "$MAIN_C" > "$T/extracted_core.c"

TESTINI="$T/live_test.ini"
mkdir -p "$T/inc"
sed "s#\"/data/ps4_ambient_light.ini\"#\"$TESTINI\"#" "$REPO/plugin/include/ambient_internal.h" > "$T/inc/ambient_internal.h"
cp "$REPO/plugin/include/"*.h "$T/inc/" 2>/dev/null || true
sed "s#\"/data/ps4_ambient_light.ini\"#\"$TESTINI\"#" "$REPO/plugin/include/ambient_internal.h" > "$T/inc/ambient_internal.h"

cp "$HERE/test_stagec_core_fixture.ini" "$TESTINI"
cp "$HERE/test_stagec_core.c" "$T/driver.c"

# driver.c pulls extracted_core.c in with #include "extracted_core.c" (textual inclusion, so the
# two share one translation unit) -- "-I $T" is what lets that #include find it. extracted_core.c is
# NOT passed to gcc as its own source file; that would compile and link it a second time.
gcc -w -D__FINAL__=1 \
    -I "$REPO/tools/host_stubs" -I "$T/inc" -I "$REPO/common" -I "$T" \
    -o "$T/drv" "$T/driver.c" \
    "$REPO/plugin/source/settings.c" "$REPO/plugin/source/config.c" \
    "$REPO/plugin/source/zones.c" "$REPO/plugin/source/gamma.c" \
    "$REPO/plugin/source/color_processing.c" "$REPO/plugin/source/pixel_formats.c" \
    "$REPO/plugin/source/tiling.c" -lm

"$T/drv"
