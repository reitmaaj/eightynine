# 0001 — libsyntax89 concept

## Identity

`libsyntax89` is the frontend's syntax interchange representation: a small,
language-agnostic C89 library that stores typed syntax nodes, typed ordered
structural edges, and source spans, and provides generic construction,
validation, freezing, traversal, and iteration.

It is not a parser and not a compiler framework. A recursive-descent parser,
LR parser, PEG parser, macro expander, deserializer, or test can all produce
the same graph.

## Layering

```text
libu89 / liblex89
        |
      parser                 (external)
        |
        v
   libsyntax89              (this library)
        |
  +-----+------+
  |     |      |
  v     v      v
resolver typer formatter    (external, side tables)
```

`libsyntax89` depends only on the C89 standard library. It does not depend on
`libu89`, `libdiag89`, or any sibling library.

## Core abstraction

```text
node  = (id, kind, span)
edge  = (parent, role, child, order)
graph = (nodes, edges, root)
```

Everything else must justify why those four concepts cannot express it.

## What belongs here

- stable, nonzero node ids that never change and are never reused;
- client-defined kind and role values the library never interprets;
- half-open byte spans into caller-owned source;
- insertion-ordered structural edges and role-filtered views;
- one distinguished root per graph, which may be any node;
- DAG sharing (one node, several parents);
- a BUILDING/FROZEN lifecycle with functional rewriting;
- structural validation: root, acyclicity, reachability, ids, spans;
- allocation-free queries and child iteration;
- iterative (non-recursive) validation and traversal.

## What does not belong here

Lexing, parsing, grammar definitions, parser generation, token storage,
source-text ownership, symbol tables, scopes, name resolution, type systems,
evaluation, bytecode, generic IR, graph databases, generic annotations,
schemas, serialization, pretty printing, formatting, diffing, incremental
parsing, deletion, and in-place rewrite.

References and annotations, declarative schemas, and a lossless CST layer are
separate future libraries layered on top.

## Design principles

- **No hidden semantics**: kind and role values are opaque.
- **No hidden state**: one caller-owned graph handle, one allocator copy.
- **Append-only construction**: BUILDING is cheap; FROZEN is predictable.
- **Atomic failure**: every operation completes or changes nothing.
- **Deterministic order**: ids ascend; children follow insertion order.
- **Bounded stack**: no core operation recurses with syntax depth.
