# 0001 - Acceptance

## Must exhibit

ACCEPTANCE A1 round-trip
    The system MUST recover the original input from a transform+inverse round
    trip for arbitrary byte data, including embedded NUL bytes and empty and
    single-byte inputs.

ACCEPTANCE A2 index-based inverse
    bwt89_ibwt MUST accept the index reported by bwt89_bwt and return the
    original bytes without adding or removing any sentinel byte.

ACCEPTANCE A3 permutation bijective map
    bwt89_bbwt MUST be a bijection on byte strings of a fixed length (it is a
    permutation), verified exhaustively on a small alphabet; bwt89_ibbwt MUST
    invert it.

ACCEPTANCE A4 oracle vectors
    bwt89_bbwt("banana") MUST equal "annbaa" and bwt89_bbwt("^BANANA$")
    MUST equal "$ANNBAA^", byte-for-byte as the OpenBWT BWTS oracle; the
    inverse MUST match UnBWTS.

ACCEPTANCE A5 dynamic correctness
    A dynamically maintained transform MUST equal a freshly computed
    bwt89_bwt of the current text after every applied edit.

ACCEPTANCE A6 linear core
    Forward construction MUST be built on the O(n) SA-IS engine and MUST NOT
    sort rotations by a naive quadratic comparison.

## Must reject / fail safe

ACCEPTANCE A7 null arguments
    The system MUST reject NULL buffer pointers or NULL length/index outputs
    with BWT89_NULL_ARG and MUST NOT dereference them.

ACCEPTANCE A8 bad index
    bwt89_ibwt MUST reject an index outside [0, n) with BWT89_BAD_INDEX.

ACCEPTANCE A9 allocation failure
    On allocation failure the system MUST return BWT89_NOMEM and MUST NOT
    leave caller buffers partially overwritten in a way that appears valid.

ACCEPTANCE A10 memory safety
    The transforms MUST be memory-safe: no out-of-bounds reads or writes and
    no leaks, as verified under ASan/UBSan across the whole test suite.
