/* cat89_test_allocfail_ctors.c - allocator-failure campaign over the generic
 * wrapper-category and derived-structure constructors (W6 continuation).
 *
 * Extends test_allocfail.c (which covered finite_builder + shape) to the
 * opposite/product/functor/nat/iso/split/cone/cocone/limit/colimit/arrow/
 * slice/coslice/comma constructors. For each constructor we build its valid
 * inputs (base categories, functors, equalities) with the default allocator,
 * then sweep a fault-injecting allocator across every allocation the target
 * constructor can make, asserting error-atomicity: on a single-allocation
 * failure the produced handle is NULL (no partial object escapes) and the call
 * returns CAT89_NOMEM; past the last allocation it succeeds and releases
 * cleanly. Running under ASan+LSan, any partial-failure leak fails the run. */
#include <stdlib.h>

#include "finite_categories.h"
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

#define SWEEP_MAX 300UL

struct faila
{
    unsigned long count;
    unsigned long fail_at;
};

static void *fa_alloc(void *ctx, size_t size)
{
    struct faila *f = ctx;
    f->count = f->count + 1;
    if (f->count == f->fail_at)
    {
        return NULL;
    }
    return malloc(size);
}

static void *fa_realloc(void *ctx, void *ptr, size_t size)
{
    struct faila *f = ctx;
    f->count = f->count + 1;
    if (f->count == f->fail_at)
    {
        return NULL;
    }
    return realloc(ptr, size);
}

static void fa_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

typedef cat89_status (*build_fn)(const cat89_allocator *alloc);

static void drive(build_fn fn)
{
    unsigned long at;
    unsigned long ok_count;
    unsigned long fail_count;
    struct faila f;
    cat89_allocator alloc;
    cat89_status st;

    ok_count = 0;
    fail_count = 0;
    for (at = 1; at <= SWEEP_MAX; at = at + 1)
    {
        f.count = 0;
        f.fail_at = at;
        alloc.ctx = &f;
        alloc.alloc = fa_alloc;
        alloc.realloc = fa_realloc;
        alloc.free = fa_free;
        st = fn(&alloc);
        if (st == CAT89_OK)
        {
            ok_count = ok_count + 1;
        }
        else
        {
            T_ASSERT(st == CAT89_NOMEM);
            fail_count = fail_count + 1;
        }
    }
    T_ASSERT(fail_count > 0);
    T_ASSERT(ok_count > 0);
}

/* A fixture held open together with (optionally) its owned morphism handles. */
struct amb
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_mor **mors;
    unsigned long n;
};

static unsigned long collect_mors(cat89_category *cat, cat89_enum *en,
                                  cat89_mor ***out)
{
    cat89_mor_iter *it;
    cat89_mor *mor;
    cat89_mor **arr;
    int done;
    unsigned long n;

    it = NULL;
    mor = NULL;
    if (cat89_mor_iter_open(en, &it) != CAT89_OK)
    {
        return 0;
    }
    done = 0;
    n = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(en, it, &mor, &done);
        if (!done)
        {
            cat89_mor_release(cat, mor);
            n = n + 1;
        }
    }
    cat89_mor_iter_close(en, it);

    arr = (cat89_mor **)malloc(n * sizeof(cat89_mor *));
    if (arr == NULL)
    {
        return 0;
    }
    it = NULL;
    if (cat89_mor_iter_open(en, &it) != CAT89_OK)
    {
        free(arr);
        return 0;
    }
    done = 0;
    n = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(en, it, &mor, &done);
        if (!done)
        {
            arr[n] = mor;
            n = n + 1;
        }
    }
    cat89_mor_iter_close(en, it);
    *out = arr;
    return n;
}

static cat89_status amb_open(enum cat89_fixture_kind kind, struct amb *a)
{
    cat89_status st;

    a->cat = NULL;
    a->eq = NULL;
    a->en = NULL;
    a->mors = NULL;
    a->n = 0;
    st = cat89_fixture_build(kind, &a->cat, &a->eq, &a->en);
    if (st != CAT89_OK)
    {
        return st;
    }
    a->n = collect_mors(a->cat, a->en, &a->mors);
    if (a->n == 0)
    {
        cat89_enum_release(a->en);
        cat89_eq_release(a->eq);
        cat89_category_release(a->cat);
        a->cat = NULL;
        return CAT89_NOMEM;
    }
    return CAT89_OK;
}

