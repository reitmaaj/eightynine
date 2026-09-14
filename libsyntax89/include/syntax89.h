#ifndef SYNTAX89_H
#define SYNTAX89_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* libsyntax89 - typed ordered syntax DAGs for strict ISO C89.
     *
     * A graph stores nodes (stable id, client kind, optional source span) and
     * ordered structural edges (parent, client role, child) and nothing else.
     * It never interprets kinds or roles, owns source text, resolves names,
     * types, evaluates, or prescribes a grammar.
     *
     *   UNINITIALIZED --init--> BUILDING --freeze--> FROZEN --destroy--> end
     *
     * BUILDING admits add_node, add_child, and set_root. FROZEN rejects them
     * with SYNTAX89_ESTATE and admits queries, iteration, traversal, and
     * validate. There is no deletion and no in-place surgery: a transformation
     * builds a new graph from a frozen source graph.
     *
     * Ownership: the graph owns nodes and edges through the allocator copied
     * at init. Nodes belong to exactly one graph; edges join nodes of the same
     * graph only. The library never owns source text. A NULL allocator selects
     * the C library malloc/realloc/free.
     *
     * Failure: every operation completes fully or changes nothing observable.
     * IDs are nonzero, stable for the graph lifetime, and never reused.
     *
     * Every field of syntax89_graph is private implementation state; clients
     * must not read or write it. Only syntax89_init produces a valid graph.
     */

    typedef unsigned long syntax89_id;
    typedef unsigned long syntax89_kind;
    typedef unsigned long syntax89_role;
    typedef unsigned long syntax89_source_id;
    typedef unsigned long syntax89_offset;

    /* The only reserved ID; it never names a node. Kind and role values are
     * opaque: 0 is an ordinary kind and an ordinary role. */
