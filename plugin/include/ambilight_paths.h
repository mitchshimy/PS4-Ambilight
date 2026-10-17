// ambilight_paths.h -- the one folder everything this project writes on the console lives in.
//
// Config, claim file, version marker, reports, update staging and the shell feed's files all sit in
// AMBILIGHT_DATA_DIR, so finding, backing up or removing the project's files is one folder. Nothing
// of ours goes loose in /data. What stays outside, because other software owns the location:
// /data/GoldHEN/ (plugins, plugins.ini, config.ini, payloads.ini) and /data/payloads/ (where
// GoldHEN looks for payload .bin files).
//
// Every other path macro (AMBIENT_CONFIG_PATH, STATE_PATH, REPORT_PATH_PREFIX, VERSION_MARKER,
// DISABLE_FILE, CLAIM_PATH, ...) is AMBILIGHT_DATA_DIR "/file", so moving the folder is this one
// line. The companion app builds with only its own include/ and keeps a matching copy in
// companion-app/include/app_paths.h; companion-app/tests/test_claim_file.c fails if they differ.
//
// The folder must exist before the first write. The companion app, the plugin (plugin_load) and
// the shell feed's installer each create it; creating an existing folder is not an error.
#ifndef AMBILIGHT_PATHS_H
#define AMBILIGHT_PATHS_H

#define AMBILIGHT_DATA_DIR "/data/ps4-ambilight"

#endif
