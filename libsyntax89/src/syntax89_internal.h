#ifndef SYNTAX89_INTERNAL_H
#define SYNTAX89_INTERNAL_H

#include "syntax89.h"

/* Lifecycle states; syntax89_graph.state takes exactly these values. */
#define SYNTAX89_STATE_UNINIT 0
#define SYNTAX89_STATE_BUILDING 1
#define SYNTAX89_STATE_FROZEN 2

/* Initial capacities; growth doubles. */
#define SYNTAX89_NODE_CAP_MIN 8UL
#define SYNTAX89_EDGE_CAP_MIN 4UL

/* Depth-first colors. */
#define SYNTAX89_COLOR_WHITE 0
#define SYNTAX89_COLOR_GRAY 1
#define SYNTAX89_COLOR_BLACK 2

/* One edge of a parent's insertion-ordered edge vector. */
struct syntax89_edge
{
    syntax89_role role;
    syntax89_id child;
};

/* One node and its insertion-ordered structural edges. */
struct syntax89_node
{
    syntax89_kind kind;
    syntax89_span span;
    struct syntax89_edge *edges;
    unsigned long edge_count;
    unsigned long edge_capacity;
};

/* One explicit depth-first frame. */
struct syntax89_frame
{
    syntax89_id id;
    unsigned long index;
};

/* Depth-first scratch: one color per node and one frame per node. */
struct syntax89_scratch
{
    unsigned char *color;
    struct syntax89_frame *stack;
};

/* 1 when span.begin <= span.end. Pure. */
int syntax89__span_ok(syntax89_span span);

/* Checked unsigned arithmetic; SYNTAX89_OK or SYNTAX89_EOVERFLOW. Pure. */
int syntax89__size_add(unsigned long a, unsigned long b, unsigned long *out);
int syntax89__size_mul(unsigned long a, unsigned long b, unsigned long *out);

/* Borrowed node for id in 1..node_count, else NULL. Pure. */
const struct syntax89_node *syntax89__node_at(const syntax89_graph *g,
                                              syntax89_id id);
struct syntax89_node *syntax89__node_mut(syntax89_graph *g, syntax89_id id);

/* Allocator access. Effectful. */
void *syntax89__alloc(const syntax89_graph *g, unsigned long bytes);
void *syntax89__realloc(const syntax89_graph *g, void *ptr,
                        unsigned long bytes);
void syntax89__free(const syntax89_graph *g, void *ptr);

/* Grow the node store to hold need nodes; unchanged on failure. */
syntax89_status syntax89__grow_nodes(syntax89_graph *g, unsigned long need);

/* Grow parent's edge vector to hold need edges; unchanged on failure. */
syntax89_status syntax89__grow_edges(syntax89_graph *g,
                                     struct syntax89_node *parent,
                                     unsigned long need);

/* Allocate color and frame scratch for n nodes; unchanged on failure. */
syntax89_status syntax89__scratch_new(const syntax89_graph *g, unsigned long n,
                                      struct syntax89_scratch *s);

/* Release scratch storage; NULL members are safe. */
void syntax89__scratch_free(const syntax89_graph *g,
                            struct syntax89_scratch *s);

/* Test hooks; not part of the public API. */
void syntax89__set_limit_nodes(syntax89_graph *g, unsigned long limit);
void syntax89__set_limit_edges(syntax89_graph *g, unsigned long limit);

#endif
