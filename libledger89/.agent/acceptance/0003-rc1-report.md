# RC1 release report (v2)

- **Result: PASS** — all 14 release gates passed.
- **Date**: 2026-09-15 (host local).
- **Base commit**: `44dc2f030b3a1a0a3895685f42edc1765c00189c` with the RC
  worktree applied (dirty; not committed).
- **Worktree fingerprint**: `6df7c889c8cbf1ea6458ae3f55eff8ddfea73493808aa1858d174d3dc6649f22`
  (sha256 over `git diff` plus the untracked RC files).
- **Host**: Linux x86_64, btrfs; GCC 15.3.1, Clang 21.1.8, Python 3.14.7.
- **Command**: `just release-check` from a clean build tree; total 7530s.
- **Machine-readable evidence**: `build/release/summary.json`; per-gate logs
  under `build/release/`.

## Gates

| Gate | Result | Seconds | Log |
| --- | --- | ---: | --- |
| doctor | pass | 0 | `doctor.log` |
| green | pass | 3 | `green.log` |
| lint | pass | 2 | `lint.log` |
| test | pass | 53 | `test.log` |
| strict | pass | 288 | `strict.log` |
| strict-sanitize | pass | 25 | `strict-sanitize.log` |
| strict-valgrind | pass | 1741 | `strict-valgrind.log` |
| ratio | pass | 1 | `ratio.log` |
| coverage | pass | 324 | `coverage.log` |
| adapters-raft | pass | 1 | `adapters-raft.log` |
| test32 | pass (skipped) | 0 | `test32.log` |
| bench | pass | 46 | `bench.log` |
| long | pass | 43 | `long.log` |
| strict-long | pass | 5003 | `strict-long.log` |

## Test totals

- strict fast: 2605 cases, 0 failures (`build/strict/report-fast.json`).
- strict full: 15095 cases, 0 failures (`build/strict/report-full.json`).
- strict long: 49344 cases, 0 failures (`build/strict/report-long.json`).
- ratio: 12.32:1 test:source NLOC (gate 10:1).
- coverage: 95.4% lines, 90.2% branches (gates 95%/90%).
- sanitizers: ASan+UBSan runner subsets pass.
- valgrind: memcheck runner subsets pass.

## Benchmarks (this run)

```text
append:  49984 records in 0.370s (~135240 rec/s)
sync:    0.033s
read:    49984 records in 0.573s (~87264 rec/s)
iterate: 49984 records in 0.031s (~1614507 rec/s)

iteration: 1000/4000/16000 records -> 590/578/600 ns per record
cold open: 100000 batches -> 0.210s ro / 0.218s rw
           10000 batches, 1000 parts -> 0.042s ro / 0.087s rw
```

## Checklist status

| ID | Status | Note |
| --- | --- | --- |
| RC-01 | pass | `open`/`read`/`verify` semantics frozen in header and spec section 8 |
| RC-02 | pass | framing vs payload corruption tests in `test/crash/test_recovery.c` |
| RC-03 | pass | `just release-check` runs the full matrix with `build/release/summary.json` |
| RC-04 | pass | GCC C89/C23 in the green matrix |
| RC-05 | pass | Clang C89/C23 in the green matrix |
| RC-06 | pass | README/AGENTS/header/spec state the exact platform contract |
| RC-07 | pass with note | `test32` skipped: no 32-bit toolchain on this host |
| RC-08 | pass | `strictrun` no longer assumes 64-bit `unsigned long` |
| RC-09 | pass | crash/fault suites green |
| RC-10 | pass | real process-kill suite green |
| RC-11 | pass | model suites: full 12 seeds and long 48 seeds, 0 failures |
| RC-12 | pass | ASan+UBSan and Valgrind subsets green |
| RC-13 | pass | iterator retains a physical cursor |
| RC-14 | pass | cursor regression tests across batches, prune, reopen, ETOOSMALL |
| RC-15 | pass | iteration flat from 1K to 16K records per batch |
| RC-16 | pass | writable recovery retains one manifest generation |
| RC-17 | pass | unlink-failure test cannot remove the CURRENT manifest |
| RC-18 | pass | cold-open benchmarks up to 100k batches / 1000 parts |
| RC-19 | pass | `test_stress_retained.c` lifecycle at 20k records / 16 parts |
| RC-20 | pass | golden and format suites green |
| RC-21 | pass | revision/ABA tests green |
| RC-22 | pass | stable-frontier contract tests green |
| RC-23 | pass | structural durability fault sweeps green |
| RC-24 | pass | poison/error behavior fault sweeps green |
| RC-25 | pass | public header reviewed and frozen |
| RC-26 | pass | POSIX/rename/directory-sync assumptions documented |
| RC-27 | pass with note | gate runs from a clean build tree; worktree uncommitted |
| RC-28 | pass | this report |

## Known limitations

- ILP32 is unverified on this host because no multilib toolchain is
  installed; `just test32` reports a skip.
- Evidence is attached to an uncommitted worktree on top of `44dc2f0`;
  commit the patchset and rerun `just release-check` (or confirm the same
  worktree fingerprint) before tagging RC1.
- Cold-open cost grows with retained physical structure; the measured
  envelope is documented in `0006-benchmark-baseline.md` and is not a
  guarantee beyond it.
- Sealed-part random reads reopen the part per access; deferred to v2.1.
