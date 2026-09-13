# libu89 testing scenarios — extended grapheme clusters

## 0001 — Forward traversal
SCENARIO next advances one cluster
GIVEN valid UTF-8 and a position at or inside a cluster
WHEN u89_grapheme_next() is called
THEN it returns the smallest cluster boundary strictly greater than the
    position, and n at the end of input.

## 0002 — Backward traversal
SCENARIO prev retreats one cluster
GIVEN valid UTF-8 and a position after at least one cluster
WHEN u89_grapheme_prev() is called
THEN it returns the largest cluster boundary strictly less than the position,
    and 0 at the start of input.

## 0003 — Boundary predicate
SCENARIO boundary agrees with traversal
GIVEN a string whose official boundaries are known
WHEN u89_grapheme_boundary() is called at every byte position
THEN it is true exactly at positions 0, n, and every official boundary.

## 0004 — Conformance
SCENARIO the official corpus passes in both directions
GIVEN every row of the pinned Unicode GraphemeBreakTest.txt
WHEN forward traversal, backward traversal, the boundary predicate, and the
    prev(next(p)) == p inverse relation are evaluated
THEN all four agree with the official boundary notation.

## 0005 — Context rules
SCENARIO Indic conjuncts, emoji ZWJ, and regional indicators
GIVEN sequences exercising GB9c, GB11, GB12, and GB13, including long
    Extend runs and odd regional-indicator runs
WHEN clustered
THEN the context-dependent joins match UAX #29 and never loop or split a
    scalar.

## 0006 — Allocation freedom
SCENARIO iteration performs no heap allocation
GIVEN any valid input
WHEN next/prev/boundary are called
THEN no allocation function is invoked.

## 0007 — GB11 adjacency
SCENARIO a ZWJ only joins the pictograph immediately after it
GIVEN an extended pictograph, a ZWJ, then one or more Extend scalars
    (e.g. a variation selector), then another pictograph
WHEN clustered
THEN the pictographs do NOT join, because GB11 requires the ZWJ to be
    immediately before the second pictograph; forward and backward traversal
    agree.

## 0008 — Randomized traversal invariants
SCENARIO forward and backward traversal agree on random sequences
GIVEN random sequences drawn from interesting grapheme classes
WHEN next, prev, and boundary are evaluated at every byte position
THEN the forward and backward boundary lists are exact reverses, the
    boundary predicate matches, and next/prev at arbitrary positions match
    the boundary sets.
