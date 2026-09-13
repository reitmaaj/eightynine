/* fx_constraint.c - constraint objects and the public require_* entry
 * points. */
#include "fx_internal.h"

static fx_status constraint_push(fx_ctx *ctx, fx_constraint_kind kind,
                                 const fx_row *a, const fx_row *b,
                                 const fx_row *extra, const fx_atom *atom,
                                 fx_constraint **out)
{
    fx_constraint *c;
    c = (fx_constraint *)fx__alloc(ctx, sizeof(fx_constraint));
    if (c == NULL)
    {
        return FX_ERR_NOMEM;
    }
    c->owner = ctx;
    c->id = ctx->next_cid;
    ctx->next_cid = ctx->next_cid + 1u;
    c->kind = kind;
    c->state = FX_CONSTRAINT_PENDING;
    c->userdata = NULL;
    c->a = a;
    c->b = b;
    c->extra = extra;
    c->atom = atom;
    c->next = NULL;
    c->qnext = NULL;
    c->enqueued = 0;
    c->live = 1;
    if (ctx->constraints_tail == NULL)
    {
        ctx->constraints = c;
    }
    else
    {
        ctx->constraints_tail->next = c;
    }
    ctx->constraints_tail = c;
    fx__watch_register(ctx, c);
    if (out != NULL)
    {
        *out = c;
    }
    return FX_OK;
}

fx_status fx_require_equal(fx_ctx *ctx, const fx_row *a, const fx_row *b,
                           fx_constraint **out)
{
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (a == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (b == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, a) == 0)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, b) == 0)
    {
        return FX_ERR_INVALID;
    }
    st = constraint_push(ctx, FX_CONSTRAINT_EQUAL, a, b, NULL, NULL, out);
    return st;
}

fx_status fx_require_subset(fx_ctx *ctx, const fx_row *subset,
                            const fx_row *superset, fx_constraint **out)
{
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (subset == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (superset == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, subset) == 0)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, superset) == 0)
    {
        return FX_ERR_INVALID;
    }
    st = constraint_push(ctx, FX_CONSTRAINT_SUBSET, subset, superset, NULL,
                         NULL, out);
    return st;
}

fx_status fx_require_member(fx_ctx *ctx, const fx_atom *atom, const fx_row *row,
                            fx_constraint **out)
{
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (atom == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (row == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (fx__atom_local(ctx, atom) == 0)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, row) == 0)
    {
        return FX_ERR_INVALID;
    }
    st = constraint_push(ctx, FX_CONSTRAINT_MEMBER, NULL, row, NULL, atom, out);
    return st;
}

fx_status fx_require_lacks(fx_ctx *ctx, const fx_atom *atom, const fx_row *row,
                           fx_constraint **out)
{
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (atom == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (row == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (fx__atom_local(ctx, atom) == 0)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, row) == 0)
    {
        return FX_ERR_INVALID;
    }
    st = constraint_push(ctx, FX_CONSTRAINT_LACKS, NULL, row, NULL, atom, out);
    return st;
}

fx_status fx_require_join(fx_ctx *ctx, const fx_row *out_row,
                          const fx_row *left, const fx_row *right,
                          fx_constraint **out)
{
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out_row == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (left == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (right == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, out_row) == 0)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, left) == 0)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, right) == 0)
    {
        return FX_ERR_INVALID;
    }
    st = constraint_push(ctx, FX_CONSTRAINT_JOIN, out_row, left, right, NULL,
                         out);
    return st;
}

fx_status fx_require_pure(fx_ctx *ctx, const fx_row *row, fx_constraint **out)
{
    const fx_row *empty;
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (row == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (fx__row_local(ctx, row) == 0)
    {
        return FX_ERR_INVALID;
    }
    empty = NULL;
    st = fx_row_empty(ctx, &empty);
    if (st != FX_OK)
    {
        return st;
    }
    st = fx_require_equal(ctx, row, empty, out);
    return st;
}

fx_constraint_id fx_constraint_id_of(const fx_constraint *constraint)
{
    if (constraint == NULL)
    {
        return 0u;
    }
    return constraint->id;
}

fx_constraint_kind fx_constraint_kind_of(const fx_constraint *constraint)
{
    if (constraint == NULL)
    {
        return FX_CONSTRAINT_EQUAL;
    }
    return constraint->kind;
}

fx_constraint_state fx_constraint_state_of(const fx_constraint *constraint)
{
    if (constraint == NULL)
    {
        return FX_CONSTRAINT_PENDING;
    }
    return constraint->state;
}

void fx_constraint_set_userdata(fx_constraint *constraint, void *userdata)
{
    if (constraint == NULL)
    {
        return;
    }
    constraint->userdata = userdata;
}

void *fx_constraint_userdata(const fx_constraint *constraint)
{
    if (constraint == NULL)
    {
        return NULL;
    }
    return constraint->userdata;
}
