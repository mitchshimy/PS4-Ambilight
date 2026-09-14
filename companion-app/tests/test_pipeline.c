#include <stdio.h>
#include "include/color_pipeline.h"

int main(void) {
    AmbientConfig cfg = {0};
    cfg.brightness = 255;
    cfg.gammaLutIndex = 0;
    cfg.saturation = 0;
    cfg.colorOrder = ORDER_RGB;
    cfg.blackLevel = 0;
    cfg.whiteLevel = 100;
    cfg.contrast = 0;
    cfg.brightnessR = cfg.brightnessG = cfg.brightnessB = 100;
    cfg.gammaR = cfg.gammaG = cfg.gammaB = 100;

    colorpipeline_rebuild_perchannel_gamma_luts(&cfg);

    // default/no-op config: input should pass through unchanged
    uint8_t out[3];
    colorpipeline_process(&cfg, 128, 64, 32, out);
    printf("no-op passthrough: in=(128,64,32) out=(%d,%d,%d) -- expect (128,64,32)\n", out[0], out[1], out[2]);
    if (out[0] != 128 || out[1] != 64 || out[2] != 32) { printf("FAIL\n"); return 1; }

    // saturation=-100 should desaturate toward luma
    cfg.saturation = -100;
    colorpipeline_process(&cfg, 220, 40, 40, out);
    printf("full desaturate: in=(220,40,40) out=(%d,%d,%d) -- should be gray (equal channels)\n", out[0], out[1], out[2]);
    if (!(out[0]==out[1] && out[1]==out[2])) { printf("FAIL\n"); return 2; }
    cfg.saturation = 0;

    // color order remap check
    colorpipeline_write_color_ordered(10, 20, 30, ORDER_GRB, out);
    printf("GRB remap of (R=10,G=20,B=30): wire bytes=(%d,%d,%d) -- expect (20,10,30)\n", out[0], out[1], out[2]);
    if (out[0]!=20 || out[1]!=10 || out[2]!=30) { printf("FAIL\n"); return 3; }

    cfg.gammaLutIndex = 4; // gamma 2.2
    colorpipeline_process(&cfg, 128, 128, 128, out);
    printf("gamma 2.2 at 128: out=(%d,%d,%d) -- expect (56,56,56) per earlier Python verification\n", out[0], out[1], out[2]);
    if (out[0] != 56) { printf("FAIL\n"); return 4; }
    cfg.gammaLutIndex = 0;

    printf("ALL CHECKS PASSED\n");
    return 0;
}
