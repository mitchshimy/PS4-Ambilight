// Where things live on the console and what the release assets are called, split out of main.c so
// the modules that install and check files share one copy. Verbatim from main.c.
#ifndef APP_PATHS_H
#define APP_PATHS_H

#include "data_dir.h"
#include "net_http.h"   // GITHUB_REPO_OWNER / GITHUB_REPO_NAME, used by PLUGIN_UPDATE_URL

// Every file of ours lives in APP_DATA_DIR (data_dir.h); only /data/GoldHEN and /data/payloads, which...
// More: docs/code-comments.md (companion-app/include/app_paths.h:9)
#define AMBIENT_CONFIG_PATH APP_DATA_DIR "/ps4_ambient_light.ini"
#define FONT_DIR "/app0/assets/fonts"

// Where the plugin binary and GoldHEN's own plugin registry live.
// More: docs/code-comments.md (companion-app/include/app_paths.h:16)
#define GOLDHEN_PLUGINS_DIR "/data/GoldHEN/plugins"
#define GOLDHEN_PLUGINS_INI "/data/GoldHEN/plugins.ini"
#define PLUGIN_PRX_PATH GOLDHEN_PLUGINS_DIR "/ps4_ambient_light.prx"

// The GitHub repo and release API URL (and the BUG FIX note on why the API is used) are in
// net_http.h.

// Plain human-facing URL, used only in log/status text (e.g. "open
// this in a browser to check") -- never fetched directly anymore.
#define PLUGIN_UPDATE_URL "https://github.com/" GITHUB_REPO_OWNER "/" GITHUB_REPO_NAME "/releases/latest/download/ps4_ambient_light.prx"

#define PLUGIN_ASSET_NAME "ps4_ambient_light.prx"

// Companion checksum asset -- must be uploaded to the SAME GitHub release as the .prx above, named by...
// More: docs/code-comments.md (companion-app/include/app_paths.h:32)
#define PLUGIN_CHECKSUM_ASSET_NAME PLUGIN_ASSET_NAME ".sha256"
#define PLUGIN_CHECKSUM_URL PLUGIN_UPDATE_URL ".sha256"

// Local sidecar recording the checksum of whatever build this app itself last installed at...
// More: docs/code-comments.md (companion-app/include/app_paths.h:56)
#define PLUGIN_INSTALLED_CHECKSUM_PATH PLUGIN_PRX_PATH ".sha256"

// Shell feed files (shell-feed/) Three files make up an install: the plugin above, the shell feed's...
// More: docs/code-comments.md (companion-app/include/app_paths.h:67)
#define SHELL_DIR             APP_DATA_DIR
#define SHELL_PRX_ASSET_NAME  "ps4_shell_feed.prx"
#define SHELL_PRX_PATH        SHELL_DIR "/" SHELL_PRX_ASSET_NAME
#define PAYLOADS_DIR          "/data/payloads"
#define PAYLOAD_ASSET_NAME    "ps4-ambilight.bin"
#define PAYLOAD_PATH          PAYLOADS_DIR "/" PAYLOAD_ASSET_NAME
// The Ambilight payload is downloaded next to the .prx, NOT into /data/payloads: a failed download must not
// leave a half-written ".new" file sitting in a folder that payload menus list.
#define PAYLOAD_STAGING_PATH  SHELL_DIR "/" PAYLOAD_ASSET_NAME ".new"
#define SHELL_PRX_STAGING_PATH SHELL_PRX_PATH ".new"
// GoldHEN's own list of payloads to run when it loads. Confirmed on a console. If a GoldHEN version
// ever names it differently, this one line is the only thing to change.
#define GOLDHEN_PAYLOADS_INI  "/data/GoldHEN/payloads.ini"
#define GOLDHEN_DIR           "/data/GoldHEN"
#define GOLDHEN_CONFIG_INI    "/data/GoldHEN/config.ini"   // [PluginLoader] PluginLoader_Enabled, see goldhen_config.h
// How GoldHEN's own UI writes the entry (the console sees /data as /user/data). payload_ini.h
// matches an existing entry by file name, so a line added by hand in the other spelling is found too.
#define PAYLOAD_AUTORUN_ENTRY "/user/data/payloads/" PAYLOAD_ASSET_NAME

// sceKernelMkdir is the SDK's POSIX-mirroring name, but no code in this repo has called it before.
// If it fails to link, change this one macro to POSIX mkdir() (<sys/stat.h>), which does the same.
#define APP_MKDIR(path) sceKernelMkdir((path), 0777)

#endif
