/* adt_ctx.c - context lifecycle, allocator, registries, and failure helper. */
#include <adt.h>

#include "adt_internal.h"

#include <stdlib.h>
#include <string.h>

static void *adt_i_default_alloc(void *userdata, size_t size)
{
    void *p;
    (void)userdata;
    p = malloc(size);
    return p;
}

static void adt_i_default_free(void *userdata, void *ptr)
{
    (void)userdata;
    free(ptr);
}

static adt_alloc_fn adt_i_pick_alloc_fn(const adt_allocator *allocator)
{
    if (allocator == NULL)
    {
        return adt_i_default_alloc;
    }
    return allocator->alloc;
}

static adt_free_fn adt_i_pick_free_fn(const adt_allocator *allocator)
{
    if (allocator == NULL)
    {
        return adt_i_default_free;
    }
    return allocator->free;
}

static void *adt_i_pick_userdata(const adt_allocator *allocator)
{
    if (allocator == NULL)
    {
        return NULL;
    }
    return allocator->userdata;
}

static void adt_i_init_allocator(adt_allocator *out,
                                 const adt_allocator *allocator)
{
    out->alloc = adt_i_pick_alloc_fn(allocator);
    out->free = adt_i_pick_free_fn(allocator);
    out->userdata = adt_i_pick_userdata(allocator);
}

static adt_ctx *adt_i_ctx_new(hm_ctx *hm, const adt_allocator *allocator)
{
    adt_ctx *ctx;
    adt_alloc_fn alloc;
    void *userdata;
    void *raw;
    if (hm == NULL)
    {
        return NULL;
    }
    alloc = adt_i_pick_alloc_fn(allocator);
    userdata = adt_i_pick_userdata(allocator);
    raw = alloc(userdata, sizeof(adt_ctx));
    if (raw == NULL)
    {
        return NULL;
    }
    ctx = raw;
    memset(ctx, 0, sizeof(adt_ctx));
    ctx->hm = hm;
    adt_i_init_allocator(&ctx->alloc, allocator);
    return ctx;
}

adt_ctx *adt_ctx_new(hm_ctx *hm)
{
    adt_ctx *ctx;
    ctx = adt_i_ctx_new(hm, NULL);
    return ctx;
}

adt_ctx *adt_ctx_new_with_allocator(hm_ctx *hm, const adt_allocator *allocator)
{
    adt_ctx *ctx;
    ctx = adt_i_ctx_new(hm, allocator);
    return ctx;
}

void adt_i_ctor_discard(adt_ctx *ctx, struct adt_ctor *ctor)
{
    if (ctor->name != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, ctor->name);
    }
    if (ctor->field_types != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, ctor->field_types);
    }
    ctx->alloc.free(ctx->alloc.userdata, ctor);
}

static void adt_i_ctor_free_all(adt_ctx *ctx, struct adt_type *type)
{
    size_t i;
    for (i = 0; i < type->ctor_count; ++i)
    {
        adt_i_ctor_discard(ctx, type->ctors[i]);
    }
}

static void adt_i_free_type_ctors(adt_ctx *ctx, struct adt_type *type)
{
    if (type->ctors != NULL)
    {
        adt_i_ctor_free_all(ctx, type);
    }
    if (type->ctors != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, type->ctors);
    }
}

void adt_i_destroy_type(adt_ctx *ctx, struct adt_type *type)
{
    if (type->ctors != NULL)
    {
        adt_i_free_type_ctors(ctx, type);
    }
    if (type->parameters != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, type->parameters);
    }
    if (type->name != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, type->name);
    }
    if (type->hm_name != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, type->hm_name);
    }
    ctx->alloc.free(ctx->alloc.userdata, type);
}

static void adt_i_free_pattern(adt_ctx *ctx, struct adt_pattern *pattern)
{
    if (pattern->kind == ADT_PATTERN_CTOR)
    {
        if (pattern->u.ctor.args != NULL)
        {
            ctx->alloc.free(ctx->alloc.userdata, pattern->u.ctor.args);
        }
    }
    ctx->alloc.free(ctx->alloc.userdata, pattern);
}

static void adt_i_free_all_patterns(adt_ctx *ctx)
{
    size_t i;
    for (i = 0; i < ctx->pattern_count; ++i)
    {
        adt_i_free_pattern(ctx, ctx->patterns[i]);
    }
    if (ctx->patterns != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, ctx->patterns);
    }
}

static void adt_i_free_all_types(adt_ctx *ctx)
{
    size_t i;
    for (i = 0; i < ctx->type_count; ++i)
    {
        adt_i_destroy_type(ctx, ctx->types[i]);
    }
    if (ctx->types != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, ctx->types);
    }
}

