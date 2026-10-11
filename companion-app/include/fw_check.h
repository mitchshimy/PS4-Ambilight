// fw_check.h -- the firmware warning. See source/fw_check.c.
#ifndef FW_CHECK_H
#define FW_CHECK_H

// Reads the firmware once and sets g_state.fwTitle/fwBody (both empty = nothing to warn about).
void firmware_check(void);

#endif
