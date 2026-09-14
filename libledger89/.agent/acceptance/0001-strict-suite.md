# Acceptance criteria: strict suite (v2)

Traceability: each criterion maps to scenarios in
`.agent/testing/0001-strict-suite.md` and to executable tables under
`test/strict/`, `test/unit/`, `test/crash/`, and `test/fault/`.

## Must exhibit

1. `just strict-fast` completes in under two minutes and proves the
   end-to-end path: open, append, sync, reopen, read, verify. (ST01, ST02)
2. Every public function has a table covering all documented results and the
   complete NULL/empty/boundary argument space. (ST04-ST09)
3. `first <= stable_end <= end` and revision monotonicity hold after every
   operation in every randomized sequence. (ST10)
4. Reopening a recovered ledger is idempotent. (ST11)
5. An independent Python codec decodes and encodes every v2 structure with
   byte-exact agreement. (ST12, ST13, ST14)
6. Every single-byte mutation, truncation, splice, and crafted header is
   classified into a documented outcome with no crash and no silent repair.
   (ST15-ST18)
7. Every injected I/O failure produces the documented error class, sticky
   poisoning where applicable, and a legal recovered state. (ST19, ST21)
8. EINTR is retried transparently by every primitive. (ST20)
   (This criterion falsified a real defect; the fix is recorded in
   `.agent/design/0007-strict-test-architecture.md` section 11.)
9. Every crash point of every structural operation resolves to exactly one
   permitted state, and repeated recovery is idempotent. (ST22-ST24)
10. Randomized differential runs match the reference model exactly, including
    after crashes. (ST25, ST26)
11. Failing random sequences are shrunk to a minimal replayable script.
    (ST27)
12. Hostile paths, sparse files, and lock tampering are rejected within the
    documented budgets. (ST28-ST30)
13. Every case is bounded by temp dirs, resource limits, and timeouts, and
    leaves no process or file behind. (ST31)
14. Latency p50/p95/p99 and N-vs-2N scaling stay under the documented
    ceilings and bounds. (ST32-ST34)
15. `just ratio` reports at least 10 test NLOC per source NLOC. (ST35)
16. `just coverage` reports at least 95% lines and 90% branches for `src/`.
    (ST36)

## Must reject / fail safely

1. No case may crash the Python driver, the runner, the shim, or the host:
   a signal death or timeout is a suite failure. (ST31)
2. No corrupted stable history may be repaired, skipped, or silently
   truncated; ECORRUPT/EFORMAT must be returned. (ST15-ST18)
3. No crash may expose a partial batch, a gap, a duplicate, a mutated sealed
   part, or a record absent from both permitted states. (ST22-ST24)
4. No faulted handle may report success for a mutation with an uncertain
   durability outcome. (ST19)
5. No hostile input may cause unbounded allocation, out-of-bounds reads, an
   infinite loop, or a whole-file scan of a huge sparse file. (ST17, ST29)
6. No unchecked on-disk length, offset, or count may be trusted. (ST17)
7. No case may write outside its temp directory or exceed its resource
   budget. (ST31)
8. The ratio gate must fail when the ratio is below 10:1, and the coverage
   gate must fail below its thresholds. (ST35, ST36)
