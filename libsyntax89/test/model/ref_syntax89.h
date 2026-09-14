#ifndef REF_SYNTAX89_H
#define REF_SYNTAX89_H

#include "syntax89.h"

/* Independent array-based reference model for differential testing. It shares
 * no code with the library and validates with its own recursive DFS. */

#define REF_MAX_NODES 32
#define REF_MAX_EDGES 128

struct ref_syntax89
{
    unsigned long node_count;
    unsigned long edge_count;
    unsigned long root; /* 0 means no root */
    syntax89_kind kinds[REF_MAX_NODES];
    syntax89_span spans[REF_MAX_NODES];
    syntax89_role roles[REF_MAX_EDGES];
    unsigned long from[REF_MAX_EDGES];
    unsigned long to[REF_MAX_EDGES];
};

void ref_init(struct ref_syntax89 *r);

/* Return the new id, or 0 when the model is full. */
unsigned long ref_add_node(struct ref_syntax89 *r, syntax89_kind kind,
                           syntax89_span span);

/* Return 0 on success, -1 when full or an endpoint is unknown. */
int ref_add_child(struct ref_syntax89 *r, unsigned long parent,
                  syntax89_role role, unsigned long child);

void ref_set_root(struct ref_syntax89 *r, unsigned long root);

/* Independent oracle: SYNTAX89_OK, SYNTAX89_EGRAPH, or SYNTAX89_ECYCLE. */
syntax89_status ref_validate(const struct ref_syntax89 *r);

/* Compare the model against a library graph observationally. */
int ref_equals_graph(const struct ref_syntax89 *r, const syntax89_graph *g);

#endif
