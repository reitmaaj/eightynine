#!/bin/sh -eu
# Regenerate the pinned UCD provenance manifest for libu89.
#
# Writes unicode-testdata/17.0.0/SOURCES.txt: the Unicode version, every
# vendored data file with its sha256, the generator source, and the command
# that regenerates the tables.

script_dir=$(dirname -- "$0")
root=$(cd -- "$script_dir/.." && pwd)
data="$root/unicode-testdata/17.0.0"
out="$data/SOURCES.txt"

{
    echo "# libu89 vendored Unicode data provenance"
    echo "#"
    echo "# Unicode version: 17.0.0"
    echo "# Generator: tools/gen_unicode_tables.py"
    echo "# Generate tables: just tables"
    echo "# Regenerate manifest: just manifest"
    echo "#"
    echo "# sha256  file (relative to unicode-testdata/17.0.0/)"
    cd "$data" || exit 1
    find . -type f ! -name SOURCES.txt | LC_ALL=C sort | while read -r f; do
        sha256sum "$f" | sed 's|\./||'
    done
} > "$out"
