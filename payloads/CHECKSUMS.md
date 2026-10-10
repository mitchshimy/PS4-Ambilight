# Payload checksums

`ps4-ambilight.bin` is a from-scratch kernel-exploit loader (not Orbis
Toolbox lineage, no third-party licensing question) that injects one SPRX
into one process (`GnmCompositor` by default). It's what loads either
`plugin/`'s or `shell-feed/`'s built `.prx` onto a 9.00 console.

Its source (shellcode + kernel + userland) lives in the `loader/` directory
of the research repo (`ps4-shell-capture-research`), not here, and that's
deliberate: this repo ships the usable binary and its checksum so anyone can
verify and use it, while the loader's own source, build setup and
desk-investigation history stay in the lab-notebook repo where the rest of
that project's evidence lives.

```
sha256sum -c <<'SUMS'
18ccf799ba4d272ca00b5f147313a465802eec2d6e9b94ade97249a800260ad6  ps4-ambilight.bin
SUMS
```

| File | Size (bytes) | sha256 | Note |
|---|---|---|---|
| `ps4-ambilight.bin` | 22,816 | `18ccf799ba4d272ca00b5f147313a465802eec2d6e9b94ade97249a800260ad6` | Targets `GnmCompositor` by prefix match, tries `ps4_shell_feed.prx` then falls back to `shell_probe.sprx`, no re-injection, mounts never torn down. 9.00 only -- see `shell-feed/docs/support-matrix.md`. Previous build (14,072 bytes, `a1e996d...`) superseded. |

After replacing the binary, run `sh tools/update_payload_checksum.sh` from the repo root: it
rewrites the hash and size above from the file (the note is yours to edit), and CI fails if they
disagree. `sh tools/update_payload_checksum.sh --check` only reports.

If you need to rebuild this binary yourself (e.g. to point it at a
different SPRX path), the buildable source is in the research repo's
`loader/` directory, not this one.
