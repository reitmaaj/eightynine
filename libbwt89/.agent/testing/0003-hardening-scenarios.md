# 0003 - Scenarios: boundary, failure, and decode hardening

SCENARIO H-BND-01 oversized length
    GIVEN a valid non-NULL buffer pair and n = BWT89_MAX_N + 1
    WHEN any static transform runs
    THEN it returns BWT89_TOO_LARGE before allocation or input access
     AND the output/index buffers are unchanged
     AND the allocation counter is zero.

SCENARIO H-BND-02 SIZE_MAX
    GIVEN n = SIZE_MAX with valid pointers
    WHEN any static transform runs
    THEN it returns BWT89_TOO_LARGE with zero allocations and unchanged output.

SCENARIO H-BND-03 maximum boundary
    GIVEN n = BWT89_MAX_N and a forced first-allocation failure
    WHEN any static transform runs
    THEN it returns BWT89_NOMEM, not BWT89_TOO_LARGE.

SCENARIO H-ALLOC-01 every allocation ordinal
    GIVEN a transform whose success path performs A allocations
    WHEN allocation k (1..A) is forced to fail
    THEN the call returns BWT89_NOMEM
     AND zero live allocations remain
     AND caller output and index are byte-for-byte unchanged.

SCENARIO H-ALLOC-02 SA-IS recursive cleanup
    GIVEN a suffix-array input whose reduction recurses
    WHEN an allocation at any recursion ordinal is forced to fail
    THEN the call reports failure AND zero live allocations remain
     AND the recursive arena is freed on the error return path.

SCENARIO H-ALLOC-03 retry succeeds
    GIVEN a failed allocation ordinal k
    WHEN failure injection is disabled and the same call is repeated
    THEN it succeeds exactly as the uninjected run.

SCENARIO H-DEC-01 malformed regular transform
    GIVEN b = {0x00}, n = 1, index = 0 (not a forward image)
    WHEN bwt89_ibwt runs
    THEN it returns BWT89_BAD_DATA and leaves out unmodified
     AND no malformed (b,index) pair returns BWT89_OK.

SCENARIO H-ALIAS-01 regular in place
    GIVEN any small text and index
    WHEN bwt89_bwt / bwt89_ibwt run with out == in
    THEN the result equals the disjoint-buffer result
     AND ibwt recovers the text in the same buffer.

SCENARIO H-ALIAS-02 bijective in place
    GIVEN any small text
    WHEN bwt89_bbwt / bwt89_ibbwt run with out == in
    THEN bbwt("banana") in place equals "annbaa"
     AND ibbwt in the same buffer recovers the original text.

SCENARIO H-ALIAS-03 guard preservation
    GIVEN guards of 0xA5 adjacent to a payload
    WHEN any transform writes its payload
    THEN exactly n output bytes may differ and the guards stay 0xA5.
