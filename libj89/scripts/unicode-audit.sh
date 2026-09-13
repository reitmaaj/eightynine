#!/bin/sh -eu
# unicode-audit.sh - no UTF-8/UTF-16 algorithm may remain in the practical
# j89 sources. Unicode facts are delegated to libu89. src/alg is exempt: its
# S carrier is an uninterpreted byte string with no Unicode dependency.
#
# The negative requirement of the delegation migration: lead-length
# classification, continuation checks, scalar encoders, surrogate predicates,
# and surrogate arithmetic must not reappear here.

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
PATTERN='j89_utf8_encode|j89_lead_len|j89_cont_byte_ok|j89_is_high|j89_is_low|0xD800|0xDBFF|0xDC00|0xDFFF'

if grep -REn --exclude-dir=alg "$PATTERN" "$ROOT/src"; then
    echo "unicode-audit: UTF-8/UTF-16 algorithm found in j89 sources" >&2
    exit 1
fi
echo "unicode-audit: clean"
