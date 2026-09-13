/* cat89_findlim.c - finite universal-construction finders (binary product).
 *
 * A binary product a x b in a finite ambient category is an apex P with
 * projections pa : P -> a and pb : P -> b such that for every object Y the
 * map  m |-> (pa o m, pb o m) : Hom(Y,P) -> Hom(Y,a) x Hom(Y,b)  is a
 * bijection. With equal finite cardinalities
 *     |Hom(Y,P)| == |Hom(Y,a)| * |Hom(Y,b)|
 * bijectivity follows from surjectivity, so the factor of every cone
 * (Y, xa, xb) is unique (CAT-I9). The result is returned as a genuine
 * cat89_limit of the DISCRETE2 diagram mapping its two points to a and b.
 *
 * The ambient is materialized once into a plain census (owned morphisms with
 * their structural dom/cod); every hom-set scan is then a linear pass. Object
 * identity uses structural handles (stable finite pointers), matching find.c;
 * morphism equality uses the supplied cat89_eq.
 *
 * This translation unit is written to the workspace green semantic profile:
 * per-element loops delegate to early-return step workers so no effectful call
 * sits inside a nested block, and every nested block holds at most one glue. */

#include "cat89_internal.h"
#include <cat89/findlim.h>
#include <cat89/functor.h>
#include <cat89/shape.h>

struct fmor
{
    cat89_mor *mor;
    const cat89_obj *dom;
    const cat89_obj *cod;
};

struct census
{
    cat89_category *category;
    const cat89_obj **objs;
    struct fmor *mors;
    unsigned long n_obj;
    unsigned long n_mor;
    cat89_allocator allocator;
};

/* Diagram object mapping: DISCRETE2 point a -> object `a`, point b -> `b`. */
struct dctx
{
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    const cat89_obj *a;
    const cat89_obj *b;
    cat89_allocator allocator;
};

/* Result context shared by the returned cone and limit (freed by the cone). */
struct pres
{
    cat89_category *category;
    cat89_enum *enumeration;
    cat89_eq *eq;
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    const cat89_obj *apex;
    cat89_mor *pa;
    cat89_mor *pb;
    cat89_allocator allocator;
};

static const cat89_allocator *resolve_alloc(const cat89_allocator *allocator)
{
    const cat89_allocator *actual;

    if (allocator == NULL)
    {
        actual = cat89_allocator_default();
    }
    else
    {
        actual = allocator;
    }
    return actual;
}

static cat89_status same_dom(cat89_category *cat, const cat89_mor *a,
                             const cat89_mor *b, int *out_ok)
{
    const cat89_obj *da;
    const cat89_obj *db;
    cat89_status st;

    st = cat89_dom(cat, a, &da);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_dom(cat, b, &db);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_ok = cat89_obj_same(cat, da, db);
    return CAT89_OK;
}

static cat89_status same_cod(cat89_category *cat, const cat89_mor *a,
                             const cat89_mor *b, int *out_ok)
{
    const cat89_obj *ca;
    const cat89_obj *cb;
    cat89_status st;

    st = cat89_cod(cat, a, &ca);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(cat, b, &cb);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_ok = cat89_obj_same(cat, ca, cb);
    return CAT89_OK;
}

static cat89_status parallel_arrows_ok(cat89_category *cat, cat89_mor *f,
                                       cat89_mor *g)
{
    cat89_status st;
    int ok;

    ok = 0;
    st = same_dom(cat, f, g, &ok);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (ok == 0)
    {
        return CAT89_DOMAIN;
    }
    st = same_cod(cat, f, g, &ok);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (ok == 0)
    {
        return CAT89_DOMAIN;
    }
    return CAT89_OK;
}

static cat89_status shared_codomain_ok(cat89_category *cat, cat89_mor *f,
                                       cat89_mor *g)
{
    cat89_status st;
    int ok;

    ok = 0;
    st = same_cod(cat, f, g, &ok);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (ok == 0)
    {
        return CAT89_DOMAIN;
    }
    return CAT89_OK;
}

static cat89_status shared_domain_ok(cat89_category *cat, cat89_mor *f,
                                     cat89_mor *g)
{
    cat89_status st;
    int ok;

    ok = 0;
    st = same_dom(cat, f, g, &ok);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (ok == 0)
    {
        return CAT89_DOMAIN;
    }
    return CAT89_OK;
}

static void rel1(cat89_category *cat, cat89_mor *mor)
{
    if (mor != NULL)
    {
        cat89_mor_release(cat, mor);
    }
}

static void rel2(cat89_category *cat, cat89_mor *a, cat89_mor *b)
{
    rel1(cat, a);
    rel1(cat, b);
}

/* ------------------------------------------------------ diagram mapping */

static const cat89_obj *obj_select(struct dctx *dx, const cat89_obj *obj)
{
    if (obj == dx->pt_a)
    {
        return dx->a;
    }
    if (obj == dx->pt_b)
    {
        return dx->b;
    }
    return NULL;
}

static cat89_status dmap_obj(void *ctx, const cat89_obj *obj,
                             const cat89_obj **out_obj)
{
    struct dctx *dx = ctx;
    const cat89_obj *res;

    *out_obj = NULL;
    res = obj_select(dx, obj);
    if (res == NULL)
    {
        return CAT89_INVALID;
    }
    *out_obj = res;
    return CAT89_OK;
}

static void dmap_destroy(void *ctx)
{
    struct dctx *dx = ctx;

    cat89_free(&dx->allocator, dx);
}

static const cat89_functor_ops d_ops = {dmap_obj, NULL, dmap_destroy};

/* ------------------------------------------------------------- census */

static void census_free(struct census *cs)
{
    unsigned long i;

    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        cat89_mor_release(cs->category, cs->mors[i].mor);
    }
    if (cs->mors != NULL)
    {
        cat89_free(&cs->allocator, cs->mors);
    }
    if (cs->objs != NULL)
    {
        cat89_free(&cs->allocator, (void *)cs->objs);
    }
    cs->objs = NULL;
    cs->mors = NULL;
    cs->n_obj = 0;
    cs->n_mor = 0;
}

static cat89_status census_objs_step(cat89_enum *en, cat89_obj_iter *it,
                                     cat89_vec *vec,
                                     const cat89_allocator *alloc,
                                     int *out_done)
{
    const cat89_obj *obj;
    cat89_status st;

    obj = NULL;
    st = cat89_obj_iter_next(en, it, &obj, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    st = cat89_vec_push(vec, alloc, &obj);
    return st;
}

static void census_objs_abort(cat89_enum *en, cat89_obj_iter *it,
                              cat89_vec *vec, const cat89_allocator *alloc)
{
    cat89_obj_iter_close(en, it);
    cat89_vec_free(vec, alloc);
}

static cat89_status census_objs(cat89_enum *en, struct census *cs)
{
    cat89_obj_iter *it;
    cat89_vec vec;
    int done;
    cat89_status st;

    cat89_vec_init(&vec, sizeof(const cat89_obj *));
    it = NULL;
    st = cat89_obj_iter_open(en, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = census_objs_step(en, it, &vec, &cs->allocator, &done);
        if (st != CAT89_OK)
        {
            census_objs_abort(en, it, &vec, &cs->allocator);
            return st;
        }
    }
    cat89_obj_iter_close(en, it);
    cs->objs = (const cat89_obj **)vec.data;
    cs->n_obj = vec.len;
    return CAT89_OK;
}

static void census_mor_row_free(cat89_category *cat, cat89_vec *vec, size_t i)
{
    const struct fmor *row;

    row = (const struct fmor *)cat89_vec_at(vec, i);
    cat89_mor_release(cat, row->mor);
}

static void census_mors_vec_free(cat89_category *cat, cat89_vec *vec,
                                 const cat89_allocator *allocator)
{
    size_t i;

    for (i = 0; i < vec->len; i = i + 1)
    {
        census_mor_row_free(cat, vec, i);
    }
    cat89_vec_free(vec, allocator);
}

static cat89_status census_mors_step(cat89_category *cat, cat89_enum *en,
                                     cat89_mor_iter *it, cat89_vec *vec,
                                     const cat89_allocator *alloc,
                                     int *out_done)
{
    struct fmor row;
    cat89_mor *mor;
    cat89_status st;

    mor = NULL;
    st = cat89_mor_iter_next(en, it, &mor, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    row.mor = mor;
    row.dom = NULL;
    row.cod = NULL;
    st = cat89_dom(cat, mor, &row.dom);
    if (st == CAT89_OK)
    {
        st = cat89_cod(cat, mor, &row.cod);
    }
    if (st == CAT89_OK)
    {
        st = cat89_vec_push(vec, alloc, &row);
    }
    if (st != CAT89_OK)
    {
        cat89_mor_release(cat, mor);
        return st;
    }
    return CAT89_OK;
}

static void census_mors_abort(cat89_category *cat, cat89_enum *en,
                              cat89_mor_iter *it, cat89_vec *vec,
                              const cat89_allocator *alloc)
{
    cat89_mor_iter_close(en, it);
    census_mors_vec_free(cat, vec, alloc);
}

static cat89_status census_mors(cat89_category *cat, cat89_enum *en,
                                struct census *cs)
{
    cat89_mor_iter *it;
    cat89_vec vec;
    int done;
    cat89_status st;

    cat89_vec_init(&vec, sizeof(struct fmor));
    it = NULL;
    st = cat89_mor_iter_open(en, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = census_mors_step(cat, en, it, &vec, &cs->allocator, &done);
        if (st != CAT89_OK)
        {
            census_mors_abort(cat, en, it, &vec, &cs->allocator);
            return st;
        }
    }
    cat89_mor_iter_close(en, it);
    cs->mors = (struct fmor *)vec.data;
    cs->n_mor = vec.len;
    return CAT89_OK;
}

static cat89_status census_build(cat89_category *cat, cat89_enum *en,
                                 const cat89_allocator *allocator,
                                 struct census *cs)
{
    cat89_status st;

    cs->category = cat;
    cs->objs = NULL;
    cs->mors = NULL;
    cs->n_obj = 0;
    cs->n_mor = 0;
    cs->allocator = *allocator;
    st = census_objs(en, cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = census_mors(cat, en, cs);
    if (st != CAT89_OK)
    {
        census_free(cs);
        return st;
    }
    return CAT89_OK;
}

/* --------------------------------------------------------- hom scans */

static cat89_status hom_count(const struct census *cs, const cat89_obj *x,
                              const cat89_obj *y, unsigned long *out_n)
{
    unsigned long i;
    unsigned long n;

    n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        if (cat89_obj_same(cs->category, cs->mors[i].dom, x) != 0)
        {
            if (cat89_obj_same(cs->category, cs->mors[i].cod, y) != 0)
            {
                ++n;
            }
        }
    }
    *out_n = n;
    return CAT89_OK;
}

static void indices_free(const cat89_allocator *alloc, unsigned long *arr)
{
    if (arr != NULL)
    {
        cat89_free(alloc, arr);
    }
}

static void rel_arrays(const cat89_allocator *alloc, unsigned long *a,
                       unsigned long *b)
{
    indices_free(alloc, a);
    indices_free(alloc, b);
}

static cat89_status gather_one(const struct census *cs, const cat89_obj *x,
                               const cat89_obj *y, unsigned long i,
                               cat89_vec *vec)
{
    unsigned long idx;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, x) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, y) == 0)
    {
        return CAT89_OK;
    }
    idx = i;
    st = cat89_vec_push(vec, &cs->allocator, &idx);
    return st;
}

static void gather_abort(cat89_vec *vec, const cat89_allocator *alloc)
{
    cat89_vec_free(vec, alloc);
}

static cat89_status gather_indices(const struct census *cs, const cat89_obj *x,
                                   const cat89_obj *y, unsigned long **out_arr,
                                   unsigned long *out_n)
{
    cat89_vec vec;
    unsigned long i;
    cat89_status st;

    cat89_vec_init(&vec, sizeof(unsigned long));
    *out_arr = NULL;
    *out_n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = gather_one(cs, x, y, i, &vec);
        if (st != CAT89_OK)
        {
            gather_abort(&vec, &cs->allocator);
            return st;
        }
    }
    *out_arr = (unsigned long *)vec.data;
    *out_n = vec.len;
    return CAT89_OK;
}

/* ------------------------------------------------------ mediation */

static cat89_status evaluate_mediator(cat89_category *cat, cat89_eq *eq,
                                      cat89_mor *pa, cat89_mor *pb,
                                      cat89_mor *mor, cat89_mor *xa,
                                      cat89_mor *xb, int *out_med)
{
    cat89_mor *c1;
    cat89_mor *c2;
    int e1;
    int e2;
    int med;
    cat89_status st;

    c1 = NULL;
    c2 = NULL;
    med = 0;
    st = cat89_compose(cat, pa, mor, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(cat, pb, mor, &c2);
    if (st != CAT89_OK)
    {
        rel1(cat, c1);
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, xa, &e1);
    if (st != CAT89_OK)
    {
        rel2(cat, c1, c2);
        return st;
    }
    e2 = 0;
    st = cat89_mor_equal(eq, c2, xb, &e2);
    rel2(cat, c1, c2);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 1)
    {
        if (e2 == 1)
        {
            med = 1;
        }
    }
    *out_med = med;
    return CAT89_OK;
}

/* Tally one candidate census morphism index `i` (x -> p) into `*out_n`. */
static cat89_status med_tally_one(const struct census *cs, cat89_eq *eq,
                                  const cat89_obj *x, const cat89_obj *p,
                                  cat89_mor *pa, cat89_mor *pb, cat89_mor *xa,
                                  cat89_mor *xb, unsigned long i,
                                  unsigned long *out_n)
{
    int med;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, x) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, p) == 0)
    {
        return CAT89_OK;
    }
    med = 0;
    st = evaluate_mediator(cs->category, eq, pa, pb, cs->mors[i].mor, xa, xb,
                           &med);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (med == 1)
    {
        ++*out_n;
    }
    return CAT89_OK;
}

static cat89_status count_mediators(const struct census *cs, cat89_eq *eq,
                                    const cat89_obj *x, const cat89_obj *p,
                                    cat89_mor *pa, cat89_mor *pb, cat89_mor *xa,
                                    cat89_mor *xb, unsigned long *out_n)
{
    unsigned long i;
    unsigned long n;
    cat89_status st;

    n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = med_tally_one(cs, eq, x, p, pa, pb, xa, xb, i, &n);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_n = n;
    return CAT89_OK;
}

static cat89_status med_pick_one(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *x, const cat89_obj *p,
                                 cat89_mor *pa, cat89_mor *pb, cat89_mor *xa,
                                 cat89_mor *xb, unsigned long i,
                                 unsigned long *out_med)
{
    int med;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, x) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, p) == 0)
    {
        return CAT89_OK;
    }
    med = 0;
    st = evaluate_mediator(cs->category, eq, pa, pb, cs->mors[i].mor, xa, xb,
                           &med);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (med == 1)
    {
        *out_med = i;
    }
    return CAT89_OK;
}

static cat89_status single_mediator(const struct census *cs, cat89_eq *eq,
                                    const cat89_obj *x, const cat89_obj *p,
                                    cat89_mor *pa, cat89_mor *pb, cat89_mor *xa,
                                    cat89_mor *xb, unsigned long *out_idx)
{
    unsigned long i;
    unsigned long n;
    unsigned long med;
    cat89_status st;

    n = 0;
    st = count_mediators(cs, eq, x, p, pa, pb, xa, xb, &n);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (n != 1)
    {
        *out_idx = 0;
        return CAT89_OK;
    }
    med = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = med_pick_one(cs, eq, x, p, pa, pb, xa, xb, i, &med);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_idx = med;
    return CAT89_OK;
}

