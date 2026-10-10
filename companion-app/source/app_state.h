// What main.c shares with the modules split out of it: the config being edited, the screen state,
// the redraw flag, and the two calls that put something on screen. Defined in main.c.
#ifndef APP_STATE_H
#define APP_STATE_H

#include "settings.h"     // AmbientConfig
#include "ui_screens.h"   // UiState
#include <stdbool.h>

extern AmbientConfig g_cfg;
extern UiState g_state;
extern bool g_dirty;          // something changed: redraw next frame

void set_status(const char *msg, bool isError);   // Home's status line (also sets g_dirty)
void render_and_present(void);                    // draw and show a frame now, for long blocking work

#endif
