/* fx_var.c - row variables, union-find, facts, bindings. */
#include "fx_internal.h"

static fx_var *rep_parent(fx_var *cur)
{
    return cur->parent;
}

struct fx_var *fx__var_rep(fx_ctx *ctx, struct fx_var *var)
{
    struct fx_var *cur;
    (void)ctx;
    cur = var;
    while (cur->parent != cur)
    {
        cur = rep_parent(cur);
    }
    return cur;
}

int fx__var_has_required(const struct fx_var *var, const fx_atom *atom)
{
    unsigned long i;
    for (i = 0; i < var->nreq; ++i)
    {
        if (fx__atom_eq(var->required[i], atom))
        {
            return 1;
        }
    }
    return 0;
}

int fx__var_has_forbidden(const struct fx_var *var, const fx_atom *atom)
{
    unsigned long i;
    for (i = 0; i < var->nforb; ++i)
    {
        if (fx__atom_eq(var->forbidden[i], atom))
        {
            return 1;
        }
    }
    return 0;
}

static void undo_require_one(fx_var *var)
{
    var->nreq = var->nreq - 1u;
}

static void undo_forbid_one(fx_var *var)
{
    var->nforb = var->nforb - 1u;
}

fx_status fx__var_require(fx_ctx *ctx, struct fx_var *var, const fx_atom *atom)
{
    const fx_atom **arr;
    fx_status st;
    int has;
    has = fx__var_has_required(var, atom);
    if (has != 0)
    {
        return FX_OK;
    }
    has = fx__var_has_forbidden(var, atom);
    if (has != 0)
    {
        fx__record_conflict(ctx, FX_CONFLICT_MEMBER_LACKS, NULL, NULL);
        return FX_ERR_UNSAT;
    }
    arr = fx__arr_push(ctx, var->required, &var->nreq, &var->creq, atom);
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    var->required = arr;
    st = fx__trail_req(ctx, var);
    if (st != FX_OK)
    {
        undo_require_one(var);
        return st;
    }
    ctx->fact_serial = ctx->fact_serial + 1u;
    fx__solve_notify(ctx, var);
    return FX_OK;
}

fx_status fx__var_forbid(fx_ctx *ctx, struct fx_var *var, const fx_atom *atom)
{
    const fx_atom **arr;
    fx_status st;
    int has;
    has = fx__var_has_forbidden(var, atom);
    if (has != 0)
    {
        return FX_OK;
    }
    has = fx__var_has_required(var, atom);
    if (has != 0)
    {
        fx__record_conflict(ctx, FX_CONFLICT_MEMBER_LACKS, NULL, NULL);
        return FX_ERR_UNSAT;
    }
    arr = fx__arr_push(ctx, var->forbidden, &var->nforb, &var->cforb, atom);
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    var->forbidden = arr;
    st = fx__trail_forb(ctx, var);
    if (st != FX_OK)
    {
        undo_forbid_one(var);
        return st;
    }
    ctx->fact_serial = ctx->fact_serial + 1u;
    fx__solve_notify(ctx, var);
    return FX_OK;
}

fx_status fx_var_new(fx_ctx *ctx, fx_var **out)
{
    fx_var *var;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    var = (fx_var *)fx__alloc_vars(ctx, 1u);
    if (var == NULL)
    {
        return FX_ERR_NOMEM;
    }
    var->id = ctx->next_var;
    ctx->next_var = ctx->next_var + 1u;
    var->owner = ctx;
    var->name = NULL;
    var->parent = var;
    var->rank = 0u;
    var->binding = NULL;
    var->watchers = NULL;
    var->required = NULL;
    var->nreq = 0u;
    var->creq = 0u;
    var->forbidden = NULL;
    var->nforb = 0u;
    var->cforb = 0u;
    var->next = ctx->vars;
    ctx->vars = var;
    *out = var;
    return FX_OK;
}

fx_var_id fx_var_id_of(const fx_var *var)
{
    if (var == NULL)
    {
        return 0u;
    }
    return var->id;
}

fx_status fx_var_set_name(fx_var *var, const char *name)
{
    char *copy;
    if (var == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (name == NULL)
    {
        return FX_ERR_INVALID;
    }
    copy = fx__strdup(var->owner, name);
    if (copy == NULL)
    {
        return FX_ERR_NOMEM;
    }
    var->name = copy;
    return FX_OK;
}

const char *fx_var_name(const fx_var *var)
{
    if (var == NULL)
    {
        return NULL;
    }
    return var->name;
}

fx_status fx_var_binding(fx_ctx *ctx, fx_var *var, const fx_row **out)
{
    fx_var *root;
    fx_row *bound;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (var == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    root = fx__var_rep(ctx, var);
    if (root->binding == NULL)
    {
        *out = NULL;
        return FX_OK;
    }
    bound = fx__row_normalize(ctx, root->binding);
    if (bound == NULL)
    {
        return FX_ERR_NOMEM;
    }
    *out = bound;
    return FX_OK;
}