static cat89_status pair_one(const struct census *cs, cat89_eq *eq,
                             const cat89_obj *y, const cat89_obj *p,
                             cat89_mor *pa, cat89_mor *pb, cat89_mor *xa,
                             unsigned long bidx, int *out_ok)
{
    unsigned long cnt;
    cat89_status st;

    cnt = 0;
    st = count_mediators(cs, eq, y, p, pa, pb, xa, cs->mors[bidx].mor, &cnt);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (cnt == 0)
    {
        *out_ok = 0;
    }
    return CAT89_OK;
}

/* Every b-leg paired with xa must have a mediator (surjectivity on a x b). */
static cat89_status pair_covered(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *y, const cat89_obj *p,
                                 cat89_mor *pa, cat89_mor *pb, cat89_mor *xa,
                                 const unsigned long *bids, unsigned long nbid,
                                 int *out_ok)
{
    unsigned long k;
    int ok;
    cat89_status st;

    ok = 1;
    for (k = 0; k < nbid; k = k + 1)
    {
        st = pair_one(cs, eq, y, p, pa, pb, xa, bids[k], &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status obj_check(const struct census *cs, cat89_eq *eq,
                              const cat89_obj *y, const cat89_obj *p,
                              const cat89_obj *a, const cat89_obj *b,
                              cat89_mor *pa, cat89_mor *pb, int *out_ok)
{
    unsigned long *aids;
    unsigned long *bids;
    unsigned long na;
    unsigned long nb;
    unsigned long npy;
    unsigned long j;
    int ok;
    cat89_status st;

    aids = NULL;
    bids = NULL;
    na = 0;
    nb = 0;
    npy = 0;
    st = hom_count(cs, y, p, &npy);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = hom_count(cs, y, a, &na);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = hom_count(cs, y, b, &nb);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (npy != na * nb)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    st = gather_indices(cs, y, a, &aids, &na);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = gather_indices(cs, y, b, &bids, &nb);
    if (st != CAT89_OK)
    {
        indices_free(&cs->allocator, aids);
        return st;
    }
    ok = 1;
    for (j = 0; j < na; j = j + 1)
    {
        st = pair_covered(cs, eq, y, p, pa, pb, cs->mors[aids[j]].mor, bids, nb,
                          &ok);
        if (st != CAT89_OK)
        {
            rel_arrays(&cs->allocator, aids, bids);
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    rel_arrays(&cs->allocator, aids, bids);
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status is_product(const struct census *cs, cat89_eq *eq,
                               const cat89_obj *p, const cat89_obj *a,
                               const cat89_obj *b, cat89_mor *pa, cat89_mor *pb,
                               int *out_ok)
{
    unsigned long i;
    int ok;
    cat89_status st;

    ok = 1;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = obj_check(cs, eq, cs->objs[i], p, a, b, pa, pb, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

/* ----------------------------------------------------- result context */

static void pres_free(void *ctx)
{
    struct pres *rx = ctx;

    cat89_enum_release(rx->enumeration);
    cat89_eq_release(rx->eq);
    rel1(rx->category, rx->pa);
    rel1(rx->category, rx->pb);
    cat89_free(&rx->allocator, rx);
}

static cat89_mor *leg_mor(struct pres *rx, const cat89_obj *shape_obj)
{
    if (shape_obj == rx->pt_a)
    {
        return rx->pa;
    }
    if (shape_obj == rx->pt_b)
    {
        return rx->pb;
    }
    return NULL;
}

static cat89_status leg_ref(struct pres *rx, const cat89_obj *shape_obj,
                            cat89_mor **out_mor)
{
    cat89_mor *src;
    cat89_status st;

    src = leg_mor(rx, shape_obj);
    if (src == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_mor_retain(rx->category, src);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_mor = src;
    return CAT89_OK;
}

static cat89_status res_leg(void *ctx, const cat89_obj *shape_obj,
                            cat89_mor **out_mor)
{
    struct pres *rx = ctx;
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    st = leg_ref(rx, shape_obj, out_mor);
    return st;
}

static cat89_status factor_mediator(cat89_category *cat, cat89_enum *en,
                                    cat89_eq *eq, const cat89_obj *x,
                                    const cat89_obj *p, cat89_mor *pa,
                                    cat89_mor *pb, cat89_mor *xa, cat89_mor *xb,
                                    cat89_mor **out,
                                    const cat89_allocator *alloc)
{
    struct census cs;
    cat89_mor *keep;
    unsigned long n;
    unsigned long idx;
    cat89_status st;

    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    *out = NULL;
    keep = NULL;
    st = census_build(cat, en, alloc, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = count_mediators(&cs, eq, x, p, pa, pb, xa, xb, &n);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (n != 1)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    st = single_mediator(&cs, eq, x, p, pa, pb, xa, xb, &idx);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    keep = cs.mors[idx].mor;
    st = cat89_mor_retain(cat, keep);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    census_free(&cs);
    *out = keep;
    return CAT89_OK;
}

static cat89_status res_factor(void *ctx, const cat89_cone *candidate,
                               cat89_mor **out_mor)
{
    struct pres *rx = ctx;
    const cat89_obj *x;
    cat89_mor *xa;
    cat89_mor *xb;
    cat89_status st;

    x = NULL;
    xa = NULL;
    xb = NULL;
    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (candidate == NULL)
    {
        return CAT89_INVALID;
    }
    x = cat89_cone_apex(candidate);
    st = cat89_cone_leg(candidate, rx->pt_a, &xa);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cone_leg(candidate, rx->pt_b, &xb);
    if (st != CAT89_OK)
    {
        rel1(rx->category, xa);
        return st;
    }
    st = factor_mediator(rx->category, rx->enumeration, rx->eq, x, rx->apex,
                         rx->pa, rx->pb, xa, xb, out_mor, &rx->allocator);
    rel2(rx->category, xa, xb);
    return st;
}

static const cat89_cone_ops cone_ops = {res_leg, pres_free};

static const cat89_limit_ops limit_ops = {res_factor, NULL};

/* --------------------------------------------------------- search */

static void claim_legs(const struct census *cs, unsigned long paj,
                       unsigned long pbk, cat89_mor **out_pa,
                       cat89_mor **out_pb, int *out_found)
{
    *out_pa = cs->mors[paj].mor;
    *out_pb = cs->mors[pbk].mor;
    *out_found = 1;
}

static cat89_status try_apex(const struct census *cs, cat89_eq *eq,
                             const cat89_obj *a, const cat89_obj *b,
                             const cat89_obj *apex, cat89_mor **out_pa,
                             cat89_mor **out_pb, int *out_found)
{
    unsigned long *aids;
    unsigned long *bids;
    unsigned long na;
    unsigned long nb;
    unsigned long j;
    unsigned long k;
    int ok;
    int pair;
    cat89_status st;

    aids = NULL;
    bids = NULL;
    na = 0;
    nb = 0;
    *out_found = 0;
    st = gather_indices(cs, apex, a, &aids, &na);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = gather_indices(cs, apex, b, &bids, &nb);
    if (st != CAT89_OK)
    {
        indices_free(&cs->allocator, aids);
        return st;
    }
    pair = 0;
    for (j = 0; j < na; j = j + 1)
    {
        for (k = 0; k < nb; k = k + 1)
        {
            st = is_product(cs, eq, apex, a, b, cs->mors[aids[j]].mor,
                            cs->mors[bids[k]].mor, &ok);
            if (st != CAT89_OK)
            {
                rel_arrays(&cs->allocator, aids, bids);
                return st;
            }
            if (ok == 1)
            {
                claim_legs(cs, aids[j], bids[k], out_pa, out_pb, &pair);
            }
            if (pair == 1)
            {
                break;
            }
        }
        if (pair == 1)
        {
            break;
        }
    }
    rel_arrays(&cs->allocator, aids, bids);
    *out_found = pair;
    return CAT89_OK;
}

static void set_win(const cat89_obj **out_win, const cat89_obj *obj)
{
    *out_win = obj;
}

static cat89_status search_product(const struct census *cs, cat89_eq *eq,
                                   const cat89_obj *a, const cat89_obj *b,
                                   const cat89_obj **out_apex,
                                   cat89_mor **out_pa, cat89_mor **out_pb,
                                   int *out_found)
{
    unsigned long i;
    const cat89_obj *win;
    int found;
    cat89_status st;

    win = NULL;
    *out_found = 0;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = try_apex(cs, eq, a, b, cs->objs[i], out_pa, out_pb, &found);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (found == 1)
        {
            set_win(&win, cs->objs[i]);
            break;
        }
    }
    if (win == NULL)
    {
        return CAT89_OK;
    }
    *out_apex = win;
    *out_found = 1;
    return CAT89_OK;
}

/* ---------------------------------------------------------- builder */

static cat89_status grab_step(cat89_enum *en, cat89_obj_iter *it,
                              const cat89_obj **o0, const cat89_obj **o1,
                              int *out_done)
{
    const cat89_obj *obj;
    cat89_status st;

    obj = NULL;
    st = cat89_obj_iter_next(en, it, &obj, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    if (*o0 == NULL)
    {
        *o0 = obj;
    }
    if (*o1 == NULL)
    {
        if (*o0 != obj)
        {
            *o1 = obj;
        }
    }
    return CAT89_OK;
}

static cat89_status grab_two(cat89_enum *en, const cat89_obj **out0,
                             const cat89_obj **out1)
{
    cat89_obj_iter *it;
    const cat89_obj *o0;
    const cat89_obj *o1;
    int done;
    cat89_status st;

    it = NULL;
    o0 = NULL;
    o1 = NULL;
    *out0 = NULL;
    *out1 = NULL;
    st = cat89_obj_iter_open(en, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = grab_step(en, it, &o0, &o1, &done);
        if (st != CAT89_OK)
        {
            cat89_obj_iter_close(en, it);
            return st;
        }
    }
    cat89_obj_iter_close(en, it);
    *out0 = o0;
    *out1 = o1;
    return CAT89_OK;
}

/* Release every live handle owned by the builder up to the current stage.
 * Handles are NULLed as they are transferred, so nothing is freed twice. */
static void build_release(struct pres *rx, cat89_category *shape,
                          cat89_enum *shapenum, struct dctx *dx,
                          cat89_functor *diagram, cat89_cone *cone,
                          struct census *cs)
{
    if (rx != NULL)
    {
        pres_free(rx);
    }
    if (diagram != NULL)
    {
        cat89_functor_release(diagram);
    }
    if (dx != NULL)
    {
        dmap_destroy(dx);
    }
    if (shape != NULL)
    {
        cat89_category_release(shape);
    }
    if (shapenum != NULL)
    {
        cat89_enum_release(shapenum);
    }
    if (cone != NULL)
    {
        cat89_cone_release(cone);
    }
    if (cs != NULL)
    {
        census_free(cs);
    }
}

cat89_status cat89_find_binary_product(cat89_category *category,
                                       cat89_enum *enumeration, cat89_eq *eq,
                                       const cat89_obj *a, const cat89_obj *b,
                                       const cat89_allocator *allocator,
                                       cat89_limit **out_limit,
                                       const cat89_obj **out_point_a,
                                       const cat89_obj **out_point_b)
{
    const cat89_allocator *alloc;
    struct census cs;
    struct pres *rx;
    cat89_category *shape;
    cat89_enum *shapenum;
    struct dctx *dx;
    cat89_functor *diagram;
    cat89_cone *cone;
    cat89_limit *limit;
    const cat89_obj *apex;
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    cat89_mor *pa;
    cat89_mor *pb;
    int found;
    cat89_status st;

    alloc = resolve_alloc(allocator);
    if (out_limit == NULL)
    {
        return CAT89_INVALID;
    }
    *out_limit = NULL;
    if (out_point_a != NULL)
    {
        *out_point_a = NULL;
    }
    if (out_point_b != NULL)
    {
        *out_point_b = NULL;
    }
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (a == NULL)
    {
        return CAT89_INVALID;
    }
    if (b == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(category, a) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(category, b) == 0)
    {
        return CAT89_INVALID;
    }
    rx = NULL;
    shape = NULL;
    shapenum = NULL;
    dx = NULL;
    diagram = NULL;
    cone = NULL;
    apex = NULL;
    pa = NULL;
    pb = NULL;
    pt_a = NULL;
    pt_b = NULL;
    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    st = census_build(category, enumeration, alloc, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = search_product(&cs, eq, a, b, &apex, &pa, &pb, &found);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (!found)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }

    st = cat89_shape_category_new(CAT89_SHAPE_DISCRETE2, alloc, &shape, NULL,
                                  &shapenum);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = grab_two(shapenum, &pt_a, &pt_b);
    if (st != CAT89_OK)
    {
        build_release(NULL, shape, shapenum, NULL, NULL, NULL, &cs);
        return st;
    }
    dx = cat89_alloc(alloc, sizeof(*dx));
    if (dx == NULL)
    {
        build_release(NULL, shape, shapenum, NULL, NULL, NULL, &cs);
        return CAT89_NOMEM;
    }
    dx->pt_a = pt_a;
    dx->pt_b = pt_b;
    dx->a = a;
    dx->b = b;
    dx->allocator = *alloc;
    st = cat89_functor_new(shape, category, &d_ops, dx, alloc, &diagram);
    if (st != CAT89_OK)
    {
        build_release(NULL, shape, shapenum, dx, NULL, NULL, &cs);
        return st;
    }
    cat89_category_release(shape);
    cat89_enum_release(shapenum);
    shape = NULL;
    shapenum = NULL;
    dx = NULL;

    rx = cat89_alloc(alloc, sizeof(*rx));
    if (rx == NULL)
    {
        build_release(NULL, NULL, NULL, NULL, diagram, NULL, &cs);
        return CAT89_NOMEM;
    }
    rx->category = category;
    rx->pt_a = pt_a;
    rx->pt_b = pt_b;
    rx->apex = apex;
    rx->enumeration = NULL;
    rx->eq = NULL;
    rx->pa = NULL;
    rx->pb = NULL;
    rx->allocator = *alloc;
    st = cat89_enum_retain(enumeration);
    if (st != CAT89_OK)
    {
        build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    rx->enumeration = enumeration;
    st = cat89_eq_retain(eq);
    if (st != CAT89_OK)
    {
        build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    rx->eq = eq;
    st = cat89_mor_retain(category, pa);
    if (st != CAT89_OK)
    {
        build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    rx->pa = pa;
    st = cat89_mor_retain(category, pb);
    if (st != CAT89_OK)
    {
        build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    rx->pb = pb;

    st = cat89_cone_new(diagram, apex, &cone_ops, rx, alloc, &cone);
    if (st != CAT89_OK)
    {
        build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    cat89_functor_release(diagram);
    diagram = NULL;

    st = cat89_limit_new(cone, &limit_ops, rx, alloc, &limit);
    if (st != CAT89_OK)
    {
        build_release(NULL, NULL, NULL, NULL, NULL, cone, &cs);
        return st;
    }
    cat89_cone_release(cone);
    cone = NULL;
    rx = NULL;
    census_free(&cs);
    *out_limit = limit;
    if (out_point_a != NULL)
    {
        *out_point_a = pt_a;
    }
    if (out_point_b != NULL)
    {
        *out_point_b = pt_b;
    }
    return CAT89_OK;
}

/* ====================================================== binary coproduct
 * The dual of the binary product: a coproduct S of a, b with injections
 * ia : a -> S and ib : b -> S is an apex such that for every object Y the map
 *     u |-> (u o ia, u o ib) : Hom(S,Y) -> Hom(a,Y) x Hom(b,Y)
 * is a bijection; with |Hom(S,Y)| == |Hom(a,Y)| * |Hom(b,Y)| bijectivity
 * follows from surjectivity, so the factor of every cocone (Y, xa, xb) is
 * unique. Reuses the shared census and the generic result ctx (pres holds the
 * two legs; here pa/pb are the injections ia, ib). */

static cat89_status evaluate_col(cat89_category *cat, cat89_eq *eq,
                                 cat89_mor *ia, cat89_mor *ib, cat89_mor *u,
                                 cat89_mor *xa, cat89_mor *xb, int *out_med)
{
    cat89_mor *c1;
    cat89_mor *c2;
    int e1;
    int e2;
    int med;
    cat89_status st;

    c1 = NULL;
    c2 = NULL;
    med = 0;
    st = cat89_compose(cat, u, ia, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(cat, u, ib, &c2);
    if (st != CAT89_OK)
    {
        rel1(cat, c1);
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, xa, &e1);
    if (st != CAT89_OK)
    {
        rel2(cat, c1, c2);
        return st;
    }
    e2 = 0;
    st = cat89_mor_equal(eq, c2, xb, &e2);
    rel2(cat, c1, c2);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 1)
    {
        if (e2 == 1)
        {
            med = 1;
        }
    }
    *out_med = med;
    return CAT89_OK;
}

static cat89_status col_tally_one(const struct census *cs, cat89_eq *eq,
                                  const cat89_obj *s, const cat89_obj *y,
                                  cat89_mor *ia, cat89_mor *ib, cat89_mor *xa,
                                  cat89_mor *xb, unsigned long i,
                                  unsigned long *out_n)
{
    int med;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, s) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, y) == 0)
    {
        return CAT89_OK;
    }
    med = 0;
    st = evaluate_col(cs->category, eq, ia, ib, cs->mors[i].mor, xa, xb, &med);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (med == 1)
    {
        ++*out_n;
    }
    return CAT89_OK;
}

static cat89_status count_col(const struct census *cs, cat89_eq *eq,
                              const cat89_obj *s, const cat89_obj *y,
                              cat89_mor *ia, cat89_mor *ib, cat89_mor *xa,
                              cat89_mor *xb, unsigned long *out_n)
{
    unsigned long i;
    unsigned long n;
    cat89_status st;

    n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = col_tally_one(cs, eq, s, y, ia, ib, xa, xb, i, &n);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_n = n;
    return CAT89_OK;
}

static cat89_status col_pick_one(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *s, const cat89_obj *y,
                                 cat89_mor *ia, cat89_mor *ib, cat89_mor *xa,
                                 cat89_mor *xb, unsigned long i,
                                 unsigned long *out_med)
{
    int med;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, s) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, y) == 0)
    {
        return CAT89_OK;
    }
    med = 0;
    st = evaluate_col(cs->category, eq, ia, ib, cs->mors[i].mor, xa, xb, &med);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (med == 1)
    {
        *out_med = i;
    }
    return CAT89_OK;
}

static cat89_status single_col(const struct census *cs, cat89_eq *eq,
                               const cat89_obj *s, const cat89_obj *y,
                               cat89_mor *ia, cat89_mor *ib, cat89_mor *xa,
                               cat89_mor *xb, unsigned long *out_idx)
{
    unsigned long i;
    unsigned long n;
    unsigned long med;
    cat89_status st;

    n = 0;
    st = count_col(cs, eq, s, y, ia, ib, xa, xb, &n);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (n != 1)
    {
        *out_idx = 0;
        return CAT89_OK;
    }
    med = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = col_pick_one(cs, eq, s, y, ia, ib, xa, xb, i, &med);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_idx = med;
    return CAT89_OK;
}

static cat89_status col_pair_one(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *s, const cat89_obj *y,
                                 cat89_mor *ia, cat89_mor *ib, cat89_mor *xa,
                                 unsigned long bidx, int *out_ok)
{
    unsigned long cnt;
    cat89_status st;

    cnt = 0;
    st = count_col(cs, eq, s, y, ia, ib, xa, cs->mors[bidx].mor, &cnt);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (cnt == 0)
    {
        *out_ok = 0;
    }
    return CAT89_OK;
}

static cat89_status col_pair_covered(const struct census *cs, cat89_eq *eq,
                                     const cat89_obj *s, const cat89_obj *y,
                                     cat89_mor *ia, cat89_mor *ib,
                                     cat89_mor *xa, const unsigned long *bids,
                                     unsigned long nbid, int *out_ok)
{
    unsigned long k;
    int ok;
    cat89_status st;

    ok = 1;
    for (k = 0; k < nbid; k = k + 1)
    {
        st = col_pair_one(cs, eq, s, y, ia, ib, xa, bids[k], &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status obj_check_col(const struct census *cs, cat89_eq *eq,
                                  const cat89_obj *y, const cat89_obj *s,
                                  const cat89_obj *a, const cat89_obj *b,
                                  cat89_mor *ia, cat89_mor *ib, int *out_ok)
{
    unsigned long *aids;
    unsigned long *bids;
    unsigned long na;
    unsigned long nb;
    unsigned long nsy;
    unsigned long j;
    int ok;
    cat89_status st;

    aids = NULL;
    bids = NULL;
    na = 0;
    nb = 0;
    nsy = 0;
    st = hom_count(cs, s, y, &nsy);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = hom_count(cs, a, y, &na);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = hom_count(cs, b, y, &nb);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (nsy != na * nb)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    st = gather_indices(cs, a, y, &aids, &na);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = gather_indices(cs, b, y, &bids, &nb);
    if (st != CAT89_OK)
    {
        indices_free(&cs->allocator, aids);
        return st;
    }
    ok = 1;
    for (j = 0; j < na; j = j + 1)
    {
        st = col_pair_covered(cs, eq, s, y, ia, ib, cs->mors[aids[j]].mor, bids,
                              nb, &ok);
        if (st != CAT89_OK)
        {
            rel_arrays(&cs->allocator, aids, bids);
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    rel_arrays(&cs->allocator, aids, bids);
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status is_coproduct(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *s, const cat89_obj *a,
                                 const cat89_obj *b, cat89_mor *ia,
                                 cat89_mor *ib, int *out_ok)
{
    unsigned long i;
    int ok;
    cat89_status st;

    ok = 1;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = obj_check_col(cs, eq, cs->objs[i], s, a, b, ia, ib, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static void claim_col_legs(const struct census *cs, unsigned long iaj,
                           unsigned long ibk, cat89_mor **out_ia,
                           cat89_mor **out_ib, int *out_found)
{
    *out_ia = cs->mors[iaj].mor;
    *out_ib = cs->mors[ibk].mor;
    *out_found = 1;
}

static cat89_status try_apex_col(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *a, const cat89_obj *b,
                                 const cat89_obj *s, cat89_mor **out_ia,
                                 cat89_mor **out_ib, int *out_found)
{
    unsigned long *aids;
    unsigned long *bids;
    unsigned long na;
    unsigned long nb;
    unsigned long j;
    unsigned long k;
    int ok;
    int pair;
    cat89_status st;

    aids = NULL;
    bids = NULL;
    na = 0;
    nb = 0;
    *out_found = 0;
    st = gather_indices(cs, a, s, &aids, &na);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = gather_indices(cs, b, s, &bids, &nb);
    if (st != CAT89_OK)
    {
        indices_free(&cs->allocator, aids);
        return st;
    }
    pair = 0;
    for (j = 0; j < na; j = j + 1)
    {
        for (k = 0; k < nb; k = k + 1)
        {
            st = is_coproduct(cs, eq, s, a, b, cs->mors[aids[j]].mor,
                              cs->mors[bids[k]].mor, &ok);
            if (st != CAT89_OK)
            {
                rel_arrays(&cs->allocator, aids, bids);
                return st;
            }
            if (ok == 1)
            {
                claim_col_legs(cs, aids[j], bids[k], out_ia, out_ib, &pair);
            }
            if (pair == 1)
            {
                break;
            }
        }
        if (pair == 1)
        {
            break;
        }
    }
    rel_arrays(&cs->allocator, aids, bids);
    *out_found = pair;
    return CAT89_OK;
}

static cat89_status search_coproduct(const struct census *cs, cat89_eq *eq,
                                     const cat89_obj *a, const cat89_obj *b,
                                     const cat89_obj **out_apex,
                                     cat89_mor **out_ia, cat89_mor **out_ib,
                                     int *out_found)
{
    unsigned long i;
    const cat89_obj *win;
    int found;
    cat89_status st;

    win = NULL;
    *out_found = 0;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = try_apex_col(cs, eq, a, b, cs->objs[i], out_ia, out_ib, &found);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (found == 1)
        {
            set_win(&win, cs->objs[i]);
            break;
        }
    }
    if (win == NULL)
    {
        return CAT89_OK;
    }
    *out_apex = win;
    *out_found = 1;
    return CAT89_OK;
}

static cat89_status factor_mediator_col(cat89_category *cat, cat89_enum *en,
                                        cat89_eq *eq, const cat89_obj *s,
                                        const cat89_obj *y, cat89_mor *ia,
                                        cat89_mor *ib, cat89_mor *xa,
                                        cat89_mor *xb, cat89_mor **out,
                                        const cat89_allocator *alloc)
{
    struct census cs;
    cat89_mor *keep;
    unsigned long n;
    unsigned long idx;
    cat89_status st;

    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    *out = NULL;
    keep = NULL;
    st = census_build(cat, en, alloc, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = count_col(&cs, eq, s, y, ia, ib, xa, xb, &n);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (n != 1)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    st = single_col(&cs, eq, s, y, ia, ib, xa, xb, &idx);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    keep = cs.mors[idx].mor;
    st = cat89_mor_retain(cat, keep);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    census_free(&cs);
    *out = keep;
    return CAT89_OK;
}

static cat89_status col_factor(void *ctx, const cat89_cocone *candidate,
                               cat89_mor **out_mor)
{
    struct pres *rx = ctx;
    const cat89_obj *y;
    cat89_mor *xa;
    cat89_mor *xb;
    cat89_status st;

    y = NULL;
    xa = NULL;
    xb = NULL;
    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (candidate == NULL)
    {
        return CAT89_INVALID;
    }
    y = cat89_cocone_apex(candidate);
    st = cat89_cocone_leg(candidate, rx->pt_a, &xa);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cocone_leg(candidate, rx->pt_b, &xb);
    if (st != CAT89_OK)
    {
        rel1(rx->category, xa);
        return st;
    }
    st = factor_mediator_col(rx->category, rx->enumeration, rx->eq, rx->apex, y,
                             rx->pa, rx->pb, xa, xb, out_mor, &rx->allocator);
    rel2(rx->category, xa, xb);
    return st;
}

static const cat89_colimit_ops colimit_ops = {col_factor, NULL};

static void col_build_release(struct pres *rx, cat89_category *shape,
                              cat89_enum *shapenum, struct dctx *dx,
                              cat89_functor *diagram, cat89_cocone *cocone,
                              struct census *cs)
{
    if (rx != NULL)
    {
        pres_free(rx);
    }
    if (diagram != NULL)
    {
        cat89_functor_release(diagram);
    }
    if (dx != NULL)
    {
        dmap_destroy(dx);
    }
    if (shape != NULL)
    {
        cat89_category_release(shape);
    }
    if (shapenum != NULL)
    {
        cat89_enum_release(shapenum);
    }
    if (cocone != NULL)
    {
        cat89_cocone_release(cocone);
    }
    if (cs != NULL)
    {
        census_free(cs);
    }
}

cat89_status cat89_find_binary_coproduct(cat89_category *category,
                                         cat89_enum *enumeration, cat89_eq *eq,
                                         const cat89_obj *a, const cat89_obj *b,
                                         const cat89_allocator *allocator,
                                         cat89_colimit **out_colimit,
                                         const cat89_obj **out_point_a,
                                         const cat89_obj **out_point_b)
{
    const cat89_allocator *alloc;
    struct census cs;
    struct pres *rx;
    cat89_category *shape;
    cat89_enum *shapenum;
    struct dctx *dx;
    cat89_functor *diagram;
    cat89_cocone *cocone;
    cat89_colimit *colimit;
    const cat89_obj *apex;
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    cat89_mor *ia;
    cat89_mor *ib;
    int found;
    cat89_status st;

    alloc = resolve_alloc(allocator);
    if (out_colimit == NULL)
    {
        return CAT89_INVALID;
    }
    *out_colimit = NULL;
    if (out_point_a != NULL)
    {
        *out_point_a = NULL;
    }
    if (out_point_b != NULL)
    {
        *out_point_b = NULL;
    }
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (a == NULL)
    {
        return CAT89_INVALID;
    }
    if (b == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(category, a) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(category, b) == 0)
    {
        return CAT89_INVALID;
    }
    rx = NULL;
    shape = NULL;
    shapenum = NULL;
    dx = NULL;
    diagram = NULL;
    cocone = NULL;
    apex = NULL;
    ia = NULL;
    ib = NULL;
    pt_a = NULL;
    pt_b = NULL;
    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    st = census_build(category, enumeration, alloc, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = search_coproduct(&cs, eq, a, b, &apex, &ia, &ib, &found);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (!found)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }

    st = cat89_shape_category_new(CAT89_SHAPE_DISCRETE2, alloc, &shape, NULL,
                                  &shapenum);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = grab_two(shapenum, &pt_a, &pt_b);
    if (st != CAT89_OK)
    {
        col_build_release(NULL, shape, shapenum, NULL, NULL, NULL, &cs);
        return st;
    }
    dx = cat89_alloc(alloc, sizeof(*dx));
    if (dx == NULL)
    {
        col_build_release(NULL, shape, shapenum, NULL, NULL, NULL, &cs);
        return CAT89_NOMEM;
    }
    dx->pt_a = pt_a;
    dx->pt_b = pt_b;
    dx->a = a;
    dx->b = b;
    dx->allocator = *alloc;
    st = cat89_functor_new(shape, category, &d_ops, dx, alloc, &diagram);
    if (st != CAT89_OK)
    {
        col_build_release(NULL, shape, shapenum, dx, NULL, NULL, &cs);
        return st;
    }
    cat89_category_release(shape);
    cat89_enum_release(shapenum);
    shape = NULL;
    shapenum = NULL;
    dx = NULL;

    rx = cat89_alloc(alloc, sizeof(*rx));
    if (rx == NULL)
    {
        col_build_release(NULL, NULL, NULL, NULL, diagram, NULL, &cs);
        return CAT89_NOMEM;
    }
    rx->category = category;
    rx->pt_a = pt_a;
    rx->pt_b = pt_b;
    rx->apex = apex;
    rx->enumeration = NULL;
    rx->eq = NULL;
    rx->pa = NULL;
    rx->pb = NULL;
    rx->allocator = *alloc;
    st = cat89_enum_retain(enumeration);
    if (st != CAT89_OK)
    {
        col_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    rx->enumeration = enumeration;
    st = cat89_eq_retain(eq);
    if (st != CAT89_OK)
    {
        col_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    rx->eq = eq;
    st = cat89_mor_retain(category, ia);
    if (st != CAT89_OK)
    {
        col_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    rx->pa = ia;
    st = cat89_mor_retain(category, ib);
    if (st != CAT89_OK)
    {
        col_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    rx->pb = ib;

    st = cat89_cocone_new(diagram, apex, &cone_ops, rx, alloc, &cocone);
    if (st != CAT89_OK)
    {
        col_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    cat89_functor_release(diagram);
    diagram = NULL;

    st = cat89_colimit_new(cocone, &colimit_ops, rx, alloc, &colimit);
    if (st != CAT89_OK)
    {
        col_build_release(NULL, NULL, NULL, NULL, NULL, cocone, &cs);
        return st;
    }
    cat89_cocone_release(cocone);
    cocone = NULL;
    rx = NULL;
    census_free(&cs);
    *out_colimit = colimit;
    if (out_point_a != NULL)
    {
        *out_point_a = pt_a;
    }
    if (out_point_b != NULL)
    {
        *out_point_b = pt_b;
    }
    return CAT89_OK;
}

/* ============================================================ equalizer
 * For parallel arrows f, g : X -> Y, an equalizer is an apex E with a leg
 * e : E -> X such that f o e == g o e and E is universal: for every object W
 * the map  u |-> e o u : Hom(W,E) -> { w : W -> X | f o w == g o w }  is a
 * bijection. With equal finite cardinalities bijectivity follows from
 * surjectivity, so the factor of every equalizing cone is unique. */

static cat89_status eqz_equalizes(cat89_category *cat, cat89_eq *eq,
                                  cat89_mor *f, cat89_mor *g, cat89_mor *mor,
                                  int *out_eq)
{
    cat89_mor *c1;
    cat89_mor *c2;
    int e1;
    cat89_status st;

    c1 = NULL;
    c2 = NULL;
    e1 = 0;
    st = cat89_compose(cat, f, mor, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(cat, g, mor, &c2);
    if (st != CAT89_OK)
    {
        rel1(cat, c1);
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, c2, &e1);
    rel2(cat, c1, c2);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_eq = e1;
    return CAT89_OK;
}

static cat89_status eqz_arrow_step(const struct census *cs, cat89_eq *eq,
                                   cat89_mor *f, cat89_mor *g,
                                   const cat89_obj *x, unsigned long i,
                                   int *out_eq)
{
    cat89_mor *mor;
    int eqv;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].cod, x) == 0)
    {
        *out_eq = 0;
        return CAT89_OK;
    }
    mor = cs->mors[i].mor;
    eqv = 0;
    st = eqz_equalizes(cs->category, eq, f, g, mor, &eqv);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_eq = eqv;
    return CAT89_OK;
}

/* Number of arrows W -> X equalizing f, g. */
static cat89_status eqz_count(const struct census *cs, cat89_eq *eq,
                              const cat89_obj *w, const cat89_obj *x,
                              cat89_mor *f, cat89_mor *g, unsigned long *out_n)
{
    unsigned long i;
    unsigned long n;
    int eqv;
    cat89_status st;

    n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        if (cat89_obj_same(cs->category, cs->mors[i].dom, w) == 0)
        {
            continue;
        }
        st = eqz_arrow_step(cs, eq, f, g, x, i, &eqv);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (eqv == 1)
        {
            ++n;
        }
    }
    *out_n = n;
    return CAT89_OK;
}

static cat89_status eqz_gather_one(const struct census *cs, cat89_eq *eq,
                                   cat89_mor *f, cat89_mor *g,
                                   const cat89_obj *w, const cat89_obj *x,
                                   unsigned long i, cat89_vec *vec)
{
    unsigned long idx;
    int eqv;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, w) == 0)
    {
        return CAT89_OK;
    }
    st = eqz_arrow_step(cs, eq, f, g, x, i, &eqv);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (eqv == 0)
    {
        return CAT89_OK;
    }
    idx = i;
    st = cat89_vec_push(vec, &cs->allocator, &idx);
    return st;
}

static cat89_status eqz_gather(const struct census *cs, cat89_eq *eq,
                               const cat89_obj *w, const cat89_obj *x,
                               cat89_mor *f, cat89_mor *g,
                               unsigned long **out_arr, unsigned long *out_n)
{
    cat89_vec vec;
    unsigned long i;
    cat89_status st;

    cat89_vec_init(&vec, sizeof(unsigned long));
    *out_arr = NULL;
    *out_n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = eqz_gather_one(cs, eq, f, g, w, x, i, &vec);
        if (st != CAT89_OK)
        {
            gather_abort(&vec, &cs->allocator);
            return st;
        }
    }
    *out_arr = (unsigned long *)vec.data;
    *out_n = vec.len;
    return CAT89_OK;
}

/* Mediator count: arrows u : W -> apex with e o u == tar (single equation). */
static cat89_status eqz_med_one(const struct census *cs, cat89_eq *eq,
                                const cat89_obj *w, const cat89_obj *apex,
                                cat89_mor *e, cat89_mor *tar, unsigned long i,
                                unsigned long *out_n)
{
    cat89_mor *u;
    cat89_mor *c1;
    int e1;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, w) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, apex) == 0)
    {
        return CAT89_OK;
    }
    u = cs->mors[i].mor;
    c1 = NULL;
    st = cat89_compose(cs->category, e, u, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, tar, &e1);
    rel1(cs->category, c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 1)
    {
        ++*out_n;
    }
    return CAT89_OK;
}

static cat89_status eqz_med_count(const struct census *cs, cat89_eq *eq,
                                  const cat89_obj *w, const cat89_obj *apex,
                                  cat89_mor *e, cat89_mor *tar,
                                  unsigned long *out_n)
{
    unsigned long i;
    unsigned long n;
    cat89_status st;

    n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = eqz_med_one(cs, eq, w, apex, e, tar, i, &n);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_n = n;
    return CAT89_OK;
}

static cat89_status eqz_covered_one(const struct census *cs, cat89_eq *eq,
                                    const cat89_obj *w, const cat89_obj *apex,
                                    cat89_mor *e, const unsigned long *eqi,
                                    unsigned long k, int *out_ok)
{
    unsigned long cnt;
    cat89_status st;

    cnt = 0;
    st = eqz_med_count(cs, eq, w, apex, e, cs->mors[eqi[k]].mor, &cnt);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (cnt == 0)
    {
        *out_ok = 0;
    }
    return CAT89_OK;
}

static cat89_status eqz_covered(const struct census *cs, cat89_eq *eq,
                                const cat89_obj *w, const cat89_obj *apex,
                                cat89_mor *e, const unsigned long *eqi,
                                unsigned long neq, int *out_ok)
{
    unsigned long k;
    int ok;
    cat89_status st;

    ok = 1;
    for (k = 0; k < neq; k = k + 1)
    {
        st = eqz_covered_one(cs, eq, w, apex, e, eqi, k, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status eqz_obj_ok(const struct census *cs, cat89_eq *eq,
                               const cat89_obj *w, const cat89_obj *x,
                               const cat89_obj *apex, cat89_mor *e,
                               cat89_mor *f, cat89_mor *g, int *out_ok)
{
    unsigned long *eqi;
    unsigned long neq;
    unsigned long na;
    int ok;
    cat89_status st;

    eqi = NULL;
    neq = 0;
    na = 0;
    st = eqz_count(cs, eq, w, x, f, g, &neq);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = hom_count(cs, w, apex, &na);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (na != neq)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    st = eqz_gather(cs, eq, w, x, f, g, &eqi, &neq);
    if (st != CAT89_OK)
    {
        return st;
    }
    ok = 1;
    st = eqz_covered(cs, eq, w, apex, e, eqi, neq, &ok);
    indices_free(&cs->allocator, eqi);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_ok = ok;
    return CAT89_OK;
}

/* Is (apex, e) a genuine equalizer of f, g over the ambient? */
static cat89_status is_equalizer(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *apex, const cat89_obj *x,
                                 cat89_mor *f, cat89_mor *g, cat89_mor *e,
                                 int *out_ok)
{
    unsigned long i;
    int eqv;
    int ok;
    cat89_status st;

    eqv = 0;
    st = eqz_equalizes(cs->category, eq, f, g, e, &eqv);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (eqv == 0)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    ok = 1;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = eqz_obj_ok(cs, eq, cs->objs[i], x, apex, e, f, g, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

/* equalizer factor: candidate cone apex W, legs z0 (pt_a -> X), z1. The unique
 * mediator u : W -> apex with e o u == z0. */
static cat89_status eqz_pick_one(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *w, const cat89_obj *apex,
                                 cat89_mor *e, cat89_mor *tar, unsigned long i,
                                 unsigned long *out_med)
{
    cat89_mor *u;
    cat89_mor *c1;
    int e1;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, w) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, apex) == 0)
    {
        return CAT89_OK;
    }
    u = cs->mors[i].mor;
    c1 = NULL;
    st = cat89_compose(cs->category, e, u, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, tar, &e1);
    rel1(cs->category, c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 1)
    {
        *out_med = i;
    }
    return CAT89_OK;
}

static cat89_status eqz_single_idx(const struct census *cs, cat89_eq *eq,
                                   const cat89_obj *w, const cat89_obj *apex,
                                   cat89_mor *e, cat89_mor *tar,
                                   unsigned long *out_idx, unsigned long *out_n)
{
    unsigned long i;
    unsigned long found;
    unsigned long n;
    cat89_status st;

    n = 0;
    st = eqz_med_count(cs, eq, w, apex, e, tar, &n);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_n = n;
    if (n != 1)
    {
        *out_idx = 0;
        return CAT89_OK;
    }
    found = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = eqz_pick_one(cs, eq, w, apex, e, tar, i, &found);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_idx = found;
    return CAT89_OK;
}

static cat89_status eqz_factor(cat89_category *cat, cat89_enum *en,
                               cat89_eq *eq, const cat89_obj *w,
                               const cat89_obj *apex, cat89_mor *e,
                               cat89_mor *z0, cat89_mor **out,
                               const cat89_allocator *alloc)
{
    struct census cs;
    cat89_mor *keep;
    unsigned long idx;
    unsigned long n;
    cat89_status st;

    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    *out = NULL;
    keep = NULL;
    st = census_build(cat, en, alloc, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = eqz_single_idx(&cs, eq, w, apex, e, z0, &idx, &n);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (n != 1)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    keep = cs.mors[idx].mor;
    st = cat89_mor_retain(cat, keep);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    census_free(&cs);
    *out = keep;
    return CAT89_OK;
}

static cat89_status eqz_limit_factor(void *ctx, const cat89_cone *candidate,
                                     cat89_mor **out_mor)
{
    struct pres *rx = ctx;
    const cat89_obj *w;
    cat89_mor *z0;
    cat89_status st;

    w = NULL;
    z0 = NULL;
    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (candidate == NULL)
    {
        return CAT89_INVALID;
    }
    w = cat89_cone_apex(candidate);
    st = cat89_cone_leg(candidate, rx->pt_a, &z0);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = eqz_factor(rx->category, rx->enumeration, rx->eq, w, rx->apex, rx->pa,
                    z0, out_mor, &rx->allocator);
    rel1(rx->category, z0);
    return st;
}

static const cat89_limit_ops equalizer_ops = {eqz_limit_factor, NULL};

static void claim_eqz_leg(const struct census *cs, unsigned long ai,
                          cat89_mor **out_leg, int *out_found)
{
    *out_leg = cs->mors[ai].mor;
    *out_found = 1;
}

static cat89_status search_equalizer(const struct census *cs, cat89_eq *eq,
                                     const cat89_obj *x, cat89_mor *f,
                                     cat89_mor *g, const cat89_obj **out_apex,
                                     cat89_mor **out_leg, int *out_found)
{
    unsigned long *aids;
    unsigned long na;
    unsigned long i;
    unsigned long j;
    const cat89_obj *win;
    int ok;
    int pair;
    cat89_status st;

    aids = NULL;
    na = 0;
    win = NULL;
    *out_found = 0;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = gather_indices(cs, cs->objs[i], x, &aids, &na);
        if (st != CAT89_OK)
        {
            return st;
        }
        pair = 0;
        for (j = 0; j < na; j = j + 1)
        {
            st = is_equalizer(cs, eq, cs->objs[i], x, f, g,
                              cs->mors[aids[j]].mor, &ok);
            if (st != CAT89_OK)
            {
                indices_free(&cs->allocator, aids);
                return st;
            }
            if (ok == 1)
            {
                claim_eqz_leg(cs, aids[j], out_leg, &pair);
            }
            if (pair == 1)
            {
                break;
            }
        }
        indices_free(&cs->allocator, aids);
        if (pair == 1)
        {
            set_win(&win, cs->objs[i]);
            break;
        }
    }
    if (win == NULL)
    {
        return CAT89_OK;
    }
    *out_apex = win;
    *out_found = 1;
    return CAT89_OK;
}

static void eqz_abort(cat89_category *cat, struct pres *rx,
                      cat89_category *shape, cat89_enum *shapenum,
                      struct dctx *dx, cat89_functor *diagram, cat89_cone *cone,
                      struct census *cs, cat89_mor *yl)
{
    build_release(rx, shape, shapenum, dx, diagram, cone, cs);
    rel1(cat, yl);
}

static cat89_status
build_equalizer_limit(cat89_category *cat, cat89_enum *enumeration,
                      cat89_eq *eq, cat89_mor *f, cat89_mor *g,
                      const cat89_allocator *alloc, cat89_limit **out_limit,
                      const cat89_obj **out_pt_a, const cat89_obj **out_pt_b)
{
    const cat89_allocator *actual;
    struct census cs;
    struct pres *rx;
    cat89_category *shape;
    cat89_enum *shapenum;
    struct dctx *dx;
    cat89_functor *diagram;
    cat89_cone *cone;
    cat89_limit *limit;
    const cat89_obj *x;
    const cat89_obj *apex;
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    cat89_mor *e;
    cat89_mor *yl;
    int found;
    cat89_status st;

    actual = resolve_alloc(alloc);
    rx = NULL;
    shape = NULL;
    shapenum = NULL;
    dx = NULL;
    diagram = NULL;
    cone = NULL;
    x = NULL;
    apex = NULL;
    pt_a = NULL;
    pt_b = NULL;
    e = NULL;
    yl = NULL;
    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    st = census_build(cat, enumeration, actual, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_dom(cat, f, &x);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = search_equalizer(&cs, eq, x, f, g, &apex, &e, &found);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (!found)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    st = cat89_compose(cat, f, e, &yl);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = cat89_shape_category_new(CAT89_SHAPE_PARALLEL, actual, &shape, NULL,
                                  &shapenum);
    if (st != CAT89_OK)
    {
        eqz_abort(cat, NULL, NULL, NULL, NULL, NULL, NULL, &cs, yl);
        return st;
    }
    st = grab_two(shapenum, &pt_a, &pt_b);
    if (st != CAT89_OK)
    {
        eqz_abort(cat, NULL, shape, shapenum, NULL, NULL, NULL, &cs, yl);
        return st;
    }
    dx = cat89_alloc(actual, sizeof(*dx));
    if (dx == NULL)
    {
        eqz_abort(cat, NULL, shape, shapenum, NULL, NULL, NULL, &cs, yl);
        return CAT89_NOMEM;
    }
    dx->pt_a = pt_a;
    dx->pt_b = pt_b;
    dx->a = x;
    dx->allocator = *actual;
    st = cat89_cod(cat, f, &dx->b);
    if (st != CAT89_OK)
    {
        eqz_abort(cat, NULL, shape, shapenum, dx, NULL, NULL, &cs, yl);
        return st;
    }
    st = cat89_functor_new(shape, cat, &d_ops, dx, actual, &diagram);
    if (st != CAT89_OK)
    {
        eqz_abort(cat, NULL, shape, shapenum, dx, NULL, NULL, &cs, yl);
        return st;
    }
    cat89_category_release(shape);
    cat89_enum_release(shapenum);
    shape = NULL;
    shapenum = NULL;
    dx = NULL;
    rx = cat89_alloc(actual, sizeof(*rx));
    if (rx == NULL)
    {
        eqz_abort(cat, NULL, NULL, NULL, NULL, diagram, NULL, &cs, yl);
        return CAT89_NOMEM;
    }
    rx->category = cat;
    rx->pt_a = pt_a;
    rx->pt_b = pt_b;
    rx->apex = apex;
    rx->enumeration = NULL;
    rx->eq = NULL;
    rx->pa = NULL;
    rx->pb = NULL;
    rx->allocator = *actual;
    st = cat89_enum_retain(enumeration);
    if (st != CAT89_OK)
    {
        eqz_abort(cat, rx, NULL, NULL, NULL, diagram, NULL, &cs, yl);
        return st;
    }
    rx->enumeration = enumeration;
    st = cat89_eq_retain(eq);
    if (st != CAT89_OK)
    {
        eqz_abort(cat, rx, NULL, NULL, NULL, diagram, NULL, &cs, yl);
        return st;
    }
    rx->eq = eq;
    st = cat89_mor_retain(cat, e);
    if (st != CAT89_OK)
    {
        eqz_abort(cat, rx, NULL, NULL, NULL, diagram, NULL, &cs, yl);
        return st;
    }
    rx->pa = e;
    st = cat89_mor_retain(cat, yl);
    if (st != CAT89_OK)
    {
        eqz_abort(cat, rx, NULL, NULL, NULL, diagram, NULL, &cs, yl);
        return st;
    }
    rx->pb = yl;
    rel1(cat, yl);
    yl = NULL;
    st = cat89_cone_new(diagram, apex, &cone_ops, rx, actual, &cone);
    if (st != CAT89_OK)
    {
        build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    cat89_functor_release(diagram);
    diagram = NULL;
    st = cat89_limit_new(cone, &equalizer_ops, rx, actual, &limit);
    if (st != CAT89_OK)
    {
        build_release(NULL, NULL, NULL, NULL, NULL, cone, &cs);
        return st;
    }
    cat89_cone_release(cone);
    cone = NULL;
    rx = NULL;
    census_free(&cs);
    *out_limit = limit;
    if (out_pt_a != NULL)
    {
        *out_pt_a = pt_a;
    }
    if (out_pt_b != NULL)
    {
        *out_pt_b = pt_b;
    }
    return CAT89_OK;
}

cat89_status cat89_find_equalizer(cat89_category *category,
                                  cat89_enum *enumeration, cat89_eq *eq,
                                  cat89_mor *f, cat89_mor *g,
                                  const cat89_allocator *allocator,
                                  cat89_limit **out_limit,
                                  const cat89_obj **out_point_a,
                                  const cat89_obj **out_point_b)
{
    cat89_status st;

    if (out_limit == NULL)
    {
        return CAT89_INVALID;
    }
    *out_limit = NULL;
    if (out_point_a != NULL)
    {
        *out_point_a = NULL;
    }
    if (out_point_b != NULL)
    {
        *out_point_b = NULL;
    }
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, f) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, g) == 0)
    {
        return CAT89_INVALID;
    }
    st = parallel_arrows_ok(category, f, g);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = build_equalizer_limit(category, enumeration, eq, f, g, allocator,
                               out_limit, out_point_a, out_point_b);
    return st;
}

/* ========================================================== coequalizer
 * The dual of the equalizer: for f, g : X -> Y a coequalizer is an apex Q with
 * a leg c : Y -> Q such that c o f == c o g and Q is universal: for every
 * object W the map  u |-> u o c : Hom(Q,W) -> { a : Y -> W | a o f == a o g }
 * is a bijection. */

static cat89_status coeq_coeq(cat89_category *cat, cat89_eq *eq, cat89_mor *f,
                              cat89_mor *g, cat89_mor *mor, int *out_eq)
{
    cat89_mor *c1;
    cat89_mor *c2;
    int e1;
    cat89_status st;

    c1 = NULL;
    c2 = NULL;
    e1 = 0;
    st = cat89_compose(cat, mor, f, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(cat, mor, g, &c2);
    if (st != CAT89_OK)
    {
        rel1(cat, c1);
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, c2, &e1);
    rel2(cat, c1, c2);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_eq = e1;
    return CAT89_OK;
}

static cat89_status coeq_arrow_step(const struct census *cs, cat89_eq *eq,
                                    cat89_mor *f, cat89_mor *g,
                                    const cat89_obj *y, const cat89_obj *w,
                                    unsigned long i, int *out_eq)
{
    cat89_mor *mor;
    int eqv;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, y) == 0)
    {
        *out_eq = 0;
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, w) == 0)
    {
        *out_eq = 0;
        return CAT89_OK;
    }
    mor = cs->mors[i].mor;
    eqv = 0;
    st = coeq_coeq(cs->category, eq, f, g, mor, &eqv);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_eq = eqv;
    return CAT89_OK;
}

static cat89_status coeq_count(const struct census *cs, cat89_eq *eq,
                               const cat89_obj *y, const cat89_obj *w,
                               cat89_mor *f, cat89_mor *g, unsigned long *out_n)
{
    unsigned long i;
    unsigned long n;
    int eqv;
    cat89_status st;

    n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = coeq_arrow_step(cs, eq, f, g, y, w, i, &eqv);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (eqv == 1)
        {
            ++n;
        }
    }
    *out_n = n;
    return CAT89_OK;
}

static cat89_status coeq_gather_one(const struct census *cs, cat89_eq *eq,
                                    cat89_mor *f, cat89_mor *g,
                                    const cat89_obj *y, const cat89_obj *w,
                                    unsigned long i, cat89_vec *vec)
{
    unsigned long idx;
    int eqv;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, y) == 0)
    {
        return CAT89_OK;
    }
    st = coeq_arrow_step(cs, eq, f, g, y, w, i, &eqv);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (eqv == 0)
    {
        return CAT89_OK;
    }
    idx = i;
    st = cat89_vec_push(vec, &cs->allocator, &idx);
    return st;
}

static cat89_status coeq_gather(const struct census *cs, cat89_eq *eq,
                                const cat89_obj *y, const cat89_obj *w,
                                cat89_mor *f, cat89_mor *g,
                                unsigned long **out_arr, unsigned long *out_n)
{
    cat89_vec vec;
    unsigned long i;
    cat89_status st;

    cat89_vec_init(&vec, sizeof(unsigned long));
    *out_arr = NULL;
    *out_n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = coeq_gather_one(cs, eq, f, g, y, w, i, &vec);
        if (st != CAT89_OK)
        {
            gather_abort(&vec, &cs->allocator);
            return st;
        }
    }
    *out_arr = (unsigned long *)vec.data;
    *out_n = vec.len;
    return CAT89_OK;
}

static cat89_status coeq_med_one(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *q, const cat89_obj *w,
                                 cat89_mor *c, cat89_mor *a1, unsigned long i,
                                 unsigned long *out_n)
{
    cat89_mor *u;
    cat89_mor *c1;
    int e1;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, q) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, w) == 0)
    {
        return CAT89_OK;
    }
    u = cs->mors[i].mor;
    c1 = NULL;
    st = cat89_compose(cs->category, u, c, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, a1, &e1);
    rel1(cs->category, c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 1)
    {
        ++*out_n;
    }
    return CAT89_OK;
}

static cat89_status coeq_med_count(const struct census *cs, cat89_eq *eq,
                                   const cat89_obj *q, const cat89_obj *w,
                                   cat89_mor *c, cat89_mor *a1,
                                   unsigned long *out_n)
{
    unsigned long i;
    unsigned long n;
    cat89_status st;

    n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = coeq_med_one(cs, eq, q, w, c, a1, i, &n);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_n = n;
    return CAT89_OK;
}

static cat89_status coeq_covered_one(const struct census *cs, cat89_eq *eq,
                                     const cat89_obj *q, const cat89_obj *w,
                                     cat89_mor *c, const unsigned long *eqi,
                                     unsigned long k, int *out_ok)
{
    unsigned long cnt;
    cat89_status st;

    cnt = 0;
    st = coeq_med_count(cs, eq, q, w, c, cs->mors[eqi[k]].mor, &cnt);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (cnt == 0)
    {
        *out_ok = 0;
    }
    return CAT89_OK;
}

static cat89_status coeq_covered(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *q, const cat89_obj *w,
                                 cat89_mor *c, const unsigned long *eqi,
                                 unsigned long neq, int *out_ok)
{
    unsigned long k;
    int ok;
    cat89_status st;

    ok = 1;
    for (k = 0; k < neq; k = k + 1)
    {
        st = coeq_covered_one(cs, eq, q, w, c, eqi, k, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status coeq_obj_ok(const struct census *cs, cat89_eq *eq,
                                const cat89_obj *w, const cat89_obj *y,
                                const cat89_obj *q, cat89_mor *c, cat89_mor *f,
                                cat89_mor *g, int *out_ok)
{
    unsigned long *eqi;
    unsigned long neq;
    unsigned long nq;
    int ok;
    cat89_status st;

    eqi = NULL;
    neq = 0;
    nq = 0;
    st = coeq_count(cs, eq, y, w, f, g, &neq);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = hom_count(cs, q, w, &nq);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (nq != neq)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    st = coeq_gather(cs, eq, y, w, f, g, &eqi, &neq);
    if (st != CAT89_OK)
    {
        return st;
    }
    ok = 1;
    st = coeq_covered(cs, eq, q, w, c, eqi, neq, &ok);
    indices_free(&cs->allocator, eqi);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status is_coequalizer(const struct census *cs, cat89_eq *eq,
                                   const cat89_obj *q, const cat89_obj *y,
                                   cat89_mor *f, cat89_mor *g, cat89_mor *c,
                                   int *out_ok)
{
    unsigned long i;
    int eqv;
    int ok;
    cat89_status st;

    eqv = 0;
    st = coeq_coeq(cs->category, eq, f, g, c, &eqv);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (eqv == 0)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    ok = 1;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = coeq_obj_ok(cs, eq, cs->objs[i], y, q, c, f, g, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static void claim_coeq_leg(const struct census *cs, unsigned long ai,
                           cat89_mor **out_leg, int *out_found)
{
    *out_leg = cs->mors[ai].mor;
    *out_found = 1;
}

static cat89_status search_coequalizer(const struct census *cs, cat89_eq *eq,
                                       const cat89_obj *y, cat89_mor *f,
                                       cat89_mor *g, const cat89_obj **out_apex,
                                       cat89_mor **out_leg, int *out_found)
{
    unsigned long *aids;
    unsigned long na;
    unsigned long i;
    unsigned long j;
    const cat89_obj *win;
    int ok;
    int pair;
    cat89_status st;

    aids = NULL;
    na = 0;
    win = NULL;
    *out_found = 0;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = gather_indices(cs, y, cs->objs[i], &aids, &na);
        if (st != CAT89_OK)
        {
            return st;
        }
        pair = 0;
        for (j = 0; j < na; j = j + 1)
        {
            st = is_coequalizer(cs, eq, cs->objs[i], y, f, g,
                                cs->mors[aids[j]].mor, &ok);
            if (st != CAT89_OK)
            {
                indices_free(&cs->allocator, aids);
                return st;
            }
            if (ok == 1)
            {
                claim_coeq_leg(cs, aids[j], out_leg, &pair);
            }
            if (pair == 1)
            {
                break;
            }
        }
        indices_free(&cs->allocator, aids);
        if (pair == 1)
        {
            set_win(&win, cs->objs[i]);
            break;
        }
    }
    if (win == NULL)
    {
        return CAT89_OK;
    }
    *out_apex = win;
    *out_found = 1;
    return CAT89_OK;
}

static cat89_status coeq_pick_one(const struct census *cs, cat89_eq *eq,
                                  const cat89_obj *q, const cat89_obj *w,
                                  cat89_mor *c, cat89_mor *a1, unsigned long i,
                                  unsigned long *out_med)
{
    cat89_mor *u;
    cat89_mor *c1;
    int e1;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, q) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, w) == 0)
    {
        return CAT89_OK;
    }
    u = cs->mors[i].mor;
    c1 = NULL;
    st = cat89_compose(cs->category, u, c, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, a1, &e1);
    rel1(cs->category, c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 1)
    {
        *out_med = i;
    }
    return CAT89_OK;
}

static cat89_status coeq_single_idx(const struct census *cs, cat89_eq *eq,
                                    const cat89_obj *q, const cat89_obj *w,
                                    cat89_mor *c, cat89_mor *a1,
                                    unsigned long *out_idx,
                                    unsigned long *out_n)
{
    unsigned long i;
    unsigned long found;
    unsigned long n;
    cat89_status st;

    n = 0;
    st = coeq_med_count(cs, eq, q, w, c, a1, &n);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_n = n;
    if (n != 1)
    {
        *out_idx = 0;
        return CAT89_OK;
    }
    found = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = coeq_pick_one(cs, eq, q, w, c, a1, i, &found);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_idx = found;
    return CAT89_OK;
}

static cat89_status coeq_factor(cat89_category *cat, cat89_enum *en,
                                cat89_eq *eq, const cat89_obj *q,
                                const cat89_obj *w, cat89_mor *c, cat89_mor *a1,
                                cat89_mor **out, const cat89_allocator *alloc)
{
    struct census cs;
    cat89_mor *keep;
    unsigned long idx;
    unsigned long n;
    cat89_status st;

    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    *out = NULL;
    keep = NULL;
    st = census_build(cat, en, alloc, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = coeq_single_idx(&cs, eq, q, w, c, a1, &idx, &n);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (n != 1)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    keep = cs.mors[idx].mor;
    st = cat89_mor_retain(cat, keep);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    census_free(&cs);
    *out = keep;
    return CAT89_OK;
}

static cat89_status coeq_col_factor(void *ctx, const cat89_cocone *candidate,
                                    cat89_mor **out_mor)
{
    struct pres *rx = ctx;
    const cat89_obj *w;
    cat89_mor *a1;
    cat89_status st;

    w = NULL;
    a1 = NULL;
    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (candidate == NULL)
    {
        return CAT89_INVALID;
    }
    w = cat89_cocone_apex(candidate);
    st = cat89_cocone_leg(candidate, rx->pt_b, &a1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = coeq_factor(rx->category, rx->enumeration, rx->eq, rx->apex, w, rx->pb,
                     a1, out_mor, &rx->allocator);
    rel1(rx->category, a1);
    return st;
}

static const cat89_colimit_ops coeq_colimit_ops = {coeq_col_factor, NULL};

static void coeq_abort(cat89_category *cat, struct pres *rx,
                       cat89_category *shape, cat89_enum *shapenum,
                       struct dctx *dx, cat89_functor *diagram,
                       cat89_cocone *cocone, struct census *cs, cat89_mor *xleg)
{
    col_build_release(rx, shape, shapenum, dx, diagram, cocone, cs);
    rel1(cat, xleg);
}

static cat89_status build_coequalizer_colimit(
    cat89_category *cat, cat89_enum *enumeration, cat89_eq *eq, cat89_mor *f,
    cat89_mor *g, const cat89_allocator *alloc, cat89_colimit **out_colimit,
    const cat89_obj **out_pt_a, const cat89_obj **out_pt_b)
{
    const cat89_allocator *actual;
    struct census cs;
    struct pres *rx;
    cat89_category *shape;
    cat89_enum *shapenum;
    struct dctx *dx;
    cat89_functor *diagram;
    cat89_cocone *cocone;
    cat89_colimit *colimit;
    const cat89_obj *y;
    const cat89_obj *apex;
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    cat89_mor *c;
    cat89_mor *xleg;
    int found;
    cat89_status st;

    actual = resolve_alloc(alloc);
    rx = NULL;
    shape = NULL;
    shapenum = NULL;
    dx = NULL;
    diagram = NULL;
    cocone = NULL;
    y = NULL;
    apex = NULL;
    pt_a = NULL;
    pt_b = NULL;
    c = NULL;
    xleg = NULL;
    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    st = census_build(cat, enumeration, actual, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(cat, f, &y);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = search_coequalizer(&cs, eq, y, f, g, &apex, &c, &found);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (!found)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    st = cat89_compose(cat, c, f, &xleg);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = cat89_shape_category_new(CAT89_SHAPE_PARALLEL, actual, &shape, NULL,
                                  &shapenum);
    if (st != CAT89_OK)
    {
        coeq_abort(cat, NULL, NULL, NULL, NULL, NULL, NULL, &cs, xleg);
        return st;
    }
    st = grab_two(shapenum, &pt_a, &pt_b);
    if (st != CAT89_OK)
    {
        coeq_abort(cat, NULL, shape, shapenum, NULL, NULL, NULL, &cs, xleg);
        return st;
    }
    dx = cat89_alloc(actual, sizeof(*dx));
    if (dx == NULL)
    {
        coeq_abort(cat, NULL, shape, shapenum, NULL, NULL, NULL, &cs, xleg);
        return CAT89_NOMEM;
    }
    dx->pt_a = pt_a;
    dx->pt_b = pt_b;
    dx->a = NULL;
    dx->allocator = *actual;
    st = cat89_dom(cat, f, &dx->a);
    if (st != CAT89_OK)
    {
        coeq_abort(cat, NULL, shape, shapenum, dx, NULL, NULL, &cs, xleg);
        return st;
    }
    st = cat89_cod(cat, f, &dx->b);
    if (st != CAT89_OK)
    {
        coeq_abort(cat, NULL, shape, shapenum, dx, NULL, NULL, &cs, xleg);
        return st;
    }
    st = cat89_functor_new(shape, cat, &d_ops, dx, actual, &diagram);
    if (st != CAT89_OK)
    {
        coeq_abort(cat, NULL, shape, shapenum, dx, NULL, NULL, &cs, xleg);
        return st;
    }
    cat89_category_release(shape);
    cat89_enum_release(shapenum);
    shape = NULL;
    shapenum = NULL;
    dx = NULL;
    rx = cat89_alloc(actual, sizeof(*rx));
    if (rx == NULL)
    {
        coeq_abort(cat, NULL, NULL, NULL, NULL, diagram, NULL, &cs, xleg);
        return CAT89_NOMEM;
    }
    rx->category = cat;
    rx->pt_a = pt_a;
    rx->pt_b = pt_b;
    rx->apex = apex;
    rx->enumeration = NULL;
    rx->eq = NULL;
    rx->pa = NULL;
    rx->pb = NULL;
    rx->allocator = *actual;
    st = cat89_enum_retain(enumeration);
    if (st != CAT89_OK)
    {
        coeq_abort(cat, rx, NULL, NULL, NULL, diagram, NULL, &cs, xleg);
        return st;
    }
    rx->enumeration = enumeration;
    st = cat89_eq_retain(eq);
    if (st != CAT89_OK)
    {
        coeq_abort(cat, rx, NULL, NULL, NULL, diagram, NULL, &cs, xleg);
        return st;
    }
    rx->eq = eq;
    st = cat89_mor_retain(cat, xleg);
    if (st != CAT89_OK)
    {
        coeq_abort(cat, rx, NULL, NULL, NULL, diagram, NULL, &cs, xleg);
        return st;
    }
    rx->pa = xleg;
    rel1(cat, xleg);
    xleg = NULL;
    st = cat89_mor_retain(cat, c);
    if (st != CAT89_OK)
    {
        col_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    rx->pb = c;
    st = cat89_cocone_new(diagram, apex, &cone_ops, rx, actual, &cocone);
    if (st != CAT89_OK)
    {
        col_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs);
        return st;
    }
    cat89_functor_release(diagram);
    diagram = NULL;
    st = cat89_colimit_new(cocone, &coeq_colimit_ops, rx, actual, &colimit);
    if (st != CAT89_OK)
    {
        col_build_release(NULL, NULL, NULL, NULL, NULL, cocone, &cs);
        return st;
    }
    cat89_cocone_release(cocone);
    cocone = NULL;
    rx = NULL;
    census_free(&cs);
    *out_colimit = colimit;
    if (out_pt_a != NULL)
    {
        *out_pt_a = pt_a;
    }
    if (out_pt_b != NULL)
    {
        *out_pt_b = pt_b;
    }
    return CAT89_OK;
}

cat89_status cat89_find_coequalizer(cat89_category *category,
                                    cat89_enum *enumeration, cat89_eq *eq,
                                    cat89_mor *f, cat89_mor *g,
                                    const cat89_allocator *allocator,
                                    cat89_colimit **out_colimit,
                                    const cat89_obj **out_point_a,
                                    const cat89_obj **out_point_b)
{
    cat89_status st;

    if (out_colimit == NULL)
    {
        return CAT89_INVALID;
    }
    *out_colimit = NULL;
    if (out_point_a != NULL)
    {
        *out_point_a = NULL;
    }
    if (out_point_b != NULL)
    {
        *out_point_b = NULL;
    }
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, f) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, g) == 0)
    {
        return CAT89_INVALID;
    }
    st = parallel_arrows_ok(category, f, g);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = build_coequalizer_colimit(category, enumeration, eq, f, g, allocator,
                                   out_colimit, out_point_a, out_point_b);
    return st;
}

/* ============================================================ pullback
 * For arrows f : X -> Z and g : Y -> Z sharing codomain Z, a pullback is an
 * apex P with legs pX : P -> X, pY : P -> Y such that f o pX == g o pY and P is
 * universal: for every object W the map u |-> (pX o u, pY o u) is a bijection
 * Hom(W,P) -> { (wX,wY) : wX in Hom(W,X), wY in Hom(W,Y), f o wX == g o wY }.
 * The returned limit is over the SPAN shape (objects -> X, Y, Z; arrows ->
 * f, g); its cone has the three legs pX, pY, and f o pX. */

struct dctx3
{
    const cat89_obj *pt[3];
    const cat89_obj *obj[3];
    cat89_allocator allocator;
};

struct p3
{
    cat89_category *category;
    cat89_enum *enumeration;
    cat89_eq *eq;
    const cat89_obj *pt[3];
    const cat89_obj *apex;
    cat89_mor *leg[3];
    cat89_allocator allocator;
};

static unsigned long find_pt3(const struct dctx3 *dx, const cat89_obj *obj)
{
    unsigned long i;

    for (i = 0; i < 3; i = i + 1)
    {
        if (obj == dx->pt[i])
        {
            return i;
        }
    }
    return 3;
}

static cat89_status dmap3_obj(void *ctx, const cat89_obj *obj,
                              const cat89_obj **out_obj)
{
    struct dctx3 *dx = ctx;
    unsigned long i;

    *out_obj = NULL;
    i = find_pt3(dx, obj);
    if (i >= 3)
    {
        return CAT89_INVALID;
    }
    *out_obj = dx->obj[i];
    return CAT89_OK;
}

static void dmap3_destroy(void *ctx)
{
    struct dctx3 *dx = ctx;

    cat89_free(&dx->allocator, dx);
}

static const cat89_functor_ops d3_ops = {dmap3_obj, NULL, dmap3_destroy};

static void p3_free(void *ctx)
{
    struct p3 *rx = ctx;
    unsigned long i;

    cat89_enum_release(rx->enumeration);
    cat89_eq_release(rx->eq);
    for (i = 0; i < 3; i = i + 1)
    {
        rel1(rx->category, rx->leg[i]);
    }
    cat89_free(&rx->allocator, rx);
}

static unsigned long find_ptp3(const struct p3 *rx, const cat89_obj *obj)
{
    unsigned long i;

    for (i = 0; i < 3; i = i + 1)
    {
        if (obj == rx->pt[i])
        {
            return i;
        }
    }
    return 3;
}

static cat89_status p3_leg(void *ctx, const cat89_obj *shape_obj,
                           cat89_mor **out_mor)
{
    struct p3 *rx = ctx;
    unsigned long i;
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    i = find_ptp3(rx, shape_obj);
    if (i >= 3)
    {
        return CAT89_INVALID;
    }
    st = cat89_mor_retain(rx->category, rx->leg[i]);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_mor = rx->leg[i];
    return CAT89_OK;
}

static const cat89_cone_ops p3_cone_ops = {p3_leg, p3_free};

static cat89_status pb_commute(cat89_category *cat, cat89_eq *eq, cat89_mor *f,
                               cat89_mor *g, cat89_mor *wX, cat89_mor *wY,
                               int *out_ok)
{
    cat89_mor *c1;
    cat89_mor *c2;
    int e1;
    cat89_status st;

    c1 = NULL;
    c2 = NULL;
    e1 = 0;
    st = cat89_compose(cat, f, wX, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(cat, g, wY, &c2);
    if (st != CAT89_OK)
    {
        rel1(cat, c1);
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, c2, &e1);
    rel2(cat, c1, c2);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_ok = e1;
    return CAT89_OK;
}

static cat89_status pb_pair_cov(const struct census *cs, cat89_eq *eq,
                                const cat89_obj *w, const cat89_obj *apex,
                                cat89_mor *pX, cat89_mor *pY, cat89_mor *wX,
                                unsigned long yidx, int *out_ok)
{
    unsigned long cnt;
    cat89_status st;

    cnt = 0;
    st = count_mediators(cs, eq, w, apex, pX, pY, wX, cs->mors[yidx].mor, &cnt);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (cnt == 0)
    {
        *out_ok = 0;
    }
    return CAT89_OK;
}

static cat89_status pb_cell(const struct census *cs, cat89_eq *eq,
                            const cat89_obj *w, const cat89_obj *apex,
                            cat89_mor *f, cat89_mor *g, cat89_mor *pX,
                            cat89_mor *pY, unsigned long xi, unsigned long yi,
                            int *out_comm, int *out_ok)
{
    int comm;
    cat89_status st;

    comm = 0;
    st = pb_commute(cs->category, eq, f, g, cs->mors[xi].mor, cs->mors[yi].mor,
                    &comm);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (comm == 1)
    {
        st = pb_pair_cov(cs, eq, w, apex, pX, pY, cs->mors[xi].mor, yi, out_ok);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_comm = comm;
    return CAT89_OK;
}

static cat89_status pb_obj_check(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *w, const cat89_obj *x,
                                 const cat89_obj *y, const cat89_obj *apex,
                                 cat89_mor *f, cat89_mor *g, cat89_mor *pX,
                                 cat89_mor *pY, int *out_ok)
{
    unsigned long *xids;
    unsigned long *yids;
    unsigned long na;
    unsigned long nb;
    unsigned long npa;
    unsigned long nc;
    unsigned long i;
    unsigned long j;
    int comm;
    int ok;
    cat89_status st;

    xids = NULL;
    yids = NULL;
    na = 0;
    nb = 0;
    npa = 0;
    nc = 0;
    st = hom_count(cs, w, apex, &npa);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = gather_indices(cs, w, x, &xids, &na);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = gather_indices(cs, w, y, &yids, &nb);
    if (st != CAT89_OK)
    {
        indices_free(&cs->allocator, xids);
        return st;
    }
    ok = 1;
    for (i = 0; i < na; i = i + 1)
    {
        for (j = 0; j < nb; j = j + 1)
        {
            st = pb_cell(cs, eq, w, apex, f, g, pX, pY, xids[i], yids[j], &comm,
                         &ok);
            if (st != CAT89_OK)
            {
                rel_arrays(&cs->allocator, xids, yids);
                return st;
            }
            if (comm == 1)
            {
                ++nc;
            }
        }
    }
    rel_arrays(&cs->allocator, xids, yids);
    if (ok == 1)
    {
        if (nc != npa)
        {
            ok = 0;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status is_pullback(const struct census *cs, cat89_eq *eq,
                                const cat89_obj *apex, const cat89_obj *x,
                                const cat89_obj *y, cat89_mor *f, cat89_mor *g,
                                cat89_mor *pX, cat89_mor *pY, int *out_ok)
{
    unsigned long i;
    int comm;
    int ok;
    cat89_status st;

    comm = 0;
    st = pb_commute(cs->category, eq, f, g, pX, pY, &comm);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (comm == 0)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    ok = 1;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = pb_obj_check(cs, eq, cs->objs[i], x, y, apex, f, g, pX, pY, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static void pb_claim(const struct census *cs, unsigned long xi,
                     unsigned long yi, cat89_mor **out_x, cat89_mor **out_y,
                     int *out_found)
{
    *out_x = cs->mors[xi].mor;
    *out_y = cs->mors[yi].mor;
    *out_found = 1;
}

static cat89_status search_pullback(const struct census *cs, cat89_eq *eq,
                                    const cat89_obj *x, const cat89_obj *y,
                                    cat89_mor *f, cat89_mor *g,
                                    const cat89_obj **out_apex,
                                    cat89_mor **out_px, cat89_mor **out_py,
                                    int *out_found)
{
    unsigned long *xids;
    unsigned long *yids;
    unsigned long na;
    unsigned long nb;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    const cat89_obj *win;
    int ok;
    int pair;
    cat89_status st;

    xids = NULL;
    yids = NULL;
    na = 0;
    nb = 0;
    win = NULL;
    *out_found = 0;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = gather_indices(cs, cs->objs[i], x, &xids, &na);
        if (st != CAT89_OK)
        {
            return st;
        }
        st = gather_indices(cs, cs->objs[i], y, &yids, &nb);
        if (st != CAT89_OK)
        {
            indices_free(&cs->allocator, xids);
            return st;
        }
        pair = 0;
        for (j = 0; j < na; j = j + 1)
        {
            for (k = 0; k < nb; k = k + 1)
            {
                st = is_pullback(cs, eq, cs->objs[i], x, y, f, g,
                                 cs->mors[xids[j]].mor, cs->mors[yids[k]].mor,
                                 &ok);
                if (st != CAT89_OK)
                {
                    rel_arrays(&cs->allocator, xids, yids);
                    return st;
                }
                if (ok == 1)
                {
                    pb_claim(cs, xids[j], yids[k], out_px, out_py, &pair);
                }
                if (pair == 1)
                {
                    break;
                }
            }
            if (pair == 1)
            {
                break;
            }
        }
        rel_arrays(&cs->allocator, xids, yids);
        if (pair == 1)
        {
            set_win(&win, cs->objs[i]);
            break;
        }
    }
    if (win == NULL)
    {
        return CAT89_OK;
    }
    *out_apex = win;
    *out_found = 1;
    return CAT89_OK;
}

static cat89_status pb_factor_med(cat89_category *cat, cat89_enum *en,
                                  cat89_eq *eq, const cat89_obj *w,
                                  const cat89_obj *apex, cat89_mor *pX,
                                  cat89_mor *pY, cat89_mor *wX, cat89_mor *wY,
                                  cat89_mor **out, const cat89_allocator *alloc)
{
    struct census cs;
    cat89_mor *keep;
    unsigned long n;
    unsigned long idx;
    cat89_status st;

    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    *out = NULL;
    keep = NULL;
    st = census_build(cat, en, alloc, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = count_mediators(&cs, eq, w, apex, pX, pY, wX, wY, &n);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (n != 1)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    st = single_mediator(&cs, eq, w, apex, pX, pY, wX, wY, &idx);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    keep = cs.mors[idx].mor;
    st = cat89_mor_retain(cat, keep);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    census_free(&cs);
    *out = keep;
    return CAT89_OK;
}

static cat89_status p3_limit_factor(void *ctx, const cat89_cone *candidate,
                                    cat89_mor **out_mor)
{
    struct p3 *rx = ctx;
    const cat89_obj *w;
    cat89_mor *wX;
    cat89_mor *wY;
    cat89_status st;

    w = NULL;
    wX = NULL;
    wY = NULL;
    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (candidate == NULL)
    {
        return CAT89_INVALID;
    }
    w = cat89_cone_apex(candidate);
    st = cat89_cone_leg(candidate, rx->pt[0], &wX);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cone_leg(candidate, rx->pt[1], &wY);
    if (st != CAT89_OK)
    {
        rel1(rx->category, wX);
        return st;
    }
    st = pb_factor_med(rx->category, rx->enumeration, rx->eq, w, rx->apex,
                       rx->leg[0], rx->leg[1], wX, wY, out_mor, &rx->allocator);
    rel2(rx->category, wX, wY);
    return st;
}

static const cat89_limit_ops p3_limit_ops = {p3_limit_factor, NULL};

static void grab3_store(const cat89_obj **ob, unsigned long *out_n,
                        const cat89_obj *obj)
{
    ob[*out_n] = obj;
    ++*out_n;
}

static cat89_status grab3_step(cat89_enum *en, cat89_obj_iter *it,
                               const cat89_obj **ob, unsigned long *out_n,
                               int *out_done)
{
    const cat89_obj *obj;
    cat89_status st;

    obj = NULL;
    st = cat89_obj_iter_next(en, it, &obj, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    if (*out_n < 3)
    {
        grab3_store(ob, out_n, obj);
    }
    return CAT89_OK;
}

static cat89_status grab_three(cat89_enum *en, const cat89_obj **o0,
                               const cat89_obj **o1, const cat89_obj **o2)
{
    cat89_obj_iter *it;
    const cat89_obj *ob[3];
    unsigned long n;
    int done;
    cat89_status st;

    it = NULL;
    n = 0;
    ob[0] = NULL;
    ob[1] = NULL;
    ob[2] = NULL;
    *o0 = NULL;
    *o1 = NULL;
    *o2 = NULL;
    st = cat89_obj_iter_open(en, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = grab3_step(en, it, ob, &n, &done);
        if (st != CAT89_OK)
        {
            cat89_obj_iter_close(en, it);
            return st;
        }
    }
    cat89_obj_iter_close(en, it);
    *o0 = ob[0];
    *o1 = ob[1];
    *o2 = ob[2];
    return CAT89_OK;
}

static void p3_build_release(struct p3 *rx, cat89_category *shape,
                             cat89_enum *shapenum, struct dctx3 *dx,
                             cat89_functor *diagram, cat89_cone *cone,
                             struct census *cs, cat89_mor *lz)
{
    if (rx != NULL)
    {
        p3_free(rx);
    }
    if (lz != NULL)
    {
        rel1(cs->category, lz);
    }
    if (diagram != NULL)
    {
        cat89_functor_release(diagram);
    }
    if (dx != NULL)
    {
        dmap3_destroy(dx);
    }
    if (shape != NULL)
    {
        cat89_category_release(shape);
    }
    if (shapenum != NULL)
    {
        cat89_enum_release(shapenum);
    }
    if (cone != NULL)
    {
        cat89_cone_release(cone);
    }
    if (cs != NULL)
    {
        census_free(cs);
    }
}

static cat89_status
build_pullback_limit(cat89_category *cat, cat89_enum *enumeration, cat89_eq *eq,
                     cat89_mor *f, cat89_mor *g, const cat89_allocator *alloc,
                     cat89_limit **out_limit, const cat89_obj **out_pt0,
                     const cat89_obj **out_pt1, const cat89_obj **out_pt2)
{
    const cat89_allocator *actual;
    struct census cs;
    struct p3 *rx;
    struct dctx3 *dx;
    cat89_category *shape;
    cat89_enum *shapenum;
    cat89_functor *diagram;
    cat89_cone *cone;
    cat89_limit *limit;
    const cat89_obj *x;
    const cat89_obj *y;
    const cat89_obj *z;
    const cat89_obj *apex;
    const cat89_obj *pt0;
    const cat89_obj *pt1;
    const cat89_obj *pt2;
    cat89_mor *pX;
    cat89_mor *pY;
    cat89_mor *lz;
    unsigned long i;
    int found;
    cat89_status st;

    actual = resolve_alloc(alloc);
    rx = NULL;
    dx = NULL;
    shape = NULL;
    shapenum = NULL;
    diagram = NULL;
    cone = NULL;
    x = NULL;
    y = NULL;
    z = NULL;
    apex = NULL;
    pt0 = NULL;
    pt1 = NULL;
    pt2 = NULL;
    pX = NULL;
    pY = NULL;
    lz = NULL;
    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    st = census_build(cat, enumeration, actual, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_dom(cat, f, &x);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = cat89_dom(cat, g, &y);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = cat89_cod(cat, f, &z);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = search_pullback(&cs, eq, x, y, f, g, &apex, &pX, &pY, &found);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (!found)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    st = cat89_compose(cat, f, pX, &lz);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = cat89_shape_category_new(CAT89_SHAPE_SPAN, actual, &shape, NULL,
                                  &shapenum);
    if (st != CAT89_OK)
    {
        p3_build_release(NULL, NULL, NULL, NULL, NULL, NULL, &cs, lz);
        return st;
    }
    st = grab_three(shapenum, &pt0, &pt1, &pt2);
    if (st != CAT89_OK)
    {
        p3_build_release(NULL, shape, shapenum, NULL, NULL, NULL, &cs, lz);
        return st;
    }
    dx = cat89_alloc(actual, sizeof(*dx));
    if (dx == NULL)
    {
        p3_build_release(NULL, shape, shapenum, NULL, NULL, NULL, &cs, lz);
        return CAT89_NOMEM;
    }
    dx->pt[0] = pt0;
    dx->pt[1] = pt1;
    dx->pt[2] = pt2;
    dx->obj[0] = x;
    dx->obj[1] = y;
    dx->obj[2] = z;
    dx->allocator = *actual;
    st = cat89_functor_new(shape, cat, &d3_ops, dx, actual, &diagram);
    if (st != CAT89_OK)
    {
        p3_build_release(NULL, shape, shapenum, dx, NULL, NULL, &cs, lz);
        return st;
    }
    cat89_category_release(shape);
    cat89_enum_release(shapenum);
    shape = NULL;
    shapenum = NULL;
    dx = NULL;
    rx = cat89_alloc(actual, sizeof(*rx));
    if (rx == NULL)
    {
        p3_build_release(NULL, NULL, NULL, NULL, diagram, NULL, &cs, lz);
        return CAT89_NOMEM;
    }
    rx->category = cat;
    rx->pt[0] = pt0;
    rx->pt[1] = pt1;
    rx->pt[2] = pt2;
    rx->apex = apex;
    rx->enumeration = NULL;
    rx->eq = NULL;
    for (i = 0; i < 3; i = i + 1)
    {
        rx->leg[i] = NULL;
    }
    rx->allocator = *actual;
    st = cat89_enum_retain(enumeration);
    if (st != CAT89_OK)
    {
        p3_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, lz);
        return st;
    }
    rx->enumeration = enumeration;
    st = cat89_eq_retain(eq);
    if (st != CAT89_OK)
    {
        p3_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, lz);
        return st;
    }
    rx->eq = eq;
    st = cat89_mor_retain(cat, pX);
    if (st != CAT89_OK)
    {
        p3_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, lz);
        return st;
    }
    rx->leg[0] = pX;
    st = cat89_mor_retain(cat, pY);
    if (st != CAT89_OK)
    {
        p3_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, lz);
        return st;
    }
    rx->leg[1] = pY;
    st = cat89_mor_retain(cat, lz);
    if (st != CAT89_OK)
    {
        p3_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, lz);
        return st;
    }
    rx->leg[2] = lz;
    rel1(cat, lz);
    lz = NULL;
    st = cat89_cone_new(diagram, apex, &p3_cone_ops, rx, actual, &cone);
    if (st != CAT89_OK)
    {
        p3_build_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, lz);
        return st;
    }
    cat89_functor_release(diagram);
    diagram = NULL;
    st = cat89_limit_new(cone, &p3_limit_ops, rx, actual, &limit);
    if (st != CAT89_OK)
    {
        p3_build_release(NULL, NULL, NULL, NULL, NULL, cone, &cs, lz);
        return st;
    }
    cat89_cone_release(cone);
    cone = NULL;
    rx = NULL;
    census_free(&cs);
    *out_limit = limit;
    if (out_pt0 != NULL)
    {
        *out_pt0 = pt0;
    }
    if (out_pt1 != NULL)
    {
        *out_pt1 = pt1;
    }
    if (out_pt2 != NULL)
    {
        *out_pt2 = pt2;
    }
    return CAT89_OK;
}

cat89_status
cat89_find_pullback(cat89_category *category, cat89_enum *enumeration,
                    cat89_eq *eq, cat89_mor *f, cat89_mor *g,
                    const cat89_allocator *allocator, cat89_limit **out_limit,
                    const cat89_obj **out_pt0, const cat89_obj **out_pt1,
                    const cat89_obj **out_pt2)
{
    cat89_status st;

    if (out_limit == NULL)
    {
        return CAT89_INVALID;
    }
    *out_limit = NULL;
    if (out_pt0 != NULL)
    {
        *out_pt0 = NULL;
    }
    if (out_pt1 != NULL)
    {
        *out_pt1 = NULL;
    }
    if (out_pt2 != NULL)
    {
        *out_pt2 = NULL;
    }
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, f) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, g) == 0)
    {
        return CAT89_INVALID;
    }
    st = shared_codomain_ok(category, f, g);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = build_pullback_limit(category, enumeration, eq, f, g, allocator,
                              out_limit, out_pt0, out_pt1, out_pt2);
    return st;
}

/* ============================================================ pushout
 * Dual of the pullback: for f : X -> Y and g : X -> Z a pushout is an apex Q
 * with legs iY : Y -> Q, iZ : Z -> Q such that iY o f == iZ o g and Q is
 * universal: for every object W the map u |-> (u o iY, u o iZ) is a bijection
 * Hom(Q,W) -> { (aY,aZ) : aY in Hom(Y,W), aZ in Hom(Z,W), aY o f == aZ o g }.
 * The returned colimit is over the COSPAN shape (objects -> X, Y, Z; arrows ->
 * f, g). */

static cat89_status po_commute(cat89_category *cat, cat89_eq *eq, cat89_mor *f,
                               cat89_mor *g, cat89_mor *aY, cat89_mor *aZ,
                               int *out_ok)
{
    cat89_mor *c1;
    cat89_mor *c2;
    int e1;
    cat89_status st;

    c1 = NULL;
    c2 = NULL;
    e1 = 0;
    st = cat89_compose(cat, aY, f, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(cat, aZ, g, &c2);
    if (st != CAT89_OK)
    {
        rel1(cat, c1);
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, c2, &e1);
    rel2(cat, c1, c2);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_ok = e1;
    return CAT89_OK;
}

/* Mediator u : q -> w with u o iY == aY and u o iZ == aZ (two equations). */
static cat89_status po_med_one(const struct census *cs, cat89_eq *eq,
                               const cat89_obj *q, const cat89_obj *w,
                               cat89_mor *iY, cat89_mor *iZ, cat89_mor *aY,
                               cat89_mor *aZ, unsigned long i,
                               unsigned long *out_n)
{
    cat89_mor *u;
    cat89_mor *c1;
    cat89_mor *c2;
    int e1;
    int e2;
    int med;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, q) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, w) == 0)
    {
        return CAT89_OK;
    }
    u = cs->mors[i].mor;
    c1 = NULL;
    c2 = NULL;
    med = 0;
    st = cat89_compose(cs->category, u, iY, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(cs->category, u, iZ, &c2);
    if (st != CAT89_OK)
    {
        rel1(cs->category, c1);
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, aY, &e1);
    if (st != CAT89_OK)
    {
        rel2(cs->category, c1, c2);
        return st;
    }
    e2 = 0;
    st = cat89_mor_equal(eq, c2, aZ, &e2);
    rel2(cs->category, c1, c2);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 1)
    {
        if (e2 == 1)
        {
            med = 1;
        }
    }
    if (med == 1)
    {
        ++*out_n;
    }
    return CAT89_OK;
}

static cat89_status po_med_count(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *q, const cat89_obj *w,
                                 cat89_mor *iY, cat89_mor *iZ, cat89_mor *aY,
                                 cat89_mor *aZ, unsigned long *out_n)
{
    unsigned long i;
    unsigned long n;
    cat89_status st;

    n = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = po_med_one(cs, eq, q, w, iY, iZ, aY, aZ, i, &n);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_n = n;
    return CAT89_OK;
}

static cat89_status po_pair_cov(const struct census *cs, cat89_eq *eq,
                                const cat89_obj *q, const cat89_obj *w,
                                cat89_mor *iY, cat89_mor *iZ,
                                unsigned long yidx, cat89_mor *aZ, int *out_ok)
{
    unsigned long cnt;
    cat89_status st;

    cnt = 0;
    st = po_med_count(cs, eq, q, w, iY, iZ, cs->mors[yidx].mor, aZ, &cnt);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (cnt == 0)
    {
        *out_ok = 0;
    }
    return CAT89_OK;
}

static cat89_status po_cell(const struct census *cs, cat89_eq *eq,
                            const cat89_obj *q, const cat89_obj *w,
                            cat89_mor *f, cat89_mor *g, cat89_mor *iY,
                            cat89_mor *iZ, unsigned long yi, unsigned long zi,
                            int *out_comm, int *out_ok)
{
    int comm;
    cat89_status st;

    comm = 0;
    st = po_commute(cs->category, eq, f, g, cs->mors[yi].mor, cs->mors[zi].mor,
                    &comm);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (comm == 1)
    {
        st = po_pair_cov(cs, eq, q, w, iY, iZ, yi, cs->mors[zi].mor, out_ok);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_comm = comm;
    return CAT89_OK;
}

static cat89_status po_obj_check(const struct census *cs, cat89_eq *eq,
                                 const cat89_obj *w, const cat89_obj *y,
                                 const cat89_obj *z, const cat89_obj *apex,
                                 cat89_mor *f, cat89_mor *g, cat89_mor *iY,
                                 cat89_mor *iZ, int *out_ok)
{
    unsigned long *yids;
    unsigned long *zids;
    unsigned long ny;
    unsigned long nz;
    unsigned long nqa;
    unsigned long ncom;
    unsigned long i;
    unsigned long j;
    int comm;
    int ok;
    cat89_status st;

    yids = NULL;
    zids = NULL;
    ny = 0;
    nz = 0;
    nqa = 0;
    ncom = 0;
    st = hom_count(cs, apex, w, &nqa);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = gather_indices(cs, y, w, &yids, &ny);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = gather_indices(cs, z, w, &zids, &nz);
    if (st != CAT89_OK)
    {
        indices_free(&cs->allocator, yids);
        return st;
    }
    ok = 1;
    for (i = 0; i < ny; i = i + 1)
    {
        for (j = 0; j < nz; j = j + 1)
        {
            st = po_cell(cs, eq, apex, w, f, g, iY, iZ, yids[i], zids[j], &comm,
                         &ok);
            if (st != CAT89_OK)
            {
                rel_arrays(&cs->allocator, yids, zids);
                return st;
            }
            if (comm == 1)
            {
                ++ncom;
            }
        }
    }
    rel_arrays(&cs->allocator, yids, zids);
    if (ok == 1)
    {
        if (ncom != nqa)
        {
            ok = 0;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status is_pushout(const struct census *cs, cat89_eq *eq,
                               const cat89_obj *apex, const cat89_obj *y,
                               const cat89_obj *z, cat89_mor *f, cat89_mor *g,
                               cat89_mor *iY, cat89_mor *iZ, int *out_ok)
{
    unsigned long i;
    int comm;
    int ok;
    cat89_status st;

    comm = 0;
    st = po_commute(cs->category, eq, f, g, iY, iZ, &comm);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (comm == 0)
    {
        *out_ok = 0;
        return CAT89_OK;
    }
    ok = 1;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = po_obj_check(cs, eq, cs->objs[i], y, z, apex, f, g, iY, iZ, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok == 0)
        {
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static void po_claim(const struct census *cs, unsigned long yi,
                     unsigned long zi, cat89_mor **out_y, cat89_mor **out_z,
                     int *out_found)
{
    *out_y = cs->mors[yi].mor;
    *out_z = cs->mors[zi].mor;
    *out_found = 1;
}

static cat89_status search_pushout(const struct census *cs, cat89_eq *eq,
                                   const cat89_obj *y, const cat89_obj *z,
                                   cat89_mor *f, cat89_mor *g,
                                   const cat89_obj **out_apex,
                                   cat89_mor **out_iy, cat89_mor **out_iz,
                                   int *out_found)
{
    unsigned long *yids;
    unsigned long *zids;
    unsigned long na;
    unsigned long nb;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    const cat89_obj *win;
    int ok;
    int pair;
    cat89_status st;

    yids = NULL;
    zids = NULL;
    na = 0;
    nb = 0;
    win = NULL;
    *out_found = 0;
    for (i = 0; i < cs->n_obj; i = i + 1)
    {
        st = gather_indices(cs, y, cs->objs[i], &yids, &na);
        if (st != CAT89_OK)
        {
            return st;
        }
        st = gather_indices(cs, z, cs->objs[i], &zids, &nb);
        if (st != CAT89_OK)
        {
            indices_free(&cs->allocator, yids);
            return st;
        }
        pair = 0;
        for (j = 0; j < na; j = j + 1)
        {
            for (k = 0; k < nb; k = k + 1)
            {
                st = is_pushout(cs, eq, cs->objs[i], y, z, f, g,
                                cs->mors[yids[j]].mor, cs->mors[zids[k]].mor,
                                &ok);
                if (st != CAT89_OK)
                {
                    rel_arrays(&cs->allocator, yids, zids);
                    return st;
                }
                if (ok == 1)
                {
                    po_claim(cs, yids[j], zids[k], out_iy, out_iz, &pair);
                }
                if (pair == 1)
                {
                    break;
                }
            }
            if (pair == 1)
            {
                break;
            }
        }
        rel_arrays(&cs->allocator, yids, zids);
        if (pair == 1)
        {
            set_win(&win, cs->objs[i]);
            break;
        }
    }
    if (win == NULL)
    {
        return CAT89_OK;
    }
    *out_apex = win;
    *out_found = 1;
    return CAT89_OK;
}

static cat89_status po_pick_one(const struct census *cs, cat89_eq *eq,
                                const cat89_obj *q, const cat89_obj *w,
                                cat89_mor *iY, cat89_mor *iZ, cat89_mor *aY,
                                cat89_mor *aZ, unsigned long i,
                                unsigned long *out_med)
{
    cat89_mor *u;
    cat89_mor *c1;
    cat89_mor *c2;
    int e1;
    int e2;
    int med;
    cat89_status st;

    if (cat89_obj_same(cs->category, cs->mors[i].dom, q) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cs->category, cs->mors[i].cod, w) == 0)
    {
        return CAT89_OK;
    }
    u = cs->mors[i].mor;
    c1 = NULL;
    c2 = NULL;
    med = 0;
    st = cat89_compose(cs->category, u, iY, &c1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(cs->category, u, iZ, &c2);
    if (st != CAT89_OK)
    {
        rel1(cs->category, c1);
        return st;
    }
    e1 = 0;
    st = cat89_mor_equal(eq, c1, aY, &e1);
    if (st != CAT89_OK)
    {
        rel2(cs->category, c1, c2);
        return st;
    }
    e2 = 0;
    st = cat89_mor_equal(eq, c2, aZ, &e2);
    rel2(cs->category, c1, c2);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 1)
    {
        if (e2 == 1)
        {
            med = 1;
        }
    }
    if (med == 1)
    {
        *out_med = i;
    }
    return CAT89_OK;
}

static cat89_status po_single_idx(const struct census *cs, cat89_eq *eq,
                                  const cat89_obj *q, const cat89_obj *w,
                                  cat89_mor *iY, cat89_mor *iZ, cat89_mor *aY,
                                  cat89_mor *aZ, unsigned long *out_idx,
                                  unsigned long *out_n)
{
    unsigned long i;
    unsigned long n;
    unsigned long med;
    cat89_status st;

    n = 0;
    st = po_med_count(cs, eq, q, w, iY, iZ, aY, aZ, &n);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_n = n;
    if (n != 1)
    {
        *out_idx = 0;
        return CAT89_OK;
    }
    med = 0;
    for (i = 0; i < cs->n_mor; i = i + 1)
    {
        st = po_pick_one(cs, eq, q, w, iY, iZ, aY, aZ, i, &med);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    *out_idx = med;
    return CAT89_OK;
}

static cat89_status po_factor(cat89_category *cat, cat89_enum *en, cat89_eq *eq,
                              const cat89_obj *q, const cat89_obj *w,
                              cat89_mor *iY, cat89_mor *iZ, cat89_mor *aY,
                              cat89_mor *aZ, cat89_mor **out,
                              const cat89_allocator *alloc)
{
    struct census cs;
    cat89_mor *keep;
    unsigned long n;
    unsigned long idx;
    cat89_status st;

    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    *out = NULL;
    keep = NULL;
    st = census_build(cat, en, alloc, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = po_med_count(&cs, eq, q, w, iY, iZ, aY, aZ, &n);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (n != 1)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    st = po_single_idx(&cs, eq, q, w, iY, iZ, aY, aZ, &idx, &n);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    keep = cs.mors[idx].mor;
    st = cat89_mor_retain(cat, keep);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    census_free(&cs);
    *out = keep;
    return CAT89_OK;
}

static cat89_status po_col_factor(void *ctx, const cat89_cocone *candidate,
                                  cat89_mor **out_mor)
{
    struct p3 *rx = ctx;
    const cat89_obj *w;
    cat89_mor *aY;
    cat89_mor *aZ;
    cat89_status st;

    w = NULL;
    aY = NULL;
    aZ = NULL;
    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (candidate == NULL)
    {
        return CAT89_INVALID;
    }
    w = cat89_cocone_apex(candidate);
    st = cat89_cocone_leg(candidate, rx->pt[1], &aY);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cocone_leg(candidate, rx->pt[2], &aZ);
    if (st != CAT89_OK)
    {
        rel1(rx->category, aY);
        return st;
    }
    st = po_factor(rx->category, rx->enumeration, rx->eq, rx->apex, w,
                   rx->leg[1], rx->leg[2], aY, aZ, out_mor, &rx->allocator);
    rel2(rx->category, aY, aZ);
    return st;
}

static const cat89_colimit_ops po_colimit_ops = {po_col_factor, NULL};

static void p3_col_release(struct p3 *rx, cat89_category *shape,
                           cat89_enum *shapenum, struct dctx3 *dx,
                           cat89_functor *diagram, cat89_cocone *cocone,
                           struct census *cs, cat89_mor *l0)
{
    if (rx != NULL)
    {
        p3_free(rx);
    }
    if (l0 != NULL)
    {
        rel1(cs->category, l0);
    }
    if (diagram != NULL)
    {
        cat89_functor_release(diagram);
    }
    if (dx != NULL)
    {
        dmap3_destroy(dx);
    }
    if (shape != NULL)
    {
        cat89_category_release(shape);
    }
    if (shapenum != NULL)
    {
        cat89_enum_release(shapenum);
    }
    if (cocone != NULL)
    {
        cat89_cocone_release(cocone);
    }
    if (cs != NULL)
    {
        census_free(cs);
    }
}

cat89_status
cat89_find_pushout(cat89_category *category, cat89_enum *enumeration,
                   cat89_eq *eq, cat89_mor *f, cat89_mor *g,
                   const cat89_allocator *allocator,
                   cat89_colimit **out_colimit, const cat89_obj **out_pt0,
                   const cat89_obj **out_pt1, const cat89_obj **out_pt2)
{
    const cat89_allocator *actual;
    struct census cs;
    struct p3 *rx;
    struct dctx3 *dx;
    cat89_category *shape;
    cat89_enum *shapenum;
    cat89_functor *diagram;
    cat89_cocone *cocone;
    cat89_colimit *colimit;
    const cat89_obj *y;
    const cat89_obj *z;
    const cat89_obj *x;
    const cat89_obj *apex;
    const cat89_obj *pt0;
    const cat89_obj *pt1;
    const cat89_obj *pt2;
    cat89_mor *iY;
    cat89_mor *iZ;
    cat89_mor *l0;
    unsigned long i;
    int found;
    cat89_status st;

    actual = resolve_alloc(allocator);
    if (out_colimit == NULL)
    {
        return CAT89_INVALID;
    }
    *out_colimit = NULL;
    if (out_pt0 != NULL)
    {
        *out_pt0 = NULL;
    }
    if (out_pt1 != NULL)
    {
        *out_pt1 = NULL;
    }
    if (out_pt2 != NULL)
    {
        *out_pt2 = NULL;
    }
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, f) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, g) == 0)
    {
        return CAT89_INVALID;
    }
    st = shared_domain_ok(category, f, g);
    if (st != CAT89_OK)
    {
        return st;
    }
    rx = NULL;
    dx = NULL;
    shape = NULL;
    shapenum = NULL;
    diagram = NULL;
    cocone = NULL;
    x = NULL;
    y = NULL;
    z = NULL;
    apex = NULL;
    pt0 = NULL;
    pt1 = NULL;
    pt2 = NULL;
    iY = NULL;
    iZ = NULL;
    l0 = NULL;
    cs.objs = NULL;
    cs.mors = NULL;
    cs.n_obj = 0;
    cs.n_mor = 0;
    st = census_build(category, enumeration, actual, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(category, f, &y);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = cat89_cod(category, g, &z);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = cat89_dom(category, f, &x);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = search_pushout(&cs, eq, y, z, f, g, &apex, &iY, &iZ, &found);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    if (!found)
    {
        census_free(&cs);
        return CAT89_NOT_FOUND;
    }
    st = cat89_compose(category, iY, f, &l0);
    if (st != CAT89_OK)
    {
        census_free(&cs);
        return st;
    }
    st = cat89_shape_category_new(CAT89_SHAPE_COSPAN, actual, &shape, NULL,
                                  &shapenum);
    if (st != CAT89_OK)
    {
        p3_col_release(NULL, NULL, NULL, NULL, NULL, NULL, &cs, l0);
        return st;
    }
    st = grab_three(shapenum, &pt0, &pt1, &pt2);
    if (st != CAT89_OK)
    {
        p3_col_release(NULL, shape, shapenum, NULL, NULL, NULL, &cs, l0);
        return st;
    }
    dx = cat89_alloc(actual, sizeof(*dx));
    if (dx == NULL)
    {
        p3_col_release(NULL, shape, shapenum, NULL, NULL, NULL, &cs, l0);
        return CAT89_NOMEM;
    }
    dx->pt[0] = pt0;
    dx->pt[1] = pt1;
    dx->pt[2] = pt2;
    dx->obj[0] = x;
    dx->obj[1] = y;
    dx->obj[2] = z;
    dx->allocator = *actual;
    st = cat89_functor_new(shape, category, &d3_ops, dx, actual, &diagram);
    if (st != CAT89_OK)
    {
        p3_col_release(NULL, shape, shapenum, dx, NULL, NULL, &cs, l0);
        return st;
    }
    cat89_category_release(shape);
    cat89_enum_release(shapenum);
    shape = NULL;
    shapenum = NULL;
    dx = NULL;
    rx = cat89_alloc(actual, sizeof(*rx));
    if (rx == NULL)
    {
        p3_col_release(NULL, NULL, NULL, NULL, diagram, NULL, &cs, l0);
        return CAT89_NOMEM;
    }
    rx->category = category;
    rx->pt[0] = pt0;
    rx->pt[1] = pt1;
    rx->pt[2] = pt2;
    rx->apex = apex;
    rx->enumeration = NULL;
    rx->eq = NULL;
    for (i = 0; i < 3; i = i + 1)
    {
        rx->leg[i] = NULL;
    }
    rx->allocator = *actual;
    st = cat89_enum_retain(enumeration);
    if (st != CAT89_OK)
    {
        p3_col_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, l0);
        return st;
    }
    rx->enumeration = enumeration;
    st = cat89_eq_retain(eq);
    if (st != CAT89_OK)
    {
        p3_col_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, l0);
        return st;
    }
    rx->eq = eq;
    st = cat89_mor_retain(category, l0);
    if (st != CAT89_OK)
    {
        p3_col_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, l0);
        return st;
    }
    rx->leg[0] = l0;
    rel1(category, l0);
    l0 = NULL;
    st = cat89_mor_retain(category, iY);
    if (st != CAT89_OK)
    {
        p3_col_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, l0);
        return st;
    }
    rx->leg[1] = iY;
    st = cat89_mor_retain(category, iZ);
    if (st != CAT89_OK)
    {
        p3_col_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, l0);
        return st;
    }
    rx->leg[2] = iZ;
    st = cat89_cocone_new(diagram, apex, &p3_cone_ops, rx, actual, &cocone);
    if (st != CAT89_OK)
    {
        p3_col_release(rx, NULL, NULL, NULL, diagram, NULL, &cs, l0);
        return st;
    }
    cat89_functor_release(diagram);
    diagram = NULL;
    st = cat89_colimit_new(cocone, &po_colimit_ops, rx, actual, &colimit);
    if (st != CAT89_OK)
    {
        p3_col_release(NULL, NULL, NULL, NULL, NULL, cocone, &cs, l0);
        return st;
    }
    cat89_cocone_release(cocone);
    cocone = NULL;
    rx = NULL;
    census_free(&cs);
    *out_colimit = colimit;
    if (out_pt0 != NULL)
    {
        *out_pt0 = pt0;
    }
    if (out_pt1 != NULL)
    {
        *out_pt1 = pt1;
    }
    if (out_pt2 != NULL)
    {
        *out_pt2 = pt2;
    }
    return CAT89_OK;
}
