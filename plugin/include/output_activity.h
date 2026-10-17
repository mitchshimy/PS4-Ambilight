// output_activity.h -- tiny host-testable helper for the game-to-shell ownership signal.
#ifndef AMBIENT_OUTPUT_ACTIVITY_H
#define AMBIENT_OUTPUT_ACTIVITY_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

// Returns non-zero and advances the shared counter only when the complete UDP datagram
// was accepted by sendto(). That is transport-submission evidence, not an end-to-end WLED ACK.
static inline int wled_output_note_send(volatile uint32_t *counter,
                                        ssize_t sentBytes,
                                        size_t expectedBytes)
{
    if (counter == NULL || sentBytes < 0 || (size_t)sentBytes != expectedBytes) return 0;
    (*counter)++;
    return 1;
}

#endif
