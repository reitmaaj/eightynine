/* cat89_test_factor_cardinality.c - FM01-FM06: finder factor cardinality.
 *
 * A finder-created factor returns CAT89_OK with exactly one owned mediator,
 * and CAT89_NOT_FOUND with NULL when the finite search sees no mediator.
 * CAT89_OK + NULL is never returned. (A genuine limit admits at most one
 * mediator for any candidate, so a two-mediator candidate cannot arise from a
 * finder-returned limit; the finder instead rejects the non-universal apex,
 * which the *-absent tests cover.) */
#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

struct legctx
{
    cat89_category *cat;
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    cat89_mor *xa;
    cat89_mor *xb;
};

static cat89_status cand_leg(void *ctx, const cat89_obj *shape_obj,
                             cat89_mor **out_mor)
{
    struct legctx *lc = ctx;
    cat89_mor *src;
    cat89_status st;

    *out_mor = NULL;
    if (shape_obj == lc->pt_a)
    {
        src = lc->xa;
    }
    else if (shape_obj == lc->pt_b)
    {
        src = lc->xb;
    }
    else
    {
        return CAT89_INVALID;
    }
    st = cat89_mor_retain(lc->cat, src);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_mor = src;
    return CAT89_OK;
}

static cat89_status grab_obj(cat89_enum *en, unsigned long idx,
                             const cat89_obj **out)
{
    cat89_obj_iter *it;
    const cat89_obj *o;
    unsigned long i;
    int done;
    cat89_status st;

    it = NULL;
    st = cat89_obj_iter_open(en, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    o = NULL;
    i = 0;
    done = 0;
    while (!done && i <= idx)
    {
        o = NULL;
        st = cat89_obj_iter_next(en, it, &o, &done);
        if (st != CAT89_OK)
        {
            cat89_obj_iter_close(en, it);
            return st;
        }
        i = i + 1;
    }
    cat89_obj_iter_close(en, it);
    if (done)
    {
        return CAT89_NOT_FOUND;
    }
    *out = o;
    return CAT89_OK;
}

static void check_factor_not_ok_null(cat89_status st, cat89_mor *m)
{
    T_ASSERT(!(st == CAT89_OK && m == NULL));
}

static void test_fm01_fm02_product_factor(void)
{
    cat89_category *cat;
    cat89_enum *en;
    cat89_eq *eq;
    const cat89_obj *A;
    const cat89_obj *B;
    const cat89_obj *P;
    const cat89_obj *X;
    cat89_limit *limit;
    cat89_cone *cone;
    cat89_cone *candidate;
    cat89_cone_ops cops;
    struct legctx lc;
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    cat89_mor *pa;
    cat89_mor *pb;
    cat89_mor *u;

    cat = NULL;
    en = NULL;
    eq = NULL;
    limit = NULL;
    candidate = NULL;
    pa = NULL;
    pb = NULL;
    u = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_BINPROD, &cat, &eq, &en), CAT89_OK);
    A = NULL;
    B = NULL;
    P = NULL;
    X = NULL;
    T_STATUS(grab_obj(en, 0, &A), CAT89_OK);
    T_STATUS(grab_obj(en, 1, &B), CAT89_OK);
    T_STATUS(grab_obj(en, 2, &P), CAT89_OK);
    T_STATUS(grab_obj(en, 3, &X), CAT89_OK);
    T_STATUS(cat89_find_binary_product(cat, en, eq, A, B, NULL, &limit, &pt_a,
                                       &pt_b),
             CAT89_OK);
    T_ASSERT(limit != NULL);
    cone = cat89_limit_cone(limit);
    T_STATUS(cat89_cone_leg(cone, pt_a, &pa), CAT89_OK);
    T_STATUS(cat89_cone_leg(cone, pt_b, &pb), CAT89_OK);

    /* FM01: the limit's own cone has the unique identity mediator. */
    T_STATUS(cat89_limit_factor(limit, cone, &u), CAT89_OK);
    T_ASSERT(u != NULL);
    T_ASSERT(cat89_owns_mor(cat, u) == 1);
    cat89_mor_release(cat, u);
    u = NULL;

    /* FM02: apex P with both legs pa -> no mediator. */
    lc.cat = cat;
    lc.pt_a = pt_a;
    lc.pt_b = pt_b;
    lc.xa = pa;
    lc.xb = pa;
    cops.leg = cand_leg;
    cops.destroy = NULL;
    T_STATUS(cat89_cone_new(cat89_cone_diagram(cone), P, &cops, &lc, NULL,
                            &candidate),
             CAT89_OK);
    T_STATUS(cat89_limit_factor(limit, candidate, &u), CAT89_NOT_FOUND);
    T_ASSERT(u == NULL);
    check_factor_not_ok_null(CAT89_NOT_FOUND, u);

    cat89_cone_release(candidate);
    cat89_mor_release(cat, pb);
    cat89_mor_release(cat, pa);
    cat89_limit_release(limit);
    (void)X;
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_fm05_fm06_coproduct_factor(void)
{
    cat89_category *cat;
    cat89_enum *en;
    cat89_eq *eq;
    const cat89_obj *A;
    const cat89_obj *B;
    const cat89_obj *S;
    cat89_colimit *colimit;
    cat89_cocone *cocone;
    cat89_cocone *candidate;
    cat89_cone_ops cops;
    struct legctx lc;
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    cat89_mor *ia;
    cat89_mor *ib;
    cat89_mor *u;

    cat = NULL;
    en = NULL;
    eq = NULL;
    colimit = NULL;
    candidate = NULL;
    ia = NULL;
    ib = NULL;
    u = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_BINCOPROD, &cat, &eq, &en),
             CAT89_OK);
    A = NULL;
    B = NULL;
    S = NULL;
    T_STATUS(grab_obj(en, 0, &A), CAT89_OK);
    T_STATUS(grab_obj(en, 1, &B), CAT89_OK);
    T_STATUS(grab_obj(en, 2, &S), CAT89_OK);
    T_STATUS(cat89_find_binary_coproduct(cat, en, eq, A, B, NULL, &colimit,
                                         &pt_a, &pt_b),
             CAT89_OK);
    T_ASSERT(colimit != NULL);
    cocone = cat89_colimit_cocone(colimit);
    T_STATUS(cat89_cocone_leg(cocone, pt_a, &ia), CAT89_OK);
    T_STATUS(cat89_cocone_leg(cocone, pt_b, &ib), CAT89_OK);

    /* FM05: the colimit's own cocone has the unique identity mediator. */
    T_STATUS(cat89_colimit_factor(colimit, cocone, &u), CAT89_OK);
    T_ASSERT(u != NULL);
    T_ASSERT(cat89_owns_mor(cat, u) == 1);
    cat89_mor_release(cat, u);
    u = NULL;

    /* FM06: apex S with both legs ia -> no mediator. */
    lc.cat = cat;
    lc.pt_a = pt_a;
    lc.pt_b = pt_b;
    lc.xa = ia;
    lc.xb = ia;
    cops.leg = cand_leg;
    cops.destroy = NULL;
    T_STATUS(cat89_cocone_new(cat89_cocone_diagram(cocone), S, &cops, &lc, NULL,
                              &candidate),
             CAT89_OK);
    T_STATUS(cat89_colimit_factor(colimit, candidate, &u), CAT89_NOT_FOUND);
    T_ASSERT(u == NULL);
    check_factor_not_ok_null(CAT89_NOT_FOUND, u);

    cat89_cocone_release(candidate);
    cat89_mor_release(cat, ib);
    cat89_mor_release(cat, ia);
    cat89_colimit_release(colimit);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

