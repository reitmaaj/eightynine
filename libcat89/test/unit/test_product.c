/* cat89_test_product.c - product category C x D tests.
 *
 * Builds a product over finite fixtures, verifies componentwise behaviour on a
 * concrete composition, and exhaustively checks identity + associativity over a
 * wrapped product using an externally supplied equality that compares product
 * morphisms by their two components. Test code is not green-owned; it only must
 * compile strict-C89 warning-clean. */
#include <stdlib.h>

#include "finite_categories.h"
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

/* Externally supplied equality over a product category: two product morphisms
 * are equal iff both component morphisms are equal under the base equalities.
 */
struct peq_ctx
{
    cat89_category *category;
    cat89_eq *left_eq;
    cat89_eq *right_eq;
};

static cat89_status p_obj_equal(void *ctx, const cat89_obj *a,
                                const cat89_obj *b, int *out_equal)
{
    struct peq_ctx *pc = ctx;
    cat89_status st;
    int el;

    st = cat89_obj_equal(pc->left_eq, cat89_product_left_obj(pc->category, a),
                         cat89_product_left_obj(pc->category, b), &el);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (el == 0)
    {
        *out_equal = 0;
        return CAT89_OK;
    }
    st = cat89_obj_equal(pc->right_eq, cat89_product_right_obj(pc->category, a),
                         cat89_product_right_obj(pc->category, b), out_equal);
    return st;
}

static cat89_status p_mor_equal(void *ctx, const cat89_mor *f,
                                const cat89_mor *g, int *out_equal)
{
    struct peq_ctx *pc = ctx;
    cat89_status st;
    int el;

    st = cat89_mor_equal(pc->left_eq, cat89_product_left_mor(pc->category, f),
                         cat89_product_left_mor(pc->category, g), &el);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (el == 0)
    {
        *out_equal = 0;
        return CAT89_OK;
    }
    st = cat89_mor_equal(pc->right_eq, cat89_product_right_mor(pc->category, f),
                         cat89_product_right_mor(pc->category, g), out_equal);
    return st;
}

static void p_eq_destroy(void *ctx)
{
    struct peq_ctx *pc = ctx;

    cat89_eq_release(pc->left_eq);
    cat89_eq_release(pc->right_eq);
    free(pc);
}

static cat89_status make_product_eq(cat89_category *product, cat89_eq *left_eq,
                                    cat89_eq *right_eq, cat89_eq **out_eq)
{
    static const cat89_eq_ops ops = {p_obj_equal, p_mor_equal, p_eq_destroy};
    struct peq_ctx *pc;
    cat89_status st;

    pc = (struct peq_ctx *)malloc(sizeof(*pc));
    if (pc == NULL)
    {
        return CAT89_NOMEM;
    }
    pc->category = product;
    pc->left_eq = left_eq;
    pc->right_eq = right_eq;
    st = cat89_eq_retain(left_eq);
    if (st != CAT89_OK)
    {
        free(pc);
        return st;
    }
    st = cat89_eq_retain(right_eq);
    if (st != CAT89_OK)
    {
        cat89_eq_release(left_eq);
        free(pc);
        return st;
    }
    st = cat89_eq_new(product, &ops, pc, NULL, out_eq);
    if (st != CAT89_OK)
    {
        cat89_eq_release(right_eq);
        cat89_eq_release(left_eq);
        free(pc);
        return st;
    }
    return CAT89_OK;
}

/* Collect all owned morphisms of a fixture category into an allocated array. */
static unsigned long collect_morphs(cat89_category *cat,
                                    cat89_enum *enumeration, cat89_mor ***out)
{
    cat89_mor_iter *it;
    cat89_mor *mor;
    cat89_mor **arr;
    int done;
    unsigned long n;

    it = NULL;
    mor = NULL;
    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        return 0;
    }
    done = 0;
    n = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (!done)
        {
            cat89_mor_release(cat, mor);
            n = n + 1;
        }
    }
    cat89_mor_iter_close(enumeration, it);

    arr = (cat89_mor **)malloc(n * sizeof(cat89_mor *));
    if (arr == NULL)
    {
        return 0;
    }
    it = NULL;
    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        free(arr);
        return 0;
    }
    done = 0;
    n = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (!done)
        {
            arr[n] = mor;
            n = n + 1;
        }
    }
    cat89_mor_iter_close(enumeration, it);
    (void)cat;
    *out = arr;
    return n;
}

