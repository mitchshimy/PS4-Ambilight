# Payload checksums

`ps4-ambilight-900.bin` is a from-scratch kernel-exploit loader (not Orbis
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
a1e996d040040c33da2d7cda8e05e617ef0a79fb7dff1c33dad6520485e3a86f  ps4-ambilight-900.bin
SUMS
```

| File | Size (bytes) | sha256 | Note |
|---|---|---|---|
| `ps4-ambilight-900.bin` | 14,072 | `a1e996d040040c33da2d7cda8e05e617ef0a79fb7dff1c33dad6520485e3a86f` | Targets `GnmCompositor` by prefix match, loads one SPRX once per boot, no re-injection, mounts never torn down. 9.00 only -- see `shell-feed/docs/support-matrix.md`. |

If you need to rebuild this binary yourself (e.g. to point it at a
different SPRX path), the buildable source is in the research repo's
`loader/` directory, not this one.
