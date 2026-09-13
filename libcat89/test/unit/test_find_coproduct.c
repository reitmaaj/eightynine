/* cat89_test_find_coproduct.c - finite binary-coproduct finder.
 *
 * Finds the coproduct S = a + b in the BINCOPROD ambient and verifies the
 * returned cat89_colimit: apex, injections (dom/cod), and the universal factor
 * of a real candidate cocone via cat89_colimit_factor (CAT-I9). Also verifies
 * NOT_FOUND when no coproduct exists and INVALID on unusable arguments. */
#include <stdlib.h>

#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

static const cat89_obj *obj_index(cat89_enum *enumeration, unsigned long want)
{
    cat89_obj_iter *it;
    const cat89_obj *obj;
    unsigned long i;
    int done;

    it = NULL;
    if (cat89_obj_iter_open(enumeration, &it) != CAT89_OK)
    {
        return NULL;
    }
    i = 0;
    done = 0;
    obj = NULL;
    while (!done)
    {
        obj = NULL;
        cat89_obj_iter_next(enumeration, it, &obj, &done);
        if (!done)
        {
            if (i == want)
            {
                cat89_obj_iter_close(enumeration, it);
                return obj;
            }
            i = i + 1;
        }
    }
    cat89_obj_iter_close(enumeration, it);
    return NULL;
}

static cat89_status grab_mor(cat89_category *cat, cat89_enum *enumeration,
                             unsigned long want, cat89_mor **out)
{
    cat89_mor_iter *it;
    cat89_mor *mor;
    unsigned long i;
    int done;

    it = NULL;
    *out = NULL;
    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        return CAT89_INVALID;
    }
    i = 0;
    done = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (!done)
        {
            if (i == want)
            {
                cat89_mor_iter_close(enumeration, it);
                *out = mor;
                return CAT89_OK;
            }
            cat89_mor_release(cat, mor);
            i = i + 1;
        }
    }
    cat89_mor_iter_close(enumeration, it);
    return CAT89_NOT_FOUND;
}

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
    cat89_status st;

    *out_mor = NULL;
    if (shape_obj == lc->pt_a)
    {
        st = cat89_mor_retain(lc->cat, lc->xa);
        if (st != CAT89_OK)
        {
            return st;
        }
        *out_mor = lc->xa;
        return CAT89_OK;
    }
    if (shape_obj == lc->pt_b)
    {
        st = cat89_mor_retain(lc->cat, lc->xb);
        if (st != CAT89_OK)
        {
            return st;
        }
        *out_mor = lc->xb;
        return CAT89_OK;
    }
    return CAT89_INVALID;
}

