/* reject_write_through_graph.c - this file must NOT compile: a const graph is
 * not writable through the public type. */

#include "syntax89.h"

int main(void)
{
    const syntax89_graph *g;

    g = NULL;
    g->node_count = 1;
    return 0;
}
