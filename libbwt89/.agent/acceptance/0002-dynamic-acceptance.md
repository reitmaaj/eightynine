# 0002 - Acceptance: dynamic (Salson) incremental edits

Status: **partially implemented.** D1, D2, D4-D8 are met and verified by
`test_dyn.c` in the current baseline. D3 (sublinear LF-localized transform
maintenance) is **not yet met**: edits update the stored text with localized
byte moves and do not rebuild the transform at edit time, but the transform is
materialized on demand by recompute rather than repaired incrementally. D3 is
the outstanding Salson milestone.

## Must exhibit

ACCEPTANCE D1 incremental equality to recompute
    After every single-character edit (insert, delete, or substitute) the
    maintained transform MUST equal a freshly computed `bwt89_bwt` of the
    current text -- same `n` output bytes and same reported `index` -- for
    arbitrary byte data including embedded NULs, empty, single-byte, and
    periodic/repeated inputs.

ACCEPTANCE D2 round-trip from the handle
    The transform (bytes and index) reported by a dynamic handle MUST be
    accepted by `bwt89_ibwt` to reproduce the current text exactly.

ACCEPTANCE D3 no full recompute per edit
    An edit MUST update the maintained transform by localized changes through
    the LF/rank structure (Salson et al.) and MUST NOT rebuild the whole suffix
    array / BWT of the current text on every operation. (Substituting is a
    delete followed by an insert.)

ACCEPTANCE D4 handle lifecycle
    Opening a handle on initial text MUST yield a transform equal to
    `bwt89_bwt` of that text, and closing a handle MUST release all its
    storage.

## Must reject / fail safe

ACCEPTANCE D5 out-of-range and empty edits
    The system MUST reject `insert` with `pos > len`, `delete`/`substitute`
    with `pos >= len`, and `delete` on an empty handle, returning
    `BWT89_BAD_EDIT`, and MUST leave the maintained transform equal to the
    recompute of the unchanged text.

ACCEPTANCE D6 null arguments
    The system MUST reject a NULL handle or a NULL length/index/output pointer
    with `BWT89_NULL_ARG` and MUST NOT dereference them.

ACCEPTANCE D7 allocation failure
    On allocation failure the system MUST return `BWT89_NOMEM` and MUST NOT
    leave the handle in a state that diverges from the recompute of the last
    successful text.

ACCEPTANCE D8 memory safety
    The dynamic module MUST be memory-safe (no out-of-bounds reads or writes,
    no leaks) as verified under ASan/UBSan across the whole suite.
