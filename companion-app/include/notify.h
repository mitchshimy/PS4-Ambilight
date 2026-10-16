// notify.h -- a line in the console's own notification bar, the same bar the plugin and the shell feed
// use ("PS4 Ambilight: shell feed is waiting: no WLED address is set..."), for the few things a person
// should see even when the app's own screen is not where they are looking.
#ifndef NOTIFY_H
#define NOTIFY_H

#include <stdbool.h>

// essential = the strip can't work, or the person is being misled, without it: sent whatever
// [notifications] enabled says (the Notifications switch on Set up). Everything else is information and respects that switch
// (on by default; the same rule as the plugin's say() and the shell feed's sh_notify()).
void app_notify(bool essential, const char *fmt, ...);

#endif
