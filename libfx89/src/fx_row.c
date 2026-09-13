/* fx_row.c - effect rows: construction, normalization, inspection. */
#include <stdlib.h>
#include <string.h>

#include "fx_internal.h"

/* --- canonical ordering of a head --- */

static int atom_ptr_cmp(const void *pa, const void *pb)
{
    const fx_atom *a;
    const fx_atom *b;
    a = *(const fx_atom *const *)pa;
    b = *(const fx_atom *const *)pb;
    if (fx__atom_less(a, b))
    {
        return -1;
    }
    if (fx__atom_less(b, a))
    {
        return 1;
    }
    return 0;
}

static unsigned long store_keep(const fx_atom **arr, unsigned long m,
                                const fx_atom *atom)
{
    arr[m] = atom;
    m = m + 1u;
    return m;
}

static unsigned long compact_step(const fx_atom **arr, unsigned long m,
                                  const fx_atom *atom)
{
    int dup;
    int eqflag;
    dup = 0;
    if (m != 0)
    {
        eqflag = fx__atom_eq(atom, arr[m - 1u]);
        if (eqflag != 0)
        {
            dup = 1;
        }
    }
    if (dup == 0)
    {
        m = store_keep(arr, m, atom);
    }
    return m;
}

static unsigned long compact_unique(const fx_atom **arr, unsigned long n)
{
    unsigned long m;
    unsigned long i;
    m = 0u;
    for (i = 0; i < n; ++i)
    {
        m = compact_step(arr, m, arr[i]);
    }
    return m;
}

static void set_empty_head(const fx_atom ***out_head, unsigned long *out_count)
{
    *out_head = NULL;
    *out_count = 0u;
}

static fx_status build_head(fx_ctx *ctx, const fx_atom *const *atoms,
                            unsigned long natoms, const fx_atom ***out_head,
                            unsigned long *out_count)
{
    const fx_atom **work;
    if (natoms == 0u)
    {
        set_empty_head(out_head, out_count);
        return FX_OK;
    }
    work = (const fx_atom **)fx__alloc(ctx, natoms * sizeof(const fx_atom *));
    if (work == NULL)
    {
        return FX_ERR_NOMEM;
    }
    memcpy(work, atoms, natoms * sizeof(const fx_atom *));
    qsort(work, natoms, sizeof(const fx_atom *), atom_ptr_cmp);
    *out_count = compact_unique(work, natoms);
    *out_head = work;
    return FX_OK;
}

/* --- row object construction --- */

struct fx_row *fx__row_make(fx_ctx *ctx, const fx_atom *const *atoms,
                            unsigned long natoms, struct fx_var *tail)
{
    fx_row *row;
    const fx_atom **head;
    unsigned long count;
    fx_status st;
    st = build_head(ctx, atoms, natoms, &head, &count);
    if (st != FX_OK)
    {
        return NULL;
    }
    row = (fx_row *)fx__alloc(ctx, sizeof(fx_row));
    if (row == NULL)
    {
        return NULL;
    }
    row->owner = ctx;
    row->head = head;
    row->nhead = count;
    row->tail = tail;
    return row;
}

/* --- accumulation into a growing temporary head --- */

static int push_one(fx_ctx *ctx, const fx_atom ***acc, unsigned long *n,
                    unsigned long *cap, const fx_atom *atom)
{
    const fx_atom **p;
    p = fx__arr_push(ctx, *acc, n, cap, atom);
    if (p == NULL)
    {
        return 0;
    }
    *acc = p;
    return 1;
}

static int push_head(fx_ctx *ctx, const fx_row *row, const fx_atom ***acc,
                     unsigned long *n, unsigned long *cap)
{
    unsigned long i;
    int ok;
    for (i = 0; i < row->nhead; ++i)
    {
        ok = push_one(ctx, acc, n, cap, row->head[i]);
        if (ok == 0)
        {
            return 0;
        }
    }
    return 1;
}

static void finish_fold(fx_var **tailp, int *go, fx_var *tail)
{
    *tailp = tail;
    *go = 0;
}

/* Dereference one bound level; pushes that binding's head atoms. Returns 0 on
 * out-of-memory. */
