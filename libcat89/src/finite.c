/* cat89_finite.c - reference finite category backend.
 *
 * A builder records objects, morphisms, identity and composition assignments
 * in checked growing vectors; build() finalizes one immutable store backing a
 * category, an equality and an enumeration capability. Morphism handles are
 * refcounted wrappers over a store row; object handles are borrowed per-object
 * tokens owned by the store. One logical builder mutation is one vector append,
 * so a failed mutation leaves the builder exactly as it was. */

#include "cat89_internal.h"
#include <cat89/finite.h>

#define FIN_SENTINEL ((unsigned long)-1)

struct finite_morphism_row
{
    unsigned long dom;
    unsigned long cod;
};

struct finite_composition_row
{
    unsigned long g;
    unsigned long f;
    unsigned long result;
};

struct cat89_finite_builder
{
    cat89_allocator allocator;
    cat89_vec ident;
    cat89_vec mors;
    cat89_vec comps;
};

/* ------------------------------------------------------------- builder */

cat89_status cat89_finite_builder_new(const cat89_allocator *allocator,
                                      cat89_finite_builder **out_builder)
{
    cat89_finite_builder *builder;
    const cat89_allocator *actual;

    if (out_builder == NULL)
    {
        return CAT89_INVALID;
    }
    *out_builder = NULL;
    if (allocator == NULL)
    {
        actual = cat89_allocator_default();
    }
    else
    {
        actual = allocator;
    }

    builder = cat89_alloc(actual, sizeof(*builder));
    if (builder == NULL)
    {
        return CAT89_NOMEM;
    }
    builder->allocator = *actual;
    cat89_vec_init(&builder->ident, sizeof(unsigned long));
    cat89_vec_init(&builder->mors, sizeof(struct finite_morphism_row));
    cat89_vec_init(&builder->comps, sizeof(struct finite_composition_row));

    *out_builder = builder;
    return CAT89_OK;
}

static const struct finite_morphism_row *
builder_mor(const cat89_finite_builder *builder, unsigned long i)
{
    return (const struct finite_morphism_row *)cat89_vec_at(&builder->mors, i);
}

static unsigned long builder_ident(const cat89_finite_builder *builder,
                                   unsigned long obj)
{
    const unsigned long *ids = (const unsigned long *)builder->ident.data;
    return ids[obj];
}

cat89_status cat89_finite_add_object(cat89_finite_builder *builder,
                                     cat89_finite_obj_id *out_id)
{
    unsigned long sentinel;
    cat89_status st;

    if (out_id == NULL)
    {
        return CAT89_INVALID;
    }
    *out_id = 0;
    if (builder == NULL)
    {
        return CAT89_INVALID;
    }
    sentinel = FIN_SENTINEL;
    st = cat89_vec_push(&builder->ident, &builder->allocator, &sentinel);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_id = builder->ident.len - 1;
    return CAT89_OK;
}

cat89_status cat89_finite_add_morphism(cat89_finite_builder *builder,
                                       cat89_finite_obj_id dom,
                                       cat89_finite_obj_id cod,
                                       cat89_finite_mor_id *out_id)
{
    struct finite_morphism_row row;
    cat89_status st;

    if (out_id == NULL)
    {
        return CAT89_INVALID;
    }
    *out_id = 0;
    if (builder == NULL)
    {
        return CAT89_INVALID;
    }
    if (dom >= builder->ident.len)
    {
        return CAT89_INVALID;
    }
    if (cod >= builder->ident.len)
    {
        return CAT89_INVALID;
    }
    row.dom = dom;
    row.cod = cod;
    st = cat89_vec_push(&builder->mors, &builder->allocator, &row);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_id = builder->mors.len - 1;
    return CAT89_OK;
}

cat89_status cat89_finite_set_identity(cat89_finite_builder *builder,
                                       cat89_finite_obj_id obj,
                                       cat89_finite_mor_id mor)
{
    unsigned long *ids;

    if (builder == NULL)
    {
        return CAT89_INVALID;
    }
    if (obj >= builder->ident.len)
    {
        return CAT89_INVALID;
    }
    if (mor >= builder->mors.len)
    {
        return CAT89_INVALID;
    }
    ids = (unsigned long *)builder->ident.data;
    ids[obj] = mor;
    return CAT89_OK;
}

