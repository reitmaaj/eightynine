# libu89 testing scenarios — UTF-16 surrogate primitives

## 0001 — Surrogate classification
SCENARIO classify a code unit as a high surrogate
GIVEN an integer code unit
WHEN u89_utf16_is_high_surrogate() is called
THEN it is true exactly for 0xD800..0xDBFF and false for every other value,
    including 0xD7FF, 0xDC00, 0xDFFF, 0xE000, and values above 0xFFFF.

SCENARIO classify a code unit as a low surrogate
GIVEN an integer code unit
WHEN u89_utf16_is_low_surrogate() is called
THEN it is true exactly for 0xDC00..0xDFFF and false for every other value,
    including 0xD7FF, 0xD800, 0xDBFF, 0xE000, and values above 0xFFFF.

## 0002 — Pair decoding
SCENARIO decode a valid surrogate pair
GIVEN a high surrogate and a low surrogate
WHEN u89_utf16_decode_pair() is called
THEN it returns success and the supplementary scalar
    0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00).

SCENARIO reject an invalid pair
GIVEN any combination that is not high-then-low, or any code unit outside the
    surrogate ranges
WHEN u89_utf16_decode_pair() is called
THEN it returns failure and leaves *cp unchanged.

SCENARIO validity query without a destination
GIVEN a valid pair and cp == NULL
WHEN u89_utf16_decode_pair() is called
THEN it returns success and writes nothing.

## 0003 — Encoding consistency
SCENARIO pair round-trips through UTF-8
GIVEN a valid surrogate pair
WHEN the decoded scalar is encoded with u89_utf8_encode() and decoded with
    u89_utf8_decode()
THEN the recovered scalar equals the decoded scalar, u89_utf16_units() reports
    2, and the encoded length is 4.
