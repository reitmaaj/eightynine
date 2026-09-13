/* fx_scheme.c - effect-only schemes: construction, generalization,
 * instantiation, inspection.
 *
 * A scheme is immutable polymorphic syntax. Building it freezes the meaning of
 * the quantified variables, the body row, and the residual constraints into
 * templates (heads plus quantified-slot tails, plus required/forbidden atom
 * facts). Instantiation replays those templates with fresh variables, so a
 * scheme's instances never change when its source variables or constraints
 * are later solved, bound, or rolled back. Live objects are retained only for
 * inspection. */
#include <stdlib.h>
#include <string.h>

#include "fx_internal.h"

#define FX_SLOT_NONE ((unsigned long)-1)

/* --- small helpers --- */

static int var_in(fx_var *v, fx_var *const *arr, unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n; ++i)
    {
        if (arr[i] == v)
        {
            return 1;
        }
    }
    return 0;
}

static int same_rep(fx_ctx *ctx, fx_var *a, fx_var *b)
{
    return fx__var_rep(ctx, a) == fx__var_rep(ctx, b);
}

static int residual_kind(fx_constraint *c)
{
    if (c->kind == FX_CONSTRAINT_SUBSET)
    {
        return 1;
    }
    if (c->kind == FX_CONSTRAINT_JOIN)
    {
        return 1;
    }
    return 0;
}

static int take_one(fx_constraint *c)
{
    int rk;
    rk = residual_kind(c);
    if (rk == 0)
    {
        return 0;
    }
    if (c->state != FX_CONSTRAINT_PENDING)
    {
        return 0;
    }
    return 1;
}

/* --- closure/ownership validation --- */

static int unique_ids(fx_var *const *vars, unsigned long n)
{
    unsigned long i;
    unsigned long j;
    for (i = 0; i < n; ++i)
    {
        for (j = i + 1u; j < n; ++j)
        {
            if (vars[i]->id == vars[j]->id)
            {
                return 0;
            }
        }
    }
    return 1;
}

static fx_var *top_var_of(fx_ctx *ctx, const fx_row *row)
{
    fx_row *n;
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return NULL;
    }
    return n->tail;
}

/* Quantified-slot index of a normalized tail variable, or FX_SLOT_NONE. */
static unsigned long slot_of(fx_ctx *ctx, fx_var *tail, fx_var *const *qv,
                             unsigned long nqv)
{
    unsigned long i;
    for (i = 0; i < nqv; ++i)
    {
        int eq;
        eq = same_rep(ctx, tail, qv[i]);
        if (eq != 0)
        {
            return i;
        }
    }
    return FX_SLOT_NONE;
}

static fx_status copy_atoms(fx_ctx *ctx, const fx_atom *const *src,
                            unsigned long n, const fx_atom ***out)
{
    const fx_atom **arr;
    if (n == 0u)
    {
        *out = NULL;
        return FX_OK;
    }
    arr = (const fx_atom **)fx__alloc(ctx, n * sizeof(const fx_atom *));
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    memcpy(arr, src, n * sizeof(const fx_atom *));
    *out = arr;
    return FX_OK;
}

/* --- template snapshotting --- */

static fx_status snapshot_facts(fx_ctx *ctx, fx_var *root, fx_var_tmpl *out)
{
    fx_status st;
    st = copy_atoms(ctx, root->required, root->nreq, &out->required);
    if (st != FX_OK)
    {
        return st;
    }
    out->nreq = root->nreq;
    st = copy_atoms(ctx, root->forbidden, root->nforb, &out->forbidden);
    if (st != FX_OK)
    {
        return st;
    }
    out->nforb = root->nforb;
    return FX_OK;
}

static void set_closed_tmpl(fx_row_tmpl *out)
{
    out->open = 0;
    out->slot = 0u;
}

static void set_open_tmpl(fx_row_tmpl *out, unsigned long slot)
{
    out->open = 1;
    out->slot = slot;
}