/* TC32/TC33: a candidate over a different diagram is rejected. */
static void test_tc32_tc33_wrong_diagram(void)
{
    cat89_category *cat;
    cat89_enum *en;
    cat89_eq *eq;
    const cat89_obj *A;
    const cat89_obj *B;
    cat89_limit *l1;
    cat89_limit *l2;
    cat89_colimit *c1;
    cat89_colimit *c2;
    cat89_mor *u;

    cat = NULL;
    en = NULL;
    eq = NULL;
    l1 = NULL;
    l2 = NULL;
    c1 = NULL;
    c2 = NULL;
    u = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_BINPROD, &cat, &eq, &en), CAT89_OK);
    A = NULL;
    B = NULL;
    T_STATUS(grab_obj(en, 0, &A), CAT89_OK);
    T_STATUS(grab_obj(en, 1, &B), CAT89_OK);
    T_STATUS(
        cat89_find_binary_product(cat, en, eq, A, B, NULL, &l1, NULL, NULL),
        CAT89_OK);
    T_STATUS(
        cat89_find_binary_product(cat, en, eq, A, B, NULL, &l2, NULL, NULL),
        CAT89_OK);
    T_ASSERT(l1 != NULL && l2 != NULL);
    T_ASSERT(cat89_cone_diagram(cat89_limit_cone(l1)) !=
             cat89_cone_diagram(cat89_limit_cone(l2)));
    T_STATUS(cat89_limit_factor(l1, cat89_limit_cone(l2), &u), CAT89_INVALID);
    T_ASSERT(u == NULL);
    cat89_limit_release(l2);
    cat89_limit_release(l1);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);

    cat = NULL;
    en = NULL;
    eq = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_BINCOPROD, &cat, &eq, &en),
             CAT89_OK);
    A = NULL;
    B = NULL;
    T_STATUS(grab_obj(en, 0, &A), CAT89_OK);
    T_STATUS(grab_obj(en, 1, &B), CAT89_OK);
    T_STATUS(
        cat89_find_binary_coproduct(cat, en, eq, A, B, NULL, &c1, NULL, NULL),
        CAT89_OK);
    T_STATUS(
        cat89_find_binary_coproduct(cat, en, eq, A, B, NULL, &c2, NULL, NULL),
        CAT89_OK);
    T_ASSERT(c1 != NULL && c2 != NULL);
    T_STATUS(cat89_colimit_factor(c1, cat89_colimit_cocone(c2), &u),
             CAT89_INVALID);
    T_ASSERT(u == NULL);
    cat89_colimit_release(c2);
    cat89_colimit_release(c1);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_fm01_fm02_product_factor();
    test_fm05_fm06_coproduct_factor();
    test_tc32_tc33_wrong_diagram();
    return T_END() ? 0 : 1;
}
