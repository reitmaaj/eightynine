# 0001 - Testing scenarios (BDD)

## SA-IS suffix array

SCENARIO SA-1 suffix array equals reference ordering
    GIVEN a text over ranks 1..K with a unique-min 0 sentinel appended
    WHEN bwt89_sa_is is called
    THEN the result equals a brute-force lexicographic suffix sort of the text

SCENARIO SA-2 handles repeated and single-symbol texts
    GIVEN a text of one distinct symbol (e.g. "aa...a") or alternating symbols
    WHEN the suffix array is computed
    THEN it equals the reference sort

## Regular transform (bwt / ibwt)

SCENARIO BWT-1 forward reports the original row index
    GIVEN any nonempty byte sequence
    WHEN bwt89_bwt runs
    THEN it writes n bytes to out and sets index in [0, n)

SCENARIO BWT-2 inverse recovers the input
    GIVEN the bytes and index produced by bwt89_bwt
    WHEN bwt89_ibwt runs
    THEN it reproduces the original n bytes exactly, including embedded NULs

SCENARIO BWT-3 round trip preserves length and content
    GIVEN arbitrary bytes including binary data
    WHEN bwt then ibwt is applied
    THEN the result equals the input

## Bijective transform (bbwt / ibbwt) -- implemented, oracle-verified

SCENARIO BBWT-1 forward needs no index
    GIVEN any byte sequence
    WHEN bwt89_bbwt runs
    THEN it writes exactly n bytes with no auxiliary index

SCENARIO BBWT-2 the map is a permutation
    GIVEN all sequences of length <= k over a small alphabet
    WHEN bwt89_bbwt is applied
    THEN the resulting multiset equals the input multiset and distinct inputs
         map to distinct outputs (injective) covering all images (surjective)

SCENARIO BBWT-3 inverse round trips
    GIVEN bytes produced by bwt89_bbwt
    WHEN bwt89_ibbwt runs
    THEN it reproduces the original bytes

SCENARIO BBWT-4 matches canonical oracle vectors
    GIVEN the reference inputs "banana" and "^BANANA$"
    WHEN the bijective transform is applied
    THEN output equals "annbaa" and "$ANNBAA^" respectively, byte-for-byte as
         the OpenBWT BWTS oracle

SCENARIO BBWT-5 inverse matches the oracle inverse
    GIVEN bytes produced by bwt89_bbwt on randomized and structured inputs
    WHEN bwt89_ibbwt runs
    THEN the recovered bytes equal both the original input and UnBWTS output

## Dynamic transform

SCENARIO DYN-1 maintained transform matches recompute
    GIVEN a maintained dynamic structure built from some text
    WHEN a sequence of insert/delete/substitute edits is applied
    THEN after each edit the maintained transform equals bwt89_bwt(text)

## Unacceptable behaviour (must reject)

SCENARIO ERR-1 rejects null arguments
    GIVEN a null buffer or length pointer
    WHEN any transform is invoked
    THEN BWT89_NULL_ARG is returned and nothing is written

SCENARIO ERR-2 rejects an out-of-range index
    GIVEN ibwt with index not in [0, n)
    THEN BWT89_BAD_INDEX is returned

SCENARIO ERR-3 reports allocation failure safely
    GIVEN allocation failure during a transform
    THEN BWT89_NOMEM is returned without corrupting caller buffers
