#!/bin/sh
# Keeps payloads/CHECKSUMS.md in step with payloads/ps4-ambilight-900.bin, the Ambilight payload.
#
#   sh tools/update_payload_checksum.sh           rewrite the hash and size in CHECKSUMS.md from the file
#   sh tools/update_payload_checksum.sh --check   change nothing; exit 1 if CHECKSUMS.md is stale
#
# It rewrites three things and nothing else: the hash in the `sha256sum -c` block, and the size
# and hash in the table row. The row's last column (the note about which build this is) is yours
# to edit by hand. Needs only sh, sed, sha256sum (or shasum) and wc, so it runs on a PC, Git Bash
# and the CI runner. CI runs --check on every push and again before a release is published.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
dir="$root/payloads"
name=ps4-ambilight-900.bin
bin="$dir/$name"
doc="$dir/CHECKSUMS.md"

mode=update
case "${1:-}" in
    "") ;;
    --check) mode=check ;;
    *) echo "usage: sh tools/update_payload_checksum.sh [--check]" >&2; exit 2 ;;
esac

[ -f "$bin" ] || { echo "error: $bin not found" >&2; exit 2; }
[ -f "$doc" ] || { echo "error: $doc not found" >&2; exit 2; }

if command -v sha256sum >/dev/null 2>&1; then hash=$(sha256sum "$bin" | cut -d' ' -f1)
else hash=$(shasum -a 256 "$bin" | cut -d' ' -f1); fi
size=$(wc -c < "$bin" | tr -d ' ')
# 14264 -> 14,264, the way the table writes sizes
pretty=$(printf '%s' "$size" | sed -e ':a' -e 's/\([0-9]\)\([0-9]\{3\}\)\($\|,\)/\1,\2\3/' -e 'ta')

block_re="^[0-9a-f]\{64\}  $name\$"
row_re="^| \`$name\` | [0-9,]* | \`[0-9a-f]\{64\}\` |"
grep -q "$block_re" "$doc" || { echo "error: no 'sha256  $name' line in $doc" >&2; exit 2; }
grep -q "$row_re" "$doc"   || { echo "error: no table row for $name in $doc" >&2; exit 2; }

doc_block=$(sed -n "s/$block_re/&/p" "$doc" | cut -d' ' -f1)
doc_hash=$(sed -n "s/^| \`$name\` | [0-9,]* | \`\([0-9a-f]\{64\}\)\` |.*/\1/p" "$doc")
doc_size=$(sed -n "s/^| \`$name\` | \([0-9,]*\) | .*/\1/p" "$doc")

if [ "$doc_block" = "$hash" ] && [ "$doc_hash" = "$hash" ] && [ "$doc_size" = "$pretty" ]; then
    echo "payloads/CHECKSUMS.md is up to date: $name, $pretty bytes, $hash"
    exit 0
fi

if [ "$mode" = check ]; then
    echo "error: payloads/CHECKSUMS.md does not match payloads/$name" >&2
    echo "  file:        $pretty bytes, $hash" >&2
    echo "  check block: $doc_block" >&2
    echo "  table row:   $doc_size bytes, $doc_hash" >&2
    echo "  Run: sh tools/update_payload_checksum.sh   (then commit CHECKSUMS.md with the binary)" >&2
    exit 1
fi

tmp="$doc.tmp.$$"
sed -e "s/$block_re/$hash  $name/" \
    -e "s/^\(| \`$name\` | \)[0-9,]* | \`[0-9a-f]\{64\}\` |/\1$pretty | \`$hash\` |/" \
    "$doc" > "$tmp"
cat "$tmp" > "$doc"     # keeps the file's mode; no mv over it
rm -f "$tmp"
echo "updated payloads/CHECKSUMS.md: $name, $pretty bytes (was $doc_size), $hash"
echo "Check the note in the table's last column still describes this build."