cat89_status cat89_finite_set_composition(cat89_finite_builder *builder,
                                          cat89_finite_mor_id g,
                                          cat89_finite_mor_id f,
                                          cat89_finite_mor_id result)
{
    struct finite_composition_row row;
    cat89_status st;

    if (builder == NULL)
    {
        return CAT89_INVALID;
    }
    if (g >= builder->mors.len)
    {
        return CAT89_INVALID;
    }
    if (f >= builder->mors.len)
    {
        return CAT89_INVALID;
    }
    if (result >= builder->mors.len)
    {
        return CAT89_INVALID;
    }
    row.g = g;
    row.f = f;
    row.result = result;
    st = cat89_vec_push(&builder->comps, &builder->allocator, &row);
    return st;
}

void cat89_finite_builder_release(cat89_finite_builder *builder)
{
    if (builder == NULL)
    {
        return;
    }
    cat89_vec_free(&builder->ident, &builder->allocator);
    cat89_vec_free(&builder->mors, &builder->allocator);
    cat89_vec_free(&builder->comps, &builder->allocator);
    cat89_free(&builder->allocator, builder);
}

/* ---------------------------------------------------------- validation */

static int ident_ref_ok(const cat89_finite_builder *builder, size_t i)
{
    unsigned long id;

    id = builder_ident(builder, i);
    if (id == FIN_SENTINEL)
    {
        return 0;
    }
    if (id >= builder->mors.len)
    {
        return 0;
    }
    return 1;
}

static int validate_ident_refs(const cat89_finite_builder *builder)
{
    int ok;

    size_t i;

    for (i = 0; i < builder->ident.len; i = i + 1)
    {
        ok = ident_ref_ok(builder, i);
        if (ok == 0)
        {
            return 0;
        }
    }
    return 1;
}

static int ident_typing_ok(const cat89_finite_builder *builder, size_t i)
{
    unsigned long id;
    const struct finite_morphism_row *row;

    id = builder_ident(builder, i);
    row = builder_mor(builder, id);
    if (row->dom != i)
    {
        return 0;
    }
    if (row->cod != i)
    {
        return 0;
    }
    return 1;
}

static int validate_ident_typing(const cat89_finite_builder *builder)
{
    int ok;

    size_t i;

    for (i = 0; i < builder->ident.len; i = i + 1)
    {
        ok = ident_typing_ok(builder, i);
        if (ok == 0)
        {
            return 0;
        }
    }
    return 1;
}

static int comp_row_ok(const cat89_finite_builder *builder, size_t i)
{
    const struct finite_composition_row *row;
    const struct finite_morphism_row *frow;
    const struct finite_morphism_row *grow;
    const struct finite_morphism_row *rrow;

    row =
        (const struct finite_composition_row *)cat89_vec_at(&builder->comps, i);
    if (row->g >= builder->mors.len)
    {
        return 0;
    }
    if (row->f >= builder->mors.len)
    {
        return 0;
    }
    if (row->result >= builder->mors.len)
    {
        return 0;
    }
    frow = builder_mor(builder, row->f);
    grow = builder_mor(builder, row->g);
    rrow = builder_mor(builder, row->result);
    if (frow->cod != grow->dom)
    {
        return 0;
    }
    if (rrow->dom != frow->dom)
    {
        return 0;
    }
    if (rrow->cod != grow->cod)
    {
        return 0;
    }
    return 1;
}

static int validate_comp_rows(const cat89_finite_builder *builder)
{
    int ok;

    size_t i;

    for (i = 0; i < builder->comps.len; i = i + 1)
    {
        ok = comp_row_ok(builder, i);
        if (ok == 0)
        {
            return 0;
        }
    }
    return 1;
}

