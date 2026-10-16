#!/usr/bin/env python3
"""handoff_capture.py -- isolated capture + analysis of the launch-gap (docs/debugging/launch-gap-test.md).

Symptom: after a game's launch picture, and before its flip hooks start producing frames, nothing is sent
to WLED, so WLED drops to its own default color.

This tool listens on the same dev UDP channel as the other tools (port 4048, [dev] dev_ip +
dev_logging=true), but keeps ONLY the event-only records that matter:
  HOF1  plugin handoff / launch-gap events   ("HANDOFF_DIAG <NAME> k=v ...")
  SHL1  shell-feed events                    (FOREGROUND GATE ON/OFF, HANDOFF_DIAG SHELL_*)
Every other packet (FLK1, GRDC, PQ8C, status rows, ...) is only counted, never shown or stored, so a debug
build's telemetry cannot muddy the capture. Do not run it together with flicker_capture.py or
udp_ground_truth_listener.py: all three use port 4048.

All timestamps are the arrival time on THIS PC's clock (one clock for plugin, shell feed and WLED), so no
cross-process tsc comparison is needed. The tsc/hz fields are used only as a same-process cross-check.
Resolution is therefore UDP jitter (normally a few ms) plus, for the plugin's LG_* milestones, one sampler
pass (~33 ms).

Usage:
  python tools/handoff_capture.py capture --out run1.jsonl [--seconds 90] [--wled-ip 192.168.x.x]
  python tools/handoff_capture.py analyze run1.jsonl
  python tools/handoff_capture.py selftest          # no console needed

Write the jsonl with --out. Do not redirect with PowerShell's '>' (UTF-16, see docs/development/testing.md).
"""
import argparse
import json
import socket
import sys
import threading
import time
import urllib.request

PORT = 4048
DDP_HDR = 10
SESSION_PRE_S = 2.0        # context kept before a session's LG_SAMPLER_START (gate OFF, WLED state, ...)
NO_GAP_S = 0.100           # a silence at or under this is not called a gap (state poll is 100 ms)
OVERLAP_OK_S = 0.250       # with the launch latch the shell backs off AFTER the game's first send: state tick
                           # (100 ms) + shell poll (100 ms) + jitter. Up to this is the expected handoff overlap.
SKEW_WARN_S = 0.050        # PC-clock vs same-process-tsc disagreement worth a warning

# What a late milestone means, for the "largest step" line.
STEP_MEANING = {
    "LG_FIRST_REGISTER_HOOK": "game had not registered its display buffers yet",
    "LG_FORMAT_CONFIRMED": "buffers registered but the pixel format was not confirmed/known yet",
    "LG_FIRST_FLIP_HOOK": "no flip hook had fired yet (launch screen / game not flipping through a hooked path)",
    "LG_LAG_HISTORY_READY": "waiting for the sample-lag history (needs distinct flipped slots, see sample_thread.c)",
    "LG_LAG_WAIT_EXPIRED": "lag-history wait ran its full ~1 s without history",
    "GAME_FIRST_VALID_FRAME": "sampler had no readable buffer + confirmed format together yet",
    "GAME_FIRST_DDP_RESULT": "first valid frame existed; delay is in the pipeline up to the first send",
}


# ---------------------------------------------------------------- parsing
def parse_packet(data):
    """-> (src, name, fields, text) or None for a packet that is not HOF1/SHL1 text."""
    body = data[DDP_HDR:] if len(data) >= DDP_HDR else b""
    if not body.startswith((b"HOF1", b"SHL1")):
        return None
    src = body[:4].decode("ascii")
    text = body[4:].decode("ascii", errors="replace").strip()
    fields = {}
    tokens = text.split()
    for tok in tokens:
        if "=" in tok:
            k, v = tok.split("=", 1)
            fields[k] = v
    name = None
    if tokens and tokens[0] == "HANDOFF_DIAG" and len(tokens) > 1:
        name = tokens[1]
    elif text.startswith("FOREGROUND GATE ON"):
        name = "SHELL_GATE_ON"
    elif text.startswith("FOREGROUND GATE OFF"):
        name = "SHELL_GATE_OFF"
    return src, name, fields, text