static int fold_step(fx_ctx *ctx, fx_var **tailp, const fx_atom ***acc,
                     unsigned long *n, unsigned long *cap, int *go)
{
    fx_var *cur;
    fx_var *rt;
    fx_row *b;
    int ok;
    cur = *tailp;
    rt = fx__var_rep(ctx, cur);
    if (rt->binding == NULL)
    {
        finish_fold(tailp, go, rt);
        return 1;
    }
    b = rt->binding;
    ok = push_head(ctx, b, acc, n, cap);
    if (ok == 0)
    {
        return 0;
    }
    if (b->tail == NULL)
    {
        finish_fold(tailp, go, NULL);
        return 1;
    }
    *tailp = b->tail;
    return 1;
}

static int fold_all(fx_ctx *ctx, fx_var **tailp, const fx_atom ***acc,
                    unsigned long *n, unsigned long *cap)
{
    int go;
    int ok;
    go = 1;
    while (go != 0)
    {
        ok = fold_step(ctx, tailp, acc, n, cap, &go);
        if (ok == 0)
        {
            return 0;
        }
    }
    return 1;
}

static int finalize_head(fx_ctx *ctx, const fx_atom **acc, unsigned long n,
                         const fx_atom ***out_head, unsigned long *out_count)
{
    if (n == 0u)
    {
        set_empty_head(out_head, out_count);
        return 1;
    }
    qsort(acc, n, sizeof(const fx_atom *), atom_ptr_cmp);
    *out_count = compact_unique(acc, n);
    *out_head = acc;
    (void)ctx;
    return 1;
}

struct fx_row *fx__row_normalize(fx_ctx *ctx, const fx_row *row)
{
    const fx_atom **acc;
    const fx_atom **head;
    unsigned long n;
    unsigned long cap;
    unsigned long count;
    fx_var *tail;
    fx_row *res;
    int ok;
    acc = NULL;
    n = 0u;
    cap = 0u;
    ok = push_head(ctx, row, &acc, &n, &cap);
    if (ok == 0)
    {
        return NULL;
    }
    tail = row->tail;
    if (tail != NULL)
    {
        ok = fold_all(ctx, &tail, &acc, &n, &cap);
        if (ok == 0)
        {
            return NULL;
        }
    }
    ok = finalize_head(ctx, acc, n, &head, &count);
    if (ok == 0)
    {
        return NULL;
    }
    res = (fx_row *)fx__alloc(ctx, sizeof(fx_row));
    if (res == NULL)
    {
        return NULL;
    }
    res->owner = ctx;
    res->head = head;
    res->nhead = count;
    res->tail = tail;
    return res;
}

static int forbid_one(fx_ctx *ctx, fx_var *root, const fx_atom *atom)
{
    fx_status st;
    st = fx__var_forbid(ctx, root, atom);
    if (st != FX_OK)
    {
        return 0;
    }
    return 1;
}

static int forbid_head_from_tail(fx_ctx *ctx, const fx_row *row)
{
    fx_var *root;
    fx_var *tail;
    unsigned long i;
    int ok;
    if (row->tail == NULL)
    {
        return 1;
    }
    tail = row->tail;
    root = fx__var_rep(ctx, tail);
    for (i = 0; i < row->nhead; ++i)
    {
        ok = forbid_one(ctx, root, row->head[i]);
        if (ok == 0)
        {
            return 0;
        }
    }
    return 1;
}

static fx_status make_normalized(fx_ctx *ctx, const fx_row *row,
                                 const fx_row **out)
{
    fx_row *n;
    fx_status st;
    int ok;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (row == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return FX_ERR_NOMEM;
    }
    ok = forbid_head_from_tail(ctx, n);
    if (ok == 0)
    {
        st = FX_ERR_UNSAT;
        return st;
    }
    *out = n;
    return FX_OK;
}

/* --- public row constructors --- */

fx_status fx_row_empty(fx_ctx *ctx, const fx_row **out)
{
    fx_row *row;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    row = fx__row_make(ctx, NULL, 0u, NULL);
    if (row == NULL)
    {
        return FX_ERR_NOMEM;
    }
    *out = row;
    return FX_OK;
}

fx_status fx_row_var(fx_ctx *ctx, fx_var *var, const fx_row **out)
{
    fx_row *row;
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
    if (var->owner != ctx)
    {
        return FX_ERR_INVALID;
    }
    row = fx__row_make(ctx, NULL, 0u, var);
    if (row == NULL)
    {
        return FX_ERR_NOMEM;
    }
    *out = row;
    return FX_OK;
}

