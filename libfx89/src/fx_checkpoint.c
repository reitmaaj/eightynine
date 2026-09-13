/* fx_checkpoint.c - checkpoints, rollback, commit (solver state).
 *
 * A checkpoint records a mutation-trail position plus the variable-list head
 * and constraint-list tail. Semantic mutations made while a checkpoint is open
 * append undo records to the trail (see fx_trail_* helpers); rollback pops
 * those records in reverse and restores the list marks. This makes rollback
 * cost proportional to the changes since the checkpoint and restores variable
 * facts/bindings, constraint completion state, and conflict state exactly.
 * Checkpoint creation is atomic: it allocates only the small checkpoint
 * object. */
#include <string.h>

#include "fx_internal.h"

/* --- trail log (dynamic array, context-owned) --- */

static struct fx_trail_rec *trail_slot(fx_ctx *ctx, unsigned long index)
{
    return &ctx->trail[index];
}

static unsigned long trail_grow_cap(unsigned long cap)
{
    unsigned long nc;
    nc = cap;
    if (nc == 0u)
    {
        nc = 8u;
        return nc;
    }
    nc = cap * 2u;
    return nc;
}

static struct fx_trail_rec *grow_trail(fx_ctx *ctx)
{
    struct fx_trail_rec *arr;
    unsigned long nc;
    if (ctx->trail_n < ctx->trail_cap)
    {
        return ctx->trail;
    }
    nc = trail_grow_cap(ctx->trail_cap);
    arr =
        (struct fx_trail_rec *)fx__alloc(ctx, nc * sizeof(struct fx_trail_rec));
    if (arr == NULL)
    {
        return NULL;
    }
    if (ctx->trail_n != 0u)
    {
        memcpy(arr, ctx->trail, ctx->trail_n * sizeof(struct fx_trail_rec));
    }
    ctx->trail = arr;
    ctx->trail_cap = nc;
    return arr;
}

static fx_status trail_append(fx_ctx *ctx, const struct fx_trail_rec *rec)
{
    struct fx_trail_rec *arr;
    struct fx_trail_rec *dst;
    if (ctx->cp_top == NULL)
    {
        return FX_OK;
    }
    arr = grow_trail(ctx);
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    dst = trail_slot(ctx, ctx->trail_n);
    *dst = *rec;
    ctx->trail_n = ctx->trail_n + 1u;
    return FX_OK;
}

fx_status fx__trail_bind(fx_ctx *ctx, fx_var *var, fx_row *prev)
{
    struct fx_trail_rec rec;
    if (ctx->cp_top == NULL)
    {
        return FX_OK;
    }
    rec.kind = FX_TR_BIND;
    rec.var = var;
    rec.bind_prev = prev;
    rec.cons = NULL;
    rec.state_prev = FX_CONSTRAINT_PENDING;
    rec.conflict = FX_CONFLICT_NONE;
    rec.conflict_primary = NULL;
    rec.conflict_secondary = NULL;
    {
        fx_status st;
        st = trail_append(ctx, &rec);
        return st;
    }
}

fx_status fx__trail_req(fx_ctx *ctx, fx_var *var)
{
    struct fx_trail_rec rec;
    if (ctx->cp_top == NULL)
    {
        return FX_OK;
    }
    rec.kind = FX_TR_REQ;
    rec.var = var;
    rec.bind_prev = NULL;
    rec.cons = NULL;
    rec.state_prev = FX_CONSTRAINT_PENDING;
    rec.conflict = FX_CONFLICT_NONE;
    rec.conflict_primary = NULL;
    rec.conflict_secondary = NULL;
    {
        fx_status st;
        st = trail_append(ctx, &rec);
        return st;
    }
}

fx_status fx__trail_forb(fx_ctx *ctx, fx_var *var)
{
    struct fx_trail_rec rec;
    if (ctx->cp_top == NULL)
    {
        return FX_OK;
    }
    rec.kind = FX_TR_FORB;
    rec.var = var;
    rec.bind_prev = NULL;
    rec.cons = NULL;
    rec.state_prev = FX_CONSTRAINT_PENDING;
    rec.conflict = FX_CONFLICT_NONE;
    rec.conflict_primary = NULL;
    rec.conflict_secondary = NULL;
    {
        fx_status st;
        st = trail_append(ctx, &rec);
        return st;
    }
}

fx_status fx__trail_state(fx_ctx *ctx, fx_constraint *c,
                          fx_constraint_state prev)
{
    struct fx_trail_rec rec;
    if (ctx->cp_top == NULL)
    {
        return FX_OK;
    }
    rec.kind = FX_TR_STATE;
    rec.var = NULL;
    rec.bind_prev = NULL;
    rec.cons = c;
    rec.state_prev = prev;
    rec.conflict = FX_CONFLICT_NONE;
    rec.conflict_primary = NULL;
    rec.conflict_secondary = NULL;
    {
        fx_status st;
        st = trail_append(ctx, &rec);
        return st;
    }
}

