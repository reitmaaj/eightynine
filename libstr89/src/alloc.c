/* alloc.c - allocator resolution and checked size arithmetic for libstr89. */

#include <stdlib.h>

#include "str89_internal.h"

void *str89__malloc(const str89_alloc *alloc, size_t size)
{
    void *p;

    if (alloc == NULL)
    {
        p = malloc(size);
        return p;
    }
    p = alloc->malloc_fn(alloc->ctx, size);
    return p;
}

void *str89__realloc(const str89_alloc *alloc, void *ptr, size_t size)
{
    void *p;

    if (alloc == NULL)
    {
        p = realloc(ptr, size);
        return p;
    }
    p = alloc->realloc_fn(alloc->ctx, ptr, size);
    return p;
}

void str89__free(const str89_alloc *alloc, void *ptr)
{
    if (alloc == NULL)
    {
        free(ptr);
        return;
    }
    alloc->free_fn(alloc->ctx, ptr);
}

int str89__add(size_t a, size_t b, size_t *out)
{
    size_t s;

    s = a + b;
    if (s < a)
    {
        return STR89_ERANGE;
    }
    *out = s;
    return STR89_OK;
}

static size_t double_cap(size_t n)
{
    return n * 2;
}

static int too_big_to_double(size_t n)
{
    return (n > STR89_NPOS / 2);
}

size_t str89__grow_cap(size_t cap, size_t need)
{
    size_t n;
    int over;

    n = cap;
    if (n < 16)
    {
        n = 16;
    }
    while (n < need)
    {
        over = too_big_to_double(n);
        if (over != 0)
        {
            return 0;
        }
        n = double_cap(n);
    }
    return n;
}

int str89__overlaps(const unsigned char *base, size_t len,
                    const unsigned char *src)
{
    if (base == NULL)
    {
        return 0;
    }
    if (src < base)
    {
        return 0;
    }
    if (src >= base + len)
    {
        return 0;
    }
    return 1;
}

int str89__data_ok(const unsigned char *data, size_t len)
{
    if (data == NULL)
    {
        if (len != 0)
        {
            return 0;
        }
    }
    return 1;
}
