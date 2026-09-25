#ifndef HELP_QR_H
#define HELP_QR_H

#include <stdbool.h>

// The one URL this app ever turns into a QR code: the GitHub README's
// own Help section, kept in sync by hand with README.md's actual
// "## Help" heading (GitHub slugifies that to #help). If that heading
// ever gets renamed, this string has to be updated to match, or the
// QR code will still scan fine but land above/below the section it's
// meant to jump to.
#define HELP_QR_URL "https://github.com/mitchshimy/PS4-Ambilight#help"

// Encodes HELP_QR_URL exactly once (on first call) and caches the
// result for the rest of the process -- the source text is a
// compile-time constant, so there's nothing to invalidate. Returns
// false if encoding somehow fails (it can't, in practice, for a URL
// this short: qrcodegen_encodeText only fails by running out of the
// version range or the buffer it's given, and HELP_QR_URL is a fixed
// ~45 chars against a generous max version -- but main.c still checks
// the return value once at startup rather than assuming).
bool help_qr_init(void);

// Side length in modules (including the quiet-zone border), valid
// only after a successful help_qr_init().
int help_qr_size(void);

// True if module (x, y) is a dark square, false if light. x and y run
// [0, help_qr_size()); out-of-range coordinates are treated as light
// rather than asserting, since the quiet-zone border baked into
// help_qr_size() means callers iterating the full grid never need to
// special-case the edge.
bool help_qr_module(int x, int y);

#endif // HELP_QR_H