def other_tag(data):
    body = data[DDP_HDR:] if len(data) >= DDP_HDR else b""
    tag = body[:4]
    if len(tag) == 4 and all(32 <= c < 127 for c in tag):
        return tag.decode("ascii")
    return "binary(len=%d)" % len(data)


# ---------------------------------------------------------------- capture
class WledPoller(threading.Thread):
    """Optional. Polls http://<ip>/json/info at ~10 Hz and records live-state transitions."""

    def __init__(self, ip, hz, sink, t0):
        super().__init__(daemon=True)
        self.url, self.period, self.sink, self.t0 = "http://%s/json/info" % ip, 1.0 / hz, sink, t0
        self.stop = threading.Event()
        self.last = None
        self.errors = 0

    def run(self):
        while not self.stop.is_set():
            started = time.monotonic()
            try:
                with urllib.request.urlopen(self.url, timeout=0.4) as r:
                    info = json.loads(r.read().decode("utf-8", errors="replace"))
                live = 1 if info.get("live") else 0
                if live != self.last:
                    self.last = live
                    self.sink({"t": time.monotonic() - self.t0, "src": "WLED", "name": "WLED_LIVE",
                               "f": {"live": str(live), "lm": str(info.get("lm", ""))}, "text": ""})
            except Exception:
                self.errors += 1
            self.stop.wait(max(0.0, self.period - (time.monotonic() - started)))


def cmd_capture(a):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind(("0.0.0.0", a.port))
    except OSError as e:
        sys.exit("cannot bind UDP %d (%s). flicker_capture.py or udp_ground_truth_listener.py running?" % (a.port, e))
    sock.settimeout(0.2)
    t0 = time.monotonic()
    events, others = [], {}
    lock = threading.Lock()

    def sink(ev):
        with lock:
            events.append(ev)
        print("  +%8.3fs  %-4s %-26s %s" % (ev["t"], ev["src"], ev["name"] or "-", _brief(ev["f"])))

    poller = None
    if a.wled_ip:
        poller = WledPoller(a.wled_ip, a.wled_poll_hz, sink, t0)
        poller.start()
    print("Listening on UDP %d. Launch the game now. Stop with Ctrl+C%s." %
          (a.port, "" if not a.seconds else " (or after %ds)" % a.seconds))
    try:
        while not a.seconds or time.monotonic() - t0 < a.seconds:
            try:
                data, addr = sock.recvfrom(65535)
            except socket.timeout:
                continue
            t = time.monotonic() - t0
            p = parse_packet(data)
            if p is None:
                tag = other_tag(data)
                others[tag] = others.get(tag, 0) + 1
                continue
            src, name, fields, text = p
            if name is None and not a.all_shell:
                continue                      # shell status rows etc.: not part of this test
            sink({"t": t, "src": src, "name": name or "SHL1_TEXT", "f": fields, "text": text, "from": addr[0]})
    except KeyboardInterrupt:
        pass
    finally:
        if poller:
            poller.stop.set()
    if a.out:
        with open(a.out, "w", encoding="utf-8") as fh:
            for ev in events:
                fh.write(json.dumps(ev) + "\n")
            fh.write(json.dumps({"src": "SUMMARY", "other_packets": others,
                                 "wled_poll_errors": poller.errors if poller else None}) + "\n")
        print("saved %d events to %s" % (len(events), a.out))
    if others:
        print("ignored (counted only): " + ", ".join("%s x%d" % kv for kv in sorted(others.items())))
    print()
    print(analyze(events, wled_polled=bool(a.wled_ip))[0])


def _brief(f):
    keep = [k for k in f if k not in ("tsc", "hz")]
    return " ".join("%s=%s" % (k, f[k]) for k in keep)


