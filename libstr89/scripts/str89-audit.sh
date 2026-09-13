#!/bin/sh -eu
# str89-audit.sh - libstr89 owns no Unicode facts.
#
# Every UTF-8 validity, scalar, and encoding fact comes from libu89. This
# tripwire rejects a reintroduced UTF-8 codec (lead-byte classification,
# continuation checks, surrogate arithmetic), any Unicode transformation
# (normalization, case folding), and any NUL-terminated-string assumption
# (strlen/strcmp/strcpy family) in the owned sources.

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
PATTERN='0xC0|0xE0|0xF0|0xF8|0xFC|0xD800|0xDBFF|0xDC00|0xDFFF|u89_normalize|u89_casefold|strlen|strcmp|strcpy|strcat|strnlen|strncmp|strncpy'

if grep -REn -- "$PATTERN" "$ROOT/src"; then
    echo "str89-audit: Unicode codec, transformation, or NUL-string API found in src" >&2
    exit 1
fi
echo "str89-audit: clean"
