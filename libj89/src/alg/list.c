/* list.c - ordered finite-list algebra (element type: Json) for j89_alg.
 *
 * All structural algorithms are iterative (width-recursion-free): list
 * release, concat, map, and the right fold use explicit loops over the spine
 * with no C recursion, so a very wide flat list cannot exhaust the stack.
 */
#include "alg_internal.h"

static int list_is_nil(const struct j89a_list *xs)
{
    if (xs->tag == J89A_LIST_NIL)
    {
        return 1;
    }
    return 0;
}

void j89a_list_node_free(struct j89a_list *xs)
{
    j89a_nfree(xs->hdr.freer, xs->hdr.fctx, xs);
}

static struct j89a_list *new_list(j89a_alloc *al)
{
    struct j89a_list *n;
    n = (struct j89a_list *)j89a_malloc(al, sizeof(struct j89a_list));
    if (n == NULL)
    {
        return NULL;
    }
    n->hdr.refs = 1;
    n->hdr.freer = al->free;
    n->hdr.fctx = al->ctx;
    n->tag = J89A_LIST_NIL;
    n->head = NULL;
    n->tail = NULL;
    return n;
}

j89a_list *j89a_list_nil(j89a_alloc *al)
{
    struct j89a_list *xs;
    xs = new_list(al);
    return xs;
}

void j89a_list_retain(j89a_list *xs)
{
    if (xs == NULL)
    {
        return;
    }
    xs->hdr.refs = xs->hdr.refs + 1;
}

/* Free one spine node whose refcount is already zero; returns the next spine
 * node to free (refcount decremented), or NULL when done. */
static struct j89a_list *list_release_next(struct j89a_list *cur)
{
    struct j89a_list *next;
    if (cur->tag != J89A_LIST_CONS)
    {
        j89a_list_node_free(cur);
        return NULL;
    }
    next = cur->tail;
    j89a_json_release(cur->head);
    j89a_list_node_free(cur);
    if (next == NULL)
    {
        return NULL;
    }
    next->hdr.refs = next->hdr.refs - 1;
    if (next->hdr.refs == 0)
    {
        return next;
    }
    return NULL;
}

void j89a_list_release(j89a_list *xs)
{
    struct j89a_list *cur;
    if (xs == NULL)
    {
        return;
    }
    xs->hdr.refs = xs->hdr.refs - 1;
    if (xs->hdr.refs != 0)
    {
        return;
    }
    cur = xs;
    while (cur != NULL)
    {
        cur = list_release_next(cur);
    }
}

j89a_list *j89a_list_cons(j89a_json *head, j89a_list *tail, j89a_alloc *al)
{
    struct j89a_list *n;
    n = new_list(al);
    if (n == NULL)
    {
        return NULL;
    }
    n->tag = J89A_LIST_CONS;
    n->head = head;
    n->tail = tail;
    j89a_json_retain(head);
    if (tail != NULL)
    {
        j89a_list_retain(tail);
    }
    return n;
}

/* ---- transient pointer vector (spine work) --------------------------------
 */

typedef struct pvec
{
    void **a;
    size_t n;
    size_t cap;
    j89a_alloc *al;
} pvec;

static j89a_status pvec_grow(pvec *v)
{
    size_t nc;
    void **m;
    nc = v->cap + v->cap + 16;
    m = (void **)v->al->realloc(v->al->ctx, v->a, nc * sizeof(void *));
    if (m == NULL)
    {
        return J89A_NOMEM;
    }
    v->a = m;
    v->cap = nc;
    return J89A_OK;
}

static j89a_status pvec_push(pvec *v, void *p)
{
    j89a_status st;
    if (v->cap <= v->n)
    {
        st = pvec_grow(v);
        if (st != J89A_OK)
        {
            return st;
        }
    }
    v->a[v->n] = p;
    v->n = v->n + 1;
    return J89A_OK;
}

static void pvec_free(pvec *v)
{
    v->al->free(v->al->ctx, v->a);
}

static void fold_free_acc_vec(pvec *v, void *acc, j89a_alloc *al)
{
    if (acc != NULL)
    {
        al->free(al->ctx, acc);
    }
    pvec_free(v);
}

/* ---- iterative concat ------------------------------------------------------
 */

