// fw_check.h -- the firmware warning. See source/fw_check.c.
#ifndef FW_CHECK_H
#define FW_CHECK_H

// Reads the firmware once and sets g_state.fwTitle/fwBody when the current loader is untested on that
// version or has no entry for it. Informational: it never blocks loading.
void firmware_check(void);

#endif
