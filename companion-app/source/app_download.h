// Downloading the newest companion app .pkg for the console's own package installer. The app can't
// install a package itself, so the download is as far as it goes.

#ifndef APP_DOWNLOAD_H
#define APP_DOWNLOAD_H

// Fetches the latest release's .pkg into /data/pkg (made when missing), checks it against the published checksum and
// leaves one copy there. Sets g_state.appPkgPath on success, and the status line either way. Blocks.
void do_app_update_download(void);

#endif
