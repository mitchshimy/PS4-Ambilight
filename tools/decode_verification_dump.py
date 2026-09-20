#!/usr/bin/env python3
"""
decode_verification_dump.py  (v3)

Decodes the raw payload_hex lines printed by udp_ground_truth_listener.py
when detile_verify_probe.prx sends its verification dump.

Packet format, v2.1 probe (11 bytes), one per (test point, param set):
    [0]     point index
    [1]     paramset id (0=base, 1=neo, 2=linear -- v2.6 probe only)
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

ps4_ambient_light.prx v1.2 also emits a 24-byte telemetry packet type,
one per ~1s window of ambient_sample_thread's own loop time (handoff
§30 step 2): min/max/avg microseconds, sample count, and how many
iterations in that window exceeded the ~30Hz budget. Also distinguished
purely by length -- paste these into PAYLOADS alongside everything else.

ps4_ambient_light.prx v2.1.2 also emits a 44-byte config-reload
diagnostic packet, one per ambient_check_config_reload() call, added
because the v2.1.1 mtime-sentinel fix didn't make live reload work on
real hardware. Reports which branch that function took (disabled /
stat() failed / baseline established / no change / change detected),
the full 64-bit mtime and size values it compared, and a running call
count -- so a capture shows exactly where reload is getting stuck
instead of guessing again. Also distinguished purely by length.

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
    """A8B8G8R8 / A8B8G8R8_SRGB (format 0x80002200).

    v3.1: NOW ACTUALLY SWAPS R and B vs. unpack_a8r8g8b8 -- reversing
    the v2.7.1 revert. That revert was based on a real hardware report
    ("red and blue are wrong") on a busy in-game scene, given
    uncertainly and without HDR controlled for. Confirmed instead
    against a real screenshot, sampled pixel-for-pixel at 5 known
    coordinates, WITH HDR OFF: this swapped version matched the real
    screen within single-digit-to-teens RGB units at all 5 points
    simultaneously -- the no-swap version never matched at any point,
    on any screen, across the whole investigation. Format ID itself
    was confirmed unchanged (0x80002200) between HDR on and off via a
    fresh registration event, so this isn't a different format --
    HDR was corrupting the actual buffer content (garbage-looking
    alpha byte, among other things), not just the color-channel
    mapping. CONFIRMED FOR HDR-OFF ONLY. HDR-on behavior for this
    format is still unverified and likely needs separate handling.
    """
    px = struct.unpack("<I", raw4)[0]
    a8 = (px >> 24) & 0xFF
    b8 = (px >> 16) & 0xFF
    g8 = (px >> 8) & 0xFF
    r8 = px & 0xFF
    return a8, (r8, g8, b8)


# ---------------------------------------------------------------------------
# HDR (A2R10G10B10_BT2020_PQ) decode -- added when HDR colors were reported
# wrong. Root cause: this format was previously routed to the SAME plain
# 10-bit truncation as SDR A2R10G10B10 (see unpack_a2r10g10b10 above) --
# defensible for gamma-encoded SDR content per that function's own
# docstring, but PQ (SMPTE ST 2084) is a fundamentally different, much
# steeper curve. Truncating raw PQ code values straight to 8-bit RGB has
# no meaningful relationship to the actual displayed color.
#
# Pipeline: PQ code value -> linear light (nits) -> tone-map down from
# PQ's 10,000-nit range -> BT.2020 -> BT.709/sRGB primaries -> sRGB gamma
# encode -> 8-bit. Bit LAYOUT (which 10 bits are R/G/B/A, MSB-first,
# blue at LSB) is unchanged from unpack_a2r10g10b10 -- HDR doesn't change
# how the tiling/channel packing works, only how the numbers should be
# interpreted once unpacked.
#
# PQ_REFERENCE_WHITE_NITS / PQ_TONE_MAP_MAX_NITS below are ASSUMPTIONS,
# not measured constants. The PQ EOTF and BT.2020->BT.709 matrix are
# fixed standards (SMPTE ST 2084 / ITU-R BT.2087) and shouldn't need
# tuning, but where to set "reference white" and how hard to roll off
# highlights depends on what THIS console/title is actually doing with
# the format, which nothing here has measured yet -- tune these against
# a real capture + known on-screen color, the exact same empirical method
# that settled base vs Neo in §21/§22. main() also prints the OLD naive
# truncation next to this decode when this format is active, so you can
# compare both against the screen the same way base vs Neo were compared.
PQ_REFERENCE_WHITE_NITS = 203.0  # ITU-R BT.2408 HDR reference white -- a reasonable starting point, not measured
PQ_TONE_MAP_MAX_NITS = 1000.0    # assumed content/display peak for the soft-rolloff below -- not measured

_PQ_M1 = 2610.0 / 16384.0
_PQ_M2 = 2523.0 / 4096.0 * 128.0
_PQ_C1 = 3424.0 / 4096.0
_PQ_C2 = 2413.0 / 4096.0 * 32.0
_PQ_C3 = 2392.0 / 4096.0 * 32.0


def pq_eotf(code_value_0_1: float) -> float:
    """Inverse PQ transfer function (SMPTE ST 2084): a normalized 10-bit
    code value (0.0-1.0) -> linear light, normalized so 1.0 == 10,000
    nits (the format's defined absolute peak). This part of the pipeline
    is a fixed standard, not a tunable."""
    n = max(code_value_0_1, 0.0)
    n_pow = n ** (1.0 / _PQ_M2)
    num = max(n_pow - _PQ_C1, 0.0)
    den = _PQ_C2 - _PQ_C3 * n_pow
    if den <= 0:
        return 0.0
    return (num / den) ** (1.0 / _PQ_M1)


def pq_tone_map(nits: float) -> float:
    """Simple Reinhard-style soft rolloff: normalizes by reference white,
    then compresses anything approaching PQ_TONE_MAP_MAX_NITS toward 1.0
    instead of hard-clipping (hard-clipping would flatten every bright
    highlight to identical white, losing exactly the detail a "does this
    look right" comparison needs). This is a placeholder tone-map, not a
    colorimetrically exact one -- good enough for a first "is this even
    in the right ballpark" check against the real screen."""
    x = nits / PQ_REFERENCE_WHITE_NITS
    peak = PQ_TONE_MAP_MAX_NITS / PQ_REFERENCE_WHITE_NITS
    return x * (1.0 + x / (peak * peak)) / (1.0 + x)


# BT.2020 -> BT.709/sRGB primaries, linear-light 3x3 (ITU-R BT.2087).
_BT2020_TO_BT709 = (
    ( 1.6605, -0.5876, -0.0728),
    (-0.1246,  1.1329, -0.0083),
    (-0.0182, -0.1006,  1.1187),
)


def bt2020_to_bt709_linear(r, g, b):
    m = _BT2020_TO_BT709
    r2 = m[0][0] * r + m[0][1] * g + m[0][2] * b
    g2 = m[1][0] * r + m[1][1] * g + m[1][2] * b
    b2 = m[2][0] * r + m[2][1] * g + m[2][2] * b
    return r2, g2, b2


def srgb_oetf(linear: float) -> float:
    """Linear (0-1, clamped) -> sRGB gamma-encoded (0-1)."""
    c = min(max(linear, 0.0), 1.0)
    if c <= 0.0031308:
        return 12.92 * c
    return 1.055 * (c ** (1.0 / 2.4)) - 0.055


def unpack_a2r10g10b10_bt2020_pq(raw4: bytes):
    """A2R10G10B10_BT2020_PQ -- real PQ decode, not truncation. Same bit
    layout as unpack_a2r10g10b10 (MSB-first: alpha 2 / R10 / G10 / B10,
    blue at LSB)."""
    px = struct.unpack("<I", raw4)[0]
    a2 = (px >> 30) & 0x3
    r10 = (px >> 20) & 0x3FF
    g10 = (px >> 10) & 0x3FF
    b10 = (px >> 0) & 0x3FF

    r_nits = pq_eotf(r10 / 1023.0) * 10000.0
    g_nits = pq_eotf(g10 / 1023.0) * 10000.0
    b_nits = pq_eotf(b10 / 1023.0) * 10000.0

    r_tm = pq_tone_map(r_nits)
    g_tm = pq_tone_map(g_nits)
    b_tm = pq_tone_map(b_nits)

    r709, g709, b709 = bt2020_to_bt709_linear(r_tm, g_tm, b_tm)

    r8 = min(max(round(srgb_oetf(r709) * 255), 0), 255)
    g8 = min(max(round(srgb_oetf(g709) * 255), 0), 255)
    b8 = min(max(round(srgb_oetf(b709) * 255), 0), 255)
    return a2, (r8, g8, b8)


# Maps the raw format enum value (as read off the wire, masked to 32 bits)
# to (display name, unpack function). Formats with no unpack function are
# known/named but not yet supported for auto-decode (e.g. float formats
# need a different raw4 size / decode path entirely).
FORMAT_TABLE = {
    0x88000000: ("A2R10G10B10_SRGB", unpack_a2r10g10b10),
    0x88060000: ("A2R10G10B10", unpack_a2r10g10b10),
    0x88740000: ("A2R10G10B10_BT2020_PQ", unpack_a2r10g10b10_bt2020_pq),
    0x80000000: ("A8R8G8B8_SRGB", unpack_a8r8g8b8),
    0x80002200: ("A8B8G8R8_SRGB", unpack_a8b8g8r8),
    -0x7F000000 & 0xFFFFFFFF: ("A16R16G16B16_FLOAT", None),  # 0xC1060000 -- 8 bytes/pixel, needs probe changes, not yet supported
}

DEFAULT_FALLBACK_FORMAT = 0x88000000  # A2R10G10B10_SRGB -- old script's hardcoded assumption


# v3.2: runtime format auto-detection via decode smoothness, added after
# a real, confirmed dead end trying to detect HDR state directly. The
# obvious approach -- hook sceVideoOutAddBufferHdrPrivilege, the PS4
# API a game presumably calls when HDR engages -- turned out to have
# no real reference implementation anywhere: not in this repo's own
# probe/plugin history, not in fpPS4 (checked its full, real
# ps4_libscevideoout.pas -- the function isn't implemented there at
# all, only present as a bare name-to-NID lookup entry with zero type
# info). Guessing a signature for a hook GoldHEN's HOOK_CONTINUE would
# need to forward calls through exactly right is a real crash risk
# with no way to verify it beforehand, so that path was dropped.
#
# This sidesteps detection entirely: since both real formats seen on
# this title are now independently confirmed correct decoders (SDR:
# A8B8G8R8_SRGB with R/B swapped, confirmed against a real screenshot
# and independently against fpPS4's own enum comment; HDR:
# A2R10G10B10_BT2020_PQ, confirmed against a real screenshot at all 5
# points), the two candidate decodes can just be tried against each
# other and scored by which one looks more like real image content.
#
# The scoring signal: real photographic/rendered content is spatially
# coherent -- neighboring pixels are usually close in value, even
# across hard edges the change is bounded. Decoding the WRONG bit
# layout effectively randomizes the bit pattern, which tends to
# produce much larger, noisier swings between adjacent samples. Each
# capture already includes 4 consecutive 32bpp words per test point
# (RAW_DUMP_BYTES=16, from v2.9) -- exactly the adjacent-sample data
# this needs, with no probe/firmware changes required at all.
def _total_variation(rgb_list):
    """Sum of per-channel absolute differences between consecutive
    RGB triples -- lower means smoother/more coherent."""
    total = 0
    for a, b in zip(rgb_list, rgb_list[1:]):
        total += sum(abs(x - y) for x, y in zip(a, b))
    return total


def detect_format_via_smoothness(pixel_results):
    """Given decoded pixel-packet rows (each with a >4-byte raw_full),
    scores SDR (A8B8G8R8_SRGB swapped) vs HDR (A2R10G10B10_BT2020_PQ)
    by total variation across each point's 4 consecutive words, and
    returns (winner_name, per_point_detail, sdr_total, hdr_total).
    Only meaningful for points with the wider v2.9+ raw dump (16+
    bytes) -- points with just the original 4-byte raw are skipped."""
    by_point = {}
    for r in pixel_results:
        raw_full = r.get("raw_full", r.get("raw4", b""))
        if len(raw_full) < 8:
            continue  # need at least 2 words to compare anything
        words = [raw_full[i:i+4] for i in range(0, len(raw_full) - 3, 4)]
        key = (r["point_idx"], r["paramset"])
        by_point[key] = words

    detail = []
    sdr_total = 0
    hdr_total = 0
    for key, words in by_point.items():
        sdr_rgbs = [unpack_a8b8g8r8(w)[1] for w in words]
        hdr_rgbs = [unpack_a2r10g10b10_bt2020_pq(w)[1] for w in words]
        sdr_tv = _total_variation(sdr_rgbs)
        hdr_tv = _total_variation(hdr_rgbs)
        sdr_total += sdr_tv
        hdr_total += hdr_tv
        detail.append((key, sdr_tv, hdr_tv, "SDR" if sdr_tv < hdr_tv else "HDR"))

    if not detail:
        return None, [], 0, 0
    winner = "SDR (A8B8G8R8_SRGB, swapped)" if sdr_total < hdr_total else "HDR (A2R10G10B10_BT2020_PQ)"
    return winner, detail, sdr_total, hdr_total


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
    once the active format for this dump is known.

    v2.9: raw payload length is no longer assumed to be exactly 4
    bytes. Everything between the x/y header (bytes 2-5) and the final
    displayBufferIndex byte is "raw" -- could be 4 bytes (the original
    single-pixel dump) or more (v2.9's 16-byte context dump, added to
    visually inspect surrounding memory instead of guessing another
    whole-format hypothesis blind). RGB decoding still only ever uses
    the first 4 bytes (raw4) -- a wider raw dump doesn't change what
    counts as "one pixel" in a 32bpp format, it just shows more of
    what comes after it.
    """
    point_idx = data[0]
    paramset = data[1]
    x = struct.unpack("<H", data[2:4])[0]
    y = struct.unpack("<H", data[4:6])[0]
    if len(data) == 10:
        # Legacy case this script's error message still allows for --
        # no real packet from this probe has ever actually been this
        # short (main.c always sends the trailing displayBufferIndex
        # byte), but keep the old fixed-4-byte behavior here rather
        # than let data[-1] silently eat a real raw byte.
        raw = data[6:10]
        display_buffer_index = None
    else:
        raw = data[6:-1]
        display_buffer_index = data[-1]
    raw4 = raw[:4]
    return {
        "kind": "pixel",
        "point_idx": point_idx,
        "paramset": "base" if paramset == 0 else "neo" if paramset == 1
                    else "linear" if paramset == 2 else f"?{paramset}",  # v2.6 probe: 2=naive linear addressing
        "x": x,
        "y": y,
        "raw4": raw4,
        "raw_hex": raw4.hex(),
        "raw_full": raw,
        "raw_full_hex": raw.hex(),
        "display_buffer_index": display_buffer_index,
    }


def decode_timing_packet(data: bytes):
    """v1.2 ps4_ambient_light telemetry packet (24 bytes, handoff §30
    step 2): per-window min/max/avg loop time (microseconds) for
    ambient_sample_thread's own buffer-resolve + sampling + UDP-send
    work, plus how many of that window's iterations ran past
    SAMPLE_INTERVAL_US (33000us, ~30Hz) and a window_id so gaps
    (plugin reload, crash) are visible. All fields uint32 LE."""
    min_us, max_us, avg_us, sample_count, over_budget_count, window_id = struct.unpack("<IIIIII", data)
    return {
        "kind": "timing",
        "min_us": min_us,
        "max_us": max_us,
        "avg_us": avg_us,
        "sample_count": sample_count,
        "over_budget_count": over_budget_count,
        "window_id": window_id,
    }


_CONFIG_RELOAD_EVENT_NAMES = {
    0: "disabled (configReloadCheckSeconds == 0)",
    1: "stat() FAILED",
    2: "baseline established",
    3: "checked, no change",
    4: "CHANGE DETECTED -- reload triggered",
}


def decode_config_reload_debug_packet(data: bytes):
    """v2.1.2 ps4_ambient_light diagnostic packet (44 bytes) -- one per
    ambient_check_config_reload() call, reporting which branch it took.
    mtime/size are full 64-bit (not truncated) since the bug being
    chased is specifically about mtime edge cases. See
    send_config_reload_debug_packet in main.c for the exact layout."""
    event, stat_errno = struct.unpack("<II", data[0:8])
    cur_mtime, last_mtime, cur_size, last_size = struct.unpack("<QQQQ", data[8:40])
    check_count = struct.unpack("<I", data[40:44])[0]
    return {
        "kind": "config_reload",
        "event": event,
        "event_name": _CONFIG_RELOAD_EVENT_NAMES.get(event, f"?unknown ({event})"),
        "stat_errno": stat_errno,
        "cur_mtime": cur_mtime,
        "last_mtime": last_mtime,
        "cur_size": cur_size,
        "last_size": last_size,
        "check_count": check_count,
    }


def decode_config_reload_debug_packet_v2(data: bytes):
    """v2.1.3 ps4_ambient_light diagnostic packet (60 bytes) -- same as
    the 44-byte v2.1.2 packet above, plus a 16-byte raw content preview
    of whatever AMBIENT_CONFIG_PATH resolves to from the plugin's own
    point of view (bytes [44:60]). Zero bytes here mean either a
    genuinely zero-filled file OR that the plugin's own open/read of
    that path failed -- check event/stat_errno to disambiguate. See
    ambient_read_content_preview/send_config_reload_debug_packet in
    main.c for the exact layout."""
    base = decode_config_reload_debug_packet(data[0:44])
    preview = data[44:60]
    base["content_preview_hex"] = preview.hex()
    # Non-printable bytes shown as '.', like a standard hex-dump ASCII gutter.
    base["content_preview_ascii"] = "".join(
        chr(b) if 32 <= b < 127 else "." for b in preview
    )
    return base


def decode_config_reload_debug_packet_v3(data: bytes):
    """v2.1.5 ps4_ambient_light diagnostic packet (68 bytes) -- same as
    the 60-byte v2.1.3/v2.1.4 packet above, plus an 8-byte FNV-1a/32
    content hash pair (bytes [60:68]: cur_hash, last_hash), added
    because size alone can't detect a same-length edit (confirmed on
    hardware: an RGB->RBG swap left cur_size/last_size both correct and
    stable but never differing). See
    ambient_get_real_config_size_and_hash/send_config_reload_debug_packet
    in main.c for the exact layout."""
    base = decode_config_reload_debug_packet_v2(data[0:60])
    cur_hash, last_hash = struct.unpack("<II", data[60:68])
    base["cur_hash"] = cur_hash
    base["last_hash"] = last_hash
    return base


def decode_packet(hexstr: str):
    data = bytes.fromhex(hexstr.strip())
    if len(data) == 68:
        return decode_config_reload_debug_packet_v3(data)
    if len(data) == 60:
        return decode_config_reload_debug_packet_v2(data)
    if len(data) == 44:
        return decode_config_reload_debug_packet(data)
    if len(data) == 32:
        return decode_registration_packet(data)
    if len(data) == 24:
        return decode_timing_packet(data)
    if len(data) < 10:
        raise ValueError(f"packet too short: {len(data)} bytes, need 10, 11, 24, 32, 44, 60, or 68")
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
    timing_events = sorted([r for r in all_results if r["kind"] == "timing"],
                            key=lambda r: r["window_id"])
    config_events = sorted([r for r in all_results if r["kind"] == "config_reload"],
                            key=lambda r: r["check_count"])
    results = [r for r in all_results if r["kind"] == "pixel"]

    if config_events:
        has_preview = any("content_preview_hex" in c for c in config_events)
        label = "v2.1.3" if has_preview else "v2.1.2"
        print(f"{len(config_events)} config-reload check(s) captured ({label} diagnostic):")
        header = (f"{'call#':>5} {'event':<38} {'cur_mtime':>12} {'last_mtime':>12} "
                  f"{'cur_size':>10} {'last_size':>10} {'errno':>6}")
        if has_preview:
            header += f"  {'content_preview (hex / ascii)':<40}"
        print(header)
        print("-" * (100 if not has_preview else 145))
        for c in config_events:
            line = (f"{c['check_count']:>5} {c['event_name']:<38} {c['cur_mtime']:>12} "
                    f"{c['last_mtime']:>12} {c['cur_size']:>10} {c['last_size']:>10} "
                    f"{c['stat_errno']:>6}")
            if "content_preview_hex" in c:
                line += f"  {c['content_preview_hex']} {c['content_preview_ascii']!r}"
            print(line)
        print()
        counts = sorted(c["check_count"] for c in config_events)
        if counts[0] > 1:
            print(f"NOTE: first captured call# is {counts[0]}, not 1 -- earlier checks ran")
            print("before this capture started, that's expected, not a problem.")
        if len(counts) >= 2 and counts != list(range(counts[0], counts[0] + len(counts))):
            print("WARNING: call# has gaps -- some checks weren't captured (packet loss or")
            print("capture started/stopped mid-run), not necessarily a plugin problem.")
        events_seen = {c["event"] for c in config_events}
        if events_seen == {0}:
            print("ALL captured checks show event 0 (disabled) -- configReloadCheckSeconds")
            print("is 0 in the currently-loaded config, so the reload check never runs at")
            print("all. Check the ini value and whether THIS binary actually re-read it.")
        elif 1 in events_seen:
            print("At least one stat() FAILURE was captured -- check the errno column above")
            print("against errno.h on the PS4 toolchain (e.g. 2=ENOENT, 13=EACCES) to see")
            print("whether AMBIENT_CONFIG_PATH (/data/ps4_ambient_light.ini) is even the file")
            print("being edited, or a permissions issue.")
        elif 4 not in events_seen and len(config_events) > 1:
            print("No 'CHANGE DETECTED' event in this capture -- every check saw the SAME")
            print("mtime/size as its own baseline. If you edited the ini between captures")
            print("and cur_mtime/cur_size never moved between rows, the OS-level mtime on")
            print("this filesystem may genuinely not be updating on save (worth testing by")
            print("changing the file's SIZE, e.g. adding a comment line, since size is the")
            print("independent fallback signal here).")
        else:
            print("At least one 'CHANGE DETECTED' event was captured -- live reload IS")
            print("firing at the check/detection level. If the light still doesn't visibly")
            print("update, the remaining bug is downstream of this function (e.g. in")
            print("ambient_load_config()/buildZoneGeometry(), not in change detection).")
        print()

    if timing_events:
        print(f"{len(timing_events)} ambient_sample_thread timing window(s) captured "
              f"(handoff \u00a730 step 2):")
        print(f"{'win#':>5} {'min_us':>8} {'avg_us':>8} {'max_us':>8} "
              f"{'n':>4} {'over_budget':>11}")
        print("-" * 50)
        for t in timing_events:
            print(f"{t['window_id']:>5} {t['min_us']:>8} {t['avg_us']:>8} {t['max_us']:>8} "
                  f"{t['sample_count']:>4} {t['over_budget_count']:>11}")
        worst_avg = max(timing_events, key=lambda t: t["avg_us"])
        worst_max = max(timing_events, key=lambda t: t["max_us"])
        total_over = sum(t["over_budget_count"] for t in timing_events)
        print()
        print(f"Worst avg_us across all windows: {worst_avg['avg_us']} (window {worst_avg['window_id']}).")
        print(f"Worst single max_us: {worst_max['max_us']} (window {worst_max['window_id']}).")
        if total_over:
            print(f"WARNING: {total_over} iteration(s) across these windows exceeded the "
                  f"33000us (~30Hz) budget -- the sampling thread is falling behind its own "
                  f"target rate, not just render-thread-side impact (which this does not measure).")
        else:
            print("No iterations exceeded the ~30Hz budget in this capture.")
        print()

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

    # v3.2: format 0x80002200 is now a known special case for this title --
    # confirmed to mean two DIFFERENT real byte layouts depending on HDR
    # state (SDR: A8B8G8R8_SRGB swapped; HDR: A2R10G10B10_BT2020_PQ), with
    # no reliable way to tell which from the registration event alone (see
    # this script's own v3.2 comment above detect_format_via_smoothness for
    # why a direct HDR-state hook was ruled out). When the registration-
    # based pick lands on this format, run the smoothness heuristic and let
    # it override the choice for the table below, rather than trusting the
    # ambiguous format ID at face value.
    if active_fmt == 0x80002200:
        winner, detail, sdr_total, hdr_total = detect_format_via_smoothness(results)
        if winner is not None:
            print(f"Format 0x80002200 is ambiguous on this title (SDR or HDR-PQ, same ID) --")
            print(f"running the smoothness heuristic across {len(detail)} point(s) to pick:")
            for (pt_idx, paramset), sdr_tv, hdr_tv, pick in detail:
                print(f"  pt{pt_idx:<3} {paramset:>7}: SDR total-variation={sdr_tv:<6} "
                      f"HDR total-variation={hdr_tv:<6} -> {pick}")
            print(f"  TOTALS: SDR={sdr_total}  HDR={hdr_total}  -> picking {winner}")
            print(f"(Lower total variation wins -- smoother/more spatially coherent decode")
            print(f" is taken as more likely to be the real bit layout. Not a certainty --")
            print(f" sanity-check against a real screenshot when one's available.)")
            print()
            if winner.startswith("HDR"):
                active_fmt = 0x88740000  # reuse the existing PQ unpack + naive-compare table
                fmt_name, unpack_fn = FORMAT_TABLE[active_fmt]
            # else: SDR already the current active_fmt/unpack_fn, nothing to change.

    # When the active format is the HDR PQ format, also show the OLD naive
    # truncation decode side by side -- this is what "colors weren't
    # accurate" was actually looking at before this fix, and printing both
    # lets you visually confirm the PQ decode is the one that now matches
    # the real screen, the same empirical comparison method that settled
    # base vs Neo in §21/§22.
    show_naive_compare = (active_fmt == 0x88740000)

    print(f"Decoding pixel packets as: {fmt_name}")
    if show_naive_compare:
        print("(HDR format -- also showing the old naive-truncation decode for comparison;")
        print(" compare BOTH against the real screen, not just the new PQ column.)")
    print()
    if show_naive_compare:
        print(f"{'pt':>2} {'x':>5} {'y':>5} {'set':>5} {'raw (first 4 = pixel)':<34} {'RGB888 (PQ)':>16} {'RGB888 (naive)':>16} {'bufIdx':>6}")
        print("-" * 106)
    else:
        print(f"{'pt':>2} {'x':>5} {'y':>5} {'set':>5} {'raw (first 4 = pixel)':<34} {'RGB888':>16} {'bufIdx':>6}")
        print("-" * 82)
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
        raw_display = r.get("raw_full_hex", r["raw_hex"])
        if show_naive_compare:
            _, naive_rgb = unpack_a2r10g10b10(r["raw4"])
            print(f"{r['point_idx']:>2} {r['x']:>5} {r['y']:>5} {r['paramset']:>5} "
                  f"{raw_display:<34} {rgb_str:>16} {str(naive_rgb):>16} {buf_str:>6}")
        else:
            print(f"{r['point_idx']:>2} {r['x']:>5} {r['y']:>5} {r['paramset']:>5} "
                  f"{raw_display:<34} {rgb_str:>16} {buf_str:>6}")
        # v2.9: if more than 4 raw bytes came through, also show what
        # each following 4-byte word would decode to as its own pixel
        # -- if this really is contiguous 32bpp pixel data, these
        # should look like a plausible, smoothly-varying continuation
        # of whatever's actually next to that point on screen. If they
        # look like noise or don't relate to nearby real colors at
        # all, that's evidence against "this memory is 32bpp pixels"
        # entirely, not just against the current offset formula.
        if unpack_fn is not None and len(r.get("raw_full", b"")) > 4:
            extra = r["raw_full"][4:]
            words = [extra[i:i+4] for i in range(0, len(extra) - 3, 4)]
            for wi, w in enumerate(words, start=1):
                _, wrgb = unpack_fn(w)
                label = f"  +{wi*4}B: {w.hex()}"
                print(f"{'':>2} {'':>5} {'':>5} {'':>5} {label:<34} {str(wrgb):>16}")
        last_point = r["point_idx"]

    print()
    print("Compare each RGB888 against what's actually at that (x,y) on your")
    print("test screen. Whichever paramset (base/neo) is right for ALL points")
    print("is the confirmed-correct one.")
    if show_naive_compare:
        print()
        print("If neither PQ nor naive matches well, PQ_REFERENCE_WHITE_NITS /")
        print("PQ_TONE_MAP_MAX_NITS at the top of this script are the two knobs to")
        print("adjust -- the PQ EOTF and BT.2020->BT.709 matrix are fixed standards")
        print("and shouldn't need touching.")

    # Sanity check for the v2.1 probe fix: every REAL test point in one
    # dump should share the same displayBufferIndex, since one capture
    # = one live frame = one buffer. v2.7's dump_buffer_scan() rows
    # (point_idx 254) are deliberately excluded -- they're tagged with
    # their own slot index on purpose, always differing from each
    # other and often from the live points, and that's not a bug.
    real_points = [r for r in results if r["point_idx"] not in (253, 254)]
    buf_indices = {r["display_buffer_index"] for r in real_points}
    buf_indices.discard(None)
    if None in (r["display_buffer_index"] for r in real_points):
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
