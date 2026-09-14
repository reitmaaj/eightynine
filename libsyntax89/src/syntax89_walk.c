/* syntax89_walk.c - node-once and occurrence traversal. */

#include "syntax89_internal.h"

static syntax89_status walk_emit(syntax89_visit_fn fn, void *ctx,
                                 syntax89_id id)
{
    syntax89_status st;

    st = fn(ctx, id);
    if (st == SYNTAX89_OK)
    {
        return SYNTAX89_OK;
    }
    if (st == SYNTAX89_END)
    {
        return SYNTAX89_END;
    }
    if (st < SYNTAX89_OK)
    {
        return st;
    }
    return SYNTAX89_EINVAL;
}

static syntax89_status walk_stop(const syntax89_graph *g,
                                 struct syntax89_scratch *s, syntax89_status st)
{
    syntax89__scratch_free(g, s);
    if (st == SYNTAX89_END)
    {
        return SYNTAX89_OK;
    }
    return st;
}

static syntax89_status walk_push(struct syntax89_scratch *s,
                                 struct syntax89_frame *stack,
                                 unsigned long *sp, syntax89_id v, int once,
                                 int pre, syntax89_visit_fn fn, void *ctx)
{
    syntax89_status st;

    if (s->color[v - 1] == SYNTAX89_COLOR_GRAY)
    {
        return SYNTAX89_ECYCLE;
    }
    if (s->color[v - 1] == SYNTAX89_COLOR_BLACK)
    {
        if (once != 0)
        {
            return SYNTAX89_OK;
        }
    }
    s->color[v - 1] = SYNTAX89_COLOR_GRAY;
    *sp += 1;
    stack[*sp].id = v;
    stack[*sp].index = 0;
    if (pre != 0)
    {
        st = walk_emit(fn, ctx, v);
        if (st != SYNTAX89_OK)
        {
            return st;
        }
    }
    return SYNTAX89_OK;
}

static syntax89_status walk_child(struct syntax89_scratch *s,
                                  struct syntax89_frame *stack,
                                  unsigned long *sp,
                                  struct syntax89_frame *frame,
                                  const struct syntax89_node *n, int once,
                                  int pre, syntax89_visit_fn fn, void *ctx)
{
    syntax89_id v;
    syntax89_status st;

    v = n->edges[frame->index].child;
    frame->index += 1;
    st = walk_push(s, stack, sp, v, once, pre, fn, ctx);
    return st;
}

static syntax89_status walk_advance(const syntax89_graph *g,
                                    struct syntax89_scratch *s,
                                    struct syntax89_frame *stack,
                                    unsigned long *sp, int once, int pre,
                                    syntax89_visit_fn fn, void *ctx)
{
    struct syntax89_frame *frame;
    const struct syntax89_node *n;
    syntax89_status st;

    frame = &stack[*sp];
    n = syntax89__node_at(g, frame->id);
    if (frame->index < n->edge_count)
    {
        st = walk_child(s, stack, sp, frame, n, once, pre, fn, ctx);
        return st;
    }
    s->color[frame->id - 1] = SYNTAX89_COLOR_BLACK;
    if (pre == 0)
    {
        st = walk_emit(fn, ctx, frame->id);
        if (st != SYNTAX89_OK)
        {
            return st;
        }
    }
    if (*sp == 0)
    {
        return SYNTAX89_END;
    }
    *sp -= 1;
    return SYNTAX89_OK;
}

static void walk_begin(const syntax89_graph *g, struct syntax89_scratch *s,
                       syntax89_id root)
{
    unsigned long i;

    for (i = 0; i < g->node_count; ++i)
    {
        s->color[i] = SYNTAX89_COLOR_WHITE;
    }
    s->stack[0].id = root;
    s->stack[0].index = 0;
    s->color[root - 1] = SYNTAX89_COLOR_GRAY;
}

static syntax89_status walk_run(const syntax89_graph *g, syntax89_id root,
                                syntax89_visit_fn fn, void *ctx, int once,
                                int pre)
{
    struct syntax89_scratch s;
    syntax89_status st;
    unsigned long sp;

    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (fn == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state == SYNTAX89_STATE_UNINIT)
    {
        return SYNTAX89_ESTATE;
    }
    if (syntax89__node_at(g, root) == NULL)
    {
        return SYNTAX89_ENODE;
    }
    st = syntax89__scratch_new(g, g->node_count, &s);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    walk_begin(g, &s, root);
    if (pre != 0)
    {
        st = walk_emit(fn, ctx, root);
        if (st != SYNTAX89_OK)
        {
            st = walk_stop(g, &s, st);
            return st;
        }
    }
    sp = 0;
    for (;;)
    {
        st = walk_advance(g, &s, s.stack, &sp, once, pre, fn, ctx);
        if (st == SYNTAX89_END)
        {
            break;
        }
        if (st != SYNTAX89_OK)
        {
            st = walk_stop(g, &s, st);
            return st;
        }
    }
    syntax89__scratch_free(g, &s);
    return SYNTAX89_OK;
}

syntax89_status syntax89_walk_nodes_pre(const syntax89_graph *g,
                                        syntax89_id root, syntax89_visit_fn fn,
                                        void *ctx)
{
    syntax89_status st;

    st = walk_run(g, root, fn, ctx, 1, 1);
    return st;
}

syntax89_status syntax89_walk_nodes_post(const syntax89_graph *g,
                                         syntax89_id root, syntax89_visit_fn fn,
                                         void *ctx)
{
    syntax89_status st;

    st = walk_run(g, root, fn, ctx, 1, 0);
    return st;
}

syntax89_status syntax89_walk_edges_pre(const syntax89_graph *g,
                                        syntax89_id root, syntax89_visit_fn fn,
                                        void *ctx)
{
    syntax89_status st;

    st = walk_run(g, root, fn, ctx, 0, 1);
    return st;
}

syntax89_status syntax89_walk_edges_post(const syntax89_graph *g,
                                         syntax89_id root, syntax89_visit_fn fn,
                                         void *ctx)
{
    syntax89_status st;

    st = walk_run(g, root, fn, ctx, 0, 0);
    return st;
}
