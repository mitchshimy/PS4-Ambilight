#!/bin/sh
# Builds and runs tests/bench_ui_scroll.c on the host. Run from companion-app/. Optional arg: optimisation flag (default -O2).
set -e
out=${TMPDIR:-/tmp}/ui_scroll_check; mkdir -p "$out/stub/orbis"
cat > "$out/stub/orbis/libkernel.h" <<'H'
#include <stdint.h>
#include <sys/types.h>
int32_t sceKernelOpen(const char*, int, int);
int64_t sceKernelLseek(int32_t, int64_t, int);
ssize_t sceKernelRead(int32_t, void*, size_t);
ssize_t sceKernelWrite(int32_t, const void*, size_t);
int sceKernelClose(int32_t);
H
SRC="source/ui_screens.c source/ui_canvas.c source/ui_theme.c source/ui_text.c source/ui_icons.c source/ui_widgets.c \
     source/led_frame.c source/layout.c source/presets.c source/settings.c source/config.c source/color_pipeline.c \
     source/help_qr.c source/qrcodegen.c"
gcc ${1:--O2} -DUI_TEXT_DESKTOP_HARNESS -w -Isource -Iinclude -I../plugin/include -I"$out/stub" -I/usr/include/freetype2 \
    -o "$out/bench" tests/bench_ui_scroll.c $SRC tools/sce_file_stubs.c -lfreetype -lm
"$out/bench" assets/fonts
