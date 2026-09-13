/* cat89_alloc.c - default allocator and allocation helpers. */

#include <stdlib.h>

#include "cat89_internal.h"
#include <cat89/alloc.h>

static void *default_alloc(void *ctx, size_t size)
{
    void *mem;

    (void)ctx;
    if (size == 0)
    {
        size = 1;
    }
    mem = malloc(size);
    return mem;
}

static void *default_realloc(void *ctx, void *ptr, size_t size)
{
    void *mem;

    (void)ctx;
    if (size == 0)
    {
        size = 1;
    }
    mem = realloc(ptr, size);
    return mem;
}

static void default_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

static const cat89_allocator default_allocator = {
    NULL, default_alloc, default_realloc, default_free};

const cat89_allocator *cat89_allocator_default(void)
{
    return &default_allocator;
}

static const cat89_allocator *resolve(const cat89_allocator *allocator)
{
    const cat89_allocator *actual;

    if (allocator == NULL)
    {
        actual = cat89_allocator_default();
    }
    else
    {
        actual = allocator;
    }
    return actual;
}

void *cat89_alloc(const cat89_allocator *allocator, size_t size)
{
    const cat89_allocator *actual;
    void *mem;

    actual = resolve(allocator);
    if (size == 0)
    {
        size = 1;
    }
    mem = actual->alloc(actual->ctx, size);
    return mem;
}

void *cat89_realloc(const cat89_allocator *allocator, void *ptr, size_t size)
{
    const cat89_allocator *actual;
    void *mem;

    actual = resolve(allocator);
    if (size == 0)
    {
        size = 1;
    }
    mem = actual->realloc(actual->ctx, ptr, size);
    return mem;
}

void cat89_free(const cat89_allocator *allocator, void *ptr)
{
    const cat89_allocator *actual;

    actual = resolve(allocator);
    if (actual->free != NULL)
    {
        actual->free(actual->ctx, ptr);
    }
}
