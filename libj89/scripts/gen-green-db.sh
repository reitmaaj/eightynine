#!/bin/sh -eu
# Generate green compile databases (gcc + clang) into build/.
#
# The green toolchain requires a compile_commands.json per compiler. Each
# entry preserves the source-semantic include flags; green replaces the
# -std/-W/codegen profile with its own and strips -c/-o. The compiler token
# in each command is a placeholder: green resolves its own gcc/clang. The
# source list is taken from the Justfile SRC variable, passed on argv.

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)

write_db() {
    compiler=$1
    shift
    out="$ROOT/build/$1/compile_commands.json"
    shift
    printf '[\n' > "$out"
    first=1
    for f in "$@"; do
        abs="$ROOT/$f"
        if [ "$first" -eq 1 ]; then
            first=0
        else
            printf ',\n' >> "$out"
        fi
        {
            printf '  {\n'
            printf '    "directory": "%s",\n' "$ROOT"
            printf '    "command": "%s -Iinclude -Isrc -I../libu89/include -I../libstr89/include -c %s",\n' "$compiler" "$f"
            printf '    "file": "%s"\n' "$abs"
            printf '  }'
        } >> "$out"
    done
    printf '\n]\n' >> "$out"
}

mkdir -p "$ROOT/build/gcc" "$ROOT/build/clang"
write_db "cc" "gcc" "$@"
write_db "clang" "clang" "$@"
