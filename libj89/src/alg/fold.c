/* fold.c - json.fold catamorphism and fold-local R containers for j89_alg.
 *
 * Ownership states of every local R slot:
 *   UNINITIALIZED --successful callback/fold--> OWNED --moved into
 * rlist/robject--> MOVED Only OWNED values are passed to j89a_rdrop. After a
 * successful move the local is MOVED and must not be dropped again.
 */
#include "alg_internal.h"

static j89a_status fold_to(const j89a_json *value, const j89a_algebra *alg,
                           j89a_alloc *al, void *dst);
static void free_robject(struct j89a_robject *o, const j89a_rtype *rt,
                         j89a_alloc *al);

static int list_node_nil(const struct j89a_list *xs)
{
    if (xs == NULL)
    {
        return 1;
    }
    if (xs->tag == J89A_LIST_NIL)
    {
        return 1;
    }
    return 0;
}

/* ---- fold-local R list (ordered) ------------------------------------------
 */

static void rlist_free_node(struct j89a_rlist *node, j89a_alloc *al)
{
    al->free(al->ctx, node);
}

static void rlist_discard_raw(struct j89a_rlist *node, j89a_alloc *al)
{
    al->free(al->ctx, node->r);
    al->free(al->ctx, node);
}

static void rlist_discard_owned(struct j89a_rlist *node, const j89a_rtype *rt,
                                j89a_alloc *al)
{
    j89a_rdrop(rt, node->r);
    al->free(al->ctx, node->r);
    al->free(al->ctx, node);
}

static void free_rlist(struct j89a_rlist *head, const j89a_rtype *rt,
                       j89a_alloc *al)
{
    struct j89a_rlist *next;
    if (head == NULL)
    {
        return;
    }
    next = head->next;
    rlist_discard_owned(head, rt, al);
    free_rlist(next, rt, al);
}

static j89a_status rlist_from_list(const j89a_list *xs, const j89a_algebra *alg,
                                   j89a_alloc *al, struct j89a_rlist **out)
{
    const struct j89a_list *n;
    struct j89a_rlist *node;
    struct j89a_rlist *rest;
    j89a_status st;
    int nil;
    n = xs;
    *out = NULL;
    nil = list_node_nil(n);
    if (nil)
    {
        return J89A_OK;
    }
    node = (struct j89a_rlist *)j89a_malloc(al, sizeof(struct j89a_rlist));
    if (node == NULL)
    {
        return J89A_NOMEM;
    }
    node->r = j89a_malloc(al, alg->rt->size);
    if (node->r == NULL)
    {
        rlist_free_node(node, al);
        return J89A_NOMEM;
    }
    st = fold_to(n->head, alg, al, node->r);
    if (st != J89A_OK)
    {
        rlist_discard_raw(node, al);
        return st;
    }
    st = rlist_from_list(n->tail, alg, al, &rest);
    if (st != J89A_OK)
    {
        rlist_discard_owned(node, alg->rt, al);
        return st;
    }
    node->next = rest;
    *out = node;
    return J89A_OK;
}

static j89a_status rlist_fold_to(const struct j89a_rlist *node,
                                 const j89a_rtype *rt, const void *zero_r,
                                 j89a_rlist_step_fn step, void *ctx, void *dst,
                                 j89a_alloc *al)
{
    struct j89a_rlist *rest;
    void *buf;
    j89a_status st;
    if (node == NULL)
    {
        st = j89a_rcopy(rt, dst, zero_r);
        return st;
    }
    rest = node->next;
    buf = j89a_malloc(al, rt->size);
    if (buf == NULL)
    {
        return J89A_NOMEM;
    }
    st = rlist_fold_to(rest, rt, zero_r, step, ctx, buf, al);
    if (st != J89A_OK)
    {
        al->free(al->ctx, buf);
        return st;
    }
    st = step(ctx, node->r, buf, dst);
    j89a_rdrop(rt, buf);
    al->free(al->ctx, buf);
    return st;
}

j89a_status j89a_rlist_fold(const j89a_rlist *xs, const j89a_rtype *rt,
                            const void *zero_r, j89a_rlist_step_fn step,
                            void *ctx, void *out_r, j89a_alloc *al)
{
    j89a_status st;
    st = rlist_fold_to(xs, rt, zero_r, step, ctx, out_r, al);
    return st;
}

