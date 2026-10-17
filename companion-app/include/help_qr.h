#ifndef HELP_QR_H
#define HELP_QR_H

#include <stdbool.h>

// The one URL this app ever turns into a QR code: the GitHub README's own Help section, kept in sync...
// More: docs/code-comments.md (companion-app/include/help_qr.h:6)
#define HELP_QR_URL "https://github.com/mitchshimy/PS4-Ambilight#help"

// Encodes HELP_QR_URL exactly once (on first call) and caches the result for the rest of the process...
// More: docs/code-comments.md (companion-app/include/help_qr.h:14)
bool help_qr_init(void);

// Side length in modules (including the quiet-zone border), valid
// only after a successful help_qr_init().
int help_qr_size(void);

// True if module (x, y) is a dark square, false if light. x and y run [0, help_qr_size())...
// More: docs/code-comments.md (companion-app/include/help_qr.h:28)
bool help_qr_module(int x, int y);

#endif // HELP_QR_H