static void comp_lookup_row(const cat89_finite_builder *builder,
                            unsigned long g, unsigned long f, size_t i,
                            unsigned long *out_result, unsigned long *out_count)
{
    const struct finite_composition_row *row;

    row =
        (const struct finite_composition_row *)cat89_vec_at(&builder->comps, i);
    if (row->g != g)
    {
        return;
    }
    if (row->f != f)
    {
        return;
    }
    *out_result = row->result;
    *out_count = *out_count + 1;
}

static void comp_lookup(const cat89_finite_builder *builder, unsigned long g,
                        unsigned long f, unsigned long *out_result,
                        unsigned long *out_count)
{
    size_t i;

    *out_result = 0;
    *out_count = 0;
    for (i = 0; i < builder->comps.len; i = i + 1)
    {
        comp_lookup_row(builder, g, f, i, out_result, out_count);
    }
}

static int pair_complete_ok(const cat89_finite_builder *builder,
                            unsigned long g, unsigned long f)
{
    const struct finite_morphism_row *frow;
    const struct finite_morphism_row *grow;
    unsigned long result;
    unsigned long count;

    frow = builder_mor(builder, f);
    grow = builder_mor(builder, g);
    if (frow->cod != grow->dom)
    {
        return 1;
    }
    result = 0;
    count = 0;
    comp_lookup(builder, g, f, &result, &count);
    if (count != 1)
    {
        return 0;
    }
    return 1;
}

static int validate_completeness(const cat89_finite_builder *builder)
{
    int ok;

    unsigned long g;
    unsigned long f;

    for (g = 0; g < builder->mors.len; g = g + 1)
    {
        for (f = 0; f < builder->mors.len; f = f + 1)
        {
            ok = pair_complete_ok(builder, g, f);
            if (ok == 0)
            {
                return 0;
            }
        }
    }
    return 1;
}

static int identity_ok(const cat89_finite_builder *builder, unsigned long f)
{
    const struct finite_morphism_row *row;
    unsigned long id;
    unsigned long result;
    unsigned long count;

    row = builder_mor(builder, f);
    id = builder_ident(builder, row->cod);
    result = 0;
    count = 0;
    comp_lookup(builder, id, f, &result, &count);
    if (count != 1)
    {
        return 0;
    }
    if (result != f)
    {
        return 0;
    }
    id = builder_ident(builder, row->dom);
    result = 0;
    count = 0;
    comp_lookup(builder, f, id, &result, &count);
    if (count != 1)
    {
        return 0;
    }
    if (result != f)
    {
        return 0;
    }
    return 1;
}

static int validate_identities(const cat89_finite_builder *builder)
{
    int ok;

    unsigned long f;

    for (f = 0; f < builder->mors.len; f = f + 1)
    {
        ok = identity_ok(builder, f);
        if (ok == 0)
        {
            return 0;
        }
    }
    return 1;
}

static int assoc_triple_ok(const cat89_finite_builder *builder, unsigned long h,
                           unsigned long g, unsigned long f, unsigned long gf)
{
    const struct finite_morphism_row *hrow;
    const struct finite_morphism_row *grow;
    unsigned long hg;
    unsigned long lhs;
    unsigned long rhs;
    unsigned long count;

    hrow = builder_mor(builder, h);
    grow = builder_mor(builder, g);
    if (hrow->dom != grow->cod)
    {
        return 1;
    }
    hg = 0;
    count = 0;
    comp_lookup(builder, h, g, &hg, &count);
    lhs = 0;
    rhs = 0;
    comp_lookup(builder, hg, f, &lhs, &count);
    comp_lookup(builder, h, gf, &rhs, &count);
    if (lhs != rhs)
    {
        return 0;
    }
    return 1;
}

static int assoc_pair_ok(const cat89_finite_builder *builder, unsigned long g,
                         unsigned long f)
{
    const struct finite_morphism_row *frow;
    const struct finite_morphism_row *grow;
    unsigned long gf;
    unsigned long count;
    unsigned long h;
    int ok;

    frow = builder_mor(builder, f);
    grow = builder_mor(builder, g);
    if (frow->cod != grow->dom)
    {
        return 1;
    }
    gf = 0;
    count = 0;
    comp_lookup(builder, g, f, &gf, &count);
    for (h = 0; h < builder->mors.len; h = h + 1)
    {
        ok = assoc_triple_ok(builder, h, g, f, gf);
        if (ok == 0)
        {
            return 0;
        }
    }
    return 1;
}

