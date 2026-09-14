#!/bin/sh -eu
set -eu
# build-selftest.sh - the archive must contain only objects built from current
# sources; a deleted source must not leave a stale symbol behind.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
probe="$root/src/zz_build_selftest.c"

cleanup() {
    rm -f "$probe" "$root/build/zz_build_selftest.o"
}
trap cleanup 0 1 2 15

printf 'int syntax89_zz_build_selftest(void);\n\nint syntax89_zz_build_selftest(void)\n{\n    return 0;\n}\n' \
    > "$probe"

(cd "$root" && just build >/dev/null)
rm -f "$probe"
(cd "$root" && just build >/dev/null)

if nm -g --defined-only "$root/build/libsyntax89.a" |
    grep -q syntax89_zz_build_selftest; then
    echo "build-selftest: stale object archived after source removal" >&2
    exit 1
fi

echo "build-selftest: ok"
