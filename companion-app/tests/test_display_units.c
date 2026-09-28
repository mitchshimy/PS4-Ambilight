#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "settings.h"

// Percent display units are UI-only: storage stays raw. Build (see README):
//   gcc -Iinclude -o test_display_units tests/test_display_units.c source/settings.c source/config.c tests/test_sce_stubs.c
int main(void) {
    // ---- percent display units (UI-only; storage stays raw) ----
    const MenuItem *bri = NULL, *thr = NULL, *sat = NULL, *bal = NULL;
    for (int i = 0; i < kMenuItemCount; i++) {
        if (!strcmp(kMenuItems[i].key, "brightness")) bri = &kMenuItems[i];
        if (!strcmp(kMenuItems[i].key, "auto_letterbox_threshold")) thr = &kMenuItems[i];
        if (!strcmp(kMenuItems[i].key, "saturation")) sat = &kMenuItems[i];
        if (!strcmp(kMenuItems[i].key, "brightness_r")) bal = &kMenuItems[i];
    }
    assert(bri && thr && sat && bal);
    char b[24];
    assert(settings_to_display(bri, 255) == 100);
    assert(settings_to_display(bri, 0) == 0);
    assert(settings_from_display(bri, 100) == 255);
    assert(settings_from_display(bri, 0) == 0);
    assert(settings_from_display(bri, 250) == 255);   // over-range clamps
    assert(settings_from_display(bri, -5) == 0);
    assert(!strcmp(settings_format_display(bri, 100, b, sizeof(b)), "100%"));
    assert(settings_to_display(thr, 18) == 7);         // default threshold reads as 7%
    assert(settings_from_display(thr, 7) == 18);       // ...and 7% maps back to the same stored default
    assert(!strcmp(settings_format_display(sat, 20, b, sizeof(b)), "+20%"));
    assert(!strcmp(settings_format_display(sat, -10, b, sizeof(b)), "-10%"));
    assert(!strcmp(settings_format_display(sat, 0, b, sizeof(b)), "0%"));
    assert(!strcmp(settings_format_display(bal, 100, b, sizeof(b)), "100%"));
    assert(settings_to_display(bal, 150) == 150);      // direct percents are not rescaled
    // every stored value must survive stored -> display -> stored within one display step
    for (int v = 0; v <= 255; v++) {
        int back = settings_from_display(bri, settings_to_display(bri, v));
        assert(abs(back - v) <= 2);
    }
    // every display percent must map to a distinct stored value (nudging can never get stuck)
    int last = -1;
    for (int d = 0; d <= 100; d++) { int st = settings_from_display(bri, d); assert(st > last); last = st; }
    printf("Percent display units: PASSED\n");

    // hidden letterbox debounce keys have no kMenuItems row, so settings_load
    // has to bound them itself -- a bad ini must not get through unclamped
    {
        FILE *f = fopen("/tmp/test_hidden_bounds.ini", "w");
        fprintf(f, "[layout]\nauto_letterbox_stability_frames=9999\nauto_letterbox_check_interval_frames=0\nscan_depth=50\n[color]\nsaturation=175\ncontrast=-500\n");
        fclose(f);
        AmbientConfig c;
        assert(settings_load(&c, "/tmp/test_hidden_bounds.ini"));
        assert(c.autoLetterboxStabilityFrames == 30);
        assert(c.autoLetterboxCheckIntervalFrames == 1);
        assert(c.scanDepth == 4);
        assert(c.saturation == 100); // was 300 max; 175 in an old ini must land on the cap
        assert(c.contrast == -100);
        printf("Hidden-key bounds on load: PASSED\n");
    }
    printf("ALL CHECKS PASSED\n");
    return 0;
}
