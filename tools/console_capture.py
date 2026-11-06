#!/usr/bin/env python3
"""console_capture.py -- read the capture file a DEBUG build writes on the console (v4.0.1).

With  [dev] capture_file=1  in /data/ps4-ambilight/ps4_ambient_light.ini  (no dev_ip needed) the plugin keeps every
debug packet in  /data/ps4-ambilight/ps4_ambient_capture_<TITLEID>.bin.  Copy that file to a PC and:

    python console_capture.py info     ps4_ambient_capture_CUSA06014.bin
    python console_capture.py to-flk   ps4_ambient_capture_CUSA06014.bin sf6.flk     # then: python flicker_capture.py analyze sf6.flk
    python console_capture.py dump     ps4_ambient_capture_CUSA06014.bin > capture.log  # the udp_ground_truth_listener.py text format,
                                                                                     # so decode_guard_packets.py etc. read it as is
    python console_capture.py selftest [file.bin count]                              # checks this tool

File format (little endian), written by plugin/source/capture_file.c, described in plugin/include/capture_file.h:
    header  "AMBCAP01", u32 version (1), u32 reserved
    record  u64 tsUs, u16 length, payload       (payload = what debug_send_raw was given, no DDP header)
The last record, "CAPX" + u32 records + u32 dropped + u32 full, is written only when the file reached its size limit (8 MB, about 30 minutes).
"""
import struct
import sys
import os
from collections import Counter

MAGIC = b"AMBCAP01"
HDR_LEN = 16
REC_HDR = 10
DDP_HDR = bytes([0x41, 0x00, 0x0B, 0x01, 0, 0, 0, 0])   # what debug_send_raw puts in front, then u16 length big endian


def read_capture(path):
    """Returns (records, problems). records = [(tsUs, payload)], problems = [text]."""
    with open(path, "rb") as f:
        buf = f.read()
    problems = []
    if len(buf) < HDR_LEN or buf[:8] != MAGIC:
        raise SystemExit(f"{path}: not a capture file (expected the AMBCAP01 header)")
    ver = struct.unpack_from("<I", buf, 8)[0]
    if ver != 1:
        problems.append(f"file version {ver}, this tool knows version 1")
    recs = []
    o = HDR_LEN
    while o + REC_HDR <= len(buf):
        ts, ln = struct.unpack_from("<QH", buf, o)
        if o + REC_HDR + ln > len(buf):
            problems.append(f"the last record is cut short ({len(buf) - o - REC_HDR} of {ln} bytes): the file was copied while it was being written")
            break
        recs.append((ts, buf[o + REC_HDR:o + REC_HDR + ln]))
        o += REC_HDR + ln
    else:
        if o != len(buf):
            problems.append(f"{len(buf) - o} stray bytes at the end")
    return recs, problems


def kind(payload):
    tag = payload[:4]
    if len(tag) == 4 and all(48 <= c < 58 or 65 <= c < 91 for c in tag):
        return tag.decode("ascii")
    return "binary"


def flk_gaps(recs):
    """FLK1 records whose sequence number did not follow the previous one (lost to a busy lock or a full buffer)."""
    gaps = lost = 0
    prev = None
    n = 0
    for _ts, p in recs:
        if p[:4] == b"FLK1" and len(p) >= 8:
            seq = struct.unpack_from("<I", p, 4)[0]
            if prev is not None and seq != ((prev + 1) & 0xFFFFFFFF):
                gaps += 1
                lost += (seq - prev - 1) & 0xFFFFFFFF
            prev = seq
            n += 1
    return n, gaps, lost


def cmd_info(path):
    recs, problems = read_capture(path)
    print(f"{path}: {len(recs)} records, {os.path.getsize(path)} bytes")
    if recs:
        span = (recs[-1][0] - recs[0][0]) / 1e6
        print(f"  spans {span:.1f} s of console time")
    for k, n in sorted(Counter(kind(p) for _t, p in recs).items(), key=lambda x: -x[1]):
        print(f"  {k:8s} {n}")
    n, gaps, lost = flk_gaps(recs)
    if n:
        print(f"  FLK1 probe packets: {n}, sequence gaps: {gaps} ({lost} packets missing)")
    for _ts, p in recs:
        if p[:4] == b"CAPX" and len(p) >= 16:
            r, d, full = struct.unpack_from("<III", p, 4)
            print(f"  closing record: {r} recorded, {d} dropped (lock busy or a stalled disk), size limit reached = {full}")
    for pr in problems:
        print("  note:", pr)
    return 0


def cmd_to_flk(path, out):
    """The .flk format of flicker_capture.py: <double seconds, u16 length> + payload per record. The seconds are the
    console's own clock, which is better than a PC's arrival time."""
    recs, problems = read_capture(path)
    with open(out, "wb") as f:
        for ts, p in recs:
            f.write(struct.pack("<dH", ts / 1e6, len(p)))
            f.write(p)
    n, gaps, lost = flk_gaps(recs)
    print(f"wrote {out}: {len(recs)} records ({n} FLK1 probe packets, {gaps} sequence gaps)")
    for pr in problems:
        print("note:", pr)
    return 0


