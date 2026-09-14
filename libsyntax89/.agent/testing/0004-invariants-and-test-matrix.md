# 0004 — invariant checker and test matrix

## Invariant checker

Two test-only artifacts make invariants pervasive rather than confined to
dedicated tests. Both live in `test/support/` and are never installed.

### `syntax89_test_check(g)`

Called after every successful mutator in unit, model, and generated tests.
It reads the internal representation (white-box) and the public API, and
asserts:

1. the state is BUILDING or FROZEN;
2. `node_count <= node_capacity`; nonempty node storage is non-NULL;
3. per node: `edge_count <= edge_capacity`; nonempty edge storage is
   non-NULL; `begin <= end`;
4. every edge child is in `1..node_count`;
5. `child_at` and the child iterator reproduce each stored edge in order;
6. the sum of per-node edge counts equals `edge_count`;
7. `syntax89_node_count`/`syntax89_edge_count` agree with the raw fields;
8. a FROZEN graph reports `is_frozen`, has a root, and validates `OK`.

### Snapshot harness

`syntax89_test_snapshot_take(g, &s)` copies state, root, counts, every node's
kind and span, and every per-node edge sequence. `syntax89_test_snapshot_
equal(s, g)` compares all of it. `syntax89_test_snapshot_free(s)` releases it.

Every unchanged-on-failure test is:

```c
syntax89_test_snapshot_take(&g, &before);
rc = operation(...);
T_EQ_LONG(rc, expected_error);
T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
syntax89_test_snapshot_free(before);
```

## Test matrix

| Concern | Spec group | Artifact | Recipe |
| --- | --- | --- | --- |
| Lifecycle and state | L01–L13 | `unit/test_lifecycle.c` | `just unit` |
| Node creation | N01–N08 | `unit/test_nodes.c` | `just unit` |
| Stable ids | ID01–ID07 | `unit/test_ids.c` | `just unit` |
| Spans | S01–S08 | `unit/test_spans.c` | `just unit` |
| Root | R01–R09 | `unit/test_root.c` | `just unit` |
| Edge insertion | E01–E10 | `unit/test_edges.c` | `just unit` |
| Child order | O01–O08 | `unit/test_order.c` | `just unit` |
| DAG sharing | D01–D06 | `unit/test_sharing.c` | `just unit` |
| Cycles | C01–C10 | `unit/test_validate_cycles.c` | `just unit` |
| Reachability | U01–U06 | `unit/test_validate_reach.c` | `just unit` |
| Root/indegree | I01–I03 | `unit/test_validate_root.c` | `just unit` |
| Exact counts | counts | `unit/test_counts.c` | `just unit` |
| Queries | Q01–Q17 | `unit/test_queries.c` | `just unit` |
| Validation idempotence | V01–V06 | `unit/test_validate_idempotence.c` | `just unit` |
| Freeze atomicity | freeze | `unit/test_freeze_atomic.c` | `just unit` |
| Failure atomicity | F01–F20 | `unit/test_atomicity.c` | `just unit` |
| Overflow | OV01–OV05 | `unit/test_overflow.c` | `just unit` |
| Iterators | iter | `unit/test_iter.c` | `just unit` |
| Traversal | T01–T09 | `unit/test_walk.c` | `just unit` |
| Determinism | P01–P06 | `unit/test_determinism.c` | `just unit` |
| NULL preconditions | contract | `unit/test_preconditions.c` | `just unit` |
| Parser interchange | fixture | `unit/test_parser_fixture.c` | `just unit` |
| Differential model | random | `model/test_model.c` | `just model` |
| Exhaustive oracle N<=4 | exhaustive | `model/test_oracle.c` | `just model` |
| Generated mutations | generated | `generated/test_mutations.c` | `just generated` |
| Allocation failure | A01–A08 | `fault/test_allocfail.c` | `just fault` |
| Deep chain | S01 | `stress/test_deep.c` | `just stress`, `just deep-test` |
| Wide node | S02 | `stress/test_wide.c` | `just stress` |
| Shared DAG | S03–S04 | `stress/test_shared.c` | `just stress` |
| Header hygiene | H01–H07 | `test/compile/*`, `test/cpp/*` | `just compile-check`, `just cpp-check` |

## Release criterion

`libsyntax89` v1 is credible only after passing:

1. all deterministic contract tests;
2. exhaustive small-graph validation (`N <= 4`, all directed graphs including
   self-loops) against an independent oracle;
3. allocator failure at every allocation point;
4. unchanged-on-failure snapshots;
5. a 100,000-depth non-recursive graph;
6. a large shared DAG without exponential behavior;
7. gcc and clang strict C89 (plus C23);
8. ASan, UBSan, and Valgrind clean diagnostic builds;
9. green profile, 100% line/branch coverage, symbol audit, API coverage, and
   the gate self-checks.

The two most important areas are graph validation and failure atomicity: they
justify `libsyntax89` as a library rather than a handful of AST structs.
