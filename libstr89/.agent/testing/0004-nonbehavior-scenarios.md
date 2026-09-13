# 0004 — non-behavior and corpus scenarios

## Unicode validity corpus

SCENARIO scalar and sequence-length boundaries
  GIVEN encoded values at U+0000, U+0001, U+007F, U+0080, U+07FF, U+0800,
    U+D7FF, U+E000, U+FFFF, U+10000, U+10FFFF, and their neighbours
  WHEN `str89_view_init` is called
  THEN the result is `STR89_OK`

SCENARIO malformed corpus
  GIVEN lone continuations, overlong encodings, truncated sequences,
    surrogate encodings, values above U+10FFFF, bad prefixes with valid
    suffixes, and valid prefixes with bad suffixes
  WHEN `str89_view_init` is called
  THEN the result is `STR89_EUTF8`

SCENARIO differential against libu89
  GIVEN random byte sequences and randomly corrupted valid sequences
  WHEN `str89_view_init` is called
  THEN it succeeds if and only if `u89_utf8_valid` succeeds

## Guard pages

SCENARIO no over-read
  GIVEN input bytes placed so that the final byte abuts an inaccessible page
  WHEN `str89_view_init`, equality, ordering, search, subview, append, and set
    are exercised, including a multibyte scalar ending at the last accessible
    byte
  THEN no access beyond the reported length occurs

## Build-language matrix

SCENARIO C89, C23, and C++23
  GIVEN the same behavioral suite
  WHEN built with gcc and clang under `-std=c89` and `-std=c23`, and the
    public header is included from `-std=c++23` translation units
  THEN every suite passes and the header compiles warning-free
