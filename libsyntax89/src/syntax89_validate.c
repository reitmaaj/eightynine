/* syntax89_validate.c - structural validation and freezing. */

#include "syntax89_internal.h"

static void fill_validation(syntax89_validation *result, syntax89_status error,
                            syntax89_id node, syntax89_id related)
{
    result->error = error;
    result->node = node;
    result->related = related;
}

static syntax89_status validate_cycle(syntax89_validation *result,
                                      syntax89_id node, syntax89_id related)
{
    if (result != NULL)
    {
        fill_validation(result, SYNTAX89_ECYCLE, node, related);
    }
    return SYNTAX89_ECYCLE;
}

static syntax89_status validate_unreachable(syntax89_validation *result,
                                            syntax89_id node)
{
    if (result != NULL)
    {
        fill_validation(result, SYNTAX89_EGRAPH, node, SYNTAX89_ID_NONE);
    }
    return SYNTAX89_EGRAPH;
}

static syntax89_status validate_root(const syntax89_graph *g,
                                     syntax89_validation *result)
{
    if (syntax89__node_at(g, g->root) != NULL)
    {
        return SYNTAX89_OK;
    }
    if (result != NULL)
    {
        fill_validation(result, SYNTAX89_EGRAPH, g->root, SYNTAX89_ID_NONE);
    }
    return SYNTAX89_EGRAPH;
}

static void validate_begin(const syntax89_graph *g, struct syntax89_scratch *s)
{
    unsigned long i;

    for (i = 0; i < g->node_count; ++i)
    {
        s->color[i] = SYNTAX89_COLOR_WHITE;
    }
    s->stack[0].id = g->root;
    s->stack[0].index = 0;
    s->color[g->root - 1] = SYNTAX89_COLOR_GRAY;
}

static void validate_push(struct syntax89_scratch *s, unsigned long *sp,
                          syntax89_id v)
{
    s->color[v - 1] = SYNTAX89_COLOR_GRAY;
    *sp += 1;
    s->stack[*sp].id = v;
    s->stack[*sp].index = 0;
}

static syntax89_status validate_visit(struct syntax89_scratch *s,
                                      syntax89_validation *result,
                                      unsigned long *sp, syntax89_id v)
{
    syntax89_status st;

    if (s->color[v - 1] == SYNTAX89_COLOR_GRAY)
    {
        st = validate_cycle(result, v, s->stack[*sp].id);
        return st;
    }
    if (s->color[v - 1] == SYNTAX89_COLOR_WHITE)
    {
        validate_push(s, sp, v);
    }
    return SYNTAX89_OK;
}

static syntax89_status validate_child(struct syntax89_scratch *s,
                                      syntax89_validation *result,
                                      unsigned long *sp,
                                      struct syntax89_frame *frame,
                                      const struct syntax89_node *n)
{
    syntax89_id v;
    syntax89_status st;

    v = n->edges[frame->index].child;
    frame->index += 1;
    st = validate_visit(s, result, sp, v);
    return st;
}

static syntax89_status validate_advance(const syntax89_graph *g,
                                        struct syntax89_scratch *s,
                                        syntax89_validation *result,
                                        unsigned long *sp)
{
    struct syntax89_frame *frame;
    const struct syntax89_node *n;
    syntax89_status st;

    frame = &s->stack[*sp];
    n = syntax89__node_at(g, frame->id);
    if (frame->index < n->edge_count)
    {
        st = validate_child(s, result, sp, frame, n);
        return st;
    }
    s->color[frame->id - 1] = SYNTAX89_COLOR_BLACK;
    if (*sp == 0)
    {
        return SYNTAX89_END;
    }
    *sp -= 1;
    return SYNTAX89_OK;
}

static syntax89_status validate_reachable(const syntax89_graph *g,
                                          struct syntax89_scratch *s,
                                          syntax89_validation *result)
{
    unsigned long i;
    syntax89_status st;

    for (i = 0; i < g->node_count; ++i)
    {
        if (s->color[i] != SYNTAX89_COLOR_BLACK)
        {
            st = validate_unreachable(result, i + 1);
            return st;
        }
    }
    return SYNTAX89_OK;
}

static syntax89_status validate_dfs(const syntax89_graph *g,
                                    struct syntax89_scratch *s,
                                    syntax89_validation *result)
{
    syntax89_status st;
    unsigned long sp;

    validate_begin(g, s);
    sp = 0;
    for (;;)
    {
        st = validate_advance(g, s, result, &sp);
        if (st == SYNTAX89_END)
        {
            break;
        }
        if (st != SYNTAX89_OK)
        {
            return st;
        }
    }
    st = validate_reachable(g, s, result);
    return st;
}

syntax89_status syntax89_validate(const syntax89_graph *g,
                                  syntax89_validation *result)
{
    struct syntax89_scratch s;
    syntax89_status st;

    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state == SYNTAX89_STATE_UNINIT)
    {
        return SYNTAX89_ESTATE;
    }
    if (result != NULL)
    {
        fill_validation(result, SYNTAX89_OK, SYNTAX89_ID_NONE,
                        SYNTAX89_ID_NONE);
    }
    st = validate_root(g, result);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    st = syntax89__scratch_new(g, g->node_count, &s);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    st = validate_dfs(g, &s, result);
    syntax89__scratch_free(g, &s);
    return st;
}

syntax89_status syntax89_freeze(syntax89_graph *g)
{
    syntax89_status st;

    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state == SYNTAX89_STATE_FROZEN)
    {
        return SYNTAX89_OK;
    }
    if (g->state != SYNTAX89_STATE_BUILDING)
    {
        return SYNTAX89_ESTATE;
    }
    st = syntax89_validate(g, NULL);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    g->state = SYNTAX89_STATE_FROZEN;
    return SYNTAX89_OK;
}