static int atoms_valid(const fx_atom *const *atoms, unsigned long natoms)
{
    unsigned long i;
    for (i = 0; i < natoms; ++i)
    {
        if (atoms[i] == NULL)
        {
            return 0;
        }
    }
    return 1;
}

fx_status fx_row_closed(fx_ctx *ctx, const fx_atom *const *atoms,
                        unsigned long natoms, const fx_row **out)
{
    fx_row *row;
    int valid;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (natoms != 0)
    {
        valid = atoms_valid(atoms, natoms);
        if (valid == 0)
        {
            return FX_ERR_INVALID;
        }
    }
    row = fx__row_make(ctx, atoms, natoms, NULL);
    if (row == NULL)
    {
        return FX_ERR_NOMEM;
    }
    *out = row;
    return FX_OK;
}

fx_status fx_row_open(fx_ctx *ctx, const fx_atom *const *atoms,
                      unsigned long natoms, fx_var *tail, const fx_row **out)
{
    fx_row *row;
    int ok;
    int valid;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (tail == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (tail->owner != ctx)
    {
        return FX_ERR_INVALID;
    }
    if (natoms != 0)
    {
        valid = atoms_valid(atoms, natoms);
        if (valid == 0)
        {
            return FX_ERR_INVALID;
        }
    }
    row = fx__row_make(ctx, atoms, natoms, tail);
    if (row == NULL)
    {
        return FX_ERR_NOMEM;
    }
    ok = forbid_head_from_tail(ctx, row);
    if (ok == 0)
    {
        return FX_ERR_UNSAT;
    }
    *out = row;
    return FX_OK;
}

fx_status fx_row_extend(fx_ctx *ctx, const fx_row *base, const fx_atom *atom,
                        const fx_row **out)
{
    const fx_row *nb;
    const fx_atom **arr;
    fx_row *res;
    unsigned long n;
    int ok;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (base == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (atom == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    nb = fx__row_normalize(ctx, base);
    if (nb == NULL)
    {
        return FX_ERR_NOMEM;
    }
    n = nb->nhead + 1u;
    arr = (const fx_atom **)fx__alloc(ctx, n * sizeof(const fx_atom *));
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    if (nb->nhead != 0)
    {
        memcpy(arr, nb->head, nb->nhead * sizeof(const fx_atom *));
    }
    arr[nb->nhead] = atom;
    res = fx__row_make(ctx, arr, n, nb->tail);
    if (res == NULL)
    {
        return FX_ERR_NOMEM;
    }
    ok = forbid_head_from_tail(ctx, res);
    if (ok == 0)
    {
        return FX_ERR_UNSAT;
    }
    *out = res;
    return FX_OK;
}

fx_status fx_row_normalize(fx_ctx *ctx, const fx_row *row, const fx_row **out)
{
    fx_status st;
    st = make_normalized(ctx, row, out);
    return st;
}

unsigned long fx_row_atom_count(fx_ctx *ctx, const fx_row *row)
{
    const fx_row *n;
    if (ctx == NULL)
    {
        return 0u;
    }
    if (row == NULL)
    {
        return 0u;
    }
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return 0u;
    }
    return n->nhead;
}

const fx_atom *fx_row_atom_at(fx_ctx *ctx, const fx_row *row,
                              unsigned long index)
{
    const fx_row *n;
    if (ctx == NULL)
    {
        return NULL;
    }
    if (row == NULL)
    {
        return NULL;
    }
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return NULL;
    }
    if (index >= n->nhead)
    {
        return NULL;
    }
    return n->head[index];
}

int fx_row_is_open(fx_ctx *ctx, const fx_row *row)
{
    const fx_row *n;
    if (ctx == NULL)
    {
        return 0;
    }
    if (row == NULL)
    {
        return 0;
    }
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return 0;
    }
    if (n->tail == NULL)
    {
        return 0;
    }
    return 1;
}

fx_var *fx_row_tail(fx_ctx *ctx, const fx_row *row)
{
    const fx_row *n;
    if (ctx == NULL)
    {
        return NULL;
    }
    if (row == NULL)
    {
        return NULL;
    }
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return NULL;
    }
    return n->tail;
}

static int head_bytes_equal(const fx_row *a, const fx_row *b)
{
    unsigned long bytes;
    int cmp;
    if (a->nhead == 0u)
    {
        return 1;
    }
    bytes = a->nhead * sizeof(const fx_atom *);
    cmp = memcmp(a->head, b->head, bytes);
    if (cmp != 0)
    {
        return 0;
    }
    return 1;
}

