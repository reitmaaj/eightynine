/* fx_solve.c - worklist main loop and open-row equality solving.
 *
 * Green structural discipline: every worker is a thin orchestrator whose
 * nested blocks hold at most one delegation; real work lives in worker
 * functions. Read-only decision helpers are listed pure in green.yaml. */

#include "fx_internal.h"

/* --- pure decision helpers (read-only) --- */

int fx__occurs_walk(fx_ctx *ctx, fx_var *root, fx_var *var)
{
    fx_var *rt;
    rt = fx__var_rep(ctx, var);
    while (rt != root)
    {
        if (rt->binding == NULL)
        {
            return 0;
        }
        rt = fx__var_rep(ctx, rt->binding->tail);
    }
    return 1;
}

int fx__occurs(fx_ctx *ctx, fx_var *root, const fx_row *row)
{
    if (row->tail == NULL)
    {
        return 0;
    }
    return fx__occurs_walk(ctx, root, row->tail);
}

/* --- small row helpers --- */

static fx_status require_on_row(fx_ctx *ctx, const fx_row *row,
                                const fx_atom *atom)
{
    unsigned long i;
    fx_var *root;
    fx_status st;
    int found;
    found = 0;
    for (i = 0; i < row->nhead; ++i)
    {
        int eq;
        eq = fx__atom_eq(row->head[i], atom);
        if (eq != 0)
        {
            found = 1;
        }
    }
    if (found != 0)
    {
        return FX_OK;
    }
    if (row->tail == NULL)
    {
        return FX_ERR_UNSAT;
    }
    root = fx__var_rep(ctx, row->tail);
    st = fx__var_require(ctx, root, atom);
    return st;
}

static fx_status forbid_on_row(fx_ctx *ctx, const fx_row *row,
                               const fx_atom *atom)
{
    unsigned long i;
    fx_var *root;
    fx_status st;
    for (i = 0; i < row->nhead; ++i)
    {
        int eq;
        eq = fx__atom_eq(row->head[i], atom);
        if (eq != 0)
        {
            return FX_ERR_UNSAT;
        }
    }
    if (row->tail == NULL)
    {
        return FX_OK;
    }
    root = fx__var_rep(ctx, row->tail);
    st = fx__var_forbid(ctx, root, atom);
    return st;
}

static fx_status apply_require(fx_ctx *ctx, const fx_row *row,
                               const fx_atom *atom, const fx_constraint *origin)
{
    fx_status st;
    st = require_on_row(ctx, row, atom);
    if (st != FX_OK)
    {
        if (st == FX_ERR_UNSAT)
        {
            fx__record_conflict(ctx, FX_CONFLICT_EQUALITY, origin, NULL);
        }
        return st;
    }
    return FX_OK;
}

static fx_status apply_forbid(fx_ctx *ctx, const fx_row *row,
                              const fx_atom *atom, const fx_constraint *origin)
{
    fx_status st;
    st = forbid_on_row(ctx, row, atom);
    if (st != FX_OK)
    {
        if (st == FX_ERR_UNSAT)
        {
            fx__record_conflict(ctx, FX_CONFLICT_EQUALITY, origin, NULL);
        }
        return st;
    }
    return FX_OK;
}

static fx_status bind_row(fx_ctx *ctx, fx_var *root, fx_row *row,
                          const fx_constraint *origin)
{
    fx_status st;
    unsigned long i;
    int cyc;
    cyc = fx__occurs(ctx, root, row);
    if (cyc != 0)
    {
        fx__record_conflict(ctx, FX_CONFLICT_OCCURS, origin, NULL);
        return FX_ERR_OCCURS;
    }
    for (i = 0; i < root->nreq; ++i)
    {
        st = apply_require(ctx, row, root->required[i], origin);
        if (st != FX_OK)
        {
            return st;
        }
    }
    for (i = 0; i < root->nforb; ++i)
    {
        st = apply_forbid(ctx, row, root->forbidden[i], origin);
        if (st != FX_OK)
        {
            return st;
        }
    }
    st = fx__trail_bind(ctx, root, root->binding);
    if (st != FX_OK)
    {
        return st;
    }
    root->binding = row;
    ctx->fact_serial = ctx->fact_serial + 1u;
    fx__solve_notify(ctx, root);
    return FX_OK;
}

