#!/bin/sh -eu
set -eu
# audit.sh - symbol and API audit for libcksum89.
#
# 1. The archive must reference no allocation or I/O symbol.
# 2. Exported symbols must be exactly the public API plus the documented
#    internal tables and shared helpers.

root="$(cd "$(dirname "$0")/.." && pwd)"
lib="$root/build/libcksum89.a"

if [ ! -e "$lib" ]; then
    echo "audit: $lib missing; run 'just build' first"
    exit 1
fi

forbidden=$(nm -u "$lib" | awk 'NF >= 2 { print $NF }' |
    grep -E '^(malloc|calloc|realloc|free|open|close|read|write|lseek|fopen|fclose|fread|fwrite|printf|fprintf)$' || true)
if [ -n "$forbidden" ]; then
    echo "audit: forbidden undefined symbols:"
    printf '%s\n' "$forbidden"
    exit 1
fi

expected="cksum89_crc32_iso_hdlc
cksum89_crc32_iso_hdlc_final
cksum89_crc32_iso_hdlc_init
cksum89_crc32_iso_hdlc_table
cksum89_crc32_iso_hdlc_update
cksum89_crc32_step
cksum89_crc32c
cksum89_crc32c_final
cksum89_crc32c_init
cksum89_crc32c_table
cksum89_crc32c_update
cksum89_crc64_nvme
cksum89_crc64_nvme_final
cksum89_crc64_nvme_init
cksum89_crc64_nvme_table
cksum89_crc64_nvme_update
cksum89_crc64_shift8
cksum89_crc64_xor
cksum89_inet16
cksum89_inet16_final
cksum89_inet16_init
cksum89_inet16_update"

actual=$(nm -g --defined-only "$lib" | awk 'NF >= 3 { print $NF }' | sort -u)
want=$(printf '%s\n' "$expected" | sort -u)

if [ "$actual" != "$want" ]; then
    echo "audit: exported symbol set mismatch"
    echo "--- actual ---"
    printf '%s\n' "$actual"
    echo "--- expected ---"
    printf '%s\n' "$want"
    exit 1
fi

echo "audit: ok"