static int head_equal(const fx_row *a, const fx_row *b)
{
    int res;
    int eq;
    res = 1;
    if (a->nhead != b->nhead)
    {
        res = 0;
    }
    if (res != 0)
    {
        eq = head_bytes_equal(a, b);
        if (eq == 0)
        {
            res = 0;
        }
    }
    return res;
}

static int tail_equal(fx_ctx *ctx, fx_var *ta, fx_var *tb)
{
    int res;
    res = 1;
    if (ta == NULL)
    {
        if (tb != NULL)
        {
            res = 0;
        }
    }
    else
    {
        if (tb == NULL)
        {
            res = 0;
        }
        else
        {
            if (fx__var_rep(ctx, ta) != fx__var_rep(ctx, tb))
            {
                res = 0;
            }
        }
    }
    return res;
}

int fx_row_equal(fx_ctx *ctx, const fx_row *a, const fx_row *b)
{
    const fx_row *na;
    const fx_row *nb;
    int eq;
    int heads;
    if (ctx == NULL)
    {
        return 0;
    }
    if (a == NULL)
    {
        return 0;
    }
    if (b == NULL)
    {
        return 0;
    }
    na = fx__row_normalize(ctx, a);
    nb = fx__row_normalize(ctx, b);
    if (na == NULL)
    {
        return 0;
    }
    if (nb == NULL)
    {
        return 0;
    }
    heads = head_equal(na, nb);
    if (heads == 0)
    {
        return 0;
    }
    eq = tail_equal(ctx, na->tail, nb->tail);
    return eq;
}

fx_truth fx_row_membership(fx_ctx *ctx, const fx_row *row, const fx_atom *atom)
{
    const fx_row *n;
    fx_var *root;
    fx_var *tail;
    unsigned long i;
    if (ctx == NULL)
    {
        return FX_UNKNOWN;
    }
    if (row == NULL)
    {
        return FX_UNKNOWN;
    }
    if (atom == NULL)
    {
        return FX_UNKNOWN;
    }
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return FX_UNKNOWN;
    }
    for (i = 0; i < n->nhead; ++i)
    {
        int eq;
        eq = fx__atom_eq(n->head[i], atom);
        if (eq != 0)
        {
            return FX_TRUE;
        }
    }
    tail = n->tail;
    if (tail == NULL)
    {
        return FX_FALSE;
    }
    root = fx__var_rep(ctx, tail);
    if (fx__var_has_required(root, atom) != 0)
    {
        return FX_TRUE;
    }
    if (fx__var_has_forbidden(root, atom) != 0)
    {
        return FX_FALSE;
    }
    return FX_UNKNOWN;
}

fx_truth fx_row_is_pure(fx_ctx *ctx, const fx_row *row)
{
    const fx_row *n;
    fx_var *root;
    if (ctx == NULL)
    {
        return FX_UNKNOWN;
    }
    if (row == NULL)
    {
        return FX_UNKNOWN;
    }
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return FX_UNKNOWN;
    }
    if (n->nhead != 0u)
    {
        return FX_FALSE;
    }
    if (n->tail == NULL)
    {
        return FX_TRUE;
    }
    root = fx__var_rep(ctx, n->tail);
    if (root->nreq != 0u)
    {
        return FX_FALSE;
    }
    return FX_UNKNOWN;
}

fx_truth fx_row_is_closed(fx_ctx *ctx, const fx_row *row)
{
    const fx_row *n;
    if (ctx == NULL)
    {
        return FX_UNKNOWN;
    }
    if (row == NULL)
    {
        return FX_UNKNOWN;
    }
    n = fx__row_normalize(ctx, row);
    if (n == NULL)
    {
        return FX_UNKNOWN;
    }
    if (n->tail == NULL)
    {
        return FX_TRUE;
    }
    return FX_UNKNOWN;
}

static int row_closed_pure(const fx_row *row)
{
    if (row->nhead != 0u)
    {
        return 0;
    }
    if (row->tail != NULL)
    {
        return 0;
    }
    return 1;
}

