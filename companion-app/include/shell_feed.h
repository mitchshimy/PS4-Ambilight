// The shell feed's file work, split out of main.c: install and repair of ps4_shell_feed.prx and the...
// More: docs/code-comments.md (companion-app/include/shell_feed.h:1)

#ifndef SHELL_FEED_H
#define SHELL_FEED_H

#include "claim_file.h"      // ClaimRun
#include "install_plan.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "lease.h"
extern ClaimRun g_claim;
extern LeaseClient g_appLease;
bool lease_app_tick(uint32_t nowMs, bool driving);  // every frame: keeps the loopback lease to the payload; true while it carries the claim
void lease_app_shutdown(void);                      // on exit: release and close

void rescan_shell_files(void);
void sync_shell_summary(void);
bool launch_payload_now(void);
void shell_alive_poll(uint32_t nowMs);   // call every frame; updates g_state.shellFeedRunning / shellFeedStarting
void do_shell_repair(void);
bool read_autoload_on(void);
void autoload_hint(const char *msg);
void toggle_autoload(void);
void on_shell_enabled_changed(void);
int claim_write_cb(void *ctx, const char *text, size_t len);
void claim_start(void);

#endif