static int validate_associativity(const cat89_finite_builder *builder)
{
    int ok;

    unsigned long g;
    unsigned long f;

    for (g = 0; g < builder->mors.len; g = g + 1)
    {
        for (f = 0; f < builder->mors.len; f = f + 1)
        {
            ok = assoc_pair_ok(builder, g, f);
            if (ok == 0)
            {
                return 0;
            }
        }
    }
    return 1;
}

static int builder_valid(const cat89_finite_builder *builder)
{
    int ok;

    ok = validate_ident_refs(builder);
    if (ok == 0)
    {
        return 0;
    }
    ok = validate_ident_typing(builder);
    if (ok == 0)
    {
        return 0;
    }
    ok = validate_comp_rows(builder);
    if (ok == 0)
    {
        return 0;
    }
    ok = validate_completeness(builder);
    if (ok == 0)
    {
        return 0;
    }
    ok = validate_associativity(builder);
    if (ok == 0)
    {
        return 0;
    }
    ok = validate_identities(builder);
    return ok;
}

cat89_status cat89_finite_validate(cat89_finite_builder *builder,
                                   int *out_valid)
{
    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (builder == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = builder_valid(builder);
    return CAT89_OK;
}

/* ------------------------------------------------ immutable store */

struct fin_mor
{
    unsigned long refs;
    struct cat89_finite_store *store;
    unsigned long index;
    struct fin_mor *next_live;
};

struct cat89_finite_store
{
    unsigned long refs;
    cat89_allocator allocator;
    unsigned long n_obj;
    unsigned long n_mor;
    unsigned long n_comp;
    unsigned char *obj_mem;
    unsigned long *dom;
    unsigned long *cod;
    unsigned long *ident;
    unsigned long *cg;
    unsigned long *cf;
    unsigned long *cr;
    struct fin_mor *live_mors;
};

static int store_drop(struct cat89_finite_store *store)
{
    int dropped;

    dropped = cat89_ref_dec(&store->refs);
    return dropped;
}

static void store_dispose(struct cat89_finite_store *store)
{
    cat89_free(&store->allocator, store->obj_mem);
    cat89_free(&store->allocator, store->dom);
    cat89_free(&store->allocator, store->cod);
    cat89_free(&store->allocator, store->ident);
    cat89_free(&store->allocator, store->cg);
    cat89_free(&store->allocator, store->cf);
    cat89_free(&store->allocator, store->cr);
    cat89_free(&store->allocator, store);
}

static void store_release(struct cat89_finite_store *store)
{
    int last;

    if (store == NULL)
    {
        return;
    }
    last = store_drop(store);
    if (last)
    {
        store_dispose(store);
    }
}

static cat89_status store_retain(struct cat89_finite_store *store)
{
    cat89_status st;

    st = cat89_ref_inc(&store->refs);
    return st;
}

static const struct fin_mor *mor_cread(const cat89_mor *mor)
{
    return (const struct fin_mor *)(const void *)mor;
}

static struct fin_mor *mor_handle(cat89_mor *mor)
{
    return (struct fin_mor *)(void *)mor;
}

static const cat89_obj *obj_of(struct cat89_finite_store *store,
                               unsigned long id)
{
    return (const cat89_obj *)(void *)&store->obj_mem[id];
}

static unsigned long obj_id_of(struct cat89_finite_store *store,
                               const cat89_obj *obj)
{
    const unsigned char *p;
    const unsigned char *base;

    p = (const unsigned char *)(const void *)obj;
    base = store->obj_mem;
    return (unsigned long)(p - base);
}

static cat89_status mor_make(struct cat89_finite_store *store,
                             unsigned long index, cat89_mor **out_mor)
{
    struct fin_mor *mor;
    cat89_status st;

    st = store_retain(store);
    if (st != CAT89_OK)
    {
        return st;
    }
    mor = cat89_alloc(&store->allocator, sizeof(*mor));
    if (mor == NULL)
    {
        store_release(store);
        return CAT89_NOMEM;
    }
    mor->refs = 1;
    mor->store = store;
    mor->index = index;
    mor->next_live = store->live_mors;
    store->live_mors = mor;
    *out_mor = (cat89_mor *)mor;
    return CAT89_OK;
}

static int mor_drop(struct fin_mor *mor)
{
    int dropped;

    dropped = cat89_ref_dec(&mor->refs);
    return dropped;
}

static struct fin_mor **mor_unlink_next(struct fin_mor **link)
{
    return &(*link)->next_live;
}

static void mor_link_remove(struct fin_mor **link, struct fin_mor *mor)
{
    *link = mor->next_live;
}

static void mor_unlink(struct cat89_finite_store *store, struct fin_mor *mor)
{
    struct fin_mor **link;

    link = &store->live_mors;
    while (*link != NULL)
    {
        if (*link == mor)
        {
            mor_link_remove(link, mor);
            return;
        }
        link = mor_unlink_next(link);
    }
}

static void mor_dispose(struct fin_mor *mor)
{
    struct cat89_finite_store *store;

    store = mor->store;
    mor_unlink(store, mor);
    cat89_free(&store->allocator, mor);
    store_release(store);
}

static unsigned long store_comp(struct cat89_finite_store *store,
                                unsigned long g, unsigned long f)
{
    unsigned long i;

    for (i = 0; i < store->n_comp; i = i + 1)
    {
        if (store->cg[i] == g)
        {
            if (store->cf[i] == f)
            {
                return store->cr[i];
            }
        }
    }
    return FIN_SENTINEL;
}

static cat89_status fin_dom(void *ctx, const cat89_mor *mor,
                            const cat89_obj **out_obj)
{
    struct cat89_finite_store *store = ctx;
    const struct fin_mor *m;
    unsigned long did;

    m = mor_cread(mor);
    did = store->dom[m->index];
    *out_obj = obj_of(store, did);
    return CAT89_OK;
}

static cat89_status fin_cod(void *ctx, const cat89_mor *mor,
                            const cat89_obj **out_obj)
{
    struct cat89_finite_store *store = ctx;
    const struct fin_mor *m;
    unsigned long cid;

    m = mor_cread(mor);
    cid = store->cod[m->index];
    *out_obj = obj_of(store, cid);
    return CAT89_OK;
}

static cat89_status fin_identity(void *ctx, const cat89_obj *obj,
                                 cat89_mor **out_mor)
{
    struct cat89_finite_store *store = ctx;
    unsigned long oid;
    unsigned long mid;
    cat89_status st;

    oid = obj_id_of(store, obj);
    mid = store->ident[oid];
    st = mor_make(store, mid, out_mor);
    return st;
}

static cat89_status fin_compose(void *ctx, const cat89_mor *g,
                                const cat89_mor *f, cat89_mor **out_mor)
{
    struct cat89_finite_store *store = ctx;
    const struct fin_mor *gm;
    const struct fin_mor *fm;
    unsigned long result;
    cat89_status st;

    gm = mor_cread(g);
    fm = mor_cread(f);
    result = store_comp(store, gm->index, fm->index);
    if (result == FIN_SENTINEL)
    {
        return CAT89_INVALID;
    }
    st = mor_make(store, result, out_mor);
    return st;
}

static int fin_obj_same(void *ctx, const cat89_obj *a, const cat89_obj *b)
{
    (void)ctx;
    return a == b;
}

static cat89_status fin_retain(void *ctx, cat89_mor *mor)
{
    struct fin_mor *m;
    cat89_status st;

    (void)ctx;
    m = mor_handle(mor);
    st = cat89_ref_inc(&m->refs);
    return st;
}

static void fin_release(void *ctx, cat89_mor *mor)
{
    struct fin_mor *m;
    int last;

    (void)ctx;
    m = mor_handle(mor);
    last = mor_drop(m);
    if (last)
    {
        mor_dispose(m);
    }
}

static void fin_destroy(void *ctx)
{
    struct cat89_finite_store *store = ctx;

    store_release(store);
}

static int fin_owns_obj(void *ctx, const cat89_obj *obj)
{
    struct cat89_finite_store *store = ctx;
    const unsigned char *p;
    const unsigned char *base;

    p = (const unsigned char *)(const void *)obj;
    base = store->obj_mem;
    if (store->n_obj == 0)
    {
        return 0;
    }
    if (p < base)
    {
        return 0;
    }
    if (p >= base + store->n_obj)
    {
        return 0;
    }
    return 1;
}

static int fin_owns_mor(void *ctx, const cat89_mor *mor)
{
    struct cat89_finite_store *store = ctx;
    struct fin_mor *m;

    for (m = store->live_mors; m != NULL; m = m->next_live)
    {
        if ((const cat89_mor *)(const void *)m == mor)
        {
            return 1;
        }
    }
    return 0;
}

static const cat89_category_ops fin_ops = {
    fin_dom,    fin_cod,     fin_identity, fin_compose,  fin_obj_same,
    fin_retain, fin_release, fin_owns_obj, fin_owns_mor, fin_destroy};

/* ---------------------------------- equality / enumeration */

struct fin_iter
{
    struct cat89_finite_store *store;
    unsigned long next;
};

static cat89_status obj_equal_fn(void *ctx, const cat89_obj *a,
                                 const cat89_obj *b, int *out_equal)
{
    struct cat89_finite_store *store = ctx;
    unsigned long ia;
    unsigned long ib;

    ia = obj_id_of(store, a);
    ib = obj_id_of(store, b);
    if (ia == ib)
    {
        *out_equal = 1;
        return CAT89_OK;
    }
    *out_equal = 0;
    return CAT89_OK;
}

static cat89_status mor_equal_fn(void *ctx, const cat89_mor *f,
                                 const cat89_mor *g, int *out_equal)
{
    const struct fin_mor *fm;
    const struct fin_mor *gm;

    (void)ctx;
    fm = mor_cread(f);
    gm = mor_cread(g);
    if (fm->index == gm->index)
    {
        *out_equal = 1;
        return CAT89_OK;
    }
    *out_equal = 0;
    return CAT89_OK;
}

static cat89_status iter_open_obj(void *ctx, cat89_obj_iter **out_iter)
{
    struct cat89_finite_store *store = ctx;
    struct fin_iter *it;

    it = cat89_alloc(&store->allocator, sizeof(*it));
    if (it == NULL)
    {
        return CAT89_NOMEM;
    }
    it->store = store;
    it->next = 0;
    *out_iter = (cat89_obj_iter *)it;
    return CAT89_OK;
}

static void obj_exhausted(const cat89_obj **out_obj, int *out_done)
{
    *out_obj = NULL;
    *out_done = 1;
}

static cat89_status obj_iter_next_fn(void *ctx, cat89_obj_iter *iter,
                                     const cat89_obj **out_obj, int *out_done)
{
    struct cat89_finite_store *store = ctx;
    struct fin_iter *it = (struct fin_iter *)iter;

    if (it->next >= store->n_obj)
    {
        obj_exhausted(out_obj, out_done);
        return CAT89_OK;
    }
    *out_obj = obj_of(store, it->next);
    it->next = it->next + 1;
    *out_done = 0;
    return CAT89_OK;
}

static void obj_iter_close_fn(void *ctx, cat89_obj_iter *iter)
{
    struct cat89_finite_store *store = ctx;
    struct fin_iter *it = (struct fin_iter *)iter;

    if (it == NULL)
    {
        return;
    }
    cat89_free(&store->allocator, it);
}

static cat89_status iter_open_mor(void *ctx, cat89_mor_iter **out_iter)
{
    struct cat89_finite_store *store = ctx;
    struct fin_iter *it;

    it = cat89_alloc(&store->allocator, sizeof(*it));
    if (it == NULL)
    {
        return CAT89_NOMEM;
    }
    it->store = store;
    it->next = 0;
    *out_iter = (cat89_mor_iter *)it;
    return CAT89_OK;
}

static void mor_exhausted(cat89_mor **out_mor, int *out_done)
{
    *out_mor = NULL;
    *out_done = 1;
}

static cat89_status mor_iter_next_fn(void *ctx, cat89_mor_iter *iter,
                                     cat89_mor **out_mor, int *out_done)
{
    struct cat89_finite_store *store = ctx;
    struct fin_iter *it = (struct fin_iter *)iter;
    cat89_status st;

    if (it->next >= store->n_mor)
    {
        mor_exhausted(out_mor, out_done);
        return CAT89_OK;
    }
    st = mor_make(store, it->next, out_mor);
    if (st != CAT89_OK)
    {
        return st;
    }
    it->next = it->next + 1;
    *out_done = 0;
    return CAT89_OK;
}

static void mor_iter_close_fn(void *ctx, cat89_mor_iter *iter)
{
    struct cat89_finite_store *store = ctx;
    struct fin_iter *it = (struct fin_iter *)iter;

    if (it == NULL)
    {
        return;
    }
    cat89_free(&store->allocator, it);
}

static const cat89_eq_ops fin_eq_ops = {obj_equal_fn, mor_equal_fn, NULL};

static const cat89_enum_ops fin_enum_ops = {iter_open_obj,
                                            obj_iter_next_fn,
                                            obj_iter_close_fn,
                                            iter_open_mor,
                                            mor_iter_next_fn,
                                            mor_iter_close_fn,
                                            NULL,
                                            NULL};

/* ---------------------------------------------- finalization */

static cat89_status alloc_ul_array(const cat89_allocator *allocator,
                                   unsigned long count, unsigned long **out)
{
    size_t bytes;
    cat89_status st;

    *out = NULL;
    if (count == 0)
    {
        return CAT89_OK;
    }
    st = cat89_size_mul(count, sizeof(unsigned long), &bytes);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out = cat89_alloc(allocator, bytes);
    if (*out == NULL)
    {
        return CAT89_NOMEM;
    }
    return CAT89_OK;
}

static void store_free_partial(struct cat89_finite_store *store)
{
    cat89_free(&store->allocator, store->obj_mem);
    cat89_free(&store->allocator, store->dom);
    cat89_free(&store->allocator, store->cod);
    cat89_free(&store->allocator, store->ident);
    cat89_free(&store->allocator, store->cg);
    cat89_free(&store->allocator, store->cf);
    cat89_free(&store->allocator, store->cr);
    cat89_free(&store->allocator, store);
}

static cat89_status store_alloc_arrays(struct cat89_finite_store *store,
                                       unsigned long n_obj, unsigned long n_mor,
                                       unsigned long n_comp)
{
    cat89_status st;

    if (n_obj > 0)
    {
        store->obj_mem = cat89_alloc(&store->allocator, n_obj);
        if (store->obj_mem == NULL)
        {
            return CAT89_NOMEM;
        }
    }
    st = alloc_ul_array(&store->allocator, n_mor, &store->dom);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = alloc_ul_array(&store->allocator, n_mor, &store->cod);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = alloc_ul_array(&store->allocator, n_obj, &store->ident);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = alloc_ul_array(&store->allocator, n_comp, &store->cg);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = alloc_ul_array(&store->allocator, n_comp, &store->cf);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = alloc_ul_array(&store->allocator, n_comp, &store->cr);
    return st;
}

static void store_copy_obj(const cat89_finite_builder *builder,
                           struct cat89_finite_store *store, unsigned long i)
{
    store->obj_mem[i] = 0;
    store->ident[i] = builder_ident(builder, i);
}

static void store_copy_mor(const cat89_finite_builder *builder,
                           struct cat89_finite_store *store, unsigned long i)
{
    const struct finite_morphism_row *mrow;

    mrow = builder_mor(builder, i);
    store->dom[i] = mrow->dom;
    store->cod[i] = mrow->cod;
}

static void store_copy_comp(const cat89_finite_builder *builder,
                            struct cat89_finite_store *store, unsigned long i)
{
    const struct finite_composition_row *crow;

    crow =
        (const struct finite_composition_row *)cat89_vec_at(&builder->comps, i);
    store->cg[i] = crow->g;
    store->cf[i] = crow->f;
    store->cr[i] = crow->result;
}

static void store_copy_rows(const cat89_finite_builder *builder,
                            struct cat89_finite_store *store)
{
    unsigned long i;

    for (i = 0; i < store->n_obj; i = i + 1)
    {
        store_copy_obj(builder, store, i);
    }
    for (i = 0; i < store->n_mor; i = i + 1)
    {
        store_copy_mor(builder, store, i);
    }
    for (i = 0; i < store->n_comp; i = i + 1)
    {
        store_copy_comp(builder, store, i);
    }
}

static cat89_status store_build(const cat89_finite_builder *builder,
                                struct cat89_finite_store **out_store)
{
    struct cat89_finite_store *store;
    cat89_status st;

    store = cat89_alloc(&builder->allocator, sizeof(*store));
    if (store == NULL)
    {
        return CAT89_NOMEM;
    }
    store->refs = 1;
    store->allocator = builder->allocator;
    store->n_obj = builder->ident.len;
    store->n_mor = builder->mors.len;
    store->n_comp = builder->comps.len;
    store->obj_mem = NULL;
    store->dom = NULL;
    store->cod = NULL;
    store->ident = NULL;
    store->cg = NULL;
    store->cf = NULL;
    store->cr = NULL;
    store->live_mors = NULL;

    st = store_alloc_arrays(store, store->n_obj, store->n_mor, store->n_comp);
    if (st != CAT89_OK)
    {
        store_free_partial(store);
        return st;
    }
    store_copy_rows(builder, store);
    *out_store = store;
    return CAT89_OK;
}

static void build_fail(cat89_category *category)
{
    cat89_category_release(category);
}

static void build_fail_eq(cat89_category *category, cat89_eq *eq)
{
    cat89_eq_release(eq);
    cat89_category_release(category);
}

static cat89_status build_common(cat89_finite_builder *builder, int check,
                                 cat89_category **out_category,
                                 cat89_eq **out_eq, cat89_enum **out_enum)
{
    struct cat89_finite_store *store;
    cat89_category *category;
    cat89_status st;
    int valid;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;
    if (out_eq != NULL)
    {
        *out_eq = NULL;
    }
    if (out_enum != NULL)
    {
        *out_enum = NULL;
    }
    if (builder == NULL)
    {
        return CAT89_INVALID;
    }
    if (check)
    {
        valid = builder_valid(builder);
        if (valid == 0)
        {
            return CAT89_INVALID;
        }
    }

    st = store_build(builder, &store);
    if (st != CAT89_OK)
    {
        return st;
    }

    st = cat89_category_new(&fin_ops, store, &store->allocator, &category);
    if (st != CAT89_OK)
    {
        store_release(store);
        return st;
    }

    if (out_eq != NULL)
    {
        st = cat89_eq_new(category, &fin_eq_ops, store, &store->allocator,
                          out_eq);
        if (st != CAT89_OK)
        {
            build_fail(category);
            return st;
        }
    }

    if (out_enum != NULL)
    {
        st = cat89_enum_new(category, &fin_enum_ops, store, &store->allocator,
                            out_enum);
        if (st != CAT89_OK)
        {
            if (out_eq != NULL)
            {
                build_fail_eq(category, *out_eq);
            }
            else
            {
                build_fail(category);
            }
            return st;
        }
    }

    *out_category = category;
    return CAT89_OK;
}

cat89_status cat89_finite_build(cat89_finite_builder *builder,
                                cat89_category **out_category,
                                cat89_eq **out_eq, cat89_enum **out_enum)
{
    cat89_status st;

    st = build_common(builder, 1, out_category, out_eq, out_enum);
    return st;
}

cat89_status cat89_finite_build_unchecked(cat89_finite_builder *builder,
                                          cat89_category **out_category,
                                          cat89_eq **out_eq,
                                          cat89_enum **out_enum)
{
    cat89_status st;

    st = build_common(builder, 0, out_category, out_eq, out_enum);
    return st;
}

void cat89_finite_test_set_mor_refs(cat89_mor *mor, unsigned long refs)
{
    struct fin_mor *m;

    if (mor == NULL)
    {
        return;
    }
    m = mor_handle(mor);
    m->refs = refs;
}
