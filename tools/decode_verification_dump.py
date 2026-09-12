#!/usr/bin/env python3
"""
decode_verification_dump.py  (v3)

Decodes the raw payload_hex lines printed by udp_ground_truth_listener.py
when detile_verify_probe.prx sends its verification dump.

Packet format, v2.1 probe (11 bytes), one per (test point, param set):
    [0]     point index
    [1]     paramset id (0=base, 1=neo)
    [2..3]  x, uint16 LE
    [4..5]  y, uint16 LE
    [6..9]  raw 4 bytes read from the tiled buffer at the computed offset
            (packed pixel value, little-endian uint32 -- exact channel
            layout depends on the *actual* registered pixel format, see
            below; it is NOT assumed to be A2R10G10B10 anymore)
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

WHAT'S NEW IN v3
----------------
Previously this script always unpacked the 4 raw pixel bytes as
A2R10G10B10_SRGB, no matter what. That was wrong as soon as a real
registration-event capture showed a different format actually live
(A8R8G8B8_SRGB, in the first real capture that included registration
packets) -- 10-bit-per-channel math applied to 8-bit-per-channel data
produces exactly the "everything pinned near max" symptom that showed
up before this fix.

Now: if the payload set includes one or more 32-byte registration
packets, the script picks an unpack function based on the *actual*
captured format (highest call_idx wins if more than one call is
present -- see the "multiple different formats" note below) and uses
that for every pixel packet. If no registration packets are present
at all, it falls back to A2R10G10B10_SRGB and prints an explicit
warning that this is an assumption, not a measurement.

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

    All packets from one dump should share the SAME displayBufferIndex
    (one press = one frame = one live buffer) -- the script warns if
    they don't, since that would mean the buffer flipped mid-dump or
    something's still off with buffer selection.
"""

import sys
import struct

# Paste captured payload_hex values here (from udp_ground_truth_listener.py's
# "payload_hex=..." lines), one string per packet. Each should decode to
# 11 bytes (22 hex chars) from the v2.1 probe, 10 bytes (20 hex chars)
# from an older capture, or 32 bytes (64 hex chars) for a v2.2
# registration packet. Order doesn't matter -- the script sorts by point
# index / call index automatically.
PAYLOADS = [
    # "00000000ff00..."  <- example placeholder, replace with real captures
]


# ---------------------------------------------------------------------------
# Per-format unpack functions. Each takes the raw 4 bytes (as read off the
# wire, little-endian uint32) and returns (alpha_bits_or_None, (r8, g8, b8)).
# ---------------------------------------------------------------------------

def unpack_a2r10g10b10(raw4: bytes):
    """A2R10G10B10 / A2R10G10B10_SRGB / A2R10G10B10_BT2020_PQ.
    Bit layout per handoff §7 (red-prig/fpPS4 comment): MSB first, blue at
    LSB -- bits 31-30 alpha, 29-20 red, 19-10 green, 9-0 blue. Simple
    10-bit -> 8-bit truncation, no gamma/PQ decode -- fine for a first
    correctness check, note this if display-accurate color is needed."""
    px = struct.unpack("<I", raw4)[0]
    a2 = (px >> 30) & 0x3
    r10 = (px >> 20) & 0x3FF
    g10 = (px >> 10) & 0x3FF
    b10 = (px >> 0) & 0x3FF
    return a2, (r10 >> 2, g10 >> 2, b10 >> 2)


def unpack_a8r8g8b8(raw4: bytes):
    """A8R8G8B8 / A8R8G8B8_SRGB. Little-endian uint32 = 0xAARRGGBB, so
    byte0=B (LSB), byte1=G, byte2=R, byte3=A (MSB)."""
    px = struct.unpack("<I", raw4)[0]
    a8 = (px >> 24) & 0xFF
    r8 = (px >> 16) & 0xFF
    g8 = (px >> 8) & 0xFF
    b8 = px & 0xFF
    return a8, (r8, g8, b8)


def unpack_a8b8g8r8(raw4: bytes):
    """A8B8G8R8 / A8B8G8R8_SRGB -- channel order reversed relative to
    A8R8G8B8: little-endian uint32 = 0xAABBGGRR."""
    px = struct.unpack("<I", raw4)[0]
    a8 = (px >> 24) & 0xFF
    b8 = (px >> 16) & 0xFF
    g8 = (px >> 8) & 0xFF
    r8 = px & 0xFF
    return a8, (r8, g8, b8)


# Maps the raw format enum value (as read off the wire, masked to 32 bits)
# to (display name, unpack function). Formats with no unpack function are
# known/named but not yet supported for auto-decode (e.g. float formats
# need a different raw4 size / decode path entirely).
FORMAT_TABLE = {
    0x88000000: ("A2R10G10B10_SRGB", unpack_a2r10g10b10),
    0x88060000: ("A2R10G10B10", unpack_a2r10g10b10),
    0x88740000: ("A2R10G10B10_BT2020_PQ", unpack_a2r10g10b10),
    0x80000000: ("A8R8G8B8_SRGB", unpack_a8r8g8b8),
    0x80002200: ("A8B8G8R8_SRGB", unpack_a8b8g8r8),
    -0x7F000000 & 0xFFFFFFFF: ("A16R16G16B16_FLOAT", None),  # 0xC1060000
}

