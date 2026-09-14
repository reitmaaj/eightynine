/* test_order.c - deterministic child order O01..O08. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static syntax89_id node(syntax89_graph *g, unsigned long tag)
{
    return syntax89_fixture_node(g, K_NAME, tag, tag + 1);
}

static void test_zero_and_one_child(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_role role;
    syntax89_id child;

    T_OK(syntax89_init(&g, NULL));
    a = node(&g, 0);
    b = node(&g, 1);
    T_EQ_UL(syntax89_child_count(&g, a), 0);
    T_EQ_LONG(syntax89_child_at(&g, a, 0, &role, &child), SYNTAX89_EINVAL);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_EQ_UL(syntax89_child_count(&g, a), 1);
    T_OK(syntax89_child_at(&g, a, 0, &role, &child));
    T_EQ_UL(role, R_LEFT);
    T_EQ_UL(child, b);
    syntax89_destroy(&g);
}

static void test_mixed_role_order_is_insertion_order(void)
{
    syntax89_graph g;
    syntax89_id call;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;
    syntax89_id fn;
    syntax89_id d;
    syntax89_role role;
    syntax89_id child;

    T_OK(syntax89_init(&g, NULL));
    call = node(&g, 0);
    a = node(&g, 1);
    b = node(&g, 2);
    c = node(&g, 3);
    fn = node(&g, 4);
    d = node(&g, 5);
    T_OK(syntax89_add_child(&g, call, R_ARG, a));
    T_OK(syntax89_add_child(&g, call, R_ARG, b));
    T_OK(syntax89_add_child(&g, call, R_ARG, c));
    T_OK(syntax89_add_child(&g, call, R_CALLEE, fn));
    T_OK(syntax89_add_child(&g, call, R_ARG, d));
    T_OK(syntax89_child_at(&g, call, 0, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, a);
    T_OK(syntax89_child_at(&g, call, 1, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, b);
    T_OK(syntax89_child_at(&g, call, 2, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, c);
    T_OK(syntax89_child_at(&g, call, 3, &role, &child));
    T_EQ_UL(role, R_CALLEE);
    T_EQ_UL(child, fn);
    T_OK(syntax89_child_at(&g, call, 4, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, d);
    T_OK(syntax89_child_at_role(&g, call, R_ARG, 0, &child));
    T_EQ_UL(child, a);
    T_OK(syntax89_child_at_role(&g, call, R_ARG, 1, &child));
    T_EQ_UL(child, b);
    T_OK(syntax89_child_at_role(&g, call, R_ARG, 2, &child));
    T_EQ_UL(child, c);
    T_OK(syntax89_child_at_role(&g, call, R_ARG, 3, &child));
    T_EQ_UL(child, d);
    syntax89_destroy(&g);
}

static void test_order_survives_growth(void)
{
    syntax89_graph g;
    syntax89_id parent;
    syntax89_id first;
    syntax89_id last;
    syntax89_id id;
    syntax89_role role;
    syntax89_id child;
    unsigned long i;

    T_OK(syntax89_init(&g, NULL));
    parent = node(&g, 0);
    first = node(&g, 1);
    last = node(&g, 2);
    T_OK(syntax89_add_child(&g, parent, R_ITEM, first));
    for (i = 0; i < 2000; ++i)
    {
        id = node(&g, 3);
        T_OK(syntax89_add_child(&g, parent, R_ITEM, id));
    }
    T_OK(syntax89_add_child(&g, parent, R_ITEM, last));
    T_EQ_UL(syntax89_child_count(&g, parent), 2002);
    T_OK(syntax89_child_at(&g, parent, 0, &role, &child));
    T_EQ_UL(child, first);
    T_OK(syntax89_child_at(&g, parent, 2001, &role, &child));
    T_EQ_UL(child, last);
    syntax89_destroy(&g);
}

static void test_order_survives_freeze(void)
{
    syntax89_graph g;
    syntax89_id root;
    syntax89_role role;
    syntax89_id child;

    root = syntax89_fixture_build(&g, FX_F2, NULL);
    T_OK(syntax89_child_at(&g, root, 1, &role, &child));
    T_EQ_UL(child, 3);
    T_OK(syntax89_freeze(&g));
    T_OK(syntax89_child_at(&g, root, 1, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, 3);
    T_OK(syntax89_child_at(&g, root, 3, &role, &child));
    T_EQ_UL(child, 5);
    syntax89_destroy(&g);
}

int main(void)
{
    test_zero_and_one_child();
    test_mixed_role_order_is_insertion_order();
    test_order_survives_growth();
    test_order_survives_freeze();
    return syntax89_test_report("test_order");
}