# ---------------------------------------------------------------- analysis
def _first(evs, name, pred=None):
    for e in evs:
        if e["name"] == name and (pred is None or pred(e)):
            return e
    return None


def _tsc_delta(a, b):
    try:
        if a["f"]["hz"] == b["f"]["hz"]:
            return (int(b["f"]["tsc"]) - int(a["f"]["tsc"])) / float(a["f"]["hz"])
    except (KeyError, ValueError, ZeroDivisionError):
        pass
    return None


def split_sessions(events):
    events = sorted(events, key=lambda e: e["t"])
    starts = [e["t"] for e in events if e["name"] == "LG_SAMPLER_START"]
    if not starts:
        return [(events[0]["t"] if events else 0.0, events)]
    out = []
    for i, s in enumerate(starts):
        lo = s - SESSION_PRE_S
        hi = starts[i + 1] - SESSION_PRE_S if i + 1 < len(starts) else float("inf")
        out.append((s, [e for e in events if lo <= e["t"] < hi]))
    return out


def analyze_session(anchor, evs, wled_polled):
    r = {"lines": [], "gap_s": None, "code": "INCOMPLETE", "largest": None}
    L = r["lines"]
    g = lambda n, p=None: _first(evs, n, p)
    start, reg, fmt = g("LG_SAMPLER_START"), g("LG_FIRST_REGISTER_HOOK"), g("LG_FORMAT_CONFIRMED")
    flip, lag, lagx = g("LG_FIRST_FLIP_HOOK"), g("LG_LAG_HISTORY_READY"), g("LG_LAG_WAIT_EXPIRED")
    frame, ddp = g("GAME_FIRST_VALID_FRAME"), g("GAME_FIRST_DDP_RESULT")
    wr1 = g("GAME_STATE_WRITE", lambda e: e["f"].get("fg") == "1")
    gate_on = g("SHELL_GATE_ON", lambda e: e["t"] >= anchor - 0.5)

    L.append("timeline (t = seconds after plugin sampler start; one PC clock)")
    for e in evs:
        L.append("  %+8.3f  %-4s %-26s %s" % (e["t"] - anchor, e["src"], e["name"], _brief(e["f"])))

    miss = [n for n, e in (("LG_SAMPLER_START", start), ("LG_FIRST_FLIP_HOOK", flip), ("GAME_FIRST_VALID_FRAME", frame),
                           ("GAME_FIRST_DDP_RESULT", ddp), ("SHELL_GATE_ON", gate_on)) if e is None]
    if miss:
        L.append("")
        L.append("not received: " + ", ".join(miss) +
                 "  (UDP is lossy, or the milestone was never reached. Re-run before concluding.)")

    # same-process cross-check of the PC clock
    hof = [e for e in evs if e["src"] == "HOF1" and "tsc" in e["f"]]
    if len(hof) >= 2:
        d_pc, d_ts = hof[-1]["t"] - hof[0]["t"], _tsc_delta(hof[0], hof[-1])
        if d_ts is not None and abs(d_pc - d_ts) > SKEW_WARN_S:
            L.append("WARNING: PC arrival span %.3fs vs plugin tsc span %.3fs differ by more than %d ms "
                     "(UDP delay/loss at the ends); treat sub-100 ms conclusions with care."
                     % (d_pc, d_ts, int(SKEW_WARN_S * 1000)))

    L.append("")
    if ddp is None or (gate_on is None and wr1 is None):
        L.append("verdict: INCOMPLETE -- need GAME_FIRST_DDP_RESULT and a silence start (SHELL_GATE_ON, or GAME_STATE_WRITE fg=1).")
        return r

    if wr1 is not None:
        L.append("fg=1 was published %+.3f s relative to the game's first DDP submission "
                 "(negative = published BEFORE the game could send)." % (wr1["t"] - ddp["t"]))
    silence_from = gate_on if gate_on is not None else wr1
    basis = "SHELL_GATE_ON" if gate_on is not None else "GAME_STATE_WRITE fg=1 (gate event missing: estimate)"
    gap = ddp["t"] - silence_from["t"]
    r["gap_s"] = gap

    # milestones that were already done when the shell backed off, vs still pending
    done = [e for e in (reg, fmt, flip, lag, frame) if e is not None and e["t"] <= silence_from["t"]]
    if done:
        L.append("already reached when the shell backed off: " + ", ".join(e["name"] for e in done))

    if gap <= NO_GAP_S:
        r["code"] = "NO_GAP" if gap >= -OVERLAP_OK_S else "OVERLAP"
        if r["code"] == "OVERLAP":
            L.append("verdict: OVERLAP -- the game was already sending %.3f s before the shell backed off "
                     "(two senders at once for longer than the state tick + shell poll explain)." % -gap)
        elif gap < 0:
            L.append("verdict: NO SENDER GAP -- the game's first DDP came first and the shell kept sending for %.3f s "
                     "more (expected handoff: state tick + shell poll, up to ~0.2 s)." % -gap)
        else:
            L.append("verdict: NO SENDER GAP -- shell backed off (%s) and the game's first DDP followed within %.3f s."
                     % (basis, gap))
        return r

    r["code"] = "GAP"
    L.append("verdict: SENDER GAP of %.3f s between %s and GAME_FIRST_DDP_RESULT." % (gap, basis))
    chain = [silence_from] + sorted([e for e in (reg, fmt, flip, lag, lagx, frame)
                                     if e is not None and silence_from["t"] < e["t"] < ddp["t"]], key=lambda e: e["t"]) + [ddp]
    steps = [(chain[i + 1]["name"], chain[i + 1]["t"] - chain[i]["t"]) for i in range(len(chain) - 1)]
    for name, d in steps:
        L.append("  +%7.3f s  until %s" % (d, name))
    name, d = max(steps, key=lambda s: s[1])
    r["largest"] = name
    L.append("largest step: %s (+%.3f s) -- %s." % (name, d, STEP_MEANING.get(name, "")))
    if flip is not None and flip["t"] > silence_from["t"]:
        L.append("note: the game's first flip hook arrived %.3f s AFTER the shell backed off; heartbeat cannot cover "
                 "this window because nothing has been sent yet (wled_send_keepalive_if_stale returns early)."
                 % (flip["t"] - silence_from["t"]))
    return r


