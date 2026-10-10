# Companion app self-updater

How the app installs, updates and enables the plugin from GitHub releases, and the
chain of bugs it went through on Sep 25 before it worked. Several of the fixes are
platform quirks, collected in [orbis-gotchas](orbis-gotchas.md).

## What it does

On **Home** the main button reads **Install**, **Update** or **Enable**, or
**Test Strip** once everything is in place.

| State | Meaning |
|---|---|
| Install | `ps4_ambient_light.prx` isn't in GoldHEN's plugin folder |
| Update | installed, but older than the latest release |
| Enable | present, but its entry in `/data/GoldHEN/plugins.ini` is commented out or missing |

The `.prx` lives at `PLUGIN_PRX_PATH` in GoldHEN's plugins folder. Install downloads
the latest release build, verifies it and registers it in `plugins.ini` under
`[default]`, which loads it for every title. Enable only fixes the `plugins.ini`
entry.

## GoldHEN's plugin loader switch

GoldHEN loads no plugin at all while **Enable Plugins Loader** is off, so every plugin button
in the app is pointless until it is on. GoldHEN saves the setting in `/data/GoldHEN/config.ini`:

```
[PluginLoader]
PluginLoader_Enabled = 1
```

That was found by saving the file from a console with the switch off and with it on; the one
value was the only difference. `include/goldhen_config.h` reads it. Only a definite `0` counts as
off. A missing file, a missing key (an older GoldHEN) or any other value is "unknown", which Home
treats as on, so an unfamiliar config can never block the plugin's buttons.

It is the first check on Home (`home_plan()` in `install_plan.h`): when it is off, Home shows
"Plugin loader is off" and its button reads **Close app to turn on plugin loader**; Install,
Update, Enable and Test strip aren't offered. Home re-reads the file at startup, every couple of
seconds while on Home, and straight away on coming back to the app.

**The app does not write it.** The first version did: it set the value to 1 and read it back. On a
console the file said 1 but GoldHEN's menu still showed the switch off, because GoldHEN keeps the
live value in memory and never re-reads the file. That also made the file disagree with the real
state, so Home would have said the loader was on while it was off. And GoldHEN refuses to change the
switch at all while a game or app is running, which this app counts as. Both are GoldHEN's behaviour
rather than something the app can work around, so the button closes the app (the same save and quit
as Options) and the person ticks the switch themselves.

## The shell feed's files

With **Shell feed** on in Set up, the same machinery handles two more files, so Home
can show one banner and one button for all three: `ps4_shell_feed.prx` (into
`/data/ps4-ambilight`) and the Ambilight payload `ps4-ambilight-900.bin` (into `/data/payloads`).
Each is resolved by name with its `.sha256`, downloaded to a staging file, hashed
against the published checksum and renamed into place. The Ambilight payload stages next to the
`.prx`, not in `/data/payloads`, so a failed download never leaves a half-written file
in a folder payload menus list. The `.prx` goes first. Whether a file counts as an
update is the same check as the plugin's: the on-disk file's SHA-256 against the
published one, and offline an installed file just counts as installed. What Home shows
and what the button does for every combination is `include/install_plan.h`, tested on a
PC in `tests/test_install_plan.c`. Autoload at boot edits GoldHEN's payload AutoRun list
in place, the same keep-every-other-byte approach as `plugins.ini` below
(`include/payload_ini.h`). With the switch off none of this runs.

## A local update source

For testing, `[dev] update_base_url=http://<pc>:8000` in the app's ini replaces the
GitHub lookup: every file and its `.sha256` is fetched from `<base>/<file name>`, with no
`Accept` header. It is read again on each check, so no restart is needed, and the app's
own save leaves it alone. `tools/serve_local_release.py` serves the three files and
writes their checksums. See [local-install-testing](local-install-testing.md).

## Editing `plugins.ini`