fx_status fx__trail_conflict(fx_ctx *ctx, fx_conflict_kind kind,
                             const fx_constraint *primary,
                             const fx_constraint *secondary)
{
    struct fx_trail_rec rec;
    if (ctx->cp_top == NULL)
    {
        return FX_OK;
    }
    rec.kind = FX_TR_CONFLICT;
    rec.var = NULL;
    rec.bind_prev = NULL;
    rec.cons = NULL;
    rec.state_prev = FX_CONSTRAINT_PENDING;
    rec.conflict = kind;
    rec.conflict_primary = primary;
    rec.conflict_secondary = secondary;
    {
        fx_status st;
        st = trail_append(ctx, &rec);
        return st;
    }
}

/* --- undo application (reverse order) --- */

static void undo_bind(struct fx_trail_rec *r)
{
    r->var->binding = r->bind_prev;
}

static void undo_req(struct fx_trail_rec *r)
{
    r->var->nreq = r->var->nreq - 1u;
}

static void undo_forb(struct fx_trail_rec *r)
{
    r->var->nforb = r->var->nforb - 1u;
}

static void undo_state(struct fx_trail_rec *r)
{
    r->cons->state = r->state_prev;
}

static void undo_conflict(fx_ctx *ctx, struct fx_trail_rec *r)
{
    ctx->conflict = r->conflict;
    ctx->conflict_primary = r->conflict_primary;
    ctx->conflict_secondary = r->conflict_secondary;
}

static void undo_one(fx_ctx *ctx, struct fx_trail_rec *r)
{
    if (r->kind == FX_TR_BIND)
    {
        undo_bind(r);
    }
    else if (r->kind == FX_TR_REQ)
    {
        undo_req(r);
    }
    else if (r->kind == FX_TR_FORB)
    {
        undo_forb(r);
    }
    else if (r->kind == FX_TR_STATE)
    {
        undo_state(r);
    }
    else
    {
        undo_conflict(ctx, r);
    }
}

static unsigned long undo_at(fx_ctx *ctx, unsigned long idx)
{
    struct fx_trail_rec *r;
    r = trail_slot(ctx, idx);
    undo_one(ctx, r);
    return idx;
}

static void pop_trail(fx_ctx *ctx, unsigned long down_to)
{
    unsigned long i;
    i = ctx->trail_n;
    while (i > down_to)
    {
        i = undo_at(ctx, i - 1u);
    }
    ctx->trail_n = down_to;
}

/* --- list restoration --- */

static void clear_constraints(fx_ctx *ctx)
{
    ctx->constraints = NULL;
    ctx->constraints_tail = NULL;
}

static fx_constraint *mark_dead_step(fx_constraint *c)
{
    c->live = 0;
    return c->next;
}

static void mark_dead_from(fx_constraint *c)
{
    while (c != NULL)
    {
        c = mark_dead_step(c);
    }
}

static void cut_tail(fx_ctx *ctx, fx_constraint *tail)
{
    tail->next = NULL;
    ctx->constraints_tail = tail;
}

static void clear_all_constraints(fx_ctx *ctx)
{
    mark_dead_from(ctx->constraints);
    clear_constraints(ctx);
}

static void truncate_after(fx_ctx *ctx, fx_constraint *tail)
{
    mark_dead_from(tail->next);
    cut_tail(ctx, tail);
}

static void truncate_constraints(fx_ctx *ctx, fx_constraint *tail)
{
    if (tail == NULL)
    {
        clear_all_constraints(ctx);
        return;
    }
    truncate_after(ctx, tail);
}

fx_checkpoint fx_ctx_checkpoint(fx_ctx *ctx)
{
    fx_cp *cp;
    fx_checkpoint token;
    if (ctx == NULL)
    {
        return 0u;
    }
    cp = (fx_cp *)fx__alloc(ctx, sizeof(fx_cp));
    if (cp == NULL)
    {
        return 0u;
    }
    token = ctx->next_cp;
    ctx->next_cp = ctx->next_cp + 1u;
    cp->token = token;
    cp->trail_mark = ctx->trail_n;
    cp->tail = ctx->constraints_tail;
    cp->vars = ctx->vars;
    cp->fact = ctx->fact_serial;
    cp->prev = ctx->cp_top;
    ctx->cp_top = cp;
    return token;
}

static fx_cp *find_top(fx_ctx *ctx, fx_checkpoint token)
{
    fx_cp *cp;
    cp = ctx->cp_top;
    if (cp == NULL)
    {
        return NULL;
    }
    if (cp->token != token)
    {
        return NULL;
    }
    return cp;
}

fx_status fx_ctx_rollback(fx_ctx *ctx, fx_checkpoint checkpoint)
{
    fx_cp *cp;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    cp = find_top(ctx, checkpoint);
    if (cp == NULL)
    {
        return FX_ERR_INVALID;
    }
    pop_trail(ctx, cp->trail_mark);
    truncate_constraints(ctx, cp->tail);
    ctx->vars = cp->vars;
    ctx->fact_serial = cp->fact;
    ctx->cp_top = cp->prev;
    return FX_OK;
}

fx_status fx_ctx_commit(fx_ctx *ctx, fx_checkpoint checkpoint)
{
    fx_cp *cp;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    cp = find_top(ctx, checkpoint);
    if (cp == NULL)
    {
        return FX_ERR_INVALID;
    }
    ctx->cp_top = cp->prev;
    return FX_OK;
}
