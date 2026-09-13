# 0003 - Acceptance: boundary hardening, failure atomicity, decode contract

Status: **implemented** (regular BWT path; alias/oracle and dynamic later files).

## Must exhibit

ACC_1001 length-bound rejection
    Every static transform (bwt, ibwt, bbwt, ibbwt) MUST return
    `BWT89_TOO_LARGE` when `n > BWT89_MAX_N` (and likewise for `n == SIZE_MAX`)
    before reading the input or performing any allocation, leaving the output
    and (for bwt) the reported index byte-for-byte unchanged.

ACC_1002 maximum boundary passes size validation
    At `n == BWT89_MAX_N` a forced first-allocation failure MUST return
    `BWT89_NOMEM`, never `BWT89_TOO_LARGE`; the shared cap leaves one symbol of
    headroom below `INT_MAX` so no internal int arithmetic can overflow.

ACC_1003 allocation-failure atomicity
    When the kth allocation of any transform fails, the call MUST return
    `BWT89_NOMEM`, leave zero live allocations (no leak), and leave caller
    output buffers and (for bwt) the index byte-for-byte unchanged.

ACC_1004 SA-IS recursion cleanup
    A failure in any recursive SA-IS arena MUST free the current arena before
    returning, so no allocation leaks on any failure ordinal of a recursive
    input.

## Must reject / fail safe

ACC_1005 malformed regular transform
    `bwt89_ibwt` MUST reject a `(b, index)` pair whose LF walk does not consume
    exactly `n` real symbols ending at the sentinel with `BWT89_BAD_DATA`, and
    MUST leave `out` unmodified. (See decode-contract file when implemented.)

## Exact in-place operation

ACC_1006 in-place all four transforms
    Every transform (bwt, ibwt, bbwt, ibbwt) MUST return the same result when
    `out == in` (same buffer) as when the input and output buffers are
    disjoint, and the forward/inverse pair MUST round-trip in a single buffer.
    Other partially overlapping ranges are outside the API contract.

ACC_1007 output extent and guards
    A successful call MUST write exactly the documented `n` output bytes and
    no guard bytes adjacent to the output buffer.

## Reproducible oracles

ACC_1008 exhaustive naive regular oracle
    bwt89_bwt MUST equal an independent naive rotation-sort oracle (same bytes
    and same index) for every text over an exhaustive small finite alphabet
    (binary through length 12, ternary through length 8).

ACC_1009 exhaustive SA oracle
    bwt89_sa_is MUST equal a brute-force suffix-array reference over every
    text with a fixed small alphabet and unique sentinel (binary through length
    13, ternary through length 9).

ACC_1010 OpenBWT differential gate
    `just oracle` MUST build Yuta Mori's OpenBWT v1.5 BWTS and byte-compare
    bwt89_bbwt / bwt89_ibbwt against it over fixed vectors, exhaustive small
    alphabets, and deterministic random inputs. It MUST fail loudly (never
    silently skip) if the pinned oracle source is absent.
