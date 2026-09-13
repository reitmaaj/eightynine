# libu89 acceptance tests — extended grapheme clusters (UAX #29)

## 0003 — Unacceptable behavior (must reject / fail safely)

- GR-13. Forward traversal MUST always advance: `next(p) > p` while
  `p < n`; a non-advancing or looping traversal is a failure.
- GR-14. Backward traversal MUST always retreat: `prev(p) < p` while
  `p > 0`.
- GR-15. `next` and `prev` MUST NOT split a UTF-8 sequence; returned
  positions always lie on scalar boundaries.
- GR-16. Iteration MUST NOT allocate.

## 0004 — Required behavior (must exhibit)

- GR-01. Every boundary in the pinned `GraphemeBreakTest.txt` corpus is
  reported by `u89_grapheme_boundary()`.
- GR-02. Repeated `u89_grapheme_next()` from 0 yields exactly the official
  boundaries.
- GR-03. Repeated `u89_grapheme_prev()` from `n` yields the same boundaries
  in reverse.
- GR-04. For every boundary `p`, `prev(next(p)) == p`; for every internal
  boundary, the forward and backward traversals agree.
- GR-05. `e` + combining acute is one cluster.
- GR-06. A base with multiple combining marks is one cluster.
- GR-07. An emoji with a skin-tone modifier is one cluster.
- GR-08. A ZWJ family sequence is one cluster.
- GR-09. A regional-indicator flag pair is one cluster; an odd run leaves
  the last indicator in its own cluster.
- GR-10. CRLF is one cluster; CR followed by a combining mark is two.
- GR-11. Empty input: `next(0) == 0`, `prev(0) == 0`, boundary at 0.
- GR-12. Positions 0 and `n` always report boundaries; interior byte
  offsets of a multi-byte scalar never do.
- GR-17. Indic conjuncts (Consonant Linker Consonant) form one cluster
  (GB9c), including `Extend*` between the linker and the consonant.
- GR-18. Hangul L V T and LV T sequences form one cluster.
- GR-19. Prepend attaches to the following scalar; a SpacingMark attaches to
  the preceding scalar.
- GR-20. A ZWJ after a pictograph only joins a following pictograph, never a
  non-pictograph (GB11).
- GR-21. GB11 MUST NOT join across `Pictograph ZWJ Extend* Pictograph`: an
  Extend scalar between the ZWJ and the second pictograph breaks the join
  (the ZWJ must be immediately before the pictograph).
- GR-22. Forward and backward traversal MUST agree on randomized sequences
  drawn from grapheme-class boundary cases.