static fx_status union_closed(fx_ctx *ctx, const fx_row *na, const fx_row *nb,
                              const fx_row **out)
{
    const fx_atom **arr;
    fx_row *u;
    unsigned long n;
    n = na->nhead + nb->nhead;
    arr = (const fx_atom **)fx__alloc(ctx, n * sizeof(const fx_atom *));
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    if (na->nhead != 0)
    {
        memcpy(arr, na->head, na->nhead * sizeof(const fx_atom *));
    }
    if (nb->nhead != 0)
    {
        memcpy(arr + na->nhead, nb->head, nb->nhead * sizeof(const fx_atom *));
    }
    u = fx__row_make(ctx, arr, n, NULL);
    if (u == NULL)
    {
        return FX_ERR_NOMEM;
    }
    *out = u;
    return FX_OK;
}

fx_status fx_row_union(fx_ctx *ctx, const fx_row *a, const fx_row *b,
                       const fx_row **out)
{
    const fx_row *na;
    const fx_row *nb;
    fx_var *z;
    const fx_row *o;
    fx_status st;
    int pa;
    int pb;
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
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    na = fx__row_normalize(ctx, a);
    if (na == NULL)
    {
        return FX_ERR_NOMEM;
    }
    nb = fx__row_normalize(ctx, b);
    if (nb == NULL)
    {
        return FX_ERR_NOMEM;
    }
    pa = row_closed_pure(na);
    if (pa != 0)
    {
        *out = b;
        return FX_OK;
    }
    pb = row_closed_pure(nb);
    if (pb != 0)
    {
        *out = a;
        return FX_OK;
    }
    if (na->tail == NULL)
    {
        if (nb->tail == NULL)
        {
            st = union_closed(ctx, na, nb, out);
            return st;
        }
    }
    z = NULL;
    st = fx_var_new(ctx, &z);
    if (st != FX_OK)
    {
        return st;
    }
    o = NULL;
    st = fx_row_var(ctx, z, &o);
    if (st != FX_OK)
    {
        return st;
    }
    st = fx_require_join(ctx, o, a, b, NULL);
    if (st != FX_OK)
    {
        return st;
    }
    *out = o;
    return FX_OK;
}

static void set_free_none(fx_var ***out, unsigned long *count)
{
    *out = NULL;
    *count = 0u;
}

static fx_status free_vars_one(fx_ctx *ctx, const fx_row *nb, fx_var ***out,
                               unsigned long *count)
{
    fx_var **arr;
    if (nb->tail == NULL)
    {
        set_free_none(out, count);
        return FX_OK;
    }
    arr = (fx_var **)fx__alloc(ctx, sizeof(fx_var *));
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    arr[0] = nb->tail;
    *out = arr;
    *count = 1u;
    return FX_OK;
}

fx_status fx_row_free_vars(fx_ctx *ctx, const fx_row *row, fx_var ***out_vars,
                           unsigned long *out_count)
{
    const fx_row *nb;
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (row == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out_vars == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out_count == NULL)
    {
        return FX_ERR_INVALID;
    }
    nb = fx__row_normalize(ctx, row);
    if (nb == NULL)
    {
        return FX_ERR_NOMEM;
    }
    st = free_vars_one(ctx, nb, out_vars, out_count);
    return st;
}

static int row_has(const fx_row *nb, const fx_atom *atom)
{
    unsigned long i;
    for (i = 0; i < nb->nhead; ++i)
    {
        int eq;
        eq = fx__atom_eq(nb->head[i], atom);
        if (eq != 0)
        {
            return 1;
        }
    }
    return 0;
}

static unsigned long atom_index(const fx_row *nb, const fx_atom *atom)
{
    unsigned long i;
    for (i = 0; i < nb->nhead; ++i)
    {
        int eq;
        eq = fx__atom_eq(nb->head[i], atom);
        if (eq != 0)
        {
            return i;
        }
    }
    return nb->nhead;
}

static fx_status build_without(fx_ctx *ctx, const fx_row *nb,
                               const fx_atom *atom, const fx_row **out)
{
    const fx_atom **arr;
    fx_row *r;
    unsigned long idx;
    unsigned long total;
    total = nb->nhead - 1u;
    arr = (const fx_atom **)fx__alloc(ctx, total * sizeof(const fx_atom *));
    if (arr == NULL)
    {
        return FX_ERR_NOMEM;
    }
    idx = atom_index(nb, atom);
    if (idx != 0)
    {
        memcpy(arr, nb->head, idx * sizeof(const fx_atom *));
    }
    if (idx < total)
    {
        memcpy(arr + idx, nb->head + idx + 1u,
               (total - idx) * sizeof(const fx_atom *));
    }
    r = fx__row_make(ctx, arr, total, nb->tail);
    if (r == NULL)
    {
        return FX_ERR_NOMEM;
    }
    *out = r;
    return FX_OK;
}