#define SYNTAX89_ID_NONE ((syntax89_id)0)

    /* Half-open byte range [begin, end) in external source. The library never
     * reads source text. source 0, begin 0, end 0 denotes an unknown span. */
    typedef struct syntax89_span
    {
        syntax89_source_id source;
        syntax89_offset begin;
        syntax89_offset end;
    } syntax89_span;

    typedef struct syntax89_node_info
    {
        syntax89_kind kind;
        syntax89_span span;
    } syntax89_node_info;

    typedef struct syntax89_allocator
    {
        void *ctx;
        void *(*alloc)(void *ctx, size_t size);
        void *(*realloc)(void *ctx, void *ptr, size_t size);
        void (*free)(void *ctx, void *ptr);
    } syntax89_allocator;

    /* SYNTAX89_END is positive: ordinary iterator or traversal exhaustion.
     * Every error is negative. */
    typedef enum syntax89_status
    {
        SYNTAX89_OK = 0,
        SYNTAX89_END = 1,
        SYNTAX89_EINVAL = -1,
        SYNTAX89_ENOMEM = -2,
        SYNTAX89_ENODE = -3,
        SYNTAX89_ESTATE = -4,
        SYNTAX89_ECYCLE = -5,
        SYNTAX89_EGRAPH = -6,
        SYNTAX89_EOVERFLOW = -7
    } syntax89_status;

    /* Validation detail. error repeats the returned status; node names the
     * offending node when one is identifiable, else SYNTAX89_ID_NONE; related
     * names a second node (for a cycle, the edge source), else NONE. */
    typedef struct syntax89_validation
    {
        syntax89_status error;
        syntax89_id node;
        syntax89_id related;
    } syntax89_validation;

    /* Graph handle. */
    typedef struct syntax89_graph
    {
        syntax89_allocator alloc;
        struct syntax89_node *nodes;
        unsigned long node_count;
        unsigned long node_capacity;
        unsigned long edge_count;
        syntax89_id root;
        int state;
        unsigned long test_limit_nodes; /* test hook; production leaves 0 */
        unsigned long test_limit_edges; /* test hook; production leaves 0 */
    } syntax89_graph;

    /* Child iterator. begin validates the parent; next yields each edge in
     * insertion order and returns SYNTAX89_END at exhaustion. Adding an edge
     * to the same parent invalidates outstanding iterators for that parent. */
    typedef struct syntax89_child_iter
    {
        const syntax89_graph *graph;
        syntax89_id parent;
        unsigned long index;
    } syntax89_child_iter;

    /* Traversal callback: SYNTAX89_OK continues, SYNTAX89_END stops
     * successfully, any negative status aborts and is returned verbatim. Any
     * other value aborts with SYNTAX89_EINVAL. */
    typedef syntax89_status (*syntax89_visit_fn)(void *ctx, syntax89_id node);

    /* ---- lifecycle ---- */

    /* Initialize g as an empty BUILDING graph and copy alloc by value. g must
     * not already be an active graph. alloc == NULL selects the C library
     * malloc/realloc/free. Returns SYNTAX89_OK, or SYNTAX89_EINVAL for a NULL
     * g. Allocates nothing. */
    syntax89_status syntax89_init(syntax89_graph *g,
                                  const syntax89_allocator *alloc);

    /* Release every node and edge and zero g. g == NULL is a no-op. A zeroed
     * graph may be destroyed again harmlessly. */
    void syntax89_destroy(syntax89_graph *g);

    /* ---- construction (BUILDING only) ---- */

    /* Append one node. Returns SYNTAX89_OK and a fresh nonzero id in *out, or
     * SYNTAX89_EINVAL (g or out NULL, span begin > end), SYNTAX89_ESTATE
     * (not BUILDING), SYNTAX89_ENOMEM, or SYNTAX89_EOVERFLOW. On failure the
     * graph and *out are unchanged. */
    syntax89_status syntax89_add_node(syntax89_graph *g, syntax89_kind kind,
                                      syntax89_span span, syntax89_id *out);

    /* Append edge parent--role-->child after the parent's existing edges.
     * Self-edges and cycles are permitted while BUILDING; freeze rejects
     * them. Returns SYNTAX89_OK, SYNTAX89_EINVAL (g NULL), SYNTAX89_ENODE
     * (unknown parent or child), SYNTAX89_ESTATE, SYNTAX89_ENOMEM, or
     * SYNTAX89_EOVERFLOW. On failure the graph is unchanged. */
    syntax89_status syntax89_add_child(syntax89_graph *g, syntax89_id parent,
                                       syntax89_role role, syntax89_id child);

    /* Set the distinguished root; SYNTAX89_ID_NONE clears it. Returns
     * SYNTAX89_OK, SYNTAX89_EINVAL (g NULL), SYNTAX89_ENODE (unknown root),
     * or SYNTAX89_ESTATE. */
    syntax89_status syntax89_set_root(syntax89_graph *g, syntax89_id root);

    /* ---- validation and freezing ---- */

    /* Prove: a root exists and names a node, the root has no incoming edge,
     * the structural edges are acyclic, and every node is reachable from the
     * root. result may be NULL. Errors in precedence order: SYNTAX89_EINVAL,
     * SYNTAX89_ESTATE, SYNTAX89_EGRAPH (root missing or unknown),
     * SYNTAX89_ECYCLE, SYNTAX89_EGRAPH (unreachable node), SYNTAX89_ENOMEM.
     * Validation never changes observable graph state. */
    syntax89_status syntax89_validate(const syntax89_graph *g,
                                      syntax89_validation *result);

    /* BUILDING: validate, then mark FROZEN. FROZEN: idempotent SYNTAX89_OK.
     * Any failure leaves the graph BUILDING and unchanged. */
    syntax89_status syntax89_freeze(syntax89_graph *g);

    /* ---- state and counts ---- */

    /* 1 exactly when g is initialized and FROZEN; 0 otherwise (including
     * NULL). */
    int syntax89_is_frozen(const syntax89_graph *g);

    /* The distinguished root, or SYNTAX89_ID_NONE when unset or g is NULL. */
    syntax89_id syntax89_root(const syntax89_graph *g);

    unsigned long syntax89_node_count(const syntax89_graph *g);
    unsigned long syntax89_edge_count(const syntax89_graph *g);

    /* ---- node inspection ---- */

    /* Fill *out from the node named by id. Returns SYNTAX89_OK,
     * SYNTAX89_EINVAL (g or out NULL), SYNTAX89_ESTATE, or SYNTAX89_ENODE
     * (ID_NONE or unknown id; *out unchanged). */
    syntax89_status syntax89_node(const syntax89_graph *g, syntax89_id id,
                                  syntax89_node_info *out);

    /* ---- child inspection ---- */

    /* Number of structural edges of parent; 0 for NULL, uninitialized, or
     * unknown parent. */
    unsigned long syntax89_child_count(const syntax89_graph *g,
                                       syntax89_id parent);

    /* Return edge index of parent in insertion order. Returns SYNTAX89_OK,
     * SYNTAX89_EINVAL (g, role, or child NULL; index out of range),
     * SYNTAX89_ESTATE, or SYNTAX89_ENODE (unknown parent). */
    syntax89_status syntax89_child_at(const syntax89_graph *g,
                                      syntax89_id parent, unsigned long index,
                                      syntax89_role *role, syntax89_id *child);

    /* Number of parent edges carrying role. */
    unsigned long syntax89_child_count_role(const syntax89_graph *g,
                                            syntax89_id parent,
                                            syntax89_role role);

    /* Return the index-th edge of parent carrying role, preserving original
     * relative order. Same status contract as syntax89_child_at. */
    syntax89_status syntax89_child_at_role(const syntax89_graph *g,
                                           syntax89_id parent,
                                           syntax89_role role,
                                           unsigned long index,
                                           syntax89_id *child);

    /* ---- child iteration ---- */

    /* Prepare an iterator over parent's edges. Returns SYNTAX89_OK,
     * SYNTAX89_EINVAL (g or it NULL), SYNTAX89_ESTATE, or SYNTAX89_ENODE. */
    syntax89_status syntax89_children_begin(const syntax89_graph *g,
                                            syntax89_id parent,
                                            syntax89_child_iter *it);

    /* Yield the next edge and advance. Returns SYNTAX89_OK with *role and
     * *child written, SYNTAX89_END at exhaustion, or SYNTAX89_EINVAL (it,
     * role, or child NULL; never begun). */
    syntax89_status syntax89_children_next(syntax89_child_iter *it,
                                           syntax89_role *role,
                                           syntax89_id *child);

    /* ---- traversal ---- */

    /* Node-once walks visit each node at most once in deterministic
     * depth-first order with children in insertion order. walk_nodes_pre
     * visits on discovery; walk_nodes_post visits after all children. A back
     * edge to the active path returns SYNTAX89_ECYCLE. */
    syntax89_status syntax89_walk_nodes_pre(const syntax89_graph *g,
                                            syntax89_id root,
                                            syntax89_visit_fn fn, void *ctx);
    syntax89_status syntax89_walk_nodes_post(const syntax89_graph *g,
                                             syntax89_id root,
                                             syntax89_visit_fn fn, void *ctx);

    /* Occurrence walks visit each structural occurrence: a shared node is
     * revisited once per incoming edge. Cost follows occurrences, not nodes.
     * Same callback, error, and cycle contract as the node-once walks. */
    syntax89_status syntax89_walk_edges_pre(const syntax89_graph *g,
                                            syntax89_id root,
                                            syntax89_visit_fn fn, void *ctx);
    syntax89_status syntax89_walk_edges_post(const syntax89_graph *g,
                                             syntax89_id root,
                                             syntax89_visit_fn fn, void *ctx);

#ifdef __cplusplus
}
#endif

#endif
