# 0001 - Stakeholders and stories

## Stakeholders

- **Embedded / no-alloc authors** who need reversible byte transforms without
  an external runtime.
- **Toolchain maintainers** who must keep every translation unit green
  (strict C89 and C23, GCC and Clang, Allman, semantic-clean).
- **Algorithm reviewers** who require proven linear-time construction, not
  naive quadratic rotation sorts.

## Stories

- As a caller with binary data, I want to transform and restore arbitrary
  byte sequences (including embedded NULs) so that no byte is ever mangled.
- As a caller, I want `bwt` to report the original row index so that `ibwt`
  can invert without appending a sentinel, so output length equals input.
- As a caller needing storage-equal transforms, I want `bbwt`/`ibbwt` so the
  transform is a reversible permutation with no index overhead.
- As a caller with an evolving document, I want incremental edits of a
  maintained transform so that each edit is cheaper than a full recompute.
- As an algorithm reviewer, I want every transform verified against an
  independent oracle and never built by quadratic rotation sorting.
