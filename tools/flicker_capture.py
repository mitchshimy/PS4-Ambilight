#!/usr/bin/env python3
"""
flicker_capture.py -- capture + analyze the ps4_ambient_light FLICKER PROBE.

Needs a plugin build made with DEBUG (build.bat debug / make DEBUG=1) and this
in /data/ps4_ambient_light.ini on the console:

    [dev]
    dev_ip=<this PC's IP>
    dev_logging=true

USAGE
    python flicker_capture.py capture menu.flk            # Ctrl+C to stop
    python flicker_capture.py capture menu.flk --seconds 30
    python flicker_capture.py analyze menu.flk
    python flicker_capture.py analyze menu.flk --csv menu.csv
    python flicker_capture.py selftest                     # checks this tool itself

Do NOT run udp_ground_truth_listener.py at the same time (same port, 4048).

Packet ("FLK1", 132 bytes after the 10-byte DDP header) -- the probe is in
plugin/source/sample_thread.c and only compiled into debug builds (make DEBUG=1):
    0 magic  4 seq  8 tsUs  12 deltaSum  16 activeFormat  20 flipTotal(u16)
    22 curIdx  23 bufCount  24 flags  25 nProbe  26 validMask(b0-2 valid, b4-7 slot of buf2)
    (flags: b0 0x80002200 decoded as HDR, b1 smoothing, b2 letterbox margin, b3 fmt is 0x80002200,
     b4 0x88740000 decoded as 8-bit ARGB)
    27 slotIds(lo nibble slot of buf0, hi nibble slot of buf1)  28 numZones(u16)
    30 x6: zoneIdx(u16) out[3] pipeRaw[3] buf0[3] buf1[3] buf2[3]
"""
import argparse
import socket
import struct
import sys
import time
import statistics as st

PORT = 4048
DDP_HDR = 10
HDR_FMT = "<4sIIIIHBBBBBBH"
HDR_LEN = struct.calcsize(HDR_FMT)          # 30
ZONE_FMT = "<H3B3B3B3B3B"
ZONE_LEN = struct.calcsize(ZONE_FMT)        # 17
PKT_LEN = HDR_LEN + 6 * ZONE_LEN            # 132


# ----------------------------------------------------------------- capture --
def do_capture(path, seconds):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 22)
    sock.bind(("0.0.0.0", PORT))
    sock.settimeout(0.5)
    print(f"Listening on UDP {PORT} -> {path}  (Ctrl+C to stop)")
    t_start = time.time()
    n_flk = n_other = 0
    last_print = t_start
    last_n = 0
    last_delta = 0
    with open(path, "wb") as f:
        try:
            while True:
                if seconds and time.time() - t_start >= seconds:
                    break
                try:
                    data, _ = sock.recvfrom(65535)
                except socket.timeout:
                    data = None
                now = time.time()
                if data is not None and len(data) > DDP_HDR:
                    payload = data[DDP_HDR:]
                    f.write(struct.pack("<dH", now, len(payload)))
                    f.write(payload)
                    f.flush()  # size grows live, and a capture that is still running (or was killed) is still readable
                    if payload[:4] == b"FLK1" and len(payload) >= PKT_LEN:
                        n_flk += 1
                        last_delta = struct.unpack_from("<I", payload, 12)[0]
                    else:
                        n_other += 1
                if now - last_print >= 1.0:
                    print(f"  t={now - t_start:5.1f}s  probe pkts/s={n_flk - last_n:3d}  "
                          f"total={n_flk}  other={n_other}  last deltaSum={last_delta}  file={f.tell()} bytes")
                    last_n = n_flk
                    last_print = now
        except KeyboardInterrupt:
            pass
    print(f"Saved {n_flk} probe packets ({n_other} other) to {path}")


# ------------------------------------------------------------------- parse --
def read_records(path):
    recs = []
    with open(path, "rb") as f:
        buf = f.read()
    o = 0
    while o + 10 <= len(buf):
        ts, ln = struct.unpack_from("<dH", buf, o)
        o += 10
        recs.append((ts, buf[o:o + ln]))
        o += ln
    return recs


