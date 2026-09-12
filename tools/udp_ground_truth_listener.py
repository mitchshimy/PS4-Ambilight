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

    # If the payload is a repeated solid color (what wled_signal sends),
    # the first 3 bytes (6 hex chars) of the payload are the R,G,B value
    # repeated for every LED — show that explicitly.
    if len(payload) >= 6:
        r = int(payload[0:2], 16)
        g = int(payload[2:4], 16)
        b = int(payload[4:6], 16)
        color_note = f"  -> first LED = RGB({r},{g},{b})"
    else:
        color_note = ""

    print(f"[{ts}] from {addr[0]}:{addr[1]}  len={len(data)}  header={header}{color_note}")
    # Full payload hex — needed for raw byte dumps (e.g. disassembling a
    # function's prologue), not just solid-color checkpoints. Short enough
    # to just print inline; paste this back for anything that isn't a
    # plain color signal.
    print(f"    payload_hex={payload}")
