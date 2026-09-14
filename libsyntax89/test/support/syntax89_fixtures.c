/* syntax89_fixtures.c - canonical fixture graphs F0..F7. */

#include "syntax89_fixtures.h"
#include "syntax89_test.h"

syntax89_id syntax89_fixture_node(syntax89_graph *g, syntax89_kind kind,
                                  unsigned long begin, unsigned long end)
{
    syntax89_id id;
    syntax89_span span;

    span.source = 1;
    span.begin = begin;
    span.end = end;
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_add_node(g, kind, span, &id));
    return id;
}

void syntax89_fixture_edge(syntax89_graph *g, syntax89_id parent,
                           syntax89_role role, syntax89_id child)
{
    T_OK(syntax89_add_child(g, parent, role, child));
}

static syntax89_id fixture_f0(syntax89_graph *g)
{
    syntax89_id a;

    a = syntax89_fixture_node(g, K_INT, 0, 0);
    T_OK(syntax89_set_root(g, a));
    return a;
}

static syntax89_id fixture_f1(syntax89_graph *g)
{
    syntax89_id add;
    syntax89_id left;
    syntax89_id right;

    add = syntax89_fixture_node(g, K_ADD, 0, 3);
    left = syntax89_fixture_node(g, K_INT, 0, 1);
    right = syntax89_fixture_node(g, K_INT, 2, 3);
    syntax89_fixture_edge(g, add, R_LEFT, left);
    syntax89_fixture_edge(g, add, R_RIGHT, right);
    T_OK(syntax89_set_root(g, add));
    return add;
}

static syntax89_id fixture_f2(syntax89_graph *g)
{
    syntax89_id call;
    syntax89_id callee;
    syntax89_id a1;
    syntax89_id a2;
    syntax89_id a3;

    call = syntax89_fixture_node(g, K_CALL, 0, 8);
    callee = syntax89_fixture_node(g, K_NAME, 0, 1);
    a1 = syntax89_fixture_node(g, K_INT, 2, 3);
    a2 = syntax89_fixture_node(g, K_NAME, 5, 6);
    a3 = syntax89_fixture_node(g, K_INT, 8, 9);
    syntax89_fixture_edge(g, call, R_CALLEE, callee);
    syntax89_fixture_edge(g, call, R_ARG, a1);
    syntax89_fixture_edge(g, call, R_ARG, a2);
    syntax89_fixture_edge(g, call, R_ARG, a3);
    T_OK(syntax89_set_root(g, call));
    return call;
}

static syntax89_id fixture_f3(syntax89_graph *g)
{
    syntax89_id add;
    syntax89_id shared;

    add = syntax89_fixture_node(g, K_ADD, 0, 3);
    shared = syntax89_fixture_node(g, K_NAME, 0, 1);
    syntax89_fixture_edge(g, add, R_LEFT, shared);
    syntax89_fixture_edge(g, add, R_RIGHT, shared);
    T_OK(syntax89_set_root(g, add));
    return add;
}

static syntax89_id fixture_f4(syntax89_graph *g)
{
    syntax89_id branch;
    syntax89_id cond;
    syntax89_id then_block;
    syntax89_id else_block;
    syntax89_id c1;
    syntax89_id c2;
    syntax89_id e1;

    branch = syntax89_fixture_node(g, K_IF, 0, 20);
    cond = syntax89_fixture_node(g, K_NAME, 3, 4);
    then_block = syntax89_fixture_node(g, K_BLOCK, 6, 14);
    else_block = syntax89_fixture_node(g, K_BLOCK, 15, 20);
    c1 = syntax89_fixture_node(g, K_CALL, 7, 10);
    c2 = syntax89_fixture_node(g, K_CALL, 11, 14);
    e1 = syntax89_fixture_node(g, K_INT, 16, 17);
    syntax89_fixture_edge(g, branch, R_COND, cond);
    syntax89_fixture_edge(g, branch, R_THEN, then_block);
    syntax89_fixture_edge(g, branch, R_ELSE, else_block);
    syntax89_fixture_edge(g, then_block, R_ITEM, c1);
    syntax89_fixture_edge(g, then_block, R_ITEM, c2);
    syntax89_fixture_edge(g, else_block, R_ITEM, e1);
    T_OK(syntax89_set_root(g, branch));
    return branch;
}

static syntax89_id fixture_f5(syntax89_graph *g)
{
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;

    a = syntax89_fixture_node(g, K_NAME, 0, 1);
    b = syntax89_fixture_node(g, K_NAME, 2, 3);
    c = syntax89_fixture_node(g, K_NAME, 4, 5);
    syntax89_fixture_edge(g, a, R_LEFT, b);
    T_OK(syntax89_set_root(g, a));
    (void)c;
    return a;
}

static syntax89_id fixture_f6(syntax89_graph *g)
{
    syntax89_id a;

    a = syntax89_fixture_node(g, K_NAME, 0, 1);
    syntax89_fixture_edge(g, a, R_LEFT, a);
    T_OK(syntax89_set_root(g, a));
    return a;
}

static syntax89_id fixture_f7(syntax89_graph *g)
{
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;

    a = syntax89_fixture_node(g, K_NAME, 0, 1);
    b = syntax89_fixture_node(g, K_NAME, 2, 3);
    c = syntax89_fixture_node(g, K_NAME, 4, 5);
    syntax89_fixture_edge(g, a, R_LEFT, b);
    syntax89_fixture_edge(g, b, R_LEFT, c);
    syntax89_fixture_edge(g, c, R_LEFT, a);
    T_OK(syntax89_set_root(g, a));
    return a;
}

syntax89_id syntax89_fixture_build(syntax89_graph *g, int fixture,
                                   const syntax89_allocator *alloc)
{
    syntax89_id root;

    T_OK(syntax89_init(g, alloc));
    root = SYNTAX89_ID_NONE;
    if (fixture == FX_F0)
    {
        root = fixture_f0(g);
    }
    else if (fixture == FX_F1)
    {
        root = fixture_f1(g);
    }
    else if (fixture == FX_F2)
    {
        root = fixture_f2(g);
    }
    else if (fixture == FX_F3)
    {
        root = fixture_f3(g);
    }
    else if (fixture == FX_F4)
    {
        root = fixture_f4(g);
    }
    else if (fixture == FX_F5)
    {
        root = fixture_f5(g);
    }
    else if (fixture == FX_F6)
    {
        root = fixture_f6(g);
    }
    else if (fixture == FX_F7)
    {
        root = fixture_f7(g);
    }
    else
    {
        T_ASSERT(0);
    }
    return root;
}