static fx_status snapshot_row(fx_ctx *ctx, const fx_row *row, fx_var *const *qv,
                              unsigned long nqv, int allow_outer,
                              fx_row_tmpl *out)
{
    fx_row *n;
    fx_status st;
    unsigned long slot;
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return FX_ERR_NOMEM;
    }
    st = copy_atoms(ctx, n->head, n->nhead, &out->head);
    if (st != FX_OK)
    {
        return st;
    }
    out->nhead = n->nhead;
    if (n->tail == NULL)
    {
        set_closed_tmpl(out);
        return FX_OK;
    }
    slot = slot_of(ctx, n->tail, qv, nqv);
    if (slot != FX_SLOT_NONE)
    {
        set_open_tmpl(out, slot);
        return FX_OK;
    }
    if (allow_outer == 0)
    {
        return FX_ERR_INVALID;
    }
    set_open_tmpl(out, FX_SLOT_NONE);
    return FX_OK;
}

static fx_status snapshot_cons(fx_ctx *ctx, fx_constraint *c, fx_var *const *qv,
                               unsigned long nqv, fx_cons_tmpl *out)
{
    fx_status st;
    out->kind = c->kind;
    st = snapshot_row(ctx, c->a, qv, nqv, 0, &out->a);
    if (st != FX_OK)
    {
        return st;
    }
    st = snapshot_row(ctx, c->b, qv, nqv, 0, &out->b);
    if (st != FX_OK)
    {
        return st;
    }
    if (c->kind == FX_CONSTRAINT_JOIN)
    {
        st = snapshot_row(ctx, c->extra, qv, nqv, 0, &out->extra);
        if (st != FX_OK)
        {
            return st;
        }
    }
    return FX_OK;
}

/* --- instantiation (replays templates with fresh variables) --- */

