/* hm_ctx.c - context lifecycle, allocator, and allocation arena. */
#include <hm.h>

#include "hm_internal.h"

#include <stdlib.h>
#include <string.h>

static void *hm_i_default_alloc(void *userdata, size_t size)
{
    void *p;
    (void)userdata;
    p = malloc(size);
    return p;
}

static void hm_i_default_free(void *userdata, void *ptr)
{
    (void)userdata;
    free(ptr);
}

static hm_alloc_fn hm_i_pick_alloc_fn(const hm_allocator *allocator)
{
    if (allocator == NULL)
    {
        return hm_i_default_alloc;
    }
    return allocator->alloc;
}

static hm_free_fn hm_i_pick_free_fn(const hm_allocator *allocator)
{
    if (allocator == NULL)
    {
        return hm_i_default_free;
    }
    return allocator->free;
}

static void *hm_i_pick_userdata(const hm_allocator *allocator)
{
    if (allocator == NULL)
    {
        return NULL;
    }
    return allocator->userdata;
}

hm_ctx *hm_ctx_new(const hm_allocator *allocator)
{
    hm_allocator a;
    hm_ctx *ctx;
    void *raw;
    a.alloc = hm_i_pick_alloc_fn(allocator);
    a.free = hm_i_pick_free_fn(allocator);
    a.userdata = hm_i_pick_userdata(allocator);
    raw = a.alloc(a.userdata, sizeof(hm_ctx));
    if (raw == NULL)
    {
        return NULL;
    }
    ctx = raw;
    ctx->alloc = a;
    ctx->blocks = NULL;
    ctx->next_var_id = 0;
    ctx->origin = NULL;
    return ctx;
}

static void hm_i_free_chain(hm_ctx *ctx, struct hm_blk *blk)
{
    struct hm_blk *next;
    if (blk == NULL)
    {
        return;
    }
    next = blk->next;
    ctx->alloc.free(ctx->alloc.userdata, blk);
    hm_i_free_chain(ctx, next);
}

void hm_ctx_destroy(hm_ctx *ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    hm_i_free_chain(ctx, ctx->blocks);
    ctx->alloc.free(ctx->alloc.userdata, ctx);
}

void hm_ctx_set_origin(hm_ctx *ctx, void *origin)
{
    ctx->origin = origin;
}

void *hm_i_alloc(hm_ctx *ctx, size_t size)
{
    struct hm_blk *blk;
    void *raw;
    void *payload;
    size_t total;
    total = sizeof(struct hm_blk) + size;
    raw = ctx->alloc.alloc(ctx->alloc.userdata, total);
    if (raw == NULL)
    {
        return NULL;
    }
    blk = raw;
    payload = blk + 1;
    blk->next = ctx->blocks;
    ctx->blocks = blk;
    memset(payload, 0, size);
    return payload;
}

char *hm_i_strdup(hm_ctx *ctx, const char *src)
{
    size_t len;
    size_t total;
    char *dst;
    void *raw;
    len = strlen(src);
    total = len + 1;
    raw = hm_i_alloc(ctx, total);
    if (raw == NULL)
    {
        return NULL;
    }
    dst = raw;
    memcpy(dst, src, total);
    return dst;
}