static void amb_close(struct amb *a)
{
    unsigned long i;

    for (i = 0; i < a->n; i = i + 1)
    {
        cat89_mor_release(a->cat, a->mors[i]);
    }
    if (a->mors != NULL)
    {
        free(a->mors);
    }
    if (a->cat != NULL)
    {
        cat89_enum_release(a->en);
        cat89_eq_release(a->eq);
        cat89_category_release(a->cat);
    }
    a->cat = NULL;
}

/* ---- opposite ---------------------------------------------------------- */

static cat89_status try_opposite(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_category *op;
    cat89_status st;

    op = NULL;
    st = amb_open(CAT89_FIX_COMP2, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_opposite_new(a.cat, alloc, &op);
    if (st == CAT89_OK)
    {
        T_ASSERT(op != NULL);
        cat89_category_release(op);
    }
    else
    {
        T_ASSERT(op == NULL);
    }
    amb_close(&a);
    return st;
}

/* ---- product ----------------------------------------------------------- */

static cat89_status try_product(const cat89_allocator *alloc)
{
    struct amb l;
    struct amb r;
    cat89_category *prod;
    cat89_status st;

    prod = NULL;
    st = amb_open(CAT89_FIX_COMP2, &l);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = amb_open(CAT89_FIX_ARROW, &r);
    if (st != CAT89_OK)
    {
        amb_close(&l);
        return st;
    }
    st = cat89_product_category_new(l.cat, r.cat, alloc, &prod);
    if (st == CAT89_OK)
    {
        T_ASSERT(prod != NULL);
        cat89_category_release(prod);
    }
    else
    {
        T_ASSERT(prod == NULL);
    }
    amb_close(&r);
    amb_close(&l);
    return st;
}

/* ---- arrow ------------------------------------------------------------- */

static cat89_status try_arrow(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_category *arrow;
    cat89_status st;

    arrow = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_arrow_category_new(a.cat, a.eq, alloc, &arrow);
    if (st == CAT89_OK)
    {
        T_ASSERT(arrow != NULL);
        cat89_category_release(arrow);
    }
    else
    {
        T_ASSERT(arrow == NULL);
    }
    amb_close(&a);
    return st;
}

/* ---- slice / coslice --------------------------------------------------- */

static const cat89_obj *amb_first_obj(const struct amb *a)
{
    const cat89_obj *obj;

    obj = NULL;
    if (cat89_dom(a->cat, a->mors[0], &obj) != CAT89_OK)
    {
        return NULL;
    }
    return obj;
}

static cat89_status try_slice(const cat89_allocator *alloc)
{
    struct amb a;
    const cat89_obj *apex;
    cat89_category *slice;
    cat89_status st;

    slice = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    apex = amb_first_obj(&a);
    T_ASSERT(apex != NULL);
    if (apex != NULL)
    {
        st = cat89_slice_category_new(a.cat, a.eq, apex, alloc, &slice);
        if (st == CAT89_OK)
        {
            T_ASSERT(slice != NULL);
            cat89_category_release(slice);
        }
        else
        {
            T_ASSERT(slice == NULL);
        }
    }
    amb_close(&a);
    return st;
}

static cat89_status try_coslice(const cat89_allocator *alloc)
{
    struct amb a;
    const cat89_obj *src;
    cat89_category *coslice;
    cat89_status st;

    coslice = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    src = amb_first_obj(&a);
    T_ASSERT(src != NULL);
    if (src != NULL)
    {
        st = cat89_coslice_category_new(a.cat, a.eq, src, alloc, &coslice);
        if (st == CAT89_OK)
        {
            T_ASSERT(coslice != NULL);
            cat89_category_release(coslice);
        }
        else
        {
            T_ASSERT(coslice == NULL);
        }
    }
    amb_close(&a);
    return st;
}

/* ---- comma ------------------------------------------------------------- */

static cat89_status try_comma(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_functor *fl;
    cat89_functor *fr;
    cat89_category *comma;
    cat89_status st;

    fl = NULL;
    fr = NULL;
    comma = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(a.cat, NULL, &fl);
    if (st == CAT89_OK)
    {
        st = cat89_functor_identity(a.cat, NULL, &fr);
    }
    if (st == CAT89_OK)
    {
        st = cat89_comma_category_new(fl, fr, a.eq, alloc, &comma);
        if (st == CAT89_OK)
        {
            T_ASSERT(comma != NULL);
            cat89_category_release(comma);
        }
        else
        {
            T_ASSERT(comma == NULL);
        }
    }
    if (fr != NULL)
    {
        cat89_functor_release(fr);
    }
    if (fl != NULL)
    {
        cat89_functor_release(fl);
    }
    amb_close(&a);
    return st;
}

/* ---- functor ----------------------------------------------------------- */

static const cat89_functor_ops fops_zero = {NULL, NULL, NULL};

static cat89_status try_functor_new(const cat89_allocator *alloc)
{
    struct amb s;
    struct amb t;
    cat89_functor *f;
    cat89_status st;

    f = NULL;
    st = amb_open(CAT89_FIX_ARROW, &s);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = amb_open(CAT89_FIX_COMP2, &t);
    if (st != CAT89_OK)
    {
        amb_close(&s);
        return st;
    }
    st = cat89_functor_new(s.cat, t.cat, &fops_zero, NULL, alloc, &f);
    if (st == CAT89_OK)
    {
        T_ASSERT(f != NULL);
        cat89_functor_release(f);
    }
    else
    {
        T_ASSERT(f == NULL);
    }
    amb_close(&t);
    amb_close(&s);
    return st;
}

static cat89_status try_functor_identity(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_functor *f;
    cat89_status st;

    f = NULL;
    st = amb_open(CAT89_FIX_ARROW, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(a.cat, alloc, &f);
    if (st == CAT89_OK)
    {
        T_ASSERT(f != NULL);
        cat89_functor_release(f);
    }
    else
    {
        T_ASSERT(f == NULL);
    }
    amb_close(&a);
    return st;
}

/* ---- nat --------------------------------------------------------------- */

static cat89_status nat_comp_id(void *ctx, const cat89_obj *obj,
                                cat89_mor **out_mor)
{
    return cat89_identity((cat89_category *)ctx, obj, out_mor);
}

static const cat89_nat_ops nat_ops = {nat_comp_id, NULL};

static cat89_status try_nat_new(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_functor *fs;
    cat89_functor *ft;
    cat89_nat *nat;
    cat89_status st;

    fs = NULL;
    ft = NULL;
    nat = NULL;
    st = amb_open(CAT89_FIX_ARROW, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(a.cat, NULL, &fs);
    if (st == CAT89_OK)
    {
        st = cat89_functor_identity(a.cat, NULL, &ft);
    }
    if (st == CAT89_OK)
    {
        st = cat89_nat_new(fs, ft, &nat_ops, a.cat, alloc, &nat);
        if (st == CAT89_OK)
        {
            T_ASSERT(nat != NULL);
            cat89_nat_release(nat);
        }
        else
        {
            T_ASSERT(nat == NULL);
        }
    }
    if (ft != NULL)
    {
        cat89_functor_release(ft);
    }
    if (fs != NULL)
    {
        cat89_functor_release(fs);
    }
    amb_close(&a);
    return st;
}

static cat89_status try_nat_identity(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_functor *f;
    cat89_nat *nat;
    cat89_status st;

    f = NULL;
    nat = NULL;
    st = amb_open(CAT89_FIX_ARROW, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(a.cat, NULL, &f);
    if (st == CAT89_OK)
    {
        st = cat89_nat_identity(f, alloc, &nat);
        if (st == CAT89_OK)
        {
            T_ASSERT(nat != NULL);
            cat89_nat_release(nat);
        }
        else
        {
            T_ASSERT(nat == NULL);
        }
    }
    if (f != NULL)
    {
        cat89_functor_release(f);
    }
    amb_close(&a);
    return st;
}

/* ---- iso / split (C2 groupoid: morphs[0]=identity e, morphs[1]=s, s o s = e)
 */

static cat89_status try_iso_new(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_iso *iso;
    cat89_status st;

    iso = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_iso_new(a.cat, a.mors[1], a.mors[1], alloc, &iso);
    if (st == CAT89_OK)
    {
        T_ASSERT(iso != NULL);
        cat89_iso_release(iso);
    }
    else
    {
        T_ASSERT(iso == NULL);
    }
    amb_close(&a);
    return st;
}

static cat89_status try_iso_identity(const cat89_allocator *alloc)
{
    struct amb a;
    const cat89_obj *obj;
    cat89_iso *iso;
    cat89_status st;

    iso = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    obj = amb_first_obj(&a);
    T_ASSERT(obj != NULL);
    if (obj != NULL)
    {
        st = cat89_iso_identity(a.cat, obj, alloc, &iso);
        if (st == CAT89_OK)
        {
            T_ASSERT(iso != NULL);
            cat89_iso_release(iso);
        }
        else
        {
            T_ASSERT(iso == NULL);
        }
    }
    amb_close(&a);
    return st;
}

static cat89_status try_split_mono_new(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_split_mono *sp;
    cat89_status st;

    sp = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_split_mono_new(a.cat, a.mors[1], a.mors[1], alloc, &sp);
    if (st == CAT89_OK)
    {
        T_ASSERT(sp != NULL);
        cat89_split_mono_release(sp);
    }
    else
    {
        T_ASSERT(sp == NULL);
    }
    amb_close(&a);
    return st;
}

static cat89_status try_split_epi_new(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_split_epi *sp;
    cat89_status st;

    sp = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_split_epi_new(a.cat, a.mors[1], a.mors[1], alloc, &sp);
    if (st == CAT89_OK)
    {
        T_ASSERT(sp != NULL);
        cat89_split_epi_release(sp);
    }
    else
    {
        T_ASSERT(sp == NULL);
    }
    amb_close(&a);
    return st;
}

/* ---- cone / cocone / limit / colimit ---------------------------------- */

static cat89_status leg_id(void *ctx, const cat89_obj *obj, cat89_mor **out_mor)
{
    return cat89_identity((cat89_category *)ctx, obj, out_mor);
}

static const cat89_cone_ops cone_ops = {leg_id, NULL};

static cat89_status limit_factor_id(void *ctx, const cat89_cone *cand,
                                    cat89_mor **out_mor)
{
    return cat89_identity((cat89_category *)ctx, cat89_cone_apex(cand),
                          out_mor);
}

static const cat89_limit_ops limit_ops = {limit_factor_id, NULL};

static cat89_status colimit_factor_id(void *ctx, const cat89_cocone *cand,
                                      cat89_mor **out_mor)
{
    return cat89_identity((cat89_category *)ctx, cat89_cocone_apex(cand),
                          out_mor);
}

static const cat89_colimit_ops colimit_ops = {colimit_factor_id, NULL};

static cat89_status try_cone_new(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_functor *dia;
    const cat89_obj *apex;
    cat89_cone *cone;
    cat89_status st;

    dia = NULL;
    cone = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(a.cat, NULL, &dia);
    if (st == CAT89_OK)
    {
        apex = amb_first_obj(&a);
        if (apex != NULL)
        {
            st = cat89_cone_new(dia, apex, &cone_ops, a.cat, alloc, &cone);
            if (st == CAT89_OK)
            {
                T_ASSERT(cone != NULL);
                cat89_cone_release(cone);
            }
            else
            {
                T_ASSERT(cone == NULL);
            }
        }
    }
    if (dia != NULL)
    {
        cat89_functor_release(dia);
    }
    amb_close(&a);
    return st;
}

static cat89_status try_cocone_new(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_functor *dia;
    const cat89_obj *apex;
    cat89_cocone *cocone;
    cat89_status st;

    dia = NULL;
    cocone = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(a.cat, NULL, &dia);
    if (st == CAT89_OK)
    {
        apex = amb_first_obj(&a);
        if (apex != NULL)
        {
            st = cat89_cocone_new(dia, apex, &cone_ops, a.cat, alloc, &cocone);
            if (st == CAT89_OK)
            {
                T_ASSERT(cocone != NULL);
                cat89_cocone_release(cocone);
            }
            else
            {
                T_ASSERT(cocone == NULL);
            }
        }
    }
    if (dia != NULL)
    {
        cat89_functor_release(dia);
    }
    amb_close(&a);
    return st;
}

static cat89_status try_limit_new(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_functor *dia;
    const cat89_obj *apex;
    cat89_cone *cone;
    cat89_limit *lim;
    cat89_status st;

    dia = NULL;
    cone = NULL;
    lim = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(a.cat, NULL, &dia);
    if (st == CAT89_OK)
    {
        apex = amb_first_obj(&a);
        if (apex != NULL)
        {
            st = cat89_cone_new(dia, apex, &cone_ops, a.cat, NULL, &cone);
            if (st == CAT89_OK)
            {
                st = cat89_limit_new(cone, &limit_ops, a.cat, alloc, &lim);
                if (st == CAT89_OK)
                {
                    T_ASSERT(lim != NULL);
                    cat89_limit_release(lim);
                }
                else
                {
                    T_ASSERT(lim == NULL);
                }
            }
        }
    }
    if (cone != NULL)
    {
        cat89_cone_release(cone);
    }
    if (dia != NULL)
    {
        cat89_functor_release(dia);
    }
    amb_close(&a);
    return st;
}

static cat89_status try_colimit_new(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_functor *dia;
    const cat89_obj *apex;
    cat89_cocone *cocone;
    cat89_colimit *col;
    cat89_status st;

    dia = NULL;
    cocone = NULL;
    col = NULL;
    st = amb_open(CAT89_FIX_C2_GROUPOID, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(a.cat, NULL, &dia);
    if (st == CAT89_OK)
    {
        apex = amb_first_obj(&a);
        if (apex != NULL)
        {
            st = cat89_cocone_new(dia, apex, &cone_ops, a.cat, NULL, &cocone);
            if (st == CAT89_OK)
            {
                st =
                    cat89_colimit_new(cocone, &colimit_ops, a.cat, alloc, &col);
                if (st == CAT89_OK)
                {
                    T_ASSERT(col != NULL);
                    cat89_colimit_release(col);
                }
                else
                {
                    T_ASSERT(col == NULL);
                }
            }
        }
    }
    if (cocone != NULL)
    {
        cat89_cocone_release(cocone);
    }
    if (dia != NULL)
    {
        cat89_functor_release(dia);
    }
    amb_close(&a);
    return st;
}

/* ---- finite universal-construction finders (partial ambients) --------- */

static const cat89_obj *amb_cod(const struct amb *a, unsigned long i)
{
    const cat89_obj *obj;

    obj = NULL;
    if (cat89_cod(a->cat, a->mors[i], &obj) != CAT89_OK)
    {
        return NULL;
    }
    return obj;
}

static const cat89_obj *amb_dom(const struct amb *a, unsigned long i)
{
    const cat89_obj *obj;

    obj = NULL;
    if (cat89_dom(a->cat, a->mors[i], &obj) != CAT89_OK)
    {
        return NULL;
    }
    return obj;
}

static cat89_status try_find_product(const cat89_allocator *alloc)
{
    struct amb a;
    const cat89_obj *pa;
    const cat89_obj *pb;
    cat89_limit *lim;
    cat89_status st;

    pa = NULL;
    pb = NULL;
    lim = NULL;
    st = amb_open(CAT89_FIX_BINPROD, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    pa = amb_cod(&a, 4);
    pb = amb_cod(&a, 5);
    T_ASSERT(pa != NULL);
    T_ASSERT(pb != NULL);
    if (pa != NULL && pb != NULL)
    {
        st = cat89_find_binary_product(a.cat, a.en, a.eq, pa, pb, alloc, &lim,
                                       NULL, NULL);
        if (st == CAT89_OK)
        {
            T_ASSERT(lim != NULL);
            cat89_limit_release(lim);
        }
        else
        {
            T_ASSERT(lim == NULL);
        }
    }
    amb_close(&a);
    return st;
}

static cat89_status try_find_coproduct(const cat89_allocator *alloc)
{
    struct amb a;
    const cat89_obj *pa;
    const cat89_obj *pb;
    cat89_colimit *col;
    cat89_status st;

    pa = NULL;
    pb = NULL;
    col = NULL;
    st = amb_open(CAT89_FIX_BINCOPROD, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    pa = amb_dom(&a, 4);
    pb = amb_dom(&a, 5);
    T_ASSERT(pa != NULL);
    T_ASSERT(pb != NULL);
    if (pa != NULL && pb != NULL)
    {
        st = cat89_find_binary_coproduct(a.cat, a.en, a.eq, pa, pb, alloc, &col,
                                         NULL, NULL);
        if (st == CAT89_OK)
        {
            T_ASSERT(col != NULL);
            cat89_colimit_release(col);
        }
        else
        {
            T_ASSERT(col == NULL);
        }
    }
    amb_close(&a);
    return st;
}

static cat89_status try_find_equalizer(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_limit *lim;
    cat89_status st;

    lim = NULL;
    st = amb_open(CAT89_FIX_EQUALIZER, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_find_equalizer(a.cat, a.en, a.eq, a.mors[5], a.mors[6], alloc,
                              &lim, NULL, NULL);
    if (st == CAT89_OK)
    {
        T_ASSERT(lim != NULL);
        cat89_limit_release(lim);
    }
    else
    {
        T_ASSERT(lim == NULL);
    }
    amb_close(&a);
    return st;
}

static cat89_status try_find_coequalizer(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_colimit *col;
    cat89_status st;

    col = NULL;
    st = amb_open(CAT89_FIX_COEQUALIZER, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_find_coequalizer(a.cat, a.en, a.eq, a.mors[4], a.mors[5], alloc,
                                &col, NULL, NULL);
    if (st == CAT89_OK)
    {
        T_ASSERT(col != NULL);
        cat89_colimit_release(col);
    }
    else
    {
        T_ASSERT(col == NULL);
    }
    amb_close(&a);
    return st;
}

static cat89_status try_find_pullback(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_limit *lim;
    cat89_status st;

    lim = NULL;
    st = amb_open(CAT89_FIX_PULLBACK, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_find_pullback(a.cat, a.en, a.eq, a.mors[8], a.mors[9], alloc,
                             &lim, NULL, NULL, NULL);
    if (st == CAT89_OK)
    {
        T_ASSERT(lim != NULL);
        cat89_limit_release(lim);
    }
    else
    {
        T_ASSERT(lim == NULL);
    }
    amb_close(&a);
    return st;
}

static cat89_status try_find_pushout(const cat89_allocator *alloc)
{
    struct amb a;
    cat89_colimit *col;
    cat89_status st;

    col = NULL;
    st = amb_open(CAT89_FIX_PUSHOUT, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_find_pushout(a.cat, a.en, a.eq, a.mors[5], a.mors[6], alloc,
                            &col, NULL, NULL, NULL);
    if (st == CAT89_OK)
    {
        T_ASSERT(col != NULL);
        cat89_colimit_release(col);
    }
    else
    {
        T_ASSERT(col == NULL);
    }
    amb_close(&a);
    return st;
}

static void test_wrapper_and_derived_ctors(void)
{
    drive(try_opposite);
    drive(try_product);
    drive(try_arrow);
    drive(try_slice);
    drive(try_coslice);
    drive(try_comma);
    drive(try_functor_new);
    drive(try_functor_identity);
    drive(try_nat_new);
    drive(try_nat_identity);
    drive(try_iso_new);
    drive(try_iso_identity);
    drive(try_split_mono_new);
    drive(try_split_epi_new);
    drive(try_cone_new);
    drive(try_cocone_new);
    drive(try_limit_new);
    drive(try_colimit_new);
    drive(try_find_product);
    drive(try_find_coproduct);
    drive(try_find_equalizer);
    drive(try_find_coequalizer);
    drive(try_find_pullback);
    drive(try_find_pushout);
}

int main(void)
{
    T_START();
    test_wrapper_and_derived_ctors();
    return T_END() ? 0 : 1;
}