def cmd_dump(path, out=sys.stdout):
    """Same text as udp_ground_truth_listener.py prints, so the decode_*.py tools read it unchanged."""
    recs, problems = read_capture(path)
    for ts, p in recs:
        secs = ts / 1e6
        stamp = "%02d:%02d:%02d.%03d" % (int(secs // 3600) % 24, int(secs // 60) % 60, int(secs) % 60, int((secs % 1) * 1000))
        data = DDP_HDR + struct.pack(">H", len(p)) + p
        hexstr = data.hex()
        header, payload = hexstr[:20], hexstr[20:]
        note = ""
        if len(payload) >= 6:
            note = "  -> first LED = RGB(%d,%d,%d)" % (int(payload[0:2], 16), int(payload[2:4], 16), int(payload[4:6], 16))
        print(f"[{stamp}] from console:0  len={len(data)}  header={header}{note}", file=out)
        if p.startswith((b"SHL1", b"HOF1")):
            print("    telemetry=" + p.decode("ascii", errors="replace"), file=out)
        else:
            print(f"    payload_hex={payload}", file=out)
    for pr in problems:
        print("note:", pr, file=sys.stderr)
    return 0


def selftest(sample=None, count=None):
    import io
    import re
    import tempfile
    here = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, here)
    import flicker_capture as fc
    fails = []

    def check(cond, what):
        if not cond:
            fails.append(what)
            print("FAIL:", what)

    # A file made here, independent of the C writer.
    def build(n):
        b = bytearray(MAGIC + struct.pack("<II", 1, 0))
        for i in range(n):
            pkt = bytearray(132)
            pkt[0:4] = b"FLK1"
            struct.pack_into("<I", pkt, 4, i)
            struct.pack_into("<I", pkt, 8, i * 33333)
            struct.pack_into("<I", pkt, 16, 0x80000000)
            pkt[22] = i % 3
            pkt[23] = 3
            pkt[25] = 6
            pkt[26] = 7 | (2 << 4)
            pkt[27] = 0x10
            b += struct.pack("<QH", 5_000_000 + i * 33333, len(pkt)) + pkt
            if i % 10 == 0:
                other = b"HOF1 hello"
                b += struct.pack("<QH", 5_000_000 + i * 33333 + 5, len(other)) + other
        return bytes(b)

    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "a.bin")
        open(p, "wb").write(build(120))
        recs, problems = read_capture(p)
        check(len(recs) == 132 and not problems, "reads every record of a clean file")
        n, gaps, lost = flk_gaps(recs)
        check((n, gaps, lost) == (120, 0, 0), "counts 120 probe packets and no gaps")
        flk = os.path.join(d, "a.flk")
        cmd_to_flk(p, flk)
        passes, others = fc.parse(fc.read_records(flk))
        check(len(passes) == 120 and len(others) == 12, "flicker_capture.py parses the converted file: 120 passes, 12 other packets")
        check(passes[5]["seq"] == 5 and passes[5]["nbuf"] == 3 and passes[5]["fmt"] == 0x80000000, "probe fields survive the conversion")
        check(abs(passes[1]["pc"] - passes[0]["pc"] - 0.033333) < 1e-6, "timestamps come from the console clock")
        lines, _ = fc.analyze(passes, others, None)
        check(len(lines) > 0, "flicker_capture.py analyze runs on it")
        out = io.StringIO()
        cmd_dump(p, out)
        text = out.getvalue().splitlines()
        check(text[0].startswith("[") and "from console:0  len=142" in text[0], "dump: the listener's line format and length (10-byte header + 132)")
        check(any(l.startswith("    telemetry=HOF1") for l in text), "dump: HOF1 is shown as telemetry like the listener does")
        check(re.match(r"\[(\S+)\] from \S+\s+len=(\d+)", text[0]) is not None, "dump: decode_guard_packets.py's pattern matches")
        # a gap and a cut-short file
        bad = bytearray(build(20))
        # drop probe packet 7: find it by its sequence number
        needle = b"FLK1" + struct.pack("<I", 7)
        at = bytes(bad).index(needle) - REC_HDR
        del bad[at:at + REC_HDR + 132]
        q = os.path.join(d, "b.bin")
        open(q, "wb").write(bytes(bad[:-50]))
        recs2, problems2 = read_capture(q)
        n2, gaps2, lost2 = flk_gaps(recs2)
        check(gaps2 == 1 and lost2 == 1, "a missing probe packet shows up as one gap of one packet")
        check(any("cut short" in x for x in problems2), "a file copied mid-write is reported, not crashed on")
        try:
            open(os.path.join(d, "c.bin"), "wb").write(b"nope")
            read_capture(os.path.join(d, "c.bin"))
            check(False, "a file with the wrong header is refused")
        except SystemExit:
            pass

    if sample:
        recs, problems = read_capture(sample)
        n, gaps, lost = flk_gaps(recs)
        want = int(count) if count else None
        print(f"{sample}: {len(recs)} records, {n} probe packets, {gaps} gaps, problems: {problems or 'none'}")
        check(not problems, "the file the C writer made reads cleanly")
        check(gaps == 0, "the C writer left no sequence gaps")
        if want is not None:
            check(n == want, f"the C writer recorded all {want} probe packets (got {n})")
        flk = sample + ".flk"
        cmd_to_flk(sample, flk)
        passes, others = fc.parse(fc.read_records(flk))
        check(len(passes) == n, "flicker_capture.py parses the converted C-written file")
        lines, _ = fc.analyze(passes, others, None)
        check(len(lines) > 0, "flicker_capture.py analyze runs on the C-written file")
        os.remove(flk)

    print("console_capture selftest:", "all checks passed" if not fails else f"{len(fails)} FAILED")
    return 1 if fails else 0


def main(argv):
    if len(argv) >= 2 and argv[0] == "info":
        return cmd_info(argv[1])
    if len(argv) >= 3 and argv[0] == "to-flk":
        return cmd_to_flk(argv[1], argv[2])
    if len(argv) >= 2 and argv[0] == "dump":
        return cmd_dump(argv[1])
    if argv and argv[0] == "selftest":
        return selftest(argv[1] if len(argv) > 1 else None, argv[2] if len(argv) > 2 else None)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