That file is GoldHEN's, shared with every other plugin, and it uses a different
dialect from the app's own ini: bare path lines with no `key = value`, and
`;` comment lines GoldHEN ships as inert examples. The app's own ini parser was tried
on it first and checked against a file shaped like the real one. It silently dropped
every comment line and rewrote every bare path as `path = `, which corrupted an
unrelated, already working plugin entry.

So the app doesn't parse it at all. It finds `[default]`, finds where that section
ends (the next `[...]` header or end of file), inserts its own path there if it isn't
already present, and copies every other byte through untouched. If the file doesn't
exist it creates one containing only `[default]` and its own entry, since the app has
no business guessing what else a user has installed. Checked against: no file, a
realistic file with comments and another section, running twice (no duplicate line),
and a file with no `[default]` section yet.

## Flow

1. On startup, on a background thread, resolve the latest release's `.prx` and
   `.sha256` assets.
2. Compare the checksum of the installed `.prx` with the published one.
3. On a mismatch, flip the Home button to Update.
4. On press, download the `.prx` to a `.new` staging file, hash it and compare with
   the published SHA-256, then copy it into place and write the checksum sidecar
   (`PLUGIN_INSTALLED_CHECKSUM_PATH`).

Any network or file error during the startup check fails silently, since it runs
unprompted on every launch.

## Bugs, in the order they were hit

**1. Every download failed ("Download FAILED", `3c432b0`).** The URLs were GitHub
`releases/latest/download/...` links. GitHub answers with a 302 to a signed
`objects.githubusercontent.com` URL. A browser follows that silently. `sceHttp`
doesn't unless you tell it to, and the code required `statusCode == 200`, so it saw
the 302 and failed every time. Fix: `sceHttpSetAutoRedirect(tpl, 1)` on both request
templates.

**2. Resolving assets through the API (`e6c62b0`).** The download links still went
through github.com's web app. The app now asks `api.github.com/.../releases/latest`,
finds the asset's own API URL and requests it with `Accept: application/octet-stream`,
so the redirect goes straight to `objects.githubusercontent.com`. Both asset URLs are
resolved up front and it fails fast with a clear status if either is missing.

**3. No update detection existed (`0c32feb`).** The UI had an `UI_INSTALL_UPDATE`
state and a screen ready for it, but nothing ever set it. Added a local checksum
sidecar written after each successful install, and the startup check above.

**4. The app felt slow to open (`96c2091`).** The check did two blocking HTTPS round
trips inline at startup, and each could hit its full 10 s timeout on a bad
connection. It runs on a background thread now, polled once a frame through a pair of
atomic flags.

**5. Startup crash on real hardware.** The new thread was created with a stack size
of 0 and crashed immediately. See the thread stack entry in
[orbis-gotchas](orbis-gotchas.md).

**6. The sidecar was trusted over the real file (`996bb85`).** The check read the
sidecar first and only hashed the real `.prx` if the sidecar was missing. But only
this app writes the sidecar, so dropping in a different build by hand (FTP, editing
the plugins folder) left it pointing at the old build, which still matched the latest
release. The app reported "up to date" while a different build ran. It now hashes the
real file first and only falls back to the sidecar if that read fails.

**7. "Update available" on every relaunch (`587e9c6`).** The staging file was opened
without `O_TRUNC`, so a stale tail could make it longer than the SHA-256 that was
verified. See the `O_TRUNC` entry in [orbis-gotchas](orbis-gotchas.md).

## Known limits

- The install path has been run end to end on a console against a local test server
  over plain `http://`, including checksum failures. It has not been exercised on every
  network setup. The fallback is to download the files from Releases and put them in
  place by hand.
- The app resolves assets by fixed names: `ps4_ambient_light.prx`,
  `ps4_shell_feed.prx` and `ps4-ambilight-900.bin`, each with a `.sha256`. The release
  workflow does not publish the last two yet, see [ci-and-releases](ci-and-releases.md);
  until it does, installing the shell feed from GitHub has nothing to find.
