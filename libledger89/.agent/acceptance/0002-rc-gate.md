# Release-candidate gate (v2)

Checklist for declaring libledger89 v2 RC1. Every item is a hard blocker.
Evidence for one run is produced by `just release-check`, which writes
`build/release/summary.json` (machine-readable) and `build/release/summary.txt`
(human-readable), plus one log per gate. Record the passing run in
`0003-rc1-report.md`.

Roles are review hats, not separate people: a change authored under one hat is
checked against the others.

| ID | Area | Owner hat | Pass criterion |
| --- | --- | --- | --- |
| RC-01 | `open` corruption semantics | API/spec | Docs distinguish recovery/framing validation from payload verification; durable payload corruption may surface from `read`/`verify` (spec section 8) |
| RC-02 | Corruption regression test | Test | `test_payload_corruption_fails_read` proves open OK, size-only read OK, payload read `ECORRUPT`, `verify` `ECORRUPT`; framing test still fails open |
| RC-03 | Authoritative release gate | Release | `just release-check` runs the complete matrix, exits nonzero on failure, records environment and per-gate results |
| RC-04 | GCC C89 build | Portability | Clean build and suites under the supported GCC invocation (green matrix) |
| RC-05 | Clang C89 build | Portability | Clean build and suites under the supported Clang invocation (green matrix) |
| RC-06 | Portability claim frozen | API/portability | README/AGENTS/header state the exact contract: C89 public API and source, GCC/Clang `unsigned long long` internal extension, POSIX backend |
| RC-07 | ILP32 decision | Portability | `build32` passes when a 32-bit toolchain is present; otherwise the skip is documented and ILP32 is explicitly unsupported for v2.0 |
| RC-08 | Strict-runner width assumption | Test/portability | `strictrun` no longer assumes `unsigned long` is 64-bit |
| RC-09 | Crash/fault suites green | Durability | Deterministic torn-write and fault sweeps pass with only permitted states |
| RC-10 | Real process-kill suite green | Durability | Kill-at-syscall tests recover to allowed old-or-new states |
| RC-11 | Randomized model suite | Test | Reference-model comparison passes for the release seeds plus the long tier |
| RC-12 | Sanitizers / memory checker | Implementation | ASan+UBSan and Valgrind configurations report no unexplained findings |
| RC-13 | Iterator complexity fix | Read path | Sequential iteration advances from a cached physical position; no per-record batch rescan |
| RC-14 | Iterator correctness | Test | Cached iteration is logically identical across batches, segments, pruning, reopen, and empty/end cases |
| RC-15 | Large-batch performance guard | Performance | Iteration scales approximately linearly from 1K to 16K records per batch |
| RC-16 | Manifest generation cleanup | Storage | Writable recovery retains one manifest generation and removes obsolete ones best-effort |
| RC-17 | Manifest cleanup crash safety | Durability | Fault injection around manifest deletion cannot remove the manifest named by `CURRENT` |
| RC-18 | Cold-open scalability characterized | Performance | Benchmarks cover increasing batch/part counts and document an acceptable envelope |
| RC-19 | Large retained-ledger smoke | Storage/test | A ledger beyond unit scale completes append/sync/reopen/read/iterate/verify and continues writing |
| RC-20 | On-disk format compatibility | Format | Golden fixtures reopen byte-identically; the writer stays deterministic |
| RC-21 | Revision / ABA semantics | API/test | `appendv_at` rejects stale callers after truncate/reappend; revision changes only as documented |
| RC-22 | Stable frontier semantics | Durability/API | Reopen exposes exactly the durable frontier; `sync` advances it only after barriers succeed |
| RC-23 | Structural-operation durability | Durability | Successful prune/rotate/truncate are self-durable per the contract |
| RC-24 | Error / poison behavior | Implementation/API | Fatal I/O leaves exactly the documented state; no success after uncertain durability |
| RC-25 | Public-header freeze | API | Header reviewed for C89 syntax, ownership, preconditions, and return semantics |
| RC-26 | Release assumptions documented | Release/API | Filesystem/POSIX assumptions (rename, directory sync, locking) are stated |
| RC-27 | Clean-tree reproducibility | Release | A clean checkout builds and passes `release-check` without local residue |
| RC-28 | Final RC report | Release | `0003-rc1-report.md` records commit, environment, gates, seeds, and known limitations |

## Run

```text
just release-check
```

The gate runs `doctor`, `green`, `lint`, `test`, `strict`, `strict-sanitize`,
`strict-valgrind`, `ratio`, `coverage`, `adapters-raft`, `build32`, `bench`,
`long`, and `strict-long` from a clean build tree, stopping at the first
failure. The long tiers run last; an interrupted run still records every
completed gate.
