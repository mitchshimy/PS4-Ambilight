// data_dir.h -- the one folder every file of ours lives in on the console.
//
// Config, claim file, version marker, reports, update staging and the shell feed .prx all sit in
// APP_DATA_DIR, so finding, backing up or removing the project's files is one folder. Only
// /data/GoldHEN and /data/payloads, which belong to GoldHEN, are outside it.
//
// This is the app's copy of AMBILIGHT_DATA_DIR in plugin/include/ambilight_paths.h (this app's build
// has no ../plugin on its include path). tests/test_claim_file.c fails if the two differ.
#ifndef DATA_DIR_H
#define DATA_DIR_H

#define APP_DATA_DIR "/data/ps4-ambilight"

#endif