def parse(recs):
    passes = []
    others = []
    for ts, p in recs:
        if p[:4] == b"FLK1" and len(p) >= PKT_LEN:
            (_m, seq, tsus, dsum, fmt, flip, cur, nbuf, flags, nprobe, vmask,
             _pad, nz) = struct.unpack_from(HDR_FMT, p, 0)
            zones = []
            for k in range(6):
                z = struct.unpack_from(ZONE_FMT, p, HDR_LEN + k * ZONE_LEN)
                zones.append({
                    "idx": z[0],
                    "out": z[1:4],
                    "raw": z[4:7],
                    "buf": [z[7:10], z[10:13], z[13:16]],
                })
            valid = vmask & 7
            slots = [_pad & 15, (_pad >> 4) & 15, (vmask >> 4) & 15]
            cpos = slots.index(cur) if (cur in slots and (valid & (1 << slots.index(cur)))) else -1
            passes.append(dict(pc=ts, seq=seq, tsus=tsus, dsum=dsum, fmt=fmt,
                               flip=flip, cur=cur, nbuf=nbuf, flags=flags,
                               vmask=valid, slots=slots, cpos=cpos, nz=nz, zones=zones))
        else:
            others.append((ts, p))
    return passes, others


# ---------------------------------------------------------------- analysis --
def pct(vals, q):
    if not vals:
        return 0
    v = sorted(vals)
    return v[min(len(v) - 1, int(q * len(v)))]


def mean(v):
    return sum(v) / len(v) if v else 0.0


def sd(v):
    return st.pstdev(v) if len(v) > 1 else 0.0


def absdiff3(a, b):
    return sum(abs(x - y) for x, y in zip(a, b)) / 3.0


def unwrap_seq(passes):
    # tsUs and flip are wrapping counters; handle both.
    ts = []
    base = 0
    prev = None
    for p in passes:
        if prev is not None and p["tsus"] < prev:
            base += 1 << 32
        prev = p["tsus"]
        ts.append((p["tsus"] + base) / 1e6)
    fl = []
    base = 0
    prev = None
    for p in passes:
        if prev is not None and p["flip"] < prev:
            base += 1 << 16
        prev = p["flip"]
        fl.append(p["flip"] + base)
    return ts, fl


