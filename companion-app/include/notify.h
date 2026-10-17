// notify.h -- a line in the console's own notification bar, the same bar the plugin and the shell feed...
// More: docs/code-comments.md (companion-app/include/notify.h:1)
#ifndef NOTIFY_H
#define NOTIFY_H

#include <stdbool.h>

// essential = the strip can't work, or the person is being misled, without it: sent whatever...
// More: docs/code-comments.md (companion-app/include/notify.h:9)
void app_notify(bool essential, const char *fmt, ...);

#endif
