#!/usr/bin/env python3
"""Serve the downloadable files from this PC, so the companion app can be tested without publishing a
GitHub release. It also serves ps4-ambilight-app.version, the one-line file the app compares with its own
version to say "a newer app is out", and the app's .pkg for the Triangle download on Home.

    python tools/serve_local_release.py                 # looks for the built files in the usual places
    python tools/serve_local_release.py --plugin X.prx --shell Y.prx --payload Z.bin --port 8000
    python tools/serve_local_release.py --app-version 9.99   # test the app update notice
    python tools/serve_local_release.py --app-version 9.99 --pkg X.pkg   # ...and its Triangle download

It copies the files into a scratch folder, writes the "<name>.sha256" next to each one (the same
"<hex>  <name>" text the release publishes), and runs Python's http.server there. Then, on the
console, add this to /data/ps4-ambilight/ps4_ambient_light.ini (use the address this prints):

    [dev]
    update_base_url=http://192.168.2.117:8000

Delete that line to go back to GitHub. Files that aren't found are skipped, which is useful for
testing the "this one is missing" paths: serve everything but one file and see what the app says.
"""
import argparse, glob, hashlib, http.server, os, shutil, socket, socketserver, sys, tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (flag, file name the app asks for, places to look when --flag isn't given)
ASSETS = [
    ("plugin",  "ps4_ambient_light.prx",  ["bin/plugins/prx/ps4_ambient_light.prx", "bin/plugins/prx_final/ps4_ambient_light.prx"]),
    ("shell",   "ps4_shell_feed.prx",     ["bin/plugins/prx/ps4_shell_feed.prx", "shell-feed/bin/plugins/prx/ps4_shell_feed.prx", "bin/plugins/prx_final/ps4_shell_feed.prx"]),
    ("payload", "ps4-ambilight.bin",  ["payloads/ps4-ambilight.bin"]),
    ("pkg",     "ps4-ambilight-app.pkg", ["companion-app/pkg_out/*.pkg"]),
]


def own_app_version():
    """APP_VERSION from companion-app/include/app_version.h, what CI publishes for this tree."""
    try:
        for line in open(os.path.join(REPO, "companion-app", "include", "app_version.h")):
            if line.startswith("#define APP_VERSION"):
                return line.split('"')[1]
    except (OSError, IndexError):
        pass
    return None


def lan_ip():
    """The address this PC uses to reach the LAN (no packet is sent)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    for flag, name, _ in ASSETS:
        ap.add_argument("--" + flag, help="path to " + name)
    ap.add_argument("--app-version", help="what ps4-ambilight-app.version says (default: this tree's APP_VERSION, so no notice); "
                                          "'none' leaves the file out")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--host", default="0.0.0.0", help="address to listen on (default: all)")
    args = ap.parse_args()

    out = tempfile.mkdtemp(prefix="ps4-ambilight-release-")
    print("Serving from", out)
    found = 0
    for flag, name, guesses in ASSETS:
        src = getattr(args, flag)
        if not src:
            for g in guesses:
                hits = sorted(glob.glob(os.path.join(REPO, g)))
                if hits and os.path.isfile(hits[0]):
                    src = hits[0]
                    break
        if not src or not os.path.isfile(src):
            print("  %-24s NOT FOUND -- not served (pass --%s PATH)" % (name, flag))
            continue
        dst = os.path.join(out, name)
        shutil.copyfile(src, dst)
        digest = hashlib.sha256(open(dst, "rb").read()).hexdigest()
        with open(dst + ".sha256", "w") as f:
            f.write("%s  %s\n" % (digest, name))
        print("  %-24s %8d bytes  sha256 %s...  (from %s)" % (name, os.path.getsize(dst), digest[:12], src))
        found += 1
    appver = args.app_version if args.app_version else own_app_version()
    if appver and appver.lower() != "none":
        with open(os.path.join(out, "ps4-ambilight-app.version"), "w") as f:
            f.write(appver + "\n")
        print("  %-24s %s" % ("ps4-ambilight-app.version", appver))
    else:
        print("  %-24s not served" % "ps4-ambilight-app.version")
    if not found and not appver:
        sys.exit("Nothing to serve.")

    ip = lan_ip()
    print("\nIn /data/ps4-ambilight/ps4_ambient_light.ini on the console:\n\n    [dev]\n    update_base_url=http://%s:%d\n" % (ip, args.port))
    print("Ctrl+C to stop. Each request is logged below.\n")

    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *a, **kw):
            super().__init__(*a, directory=out, **kw)

    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer((args.host, args.port), Handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nstopped")
        finally:
            shutil.rmtree(out, ignore_errors=True)


if __name__ == "__main__":
    main()
