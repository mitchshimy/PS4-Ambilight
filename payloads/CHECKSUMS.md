# Payload checksums

`ps4-ambilight.bin` is a from-scratch kernel-exploit loader (not Orbis
Toolbox lineage, no third-party licensing question) that injects one SPRX
into one process (`GnmCompositor` by default). It's what loads either
`plugin/`'s or `shell-feed/`'s built `.prx`. It carries offsets for several firmwares and picks
the right one at runtime (see `shell-feed/docs/support-matrix.md` for which); on a firmware it
doesn't recognise it writes `PLUGIN_DIR/unsupported_fw` and exits without touching anything.

Its source (shellcode + kernel + userland) lives in the `loader/` directory
of the research repo (`ps4-shell-capture-research`), not here, and that's
deliberate: this repo ships the usable binary and its checksum so anyone can
verify and use it, while the loader's own source, build setup and
desk-investigation history stay in the lab-notebook repo where the rest of
that project's evidence lives.

```
sha256sum -c <<'SUMS'
91c40e8034f20c2d20144a8a8aa9686c1c0a4a2c4de1a53b08d3cef90bdabd9a  ps4-ambilight.bin
SUMS
```

| File | Size (bytes) | sha256 | Note |
|---|---|---|---|
| `ps4-ambilight.bin` | 24,000 | `91c40e8034f20c2d20144a8a8aa9686c1c0a4a2c4de1a53b08d3cef90bdabd9a` | Targets `GnmCompositor` by prefix match, tries `ps4_shell_feed.prx` then falls back to `shell_probe.sprx` if that module is already loaded (a re-send while already running refuses rather than re-injecting -- confirmed on a console, no crash), mounts never torn down. Offsets for 5.05 through 12.02; only 9.00 is confirmed on real hardware so far, the rest have offsets but are untested -- see `shell-feed/docs/support-matrix.md`. Firmwares 12.50 and up are not yet supported (missing kernel offsets). Replaces the single-firmware 9.00-only build (22,816 bytes, `18ccf799...`), which replaced the original (14,264 bytes, `5c922cd7...`). |

After replacing the binary, run `sh tools/update_payload_checksum.sh` from the repo root: it
rewrites the hash and size above from the file (the note is yours to edit), and CI fails if they
disagree. `sh tools/update_payload_checksum.sh --check` only reports.

If you need to rebuild this binary yourself (e.g. to point it at a
different SPRX path), the buildable source is in the research repo's
`loader/` directory, not this one.
