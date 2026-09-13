/* hm_unify.c - destructive unification with occurs checking. */
#include <hm.h>

#include "hm_internal.h"

#include <string.h>

static int hm_i_occurs(hm_type *var, hm_type *type)
{
    hm_type *p;
    size_t i;
    int found;
    p = hm_type_prune(type);
    if (p == var)
    {
        return 1;
    }
    if (p->kind == HM_TYPE_VAR)
    {
        return 0;
    }
    for (i = 0; i < p->u.con.arity; ++i)
    {
        found = hm_i_occurs(var, p->u.con.args[i]);
        if (found != 0)
        {
            return 1;
        }
    }
    return 0;
}

static hm_error *hm_i_err_pair(hm_ctx *ctx, int kind, const char *message,
                               hm_type *left, hm_type *right)
{
    hm_error *e;
    e = hm_i_error(ctx, kind, message);
    if (e == NULL)
    {
        return NULL;
    }
    e->left = left;
    e->right = right;
    return e;
}

static hm_status hm_i_mismatch(hm_ctx *ctx, hm_type *a, hm_type *b,
                               hm_error **error)
{
    if (error != NULL)
    {
        *error = hm_i_err_pair(ctx, HM_ERR_MISMATCH, "type mismatch", a, b);
    }
    return HM_ERROR_MISMATCH;
}

static hm_status hm_i_bind_var(hm_ctx *ctx, hm_type *var, hm_type *other,
                               hm_error **error)
{
    int occurs;
    occurs = hm_i_occurs(var, other);
    if (occurs != 0)
    {
        if (error != NULL)
        {
            *error = hm_i_err_pair(ctx, HM_ERR_OCCURS, "occurs check failed",
                                   var, other);
        }
        return HM_ERROR_OCCURS;
    }
    var->u.var.link = other;
    return HM_OK;
}

hm_status hm_unify(hm_ctx *ctx, hm_type *left, hm_type *right, hm_error **error)
{
    hm_type *a;
    hm_type *b;
    int cmp;
    size_t arity;
    size_t i;
    hm_status st;
    if (error != NULL)
    {
        *error = NULL;
    }
    a = hm_type_prune(left);
    b = hm_type_prune(right);
    if (a == b)
    {
        return HM_OK;
    }
    if (a->kind == HM_TYPE_VAR)
    {
        st = hm_i_bind_var(ctx, a, b, error);
        return st;
    }
    if (b->kind == HM_TYPE_VAR)
    {
        st = hm_i_bind_var(ctx, b, a, error);
        return st;
    }
    cmp = strcmp(a->u.con.name, b->u.con.name);
    if (cmp != 0)
    {
        st = hm_i_mismatch(ctx, a, b, error);
        return st;
    }
    if (a->u.con.arity != b->u.con.arity)
    {
        st = hm_i_mismatch(ctx, a, b, error);
        return st;
    }
    arity = a->u.con.arity;
    for (i = 0; i < arity; ++i)
    {
        st = hm_unify(ctx, a->u.con.args[i], b->u.con.args[i], error);
        if (st != HM_OK)
        {
            return st;
        }
    }
    return HM_OK;
}
