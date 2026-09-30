#!/usr/bin/env python3
"""
decode_guard_packets.py

Reads a log made by udp_ground_truth_listener.py (a debug build, [dev] dev_logging on) and
prints the v3.9 buffer guard telemetry: GRDC (guard counters), GRDI (kernel map info for the
last rejected region), RMAP (GPU-only buffer remap status), and a summary of the sampler's
timing packets. Written for the Mortal Kombat 11 investigation, see
docs/debugging/gpu-only-buffers.md.

    python tools/decode_guard_packets.py capture.log
    python tools/decode_guard_packets.py capture.log --all      # every packet, not first/last few
    python tools/decode_guard_packets.py --selftest

The packets are told apart by their 4 byte ASCII tag, NOT by length: GRDI, PQ8C and HDRV are
all 48 bytes after the DDP header, and a length-only decoder reads one as another. Layouts are in
plugin/source/network.c.

The listener log is UTF-16 if it was redirected from PowerShell with `>`. This reads either.
"""
import re
import struct
import sys

REMAP_METHOD = {0: "none yet", 1: "typed MapDirectMemory2", 2: "plain MapDirectMemory", 3: "mprotect"}
GUARD_REASON = {-1: "readable but shorter than the padded size", 0: "ok", 1: "unmapped gap", 2: "not CPU-readable"}
PROT_BITS = [(0x1, "CPU_R"), (0x2, "CPU_W"), (0x10, "GPU_R"), (0x20, "GPU_W")]


def read_text(path):
    raw = open(path, "rb").read()
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff") or (len(raw) > 1 and raw[1] == 0):
        return raw.decode("utf-16", errors="replace")
    return raw.decode("utf-8", errors="replace")


def packets(text):
    """Yield (timestamp, payload bytes) for every packet in a listener log."""
    lines = text.replace("\r", "").split("\n")
    for i, line in enumerate(lines[:-1]):
        m = re.match(r"\[(\S+)\] from \S+\s+len=(\d+)", line)
        if not m:
            continue
        h = re.search(r"payload_hex=([0-9a-fA-F]*)", lines[i + 1])
        if h:
            yield m.group(1), bytes.fromhex(h.group(1))


def fmt_prot(p):
    names = [n for bit, n in PROT_BITS if p & bit]
    return "%#x (%s)" % (p, "+".join(names) if names else "none")


def decode_grdc(t, b):
    avail, rejects, last_ret, addr = struct.unpack("<IIiQ", b[4:24])
    return "%s GRDC guard=%s rejects=%d last_reason=%d (%s) last_bad_addr=%#x" % (
        t, "on" if avail else "FAILING OPEN", rejects, last_ret, GUARD_REASON.get(last_ret, "?"), addr)


def decode_grdi(t, b):
    start, end, off, addr, prot, mtype, flags, ret = struct.unpack("<QQQQiiIi", b[4:52])
    return ("%s GRDI region %#x-%#x (%#x bytes) dmem_offset=%#x prot=%s type=%d flags=%#x (%s%s) reason=%d\n"
            "              queried %#x") % (
        t, start, end, end - start, off, fmt_prot(prot), mtype, flags,
        "direct " if flags & 2 else "", "committed" if flags & 0x10 else "", ret, addr)


def decode_rmap(t, b):
    flags, created, failed, passes, last_ret, alias_lo = struct.unpack("<IIIIiI", b[4:28])
    resolved = [n for bit, n in ((0x10, "Map2"), (0x20, "Map"), (0x40, "Munmap"), (0x80, "Mprotect")) if flags & bit]
    return ("%s RMAP setting=%d resolved=[%s] last_method=%s created=%d failed=%d passes=%d last_ret=%#x%s") % (
        t, flags & 0xF, ",".join(resolved) or "none", REMAP_METHOD.get((flags >> 8) & 0xFF, "?"),
        created, failed, passes, last_ret & 0xFFFFFFFF,
        "  (0x80020010 = EBUSY, a second mapping was refused)" if (last_ret & 0xFFFFFFFF) == 0x80020010 else "")


