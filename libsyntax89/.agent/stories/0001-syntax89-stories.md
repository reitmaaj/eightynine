# 0001 — libsyntax89 stories

## Parser author

AS a parser author
I WANT a small graph API with append-only node and edge construction
SO THAT I can emit abstract syntax without adopting a compiler framework.

AS a parser author
I WANT stable node ids and byte spans
SO THAT I can keep token text, literal values, and diagnostics in side tables
keyed by node id.

## Compiler engineer

AS a compiler engineer
I WANT to freeze a graph and validate root, acyclicity, and reachability
SO THAT later passes can rely on a predictable DAG.

AS a compiler engineer
I WANT traversal that never recurses with syntax depth
SO THAT generated or machine-written syntax of arbitrary depth is safe.

AS a compiler engineer
I WANT to build a replacement graph from a frozen source graph
SO THAT transformations are functional and never invalidate earlier passes.

## Tooling author

AS a formatter or refactoring author
I WANT ordered, role-filtered child access and a child iterator
SO THAT I can walk and re-emit syntax without depending on internal storage.

## Library maintainer

AS a library maintainer
I WANT every operation to be unchanged-on-failure and leak-free
SO THAT callers can recover from allocation failure without rollback logic.

AS a library maintainer
I WANT a strict C89, green-compliant, dependency-free implementation
SO THAT `libsyntax89` fits the lib*89 family and any C89 toolchain.