static int row_has_atom(const fx_row *row, const fx_atom *atom)
{
    unsigned long i;
    for (i = 0; i < row->nhead; ++i)
    {
        int eq;
        eq = fx__atom_eq(row->head[i], atom);
        if (eq != 0)
        {
            return 1;
        }
    }
    return 0;
}

static void store_diff_atom(const fx_atom **arr, unsigned long *n,
                            const fx_atom *atom)
{
    arr[*n] = atom;
    *n = *n + 1u;
}

static void maybe_keep_diff(fx_ctx *ctx, const fx_row *other,
                            const fx_atom **arr, unsigned long *n,
                            const fx_atom *atom)
{
    int present;
    (void)ctx;
    present = row_has_atom(other, atom);
    if (present == 0)
    {
        store_diff_atom(arr, n, atom);
    }
}
static void set_diff_empty(const fx_atom ***out, unsigned long *out_n)
{
    *out = NULL;
    *out_n = 0u;
}

/* Atoms of src not present in other. Context-owned; caller frees nothing.
 * Allocation failure is reported as FX_ERR_NOMEM and is never encoded as an
 * (empty) result. */
static fx_status diff_atoms(fx_ctx *ctx, const fx_row *src, const fx_row *other,
                            const fx_atom ***out, unsigned long *out_n)
{
    const fx_atom **arr;
    unsigned long n;
    unsigned long i;
    if (src->nhead == 0u)
    {
        set_diff_empty(out, out_n);
        return FX_OK;
    }
    arr =
        (const fx_atom **)fx__alloc(ctx, src->nhead * sizeof(const fx_atom *));
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    n = 0u;
    for (i = 0; i < src->nhead; ++i)
    {
        maybe_keep_diff(ctx, other, arr, &n, src->head[i]);
    }
    *out = arr;
    *out_n = n;
    return FX_OK;
}

/* --- equality construction workers --- */

static fx_status make_row(fx_ctx *ctx, const fx_atom *const *atoms,
                          unsigned long natoms, fx_var *tail, fx_row **out)
{
    fx_row *row;
    row = fx__row_make(ctx, atoms, natoms, tail);
    if (row == NULL)
    {
        return FX_ERR_NOMEM;
    }
    *out = row;
    return FX_OK;
}

static fx_status bind_closed_tail(fx_ctx *ctx, fx_constraint *c, fx_var *tail,
                                  const fx_atom *const *atoms,
                                  unsigned long natoms)
{
    fx_row *row;
    fx_status st;
    row = NULL;
    st = make_row(ctx, atoms, natoms, NULL, &row);
    if (st != FX_OK)
    {
        return st;
    }
    st = bind_row(ctx, fx__var_rep(ctx, tail), row, c);
    if (st != FX_OK)
    {
        return st;
    }
    c->state = FX_CONSTRAINT_SATISFIED;
    return FX_OK;
}

static fx_status bind_open_shared(fx_ctx *ctx, fx_constraint *c, fx_var *t1,
                                  const fx_atom *const *rarr, unsigned long nr,
                                  fx_var *t2, const fx_atom *const *larr,
                                  unsigned long nl)
{
    fx_var *z;
    fx_row *row1;
    fx_row *row2;
    fx_status st;
    z = NULL;
    st = fx_var_new(ctx, &z);
    if (st != FX_OK)
    {
        return st;
    }
    row1 = NULL;
    st = make_row(ctx, rarr, nr, z, &row1);
    if (st != FX_OK)
    {
        return st;
    }
    st = bind_row(ctx, fx__var_rep(ctx, t1), row1, c);
    if (st != FX_OK)
    {
        return st;
    }
    row2 = NULL;
    st = make_row(ctx, larr, nl, z, &row2);
    if (st != FX_OK)
    {
        return st;
    }
    st = bind_row(ctx, fx__var_rep(ctx, t2), row2, c);
    if (st != FX_OK)
    {
        return st;
    }
    c->state = FX_CONSTRAINT_SATISFIED;
    return FX_OK;
}

/* --- equality case workers (each returns the solver outcome) --- */

static fx_status fail_eq(fx_ctx *ctx, fx_constraint *c)
{
    c->state = FX_CONSTRAINT_FAILED;
    fx__record_conflict(ctx, FX_CONFLICT_EQUALITY, c, NULL);
    return FX_ERR_UNSAT;
}