def analyze_wled(evs, anchor, gate_t):
    out = []
    ws = [e for e in evs if e["src"] == "WLED"]
    for i, e in enumerate(ws):
        if e["f"].get("live") == "0":
            end = ws[i + 1]["t"] if i + 1 < len(ws) else None
            out.append((e["t"], end))
    return out


def analyze(events, wled_polled=False):
    events = [e for e in events if e.get("src") != "SUMMARY"]
    if not events:
        return "no events captured", []
    results, text = [], []
    sessions = split_sessions(events)
    for i, (anchor, evs) in enumerate(sessions):
        text.append("=== run %d of %d ===" % (i + 1, len(sessions)))
        r = analyze_session(anchor, evs, wled_polled)
        text.extend(r["lines"])
        if wled_polled:
            ivs = analyze_wled(evs, anchor, None)
            if not ivs:
                text.append("WLED live flag: stayed live (no fallback seen at the poll rate).")
            for s, e in ivs:
                text.append("WLED live flag: NOT live from %+.3f s%s -- WLED was on its own default there."
                            % (s - anchor, (" to %+.3f s (%.3f s)" % (e - anchor, e - s)) if e else " until the end of the capture"))
        else:
            text.append("WLED was not polled (add --wled-ip) so the visible fallback itself is not timed; "
                        "only the sender side is.")
        text.append("")
        results.append(r)
    return "\n".join(text), results


