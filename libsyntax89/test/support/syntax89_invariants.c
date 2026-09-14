/* syntax89_invariants.c - white-box invariant checker and snapshot harness. */

#include <stdlib.h>

#include "syntax89_internal.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

struct syntax89_test_snapshot
{
    int state;
    syntax89_id root;
    unsigned long node_count;
    unsigned long edge_count;
    syntax89_kind *kinds;
    syntax89_span *spans;
    unsigned long *edge_counts;
    syntax89_role *roles;
    syntax89_id *children;
    unsigned long total_edges;
};

static void check_node_edges(const syntax89_graph *g,
                             const struct syntax89_node *n)
{
    unsigned long j;

    T_ASSERT(n->edge_count <= n->edge_capacity);
    if (n->edge_capacity > 0)
    {
        T_ASSERT(n->edges != NULL);
    }
    for (j = 0; j < n->edge_count; ++j)
    {
        T_ASSERT(n->edges[j].child >= 1);
        T_ASSERT(n->edges[j].child <= g->node_count);
    }
}

static void check_public_children(const syntax89_graph *g, syntax89_id id,
                                  const struct syntax89_node *n)
{
    syntax89_child_iter it;
    syntax89_role role;
    syntax89_id child;
    syntax89_status st;
    unsigned long j;

    T_EQ_UL(syntax89_child_count(g, id), n->edge_count);
    for (j = 0; j < n->edge_count; ++j)
    {
        role = 0;
        child = SYNTAX89_ID_NONE;
        T_OK(syntax89_child_at(g, id, j, &role, &child));
        T_EQ_UL(role, n->edges[j].role);
        T_EQ_UL(child, n->edges[j].child);
    }
    T_OK(syntax89_children_begin(g, id, &it));
    for (j = 0; j < n->edge_count; ++j)
    {
        st = syntax89_children_next(&it, &role, &child);
        T_EQ_LONG(st, SYNTAX89_OK);
        T_EQ_UL(role, n->edges[j].role);
        T_EQ_UL(child, n->edges[j].child);
    }
    st = syntax89_children_next(&it, &role, &child);
    T_EQ_LONG(st, SYNTAX89_END);
}

void syntax89_test_check(const syntax89_graph *g)
{
    const struct syntax89_node *n;
    unsigned long i;
    unsigned long total;

    T_ASSERT(g != NULL);
    T_ASSERT(g->state == SYNTAX89_STATE_BUILDING ||
             g->state == SYNTAX89_STATE_FROZEN);
    T_ASSERT(g->node_count <= g->node_capacity);
    if (g->node_capacity > 0)
    {
        T_ASSERT(g->nodes != NULL);
    }
    total = 0;
    for (i = 0; i < g->node_count; ++i)
    {
        n = &g->nodes[i];
        T_ASSERT(n->span.begin <= n->span.end);
        check_node_edges(g, n);
        check_public_children(g, i + 1, n);
        total += n->edge_count;
    }
    T_EQ_UL(total, g->edge_count);
    T_EQ_UL(syntax89_node_count(g), g->node_count);
    T_EQ_UL(syntax89_edge_count(g), g->edge_count);
    if (g->state == SYNTAX89_STATE_FROZEN)
    {
        T_ASSERT(syntax89_is_frozen(g) != 0);
        T_ASSERT(g->root != SYNTAX89_ID_NONE);
        T_OK(syntax89_validate(g, NULL));
    }
}

int syntax89_test_snapshot_take(const syntax89_graph *g,
                                struct syntax89_test_snapshot **out)
{
    struct syntax89_test_snapshot *s;
    const struct syntax89_node *n;
    unsigned long i;
    unsigned long j;
    unsigned long k;

    s = malloc(sizeof(*s));
    if (s == NULL)
    {
        return -1;
    }
    s->state = g->state;
    s->root = g->root;
    s->node_count = g->node_count;
    s->edge_count = g->edge_count;
    s->total_edges = g->edge_count;
    s->kinds = NULL;
    s->spans = NULL;
    s->edge_counts = NULL;
    s->roles = NULL;
    s->children = NULL;
    if (g->node_count > 0)
    {
        s->kinds = malloc(g->node_count * sizeof(*s->kinds));
        s->spans = malloc(g->node_count * sizeof(*s->spans));
        s->edge_counts = malloc(g->node_count * sizeof(*s->edge_counts));
    }
    if (g->edge_count > 0)
    {
        s->roles = malloc(g->edge_count * sizeof(*s->roles));
        s->children = malloc(g->edge_count * sizeof(*s->children));
    }
    if (s->kinds == NULL && g->node_count > 0)
    {
        syntax89_test_snapshot_free(s);
        return -1;
    }
    if (s->spans == NULL && g->node_count > 0)
    {
        syntax89_test_snapshot_free(s);
        return -1;
    }
    if (s->edge_counts == NULL && g->node_count > 0)
    {
        syntax89_test_snapshot_free(s);
        return -1;
    }
    if (s->roles == NULL && g->edge_count > 0)
    {
        syntax89_test_snapshot_free(s);
        return -1;
    }
    if (s->children == NULL && g->edge_count > 0)
    {
        syntax89_test_snapshot_free(s);
        return -1;
    }
    k = 0;
    for (i = 0; i < g->node_count; ++i)
    {
        n = &g->nodes[i];
        s->kinds[i] = n->kind;
        s->spans[i] = n->span;
        s->edge_counts[i] = n->edge_count;
        for (j = 0; j < n->edge_count; ++j)
        {
            s->roles[k] = n->edges[j].role;
            s->children[k] = n->edges[j].child;
            k += 1;
        }
    }
    *out = s;
    return 0;
}

int syntax89_test_snapshot_equal(const struct syntax89_test_snapshot *s,
                                 const syntax89_graph *g)
{
    const struct syntax89_node *n;
    unsigned long i;
    unsigned long j;
    unsigned long k;

    if (s->state != g->state)
    {
        return 0;
    }
    if (s->root != g->root)
    {
        return 0;
    }
    if (s->node_count != g->node_count)
    {
        return 0;
    }
    if (s->edge_count != g->edge_count)
    {
        return 0;
    }
    k = 0;
    for (i = 0; i < g->node_count; ++i)
    {
        n = &g->nodes[i];
        if (s->kinds[i] != n->kind)
        {
            return 0;
        }
        if (s->spans[i].source != n->span.source)
        {
            return 0;
        }
        if (s->spans[i].begin != n->span.begin)
        {
            return 0;
        }
        if (s->spans[i].end != n->span.end)
        {
            return 0;
        }
        if (s->edge_counts[i] != n->edge_count)
        {
            return 0;
        }
        for (j = 0; j < n->edge_count; ++j)
        {
            if (s->roles[k] != n->edges[j].role)
            {
                return 0;
            }
            if (s->children[k] != n->edges[j].child)
            {
                return 0;
            }
            k += 1;
        }
    }
    return 1;
}

void syntax89_test_snapshot_free(struct syntax89_test_snapshot *s)
{
    if (s == NULL)
    {
        return;
    }
    free(s->kinds);
    free(s->spans);
    free(s->edge_counts);
    free(s->roles);
    free(s->children);
    free(s);
}
