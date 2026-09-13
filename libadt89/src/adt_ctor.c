/* adt_ctor.c - constructor declarations and introspection. */
#include <adt.h>

#include "adt_internal.h"

static int adt_i_fields_valid(adt_ctx *ctx, struct adt_type *owner,
                              size_t field_count, hm_type **field_types)
{
    size_t i;
    if (field_count == 0)
    {
        return 1;
    }
    if (field_types == NULL)
    {
        adt_i_fail(ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID, "null field types",
                   owner, NULL, NULL);
        return 0;
    }
    for (i = 0; i < field_count; ++i)
    {
        if (field_types[i] == NULL)
        {
            adt_i_fail(ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID,
                       "null field type", owner, NULL, NULL);
            return 0;
        }
    }
    return 1;
}

static void adt_i_copy_field(struct adt_ctor *ctor, size_t i,
                             hm_type **field_types)
{
    ctor->field_types[i] = field_types[i];
}

static adt_status adt_i_ctor_copy_fields(adt_ctx *ctx, struct adt_ctor *ctor,
                                         size_t field_count,
                                         hm_type **field_types)
{
    size_t i;
    size_t bytes;
    void *raw;
    if (field_count == 0)
    {
        return ADT_OK;
    }
    bytes = field_count * sizeof(struct hm_type *);
    raw = adt_i_alloc(ctx, bytes);
    if (raw == NULL)
    {
        return ADT_ERROR_NOMEM;
    }
    ctor->field_types = raw;
    ctor->field_count = field_count;
    for (i = 0; i < field_count; ++i)
    {
        adt_i_copy_field(ctor, i, field_types);
    }
    return ADT_OK;
}

static int adt_i_ctor_append(adt_ctx *ctx, struct adt_type *owner,
                             struct adt_ctor *ctor)
{
    int ok;
    ok = adt_i_grow(ctx, (void ***)&owner->ctors, &owner->ctor_count,
                    &owner->ctor_capacity);
    if (ok == 0)
    {
        return 0;
    }
    owner->ctors[owner->ctor_count] = ctor;
    ++owner->ctor_count;
    return 1;
}

adt_ctor *adt_ctor_add(adt_type *owner, const char *name, size_t field_count,
                       hm_type **field_types)
{
    struct adt_ctx *ctx;
    struct adt_ctor *ctor;
    char *n;
    adt_status st;
    void *raw;
    int dup;
    int valid;
    int app;
    if (owner == NULL)
    {
        return NULL;
    }
    if (name == NULL)
    {
        return NULL;
    }
    ctx = owner->ctx;
    if (owner->sealed != 0)
    {
        adt_i_fail(ctx, ADT_ERROR_SEALED, ADT_ERR_SEALED,
                   "cannot add a constructor to a sealed type", owner, NULL,
                   NULL);
        return NULL;
    }
    dup = adt_i_ctx_has_ctor_name(ctx, name);
    if (dup != 0)
    {
        adt_i_fail(ctx, ADT_ERROR_DUPLICATE_CTOR, ADT_ERR_DUPLICATE_CTOR,
                   "duplicate constructor name", owner, NULL, NULL);
        return NULL;
    }
    valid = adt_i_fields_valid(ctx, owner, field_count, field_types);
    if (valid == 0)
    {
        return NULL;
    }
    raw = adt_i_alloc(ctx, sizeof(struct adt_ctor));
    if (raw == NULL)
    {
        adt_i_fail(ctx, ADT_ERROR_NOMEM, ADT_ERR_INVALID, "out of memory",
                   owner, NULL, NULL);
        return NULL;
    }
    ctor = raw;
    ctor->owner = owner;
    ctor->scheme = NULL;
    ctor->origin = ctx->origin;
    n = adt_i_strdup(ctx, name);
    if (n == NULL)
    {
        adt_i_ctor_discard(ctx, ctor);
        return NULL;
    }
    ctor->name = n;
    st = adt_i_ctor_copy_fields(ctx, ctor, field_count, field_types);
    if (st != ADT_OK)
    {
        adt_i_ctor_discard(ctx, ctor);
        return NULL;
    }
    app = adt_i_ctor_append(ctx, owner, ctor);
    if (app == 0)
    {
        adt_i_ctor_discard(ctx, ctor);
        return NULL;
    }
    return ctor;
}

const char *adt_ctor_name(const adt_ctor *ctor)
{
    if (ctor == NULL)
    {
        return "";
    }
    return ctor->name;
}

adt_type *adt_ctor_owner(const adt_ctor *ctor)
{
    if (ctor == NULL)
    {
        return NULL;
    }
    return ctor->owner;
}

size_t adt_ctor_field_count(const adt_ctor *ctor)
{
    if (ctor == NULL)
    {
        return 0;
    }
    return ctor->field_count;
}

hm_type *adt_ctor_field_type(const adt_ctor *ctor, size_t index)
{
    if (ctor == NULL)
    {
        return NULL;
    }
    if (index >= ctor->field_count)
    {
        return NULL;
    }
    return ctor->field_types[index];
}