static fx_status var_require_restored(fx_ctx *ctx, fx_var *var, unsigned long i,
                                      const fx_var_tmpl *vt)
{
    unsigned long j;
    for (j = 0; j < vt[i].nreq; ++j)
    {
        fx_status st;
        st = fx__var_require(ctx, var, vt[i].required[j]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    return FX_OK;
}

static fx_status var_forbid_restored(fx_ctx *ctx, fx_var *var, unsigned long i,
                                     const fx_var_tmpl *vt)
{
    unsigned long j;
    for (j = 0; j < vt[i].nforb; ++j)
    {
        fx_status st;
        st = fx__var_forbid(ctx, var, vt[i].forbidden[j]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    return FX_OK;
}

static fx_status restore_slot(fx_ctx *ctx, fx_var *fresh, unsigned long i,
                              const fx_var_tmpl *vt)
{
    fx_status st;
    st = var_require_restored(ctx, fresh, i, vt);
    if (st != FX_OK)
    {
        return st;
    }
    st = var_forbid_restored(ctx, fresh, i, vt);
    return st;
}

static fx_status fresh_all(fx_ctx *ctx, fx_var **newv, unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n; ++i)
    {
        fx_status st;
        st = fx_var_new(ctx, &newv[i]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    return FX_OK;
}

static fx_status restore_all(fx_ctx *ctx, fx_var *const *newv, unsigned long n,
                             const fx_var_tmpl *vt)
{
    unsigned long i;
    for (i = 0; i < n; ++i)
    {
        fx_status st;
        st = restore_slot(ctx, newv[i], i, vt);
        if (st != FX_OK)
        {
            return st;
        }
    }
    return FX_OK;
}

/* Materialize a row template into a live row, using fresh[slot] for open
 * tails. Open construction re-asserts the disjoint-head lacks on the tail. */
static fx_status materialize_row(fx_ctx *ctx, const fx_row_tmpl *t,
                                 fx_var *const *fresh, const fx_row **out)
{
    fx_status st;
    if (t->open == 0)
    {
        st = fx_row_closed(ctx, t->head, t->nhead, out);
        return st;
    }
    if (t->slot == FX_SLOT_NONE)
    {
        return FX_ERR_UNSUPPORTED;
    }
    st = fx_row_open(ctx, t->head, t->nhead, fresh[t->slot], out);
    return st;
}

static fx_status reify_subset_tmpl(fx_ctx *ctx, const fx_cons_tmpl *t,
                                   fx_var *const *fresh)
{
    const fx_row *a;
    const fx_row *b;
    fx_status st;
    a = NULL;
    st = materialize_row(ctx, &t->a, fresh, &a);
    if (st != FX_OK)
    {
        return st;
    }
    b = NULL;
    st = materialize_row(ctx, &t->b, fresh, &b);
    if (st != FX_OK)
    {
        return st;
    }
    st = fx_require_subset(ctx, a, b, NULL);
    return st;
}

static fx_status reify_join_tmpl(fx_ctx *ctx, const fx_cons_tmpl *t,
                                 fx_var *const *fresh)
{
    const fx_row *a;
    const fx_row *b;
    const fx_row *e;
    fx_status st;
    a = NULL;
    st = materialize_row(ctx, &t->a, fresh, &a);
    if (st != FX_OK)
    {
        return st;
    }
    b = NULL;
    st = materialize_row(ctx, &t->b, fresh, &b);
    if (st != FX_OK)
    {
        return st;
    }
    e = NULL;
    st = materialize_row(ctx, &t->extra, fresh, &e);
    if (st != FX_OK)
    {
        return st;
    }
    st = fx_require_join(ctx, a, b, e, NULL);
    return st;
}

static fx_status reify_one(fx_ctx *ctx, const fx_cons_tmpl *t,
                           fx_var *const *fresh)
{
    fx_status st;
    if (t->kind == FX_CONSTRAINT_SUBSET)
    {
        st = reify_subset_tmpl(ctx, t, fresh);
        return st;
    }
    if (t->kind == FX_CONSTRAINT_JOIN)
    {
        st = reify_join_tmpl(ctx, t, fresh);
        return st;
    }
    return FX_ERR_UNSUPPORTED;
}

static fx_status reify_all(fx_ctx *ctx, const fx_cons_tmpl *ct,
                           unsigned long ncons, fx_var *const *fresh)
{
    unsigned long i;
    for (i = 0; i < ncons; ++i)
    {
        fx_status st;
        st = reify_one(ctx, &ct[i], fresh);
        if (st != FX_OK)
        {
            return st;
        }
    }
    return FX_OK;
}

static fx_status instantiate_work(fx_ctx *ctx, const fx_scheme *scheme,
                                  const fx_row **out)
{
    fx_var **fresh;
    const fx_row *body;
    fx_status st;
    fresh = NULL;
    body = NULL;
    fresh = (fx_var **)fx__alloc(ctx, scheme->nvars * sizeof(fx_var *));
    if (fresh == NULL)
    {
        return FX_ERR_NOMEM;
    }
    st = fresh_all(ctx, fresh, scheme->nvars);
    if (st != FX_OK)
    {
        return st;
    }
    st = restore_all(ctx, fresh, scheme->nvars, scheme->vtmpl);
    if (st != FX_OK)
    {
        return st;
    }
    st = reify_all(ctx, scheme->ctmpl, scheme->ncons, fresh);
    if (st != FX_OK)
    {
        return st;
    }
    st = materialize_row(ctx, &scheme->body_tmpl, fresh, &body);
    if (st != FX_OK)
    {
        return st;
    }
    *out = body;
    return FX_OK;
}

fx_status fx_instantiate(fx_ctx *ctx, const fx_scheme *scheme,
                         const fx_row **out)
{
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (scheme == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    st = instantiate_work(ctx, scheme, out);
    return st;
}

/* --- scheme construction --- */

static fx_status copy_var_ptrs(fx_ctx *ctx, unsigned long nvars,
                               fx_var *const *vars, fx_var ***out)
{
    fx_var **vp;
    if (nvars == 0u)
    {
        *out = NULL;
        return FX_OK;
    }
    vp = (fx_var **)fx__alloc(ctx, nvars * sizeof(fx_var *));
    if (vp == NULL)
    {
        return FX_ERR_NOMEM;
    }
    memcpy(vp, vars, nvars * sizeof(fx_var *));
    *out = vp;
    return FX_OK;
}

static fx_status copy_cons_ptrs(fx_ctx *ctx, unsigned long ncons,
                                fx_constraint *const *cons,
                                fx_constraint ***out)
{
    fx_constraint **cp;
    if (ncons == 0u)
    {
        *out = NULL;
        return FX_OK;
    }
    cp = (fx_constraint **)fx__alloc(ctx, ncons * sizeof(fx_constraint *));
    if (cp == NULL)
    {
        return FX_ERR_NOMEM;
    }
    memcpy(cp, cons, ncons * sizeof(fx_constraint *));
    *out = cp;
    return FX_OK;
}

static fx_status snapshot_one_slot(fx_ctx *ctx, fx_var *var, fx_var_tmpl *vt,
                                   unsigned long *idp)
{
    fx_var *root;
    fx_status st;
    root = fx__var_rep(ctx, var);
    st = snapshot_facts(ctx, root, vt);
    if (st != FX_OK)
    {
        return st;
    }
    *idp = var->id;
    return FX_OK;
}

static void set_empty_slots(fx_var_tmpl **out, unsigned long **out_ids)
{
    *out = NULL;
    *out_ids = NULL;
}

static fx_status capture_slots(fx_ctx *ctx, unsigned long nvars,
                               fx_var *const *vars, fx_var_tmpl **out,
                               unsigned long **out_ids)
{
    fx_var_tmpl *vt;
    unsigned long *ids;
    unsigned long i;
    fx_status st;
    vt = NULL;
    ids = NULL;
    if (nvars == 0u)
    {
        set_empty_slots(out, out_ids);
        return FX_OK;
    }
    vt = (fx_var_tmpl *)fx__alloc(ctx, nvars * sizeof(fx_var_tmpl));
    if (vt == NULL)
    {
        return FX_ERR_NOMEM;
    }
    ids = (unsigned long *)fx__alloc(ctx, nvars * sizeof(unsigned long));
    if (ids == NULL)
    {
        return FX_ERR_NOMEM;
    }
    for (i = 0; i < nvars; ++i)
    {
        st = snapshot_one_slot(ctx, vars[i], &vt[i], &ids[i]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    *out = vt;
    *out_ids = ids;
    return FX_OK;
}

static fx_status capture_rows(fx_ctx *ctx, const fx_row *body,
                              fx_var *const *vars, unsigned long nvars,
                              int allow_outer, fx_row_tmpl *out)
{
    fx_status st;
    st = snapshot_row(ctx, body, vars, nvars, allow_outer, out);
    return st;
}

static fx_status capture_cons_all(fx_ctx *ctx, fx_constraint *const *cons,
                                  unsigned long ncons, fx_var *const *vars,
                                  unsigned long nvars, fx_cons_tmpl **out)
{
    fx_cons_tmpl *ct;
    unsigned long i;
    fx_status st;
    if (ncons == 0u)
    {
        *out = NULL;
        return FX_OK;
    }
    ct = (fx_cons_tmpl *)fx__alloc(ctx, ncons * sizeof(fx_cons_tmpl));
    if (ct == NULL)
    {
        return FX_ERR_NOMEM;
    }
    for (i = 0; i < ncons; ++i)
    {
        st = snapshot_cons(ctx, cons[i], vars, nvars, &ct[i]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    *out = ct;
    return FX_OK;
}

static fx_status scheme_build(fx_ctx *ctx, fx_var *const *vars,
                              unsigned long nvars, const fx_row *body,
                              fx_constraint *const *cons, unsigned long ncons,
                              int allow_outer, fx_scheme **out)
{
    fx_scheme *sch;
    fx_var **vp;
    fx_constraint **cp;
    fx_var_tmpl *vt;
    unsigned long *ids;
    fx_cons_tmpl *ct;
    fx_status st;
    vp = NULL;
    cp = NULL;
    vt = NULL;
    ids = NULL;
    ct = NULL;
    st = copy_var_ptrs(ctx, nvars, vars, &vp);
    if (st != FX_OK)
    {
        return st;
    }
    st = copy_cons_ptrs(ctx, ncons, cons, &cp);
    if (st != FX_OK)
    {
        return st;
    }
    st = capture_slots(ctx, nvars, vars, &vt, &ids);
    if (st != FX_OK)
    {
        return st;
    }
    sch = (fx_scheme *)fx__alloc(ctx, sizeof(fx_scheme));
    if (sch == NULL)
    {
        return FX_ERR_NOMEM;
    }
    sch->owner = ctx;
    sch->nvars = nvars;
    sch->ncons = ncons;
    sch->vars = vp;
    sch->cons = cp;
    sch->vtmpl = vt;
    sch->slot_ids = ids;
    st = capture_rows(ctx, body, vars, nvars, allow_outer, &sch->body_tmpl);
    if (st != FX_OK)
    {
        return st;
    }
    st = capture_cons_all(ctx, cons, ncons, vars, nvars, &ct);
    if (st != FX_OK)
    {
        return st;
    }
    sch->ctmpl = ct;
    sch->body = body;
    *out = sch;
    return FX_OK;
}

static fx_status check_quant_var(fx_ctx *ctx, fx_var *v)
{
    if (v == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (v->owner != ctx)
    {
        return FX_ERR_INVALID;
    }
    return FX_OK;
}

static fx_status check_all_quant_owned(fx_ctx *ctx, fx_var *const *vars,
                                       unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n; ++i)
    {
        fx_status st;
        st = check_quant_var(ctx, vars[i]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    return FX_OK;
}

static fx_status check_cons_entry(fx_constraint *c)
{
    if (c == NULL)
    {
        return FX_ERR_INVALID;
    }
    return FX_OK;
}

static fx_status check_all_cons_entries(fx_constraint *const *cons,
                                        unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n; ++i)
    {
        fx_status st;
        st = check_cons_entry(cons[i]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    return FX_OK;
}

fx_status fx_scheme_new(fx_ctx *ctx, fx_var *const *vars, unsigned long nvars,
                        const fx_row *body, fx_constraint *const *constraints,
                        unsigned long nconstraints, fx_scheme **out)
{
    fx_status st;
    int dup;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (body == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (nvars != 0)
    {
        if (vars == NULL)
        {
            return FX_ERR_INVALID;
        }
    }
    if (nconstraints != 0)
    {
        if (constraints == NULL)
        {
            return FX_ERR_INVALID;
        }
    }
    st = check_all_quant_owned(ctx, vars, nvars);
    if (st != FX_OK)
    {
        return st;
    }
    st = check_all_cons_entries(constraints, nconstraints);
    if (st != FX_OK)
    {
        return st;
    }
    dup = unique_ids(vars, nvars);
    if (dup == 0)
    {
        return FX_ERR_INVALID;
    }
    st =
        scheme_build(ctx, vars, nvars, body, constraints, nconstraints, 0, out);
    return st;
}

/* --- generalization over the live constraint list --- */

static int row_in_vars(fx_ctx *ctx, const fx_row *row, fx_var *const *gv,
                       unsigned long ngv)
{
    fx_var *v;
    int res;
    v = top_var_of(ctx, row);
    if (v == NULL)
    {
        return 1;
    }
    res = var_in(v, gv, ngv);
    return res;
}

static int cons_in_vars(fx_ctx *ctx, fx_constraint *c, fx_var *const *gv,
                        unsigned long ngv)
{
    int ok;
    int el;
    ok = 1;
    if (c->extra != NULL)
    {
        el = row_in_vars(ctx, c->extra, gv, ngv);
        if (el == 0)
        {
            ok = 0;
        }
    }
    if (ok != 0)
    {
        el = row_in_vars(ctx, c->b, gv, ngv);
        if (el == 0)
        {
            ok = 0;
        }
    }
    if (ok != 0)
    {
        el = row_in_vars(ctx, c->a, gv, ngv);
        if (el == 0)
        {
            ok = 0;
        }
    }
    return ok;
}

static int eligible_one(fx_ctx *ctx, fx_constraint *c, fx_var *const *gv,
                        unsigned long ngv)
{
    int res;
    int tk;
    res = 0;
    tk = take_one(c);
    if (tk != 0)
    {
        res = cons_in_vars(ctx, c, gv, ngv);
    }
    return res;
}

static void bump_count(unsigned long *n)
{
    *n = *n + 1u;
}

static fx_constraint *count_eligible_step(fx_ctx *ctx, fx_var *const *gv,
                                          unsigned long ngv, fx_constraint *c,
                                          unsigned long *n)
{
    int take;
    take = eligible_one(ctx, c, gv, ngv);
    if (take != 0)
    {
        bump_count(n);
    }
    return c->next;
}

static unsigned long count_eligible(fx_ctx *ctx, fx_var *const *gv,
                                    unsigned long ngv)
{
    fx_constraint *cur;
    unsigned long n;
    n = 0u;
    cur = ctx->constraints;
    while (cur != NULL)
    {
        cur = count_eligible_step(ctx, gv, ngv, cur, &n);
    }
    return n;
}

static void store_cons(fx_constraint **dst, unsigned long *idx,
                       fx_constraint *c)
{
    dst[*idx] = c;
    *idx = *idx + 1u;
}

static fx_constraint *fill_step(fx_ctx *ctx, fx_var *const *gv,
                                unsigned long ngv, fx_constraint *c,
                                fx_constraint **dst, unsigned long *idx)
{
    int take;
    take = eligible_one(ctx, c, gv, ngv);
    if (take != 0)
    {
        store_cons(dst, idx, c);
    }
    return c->next;
}

static fx_status fill_residuals(fx_ctx *ctx, fx_var *const *gv,
                                unsigned long ngv, fx_constraint **dst)
{
    fx_constraint *cur;
    unsigned long idx;
    idx = 0u;
    cur = ctx->constraints;
    while (cur != NULL)
    {
        cur = fill_step(ctx, gv, ngv, cur, dst, &idx);
    }
    return FX_OK;
}

static fx_status capture_residual(fx_ctx *ctx, fx_var *gv, fx_constraint ***res,
                                  unsigned long *count)
{
    fx_constraint **arr;
    fx_status st;
    unsigned long c;
    c = count_eligible(ctx, &gv, 1u);
    *count = c;
    if (c == 0u)
    {
        *res = NULL;
        return FX_OK;
    }
    arr = (fx_constraint **)fx__alloc(ctx, c * sizeof(fx_constraint *));
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    st = fill_residuals(ctx, &gv, 1u, arr);
    if (st != FX_OK)
    {
        return st;
    }
    *res = arr;
    return FX_OK;
}

fx_status fx_generalize(fx_ctx *ctx, const fx_row *body,
                        fx_var *const *nongeneralizable,
                        unsigned long nnongeneralizable, fx_scheme **out)
{
    fx_var *tv;
    fx_var *gv;
    fx_constraint **res;
    fx_status st;
    unsigned long ngv;
    unsigned long count;
    int excluded;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (body == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    tv = top_var_of(ctx, body);
    excluded = 0;
    if (tv != NULL)
    {
        excluded = var_in(tv, nongeneralizable, nnongeneralizable);
    }
    gv = NULL;
    if (tv != NULL)
    {
        if (excluded == 0)
        {
            gv = tv;
        }
    }
    ngv = 0u;
    if (gv != NULL)
    {
        ngv = 1u;
    }
    res = NULL;
    count = 0u;
    if (ngv != 0)
    {
        st = capture_residual(ctx, gv, &res, &count);
        if (st != FX_OK)
        {
            return st;
        }
    }
    st = scheme_build(ctx, &gv, ngv, body, res, count, 1, out);
    return st;
}

/* --- inspection --- */

unsigned long fx_scheme_var_count(const fx_scheme *scheme)
{
    if (scheme == NULL)
    {
        return 0u;
    }
    return scheme->nvars;
}

fx_var_id fx_scheme_var_id_at(const fx_scheme *scheme, unsigned long index)
{
    if (scheme == NULL)
    {
        return 0u;
    }
    if (index >= scheme->nvars)
    {
        return 0u;
    }
    return scheme->slot_ids[index];
}

const fx_row *fx_scheme_body(const fx_scheme *scheme)
{
    if (scheme == NULL)
    {
        return NULL;
    }
    return scheme->body;
}

unsigned long fx_scheme_constraint_count(const fx_scheme *scheme)
{
    if (scheme == NULL)
    {
        return 0u;
    }
    return scheme->ncons;
}

const fx_constraint *fx_scheme_constraint_at(const fx_scheme *scheme,
                                             unsigned long index)
{
    if (scheme == NULL)
    {
        return NULL;
    }
    if (index >= scheme->ncons)
    {
        return NULL;
    }
    return scheme->cons[index];
}