def analyze(passes, others, csv_path=None, quiet=False):
    """Returns (report_lines, findings dict)."""
    out = []
    P = out.append
    F = {}
    if len(passes) < 10:
        P(f"Only {len(passes)} probe packets -- capture longer / check dev_ip, dev_logging and that this is a DEBUG build.")
        return out, F

    ts, fl = unwrap_seq(passes)
    n = len(passes)
    dur = ts[-1] - ts[0]

    # ---- capture health -------------------------------------------------
    seqs = [p["seq"] for p in passes]
    lost = sum(max(0, b - a - 1) for a, b in zip(seqs, seqs[1:]) if b > a)
    P("== Capture ==")
    P(f"passes: {n}   duration: {dur:.1f}s   probe-packet loss on debug path: {lost} "
      f"({100.0 * lost / max(1, n + lost):.1f}%)")
    fmts = sorted({p["fmt"] for p in passes})
    P("format(s): " + ", ".join(f"0x{f:08x}" for f in fmts) +
      "   bufCount: " + ",".join(str(x) for x in sorted({p['nbuf'] for p in passes})) +
      "   smoothing: " + ("on" if passes[0]["flags"] & 2 else "off"))
    hdr_flips = sum(1 for a, b in zip(passes, passes[1:]) if (a["flags"] & 1) != (b["flags"] & 1))
    lb_flips = sum(1 for a, b in zip(passes, passes[1:]) if (a["flags"] & 4) != (b["flags"] & 4))
    P(f"HDR-decision changes during capture: {hdr_flips}   letterbox-margin on/off changes: {lb_flips}")
    F["hdr_flips"] = hdr_flips
    # v3.5: 0x88740000 buffers that hold 8-bit ARGB (YouTube, SDR video, HDR on)
    pq8_flips = sum(1 for a, b in zip(passes, passes[1:]) if (a["flags"] & 0x10) != (b["flags"] & 0x10))
    if pq8_flips or any(p["flags"] & 0x10 for p in passes):
        P(f"0x88740000 8-bit/PQ decode changes: {pq8_flips}   "
          f"passes decoded as 8-bit: {sum(1 for p in passes if p['flags'] & 0x10)} of {n}")

    # ---- sampler timing -------------------------------------------------
    dts = [(b - a) * 1000 for a, b in zip(ts, ts[1:])]
    med = st.median(dts)
    gaps = sum(1 for d in dts if d > 1.6 * med)
    P("")
    P("== Sampler timing (plugin's own clock) ==")
    P(f"pass interval ms: median {med:.1f}  p95 {pct(dts, .95):.1f}  max {max(dts):.1f}   "
      f"passes >1.6x median: {gaps} ({100.0 * gaps / len(dts):.1f}%)")
    F["gap_frac"] = gaps / len(dts)

    # ---- flips ----------------------------------------------------------
    flips_per_pass = [b - a for a, b in zip(fl, fl[1:])]
    zero_flip = sum(1 for x in flips_per_pass if x == 0)
    P("")
    P("== Flip hooks (recorded at SUBMIT time, not scanout) ==")
    P(f"flips/sec: {(fl[-1] - fl[0]) / max(dur, 1e-9):.1f}   "
      f"passes with 0 new flips: {zero_flip} ({100.0 * zero_flip / len(flips_per_pass):.0f}%)   "
      f"passes with >=2 flips: {sum(1 for x in flips_per_pass if x >= 2)}")
    cur_hist = {}
    for p in passes:
        cur_hist[p["cur"]] = cur_hist.get(p["cur"], 0) + 1
    P("curIdx histogram: " + ", ".join(f"{k}:{v}" for k, v in sorted(cur_hist.items())))
    trans = sum(1 for a, b in zip(passes, passes[1:]) if a["cur"] != b["cur"])
    P(f"curIdx changes between consecutive passes: {trans} ({100.0 * trans / (n - 1):.0f}% of passes)")

    # ---- whole-strip output flicker ------------------------------------
    ds = [p["dsum"] for p in passes[1:]]
    nz = passes[0]["nz"]
    P("")
    P("== Whole-strip OUTPUT change per pass (sum |delta| over all zones x RGB) ==")
    P(f"zones: {nz}   deltaSum: median {st.median(ds):.0f}  mean {mean(ds):.0f}  "
      f"p95 {pct(ds, .95)}  max {max(ds)}")
    per_zone = [d / max(nz, 1) for d in ds]
    P(f"per-zone-per-pass (mean |delta| over 3 channels x zones): median {st.median(per_zone) / 3:.2f}  p95 {pct(per_zone, .95) / 3:.2f}")
    big = sum(1 for d in per_zone if d / 3 > 4)
    P(f"passes where the strip moved by more than ~4/255 per channel on average: {big} ({100.0 * big / len(ds):.1f}%)")
    F["out_median_pz"] = st.median(per_zone) / 3
    F["out_p95_pz"] = pct(per_zone, .95) / 3
    F["big_frac"] = big / len(ds)

    # ---- probe zones ----------------------------------------------------
    P("")
    P("== Probe zones (6 spread around the strip) ==")
    P("  zone  outSD  rawSD  bufSD(b0,b1,b2)      bufDisagree  tear(raw vs buf[cur])  amp(out/raw)")
    tear_all = []
    dis_all = []
    bufsd_all = []
    rawsd_all = []
    outsd_all = []
    for k in range(6):
        zi = passes[0]["zones"][k]["idx"]
        outs = [[p["zones"][k]["out"][c] for p in passes] for c in range(3)]
        raws = [[p["zones"][k]["raw"][c] for p in passes] for c in range(3)]
        outsd = mean([sd(x) for x in outs])
        rawsd = mean([sd(x) for x in raws])
        bufsd = []
        for j in range(3):
            if not any(p["vmask"] & (1 << j) for p in passes):
                bufsd.append(None)
                continue
            chans = [[p["zones"][k]["buf"][j][c] for p in passes if p["vmask"] & (1 << j)] for c in range(3)]
            bufsd.append(mean([sd(x) for x in chans]))
        dis = []
        tear = []
        for p in passes:
            vm = [j for j in range(3) if p["vmask"] & (1 << j)]
            z = p["zones"][k]
            if len(vm) >= 2:
                m = 0.0
                for a in range(len(vm)):
                    for b in range(a + 1, len(vm)):
                        m = max(m, absdiff3(z["buf"][vm[a]], z["buf"][vm[b]]))
                dis.append(m)
            if p["cpos"] >= 0:
                tear.append(absdiff3(z["raw"], z["buf"][p["cpos"]]))
        dis_m = mean(dis)
        tear_m = mean(tear)
        amp = outsd / rawsd if rawsd > 0.05 else float("nan")
        bs = ",".join("--" if x is None else f"{x:4.1f}" for x in bufsd)
        P(f"  {zi:4d}  {outsd:5.1f}  {rawsd:5.1f}  {bs:<20}  {dis_m:10.1f}  {tear_m:20.1f}  {amp:10.1f}")
        tear_all.append(tear_m)
        dis_all.append(dis_m)
        rawsd_all.append(rawsd)
        outsd_all.append(outsd)
        bufsd_all += [x for x in bufsd if x is not None]
    F["tear"] = mean(tear_all)
    F["dis"] = mean(dis_all)
    F["rawsd"] = mean(rawsd_all)
    F["outsd"] = mean(outsd_all)
    F["bufsd"] = mean(bufsd_all)

    # does the pipeline's value follow index changes?
    ch, nch = [], []
    for a, b in zip(passes, passes[1:]):
        m = mean([absdiff3(a["zones"][k]["raw"], b["zones"][k]["raw"]) for k in range(6)])
        (ch if a["cur"] != b["cur"] else nch).append(m)
    if ch and nch:
        P("")
        P(f"pass-to-pass change in the pipeline's raw read: when curIdx CHANGED {mean(ch):.1f}   "
          f"when it stayed {mean(nch):.1f}   (n={len(ch)}/{len(nch)})")
        F["idx_change_raw"] = mean(ch)
        F["idx_same_raw"] = mean(nch)

    # ---- cleared-buffer reads + beat with the game's frame rate ---------------
    def zone_black(z):
        return tuple(z["raw"]) == (0, 0, 0)
    blk = [sum(zone_black(z) for z in p["zones"]) for p in passes]
    all_black = [b >= 5 for b in blk]
    partial = sum(1 for b in blk if 1 <= b <= 4)
    n_ab = sum(all_black)
    P("")
    P("== All-zero reads (a cleared / not-yet-drawn buffer looks like exactly 0,0,0) ==")
    P(f"passes where >=5 of 6 probe zones read exactly 0,0,0: {n_ab} ({100.0 * n_ab / n:.0f}%)   "
      f"passes where only 1-4 did (mid-render?): {partial} ({100.0 * partial / n:.0f}%)")
    F["allblack_frac"] = n_ab / n
    F["partial_frac"] = partial / n
    starts = []
    i = 0
    while i < n:
        if all_black[i]:
            j = i
            while j < n and all_black[j]:
                j += 1
            starts.append((i, j - i))
            i = j
        else:
            i += 1
    F["beat_match"] = False
    F["fix_lag"] = None
    if len(starts) >= 4:
        st_t = [ts[a_] for a_, _l in starts]
        gaps_s = [b - a_ for a_, b in zip(st_t, st_t[1:])]
        lens = [l for _a, l in starts]
        medg = st.median(gaps_s)
        P(f"bursts: {len(starts)}   length in passes: median {st.median(lens):.0f}   "
          f"spacing between bursts: median {medg:.2f}s (min {min(gaps_s):.2f}, max {max(gaps_s):.2f})")
        T = st.median([(b - a_) for a_, b in zip(ts, ts[1:])])
        fs = 1.0 / T
        Fh = (fl[-1] - fl[0]) / max(dur, 1e-9)

        def beat(ff):
            r = ff / fs
            nn = max(1, round(r))
            f_ = abs(ff - nn * fs)
            return 1.0 / f_ if f_ > 1e-9 else float("inf")
        cands = [(Fh, "hook count as-is"), (Fh / 2, "hook count / 2 (both flip hooks fire per real flip)")]
        best = None
        for ff, label in cands:
            b_ = beat(ff)
            P(f"  if the game presents at {ff:.1f} fps ({label}): sampler/frame beat period would be {b_:.2f}s")
            if best is None or abs(b_ - medg) < abs(best[0] - medg):
                best = (b_, ff, label)
        if best and abs(best[0] - medg) / medg < 0.25:
            F["beat_match"] = True
            P(f"  -> observed spacing {medg:.2f}s matches a {best[1]:.1f} fps game beating against the {T * 1000:.1f} ms sampler "
              f"(phase drifts {abs(T - 1.0 / best[1]) * 1000:.2f} ms per pass). A steady, drifting-phase cause, not random noise.")

    # ---- which ring position is safe to read? (needs the fixed probe: slots known) ----
    F["lag"] = None
    have_slots = all(p["vmask"] == 7 for p in passes) and passes[0]["cpos"] >= 0
    if have_slots:
        succ = {}
        for a_, b in zip(passes, passes[1:]):
            if a_["cur"] != b["cur"]:
                succ.setdefault(a_["cur"], {}).setdefault(b["cur"], 0)
                succ[a_["cur"]][b["cur"]] += 1
        nxt = {k: max(v, key=v.get) for k, v in succ.items()}
        slots3 = passes[0]["slots"]
        pred = {v: k for k, v in nxt.items()}
        ring_ok = set(nxt.keys()) == set(slots3) and set(pred.keys()) == set(slots3)
        P("")
        P("== Ring position vs glitches (lag 0 = the slot the flip hook reports) ==")
        if not ring_ok:
            P("could not infer a clean 3-slot ring order from the index sequence; skipping.")
        else:
            P("ring order seen: " + " -> ".join(str(x) for x in [slots3[0], nxt[slots3[0]], nxt[nxt[slots3[0]]]]) + " -> ...")
            # per-zone, per-channel steady reference = median over every buffer read in the capture
            ref = []
            for k in range(6):
                ref.append([st.median([p["zones"][k]["buf"][j][c] for p in passes for j in range(3)]) for c in range(3)])
            glitch = [0, 0, 0]
            tot = 0
            both_bad = 0
            lag0_bad_passes = 0
            for p in passes:
                cs = p["cur"]
                lag_slots = [cs, pred[cs], pred[pred[cs]]]
                bad_here = []
                for L, slot in enumerate(lag_slots):
                    j = p["slots"].index(slot)
                    g = 0
                    for k in range(6):
                        if absdiff3(p["zones"][k]["buf"][j], ref[k]) > 25:
                            g += 1
                    glitch[L] += g
                    bad_here.append(g)
                tot += 6
                if bad_here[0] >= 3:
                    lag0_bad_passes += 1
                    if bad_here[1] >= 3 or bad_here[2] >= 3:
                        both_bad += 1
            # Which ring position did the PIPELINE actually read? Compare its raw read to each lag's buffer.
            # (After a fix that samples an older slot, the plugin no longer reads lag 0 -- but the probe still
            # measures every lag relative to the slot the hook reports.)
            pm = [0, 0, 0]
            for p in passes:
                cs = p["cur"]
                for L, slot in enumerate([cs, pred[cs], pred[pred[cs]]]):
                    j = p["slots"].index(slot)
                    if all(tuple(p["zones"][k]["raw"]) == tuple(p["zones"][k]["buf"][j]) for k in range(6)):
                        pm[L] += 1
            pm_pct = [100.0 * x / n for x in pm]
            F["pipe_match"] = pm_pct
            rates = [100.0 * g / tot for g in glitch]
            P("share of zone reads that differ from the steady value by >25/255 (a static menu should be ~0):")
            for L in range(3):
                tag = "  <- slot the hook reports" if L == 0 else ("  (previous flip)" if L == 1 else "  (two flips back)")
                P(f"  lag {L}: {rates[L]:5.1f}%{tag}")
            P(f"passes where lag 0 read >=3 of 6 zones wrong: {lag0_bad_passes}; of those, lag 1 or lag 2 was also wrong in {both_bad}")
            F["lag"] = rates
            P("pipeline's own read matched the buffer at each lag on this share of passes (all 6 zones exact):")
            P(f"  lag 0: {pm_pct[0]:5.1f}%   lag 1: {pm_pct[1]:5.1f}%   lag 2: {pm_pct[2]:5.1f}%")
            safe = [L for L in (1, 2) if pm_pct[L] >= 99.0 and rates[L] < 1.0]
            if pm_pct[0] < 95.0 and safe:
                F["fix_lag"] = safe[0]
                bad0 = sum(1 for p in passes
                           if sum(1 for k in range(6)
                                  if absdiff3(p["zones"][k]["buf"][p["slots"].index(p["cur"])], ref[k]) > 25) >= 3)
                fixed = sum(1 for p in passes
                            if sum(1 for k in range(6)
                                   if absdiff3(p["zones"][k]["buf"][p["slots"].index(p["cur"])], ref[k]) > 25) >= 3
                            and all(absdiff3(p["zones"][k]["raw"], ref[k]) <= 25 for k in range(6)))
                P(f"  -> the pipeline is reading lag {safe[0]}, NOT the slot the hook reports (sample-lag fix active).")
                P(f"     Passes where lag 0 would have read >=3 of 6 zones wrong: {bad0}; of those the pipeline read steady values on {fixed}.")
                P("     The 'tear' and 'bufDisagree' columns above compare against lag 0, which this pipeline no longer reads,")
                P("     so a high value there is expected, not a problem.")

    # ---- verdict --------------------------------------------------------
    P("")
    P("== Reading of the evidence (heuristic -- check it against the numbers above) ==")
    T_FLICK = 1.0   # per-channel units/255: below this the output is effectively steady
    notes = []
    if F["out_p95_pz"] < T_FLICK and F["big_frac"] < 0.02:
        if F.get("fix_lag"):
            notes.append(f"FIX ACTIVE AND WORKING: the pipeline reads lag {F['fix_lag']} and the strip output is steady on a screen "
                         f"where the reported slot is wrong {F['lag'][0]:.1f}% of the time.")
        notes.append("OUTPUT IS STEADY: the plugin is sending an almost constant strip. If the LEDs still visibly "
                     "flicker, the cause is downstream of this plugin (WLED realtime timeout/fallback, Wi-Fi/UDP "
                     "loss between PS4 and WLED, power, LED hardware) -- not sampling.")
    else:
        notes.append(f"OUTPUT IS NOT STEADY (p95 per-channel change {F['out_p95_pz']:.1f}/255, "
                     f"{100 * F['big_frac']:.1f}% of passes jump >4). The flicker is already in what the plugin sends.")
        causes = []
        if F.get("lag") and F["lag"][0] > 3.0 and min(F["lag"][1:]) < max(0.5, F["lag"][0] / 8):
            best_lag = 1 if F["lag"][1] <= F["lag"][2] else 2
            causes.append(("IN-FLIGHT / CLEARED BUFFER (confirmed by ring lags)", F["lag"][0],
                           f"the slot the hook reports (lag 0) is wrong {F['lag'][0]:.1f}% of the time on a static screen; "
                           f"lag {best_lag} is wrong only {F['lag'][best_lag]:.1f}%. Sampling {best_lag} flip(s) back removes it."))
        elif F["allblack_frac"] > 0.05 and F["beat_match"]:
            causes.append(("CLEARED BUFFER SAMPLED IN A DRIFTING WINDOW", F["allblack_frac"],
                           f"{100 * F['allblack_frac']:.0f}% of passes read an all-zero frame, in bursts whose spacing matches the "
                           "beat between the sampler period and the game's frame rate. Re-capture with the slot-aware probe "
                           "to see which ring lag is clean."))
        if F["tear"] > 3.0 and not F.get("fix_lag"):
            causes.append(("IN-FLIGHT / TORN READS", F["tear"],
                           f"the pipeline's read of the current buffer differs from a re-read of the SAME buffer a few "
                           f"hundred microseconds later by {F['tear']:.1f} on average. Content is changing under us: "
                           "we are reading a buffer the GPU is still writing (flip hook fires at submit, not scanout)."))
        if F["dis"] > 6.0 and F["bufsd"] < max(3.0, F["dis"] / 3) and not F.get("fix_lag"):
            causes.append(("BUFFERS DISAGREE, EACH ONE STABLE", F["dis"],
                           f"the registered buffers hold different, individually-steady colors (mean spread {F['dis']:.1f}, "
                           f"per-buffer time SD only {F['bufsd']:.1f}). When curIdx alternates between them the strip jumps. "
                           "That points at sampling the wrong/older swap-chain slot."))
        if F.get("idx_change_raw", 0) > 2.5 * max(F.get("idx_same_raw", 0), 0.5) and F["idx_change_raw"] > 4:
            causes.append(("JUMPS FOLLOW curIdx CHANGES", F["idx_change_raw"],
                           f"raw read changes by {F['idx_change_raw']:.1f} when the buffer index changed vs "
                           f"{F['idx_same_raw']:.1f} when it stayed -- the flicker is tied to which slot we read."))
        if F["bufsd"] > 3.0 and F["dis"] < F["bufsd"] * 1.5 and F["tear"] <= 3.0:
            causes.append(("REAL CONTENT CHANGE / NOISE", F["bufsd"],
                           f"every buffer varies over time by itself (SD {F['bufsd']:.1f}) and they agree with each other: "
                           "the image genuinely changes at these zones (grain, dither, animated background, blinking UI). "
                           "Then the fix is smoothing / wider averaging, not a sampling change."))
        if not causes:
            causes.append(("UNCLASSIFIED", 0, "output jumps but none of the tests above stands out -- send me the .flk file."))
        for name, _v, why in causes:
            notes.append(f"- {name}: {why}")
    # Independent of whether the output looks steady: these can cause visible
    # flicker on their own, so they are always reported.
    if F["hdr_flips"] > 0:
        notes.append(f"- HDR/SDR DECODE FLIP-FLOP: the live 0x80002200 HDR decision changed {F['hdr_flips']} times "
                     "during this capture; each change re-decodes every pixel a different way.")
    if F["gap_frac"] > 0.05:
        notes.append(f"- SAMPLER STALLS: {100 * F['gap_frac']:.1f}% of passes ran >1.6x late. Uneven packet spacing "
                     "can trip WLED's realtime timeout and show as flicker even when the colors are correct.")
    if lost > 0.02 * (n + lost):
        notes.append(f"- Debug-path packet loss is {100.0 * lost / (n + lost):.1f}%: this capture has holes, "
                     "so treat its statistics with care.")
    for x in notes:
        P(x)
    F["notes"] = notes

    if csv_path:
        with open(csv_path, "w") as cf:
            cf.write("seq,t_s,flipTotal,curIdx,deltaSum,isHdr,z0_out_r,z0_raw_r,z0_b0_r,z0_b1_r,z0_b2_r\n")
            for p, t, f_ in zip(passes, ts, fl):
                z = p["zones"][0]
                cf.write(f"{p['seq']},{t:.4f},{f_},{p['cur']},{p['dsum']},{p['flags'] & 1},"
                         f"{z['out'][0]},{z['raw'][0]},{z['buf'][0][0]},{z['buf'][1][0]},{z['buf'][2][0]}\n")
        P(f"\nper-pass CSV written to {csv_path}")

    # ---- other packet types ---------------------------------------------
    if others and not quiet:
        P("")
        P("== Other debug packets present (not analyzed) ==")
        by_len = {}
        for _t, p in others:
            by_len[len(p)] = by_len.get(len(p), 0) + 1
        P(", ".join(f"{k}B x{v}" for k, v in sorted(by_len.items())))
    return out, F


