/* syntax89_fault_alloc.c - allocation-failure injection fixture. */

#include <stdlib.h>

#include "syntax89_fault_alloc.h"

static int fa_should_fail(struct syntax89_fault_alloc *f)
{
    f->calls += 1;
    if (f->fail_at == 0)
    {
        return 0;
    }
    if (f->calls == f->fail_at)
    {
        return 1;
    }
    return 0;
}

static void *fa_alloc(void *ctx, size_t size)
{
    struct syntax89_fault_alloc *f;
    void *p;

    f = ctx;
    if (fa_should_fail(f) != 0)
    {
        return NULL;
    }
    p = malloc(size);
    if (p != NULL)
    {
        f->allocs += 1;
        f->live += 1;
    }
    return p;
}

static void *fa_realloc(void *ctx, void *ptr, size_t size)
{
    struct syntax89_fault_alloc *f;
    void *p;

    f = ctx;
    if (fa_should_fail(f) != 0)
    {
        return NULL;
    }
    p = realloc(ptr, size);
    if (p != NULL)
    {
        f->allocs += 1;
        if (ptr == NULL)
        {
            f->live += 1;
        }
    }
    return p;
}

static void fa_free(void *ctx, void *ptr)
{
    struct syntax89_fault_alloc *f;

    f = ctx;
    if (ptr != NULL)
    {
        f->frees += 1;
        f->live -= 1;
    }
    free(ptr);
}

void syntax89_fault_alloc_init(struct syntax89_fault_alloc *f)
{
    f->calls = 0;
    f->fail_at = 0;
    f->allocs = 0;
    f->frees = 0;
    f->live = 0;
}

void syntax89_fault_alloc_use(struct syntax89_fault_alloc *f,
                              syntax89_allocator *out)
{
    out->ctx = f;
    out->alloc = fa_alloc;
    out->realloc = fa_realloc;
    out->free = fa_free;
}

void syntax89_fault_alloc_fail_at(struct syntax89_fault_alloc *f,
                                  unsigned long n)
{
    f->calls = 0;
    f->fail_at = n;
}

void syntax89_fault_alloc_disable(struct syntax89_fault_alloc *f)
{
    f->fail_at = 0;
}