DEFAULT_FALLBACK_FORMAT = 0x88000000  # A2R10G10B10_SRGB -- old script's hardcoded assumption


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


def decode_pixel_packet_raw(data: bytes):
    """Parses everything except the color -- color is unpacked later,
    once the active format for this dump is known."""
    point_idx = data[0]
    paramset = data[1]
    x = struct.unpack("<H", data[2:4])[0]
    y = struct.unpack("<H", data[4:6])[0]
    raw4 = data[6:10]
    display_buffer_index = data[10] if len(data) >= 11 else None
    return {
        "kind": "pixel",
        "point_idx": point_idx,
        "paramset": "base" if paramset == 0 else "neo" if paramset == 1 else f"?{paramset}",
        "x": x,
        "y": y,
        "raw4": raw4,
        "raw_hex": raw4.hex(),
        "display_buffer_index": display_buffer_index,
    }


def decode_packet(hexstr: str):
    data = bytes.fromhex(hexstr.strip())
    if len(data) == 32:
        return decode_registration_packet(data)
    if len(data) < 10:
        raise ValueError(f"packet too short: {len(data)} bytes, need 10, 11, or 32")
    return decode_pixel_packet_raw(data)


def pick_active_format(reg_events):
    """Choose which registered format to use for unpacking this dump's
    pixel packets. Per the script's own guidance: the live format is
    whichever registration call has the highest call_idx (most recent
    call wins) -- this does NOT check start_idx/buf_num against the
    pixel packets' displayBufferIndex, so if multiple *different*
    formats were registered, treat the choice as a best guess and
    verify manually."""
    if not reg_events:
        return None
    latest = max(reg_events, key=lambda r: r["call_idx"])
    return latest["format"] & 0xFFFFFFFF


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

    if reg_events:
        print(f"{len(reg_events)} registration event(s) captured this session:")
        print(f"{'call#':>5} {'start':>5} {'num':>4} {'format':>12} {'name':>22} "
              f"{'w':>5} {'h':>5} {'pitch':>6} {'addr':>16}")
        print("-" * 90)
        for r in reg_events:
            fmt_u = r["format"] & 0xFFFFFFFF
            name = FORMAT_TABLE.get(fmt_u, (f"?unknown ({fmt_u:#010x})", None))[0]
            print(f"{r['call_idx']:>5} {r['start_idx']:>5} {r['buf_num']:>4} "
                  f"{fmt_u:#012x} {name:>22} {r['width']:>5} {r['height']:>5} "
                  f"{r['pitch']:>6} {r['addr']:#018x}")
        distinct_formats = {r["format"] & 0xFFFFFFFF for r in reg_events}
        print()
        if len(distinct_formats) > 1:
            print("NOTE: multiple DIFFERENT pixel formats registered this session --")
            print("using the HIGHEST call# as the active format for auto-decode below")
            print("(most recent registration wins). This does NOT check displayBufferIndex")
            print("against start_idx/buf_num, so treat the decode below as a best guess")
            print("and verify manually if results look off.")
        else:
            fmt_name = FORMAT_TABLE.get(next(iter(distinct_formats)), ("?unknown", None))[0]
            print(f"Only one distinct format registered this session: {fmt_name}.")
            print("Using it to auto-decode the pixel packets below.")
        print()

    active_fmt = pick_active_format(reg_events)
    used_fallback = False
    if active_fmt is None:
        active_fmt = DEFAULT_FALLBACK_FORMAT
        used_fallback = True

    fmt_name, unpack_fn = FORMAT_TABLE.get(active_fmt, (f"?unknown ({active_fmt:#010x})", None))

    if used_fallback:
        print(f"WARNING: no registration packets in this payload set -- falling back to")
        print(f"the old hardcoded assumption ({fmt_name}). This is NOT a measurement,")
        print(f"just a guess. Include the 32-byte registration packets from the same")
        print(f"capture session to get an auto-selected, verified format instead.")
        print()

    if unpack_fn is None:
        print(f"Active format is {fmt_name}, which this script doesn't have an unpack")
        print(f"function for yet (e.g. a float format needs different handling than a")
        print(f"packed-int one). Showing raw hex only -- add an unpack_* function and")
        print(f"a FORMAT_TABLE entry for this format to get decoded RGB.")
        print()

    if not results:
        return

    print(f"Decoding pixel packets as: {fmt_name}")
    print()
    print(f"{'pt':>2} {'x':>5} {'y':>5} {'set':>5} {'raw':>10} {'RGB888':>16} {'bufIdx':>6}")
    print("-" * 58)
    last_point = None
    for r in results:
        if last_point is not None and r["point_idx"] != last_point:
            print()
        buf_str = str(r["display_buffer_index"]) if r["display_buffer_index"] is not None else "?"
        if unpack_fn is not None:
            _, rgb = unpack_fn(r["raw4"])
            rgb_str = str(rgb)
        else:
            rgb_str = "n/a"
        print(f"{r['point_idx']:>2} {r['x']:>5} {r['y']:>5} {r['paramset']:>5} "
              f"{r['raw_hex']:>10} {rgb_str:>16} {buf_str:>6}")
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
