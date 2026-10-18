# Payload checksums

`ps4-ambilight.bin` is the standalone loader payload shipped by this repository. It starts
`ps4_shell_feed.prx` inside `GnmCompositor`. The loader implementation and build source live in a
separate repository by design; this repository intentionally contains only the built binary and its
checksum, not the loader source.

The current binary was confirmed working end-to-end with the shell feed on PS4 firmware 9.00 and
12.52. Other firmware versions are untested or not in its firmware list. The old offset-based loader's table and its
old refusal behavior do not describe this binary.


```
sha256sum -c <<'SUMS'
bda812ef59a106bdb35a7f8a68995770df570591487242955cb2ec640bab174b  ps4-ambilight.bin
SUMS
```

| File | Size (bytes) | sha256 | Note |
|---|---|---|---|
| `ps4-ambilight.bin` | 69,216 | `bda812ef59a106bdb35a7f8a68995770df570591487242955cb2ec640bab174b` | Standalone loader payload. Starts `ps4_shell_feed.prx` in `GnmCompositor`. End-to-end confirmed on PS4 firmware 9.00 and 12.52; other firmware versions are untested or not in its firmware list. Loader source/build remain in a separate repository and are intentionally not included here. See `shell-feed/docs/support-matrix.md`. |

After replacing the binary, run `sh tools/update_payload_checksum.sh` from the repo root: it
rewrites the hash and size above from the file (the note is yours to edit), and CI fails if they
disagree. `sh tools/update_payload_checksum.sh --check` only reports.

To rebuild this binary, use the separate loader repository and its build instructions. This main
repository intentionally does not contain the loader implementation source.
