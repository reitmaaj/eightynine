/* cat89_fault_alloc.c - allocation-failure injection fixture (F07). */

#include <stdlib.h>

#include "fault_alloc.h"

static void *fa_alloc(void *ctx, size_t size)
{
    struct cat89_fault_alloc *f = ctx;
    f->count = f->count + 1;
    f->allocs = f->allocs + 1;
    if (f->fail_at != 0 && f->count == f->fail_at)
    {
        return NULL;
    }
    return malloc(size);
}

static void *fa_realloc(void *ctx, void *ptr, size_t size)
{
    struct cat89_fault_alloc *f = ctx;
    f->count = f->count + 1;
    f->allocs = f->allocs + 1;
    if (f->fail_at != 0 && f->count == f->fail_at)
    {
        return NULL;
    }
    return realloc(ptr, size);
}

static void fa_free(void *ctx, void *ptr)
{
    struct cat89_fault_alloc *f = ctx;
    f->frees = f->frees + 1;
    free(ptr);
}

void cat89_fault_alloc_init(struct cat89_fault_alloc *f)
{
    f->count = 0;
    f->fail_at = 0;
    f->allocs = 0;
    f->frees = 0;
}

void cat89_fault_alloc_use(struct cat89_fault_alloc *f, cat89_allocator *out)
{
    out->ctx = f;
    out->alloc = fa_alloc;
    out->realloc = fa_realloc;
    out->free = fa_free;
}

void cat89_fault_alloc_fail_at(struct cat89_fault_alloc *f, unsigned long n)
{
    f->count = 0;
    f->fail_at = n;
}

void cat89_fault_alloc_disable(struct cat89_fault_alloc *f)
{
    f->fail_at = 0;
}
