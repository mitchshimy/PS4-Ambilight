#!/bin/sh
# Renders Home and Set up to PNG with the real UI code. Run from companion-app/.
# Needs gcc, FreeType headers and (for the PNG conversion) Python with Pillow.
set -e
out=${1:-/tmp/ui_preview}
mkdir -p "$out"
SRC="source/ui_screens.c source/ui_canvas.c source/ui_theme.c source/ui_text.c source/ui_icons.c source/ui_widgets.c \
     source/led_frame.c source/layout.c source/presets.c source/settings.c source/config.c source/color_pipeline.c \
     source/help_qr.c source/qrcodegen.c"
# config.c includes <orbis/libkernel.h>; a one-header stand-in is enough on a PC (the file calls
# are served by tests/test_sce_stubs.c, same as the host tests).
mkdir -p "$out/stub/orbis"
cat > "$out/stub/orbis/libkernel.h" <<'H'
#include <stdint.h>
#include <sys/types.h>
int32_t sceKernelOpen(const char*, int, int);
int64_t sceKernelLseek(int32_t, int64_t, int);
ssize_t sceKernelRead(int32_t, void*, size_t);
ssize_t sceKernelWrite(int32_t, const void*, size_t);
int sceKernelClose(int32_t);
H
gcc -O1 -DUI_TEXT_DESKTOP_HARNESS -w -Isource -Iinclude -I../plugin/include -I"$out/stub" -I/usr/include/freetype2 \
    -o "$out/ui_preview" tools/ui_preview.c $SRC tests/test_sce_stubs.c -lfreetype -lm
"$out/ui_preview" "$out" assets/fonts
python3 - "$out" <<'PY'
import sys, glob
from PIL import Image
for p in glob.glob(sys.argv[1] + "/*.ppm"):
    Image.open(p).save(p[:-4] + ".png")
PY
ls "$out"/*.png