static fx_status eq_both_closed(fx_ctx *ctx, fx_constraint *c, unsigned long nl,
                                unsigned long nr)
{
    fx_status res;
    res = FX_OK;
    if (nl != 0)
    {
        res = fail_eq(ctx, c);
    }
    if (res == FX_OK)
    {
        if (nr != 0)
        {
            res = fail_eq(ctx, c);
        }
    }
    if (res == FX_OK)
    {
        c->state = FX_CONSTRAINT_SATISFIED;
    }
    return res;
}

static fx_status eq_left_closed(fx_ctx *ctx, fx_constraint *c, fx_var *t2,
                                const fx_atom *const *larr, unsigned long nl,
                                unsigned long nr)
{
    fx_status res;
    if (nr != 0)
    {
        res = fail_eq(ctx, c);
        return res;
    }
    res = bind_closed_tail(ctx, c, t2, larr, nl);
    return res;
}

static fx_status eq_right_closed(fx_ctx *ctx, fx_constraint *c, fx_var *t1,
                                 const fx_atom *const *rarr, unsigned long nr,
                                 unsigned long nl)
{
    fx_status res;
    if (nl != 0)
    {
        res = fail_eq(ctx, c);
        return res;
    }
    res = bind_closed_tail(ctx, c, t1, rarr, nr);
    return res;
}

static fx_status eq_same_tail(fx_ctx *ctx, fx_constraint *c, unsigned long nl,
                              unsigned long nr)
{
    fx_status res;
    res = FX_OK;
    if (nl != 0)
    {
        res = fail_eq(ctx, c);
    }
    if (res == FX_OK)
    {
        if (nr != 0)
        {
            res = fail_eq(ctx, c);
        }
    }
    if (res == FX_OK)
    {
        c->state = FX_CONSTRAINT_SATISFIED;
    }
    return res;
}

/* --- equality dispatcher --- */

