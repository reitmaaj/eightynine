# 0005 — Unicode delegation acceptance

## Must exhibit

ACCEPT: raw UTF-8 scalar boundaries U+007F, U+0080, U+07FF, U+0800, U+D7FF,
U+E000, U+FFFF, U+10000, and U+10FFFF parse and store byte-identically.
ACCEPT: BMP escapes `\u0000`, `\u007f`, `\u0080`, `\ud7ff`, `\ue000`, and
`\uffff` decode to the UTF-8 encoding of the scalar.
ACCEPT: surrogate pairs `\ud800\udc00`, `\ud800\udfff`, `\udbff\udc00`, and
`\udbff\udfff` decode to U+10000, U+103FF, U+10FC00, and U+10FFFF.
ACCEPT: a raw scalar and its escape notation produce identical stored bytes.
ACCEPT: object keys follow the same raw/escape rules as string values.
ACCEPT: `j89_string_new` accepts well-formed UTF-8, including embedded NUL,
and stores it exactly.
ACCEPT: `j89_object_set` accepts a well-formed UTF-8 key, including embedded
NUL, and `j89_object_key_length` reports the exact byte length.
ACCEPT: the algebraic bridge carries an embedded-NUL key without truncation.
ACCEPT: `u89_utf8_valid(s) == 1` implies `j89_parse("\"" + s + "\"")`
succeeds for sequences free of raw quote, backslash, and U+0000..U+001F.
ACCEPT: every boundary scalar parses, renders, and re-parses to the same
stored bytes.

## Must reject (unacceptable behavior)

REJECT: malformed raw UTF-8 in a string value or object key: lone
continuation byte, C0/C1 overlong, E0/F0 overlong, encoded surrogate,
encoding above U+10FFFF, F5 lead, or truncated 2/3/4-byte sequence.
REJECT: a lone low surrogate escape (`\udc00`).
REJECT: a lone high surrogate escape (`\ud800`), including one followed by
end of input.
REJECT: a high surrogate followed by a BMP escape (`\ud800\u0041`), by
another high surrogate (`\ud800\ud800`), or by a non-`\u` escape.
REJECT: a malformed hex escape inside `\uXXXX`.
REJECT: malformed UTF-8 supplied to `j89_string_new`, returning J89_BAD,
marking the arena failed, recording a message naming UTF-8, and consuming no
arena storage.
REJECT: a malformed UTF-8 object key supplied to `j89_object_set`, marking
the arena failed and leaving the target member slot unchanged.
REJECT: `u89_utf8_valid(s) == 0` while `j89_parse("\"" + s + "\"")` succeeds
for sequences free of raw quote, backslash, and U+0000..U+001F.
REJECT: any UTF-8 encoding/decoding algorithm remaining in `src/*.c`
(excluding `src/alg`), as enforced by the `unicode-audit` tripwire.
