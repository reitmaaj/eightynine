# 0001-acceptance

## Must exhibit

ACCEPT: a well-formed WID-style JSON object parses to a faithful tree.
ACCEPT: escapes and UTF-8 strings decode to their content bytes.
ACCEPT: `j89_string_value` and `j89_object_key` return NUL-terminated bytes at
the reported length; a string may still contain embedded NUL bytes, which
require the length accessors to read exactly.
ACCEPT: integers (including negative and zero) parse to their exact value,
including the full exact `double` range |n| <= 2^53.
ACCEPT: the parser builds warning-free on ILP32 and accepts/rejects the same
integer range as LP64, because the accumulator is a fixed 64-bit type rather
than a host `long`.
ACCEPT: `true`, `false`, `null`, arrays, and nested objects parse.
ACCEPT: a fractional or exponent number (`1.5`, `1e3`, `-0.5`, `123e65`) parses
to a FLOAT node whose double value is correct.
ACCEPT: an optional leading UTF-8 BOM (EF BB BF) before a document is ignored.
ACCEPT: `j89_parse` returns `J89_BAD` and records a message on any syntax
error, and the caller may continue to use the arena.

## Must reject (unacceptable behavior)

REJECT: a malformed number grammar (`1.`, `.5`, `1e`, `1e+`, `01`, bare `-`,
`Infinity`, `NaN`).
REJECT: a number whose magnitude exceeds the representable `double` range
(e.g. `1e999`), without overflowing or crashing.
REJECT: invalid UTF-8 inside a string value or object key (lone continuation
byte, truncated sequence, overlong encoding, encoded surrogate, code point
above U+10FFFF).
REJECT: a document preceded by a partial BOM (EF, or EF BB).
REJECT: an object with duplicate member keys (e.g. `{"a":1,"a":2}`).
REJECT: trailing non-whitespace after the root value.
REJECT: malformed structural JSON (missing colon/comma/bracket, stray
tokens, unknown literals).
REJECT: an unterminated string, an invalid `\uXXXX` escape, or a control
character inside a string.
REJECT: an empty/invalid root (e.g. a leading comma).

REJECT: an integer whose magnitude exceeds the exact `double` range (|n| > 2^53,
e.g. `9007199254740993`), without overflowing or crashing.

REJECT: returning success without consuming the input or without a valid
root node on malformed input.
REJECT: returning an INTEGER node for a token that carries a fraction or an
exponent, or a FLOAT node for a pure-integer token.
