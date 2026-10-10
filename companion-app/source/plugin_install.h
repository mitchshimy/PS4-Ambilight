// Installing, enabling and checking the ambient light plugin, and reading GoldHEN's plugin loader
// switch, split out of main.c. The code is as it ran on hardware.

#ifndef PLUGIN_INSTALL_H
#define PLUGIN_INSTALL_H

#include "ui_screens.h"     // UiInstallState
#include "goldhen_config.h" // PluginLoaderState
#include <stdbool.h>

UiInstallState plugin_registration_state(void);
void do_plugin_update(void);
void do_plugin_enable(void);
PluginLoaderState read_plugin_loader_state(void);
void refresh_plugin_loader_state(void);

#endif
