#!/bin/sh -eu
set -eu
# shell-selftest.sh - every script must abort on failure even when a caller
# invokes it as `sh scripts/<name>` (which ignores the shebang).
#
# The project Justfile invokes scripts that way, so each script must carry an
# explicit `set -eu` in its body; the shebang alone is not enough.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
fail=0

for f in "$root"/scripts/*; do
    [ -e "$f" ] || continue
    if ! grep -q '^set -eu$' "$f"; then
        echo "shell-selftest: $f lacks explicit set -eu" >&2
        fail=1
    fi
done

if [ "$fail" -ne 0 ]; then
    exit 1
fi

tmp="$(mktemp -d)"

cleanup() {
    rm -rf "$tmp"
}
trap cleanup 0 1 2 15

printf '#!/bin/sh -eu\nset -eu\nfalse\necho reached\n' > "$tmp/probe.sh"
if sh "$tmp/probe.sh" >/dev/null 2>&1; then
    echo "shell-selftest: sh invocation did not abort on failure" >&2
    exit 1
fi

echo "shell-selftest: ok"
