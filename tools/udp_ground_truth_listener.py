import socket
from datetime import datetime

PORT = 4048

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", PORT))

print(f"Listening for UDP packets on 0.0.0.0:{PORT} ... (Ctrl+C to stop)")

while True:
    data, addr = sock.recvfrom(65535)
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    hexstr = data.hex()

    # DDP header is the first 10 bytes; break it out for readability
    header = hexstr[:20]
    payload = hexstr[20:]

    # If the payload is a repeated solid color (what wled_signal sends), the first 3 bytes (6 hex chars)...
    # More: docs/code-comments.md (tools/udp_ground_truth_listener.py:20)
    if len(payload) >= 6:
        r = int(payload[0:2], 16)
        g = int(payload[2:4], 16)
        b = int(payload[4:6], 16)
        color_note = f"  -> first LED = RGB({r},{g},{b})"
    else:
        color_note = ""

    print(f"[{ts}] from {addr[0]}:{addr[1]}  len={len(data)}  header={header}{color_note}")
    # Existing dev telemetry wraps text in a 10-byte DDP-like header.
    # SHL1 is the shell-feed mirror; HOF1 is the plugin's handoff-event mirror.
    body = data[10:] if len(data) >= 10 else b""
    if body.startswith((b"SHL1", b"HOF1")):
        print("    telemetry=" + body.decode("ascii", errors="replace"))
    else:
        # Full payload hex for existing binary diagnostics / color packets.
        print(f"    payload_hex={payload}")
