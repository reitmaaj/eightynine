/* object.c - bag-of-member algebra (Object<S, Json>) for j89_alg. */
#include <stdlib.h>

#include "alg_internal.h"

void j89a_object_node_free(struct j89a_object *o)
{
    j89a_nfree(o->hdr.freer, o->hdr.fctx, o);
}

static struct j89a_object *new_object(j89a_alloc *al)
{
    struct j89a_object *o;
    o = (struct j89a_object *)j89a_malloc(al, sizeof(struct j89a_object));
    if (o == NULL)
    {
        return NULL;
    }
    o->hdr.refs = 1;
    o->hdr.freer = al->free;
    o->hdr.fctx = al->ctx;
    o->tag = J89A_OBJECT_EMPTY;
    o->name = NULL;
    o->value = NULL;
    o->left = NULL;
    o->right = NULL;
    return o;
}

j89a_object *j89a_object_empty(j89a_alloc *al)
{
    struct j89a_object *o;
    o = new_object(al);
    return o;
}

void j89a_object_retain(j89a_object *o)
{
    if (o == NULL)
    {
        return;
    }
    o->hdr.refs = o->hdr.refs + 1;
}

static void release_member(struct j89a_object *o)
{
    j89a_str_release(o->name);
    j89a_json_release(o->value);
}

/* ---- iterative object release ---------------------------------------------
 */

typedef struct objstack
{
    struct j89a_object **a;
    size_t n;
    size_t cap;
} objstack;

static int objstack_grow(objstack *s)
{
    size_t nc;
    struct j89a_object **m;
    nc = s->cap + s->cap + 16;
    m = (struct j89a_object **)realloc(s->a, nc * sizeof(struct j89a_object *));
    if (m == NULL)
    {
        return 0;
    }
    s->a = m;
    s->cap = nc;
    return 1;
}

static int objstack_push(objstack *s, struct j89a_object *o)
{
    int g;
    if (s->cap <= s->n)
    {
        g = objstack_grow(s);
        if (!g)
        {
            return 0;
        }
    }
    s->a[s->n] = o;
    s->n = s->n + 1;
    return 1;
}

/* Decrement one child's refcount; if it reaches zero push it for processing. */
static void obj_dec_push(objstack *s, struct j89a_object *child)
{
    child->hdr.refs = child->hdr.refs - 1;
    if (child->hdr.refs == 0)
    {
        objstack_push(s, child);
    }
}

static void member_free(struct j89a_object *o)
{
    release_member(o);
    j89a_object_node_free(o);
}

static void union_free(struct j89a_object *o, objstack *s)
{
    obj_dec_push(s, o->left);
    obj_dec_push(s, o->right);
    j89a_object_node_free(o);
}

/* Process one object whose refcount already reached zero. Returns 1 when more
 * nodes remain, 0 when the stack is empty. */
static int obj_release_step(objstack *s)
{
    struct j89a_object *o;
    o = s->a[s->n - 1];
    s->n = s->n - 1;
    if (o->tag == J89A_OBJECT_MEMBER)
    {
        member_free(o);
    }
    else if (o->tag == J89A_OBJECT_UNION)
    {
        union_free(o, s);
    }
    else
    {
        j89a_object_node_free(o);
    }
    if (s->n != 0)
    {
        return 1;
    }
    return 0;
}

void j89a_object_release(j89a_object *o)
{
    objstack s;
    if (o == NULL)
    {
        return;
    }
    o->hdr.refs = o->hdr.refs - 1;
    if (o->hdr.refs != 0)
    {
        return;
    }
    s.a = NULL;
    s.n = 0;
    s.cap = 0;
    objstack_push(&s, o);
    while (s.n != 0)
    {
        obj_release_step(&s);
    }
    free(s.a);
}

j89a_object *j89a_object_member(j89a_str *name, j89a_json *value,
                                j89a_alloc *al)
{
    struct j89a_object *o;
    o = new_object(al);
    if (o == NULL)
    {
        return NULL;
    }
    o->tag = J89A_OBJECT_MEMBER;
    o->name = name;
    o->value = value;
    j89a_str_retain(name);
    j89a_json_retain(value);
    return o;
}

j89a_object *j89a_object_union(j89a_object *left, j89a_object *right,
                               j89a_alloc *al)
{
    struct j89a_object *o;
    o = new_object(al);
    if (o == NULL)
    {
        return NULL;
    }
    o->tag = J89A_OBJECT_UNION;
    o->left = left;
    o->right = right;
    j89a_object_retain(left);
    j89a_object_retain(right);
    return o;
}

