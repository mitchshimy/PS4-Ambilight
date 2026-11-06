# Capture file: the debug packets, saved on the console (v4.0.1)

`tools/flicker_capture.py` and `tools/udp_ground_truth_listener.py` both need a PC on the same network, a
`dev_ip` in the ini and a free UDP port 4048. A **debug build** (`make DEBUG=1`) can instead keep the same packets in a
file on the console, so a capture needs no PC while it runs.

## Turn it on

In `/data/ps4-ambilight/ps4_ambient_light.ini` (the `[dev]` section is not in the shipped template):

```
[dev]
capture_file=1
```

No `dev_ip` or `dev_logging` is needed. Start the game, use the screen you want to capture (for flicker: a static
menu), play as long as you need, and leave the game. The plugin writes
`/data/ps4-ambilight/ps4_ambient_capture_<TITLEID>.bin` once a second, so the file is usable even if the game crashes.
Copy it to a PC.

A release build ignores `capture_file`, and the code is compiled out of it.

## Read it

```
python tools/console_capture.py info   ps4_ambient_capture_CUSA06014.bin
python tools/console_capture.py to-flk ps4_ambient_capture_CUSA06014.bin game.flk
python tools/flicker_capture.py analyze game.flk
python tools/console_capture.py dump   ps4_ambient_capture_CUSA06014.bin > capture.log
```

`dump` prints the text `udp_ground_truth_listener.py` prints, so `decode_guard_packets.py` and the other decoders read
it as is. `info` shows the packet kinds, the length of the capture, and any gaps in the FLK1 sequence numbers.

## Limits

- Size: the file grows until it reaches 8 MB, about 30 minutes at the FLK1 probe rate. Then recording stops (the start
  of the capture is kept) and one `CAPX` closing record is written. `info` shows it. The 256 KB memory buffer only
  holds what waits to be written, about a second's worth.
- A record is dropped, not waited for, if two threads record at the same instant, or if the disk stalls long enough to
  fill the buffer. `info` reports the gaps this leaves.
- It captures what `debug_send_raw()` is given. That is everything the listener and `flicker_capture.py` receive. It
  does not capture the colour packets sent to WLED.
- The debug build still reads extra pixels for the FLK1 probe on every sampler pass, with or without capture. A
  capture shows what the debug build does, which can differ slightly from a release build's timing.
- The file is not cleared between launches: starting the same title again starts the file over.

## Tests

```
gcc -std=gnu11 -Wall -Wextra -D__FINAL__=0 -I tools/host_stubs/capture_file -I plugin/include \
    tools/test_capture_file.c plugin/source/capture_file.c -o /tmp/test_capture_file
/tmp/test_capture_file
/tmp/test_capture_file /tmp/cap.bin 600 && python tools/console_capture.py selftest /tmp/cap.bin 600
python tools/console_capture.py selftest
```

The first runs the real `capture_file.c` against a fake kernel. The last two check the PC tool and that
`flicker_capture.py` parses what the C code writes. None of this was run on a console.
