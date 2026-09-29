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

- The install path is newer than the rest and has not been exercised end to end on
  every network setup. The fallback is to download the `.prx` from Releases and drop
  it into GoldHEN's plugin folder by hand.
- The app resolves assets by the fixed names `ps4_ambient_light.prx` and its
  `.sha256`. The release workflow publishes exactly those, see
  [ci-and-releases](ci-and-releases.md).