static int equal_class(fx_ctx *ctx, fx_var *t1, fx_var *t2)
{
    if (t1 == NULL)
    {
        if (t2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    if (t2 == NULL)
    {
        return 2;
    }
    if (fx__var_rep(ctx, t1) == fx__var_rep(ctx, t2))
    {
        return 3;
    }
    return 4;
}

int fx__equal_class(fx_ctx *ctx, const fx_row *na, const fx_row *nb)
{
    int code;
    code = equal_class(ctx, na->tail, nb->tail);
    return code;
}

/* Whole-row equality over two already-normalized rows: compute the head
 * differences and the tail classes, then dispatch to the equality case
 * workers. Shared by the EQUAL processor and the closed-input JOIN step,
 * which reduces `out = left union right` to whole-row equality of the output
 * against the computed closed union. */
static fx_status equal_rows(fx_ctx *ctx, fx_constraint *c, const fx_row *na,
                            const fx_row *nb)
{
    const fx_atom **larr;
    const fx_atom **rarr;
    unsigned long nl;
    unsigned long nr;
    fx_var *t1;
    fx_var *t2;
    fx_status res;
    int code;
    larr = NULL;
    rarr = NULL;
    res = diff_atoms(ctx, na, nb, &larr, &nl);
    if (res != FX_OK)
    {
        return res;
    }
    res = diff_atoms(ctx, nb, na, &rarr, &nr);
    if (res != FX_OK)
    {
        return res;
    }
    t1 = na->tail;
    t2 = nb->tail;
    code = equal_class(ctx, t1, t2);
    res = FX_ERR_INTERNAL;
    if (code == 0)
    {
        res = eq_both_closed(ctx, c, nl, nr);
    }
    else if (code == 1)
    {
        res = eq_left_closed(ctx, c, t2, larr, nl, nr);
    }
    else if (code == 2)
    {
        res = eq_right_closed(ctx, c, t1, rarr, nr, nl);
    }
    else if (code == 3)
    {
        res = eq_same_tail(ctx, c, nl, nr);
    }
    else
    {
        res = bind_open_shared(ctx, c, t1, rarr, nr, t2, larr, nl);
    }
    return res;
}

static fx_status satisfy_equal(fx_ctx *ctx, fx_constraint *c)
{
    fx_row *na;
    fx_row *nb;
    fx_status res;
    na = fx__row_normalize(ctx, c->a);
    if (na == NULL)
    {
        return FX_ERR_NOMEM;
    }
    nb = fx__row_normalize(ctx, c->b);
    if (nb == NULL)
    {
        return FX_ERR_NOMEM;
    }
    res = equal_rows(ctx, c, na, nb);
    return res;
}

/* --- subset processor --- */

static int head_has(const fx_row *row, const fx_atom *atom)
{
    unsigned long i;
    for (i = 0; i < row->nhead; ++i)
    {
        int eq;
        eq = fx__atom_eq(row->head[i], atom);
        if (eq != 0)
        {
            return 1;
        }
    }
    return 0;
}

static fx_status require_rhs(fx_ctx *ctx, fx_constraint *c, const fx_row *rhs,
                             const fx_atom *atom)
{
    int present;
    fx_status st;
    present = head_has(rhs, atom);
    if (present != 0)
    {
        return FX_OK;
    }
    st = require_on_row(ctx, rhs, atom);
    if (st != FX_OK)
    {
        c->state = FX_CONSTRAINT_FAILED;
        return st;
    }
    return FX_OK;
}

static fx_status forbid_lhs(fx_ctx *ctx, fx_constraint *c, const fx_row *lhs,
                            const fx_atom *atom)
{
    fx_status st;
    st = forbid_on_row(ctx, lhs, atom);
    if (st != FX_OK)
    {
        c->state = FX_CONSTRAINT_FAILED;
        return st;
    }
    return FX_OK;
}

static fx_status forbid_if_absent(fx_ctx *ctx, fx_constraint *c,
                                  const fx_row *lhs, const fx_row *rhs,
                                  const fx_atom *atom)
{
    int present;
    fx_status st;
    present = head_has(rhs, atom);
    if (present != 0)
    {
        return FX_OK;
    }
    st = forbid_lhs(ctx, c, lhs, atom);
    return st;
}

static fx_status process_subset(fx_ctx *ctx, fx_constraint *c)
{
    fx_row *la;
    fx_row *rb;
    fx_var *root_l;
    fx_var *root_r;
    fx_status st;
    unsigned long i;
    la = fx__row_normalize(ctx, c->a);
    if (la == NULL)
    {
        return FX_ERR_NOMEM;
    }
    rb = fx__row_normalize(ctx, c->b);
    if (rb == NULL)
    {
        return FX_ERR_NOMEM;
    }
    for (i = 0; i < la->nhead; ++i)
    {
        st = require_rhs(ctx, c, rb, la->head[i]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    if (la->tail != NULL)
    {
        root_l = fx__var_rep(ctx, la->tail);
        for (i = 0; i < root_l->nreq; ++i)
        {
            st = require_rhs(ctx, c, rb, root_l->required[i]);
            if (st != FX_OK)
            {
                return st;
            }
        }
    }
    if (rb->tail != NULL)
    {
        root_r = fx__var_rep(ctx, rb->tail);
        for (i = 0; i < root_r->nforb; ++i)
        {
            st = forbid_if_absent(ctx, c, la, rb, root_r->forbidden[i]);
            if (st != FX_OK)
            {
                return st;
            }
        }
    }
    if (la->tail == NULL)
    {
        if (rb->tail == NULL)
        {
            c->state = FX_CONSTRAINT_SATISFIED;
            return FX_OK;
        }
    }
    c->state = FX_CONSTRAINT_PENDING;
    return FX_OK;
}

/* --- membership and lacks processors --- */

static fx_status process_member(fx_ctx *ctx, fx_constraint *c)
{
    fx_row *n;
    fx_var *root;
    fx_var *tail;
    fx_status st;
    int found;
    n = fx__row_normalize(ctx, c->b);
    if (n == NULL)
    {
        return FX_ERR_NOMEM;
    }
    found = head_has(n, c->atom);
    if (found != 0)
    {
        c->state = FX_CONSTRAINT_SATISFIED;
        return FX_OK;
    }
    tail = n->tail;
    if (tail == NULL)
    {
        c->state = FX_CONSTRAINT_FAILED;
        return FX_ERR_UNSAT;
    }
    root = fx__var_rep(ctx, tail);
    st = fx__var_require(ctx, root, c->atom);
    if (st != FX_OK)
    {
        c->state = FX_CONSTRAINT_FAILED;
        return st;
    }
    c->state = FX_CONSTRAINT_SATISFIED;
    return FX_OK;
}

static fx_status process_lacks(fx_ctx *ctx, fx_constraint *c)
{
    fx_row *n;
    fx_var *root;
    fx_var *tail;
    fx_status st;
    int found;
    n = fx__row_normalize(ctx, c->b);
    if (n == NULL)
    {
        return FX_ERR_NOMEM;
    }
    found = head_has(n, c->atom);
    if (found != 0)
    {
        c->state = FX_CONSTRAINT_FAILED;
        return FX_ERR_UNSAT;
    }
    tail = n->tail;
    if (tail == NULL)
    {
        c->state = FX_CONSTRAINT_SATISFIED;
        return FX_OK;
    }
    root = fx__var_rep(ctx, tail);
    st = fx__var_forbid(ctx, root, c->atom);
    if (st != FX_OK)
    {
        c->state = FX_CONSTRAINT_FAILED;
        return st;
    }
    c->state = FX_CONSTRAINT_SATISFIED;
    return FX_OK;
}

/* --- join processor --- */

static fx_status join_require(fx_ctx *ctx, fx_constraint *c, const fx_row *row,
                              const fx_atom *atom)
{
    fx_status st;
    st = require_on_row(ctx, row, atom);
    if (st != FX_OK)
    {
        c->state = FX_CONSTRAINT_FAILED;
        return st;
    }
    return FX_OK;
}

static fx_status join_forbid(fx_ctx *ctx, fx_constraint *c, const fx_row *row,
                             const fx_atom *atom)
{
    fx_status st;
    st = forbid_on_row(ctx, row, atom);
    if (st != FX_OK)
    {
        c->state = FX_CONSTRAINT_FAILED;
        return st;
    }
    return FX_OK;
}

/* Apply the per-atom rules of O = L union R for one atom. */
static fx_status join_rule(fx_ctx *ctx, fx_constraint *c, const fx_row *o,
                           const fx_row *l, const fx_row *r,
                           const fx_atom *atom)
{
    fx_truth mo;
    fx_truth ml;
    fx_truth mr;
    fx_status st;
    mo = fx_row_membership(ctx, o, atom);
    ml = fx_row_membership(ctx, l, atom);
    mr = fx_row_membership(ctx, r, atom);
    if (ml == FX_TRUE)
    {
        if (mo != FX_TRUE)
        {
            st = join_require(ctx, c, o, atom);
            if (st != FX_OK)
            {
                return st;
            }
        }
    }
    if (mr == FX_TRUE)
    {
        if (mo != FX_TRUE)
        {
            st = join_require(ctx, c, o, atom);
            if (st != FX_OK)
            {
                return st;
            }
        }
    }
    if (mo == FX_FALSE)
    {
        if (ml != FX_FALSE)
        {
            st = join_forbid(ctx, c, l, atom);
            if (st != FX_OK)
            {
                return st;
            }
        }
        if (mr != FX_FALSE)
        {
            st = join_forbid(ctx, c, r, atom);
            if (st != FX_OK)
            {
                return st;
            }
        }
    }
    if (ml == FX_FALSE)
    {
        if (mr == FX_FALSE)
        {
            if (mo != FX_FALSE)
            {
                st = join_forbid(ctx, c, o, atom);
                if (st != FX_OK)
                {
                    return st;
                }
            }
        }
        else
        {
            if (mo == FX_TRUE)
            {
                st = join_require(ctx, c, r, atom);
                if (st != FX_OK)
                {
                    return st;
                }
            }
        }
    }
    if (mr == FX_FALSE)
    {
        if (ml != FX_FALSE)
        {
            if (mo == FX_TRUE)
            {
                st = join_require(ctx, c, l, atom);
                if (st != FX_OK)
                {
                    return st;
                }
            }
        }
    }
    return FX_OK;
}

/* Add atom to a unique context-owned support list. Reports allocation
 * failure rather than silently truncating the support. */
static fx_status join_support(fx_ctx *ctx, const fx_atom ***arr,
                              unsigned long *n, unsigned long *cap,
                              const fx_atom *atom)
{
    unsigned long i;
    int present;
    const fx_atom **out;
    present = 0;
    for (i = 0; i < *n; ++i)
    {
        int eq;
        eq = fx__atom_eq((*arr)[i], atom);
        if (eq != 0)
        {
            present = 1;
        }
    }
    if (present != 0)
    {
        return FX_OK;
    }
    out = fx__arr_push(ctx, *arr, n, cap, atom);
    if (out == NULL)
    {
        return FX_ERR_NOMEM;
    }
    *arr = out;
    return FX_OK;
}

static fx_status join_collect(fx_ctx *ctx, const fx_row *row,
                              const fx_atom ***arr, unsigned long *n,
                              unsigned long *cap)
{
    fx_var *root;
    unsigned long i;
    fx_status st;
    for (i = 0; i < row->nhead; ++i)
    {
        st = join_support(ctx, arr, n, cap, row->head[i]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    if (row->tail != NULL)
    {
        root = fx__var_rep(ctx, row->tail);
        for (i = 0; i < root->nreq; ++i)
        {
            st = join_support(ctx, arr, n, cap, root->required[i]);
            if (st != FX_OK)
            {
                return st;
            }
        }
        for (i = 0; i < root->nforb; ++i)
        {
            st = join_support(ctx, arr, n, cap, root->forbidden[i]);
            if (st != FX_OK)
            {
                return st;
            }
        }
    }
    return FX_OK;
}

static fx_status join_closed(fx_ctx *ctx, fx_constraint *c, const fx_row *no,
                             const fx_row *nl, const fx_row *nr)
{
    const fx_atom **support;
    fx_row *u;
    unsigned long n;
    unsigned long cap;
    fx_status st;
    support = NULL;
    u = NULL;
    n = 0u;
    cap = 0u;
    st = join_collect(ctx, nl, &support, &n, &cap);
    if (st != FX_OK)
    {
        return st;
    }
    st = join_collect(ctx, nr, &support, &n, &cap);
    if (st != FX_OK)
    {
        return st;
    }
    st = make_row(ctx, support, n, NULL, &u);
    if (st != FX_OK)
    {
        return st;
    }
    /* out = normalize(left union right): whole-row equality against the
     * computed closed union, not a binding of the output tail alone. */
    st = equal_rows(ctx, c, no, u);
    return st;
}

static fx_status join_residual(fx_ctx *ctx, fx_constraint *c, const fx_row *no,
                               const fx_row *nl, const fx_row *nr)
{
    const fx_atom **support;
    unsigned long n;
    unsigned long cap;
    unsigned long i;
    fx_status st;
    support = NULL;
    n = 0u;
    cap = 0u;
    st = join_collect(ctx, no, &support, &n, &cap);
    if (st != FX_OK)
    {
        return st;
    }
    st = join_collect(ctx, nl, &support, &n, &cap);
    if (st != FX_OK)
    {
        return st;
    }
    st = join_collect(ctx, nr, &support, &n, &cap);
    if (st != FX_OK)
    {
        return st;
    }
    for (i = 0; i < n; ++i)
    {
        st = join_rule(ctx, c, no, nl, nr, support[i]);
        if (st != FX_OK)
        {
            return st;
        }
    }
    c->state = FX_CONSTRAINT_PENDING;
    return FX_OK;
}

static fx_status process_join(fx_ctx *ctx, fx_constraint *c)
{
    fx_row *no;
    fx_row *nl;
    fx_row *nr;
    fx_status st;
    no = fx__row_normalize(ctx, c->a);
    if (no == NULL)
    {
        return FX_ERR_NOMEM;
    }
    nl = fx__row_normalize(ctx, c->b);
    if (nl == NULL)
    {
        return FX_ERR_NOMEM;
    }
    nr = fx__row_normalize(ctx, c->extra);
    if (nr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    if (nl->tail == NULL)
    {
        if (nr->tail == NULL)
        {
            st = join_closed(ctx, c, no, nl, nr);
            return st;
        }
    }
    st = join_residual(ctx, c, no, nl, nr);
    return st;
}

/* --- main loop --- */

static fx_status solve_one(fx_ctx *ctx, fx_constraint *c)
{
    fx_status st;
    fx_constraint_state before;
    if (c->state != FX_CONSTRAINT_PENDING)
    {
        return FX_OK;
    }
    /* Record the entry completion state on the trail (no-op without an open
     * checkpoint). Rollback restores it, undoing any completion reached during
     * the rolled-back solve regardless of which processor ran. */
    before = c->state;
    st = fx__trail_state(ctx, c, before);
    if (st != FX_OK)
    {
        return st;
    }
    if (c->kind == FX_CONSTRAINT_EQUAL)
    {
        st = satisfy_equal(ctx, c);
        return st;
    }
    if (c->kind == FX_CONSTRAINT_MEMBER)
    {
        st = process_member(ctx, c);
        return st;
    }
    if (c->kind == FX_CONSTRAINT_LACKS)
    {
        st = process_lacks(ctx, c);
        return st;
    }
    if (c->kind == FX_CONSTRAINT_SUBSET)
    {
        st = process_subset(ctx, c);
        return st;
    }
    if (c->kind == FX_CONSTRAINT_JOIN)
    {
        st = process_join(ctx, c);
        return st;
    }
    return FX_OK;
}

/* --- worklist main loop --- */

static void enqueue(fx_ctx *ctx, fx_constraint *c)
{
    if (c->enqueued != 0)
    {
        return;
    }
    c->enqueued = 1;
    c->qnext = NULL;
    if (ctx->q_tail == NULL)
    {
        ctx->q_head = c;
    }
    else
    {
        ctx->q_tail->qnext = c;
    }
    ctx->q_tail = c;
}

static fx_constraint *dequeue(fx_ctx *ctx)
{
    fx_constraint *c;
    c = ctx->q_head;
    if (c == NULL)
    {
        return NULL;
    }
    ctx->q_head = c->qnext;
    if (ctx->q_head == NULL)
    {
        ctx->q_tail = NULL;
    }
    c->qnext = NULL;
    c->enqueued = 0;
    return c;
}

static fx_constraint *next_constraint(fx_constraint *c)
{
    return c->next;
}

static void enqueue_all_pending(fx_ctx *ctx)
{
    fx_constraint *c;
    c = ctx->constraints;
    while (c != NULL)
    {
        if (c->state == FX_CONSTRAINT_PENDING)
        {
            enqueue(ctx, c);
        }
        c = next_constraint(c);
    }
}

/* --- variable watchers (precise invalidation, D4) --- */

static fx_watch *watch_next(fx_watch *w)
{
    return w->next;
}

static int watch_has(fx_var *var, fx_constraint *c)
{
    fx_watch *w;
    w = var->watchers;
    while (w != NULL)
    {
        if (w->constraint == c)
        {
            return 1;
        }
        w = watch_next(w);
    }
    return 0;
}

static void watch_add(fx_ctx *ctx, fx_var *var, fx_constraint *c)
{
    fx_watch *w;
    int present;
    present = watch_has(var, c);
    if (present != 0)
    {
        return;
    }
    w = (fx_watch *)fx__alloc(ctx, sizeof(fx_watch));
    if (w == NULL)
    {
        return;
    }
    w->constraint = c;
    w->next = var->watchers;
    var->watchers = w;
}

/* Register a watcher on the unresolved tail of one operand row, if any. */
static void register_row(fx_ctx *ctx, fx_constraint *c, const fx_row *row)
{
    fx_row *n;
    if (row == NULL)
    {
        return;
    }
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return;
    }
    if (n->tail == NULL)
    {
        return;
    }
    watch_add(ctx, fx__var_rep(ctx, n->tail), c);
}

void fx__watch_register(fx_ctx *ctx, fx_constraint *c)
{
    register_row(ctx, c, c->a);
    register_row(ctx, c, c->b);
    register_row(ctx, c, c->extra);
}

static void fire_watcher(fx_ctx *ctx, fx_constraint *c)
{
    if (c->live != 0)
    {
        if (c->state == FX_CONSTRAINT_PENDING)
        {
            enqueue(ctx, c);
        }
    }
}

static fx_watch *fire_next_watcher(fx_ctx *ctx, fx_watch *w)
{
    fire_watcher(ctx, w->constraint);
    return w->next;
}

/* Precise invalidation: a semantic mutation on a variable fires that
 * variable's watchers, re-enqueuing the live PENDING constraints that depend
 * on its unresolved tail. Rollback-orphaned (dead) constraints are skipped. */
void fx__solve_notify(fx_ctx *ctx, fx_var *var)
{
    fx_watch *w;
    if (ctx->solving == 0)
    {
        return;
    }
    w = var->watchers;
    while (w != NULL)
    {
        w = fire_next_watcher(ctx, w);
    }
}

fx_status fx_solve(fx_ctx *ctx)
{
    fx_constraint *c;
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    ctx->solving = 1;
    enqueue_all_pending(ctx);
    for (;;)
    {
        c = dequeue(ctx);
        if (c == NULL)
        {
            break;
        }
        st = solve_one(ctx, c);
        if (st != FX_OK)
        {
            ctx->solving = 0;
            return st;
        }
        fx__watch_register(ctx, c);
    }
    ctx->solving = 0;
    return FX_OK;
}
