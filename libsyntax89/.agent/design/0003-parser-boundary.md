# 0003 — parser boundary: AST, CST, and the producer API

## Boundary

```text
source text
   |
 liblex89 / lexer          (external)
   |
 tokens
   |
 parser internals          (external)
   |
   v
libsyntax89 graph           (this library)
   |
 resolver / typer / evaluator / compiler / formatter
```

The parser emits **abstract syntax**, not its raw parse tree. Parsing
`1 + 2 * 3` may internally build a grammar-shaped concrete tree, but it emits:

```text
ADD
 ├─ lhs ──> INT(1)
 └─ rhs ──> MUL
             ├─ lhs ──> INT(2)
             └─ rhs ──> INT(3)
```

Grammar machinery (`expression`, `additive-expression`, parentheses tokens)
is omitted unless the language actually needs it. `libsyntax89` does not
dictate how parsing works: recursive descent, LR, PEG, hand-written, or
deserialization can all produce the same graph.

The parser need not be the only producer. Macro expansion, programmatic AST
construction, desugaring, source-to-source transforms, cached syntax
deserialization, tests, and REPL fragments can all build graphs.

## AST versus CST

| | AST (what `libsyntax89` stores) | CST / lossless |
|---|---|---|
| Node kinds | language-semantic (`CALL`, `IF`, `ADD`) | grammar productions and tokens |
| Trivia | absent; comments and whitespace live outside | preserved |
| Delimiters | absent | exact punctuation |
| Spellings and literals | external side tables keyed by node id | stored or token-indexed |
| Source fidelity | byte spans into caller-owned buffers | full round trip |
| Consumers | resolver, typer, evaluator, compiler | formatters, refactorers, round-trip tooling |

Decision: `libsyntax89` v1 is the abstract interchange representation. A
lossless layer is a separate future library (for example `libcst89`) and must
not contaminate this core.

## Producer API sketch (documentation only)

A parser keeps its own lexer, token stream, and parser stack. It calls the
public construction API and keeps token text in side tables keyed by node id:

```c
struct my_parser
{
    my_lexer lex;
    struct my_token current;
    const char *text;      /* caller-owned source buffer */
    const char **names;    /* external side table: node id -> spelling */
    syntax89_graph *out;
};

static syntax89_id parse_atom(struct my_parser *p);
static syntax89_id parse_expr(struct my_parser *p);
```

On a parse error the parser simply calls `syntax89_destroy` on the partial
graph; the library has no rollback and needs none.

## Interchange fixture

`test/unit/test_parser_fixture.c` builds `f(1, x + 2)` twice:

1. through a parser-shaped emitter over a token array, with token text kept in
   an external side table keyed by node id;
2. through direct construction.

The two graphs are compared by kind, span, and ordered roles, independent of
node ids because the producers allocate nodes in different orders. Both must
freeze. This proves the interchange role without linking a parser into the
library.
