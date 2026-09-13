# libcat89 testing scenarios (BDD) - bugfix pass

## 0010-fallible-out-null.md

Every fallible public constructor in the library sets its `*out` to NULL before
validating arguments, so a failure (including `CAT89_INVALID`) always leaves
`*out == NULL`. A subset of the derived-structure binary ops must honour the
same contract.

- SCENARIO compose nulls out on mismatch: GIVEN two valid isos (or split
  monos/epis) whose categories are distinct instances WHEN the binary
  `cat89_*_compose` runs for them THEN it returns `CAT89_INVALID` and leaves
  `*out == NULL` (the caller must not be handed a stale handle it may treat as
  owned).
- SCENARIO compose/invert/adapter nulls out on null handle: GIVEN a NULL handle
  argument to `cat89_iso_compose`, `cat89_split_mono_compose`,
  `cat89_split_epi_compose`, `cat89_iso_invert`, `cat89_iso_as_split_mono`, or
  `cat89_iso_as_split_epi` WHEN it runs THEN it returns `CAT89_INVALID` and
  leaves `*out == NULL`.