static void release_all(cat89_category *cat, cat89_mor **arr, unsigned long n)
{
    unsigned long i;

    for (i = 0; i < n; i = i + 1)
    {
        cat89_mor_release(cat, arr[i]);
    }
    free(arr);
}

static int composable(cat89_category *cat, const cat89_mor *g,
                      const cat89_mor *f)
{
    const cat89_obj *codf;
    const cat89_obj *domg;

    cat89_cod(cat, f, &codf);
    cat89_dom(cat, g, &domg);
    return cat89_obj_same(cat, codf, domg) != 0;
}

/* A base category fully as a pair of left/right fixture builds. */
struct base_pair
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *enumeration;
    cat89_mor **morphs;
    unsigned long nmorphs;
};

static void base_build(enum cat89_fixture_kind kind, struct base_pair *bp)
{
    bp->cat = NULL;
    bp->eq = NULL;
    bp->enumeration = NULL;
    bp->morphs = NULL;
    bp->nmorphs = 0;
    T_STATUS(cat89_fixture_build(kind, &bp->cat, &bp->eq, &bp->enumeration),
             CAT89_OK);
    bp->nmorphs = collect_morphs(bp->cat, bp->enumeration, &bp->morphs);
    T_ASSERT(bp->morphs != NULL);
    T_ASSERT(bp->nmorphs > 0);
}

static void base_teardown(struct base_pair *bp)
{
    cat89_category *cat = bp->cat;

    release_all(cat, bp->morphs, bp->nmorphs);
    cat89_enum_release(bp->enumeration);
    cat89_eq_release(bp->eq);
    cat89_category_release(cat);
}

/* Build the product of two fixture categories and wrap every (l, r) pair of
 * base morphisms as one owned product morphism. */
static unsigned long build_product(struct base_pair *l, struct base_pair *r,
                                   cat89_category **out_product,
                                   cat89_eq **out_peq, cat89_mor ***out_pm)
{
    cat89_category *product;
    cat89_eq *peq;
    cat89_mor **pm;
    unsigned long total;
    unsigned long i;
    unsigned long j;
    unsigned long k;

    product = NULL;
    peq = NULL;
    pm = NULL;
    T_STATUS(cat89_product_category_new(l->cat, r->cat, NULL, &product),
             CAT89_OK);
    T_ASSERT(product != NULL);
    T_STATUS(make_product_eq(product, l->eq, r->eq, &peq), CAT89_OK);

    total = l->nmorphs * r->nmorphs;
    pm = (cat89_mor **)malloc(total * sizeof(cat89_mor *));
    T_ASSERT(pm != NULL);
    k = 0;
    for (i = 0; i < l->nmorphs; i = i + 1)
    {
        for (j = 0; j < r->nmorphs; j = j + 1)
        {
            T_STATUS(cat89_product_mor_new(product, l->morphs[i], r->morphs[j],
                                           &pm[k]),
                     CAT89_OK);
            k = k + 1;
        }
    }
    *out_product = product;
    *out_peq = peq;
    *out_pm = pm;
    return total;
}

static void law_sweep(enum cat89_fixture_kind kind_l,
                      enum cat89_fixture_kind kind_r)
{
    struct base_pair l;
    struct base_pair r;
    cat89_category *product;
    cat89_eq *peq;
    cat89_mor **pm;
    unsigned long n;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    unsigned long assoc_cases;
    int valid;

    base_build(kind_l, &l);
    base_build(kind_r, &r);
    n = build_product(&l, &r, &product, &peq, &pm);
    T_EQ_UL(n, l.nmorphs * r.nmorphs);

    for (i = 0; i < n; i = i + 1)
    {
        valid = -1;
        T_STATUS(cat89_check_left_identity(product, peq, pm[i], &valid),
                 CAT89_OK);
        T_EQ_UL(valid, 1);
        valid = -1;
        T_STATUS(cat89_check_right_identity(product, peq, pm[i], &valid),
                 CAT89_OK);
        T_EQ_UL(valid, 1);
    }

