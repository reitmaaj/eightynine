#ifndef SYNTAX89_FIXTURES_H
#define SYNTAX89_FIXTURES_H

#include "syntax89.h"

/* Canonical fixture vocabulary, deliberately unrelated to any language. */
enum
{
    K_INT = 1,
    K_NAME,
    K_ADD,
    K_CALL,
    K_IF,
    K_BLOCK
};

enum
{
    R_LEFT = 1,
    R_RIGHT,
    R_CALLEE,
    R_ARG,
    R_COND,
    R_THEN,
    R_ELSE,
    R_ITEM
};

enum
{
    FX_F0 = 0, /* single node */
    FX_F1,     /* simple tree */
    FX_F2,     /* ordered repeated role */
    FX_F3,     /* DAG sharing */
    FX_F4,     /* deeper graph */
    FX_F5,     /* unreachable node */
    FX_F6,     /* direct cycle */
    FX_F7      /* indirect cycle */
};

/* Append a node with the fixture vocabulary and assert success. */
syntax89_id syntax89_fixture_node(syntax89_graph *g, syntax89_kind kind,
                                  unsigned long begin, unsigned long end);

/* Append an edge and assert success. */
void syntax89_fixture_edge(syntax89_graph *g, syntax89_id parent,
                           syntax89_role role, syntax89_id child);

/* Build one canonical fixture; return its root id (SYNTAX89_ID_NONE for F5
 * only when the fixture has no root, which it does not). */
syntax89_id syntax89_fixture_build(syntax89_graph *g, int fixture,
                                   const syntax89_allocator *alloc);

#endif
