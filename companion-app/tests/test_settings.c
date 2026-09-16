#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "include/settings.h"

int main(void) {
    AmbientConfig cfg1;
    settings_set_defaults(&cfg1);
    printf("Menu item count: %d\n", kMenuItemCount);
    assert(kMenuItemCount == 35); // +3 for Relay Signal/Relay Host/Relay Port

    // save defaults to a temp file, reload, confirm round-trip
    assert(settings_save(&cfg1, "/tmp/test_ambient.ini"));

    AmbientConfig cfg2;
    bool loaded = settings_load(&cfg2, "/tmp/test_ambient.ini");
    printf("load result: %d\n", loaded);
    assert(loaded);

    assert(strcmp(cfg1.wledHost, cfg2.wledHost) == 0);
    assert(cfg1.wledPort == cfg2.wledPort);
    assert(strcmp(cfg1.relayHost, cfg2.relayHost) == 0);
    assert(cfg1.relayPort == cfg2.relayPort);
    assert(cfg1.relaySignalEnabled == cfg2.relaySignalEnabled);
    assert(cfg1.ledCountTop == cfg2.ledCountTop);
    assert(cfg1.startCorner == cfg2.startCorner);
    assert(cfg1.gammaLutIndex == cfg2.gammaLutIndex);
    assert(cfg1.colorOrder == cfg2.colorOrder);
    assert(cfg1.smoothingEnabled == cfg2.smoothingEnabled);
    printf("Round-trip defaults: PASSED\n");

    // now change several values, verify save/load reflects the changes
    cfg1.wledPort = 5000;
    strcpy(cfg1.wledHost, "10.0.0.55");
    cfg1.startCorner = CORNER_TOP_RIGHT;
    cfg1.direction = DIR_COUNTERCLOCKWISE;
    cfg1.colorOrder = ORDER_GRB;
    cfg1.gammaLutIndex = 4;
    cfg1.saturation = -50;
    cfg1.smoothingEnabled = 1;
    cfg1.ledOffset = -12;
    strcpy(cfg1.relayHost, "10.0.0.99");
    cfg1.relayPort = 9999;
    cfg1.relaySignalEnabled = true;
    assert(settings_save(&cfg1, "/tmp/test_ambient2.ini"));

    AmbientConfig cfg3;
    assert(settings_load(&cfg3, "/tmp/test_ambient2.ini"));
    assert(cfg3.wledPort == 5000);
    assert(strcmp(cfg3.wledHost, "10.0.0.55") == 0);
    assert(cfg3.startCorner == CORNER_TOP_RIGHT);
    assert(cfg3.direction == DIR_COUNTERCLOCKWISE);
    assert(cfg3.colorOrder == ORDER_GRB);
    assert(cfg3.gammaLutIndex == 4);
    assert(cfg3.saturation == -50);
    assert(cfg3.smoothingEnabled == 1);
    assert(cfg3.ledOffset == -12);
    assert(strcmp(cfg3.relayHost, "10.0.0.99") == 0);
    assert(cfg3.relayPort == 9999);
    assert(cfg3.relaySignalEnabled == true);
    printf("Round-trip with modified values: PASSED\n");

    // partial ini (only network section) -- everything else should be default
    FILE *f = fopen("/tmp/test_partial.ini", "w");
    fprintf(f, "[network]\nwled_host=1.2.3.4\nwled_port=1234\n");
    fclose(f);
    AmbientConfig cfg4;
    assert(settings_load(&cfg4, "/tmp/test_partial.ini"));
    assert(strcmp(cfg4.wledHost, "1.2.3.4") == 0);
    assert(cfg4.wledPort == 1234);
    assert(cfg4.ledCountTop == 73); // default, since layout section absent
    assert(cfg4.brightness == 255); // default
    assert(strcmp(cfg4.relayHost, "192.168.2.115") == 0); // default, relay_host absent
    assert(cfg4.relayPort == 24689); // default
    assert(cfg4.relaySignalEnabled == false); // default
    printf("Partial ini falls back to defaults correctly: PASSED\n");

    // generic get/set via MenuItem offsets
    for (int i = 0; i < kMenuItemCount; i++) {
        if (kMenuItems[i].type == FIELD_STRING) continue;
        int32_t before = settings_get_i32(&cfg1, &kMenuItems[i]);
        settings_set_i32(&cfg1, &kMenuItems[i], kMenuItems[i].min);
        int32_t after = settings_get_i32(&cfg1, &kMenuItems[i]);
        if (after != kMenuItems[i].min) { printf("FAIL generic set on %s: got %d expect %d\n", kMenuItems[i].label, after, kMenuItems[i].min); return 1; }
        (void)before;
    }
    printf("Generic offset-based get/set: PASSED for all %d numeric/enum/bool items\n", kMenuItemCount);

    printf("ALL CHECKS PASSED\n");
    return 0;
}