DECODERS = {b"GRDC": (24, decode_grdc), b"GRDI": (52, decode_grdi), b"RMAP": (28, decode_rmap)}


def summarize_timing(pkts):
    """The 36 byte timing packet: min, max, avg (microseconds), pass count, over-budget count, ..."""
    rows = []
    for t, b in pkts:
        if len(b) == 36:
            rows.append((t,) + struct.unpack("<9i", b))
    if not rows:
        return None
    avgs = sorted(r[3] for r in rows)
    return "timing: %d windows, avg pass %d us (median), %d us (worst window), worst single pass %d us, over-budget passes %d" % (
        len(rows), avgs[len(avgs) // 2], avgs[-1], max(r[2] for r in rows), sum(r[5] for r in rows))


def run(text, show_all):
    pkts = list(packets(text))
    out, seen = [], {}
    for t, b in pkts:
        tag = b[:4]
        if tag in DECODERS and len(b) == DECODERS[tag][0]:
            seen.setdefault(tag, []).append(DECODERS[tag][1](t, b))
    for tag in (b"GRDC", b"RMAP", b"GRDI"):
        rows = seen.get(tag, [])
        if not rows:
            out.append("%s: none in this capture" % tag.decode())
            continue
        out.append("%s: %d packets" % (tag.decode(), len(rows)))
        shown = rows if show_all or len(rows) <= 4 else rows[:2] + ["   ..."] + rows[-2:]
        out.extend("  " + r for r in shown)
    tm = summarize_timing(pkts)
    if tm:
        out.append(tm)
    if not seen:
        out.append("No guard packets. Is this a debug build (make DEBUG=1) with dev_logging on?")
    return "\n".join(out)


def selftest():
    grdc = b"GRDC" + struct.pack("<IIiQ", 1, 3, 2, 0x49ff608000)
    grdi = b"GRDI" + struct.pack("<QQQQiiIi", 0x49ff608000, 0x49ffdf4000, 0x64a08000, 0x49ff608000, 0x30, 3, 0x12, 2)
    rmap = b"RMAP" + struct.pack("<IIIIiI", 3 | 0xF0 | (3 << 8), 3, 0, 3, 0, 0)
    ebusy = b"RMAP" + struct.pack("<IIIIiI", 1 | 0x70 | (1 << 8), 0, 3, 0, -2147352560, 0)
    pq8c_like = b"PQ8C" + bytes(44)          # same 48 byte length as a GRDI-sized packet must NOT decode as one
    hdr = "41000b0100000000%04x" % 0
    log = ""
    for i, p in enumerate([grdc, grdi, rmap, ebusy, pq8c_like]):
        log += "[00:00:%02d.000] from 1.2.3.4:5  len=%d  header=%s\n    payload_hex=%s\n" % (i, len(p) + 10, hdr, p.hex())
    text = run(log, True)
    checks = [
        ("GRDC decoded", "rejects=3" in text and "not CPU-readable" in text),
        ("GRDI decoded", "0x7ec000 bytes" in text and "GPU_R+GPU_W" in text and "direct committed" in text),
        ("RMAP mprotect", "last_method=mprotect created=3 failed=0" in text),
        ("RMAP EBUSY", "EBUSY" in text and "last_method=typed MapDirectMemory2" in text),
        ("PQ8C ignored", text.count("GRDI region") == 1),
    ]
    ok = True
    for name, good in checks:
        print(("ok   " if good else "FAIL ") + name)
        ok = ok and good
    return 0 if ok else 1


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--selftest" in sys.argv:
        sys.exit(selftest())
    if not args:
        print(__doc__)
        sys.exit(2)
    print(run(read_text(args[0]), "--all" in sys.argv))