/* Gather one left-spine head into v; advance cur, stopping at NIL. */
static j89a_status concat_gather(pvec *v, const struct j89a_list **cur)
{
    j89a_status st;
    if ((*cur)->tag == J89A_LIST_NIL)
    {
        *cur = NULL;
        return J89A_OK;
    }
    st = pvec_push(v, (void *)(*cur)->head);
    if (st != J89A_OK)
    {
        return st;
    }
    *cur = (*cur)->tail;
    return J89A_OK;
}

/* Link one gathered head onto the front of result (popped from the back). */
static j89a_status concat_link(pvec *v, j89a_list **result, j89a_alloc *al)
{
    j89a_json *head;
    j89a_list *nx;
    v->n = v->n - 1;
    head = (j89a_json *)v->a[v->n];
    nx = j89a_list_cons(head, *result, al);
    if (nx == NULL)
    {
        j89a_list_release(*result);
        return J89A_NOMEM;
    }
    j89a_list_release(*result);
    *result = nx;
    return J89A_OK;
}

j89a_list *j89a_list_concat(j89a_list *left, j89a_list *right, j89a_alloc *al)
{
    pvec v;
    j89a_list *result;
    const struct j89a_list *cur;
    j89a_status st;
    int le;
    int re;
    le = list_is_nil(left);
    re = list_is_nil(right);
    if (le)
    {
        j89a_list_retain(right);
        return right;
    }
    if (re)
    {
        j89a_list_retain(left);
        return left;
    }
    v.a = NULL;
    v.n = 0;
    v.cap = 0;
    v.al = al;
    cur = left;
    while (cur != NULL)
    {
        st = concat_gather(&v, &cur);
        if (st != J89A_OK)
        {
            pvec_free(&v);
            return NULL;
        }
    }
    result = right;
    j89a_list_retain(right);
    while (v.n > 0)
    {
        st = concat_link(&v, &result, al);
        if (st != J89A_OK)
        {
            pvec_free(&v);
            return NULL;
        }
    }
    pvec_free(&v);
    return result;
}

/* ---- iterative right fold --------------------------------------------------
 */

static void acc_discard(const j89a_rtype *rt, void *acc, j89a_alloc *al)
{
    j89a_rdrop(rt, acc);
    al->free(al->ctx, acc);
}

static void acc_free_null(const j89a_rtype *rt, void **acc, j89a_alloc *al)
{
    if (*acc != NULL)
    {
        acc_discard(rt, *acc, al);
    }
    *acc = NULL;
}

static void fold_fail(const j89a_rtype *rt, void **acc, void *nb,
                      j89a_alloc *al)
{
    acc_free_null(rt, acc, al);
    al->free(al->ctx, nb);
}

/* Fold one element (popped from the back) onto the accumulator. */
static j89a_status fold_on_head(j89a_json *head, const j89a_rtype *rt,
                                j89a_list_step_fn step, void *ctx, void **acc,
                                j89a_alloc *al)
{
    void *nb;
    j89a_status st;
    nb = j89a_malloc(al, rt->size);
    if (nb == NULL)
    {
        acc_free_null(rt, acc, al);
        return J89A_NOMEM;
    }
    st = step(ctx, head, *acc, nb);
    if (st != J89A_OK)
    {
        fold_fail(rt, acc, nb, al);
        return st;
    }
    acc_discard(rt, *acc, al);
    *acc = nb;
    return J89A_OK;
}

/* Collect one spine node pointer into v; advance cur, stopping at NIL. */
static j89a_status fold_collect(pvec *v, const struct j89a_list **cur)
{
    j89a_status st;
    if ((*cur)->tag == J89A_LIST_NIL)
    {
        *cur = NULL;
        return J89A_OK;
    }
    st = pvec_push(v, (void *)(*cur)->head);
    if (st != J89A_OK)
    {
        return st;
    }
    *cur = (*cur)->tail;
    return J89A_OK;
}

/* Pop one collected element (from the back) and fold it. */
static j89a_status fold_pop_step(pvec *v, const j89a_rtype *rt,
                                 j89a_list_step_fn step, void *ctx, void **acc,
                                 j89a_alloc *al)
{
    j89a_json *head;
    j89a_status st;
    v->n = v->n - 1;
    head = (j89a_json *)v->a[v->n];
    st = fold_on_head(head, rt, step, ctx, acc, al);
    return st;
}

