# 0001 - Design

## Modules

| Translation unit | Responsibility |
|------------------|----------------|
| `src/bwt89_sa.c` | SA-IS suffix array over an integer alphabet with unique-min sentinel (`bwt89_sa_is`). O(n). |
| `src/bwt89_bwt.c` | Regular `bwt`/`ibwt` over the internal header contract. |
| `src/bwt89_bwts.c` | Bijective `bbwt`/`ibbwt` (Duval + conjugation sort). |
| `src/bwt89_dyn.c` | Incremental dynamic edits. |

`include/bwt89.h` is the only public surface; `src/bwt89_internal.h` carries
shared prototypes (green requires `-Wmissing-prototypes`).

## Encodings

- Bytes are mapped to ranks `1..sigma`; internal text is stored as `int`
  arrays with a `0` sentinel for suffix-array work.
- **Regular forward:** compute suffix array of `s + 0`; the transform is the
  predecessor bytes `s[SA[i]-1]` over the `n` real rows; `index` is where the
  original rotation sits. **Regular inverse:** LF-mapping reconstruction
  (first column = sorted bytes, walk back from `index`).
- **Bijective:** factor into non-increasing Lyndon words (Duval), order the
  block conjugates by the infinite-repeat comparison, take last bytes.
  **Inverse:** LF reconstruction recovering the Lyndon blocks in reverse order.
- **Dynamic:** maintain text + BWT + occurrence tables; edits apply local
  reorderings (Salson et al.).

## Correctness oracles (implementation-independent)

- `SA-IS` equals a brute-force suffix sort on randomized/structured inputs.
- `bwt/ibwt` round-trips all inputs; exhaustive permutation property.
- `bbwt/ibbwt` is a permutation (injective + surjective) on small alphabets;
  round-trips; preserves character multiset; byte-equals OpenBWT `BWTS` /
  `UnBWTS` (the canonical oracle).
- Dynamic: every edit step matches `bwt(text)` recompute.

## Status

- **Implemented and fully green on `main`:** regular `bwt`/`ibwt` (SA-IS +
  LF) and bijective `bbwt`/`ibbwt` (Duval + conjugation sort). `just green`
  and `just test` PASS on `main`.
- **Planned:** `src/bwt89_dyn.c` incremental dynamic edits.

## Green-flat note (bijective entry points)

`green-flat` forbids inline computation (a `Work` statement) inside any
non-root control/block body; an assignment whose RHS is an array subscript is
`Work` (see green `tidy/GreenChecks.cc` `classifyExpr`). The bbwt entry points
originally failed with `if (n == 1) { out[0] = s[0]; }` because `s[0]` is not a
"simple" load. Removing that redundant `n==1` shortcut (the general path already
handles `n==1`) cleared both diagnostics without changing behaviour.
