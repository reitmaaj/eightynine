# BDD scenarios: allocation failure and leak checks

Drives `test/failalloc/test_alloc_failure.c` (linked with `--wrap`) and the
`just valgrind` recipe.

- SCENARIO Create allocation failure: GIVEN the n-th allocation fails WHEN
  `raft89_create` runs THEN it returns `RAFT89_ERR_NOMEM`, `*out` is `NULL`,
  and every allocation is released.
- SCENARIO Propose allocation failure: GIVEN a leader and the n-th
  allocation fails WHEN `raft89_propose` runs THEN it returns
  `RAFT89_ERR_NOMEM`, the log is unchanged, and the node stays usable.
- SCENARIO Receive allocation failure: GIVEN a follower and the n-th
  allocation fails WHEN an AppendEntries with entries arrives THEN it returns
  `RAFT89_ERR_NOMEM` with no action and no durable change.
- SCENARIO No leaks: GIVEN any injected failure WHEN the node is destroyed
  THEN the live allocation count is zero.
- SCENARIO Valgrind: GIVEN the fast suite binaries WHEN run under
  `just valgrind` THEN valgrind reports no errors and no leaks.
