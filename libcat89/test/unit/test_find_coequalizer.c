/* cat89_test_find_coequalizer.c - finite coequalizer finder.
 *
 * Finds the coequalizer Q of parallel arrows f, g : X -> Y in the COEQUALIZER
 * ambient and verifies the returned cat89_colimit: apex Q (not the decoy W),
 * the cocone leg c : Y -> Q, and the universal factor of a real candidate
 * cocone via cat89_colimit_factor (CAT-I9). Also NOT_FOUND and INVALID. */
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
    const cat89_obj *pt0;
    const cat89_obj *pt1;
    cat89_mor *l0;
    cat89_mor *l1;
};

static cat89_status cand_leg(void *ctx, const cat89_obj *shape_obj,
                             cat89_mor **out_mor)
{
    struct legctx *lc = ctx;
    cat89_status st;

    *out_mor = NULL;
    if (shape_obj == lc->pt0)
    {
        st = cat89_mor_retain(lc->cat, lc->l0);
        if (st != CAT89_OK)
        {
            return st;
        }
        *out_mor = lc->l0;
        return CAT89_OK;
    }
    if (shape_obj == lc->pt1)
    {
        st = cat89_mor_retain(lc->cat, lc->l1);
        if (st != CAT89_OK)
        {
            return st;
        }
        *out_mor = lc->l1;
        return CAT89_OK;
    }
    return CAT89_INVALID;
}

static void test_coequalizer_found(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    const cat89_obj *Q;
    const cat89_obj *W;
    const cat89_obj *pt0;
    const cat89_obj *pt1;
    cat89_diagram *diagram;
    cat89_cocone *cocone;
    cat89_colimit *colimit;
    cat89_cocone *candidate;
    cat89_mor *f;
    cat89_mor *g;
    cat89_mor *c;
    cat89_mor *a1;
    cat89_mor *a0;
    cat89_mor *d;
    cat89_mor *leg;
    cat89_mor *u;
    cat89_cone_ops cops;
    struct legctx lc;
    const cat89_obj *dm;
    const cat89_obj *cd;
    cat89_status st;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    colimit = NULL;
    pt0 = NULL;
    pt1 = NULL;
    T_STATUS(
        cat89_fixture_build(CAT89_FIX_COEQUALIZER, &cat, &eq, &enumeration),
        CAT89_OK);
    Q = obj_index(enumeration, 0);
    W = obj_index(enumeration, 3);
    T_ASSERT(Q != NULL && W != NULL);

    f = NULL;
    g = NULL;
    c = NULL;
    a1 = NULL;
    a0 = NULL;
    d = NULL;
    T_STATUS(grab_mor(cat, enumeration, 4, &f), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 5, &g), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 6, &c), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 8, &a1), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 9, &a0), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 10, &d), CAT89_OK);
    T_ASSERT(f && g && c && a1 && a0 && d);

    st = cat89_find_coequalizer(cat, enumeration, eq, f, g, NULL, &colimit,
                                &pt0, &pt1);
    T_STATUS(st, CAT89_OK);
    T_ASSERT(colimit != NULL);
    T_ASSERT(pt0 != NULL && pt1 != NULL);

    cocone = cat89_colimit_cocone(colimit);
    T_ASSERT(cat89_obj_same(cat, cat89_cocone_apex(cocone), Q));

    leg = NULL;
    T_STATUS(cat89_cocone_leg(cocone, pt1, &leg), CAT89_OK);
    cat89_dom(cat, leg, &dm);
    cat89_cod(cat, leg, &cd);
    T_ASSERT(cat89_obj_same(cat, dm, obj_index(enumeration, 2)));
    T_ASSERT(cat89_obj_same(cat, cd, Q));
    {
        int eqv = 0;
        T_STATUS(cat89_mor_equal(eq, leg, c, &eqv), CAT89_OK);
        T_EQ_UL(eqv, 1);
    }
    cat89_mor_release(cat, leg);

    /* candidate cocone over the returned diagram: apex W, legs a0:X->W,
     * a1:Y->W */
    diagram = cat89_cocone_diagram(cocone);
    lc.cat = cat;
    lc.pt0 = pt0;
    lc.pt1 = pt1;
    lc.l0 = a0;
    lc.l1 = a1;
    cops.leg = cand_leg;
    cops.destroy = NULL;
    candidate = NULL;
    T_STATUS(cat89_cocone_new(diagram, W, &cops, &lc, NULL, &candidate),
             CAT89_OK);

    u = NULL;
    T_STATUS(cat89_colimit_factor(colimit, candidate, &u), CAT89_OK);
    T_ASSERT(u != NULL);
    cat89_dom(cat, u, &dm);
    cat89_cod(cat, u, &cd);
    T_ASSERT(cat89_obj_same(cat, dm, Q));
    T_ASSERT(cat89_obj_same(cat, cd, W));
    {
        int eqv = 0;
        T_STATUS(cat89_mor_equal(eq, u, d, &eqv), CAT89_OK);
        T_EQ_UL(eqv, 1);
    }
    cat89_mor_release(cat, u);

    cat89_cocone_release(candidate);
    cat89_mor_release(cat, f);
    cat89_mor_release(cat, g);
    cat89_mor_release(cat, c);
    cat89_mor_release(cat, a1);
    cat89_mor_release(cat, a0);
    cat89_mor_release(cat, d);
    cat89_colimit_release(colimit);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_coequalizer_absent(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    cat89_mor *f;
    cat89_mor *g;
    cat89_colimit *colimit;
    cat89_status st;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    colimit = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_EQUALIZER, &cat, &eq, &enumeration),
             CAT89_OK);
    f = NULL;
    g = NULL;
    T_STATUS(grab_mor(cat, enumeration, 5, &f), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 6, &g), CAT89_OK);
    T_ASSERT(f && g);
    st = cat89_find_coequalizer(cat, enumeration, eq, f, g, NULL, &colimit,
                                NULL, NULL);
    T_STATUS(st, CAT89_NOT_FOUND);
    T_ASSERT(colimit == NULL);
    cat89_mor_release(cat, f);
    cat89_mor_release(cat, g);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_coequalizer_invalid(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    cat89_mor *f;
    cat89_mor *g;
    cat89_colimit *colimit;
    cat89_status st;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    colimit = NULL;
    T_STATUS(
        cat89_fixture_build(CAT89_FIX_COEQUALIZER, &cat, &eq, &enumeration),
        CAT89_OK);
    f = NULL;
    g = NULL;
    T_STATUS(grab_mor(cat, enumeration, 4, &f), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 5, &g), CAT89_OK);
    st = cat89_find_coequalizer(cat, enumeration, NULL, f, g, NULL, &colimit,
                                NULL, NULL);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(colimit == NULL);
    cat89_mor_release(cat, f);
    cat89_mor_release(cat, g);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_coequalizer_found();
    test_coequalizer_absent();
    test_coequalizer_invalid();
    return T_END() ? 0 : 1;
}