static void abort_member(j89a_str *new_name)
{
    j89a_str_release(new_name);
}

static j89a_status obj_map_member(const j89a_object *o,
                                  j89a_object_name_fn name_map,
                                  j89a_object_value_fn value_map, void *ctx,
                                  j89a_object **out, j89a_alloc *al)
{
    j89a_str *new_name;
    j89a_json *new_value;
    j89a_object *mapped;
    j89a_status st;
    *out = NULL;
    new_name = name_map(ctx, o->name);
    if (new_name == NULL)
    {
        return J89A_NOMEM;
    }
    st = value_map(ctx, o->value, &new_value);
    if (st != J89A_OK)
    {
        abort_member(new_name);
        return st;
    }
    mapped = j89a_object_member(new_name, new_value, al);
    abort_member(new_name);
    j89a_json_release(new_value);
    if (mapped == NULL)
    {
        return J89A_NOMEM;
    }
    *out = mapped;
    return J89A_OK;
}

/* ---- iterative object map --------------------------------------------------
 */

typedef struct mfvec
{
    const struct j89a_object **a;
    size_t n;
    size_t cap;
    j89a_alloc *al;
} mfvec;

typedef struct mrvec
{
    struct j89a_object **a;
    size_t n;
    size_t cap;
    j89a_alloc *al;
} mrvec;

static int mf_grow(mfvec *v)
{
    size_t nc;
    const struct j89a_object **m;
    nc = v->cap + v->cap + 16;
    m = (const struct j89a_object **)v->al->realloc(
        v->al->ctx, v->a, nc * sizeof(const struct j89a_object *));
    if (m == NULL)
    {
        return 0;
    }
    v->a = m;
    v->cap = nc;
    return 1;
}

static int mr_grow(mrvec *v)
{
    size_t nc;
    struct j89a_object **m;
    nc = v->cap + v->cap + 16;
    m = (struct j89a_object **)v->al->realloc(
        v->al->ctx, v->a, nc * sizeof(struct j89a_object *));
    if (m == NULL)
    {
        return 0;
    }
    v->a = m;
    v->cap = nc;
    return 1;
}

static j89a_status mf_push(mfvec *v, const struct j89a_object *o)
{
    int g;
    if (v->cap <= v->n)
    {
        g = mf_grow(v);
        if (!g)
        {
            return J89A_NOMEM;
        }
    }
    v->a[v->n] = o;
    v->n = v->n + 1;
    return J89A_OK;
}

static j89a_status mr_push(mrvec *v, struct j89a_object *o)
{
    int g;
    if (v->cap <= v->n)
    {
        g = mr_grow(v);
        if (!g)
        {
            return J89A_NOMEM;
        }
    }
    v->a[v->n] = o;
    v->n = v->n + 1;
    return J89A_OK;
}

static void map_fail(mfvec *f, mrvec *r)
{
    size_t i;
    for (i = 0; i < r->n; i = i + 1)
    {
        j89a_object_release(r->a[i]);
    }
    f->al->free(f->al->ctx, f->a);
    r->al->free(r->al->ctx, r->a);
}

/* Map an EMPTY source node and push the empty result. */
static j89a_status map_empty(mrvec *r, j89a_alloc *al)
{
    j89a_object *m;
    j89a_status st;
    m = j89a_object_empty(al);
    if (m == NULL)
    {
        return J89A_NOMEM;
    }
    st = mr_push(r, m);
    if (st != J89A_OK)
    {
        j89a_object_release(m);
        return st;
    }
    return J89A_OK;
}

/* Map one MEMBER source node and push the mapped result. */
static j89a_status map_member_node(const struct j89a_object *o,
                                   j89a_object_name_fn name_map,
                                   j89a_object_value_fn value_map, void *ctx,
                                   mrvec *r, j89a_alloc *al)
{
    j89a_object *m;
    j89a_status st;
    st = obj_map_member(o, name_map, value_map, ctx, &m, al);
    if (st != J89A_OK)
    {
        return st;
    }
    st = mr_push(r, m);
    if (st != J89A_OK)
    {
        j89a_object_release(m);
        return st;
    }
    return J89A_OK;
}

/* A UNION pushes a combine marker and its children so left maps first. */
static j89a_status map_union(const struct j89a_object *o, mfvec *f)
{
    j89a_status st;
    st = mf_push(f, NULL);
    if (st != J89A_OK)
    {
        return st;
    }
    st = mf_push(f, o->right);
    if (st != J89A_OK)
    {
        return st;
    }
    st = mf_push(f, o->left);
    if (st != J89A_OK)
    {
        return st;
    }
    return J89A_OK;
}