/* ---- fold-local R object (bag) --------------------------------------------
 */

static void robj_node_free(struct j89a_robject *node, j89a_alloc *al)
{
    al->free(al->ctx, node);
}

static void robj_discard_raw(struct j89a_robject *node, j89a_alloc *al)
{
    al->free(al->ctx, node->r);
    al->free(al->ctx, node);
}

static void robj_free_member(struct j89a_robject *o, const j89a_rtype *rt,
                             j89a_alloc *al)
{
    j89a_rdrop(rt, o->r);
    al->free(al->ctx, o->r);
    robj_node_free(o, al);
}

static void robj_free_union(struct j89a_robject *o, const j89a_rtype *rt,
                            j89a_alloc *al)
{
    free_robject(o->left, rt, al);
    free_robject(o->right, rt, al);
    robj_node_free(o, al);
}

static void free_robject(struct j89a_robject *o, const j89a_rtype *rt,
                         j89a_alloc *al)
{
    if (o == NULL)
    {
        return;
    }
    if (o->tag == J89A_ROBJ_MEMBER)
    {
        robj_free_member(o, rt, al);
    }
    else if (o->tag == J89A_ROBJ_UNION)
    {
        robj_free_union(o, rt, al);
    }
    else
    {
        robj_node_free(o, al);
    }
}

static j89a_status robj_make_member(const struct j89a_object *obj,
                                    const j89a_algebra *alg, j89a_alloc *al,
                                    struct j89a_robject **out)
{
    struct j89a_robject *node;
    j89a_status st;
    node = (struct j89a_robject *)j89a_malloc(al, sizeof(struct j89a_robject));
    if (node == NULL)
    {
        return J89A_NOMEM;
    }
    node->r = j89a_malloc(al, alg->rt->size);
    if (node->r == NULL)
    {
        robj_node_free(node, al);
        return J89A_NOMEM;
    }
    st = fold_to(obj->value, alg, al, node->r);
    if (st != J89A_OK)
    {
        robj_discard_raw(node, al);
        return st;
    }
    node->tag = J89A_ROBJ_MEMBER;
    node->name = obj->name;
    node->left = NULL;
    node->right = NULL;
    *out = node;
    return J89A_OK;
}

static void robj_free_both(struct j89a_robject *left,
                           struct j89a_robject *right, const j89a_rtype *rt,
                           j89a_alloc *al)
{
    free_robject(left, rt, al);
    free_robject(right, rt, al);
}

static j89a_status robject_from_object(const j89a_object *o,
                                       const j89a_algebra *alg, j89a_alloc *al,
                                       struct j89a_robject **out)
{
    const struct j89a_object *obj;
    struct j89a_robject *left;
    struct j89a_robject *right;
    struct j89a_robject *node;
    j89a_status st;
    obj = o;
    *out = NULL;
    if (obj->tag == J89A_OBJECT_EMPTY)
    {
        return J89A_OK;
    }
    if (obj->tag == J89A_OBJECT_MEMBER)
    {
        st = robj_make_member(obj, alg, al, out);
        return st;
    }
    st = robject_from_object(obj->left, alg, al, &left);
    if (st != J89A_OK)
    {
        return st;
    }
    st = robject_from_object(obj->right, alg, al, &right);
    if (st != J89A_OK)
    {
        free_robject(left, alg->rt, al);
        return st;
    }
    node = (struct j89a_robject *)j89a_malloc(al, sizeof(struct j89a_robject));
    if (node == NULL)
    {
        robj_free_both(left, right, alg->rt, al);
        return J89A_NOMEM;
    }
    node->tag = J89A_ROBJ_UNION;
    node->left = left;
    node->right = right;
    node->name = NULL;
    node->r = NULL;
    *out = node;
    return J89A_OK;
}

static void robj_drop_buffer(const j89a_rtype *rt, void *buf, j89a_alloc *al)
{
    j89a_rdrop(rt, buf);
    al->free(al->ctx, buf);
}

static void robj_abort_right(const j89a_rtype *rt, void *lb, void *rb,
                             j89a_alloc *al)
{
    j89a_rdrop(rt, lb);
    al->free(al->ctx, lb);
    al->free(al->ctx, rb);
}