static void test_coproduct_found(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    const cat89_obj *A;
    const cat89_obj *B;
    const cat89_obj *S;
    const cat89_obj *X;
    const cat89_obj *pt_a;
    const cat89_obj *pt_b;
    cat89_diagram *diagram;
    cat89_cocone *cocone;
    cat89_colimit *colimit;
    cat89_cocone *candidate;
    cat89_mor *leg;
    cat89_mor *u;
    cat89_mor *med_ref;
    cat89_mor *ya;
    cat89_mor *yb;
    cat89_cone_ops cops;
    struct legctx lc;
    const cat89_obj *d;
    const cat89_obj *c;
    cat89_status st;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    colimit = NULL;
    pt_a = NULL;
    pt_b = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_BINCOPROD, &cat, &eq, &enumeration),
             CAT89_OK);
    A = obj_index(enumeration, 0);
    B = obj_index(enumeration, 1);
    S = obj_index(enumeration, 2);
    X = obj_index(enumeration, 3);
    T_ASSERT(A != NULL && B != NULL && S != NULL && X != NULL);

    st = cat89_find_binary_coproduct(cat, enumeration, eq, A, B, NULL, &colimit,
                                     &pt_a, &pt_b);
    T_STATUS(st, CAT89_OK);
    T_ASSERT(colimit != NULL);
    T_ASSERT(pt_a != NULL && pt_b != NULL);

    cocone = cat89_colimit_cocone(colimit);
    T_ASSERT(cat89_obj_same(cat, cat89_cocone_apex(cocone), S));

    leg = NULL;
    T_STATUS(cat89_cocone_leg(cocone, pt_a, &leg), CAT89_OK);
    cat89_dom(cat, leg, &d);
    cat89_cod(cat, leg, &c);
    T_ASSERT(cat89_obj_same(cat, d, A));
    T_ASSERT(cat89_obj_same(cat, c, S));
    cat89_mor_release(cat, leg);
    leg = NULL;
    T_STATUS(cat89_cocone_leg(cocone, pt_b, &leg), CAT89_OK);
    cat89_dom(cat, leg, &d);
    T_ASSERT(cat89_obj_same(cat, d, B));
    cat89_mor_release(cat, leg);

    /* candidate cocone over the returned diagram: apex X, legs yA:A->X,
     * yB:B->X */
    diagram = cat89_cocone_diagram(cocone);
    ya = NULL;
    yb = NULL;
    T_STATUS(grab_mor(cat, enumeration, 6, &ya), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 7, &yb), CAT89_OK);
    T_ASSERT(ya != NULL && yb != NULL);
    lc.cat = cat;
    lc.pt_a = pt_a;
    lc.pt_b = pt_b;
    lc.xa = ya;
    lc.xb = yb;
    cops.leg = cand_leg;
    cops.destroy = NULL;
    candidate = NULL;
    T_STATUS(cat89_cocone_new(diagram, X, &cops, &lc, NULL, &candidate),
             CAT89_OK);

    u = NULL;
    T_STATUS(cat89_colimit_factor(colimit, candidate, &u), CAT89_OK);
    T_ASSERT(u != NULL);
    cat89_dom(cat, u, &d);
    cat89_cod(cat, u, &c);
    T_ASSERT(cat89_obj_same(cat, d, S));
    T_ASSERT(cat89_obj_same(cat, c, X));
    med_ref = NULL;
    T_STATUS(grab_mor(cat, enumeration, 8, &med_ref), CAT89_OK);
    {
        int eqv = 0;
        T_STATUS(cat89_mor_equal(eq, u, med_ref, &eqv), CAT89_OK);
        T_EQ_UL(eqv, 1);
    }
    cat89_mor_release(cat, med_ref);
    cat89_mor_release(cat, u);

    cat89_cocone_release(candidate);
    cat89_mor_release(cat, ya);
    cat89_mor_release(cat, yb);
    cat89_colimit_release(colimit);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_coproduct_absent(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    const cat89_obj *o0;
    const cat89_obj *o1;
    cat89_colimit *colimit;
    cat89_status st;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    colimit = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_DISCRETE2, &cat, &eq, &enumeration),
             CAT89_OK);
    o0 = obj_index(enumeration, 0);
    o1 = obj_index(enumeration, 1);
    T_ASSERT(o0 != NULL && o1 != NULL);
    st = cat89_find_binary_coproduct(cat, enumeration, eq, o0, o1, NULL,
                                     &colimit, NULL, NULL);
    T_STATUS(st, CAT89_NOT_FOUND);
    T_ASSERT(colimit == NULL);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_coproduct_invalid(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    const cat89_obj *o0;
    const cat89_obj *o1;
    cat89_colimit *colimit;
    cat89_status st;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    colimit = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_BINCOPROD, &cat, &eq, &enumeration),
             CAT89_OK);
    o0 = obj_index(enumeration, 0);
    o1 = obj_index(enumeration, 1);
    st = cat89_find_binary_coproduct(cat, enumeration, NULL, o0, o1, NULL,
                                     &colimit, NULL, NULL);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(colimit == NULL);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_coproduct_found();
    test_coproduct_absent();
    test_coproduct_invalid();
    return T_END() ? 0 : 1;
}
