# shell-feed: support matrix

| | `plugin/` (sister) | `shell-feed/` |
|---|---|---|
| PS4 firmware | 9.00 and 12.50 | offsets for 5.05-12.02 (see table below); **only 9.00 confirmed on a console** |
| GoldHEN version | see main README | tested against GoldHEN v2.4b18.10 (AutoRun trials) |
| Rest mode | n/a (game-process plugin) | PS-button suspend/resume confirmed working (capture + person report); full rest-mode entry/long-rest not captured in logs, person-reported working across several cycles but treat as **not yet verified** |

shell-feed's half of the picture (the `.prx` itself, built and captured only on 9.00) is the row
above. The other half is `ps4-ambilight.bin`, the payload that loads that `.prx` onto the console
(see `payloads/CHECKSUMS.md` in the repo root): it now carries offsets for several firmwares and
picks the right one at runtime, refusing cleanly (`PLUGIN_DIR/unsupported_fw`, no changes made) on
one it doesn't recognise. The table below is that payload's own matrix, from the firmware-offset
work in the research repo (`ps4-shell-capture-research`), copied here so the two repos don't drift.

**Status legend**

| Symbol | Meaning |
|--------|---------|
| ✅ | Confirmed working on a real console |
| 🔬 | Offsets complete, not yet tested on hardware |
| ⚠️ | Partial -- some PATCH values still TODO (will be refused at runtime) |
| ❌ | Not planned / no offset data |

## Fully supported (all 17 PATCH macros filled)

| Firmware | Offsets complete | Console-tested | Notes |
|----------|-----------------|----------------|-------|
| 5.05  | ✅ | 🔬 | |
| 6.71  | ✅ | 🔬 | |
| 6.72  | ✅ | 🔬 | |
| 7.50  | ✅ | 🔬 | |
| 7.51  | ✅ | 🔬 | |
| 7.55  | ✅ | 🔬 | |
| 8.00  | ✅ | 🔬 | |
| 8.01  | ✅ | 🔬 | |
| 8.03  | ✅ | 🔬 | |
| 8.50  | ✅ | 🔬 | |
| 8.52  | ✅ | 🔬 | |
| 9.00  | ✅ | ✅ | Original development firmware. The payload's fallback path (retrying as `shell_probe.sprx` when the primary module is already loaded) was deliberately re-sent on a live console to check it refuses cleanly instead of re-injecting or crashing -- it did. |
| 9.03  | ✅ | 🔬 | |
| 9.04  | ✅ | 🔬 | |
| 9.50  | ✅ | 🔬 | |
| 9.51  | ✅ | 🔬 | |
| 9.60  | ✅ | 🔬 | |
| 10.00 | ✅ | 🔬 | |
| 10.01 | ✅ | 🔬 | |
| 10.50 | ✅ | 🔬 | |
| 10.70 | ✅ | 🔬 | addr_* TODOs resolved at runtime by scan |
| 10.71 | ✅ | 🔬 | addr_* TODOs resolved at runtime by scan |
| 11.00 | ✅ | 🔬 | KMEM_PROT filled from GoldHEN; addr_* via scan |
| 11.02 | ✅ | 🔬 | KMEM_PROT filled from GoldHEN; addr_* via scan |
| 11.50 | ✅ | 🔬 | KMEM_PROT filled from GoldHEN; addr_* via scan |
| 11.52 | ✅ | 🔬 | KMEM_PROT filled from GoldHEN; addr_* via scan |
| 12.00 | ✅ | 🔬 | KMEM_PROT filled from GoldHEN; addr_* via scan |
| 12.02 | ✅ | 🔬 | KMEM_PROT filled from GoldHEN; addr_* via scan |

## Partial -- refused at runtime until TODOs are filled

| Firmware | Missing PATCH values | Needed source |
|----------|--------------------|---------------|
| 12.50 | ACMGR_BYPASS1/2, COPYIN_NOP1/2 | Kernel dump or HEN binary for 12.50 |
| 12.52 | ACMGR_BYPASS1/2, COPYIN_NOP1/2 | Kernel dump or HEN binary for 12.52 |
| 13.00 | ACMGR_BYPASS1/2, COPYIN_NOP1/2 | Kernel dump or HEN binary for 13.00 |
| 13.02 | ACMGR_BYPASS1/2, COPYIN_NOP1/2 | Kernel dump or HEN binary for 13.02 |
| 13.04 | ACMGR_BYPASS1/2, COPYIN_NOP1/2 | Kernel dump or HEN binary for 13.04 |
| 13.50 | ACMGR_BYPASS1/2, COPYIN_NOP1/2 | Kernel dump or HEN binary for 13.50 |
| 13.52 | ACMGR_BYPASS1/2, COPYIN_NOP1/2 | Kernel dump or HEN binary for 13.52 |
| 14.00 | ACMGR_BYPASS1/2, COPYIN_NOP1/2, KMEM_PROT1/2 | Kernel dump or GoldHEN 14.00 |

Adding a firmware, or moving one from 🔬 to ✅, happens in the research repo
(`ps4-shell-capture-research`), not here -- its own docs cover that. What changes here is which
built `ps4-ambilight.bin` is checked into `payloads/` and this table, kept in step with it.

If you try shell-feed on a firmware marked 🔬 here, or on a different GoldHEN version or strip
configuration than tested above, open an issue with what you find -- same ask the main README
already makes for the sister plugin. 12.50+ (the Partial table) will refuse to run at all until
their offsets are filled; that is expected, not a bug report.