j89a_status j89a_list_fold(const j89a_list *xs, const j89a_rtype *rt,
                           const void *zero_r, j89a_list_step_fn step,
                           void *ctx, void *out_r, j89a_alloc *al)
{
    pvec v;
    void *acc;
    const struct j89a_list *cur;
    j89a_status st;
    v.a = NULL;
    v.n = 0;
    v.cap = 0;
    v.al = al;
    cur = xs;
    while (cur != NULL)
    {
        st = fold_collect(&v, &cur);
        if (st != J89A_OK)
        {
            pvec_free(&v);
            return st;
        }
    }
    acc = j89a_malloc(al, rt->size);
    if (acc == NULL)
    {
        pvec_free(&v);
        return J89A_NOMEM;
    }
    st = j89a_rcopy(rt, acc, zero_r);
    if (st != J89A_OK)
    {
        fold_free_acc_vec(&v, acc, al);
        return st;
    }
    while (v.n > 0)
    {
        st = fold_pop_step(&v, rt, step, ctx, &acc, al);
        if (st != J89A_OK)
        {
            pvec_free(&v);
            return st;
        }
    }
    st = j89a_rcopy(rt, out_r, acc);
    acc_discard(rt, acc, al);
    pvec_free(&v);
    return st;
}

/* ---- iterative map ---------------------------------------------------------
 */

static void map_abort(pvec *v)
{
    size_t i;
    for (i = 0; i < v->n; i = i + 1)
    {
        j89a_json_release((j89a_json *)v->a[i]);
    }
    pvec_free(v);
}

/* Map one spine element forward; on success the owned result is pushed. */
static j89a_status map_step(void *ctx, j89a_list_map_fn map,
                            const struct j89a_list **cur, pvec *v)
{
    j89a_json *mapped;
    j89a_status st;
    if ((*cur)->tag == J89A_LIST_NIL)
    {
        *cur = NULL;
        return J89A_OK;
    }
    st = map(ctx, (*cur)->head, &mapped);
    if (st != J89A_OK)
    {
        return st;
    }
    st = pvec_push(v, mapped);
    if (st != J89A_OK)
    {
        j89a_json_release(mapped);
        return st;
    }
    *cur = (*cur)->tail;
    return J89A_OK;
}

/* Link one mapped element (popped from the back) onto the result. */
static j89a_status map_link_fail(j89a_json *head, j89a_list **result)
{
    j89a_json_release(head);
    j89a_list_release(*result);
    *result = NULL;
    return J89A_NOMEM;
}

static j89a_status map_link(pvec *v, j89a_list **result, j89a_alloc *al)
{
    j89a_json *head;
    j89a_list *nx;
    j89a_status st;
    v->n = v->n - 1;
    head = (j89a_json *)v->a[v->n];
    nx = j89a_list_cons(head, *result, al);
    if (nx == NULL)
    {
        st = map_link_fail(head, result);
        return st;
    }
    j89a_json_release(head);
    j89a_list_release(*result);
    *result = nx;
    return J89A_OK;
}

j89a_status j89a_list_map(const j89a_list *xs, j89a_list_map_fn map, void *ctx,
                          j89a_list **out, j89a_alloc *al)
{
    pvec v;
    j89a_list *result;
    const struct j89a_list *cur;
    j89a_status st;
    int nil;
    *out = NULL;
    nil = list_is_nil(xs);
    if (nil)
    {
        result = j89a_list_nil(al);
        if (result == NULL)
        {
            return J89A_NOMEM;
        }
        *out = result;
        return J89A_OK;
    }
    v.a = NULL;
    v.n = 0;
    v.cap = 0;
    v.al = al;
    cur = xs;
    while (cur != NULL)
    {
        st = map_step(ctx, map, &cur, &v);
        if (st != J89A_OK)
        {
            map_abort(&v);
            return st;
        }
    }
    result = j89a_list_nil(al);
    if (result == NULL)
    {
        map_abort(&v);
        return J89A_NOMEM;
    }
    while (v.n > 0)
    {
        st = map_link(&v, &result, al);
        if (st != J89A_OK)
        {
            map_abort(&v);
            return J89A_NOMEM;
        }
    }
    pvec_free(&v);
    *out = result;
    return J89A_OK;
}
