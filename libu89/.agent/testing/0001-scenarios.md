# libu89 testing scenarios (BDD)

## 0001 — UTF-8 validation and iteration
SCENARIO reject malformed UTF-8
GIVEN a byte range with an overlong encoding, a UTF-8-encoded surrogate,
    a code point above U+10FFFF, or a truncated sequence
WHEN it is validated
THEN validation reports failure and iteration stops with an error, never
    emitting U+FFFD.

SCENARIO accept valid scalars
GIVEN a byte range of valid UTF-8 including supplementary code points
    and embedded U+0000
WHEN it is iterated
THEN each scalar is decoded exactly once and the encoded form round-trips.

## 0002 — Scalar semantics
SCENARIO scalar boundary
GIVEN code points U+D800..U+DFFF
WHEN tested as scalars
THEN they are rejected as non-scalar, and U+0000..U+D7FF and U+E000..U+10FFFF
    are accepted.

## 0003 — XID identifiers
SCENARIO Unicode identifiers
GIVEN XID_Start and XID_Continue properties
WHEN asked whether a scalar may start or continue an identifier
THEN XID_Start scalars (and continuation scalars) are accepted and others are
    rejected.

## 0004 — Display width and wrap
SCENARIO width classes
GIVEN narrow, wide, ambiguous, combining, and control scalars
WHEN their display width is queried
THEN narrow/neutral yield 1, wide/fullwidth yield 2, ambiguous resolve per
    policy, combining/zero-width yield 0, and control scalars are flagged.

SCENARIO wrap at Unicode whitespace
GIVEN a UTF-8 string with Unicode whitespace (including NBSP, U+3000, U+2028)
WHEN wrapped at a maximum column width
THEN lines never exceed the width, breaks occur only at whitespace, and no
    grapheme cluster is split.

## 0005 — Normalization
SCENARIO canonical and compatibility forms
GIVEN text in various combining arrangements
WHEN normalized
THEN NFC and NFD are canonical forms (idempotent), NFKC/NFKD fold
    compatibility characters, and Hangul syllables compose/decompose correctly.

## 0006 — Composition blocking and leading non-starters
SCENARIO composition is not blocked by a preceding non-starter
GIVEN a leading non-starter before a starter+combining sequence
WHEN NFC is applied
THEN the starter still composes with its following combining mark (the
    non-starter before the starter does not block it).

SCENARIO equal combining classes do not reorder
GIVEN two combining marks with equal canonical combining class
WHEN normalized
THEN their relative order is preserved, and the second does not consume the
    first unless a primary composite exists.

## 0007 — Buffer sizing and safety
SCENARIO undersized buffer
GIVEN a normalization destination smaller than required
WHEN normalization runs
THEN it returns -1 and does not overflow the destination.

SCENARIO sizing call
GIVEN a normalize call with a null destination
WHEN it returns a required capacity
THEN a buffer of exactly that capacity is sufficient to normalize without error.

## 0008 — Differential conformance (unicodedata)
SCENARIO normalization matches a reference implementation
GIVEN random BMP strings over starters, combining marks, compatibility
    characters, and Hangul
WHEN NFC/NFD/NFKC/NFKD are applied by libu89 and by Python unicodedata
THEN the outputs are byte-identical (differential test, tools/difftest.py).

SCENARIO repeated characters in a decomposition are preserved
GIVEN a character whose compatibility decomposition repeats a character
    (e.g. U+FDFA, U+2A74)
WHEN NFKD/NFKC is applied
THEN every occurrence is preserved (no truncation from cycle detection).

SCENARIO multi-step canonical composites recompose
GIVEN a primary composite whose full decomposition exceeds two code points
    (e.g. U+1F06)
WHEN NFC is applied to its decomposed form
THEN it recomposes via its direct two-code-point canonical decomposition.

SCENARIO Hangul jamo composition requires adjacency
GIVEN an L jamo and a V jamo separated by a combining mark
WHEN NFC is applied
THEN they do NOT compose into a Hangul syllable.

## 0009 — Canonical composition with a ccc-0 second member
SCENARIO generic composition applies to starter/starter pairs
GIVEN a canonical pair whose second member also carries ccc 0, such as
    U+09C7 BENGALI VOWEL SIGN E + U+09BE BENGALI VOWEL SIGN AA
WHEN NFC is applied
THEN the pair canonically composes (U+09C7 U+09BE -> U+09CB), because generic
    composition must run regardless of the second scalar's combining class.

SCENARIO composition is blocked between non-starters
GIVEN a starter followed by combining marks of non-decreasing class
WHEN NFC is applied
THEN a later mark with a class no larger than its predecessor is not consumed
    (UAX #15 blocking rule), preserving canonical ordering.

## 0010 — Positional UTF-8 navigation
SCENARIO decode at an arbitrary scalar boundary
GIVEN valid UTF-8 and a byte position on a scalar boundary
WHEN u89_utf8_decode() runs at that position
THEN it returns U89_OK, the scalar, and the first byte after the scalar.

SCENARIO decode past the end
GIVEN a byte position equal to or greater than the input length
WHEN decoding
THEN U89_ERANGE is returned, never U89_EUTF8 and never a scalar.

SCENARIO reject malformed input at a position
GIVEN overlong, surrogate, out-of-range, truncated, isolated-continuation,
    invalid-lead, or bad-continuation bytes
WHEN decoding
THEN U89_EUTF8 is returned and no scalar is produced.

SCENARIO find the preceding scalar
GIVEN a position on a scalar boundary
WHEN u89_utf8_prev() runs
THEN it returns the scalar ending at that position and its starting offset,
    and prev at position 0 returns U89_ERANGE.

SCENARIO positional forward/backward agreement
GIVEN every scalar of a valid string
WHEN decoded forward and then reversed with u89_utf8_prev()
THEN the same scalar and start offset are recovered.

SCENARIO expected sequence length from a lead byte
GIVEN a leading byte that may begin a UTF-8 sequence
WHEN u89_utf8_seq_len() is asked
THEN it reports 1..4 for valid leads and 0 for continuation bytes, overlong
    leads (C0, C1), and leads above F4, so callers can distinguish a
    truncated sequence from a malformed one.

## 0011 — ILP32 portability
SCENARIO build and test on a 32-bit target
GIVEN the same sources and the strict C89 flag set
WHEN compiled and tested with -m32
THEN the library builds warning-free and every unit check passes, with no
    arithmetic that depends on the host unsigned long width: the
    normalization work-bound guard compares against the size_t maximum, and
    the grapheme random generator selects a native-width LCG.



