# 0005 — Unicode delegation scenarios (BDD)

## 0001 — Raw UTF-8 delegation
SCENARIO accept every raw scalar boundary
GIVEN a JSON string containing the raw UTF-8 encoding of U+007F, U+0080,
    U+07FF, U+0800, U+D7FF, U+E000, U+FFFF, U+10000, or U+10FFFF
WHEN j89_parse is called
THEN the stored bytes equal the input bytes exactly.

SCENARIO reject malformed raw UTF-8
GIVEN a JSON string or object key containing a lone continuation byte, a
    C0/C1 or E0/F0 overlong form, an encoded surrogate, an encoding above
    U+10FFFF, an F5 lead, or a truncated 2/3/4-byte sequence
WHEN j89_parse is called
THEN it reports an error and returns J89_BAD.

SCENARIO raw bytes are copied, not canonicalized
GIVEN a raw UTF-8 scalar
WHEN it is parsed
THEN the stored bytes are byte-identical to the input sequence.

## 0002 — Escaped scalar delegation
SCENARIO BMP escapes decode to their scalar
GIVEN `\u0000`, `\u007f`, `\u0080`, `\ud7ff`, `\ue000`, or `\uffff`
WHEN j89_parse is called
THEN the stored bytes are the UTF-8 encoding of that scalar.

SCENARIO surrogate pairs decode to a supplementary scalar
GIVEN `\ud800\udc00`, `\ud800\udfff`, `\udbff\udc00`, or `\udbff\udfff`
WHEN j89_parse is called
THEN the stored bytes are the UTF-8 encoding of U+10000, U+103FF, U+10FC00,
    or U+10FFFF respectively.

SCENARIO malformed surrogate escapes are rejected
GIVEN a lone low surrogate, a lone high surrogate, a high surrogate followed
    by a BMP escape, a high surrogate followed by another high surrogate, a
    high surrogate followed by `\n`, a high surrogate followed by end of
    input, or a malformed hex escape
WHEN j89_parse is called
THEN it reports an error and returns J89_BAD.

SCENARIO raw and escaped forms are equivalent
GIVEN the raw UTF-8 bytes of a scalar and its `\uXXXX` or surrogate-pair
    escape notation
WHEN both are parsed
THEN the stored byte sequences and lengths are identical.

## 0003 — Object keys
SCENARIO keys follow the same Unicode rules
GIVEN an object whose key uses a raw scalar boundary, a BMP escape, or a
    surrogate pair
WHEN j89_parse is called
THEN the key bytes are stored exactly; malformed UTF-8 or escapes in a key
    are rejected exactly as in a string value.

## 0004 — Builder representation invariant
SCENARIO builder accepts well-formed UTF-8
GIVEN bytes that are well-formed UTF-8, including multibyte and embedded NUL
WHEN j89_string_new or j89_object_set is called
THEN the bytes are stored exactly and the key length is retrievable.

SCENARIO builder rejects malformed UTF-8 before allocating
GIVEN malformed UTF-8 for a string value or object key
WHEN j89_string_new or j89_object_set is called
THEN it returns J89_BAD, marks the arena failed, records a message naming
    UTF-8, and leaves the arena offset (and any previously written member
    slot) unchanged.

SCENARIO key length is explicit
GIVEN an object member whose key contains an embedded NUL
WHEN j89_object_key_length is called
THEN it returns the full byte length, and the algebraic bridge carries the
    key without truncation.

## 0005 — Cross-library contract
SCENARIO j89 and u89 cannot disagree about raw UTF-8
GIVEN a byte sequence with no raw quote, backslash, or U+0000..U+001F
WHEN u89_utf8_valid is evaluated and the sequence is embedded in a JSON
    string and parsed
THEN validity and parse success agree exactly.

## 0006 — Rendering
SCENARIO boundary scalars round-trip
GIVEN every boundary scalar
WHEN it is parsed, rendered, and parsed again
THEN the second tree stores the same bytes as the first.
