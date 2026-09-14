# 0003 — failure and scale scenarios

SCENARIO allocation failure is atomic
  GIVEN a counting allocator that fails the Nth allocation
  WHEN any allocating operation runs
  THEN the result is `SYNTAX89_ENOMEM`
  AND the observable graph snapshot is unchanged
  AND every successful allocation is freed by destroy

SCENARIO integer overflow is rejected
  GIVEN a test limit that forces the next growth to overflow
  WHEN `syntax89_add_node` or `syntax89_add_child` runs
  THEN the result is `SYNTAX89_EOVERFLOW`
  AND the graph is unchanged

SCENARIO deep chain is not recursive
  GIVEN a chain of 100,000 nodes rooted at the first
  WHEN the graph is validated, frozen, walked, and destroyed
  THEN every operation completes without exhausting the C stack

SCENARIO wide node
  GIVEN one parent with 100,000 children
  WHEN the graph is frozen and queried
  THEN insertion order is preserved exactly

SCENARIO layered shared DAG
  GIVEN a layered diamond whose path count grows exponentially while the node
        count stays linear
  WHEN `syntax89_validate` and `syntax89_freeze` run
  THEN both complete in time proportional to nodes plus edges

SCENARIO deterministic construction
  GIVEN the same insertion sequence applied twice
  WHEN ids, child order, traversal order, and validation are compared
  THEN every observation is identical