/* Pop the two child results and union them (left below right). */
static j89a_status map_combine(mrvec *r, j89a_alloc *al)
{
    j89a_object *right;
    j89a_object *left;
    j89a_object *u;
    j89a_status st;
    right = r->a[r->n - 1];
    r->n = r->n - 1;
    left = r->a[r->n - 1];
    r->n = r->n - 1;
    u = j89a_object_union(left, right, al);
    j89a_object_release(left);
    j89a_object_release(right);
    if (u == NULL)
    {
        return J89A_NOMEM;
    }
    st = mr_push(r, u);
    if (st != J89A_OK)
    {
        j89a_object_release(u);
        return st;
    }
    return J89A_OK;
}

/* Process one frame. Returns 1 while frames remain, 0 when done. */
static j89a_status obj_map_step(mfvec *f, mrvec *r,
                                j89a_object_name_fn name_map,
                                j89a_object_value_fn value_map, void *ctx,
                                j89a_alloc *al)
{
    const struct j89a_object *node;
    j89a_status st;
    node = f->a[f->n - 1];
    f->n = f->n - 1;
    if (node == NULL)
    {
        st = map_combine(r, al);
        return st;
    }
    if (node->tag == J89A_OBJECT_EMPTY)
    {
        st = map_empty(r, al);
        return st;
    }
    if (node->tag == J89A_OBJECT_MEMBER)
    {
        st = map_member_node(node, name_map, value_map, ctx, r, al);
        return st;
    }
    st = map_union(node, f);
    return st;
}

j89a_status j89a_object_map(const j89a_object *o, j89a_object_name_fn name_map,
                            j89a_object_value_fn value_map, void *ctx,
                            j89a_object **out, j89a_alloc *al)
{
    mfvec f;
    mrvec r;
    j89a_status st;
    *out = NULL;
    f.a = NULL;
    f.n = 0;
    f.cap = 0;
    f.al = al;
    r.a = NULL;
    r.n = 0;
    r.cap = 0;
    r.al = al;
    st = mf_push(&f, o);
    if (st != J89A_OK)
    {
        f.al->free(f.al->ctx, f.a);
        return st;
    }
    while (f.n != 0)
    {
        st = obj_map_step(&f, &r, name_map, value_map, ctx, al);
        if (st != J89A_OK)
        {
            map_fail(&f, &r);
            return st;
        }
    }
    *out = r.a[r.n - 1];
    f.al->free(f.al->ctx, f.a);
    r.al->free(r.al->ctx, r.a);
    return J89A_OK;
}

/* ---- iterative object fold ------------------------------------------------
 */

typedef struct frvec
{
    void **a;
    size_t n;
    size_t cap;
    j89a_alloc *al;
} frvec;

static int fr_grow(frvec *v)
{
    size_t nc;
    void **m;
    nc = v->cap + v->cap + 16;
    m = (void **)v->al->realloc(v->al->ctx, v->a, nc * sizeof(void *));
    if (m == NULL)
    {
        return 0;
    }
    v->a = m;
    v->cap = nc;
    return 1;
}

static j89a_status fr_push(frvec *v, void *p)
{
    int g;
    if (v->cap <= v->n)
    {
        g = fr_grow(v);
        if (!g)
        {
            return J89A_NOMEM;
        }
    }
    v->a[v->n] = p;
    v->n = v->n + 1;
    return J89A_OK;
}

static void fold_drop_buf(const j89a_rtype *rt, void *buf, j89a_alloc *al)
{
    j89a_rdrop(rt, buf);
    al->free(al->ctx, buf);
}

static void fold_drop_two(const j89a_rtype *rt, void *left, void *right,
                          j89a_alloc *al)
{
    fold_drop_buf(rt, left, al);
    fold_drop_buf(rt, right, al);
}

static void fold_abort(mfvec *f, frvec *r, const j89a_rtype *rt, j89a_alloc *al)
{
    size_t i;
    for (i = 0; i < r->n; i = i + 1)
    {
        fold_drop_buf(rt, r->a[i], al);
    }
    f->al->free(f->al->ctx, f->a);
    r->al->free(r->al->ctx, r->a);
}

/* Fold an EMPTY source node: push a copy of the monoid zero. */
static j89a_status fold_empty(const j89a_rtype *rt, const void *zero, frvec *r,
                              j89a_alloc *al)
{
    void *buf;
    j89a_status st;
    buf = j89a_malloc(al, rt->size);
    if (buf == NULL)
    {
        return J89A_NOMEM;
    }
    st = j89a_rcopy(rt, buf, zero);
    if (st != J89A_OK)
    {
        al->free(al->ctx, buf);
        return st;
    }
    st = fr_push(r, buf);
    if (st != J89A_OK)
    {
        fold_drop_buf(rt, buf, al);
        return st;
    }
    return J89A_OK;
}