    assoc_cases = 0;
    for (i = 0; i < n; i = i + 1)
    {
        for (j = 0; j < n; j = j + 1)
        {
            for (k = 0; k < n; k = k + 1)
            {
                if (!composable(product, pm[j], pm[k]))
                {
                    continue;
                }
                if (!composable(product, pm[i], pm[j]))
                {
                    continue;
                }
                valid = -1;
                T_STATUS(cat89_check_associativity(product, peq, pm[i], pm[j],
                                                   pm[k], &valid),
                         CAT89_OK);
                T_EQ_UL(valid, 1);
                assoc_cases = assoc_cases + 1;
            }
        }
    }
    T_ASSERT(assoc_cases > 0);

    for (i = 0; i < n; i = i + 1)
    {
        cat89_mor_release(product, pm[i]);
    }
    free(pm);
    cat89_eq_release(peq);
    cat89_category_release(product);
    base_teardown(&l);
    base_teardown(&r);
}

static int is_identity(cat89_category *cat, const cat89_mor *m)
{
    const cat89_obj *d;
    const cat89_obj *c;

    cat89_dom(cat, m, &d);
    cat89_cod(cat, m, &c);
    return cat89_obj_same(cat, d, c) != 0;
}

static int cmp_base_component(cat89_eq *leq, const cat89_mor *expect,
                              const cat89_mor *actual)
{
    int equal;

    equal = 0;
    T_STATUS(cat89_mor_equal(leq, expect, actual, &equal), CAT89_OK);
    return equal;
}

/* Concrete componentwise composition over COMP2 x COMP2: compose non-identity
 * composable pairs and check each product component equals the base compose. */
static void real_composition(void)
{
    struct base_pair l;
    struct base_pair r;
    cat89_category *product;
    cat89_eq *peq;
    cat89_mor *al;
    cat89_mor *bl;
    cat89_mor *ar;
    cat89_mor *br;
    cat89_mor *lcomp;
    cat89_mor *rcomp;
    cat89_mor *pa;
    cat89_mor *pb;
    cat89_mor *pc;
    unsigned long i;
    unsigned long j;
    int done;

    base_build(CAT89_FIX_COMP2, &l);
    base_build(CAT89_FIX_COMP2, &r);
    T_STATUS(cat89_product_category_new(l.cat, r.cat, NULL, &product),
             CAT89_OK);
    T_STATUS(make_product_eq(product, l.eq, r.eq, &peq), CAT89_OK);

    al = NULL;
    bl = NULL;
    ar = NULL;
    br = NULL;
    done = 0;
    for (i = 0; i < l.nmorphs && !done; i = i + 1)
    {
        if (is_identity(l.cat, l.morphs[i]))
        {
            continue;
        }
        for (j = 0; j < r.nmorphs && !done; j = j + 1)
        {
            if (!composable(l.cat, l.morphs[j], l.morphs[i]))
            {
                continue;
            }
            if (!composable(r.cat, r.morphs[j], r.morphs[i]))
            {
                continue;
            }
            al = l.morphs[i];
            bl = l.morphs[j];
            ar = r.morphs[i];
            br = r.morphs[j];
            done = 1;
        }
    }
    T_ASSERT(done);

    lcomp = NULL;
    rcomp = NULL;
    T_STATUS(cat89_compose(l.cat, bl, al, &lcomp), CAT89_OK);
    T_STATUS(cat89_compose(r.cat, br, ar, &rcomp), CAT89_OK);

    pa = NULL;
    pb = NULL;
    pc = NULL;
    T_STATUS(cat89_product_mor_new(product, al, ar, &pa), CAT89_OK);
    T_STATUS(cat89_product_mor_new(product, bl, br, &pb), CAT89_OK);
    T_STATUS(cat89_compose(product, pb, pa, &pc), CAT89_OK);

    T_ASSERT(cmp_base_component(l.eq, lcomp,
                                cat89_product_left_mor(product, pc)) != 0);
    T_ASSERT(cmp_base_component(r.eq, rcomp,
                                cat89_product_right_mor(product, pc)) != 0);

    cat89_mor_release(product, pc);
    cat89_mor_release(product, pb);
    cat89_mor_release(product, pa);
    cat89_mor_release(l.cat, lcomp);
    cat89_mor_release(r.cat, rcomp);
    cat89_eq_release(peq);
    cat89_category_release(product);
    base_teardown(&l);
    base_teardown(&r);
}

int main(void)
{
    T_START();
    real_composition();
    law_sweep(CAT89_FIX_ARROW, CAT89_FIX_COMP2);
    law_sweep(CAT89_FIX_COMP2, CAT89_FIX_ARROW);
    return T_END() ? 0 : 1;
}
