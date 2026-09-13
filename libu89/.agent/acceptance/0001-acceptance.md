# libu89 acceptance tests

## 0001 — Unacceptable behavior (must reject / fail safely)

The following behaviors are UNACCEPTABLE and MUST be rejected, refused, or
reported as errors. Each maps to an automated test.

- A. UTF-8 overlong encoding (e.g. U+0000 as `C0 80`, U+2F as `C0 AF`) MUST be rejected.
- B. A UTF-8-encoded surrogate (e.g. U+D800 as `ED A0 80`) MUST be rejected.
- C. A UTF-8 encoding of a code point above U+10FFFF (e.g. `F4 90 80 80`) MUST be rejected.
- D. A truncated UTF-8 sequence at end of input MUST be rejected.
- E. A lone or mis-paired UTF-8 continuation byte MUST be rejected.
- F. The surrogate range U+D800..U+DFFF MUST NOT be a scalar.
- G. A string MUST NOT be assumed NUL-terminated: embedded U+0000 MUST be
   preserved per the explicit length.
- H. The iteration API MUST NOT silently recover invalid input to U+FFFD;
   it MUST stop and report an error.
- I. `XID_Start` MUST reject non-start scalars (digits, marks, punctuation).
- J. A control scalar MUST be flagged as unprintable/zero-width, never
   reported as a positive width.
- K. Word wrap MUST NOT emit a line wider than the maximum allowed width.
- L. Word wrap MUST NOT split a grapheme cluster.
- M. Word wrap MUST NOT break except at Unicode whitespace.
- N. Normalization MUST be idempotent (NFC(NFC(x)) == NFC(x), NFD likewise).
- O. Normalization MUST NOT corrupt embedded NUL and MUST respect explicit lengths.
- P. A combining sequence normalized must not be shorter than its base scalar.
- Q. A leading non-starter MUST NOT block composition of the following
   starter+combining sequence (UAX #15 LastClass resets at a starter).
- R. Equal-ccc combining marks MUST NOT be reordered.
- S. An undersized normalization destination MUST return -1, never overflow.
- T. The sizing call MUST return a capacity sufficient for a successful
   normalization into a buffer of exactly that size.
- U. A decomposition with a repeated character (e.g. U+FDFA, U+2A74) MUST
   preserve every occurrence, not truncate due to cycle detection.
- V. Every primary composite MUST be recomposable under NFC/NFKC, including
   those whose full decomposition exceeds two code points (via their direct
   two-code-point canonical decomposition).
- W. Hangul L+V (and LV+T) MUST compose ONLY when adjacent; a combining mark
   between L and V MUST block composition.
- W2. NFC MUST compose a canonical pair whose second member carries ccc 0
   (e.g. U+09C7 U+09BE -> U+09CB); generic composition MUST NOT be gated on a
   nonzero second-member combining class.
- X2. Normalization SHALL pass the complete pinned Unicode NormalizationTest.txt
   suite (every row, all four forms, no sampling).
- X. NFKC/NFKD MUST match a reference implementation (unicodedata) on the
   common BMP codepoint set (differential test).

## 0002 — Required behavior (must exhibit)

- A. Valid UTF-8, including supplementary code points and embedded NUL,
   decodes and re-encodes to identical bytes.
- B. Scalar boundaries: U+0000..U+D7FF and U+E000..U+10FFFF are scalars.
- C. XID_Start/XID_Continue accept Latin, Greek, and CJK start/continue scalars.
- D. Width: narrow=1, wide/fullwidth=2, combining=0; ambiguous resolves per policy.
- E. Wrap: all lines ≤ max width; breaks only at Unicode whitespace.
- F. NFC/NFD are canonical and mutually inverse for canonical sequences.
- G. NFKC/NFKD fold compatibility characters (e.g. full-width forms).
- H. Hangul syllables compose (L+V+T) and decompose (to L,V,T jamo).