/* Fold one MEMBER: push member_fn(ctx, name, value) as an OWNED R. */
static j89a_status fold_member(const struct j89a_object *o,
                               const j89a_rtype *rt,
                               j89a_object_member_fn member, void *ctx,
                               frvec *r, j89a_alloc *al)
{
    void *buf;
    j89a_status st;
    buf = j89a_malloc(al, rt->size);
    if (buf == NULL)
    {
        return J89A_NOMEM;
    }
    st = member(ctx, o->name, o->value, buf);
    if (st != J89A_OK)
    {
        al->free(al->ctx, buf);
        return st;
    }
    st = fr_push(r, buf);
    if (st != J89A_OK)
    {
        fold_drop_buf(rt, buf, al);
        return st;
    }
    return J89A_OK;
}

/* A UNION pushes a combine marker and its children. */
static j89a_status fold_union(const struct j89a_object *o, mfvec *f)
{
    j89a_status st;
    st = mf_push(f, NULL);
    if (st != J89A_OK)
    {
        return st;
    }
    st = mf_push(f, o->right);
    if (st != J89A_OK)
    {
        return st;
    }
    st = mf_push(f, o->left);
    if (st != J89A_OK)
    {
        return st;
    }
    return J89A_OK;
}

/* Pop two OWNED child R values, combine them, push the OWNED result. */
static j89a_status fold_combine(const j89a_rtype *rt,
                                j89a_object_combine_fn combine, void *ctx,
                                frvec *r, j89a_alloc *al)
{
    void *right;
    void *left;
    void *out;
    j89a_status st;
    right = r->a[r->n - 1];
    r->n = r->n - 1;
    left = r->a[r->n - 1];
    r->n = r->n - 1;
    out = j89a_malloc(al, rt->size);
    if (out == NULL)
    {
        fold_drop_two(rt, left, right, al);
        return J89A_NOMEM;
    }
    st = combine(ctx, left, right, out);
    fold_drop_two(rt, left, right, al);
    if (st != J89A_OK)
    {
        al->free(al->ctx, out);
        return st;
    }
    st = fr_push(r, out);
    if (st != J89A_OK)
    {
        fold_drop_buf(rt, out, al);
        return st;
    }
    return J89A_OK;
}

/* Process one fold frame. */
static j89a_status obj_fold_step(mfvec *f, frvec *r, const j89a_rtype *rt,
                                 const void *zero, j89a_object_member_fn member,
                                 j89a_object_combine_fn combine, void *ctx,
                                 j89a_alloc *al)
{
    const struct j89a_object *node;
    j89a_status st;
    node = f->a[f->n - 1];
    f->n = f->n - 1;
    if (node == NULL)
    {
        st = fold_combine(rt, combine, ctx, r, al);
        return st;
    }
    if (node->tag == J89A_OBJECT_EMPTY)
    {
        st = fold_empty(rt, zero, r, al);
        return st;
    }
    if (node->tag == J89A_OBJECT_MEMBER)
    {
        st = fold_member(node, rt, member, ctx, r, al);
        return st;
    }
    st = fold_union(node, f);
    return st;
}

j89a_status j89a_object_fold(const j89a_object *o, const j89a_rtype *rt,
                             const void *zero_r,
                             j89a_object_member_fn member_fn,
                             j89a_object_combine_fn combine_fn, void *ctx,
                             void *out_r, j89a_alloc *al)
{
    mfvec f;
    frvec r;
    void *final;
    j89a_status st;
    f.a = NULL;
    f.n = 0;
    f.cap = 0;
    f.al = al;
    r.a = NULL;
    r.n = 0;
    r.cap = 0;
    r.al = al;
    st = mf_push(&f, o);
    if (st != J89A_OK)
    {
        f.al->free(f.al->ctx, f.a);
        return st;
    }
    while (f.n != 0)
    {
        st = obj_fold_step(&f, &r, rt, zero_r, member_fn, combine_fn, ctx, al);
        if (st != J89A_OK)
        {
            fold_abort(&f, &r, rt, al);
            return st;
        }
    }
    final = r.a[r.n - 1];
    st = j89a_rcopy(rt, out_r, final);
    fold_drop_buf(rt, final, al);
    f.al->free(f.al->ctx, f.a);
    r.al->free(r.al->ctx, r.a);
    return st;
}
