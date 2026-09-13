/* dl89_mem.c - default allocator behind the internal memory seam. */

#include <stdlib.h>

#include "dl89_internal.h"

void *dl89_mem_alloc(size_t size)
{
    void *mem;

    if (size == 0)
    {
        size = 1;
    }
    mem = malloc(size);
    return mem;
}

void *dl89_mem_realloc(void *ptr, size_t size)
{
    void *mem;

    if (size == 0)
    {
        size = 1;
    }
    mem = realloc(ptr, size);
    return mem;
}

void dl89_mem_free(void *ptr)
{
    free(ptr);
}
