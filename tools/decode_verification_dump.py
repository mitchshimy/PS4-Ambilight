#!/usr/bin/env python3
"""
decode_verification_dump.py

Decodes the raw payload_hex lines printed by udp_ground_truth_listener.py
when detile_verify_probe.prx sends its verification dump.

Packet format, v2.1 probe (11 bytes), one per (test point, param set):
    [0]     point index
    [1]     paramset id (0=base, 1=neo)
    [2..3]  x, uint16 LE
    [4..5]  y, uint16 LE
    [6..9]  raw 4 bytes read from the tiled buffer at the computed offset
            (packed A2R10G10B10_SRGB, little-endian uint32)
    [10]    displayBufferIndex the read was actually taken from (added in
            the v2.1 probe fix -- the swap-chain-buffer bug where every
            dump used to read a fixed buffer[0] regardless of which
            buffer was actually live)

Still accepts old 10-byte packets (pre-v2.1 probe, no [10] byte) for
backward compatibility -- displayBufferIndex just shows as "?" for those.

v2.2 probe also emits a second, 32-byte packet type -- one per
sceVideoOutRegisterBuffers call (not just the first), reporting that
call's pixelFormat/tmode/width/height/pitch/first-buffer-address. This
lets you see whether the game re-registered its swap chain (different
format, different scene) between an earlier format capture and the
current pixel-sample dump, instead of silently trusting a possibly
stale format assumption. Distinguished from the 11-byte pixel packets
purely by length -- just paste both kinds into the same PAYLOADS list.

USAGE:
    Paste each captured payload_hex string as a line into PAYLOADS below
    (or pipe them in -- see __main__), then run this script. It will
    print a table: point index, x, y, paramset, raw hex, decoded RGB888,
    and which display buffer the read came from. Points sharing the same
    (x,y) but different paramset are grouped together so base vs Neo can
    be compared directly.

    Compare the printed RGB against what's actually showing at that
    screen coordinate on your test screen. Whichever paramset matches
    is the correct one -- and confirms/refutes Neo mode per handoff §7.

    All ten packets from one dump should share the SAME
    displayBufferIndex (one press = one frame = one live buffer) -- the
    script warns if they don't, since that would mean the buffer flipped
    mid-dump or something's still off with buffer selection.
"""

import sys
import struct

# Paste captured payload_hex values here (from udp_ground_truth_listener.py's
# "payload_hex=..." lines), one string per packet. Each should decode to
# 11 bytes (22 hex chars) from the v2.1 probe, or 10 bytes (20 hex chars)
# from an older capture. Order doesn't matter -- the script sorts by point
# index automatically.
PAYLOADS = [
    # "00000000ff00..."  <- example placeholder, replace with real captures
]


def unpack_a2r10g10b10_srgb(raw4: bytes):
    """Unpack a little-endian uint32 A2R10G10B10_SRGB value.
    Bit layout per handoff §7 (red-prig/fpPS4 comment): MSB first, blue at
    LSB -- bits 31-30 alpha, 29-20 red, 19-10 green, 9-0 blue."""
    px = struct.unpack("<I", raw4)[0]
    a2 = (px >> 30) & 0x3
    r10 = (px >> 20) & 0x3FF
    g10 = (px >> 10) & 0x3FF
    b10 = (px >> 0) & 0x3FF
    # simple 10-bit -> 8-bit truncation, no gamma decode (raw sRGB-encoded
    # values as captured -- fine for a first correctness check, note this
    # if you need display-accurate color later)
    r8 = r10 >> 2
    g8 = g10 >> 2
    b8 = b10 >> 2
    return a2, (r8, g8, b8)


def decode_registration_packet(data: bytes):
    """v2.2 registration-dump packet (32 bytes): reports every
    sceVideoOutRegisterBuffers call, not just the first, so a
    re-registration with a different format/size between an earlier
    capture and this one is visible instead of silently ignored."""
    call_idx, start_idx, buf_num = data[0], data[1], data[2]
    fmt, tmode, width, height, pitch = struct.unpack("<iiIII", data[4:24])
    addr = struct.unpack("<Q", data[24:32])[0]
    return {
        "kind": "registration",
        "call_idx": call_idx,
        "start_idx": start_idx,
        "buf_num": buf_num,
        "format": fmt,
        "tmode": tmode,
        "width": width,
        "height": height,
        "pitch": pitch,
        "addr": addr,
    }


def decode_packet(hexstr: str):
    data = bytes.fromhex(hexstr.strip())
    if len(data) == 32:
        return decode_registration_packet(data)
    if len(data) < 10:
        raise ValueError(f"packet too short: {len(data)} bytes, need 10, 11, or 32")
    point_idx = data[0]
    paramset = data[1]
    x = struct.unpack("<H", data[2:4])[0]
    y = struct.unpack("<H", data[4:6])[0]
    raw4 = data[6:10]
    a2, rgb = unpack_a2r10g10b10_srgb(raw4)
    display_buffer_index = data[10] if len(data) >= 11 else None
    return {
        "kind": "pixel",
        "point_idx": point_idx,
        "paramset": "base" if paramset == 0 else "neo" if paramset == 1 else f"?{paramset}",
        "x": x,
        "y": y,
        "raw_hex": raw4.hex(),
        "alpha2": a2,
        "rgb888": rgb,
        "display_buffer_index": display_buffer_index,
    }