fx_status fx_row_remove(fx_ctx *ctx, const fx_row *row, const fx_atom *atom,
                        const fx_row **out)
{
    const fx_row *nb;
    fx_var *root;
    fx_status st;
    int present;
    int absent;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (row == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (atom == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    nb = fx__row_normalize(ctx, row);
    if (nb == NULL)
    {
        return FX_ERR_NOMEM;
    }
    present = row_has(nb, atom);
    if (present != 0)
    {
        st = build_without(ctx, nb, atom, out);
        return st;
    }
    if (nb->tail == NULL)
    {
        *out = nb;
        return FX_OK;
    }
    root = fx__var_rep(ctx, nb->tail);
    absent = fx__var_has_forbidden(root, atom);
    if (absent != 0)
    {
        *out = nb;
        return FX_OK;
    }
    return FX_ERR_UNSUPPORTED;
}

fx_status fx_row_remove_many(fx_ctx *ctx, const fx_row *row,
                             const fx_atom *const *atoms, unsigned long natoms,
                             const fx_row **out)
{
    const fx_row *cur;
    fx_status st;
    unsigned long i;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (row == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (natoms != 0)
    {
        if (atoms == NULL)
        {
            return FX_ERR_INVALID;
        }
    }
    cur = row;
    for (i = 0; i < natoms; ++i)
    {
        st = fx_row_remove(ctx, cur, atoms[i], &cur);
        if (st != FX_OK)
        {
            return st;
        }
    }
    *out = cur;
    return FX_OK;
}

fx_truth fx_check_lacks(fx_ctx *ctx, const fx_atom *atom, const fx_row *row)
{
    fx_truth m;
    if (ctx == NULL)
    {
        return FX_UNKNOWN;
    }
    if (atom == NULL)
    {
        return FX_UNKNOWN;
    }
    if (row == NULL)
    {
        return FX_UNKNOWN;
    }
    m = fx_row_membership(ctx, row, atom);
    if (m == FX_TRUE)
    {
        return FX_FALSE;
    }
    if (m == FX_FALSE)
    {
        return FX_TRUE;
    }
    return FX_UNKNOWN;
}

static fx_truth check_lhs_in_rhs(fx_ctx *ctx, const fx_row *lhs,
                                 const fx_row *rhs)
{
    unsigned long i;
    fx_truth acc;
    fx_truth got;
    int unknown;
    acc = FX_TRUE;
    unknown = 0;
    for (i = 0; i < lhs->nhead; ++i)
    {
        got = fx_row_membership(ctx, rhs, lhs->head[i]);
        if (got == FX_FALSE)
        {
            return FX_FALSE;
        }
        if (got == FX_UNKNOWN)
        {
            unknown = 1;
        }
    }
    if (unknown != 0)
    {
        acc = FX_UNKNOWN;
    }
    return acc;
}

static int open_rows_equal(fx_ctx *ctx, const fx_row *ls, const fx_row *rs)
{
    int he;
    int te;
    he = head_equal(ls, rs);
    if (he == 0)
    {
        return 0;
    }
    te = tail_equal(ctx, ls->tail, rs->tail);
    return te;
}

fx_truth fx_check_subset(fx_ctx *ctx, const fx_row *subset,
                         const fx_row *superset)
{
    const fx_row *ls;
    const fx_row *rs;
    fx_truth res;
    int reflexive;
    if (ctx == NULL)
    {
        return FX_UNKNOWN;
    }
    if (subset == NULL)
    {
        return FX_UNKNOWN;
    }
    if (superset == NULL)
    {
        return FX_UNKNOWN;
    }
    ls = fx__row_normalize(ctx, subset);
    rs = fx__row_normalize(ctx, superset);
    if (ls == NULL)
    {
        return FX_UNKNOWN;
    }
    if (rs == NULL)
    {
        return FX_UNKNOWN;
    }
    res = check_lhs_in_rhs(ctx, ls, rs);
    if (res == FX_FALSE)
    {
        return FX_FALSE;
    }
    if (ls->tail != NULL)
    {
        reflexive = open_rows_equal(ctx, ls, rs);
        if (reflexive != 0)
        {
            return FX_TRUE;
        }
        return FX_UNKNOWN;
    }
    return res;
}