static void adt_i_ctx_free_owned(adt_ctx *ctx)
{
    if (ctx->type_count != 0)
    {
        adt_i_free_all_types(ctx);
    }
    if (ctx->pattern_count != 0)
    {
        adt_i_free_all_patterns(ctx);
    }
    if (ctx->last_error != NULL)
    {
        ctx->alloc.free(ctx->alloc.userdata, ctx->last_error);
    }
}

void adt_ctx_destroy(adt_ctx *ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    adt_i_ctx_free_owned(ctx);
    ctx->alloc.free(ctx->alloc.userdata, ctx);
}

void adt_ctx_set_origin(adt_ctx *ctx, void *origin)
{
    if (ctx == NULL)
    {
        return;
    }
    ctx->origin = origin;
}

const adt_error *adt_ctx_error(const adt_ctx *ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }
    return ctx->last_error;
}

void *adt_i_alloc(adt_ctx *ctx, size_t size)
{
    void *raw;
    raw = ctx->alloc.alloc(ctx->alloc.userdata, size);
    if (raw == NULL)
    {
        return NULL;
    }
    memset(raw, 0, size);
    return raw;
}

char *adt_i_strdup(adt_ctx *ctx, const char *src)
{
    size_t len;
    void *raw;
    char *dst;
    len = strlen(src);
    raw = adt_i_alloc(ctx, len + 1);
    if (raw == NULL)
    {
        return NULL;
    }
    dst = raw;
    memcpy(dst, src, len + 1);
    return dst;
}

static size_t adt_i_grow_capacity(size_t capacity)
{
    if (capacity == 0)
    {
        return 4;
    }
    return capacity * 2;
}

static void adt_i_grow_copy(adt_ctx *ctx, void **dst, void **items,
                            size_t count)
{
    if (items == NULL)
    {
        return;
    }
    memcpy(dst, items, count * sizeof(void *));
    ctx->alloc.free(ctx->alloc.userdata, items);
}

int adt_i_grow(adt_ctx *ctx, void ***items, size_t *count, size_t *capacity)
{
    size_t nc;
    size_t bytes;
    void *raw;
    void **dst;
    if (*count < *capacity)
    {
        return 1;
    }
    nc = adt_i_grow_capacity(*capacity);
    bytes = nc * sizeof(void *);
    raw = ctx->alloc.alloc(ctx->alloc.userdata, bytes);
    if (raw == NULL)
    {
        return 0;
    }
    dst = raw;
    adt_i_grow_copy(ctx, dst, *items, *count);
    *items = dst;
    *capacity = nc;
    return 1;
}

int adt_i_register_type(adt_ctx *ctx, struct adt_type *type)
{
    int ok;
    ok = adt_i_grow(ctx, (void ***)&ctx->types, &ctx->type_count,
                    &ctx->type_capacity);
    if (ok == 0)
    {
        return 0;
    }
    ctx->types[ctx->type_count] = type;
    ++ctx->type_count;
    return 1;
}

int adt_i_register_pattern(adt_ctx *ctx, struct adt_pattern *pattern)
{
    int ok;
    ok = adt_i_grow(ctx, (void ***)&ctx->patterns, &ctx->pattern_count,
                    &ctx->pattern_capacity);
    if (ok == 0)
    {
        return 0;
    }
    ctx->patterns[ctx->pattern_count] = pattern;
    ++ctx->pattern_count;
    return 1;
}

static void adt_i_error_free(adt_ctx *ctx)
{
    if (ctx->last_error == NULL)
    {
        return;
    }
    ctx->alloc.free(ctx->alloc.userdata, ctx->last_error);
    ctx->last_error = NULL;
}

adt_status adt_i_fail(adt_ctx *ctx, adt_status status, adt_error_kind kind,
                      const char *message, struct adt_type *type,
                      struct adt_ctor *ctor, const struct hm_error *hm_err)
{
    struct adt_error *error;
    adt_i_error_free(ctx);
    error = adt_i_alloc(ctx, sizeof(struct adt_error));
    if (error == NULL)
    {
        return status;
    }
    error->kind = kind;
    error->message = message;
    error->type = type;
    error->ctor = ctor;
    error->hm_error = hm_err;
    error->origin = ctx->origin;
    ctx->last_error = error;
    return status;
}

static int adt_i_ctor_name_taken(struct adt_type *type, const char *name)
{
    size_t j;
    int cmp;
    for (j = 0; j < type->ctor_count; ++j)
    {
        cmp = strcmp(type->ctors[j]->name, name);
        if (cmp == 0)
        {
            return 1;
        }
    }
    return 0;
}

int adt_i_ctx_has_ctor_name(adt_ctx *ctx, const char *name)
{
    size_t i;
    int taken;
    for (i = 0; i < ctx->type_count; ++i)
    {
        taken = adt_i_ctor_name_taken(ctx->types[i], name);
        if (taken != 0)
        {
            return 1;
        }
    }
    return 0;
}
