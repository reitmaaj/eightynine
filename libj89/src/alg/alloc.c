/* alloc.c - allocator and result-carrier runtime helpers for j89_alg. */
#include <stdlib.h>

#include "alg_internal.h"

static void *def_alloc(void *ctx, size_t size)
{
    void *p;
    (void)ctx;
    p = malloc(size);
    return p;
}

static void *def_realloc(void *ctx, void *p, size_t size)
{
    void *q;
    (void)ctx;
    q = realloc(p, size);
    return q;
}

static void def_free(void *ctx, void *p)
{
    (void)ctx;
    free(p);
}

void j89a_alloc_init(j89a_alloc *al)
{
    al->alloc = def_alloc;
    al->realloc = def_realloc;
    al->free = def_free;
    al->ctx = NULL;
    al->failed = 0;
}

void *j89a_malloc(j89a_alloc *al, size_t size)
{
    void *p;
    p = al->alloc(al->ctx, size);
    if (p == NULL)
    {
        al->failed = 1;
    }
    return p;
}

void j89a_nfree(void (*freer)(void *, void *), void *fctx, void *p)
{
    if (p != NULL)
    {
        freer(fctx, p);
    }
}

j89a_status j89a_rcopy(const j89a_rtype *rt, void *dst, const void *src)
{
    j89a_status st;
    if (rt->copy != NULL)
    {
        st = rt->copy(rt->ctx, dst, src);
        return st;
    }
    memcpy(dst, src, rt->size);
    return J89A_OK;
}

void j89a_rdrop(const j89a_rtype *rt, void *val)
{
    if (rt->drop != NULL)
    {
        rt->drop(rt->ctx, val);
    }
}
