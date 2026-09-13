# 0002 - Testing scenarios (BDD): dynamic (Salson) incremental edits

Status: **partially implemented.** The correctness scenarios (recompute
equality after every edit, round-trip, rejection, lifecycle) are implemented
and pass in `test_dyn.c` against the current baseline. The scenario implying
sublinear per-edit transform maintenance remains future work (see
`acceptance/0002` D3).

## Construction

SCENARIO DYN-1 opening on initial text
    GIVEN an initial byte sequence, possibly empty or containing NULs
    WHEN a dynamic handle is opened on it
    THEN the handle length equals n and its transform equals bwt89_bwt(text)

## Editing (must exhibit)

SCENARIO DYN-2 maintained transform equals recompute after every insert
    GIVEN a handle on some text
    WHEN a byte is inserted at any valid position (including append)
    THEN immediately the maintained transform equals bwt89_bwt(new text), byte
         for byte and with the same index

SCENARIO DYN-3 maintained transform equals recompute after every delete
    GIVEN a handle on some nonempty text
    WHEN a byte at any position is deleted
    THEN immediately the maintained transform equals bwt89_bwt(new text)

SCENARIO DYN-4 maintained transform equals recompute after every substitute
    GIVEN a handle on some nonempty text
    WHEN the byte at any position is replaced
    THEN immediately the maintained transform equals bwt89_bwt(new text)

SCENARIO DYN-5 arbitrary edit sequences
    GIVEN a handle
    WHEN any sequence of insert/delete/substitute edits is applied
    THEN after every single edit step the maintained transform equals
         bwt89_bwt of the current text

SCENARIO DYN-6 inverse round-trip from the maintained transform
    GIVEN the transform bytes and index reported by a handle
    WHEN bwt89_ibwt is applied
    THEN it reproduces the current text exactly, including embedded NULs

## Unacceptable behaviour (must reject / fail safe)

SCENARIO DYN-7 rejects invalid edits without corruption
    GIVEN a handle
    WHEN insert with pos > len, or delete/substitute with pos >= len, or a
         delete on an empty handle, is attempted
    THEN BWT89_BAD_EDIT is returned and the transform still equals
         bwt89_bwt of the unchanged text

SCENARIO DYN-8 rejects null arguments
    GIVEN a NULL handle or a NULL length/index/output pointer
    WHEN any dynamic routine is invoked
    THEN BWT89_NULL_ARG is returned and nothing is written

SCENARIO DYN-9 reports allocation failure safely
    GIVEN allocation failure during handle creation or an edit
    THEN BWT89_NOMEM is returned without corrupting the handle
