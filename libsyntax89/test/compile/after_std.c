/* after_std.c - the header compiles after standard headers. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "syntax89.h"

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