def main(payloads):
    if not payloads:
        print("No payloads to decode. Paste captured payload_hex strings into")
        print("PAYLOADS at the top of this script, or pipe hex lines on stdin:")
        print("  echo '<hex>' | python3 decode_verification_dump.py")
        return

    all_results = [decode_packet(p) for p in payloads]
    reg_events = sorted([r for r in all_results if r["kind"] == "registration"],
                         key=lambda r: r["call_idx"])
    results = [r for r in all_results if r["kind"] == "pixel"]

    KNOWN_FORMATS = {
        0x88000000: "A2R10G10B10_SRGB",
        0x88060000: "A2R10G10B10",
        0x88740000: "A2R10G10B10_BT2020_PQ",
        0x80000000: "A8R8G8B8_SRGB",
        0x80002200: "A8B8G8R8_SRGB",
        -0x7F000000 & 0xFFFFFFFF: "A16R16G16B16_FLOAT",  # 0xC1060000 as unsigned
    }

    if reg_events:
        print(f"{len(reg_events)} registration event(s) captured this session:")
        print(f"{'call#':>5} {'start':>5} {'num':>4} {'format':>12} {'name':>22} "
              f"{'w':>5} {'h':>5} {'pitch':>6} {'addr':>16}")
        print("-" * 90)
        for r in reg_events:
            fmt_u = r["format"] & 0xFFFFFFFF
            name = KNOWN_FORMATS.get(fmt_u, "?unknown")
            print(f"{r['call_idx']:>5} {r['start_idx']:>5} {r['buf_num']:>4} "
                  f"{fmt_u:#012x} {name:>22} {r['width']:>5} {r['height']:>5} "
                  f"{r['pitch']:>6} {r['addr']:#018x}")
        distinct_formats = {r["format"] & 0xFFFFFFFF for r in reg_events}
        print()
        if len(distinct_formats) > 1:
            print("NOTE: multiple DIFFERENT pixel formats registered this session --")
            print("the format live at capture time is whichever call has the HIGHEST")
            print("call# with start_idx/buf_num covering the slot displayBufferIndex")
            print("pointed to during the pixel-sample dump below. Don't assume the")
            print("first (or an earlier session's) capture still applies.")
        else:
            print(f"Only one distinct format registered this session: "
                  f"{KNOWN_FORMATS.get(next(iter(distinct_formats)), '?unknown')}. "
                  f"If the pixel data below still doesn't match this format's channel")
            print("layout, the format isn't the explanation -- look at buffer/address")
            print("resolution (displayBufferIndex) instead.")
        print()

    if not results:
        return

    print(f"{'pt':>2} {'x':>5} {'y':>5} {'set':>5} {'raw':>10} {'RGB888':>16} {'bufIdx':>6}")
    print("-" * 58)
    last_point = None
    for r in results:
        if last_point is not None and r["point_idx"] != last_point:
            print()
        buf_str = str(r["display_buffer_index"]) if r["display_buffer_index"] is not None else "?"
        print(f"{r['point_idx']:>2} {r['x']:>5} {r['y']:>5} {r['paramset']:>5} "
              f"{r['raw_hex']:>10} {str(r['rgb888']):>16} {buf_str:>6}")
        last_point = r["point_idx"]

    print()
    print("Compare each RGB888 against what's actually at that (x,y) on your")
    print("test screen. Whichever paramset (base/neo) is right for ALL points")
    print("is the confirmed-correct one.")

    # Sanity check for the v2.1 probe fix: every packet in one dump should
    # share the same displayBufferIndex, since one combo press = one live
    # frame = one buffer. If they differ, buffer selection is still off
    # (or packets from two different presses got mixed together).
    buf_indices = {r["display_buffer_index"] for r in results}
    buf_indices.discard(None)
    if None in (r["display_buffer_index"] for r in results):
        print()
        print("Note: some packets have no displayBufferIndex byte (10-byte,")
        print("pre-v2.1 probe format) -- can't run the buffer-consistency check.")
    elif len(buf_indices) > 1:
        print()
        print(f"WARNING: packets report {len(buf_indices)} different displayBufferIndex")
        print(f"values ({sorted(buf_indices)}) within what should be one dump. Either")
        print("packets from separate presses got mixed together, or the buffer")
        print("flipped mid-dump -- treat this capture as suspect.")
    elif len(buf_indices) == 1:
        print()
        print(f"displayBufferIndex consistent across all packets: buffer {buf_indices.pop()}.")


if __name__ == "__main__":
    payloads = list(PAYLOADS)
    if not sys.stdin.isatty():
        for line in sys.stdin:
            line = line.strip()
            if line:
                payloads.append(line)
    main(payloads)
