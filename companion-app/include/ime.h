// The on-screen keyboard for the numeric and string rows of the settings screens, split out of
// main.c. It knows nothing about main.c's globals: everything it needs comes in through ImeHost.
//
// Flow: Cross on a FIELD_STRING / FIELD_U32 / FIELD_I32 / FIELD_U16 row calls ime_open_field();
// every frame while ime_is_open(), the main loop calls ime_update(), which does nothing until the
// system keyboard closes, then applies (or discards) what was typed and reports through
// host->status. ime_shutdown() closes a dialog that is still open when the app quits.
#ifndef IME_H
#define IME_H

#include "settings.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    AmbientConfig *cfg;        // the config being edited; ime_update() writes the typed value here
    int32_t        userId;     // the PS4 user the dialog belongs to (from pad_init)
    float          centerX;    // where the dialog opens: the middle of the frame
    float          centerY;
    void         (*status)(const char *msg, bool isError);   // the status line (main.c's set_status)
} ImeHost;

// Opens the keyboard for kMenuItems[itemIndex]. Does nothing if a keyboard or another system
// dialog is already up; reports through host->status if the system refuses to open it.
void ime_open_field(int itemIndex, const ImeHost *host);

bool ime_is_open(void);

// Call once per frame while ime_is_open(). Returns true on the frame the dialog closed (typed value
// applied, rejected, or cancelled), so the caller can redraw; false while it is still up.
bool ime_update(const ImeHost *host);

// Closes the dialog if it is open; safe to call when it is not.
void ime_shutdown(void);

#endif
