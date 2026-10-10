// The shell feed's file work, split out of main.c: install and repair of ps4_shell_feed.prx and the
// Ambilight payload, sending the payload to GoldHEN's BinLoader, the autoload entry in GoldHEN's
// payload list, and the claim that keeps the shell feed off the strip while this app drives it.
// The code is as it ran on hardware.

#ifndef SHELL_FEED_H
#define SHELL_FEED_H

#include "claim_file.h"      // ClaimRun
#include "install_plan.h"
#include <stdbool.h>
#include <stddef.h>

extern ClaimRun g_claim;

void rescan_shell_files(void);
void sync_shell_summary(void);
bool launch_payload_now(void);
void do_shell_repair(void);
bool read_autoload_on(void);
void autoload_hint(const char *msg);
void toggle_autoload(void);
void on_shell_enabled_changed(void);
int claim_write_cb(void *ctx, const char *text, size_t len);
void claim_start(void);

#endif
