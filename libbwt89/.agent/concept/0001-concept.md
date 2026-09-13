# 0001 - libbwt89 concept

## Purpose

`libbwt89` is a strict-C89, green-compliant, dependency-free library
implementing variants of the Burrows-Wheeler transform over arbitrary bytes:

- **Regular** (`bwt89_bwt` / `bwt89_ibwt`) - the classic forward/inverse pair,
  using an index instead of a sentinel byte so no byte is added or removed.
  *Implemented and fully green.*
- **Bijective** (`bwt89_bbwt` / `bwt89_ibbwt`) - the reversible variant in
  which the transform is a permutation of the input (same multiset, no index).
  *Implemented, oracle-verified, fully green.*
- **Dynamic** - incremental updates of a maintained transform under text edits
  (insert/delete/substitute) without recomputing from scratch. *Planned.*

All variants are backed by a linear-time core: SA-IS suffix-array
construction (forward) and LF-mapping (inverse), with Duval Lyndon
factorization for the bijective form.

## Non-goals

- No compression or move-to-front / run-length stages; only the transforms.
- No CLI (library only), no hidden caller-visible allocation model beyond an
  out-of-memory status.
- No naive quadratic rotation sorting anywhere in the hot path.

## Data model

Inputs and outputs are caller-owned length-prefixed byte buffers
(`const unsigned char *` + `size_t`). Transforms may allocate O(n) temporary
scratch and report `BWT89_NOMEM`. Embedded NUL bytes are valid data.
