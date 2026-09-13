# BDD scenarios: core API (L1)

Scenarios in this file drive `test/smoke.c`, `test/api/test_open.c`,
`test/api/test_append.c`, `test/api/test_read_iter.c`,
`test/api/test_observer.c`, and `test/api/test_misuse.c`. IDs cross-reference
`.agent/acceptance/0001-core-api.md`.

## Open, create, close

- SCENARIO Open nonexistent path: GIVEN a path that does not exist WHEN
  opened THEN an empty ledger is created with `first_index == 1` and
  `last_index == 0` (O01).
- SCENARIO Open empty directory: GIVEN an existing empty directory WHEN
  opened THEN an empty ledger (O02).
- SCENARIO Reopen: GIVEN a ledger with records WHEN closed and reopened THEN
  the exact contents return (O03).
- SCENARIO Close empty/populated: GIVEN any handle WHEN closed THEN success
  and no logical modification (O04, O05).
- SCENARIO Double open: GIVEN one handle on a path WHEN a second open runs
  THEN `LEDGER89_ERR_BUSY` (O07).
- SCENARIO Null arguments: GIVEN `out == NULL` or `config == NULL` or
  `config->path == NULL` WHEN opened THEN `LEDGER89_ERR_ARG` and `*out` is
  `NULL` (O08).

## Append

- SCENARIO First record: GIVEN an empty ledger WHEN index 1 is appended THEN
  `last_index == 1` (A01).
- SCENARIO Sequential: GIVEN records 1..N WHEN appended one by one THEN the
  exact sequence is visible (A02).
- SCENARIO Duplicate index: GIVEN `last_index == 3` WHEN index 3 is appended
  THEN `LEDGER89_ERR_SEQUENCE` and nothing changes (A03).
- SCENARIO Skipped index: GIVEN `last_index == 3` WHEN index 5 is appended
  THEN `LEDGER89_ERR_SEQUENCE` and nothing changes (A04).
- SCENARIO Empty payload: GIVEN size 0 WHEN appended THEN success and read
  returns size 0 (A06).
- SCENARIO Binary payload: GIVEN payload bytes containing NUL and 0xFF WHEN
  appended THEN read returns the exact bytes (A07).
- SCENARIO Tags: GIVEN tag 0 or `ULONG_MAX` WHEN appended THEN read returns
  the same tag (A08, A09).
- SCENARIO Oversized payload: GIVEN `size > 16 MiB` WHEN appended THEN
  `LEDGER89_ERR_ARG` and nothing changes (A11).

## Batch append

- SCENARIO Batch of one: GIVEN one record WHEN appended as a batch THEN
  identical to single append (B01).
- SCENARIO Batch of 100: GIVEN 100 contiguous records WHEN appended THEN all
  are visible in order (B02).
- SCENARIO Zero count: GIVEN `count == 0` WHEN appended THEN
  `LEDGER89_ERR_ARG` (B03).
- SCENARIO Bad first index: GIVEN a batch whose first index is wrong WHEN
  appended THEN `LEDGER89_ERR_SEQUENCE` and no record of the batch is
  visible (B04).
- SCENARIO Gap inside batch: GIVEN a batch with a gap WHEN appended THEN
  `LEDGER89_ERR_SEQUENCE` and nothing changes (B05).
- SCENARIO Invalid record midway: GIVEN a batch whose second record has an
  oversized payload WHEN appended THEN `LEDGER89_ERR_ARG` and nothing
  changes (B07).

## Read and iteration

- SCENARIO Read first/last/middle: GIVEN records WHEN read by index THEN the
  exact record returns (RD01, RD02, RD03).
- SCENARIO Read missing: GIVEN an index outside the range WHEN read THEN
  `LEDGER89_ERR_NOTFOUND` (RD04, RD05).
- SCENARIO Iterate all: GIVEN records WHEN iterated THEN ascending exact
  order, ending with `LEDGER89_END` (IT03).
- SCENARIO Iterate subrange: GIVEN `[first,last]` WHEN iterated THEN exactly
  that range (IT04).
- SCENARIO Iterate empty ledger: GIVEN no records WHEN iterated THEN the
  first `iter_next` returns `LEDGER89_END` (IT01).
- SCENARIO Wildcards: GIVEN `first == 0` and/or `last == 0` WHEN iterated
  THEN the range is from the first and/or to the last record (IT10, IT11).
- SCENARIO Reversed range: GIVEN `first > last`, both nonzero WHEN iterated
  THEN `LEDGER89_ERR_ARG` (IT12).
- SCENARIO Independent iterators: GIVEN two iterators WHEN advanced
  alternately THEN each yields its own sequence (IT14).

## Observer

- SCENARIO Observer sees appends: GIVEN an observer WHEN a batch is appended
  THEN the observer receives each record once in ascending order after
  visibility (OB01).
- SCENARIO Observer cleared: GIVEN an observer WHEN set to `NULL` THEN later
  appends do not call it (OB02).
- SCENARIO Failed append: GIVEN a rejected batch WHEN append fails THEN the
  observer is not called (OB03).