static j89a_status robj_fold_union(const struct j89a_robject *u,
                                   const j89a_rtype *rt, const void *zero_r,
                                   j89a_robject_member_fn member_fn,
                                   j89a_object_combine_fn combine_fn, void *ctx,
                                   void *dst, j89a_alloc *al)
{
    void *lb;
    void *rb;
    j89a_status st;
    lb = j89a_malloc(al, rt->size);
    if (lb == NULL)
    {
        return J89A_NOMEM;
    }
    st = j89a_robject_fold(u->left, rt, zero_r, member_fn, combine_fn, ctx, lb,
                           al);
    if (st != J89A_OK)
    {
        al->free(al->ctx, lb);
        return st;
    }
    rb = j89a_malloc(al, rt->size);
    if (rb == NULL)
    {
        robj_drop_buffer(rt, lb, al);
        return J89A_NOMEM;
    }
    st = j89a_robject_fold(u->right, rt, zero_r, member_fn, combine_fn, ctx, rb,
                           al);
    if (st != J89A_OK)
    {
        robj_abort_right(rt, lb, rb, al);
        return st;
    }
    st = combine_fn(ctx, lb, rb, dst);
    robj_drop_buffer(rt, lb, al);
    robj_drop_buffer(rt, rb, al);
    return st;
}

j89a_status j89a_robject_fold(const j89a_robject *o, const j89a_rtype *rt,
                              const void *zero_r,
                              j89a_robject_member_fn member_fn,
                              j89a_object_combine_fn combine_fn, void *ctx,
                              void *out_r, j89a_alloc *al)
{
    j89a_status st;
    if (o == NULL)
    {
        st = j89a_rcopy(rt, out_r, zero_r);
        return st;
    }
    if (o->tag == J89A_ROBJ_MEMBER)
    {
        st = member_fn(ctx, o->name, o->r, out_r);
        return st;
    }
    st = robj_fold_union(o, rt, zero_r, member_fn, combine_fn, ctx, out_r, al);
    return st;
}

/* ---- JSON catamorphism -----------------------------------------------------
 */

static j89a_status fold_array(const struct j89a_json *v,
                              const j89a_algebra *alg, j89a_alloc *al,
                              void *dst)
{
    struct j89a_rlist *children;
    j89a_status st;
    st = rlist_from_list(v->u.l, alg, al, &children);
    if (st != J89A_OK)
    {
        return st;
    }
    st = alg->array_case(alg->ctx, (const j89a_rlist *)children, dst);
    free_rlist(children, alg->rt, al);
    return st;
}

static j89a_status fold_object(const struct j89a_json *v,
                               const j89a_algebra *alg, j89a_alloc *al,
                               void *dst)
{
    struct j89a_robject *members;
    j89a_status st;
    st = robject_from_object(v->u.o, alg, al, &members);
    if (st != J89A_OK)
    {
        return st;
    }
    st = alg->object_case(alg->ctx, (const j89a_robject *)members, dst);
    free_robject(members, alg->rt, al);
    return st;
}

static j89a_status fold_to(const j89a_json *value, const j89a_algebra *alg,
                           j89a_alloc *al, void *dst)
{
    const struct j89a_json *v;
    j89a_status st;
    v = value;
    if (v->kind == J89A_NULL)
    {
        st = alg->null_case(alg->ctx, dst);
        return st;
    }
    if (v->kind == J89A_BOOLEAN)
    {
        st = alg->boolean_case(alg->ctx, v->u.b, dst);
        return st;
    }
    if (v->kind == J89A_NUMBER)
    {
        st = alg->number_case(alg->ctx, v->u.n, dst);
        return st;
    }
    if (v->kind == J89A_STRING)
    {
        st = alg->string_case(alg->ctx, v->u.s, dst);
        return st;
    }
    if (v->kind == J89A_ARRAY)
    {
        st = fold_array(v, alg, al, dst);
        return st;
    }
    st = fold_object(v, alg, al, dst);
    return st;
}

j89a_status j89a_json_fold(const j89a_json *value, const j89a_algebra *alg,
                           void *out_r, j89a_alloc *al)
{
    j89a_status st;
    st = fold_to(value, alg, al, out_r);
    return st;
}
