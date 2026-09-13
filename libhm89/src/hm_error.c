/* hm_error.c - structured error nodes and accessors. */
#include <hm.h>

#include "hm_internal.h"

struct hm_error *hm_i_error(hm_ctx *ctx, int kind, const char *message)
{
    struct hm_error *e;
    void *raw;
    raw = hm_i_alloc(ctx, sizeof(struct hm_error));
    if (raw == NULL)
    {
        return NULL;
    }
    e = raw;
    e->kind = kind;
    e->message = message;
    e->left = NULL;
    e->right = NULL;
    e->name = NULL;
    e->origin = ctx->origin;
    return e;
}

hm_error_kind hm_error_kind_of(const hm_error *error)
{
    if (error == NULL)
    {
        return HM_ERR_NONE;
    }
    return (hm_error_kind)error->kind;
}

const char *hm_error_message(const hm_error *error)
{
    if (error == NULL)
    {
        return "";
    }
    if (error->message == NULL)
    {
        return "";
    }
    return error->message;
}

hm_type *hm_error_left(const hm_error *error)
{
    if (error == NULL)
    {
        return NULL;
    }
    return error->left;
}

hm_type *hm_error_right(const hm_error *error)
{
    if (error == NULL)
    {
        return NULL;
    }
    return error->right;
}

const char *hm_error_name(const hm_error *error)
{
    if (error == NULL)
    {
        return "";
    }
    if (error->name == NULL)
    {
        return "";
    }
    return error->name;
}

void *hm_error_origin(const hm_error *error)
{
    if (error == NULL)
    {
        return NULL;
    }
    return error->origin;
}