def cmd_analyze(a):
    ev = []
    with open(a.file, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if line:
                ev.append(json.loads(line))
    wled = any(e.get("src") == "WLED" for e in ev)
    print(analyze(ev, wled_polled=wled)[0])


# ---------------------------------------------------------------- selftest
def _ev(t, src, name, **f):
    return {"t": t, "src": src, "name": name, "f": {k: str(v) for k, v in f.items()}, "text": ""}


def _pkt(tag, text):
    return bytes([0x41, 0, 0x0B, 1, 0, 0, 0, 0, 0, 0]) + tag + text


def cmd_selftest(_a):
    ok = True

    def check(label, cond):
        nonlocal ok
        print("%-62s %s" % (label, "ok" if cond else "FAIL"))
        ok &= bool(cond)

    # parsing: plugin record has a space after HOF1, the shell's SHL1 text follows the magic directly
    p = parse_packet(_pkt(b"HOF1", b" HANDOFF_DIAG LG_FIRST_FLIP_HOOK gnm=3 vo=0 wl=0 idx=1 bg=0 tsc=10 hz=1000"))
    check("parse HOF1 milestone", p and p[0] == "HOF1" and p[1] == "LG_FIRST_FLIP_HOOK" and p[2]["gnm"] == "3")
    p = parse_packet(_pkt(b"SHL1", b"HANDOFF_DIAG SHELL_FIRST_DDP_RESULT bytes=700 expected=700 zones=230 tsc=1 hz=2"))
    check("parse SHL1 (no space after magic)", p and p[1] == "SHELL_FIRST_DDP_RESULT" and p[2]["zones"] == "230")
    p = parse_packet(_pkt(b"SHL1", b"FOREGROUND GATE ON t=1234ms"))
    check("parse shell gate ON", p and p[1] == "SHELL_GATE_ON")
    check("FLK1 is not kept", parse_packet(_pkt(b"FLK1", b"\x00" * 40)) is None)
    check("FLK1 is counted by tag", other_tag(_pkt(b"FLK1", b"\x00" * 40)) == "FLK1")

    base = [_ev(0.00, "HOF1", "LG_SAMPLER_START", bg=0),
            _ev(0.03, "HOF1", "GAME_STATE_WRITE", fg=1, seq=1)]

    # 1. gap dominated by waiting for the first flip hook
    ev = base + [_ev(0.20, "SHL1", "SHELL_GATE_ON"), _ev(1.00, "HOF1", "LG_FIRST_REGISTER_HOOK", ok=1),
                 _ev(1.00, "HOF1", "LG_FORMAT_CONFIRMED"), _ev(3.20, "HOF1", "LG_FIRST_FLIP_HOOK", bg=0),
                 _ev(3.30, "HOF1", "LG_LAG_HISTORY_READY"), _ev(3.36, "HOF1", "GAME_FIRST_VALID_FRAME"),
                 _ev(3.40, "HOF1", "GAME_FIRST_DDP_RESULT")]
    r = analyze(ev)[1][0]
    check("scenario flip-wait: gap found", r["code"] == "GAP" and abs(r["gap_s"] - 3.20) < 1e-6)
    check("scenario flip-wait: largest = first flip hook", r["largest"] == "LG_FIRST_FLIP_HOOK")

    # 2. gap dominated by the lag-history wait
    ev = base + [_ev(0.20, "SHL1", "SHELL_GATE_ON"), _ev(0.40, "HOF1", "LG_FIRST_REGISTER_HOOK"),
                 _ev(0.40, "HOF1", "LG_FORMAT_CONFIRMED"), _ev(0.50, "HOF1", "LG_FIRST_FLIP_HOOK"),
                 _ev(1.60, "HOF1", "LG_LAG_WAIT_EXPIRED"), _ev(1.66, "HOF1", "GAME_FIRST_VALID_FRAME"),
                 _ev(1.70, "HOF1", "GAME_FIRST_DDP_RESULT")]
    r = analyze(ev)[1][0]
    check("scenario lag-wait: largest = lag wait expired", r["code"] == "GAP" and r["largest"] == "LG_LAG_WAIT_EXPIRED")

    # 3. no gap: the game sends right after the shell backs off
    ev = base + [_ev(0.05, "HOF1", "LG_FIRST_FLIP_HOOK"), _ev(0.20, "SHL1", "SHELL_GATE_ON"),
                 _ev(0.25, "HOF1", "GAME_FIRST_DDP_RESULT")]
    check("scenario no-gap", analyze(ev)[1][0]["code"] == "NO_GAP")

    # 3b. launch latch: game sends first, shell backs off ~150 ms later = normal handoff, not an overlap warning
    ev = base + [_ev(0.40, "HOF1", "GAME_FIRST_DDP_RESULT"), _ev(0.55, "SHL1", "SHELL_GATE_ON")]
    check("scenario latch handoff (shell 150 ms late)", analyze(ev)[1][0]["code"] == "NO_GAP")

    # 4. overlap: the game was sending long before the shell backed off
    ev = base + [_ev(0.10, "HOF1", "GAME_FIRST_DDP_RESULT"), _ev(0.90, "SHL1", "SHELL_GATE_ON")]
    check("scenario overlap", analyze(ev)[1][0]["code"] == "OVERLAP")

    # 5. incomplete: no first DDP received
    ev = base + [_ev(0.20, "SHL1", "SHELL_GATE_ON"), _ev(2.00, "HOF1", "LG_FIRST_FLIP_HOOK")]
    check("scenario incomplete (no DDP event)", analyze(ev)[1][0]["code"] == "INCOMPLETE")

    # 6. two launches in one capture are split and judged separately
    run2 = [_ev(100.00, "HOF1", "LG_SAMPLER_START"), _ev(100.03, "HOF1", "GAME_STATE_WRITE", fg=1),
            _ev(100.20, "SHL1", "SHELL_GATE_ON"), _ev(100.22, "HOF1", "GAME_FIRST_DDP_RESULT")]
    first = base + [_ev(0.20, "SHL1", "SHELL_GATE_ON"), _ev(2.00, "HOF1", "GAME_FIRST_DDP_RESULT")]
    res = analyze(first + run2)[1]
    check("scenario two runs split", len(res) == 2 and res[0]["code"] == "GAP" and res[1]["code"] == "NO_GAP")

    # 7. WLED fallback interval reported with the run it overlaps
    ev = base + [_ev(0.20, "SHL1", "SHELL_GATE_ON"), _ev(1.20, "WLED", "WLED_LIVE", live=0),
                 _ev(2.40, "WLED", "WLED_LIVE", live=1), _ev(2.40, "HOF1", "GAME_FIRST_DDP_RESULT")]
    txt = analyze(ev, wled_polled=True)[0]
    check("scenario WLED fallback interval printed", "NOT live from +1.200 s to +2.400 s (1.200 s)" in txt)

    print("selftest", "PASSED" if ok else "FAILED")
    sys.exit(0 if ok else 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("capture", help="listen, filter, save and analyze one launch")
    c.add_argument("--port", type=int, default=PORT)
    c.add_argument("--out", help="save kept events as jsonl")
    c.add_argument("--seconds", type=int, default=0, help="stop after N seconds (default: until Ctrl+C)")
    c.add_argument("--wled-ip", help="also poll http://<ip>/json/info for the live flag (times the visible fallback)")
    c.add_argument("--wled-poll-hz", type=float, default=10.0)
    c.add_argument("--all-shell", action="store_true", help="also keep SHL1 lines that are not handoff events")
    c.set_defaults(fn=cmd_capture)
    an = sub.add_parser("analyze", help="analyze a saved jsonl")
    an.add_argument("file")
    an.set_defaults(fn=cmd_analyze)
    st = sub.add_parser("selftest", help="run the analyzer on synthetic scenarios")
    st.set_defaults(fn=cmd_selftest)
    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
