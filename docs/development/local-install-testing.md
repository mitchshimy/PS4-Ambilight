# Testing the install flow without a release

The companion app downloads three files: `ps4_ambient_light.prx`, `ps4_shell_feed.prx` and
`ps4-ambilight.bin`, each with a `<name>.sha256` beside it. To try that on a console without
publishing anything to GitHub, serve them from your PC.

1. Build the plugin and the shell feed (`make -C plugin`, `make -C shell-feed`). The payload is
   already in `payloads/`.
2. On the PC, from the repo root:

   ```
   python tools/serve_local_release.py
   ```

   It finds the built files (or take `--plugin`, `--shell`, `--payload` paths), writes the
   checksums, serves `ps4-ambilight-app.version` (this tree's `APP_VERSION`, so no notice; pass
   `--app-version 9.99` to see the "App update available" notice on Home, `--app-version none` to
   leave the file out), prints the address, and logs every request. The PC and the console must be on the
   same network, and the PC's firewall must allow the port (default 8000).
3. On the console, add to `/data/ps4-ambilight/ps4_ambient_light.ini`:

   ```
   [dev]
   update_base_url=http://<the address it printed>:8000
   ```

   The app reads it each time it checks or downloads, so no restart is needed. The status line says
   "from the local server" while it's in use, and the log prints `[update-source]`.
4. Delete that line to go back to GitHub.

## What to try

| Do this | Expect |
|---|---|
| With GoldHEN's plugin loader off (Settings > GoldHEN > Plugin Settings), open the app | Home says "Plugin loader is off" in the plugin banner and the button reads "Close app to turn on plugin loader"; Test strip, Install and Update are not offered. |
| Press that button | The app closes (saving its settings like Options does). `/data/GoldHEN/config.ini` is not touched. |
| With the app closed, tick Enable Plugins Loader in GoldHEN, then open the app | Home no longer mentions the loader and the usual button returns. |
| Turn it off in GoldHEN's menu while the app is in the background, then come back | Home shows the loader banner again within about two seconds. |
| Start a game with the loader on | The plugin loads. |
| Shell feed off (the default) | Home and everything else exactly as before. No requests for the shell files in the server log. |
| Set up, Shell feed on | Home gets a Shell feed banner on the left. Nothing installed: "Shell feed not installed", button "Install shell feed" (or "Install all (3)" if the plugin is missing too). Test strip is gone. |
| Press the button | `/data/ps4-ambilight` and `/data/payloads` are created if missing; the `.prx` then the Ambilight payload are downloaded, checked and moved into place; Test strip comes back. Other files in `/data/payloads` are untouched. |
| Delete `/data/ps4-ambilight/ps4_shell_feed.prx` over FTP, go to Home | "Shell feed needs repair", button "Repair shell feed", Test strip hidden. |
| Leave the folder empty | Same: Test strip is never offered while a file is missing. |
| Rebuild one file and restart the server | "Update available" for that file only; the button names it. Two or three files: "Update all (n)". |
| Stop the server | Nothing is reported as an update (can't tell); installed files still count as installed. |
| Set up, Autoload at boot on, then open `/data/GoldHEN/payloads.ini` | `/user/data/payloads/ps4-ambilight.bin = 1` under `[AutoRun]`, other lines unchanged. Off sets it to `= 0`. |
| Press Autoload at boot while Shell feed is off, the plugin or the Ambilight payload is missing | The save bar shows what to do first (amber, replacing "Changes apply after saving."); the next button press clears it. Home's status line is unchanged. |
| Set up, press **Launch Ambilight payload** (Shell feed card) with everything installed | The button reads "Launch Ambilight payload" (not "Reset ..."). The status says the Ambilight payload was sent to GoldHEN and the shell feed lights the strip without a reboot. Pressing it again while it runs is harmless. |
| Press **Launch Ambilight payload** with the Shell feed off, the plugin missing or disabled, or a file missing | The save bar says what to fix first; nothing is sent. |
| Turn Shell feed off in Set up (don't save yet) with the shell feed running | The strip doesn't flicker; it stays claimed until you press Save (or quit). |
| Press the PS button while on Set up, Customization or Test strip | The app stops sending and `/data/ps4-ambilight/ps4_ambient_state` goes to `fg=0`; the strip is free. Coming back to the app resumes both. The log prints `[main] backgrounded` / `foregrounded`. |
| Open Set up or Test strip with the shell feed running | The strip no longer flickers. `/data/ps4-ambilight/ps4_ambient_state` reads `fg=1 ... title=COMPANION` while the app is sending and `fg=0` after. |
