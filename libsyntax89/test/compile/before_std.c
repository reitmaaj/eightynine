/* before_std.c - the header is self-contained before any standard header. */

#include "syntax89.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    syntax89_graph g;

    if (syntax89_init(&g, NULL) != SYNTAX89_OK)
    {
        return 1;
    }
    syntax89_destroy(&g);
    return 0;
}