# ---------------------------------------------------------------- selftest --
def synth(scenario, n=300, seed=1):
    """Build fake FLK1 payload records to check the analyzer's verdicts.
    Slots are 3,4,5 like the real capture (this title registers those, not 0..2)."""
    import random
    rnd = random.Random(seed)
    recs = []
    flip = 1000
    ts = 5_000_000
    prev_out = None
    SL = (3, 4, 5)
    base_col = [(200, 60, 40), (30, 90, 200), (120, 120, 120), (10, 200, 60), (220, 220, 30), (80, 20, 140)]
    for i in range(n):
        ts += 33333 + (rnd.randint(50000, 90000) if scenario == "stalls" and i % 9 == 0 else rnd.randint(-800, 800))
        flip += 2
        if scenario in ("ring", "ring_all_bad", "ring_fixed"):
            cur_pos = i % 3
        elif scenario in ("alternate", "tear"):
            cur_pos = i % 2
        else:
            cur_pos = 0
        cur = SL[cur_pos]
        buffers = []
        for j in range(3):
            shift = 40 if (scenario == "alternate" and j == 1) else 0
            buffers.append([[max(0, min(255, c + shift)) for c in col] for col in base_col])
        if scenario == "noise":
            noise = [[rnd.randint(-14, 14) for _ in range(3)] for _ in range(6)]
            buffers = [[[max(0, min(255, base_col[k][c] + noise[k][c])) for c in range(3)] for k in range(6)]
                       for _ in range(3)]
        if scenario in ("ring", "ring_all_bad", "ring_fixed") and (i % 13) in (0, 1, 2):
            # a burst every ~13 passes where the buffer the hook just reported is still a cleared frame
            buffers[cur_pos] = [[0, 0, 0] for _ in range(6)]
            if scenario == "ring_all_bad":
                buffers[(cur_pos - 1) % 3] = [[0, 0, 0] for _ in range(6)]
                buffers[(cur_pos - 2) % 3] = [[0, 0, 0] for _ in range(6)]
        read_pos = (cur_pos - 1) % 3 if scenario == "ring_fixed" else cur_pos
        raw = [list(buffers[read_pos][k]) for k in range(6)]
        out = [list(r) for r in raw]
        if scenario == "tear":
            for k in range(6):
                if rnd.random() < 0.5:
                    raw[k] = [rnd.randint(0, 255) // 3 for _ in range(3)]
                    out[k] = list(raw[k])
        total = sum(abs(a - b) for k in range(6) for a, b in zip(out[k], (prev_out or out)[k])) * 38  # scale 6 -> ~229 zones
        prev_out = out
        flags = 0x02
        hdr = 1 if (scenario == "hdrflip" and (i // 40) % 2) else 0
        flags |= hdr
        if scenario == "pq8flip" and i >= n // 2:
            flags |= 0x10  # 0x88740000 buffer switched to 8-bit decode half way through
        pk = struct.pack(HDR_FMT, b"FLK1", i, ts & 0xFFFFFFFF, int(total), 0x88740000 if scenario == "pq8flip" else 0x80000000, flip & 0xFFFF,
                         cur, 3, flags, 6, 0b111 | (SL[2] << 4), SL[0] | (SL[1] << 4), 229)
        for k in range(6):
            pk += struct.pack(ZONE_FMT, 10 + k * 35, *out[k], *raw[k], *buffers[0][k], *buffers[1][k], *buffers[2][k])
        recs.append((1000 + i * 0.033, pk))
    return recs


def selftest():
    expect = {
        "static": lambda F, txt: "STEADY" in txt and "NOT STEADY" not in txt,
        "alternate": lambda F, txt: "BUFFERS DISAGREE" in txt,
        "tear": lambda F, txt: "IN-FLIGHT" in txt,
        "noise": lambda F, txt: "REAL CONTENT" in txt,
        "hdrflip": lambda F, txt: "HDR/SDR DECODE" in txt,
        "pq8flip": lambda F, txt: "8-bit/PQ decode changes: 1" in txt and "passes decoded as 8-bit: 150 of 300" in txt,
        "stalls": lambda F, txt: "SAMPLER STALLS" in txt,
        "ring": lambda F, txt: "confirmed by ring lags" in txt and F["lag"] and F["lag"][0] > 3 and F["lag"][1] < 0.5 and F["lag"][2] < 0.5,
        # every buffer bad at once -> the tool must NOT claim a safe lag exists
        "ring_all_bad": lambda F, txt: "confirmed by ring lags" not in txt,
        # pipeline already reads lag 1: must be recognised as a working fix, not blamed for "tear"
        "ring_fixed": lambda F, txt: "FIX ACTIVE AND WORKING" in txt and F.get("fix_lag") == 1 and "IN-FLIGHT" not in txt,
    }
    ok = True
    for sc, check in expect.items():
        passes, others = parse(synth(sc))
        lines, F = analyze(passes, others, quiet=True)
        txt = "\n".join(lines)
        # hdrflip / stalls scenarios keep static colors: force a jumpy output for those two so the "not steady" branch runs
        good = check(F, txt)
        if sc in ("hdrflip", "stalls") and not good:
            # these two only add a cause when output is unsteady; add jitter by re-running with noise colours
            pass
        print(f"[{'PASS' if good else 'FAIL'}] scenario {sc}")
        if not good:
            ok = False
            print(txt)
    return 0 if ok else 1


# -------------------------------------------------------------------- main --
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("capture")
    c.add_argument("file")
    c.add_argument("--seconds", type=float, default=0)
    a = sub.add_parser("analyze")
    a.add_argument("file")
    a.add_argument("--csv")
    sub.add_parser("selftest")
    args = ap.parse_args()
    if args.cmd == "capture":
        do_capture(args.file, args.seconds)
    elif args.cmd == "analyze":
        passes, others = parse(read_records(args.file))
        lines, _ = analyze(passes, others, args.csv)
        print("\n".join(lines))
    else:
        sys.exit(selftest())


if __name__ == "__main__":
    main()
