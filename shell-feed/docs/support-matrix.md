# shell-feed: support matrix

This matrix describes the **current standalone `payloads/ps4-ambilight.bin`** and the shell-feed PRX
that it starts. It does not describe the previous bundled loader's firmware-offset table.

| Component / firmware | Status | Evidence |
|---|---|---|
| Standalone payload + shell feed, PS4 9.00 | ✅ Confirmed | User-confirmed end-to-end hardware test on 2026-10-12. |
| Standalone payload + shell feed, PS4 12.52 | ✅ Confirmed | User-confirmed end-to-end test; user-provided PuTTY log records `LOAD_START_OK`, `module_start done, reporter thread running`, 229 zones, 30 Hz, 901 sends in 30 seconds and 0 skipped sends. |
| Other firmware in the loader's list: 5.05, 6.72, 7.02, 7.55, 8.03, 8.52, 9.04, 9.60, 10.02, 10.71, 11.00, 11.02, 11.52, 12.02, 13.00, 13.04, 13.50 and 13.52 | 🔬 Untested | The loader has a per-firmware entry (an `init_<n>sdk` section in the binary), but nobody has run it with this payload and shell feed. |
| Any firmware not in that list (for example 14.00) | ⛔ Not listed | No entry in the loader; the binary contains "unsupported firmware %u - kernel not patched", so it will most likely refuse. |
| GoldHEN | Tested v2.4b18.10 for AutoRun trials | See `docs/loading.md`; other GoldHEN versions may differ. |
| PS-button suspend/resume | Prior shell-feed captures and user reports | See `docs/loading.md`; the current standalone loader's long-term/cold-boot stability remains a separate test. |
| Full rest-mode entry and long rest | Not fully verified | A report of working across several cycles exists, but this repo does not contain a complete capture covering entry and a long rest. |

## How to interpret firmware status

- **Confirmed** means the currently shipped standalone payload and shell-feed PRX were reported working on a real console.
- **Untested** means the loader has an entry for that firmware but there is no end-to-end result yet. It may work.
- **Not listed** means the loader has no entry for it and will most likely report "unsupported firmware" and not start.
- The list comes from the section names in the shipped binary (`.text.init_<n>sdk`, n = major*100 + minor), not from a disassembly or from the loader's source, which is in a separate repository. Re-read it whenever `payloads/ps4-ambilight.bin` is replaced, and update `common/fw_support.h` (and its copy in the companion app) to match.
- The older payload's `PATCH`/kernel-offset table, including its old partial/unsupported rows for 12.50+, does not describe this replacement binary and must not be used to predict its behavior.

The standalone loader's implementation and build source live in its separate repository by design. This repository ships only the built `payloads/ps4-ambilight.bin` artifact and its checksum. The companion app's firmware notice is informational only; the loader itself decides at runtime whether it can run on the console.

If you test another firmware version, please report the exact PS4 firmware, GoldHEN version, whether the payload loaded, whether the shell-feed PRX started, and whether WLED colors streamed. Only move a version to **Confirmed** with end-to-end evidence.
