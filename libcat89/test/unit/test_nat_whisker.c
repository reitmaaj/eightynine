/* cat89_test_nat_whisker.c - left/right nat whiskering over identity functors.
 */
#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

static const cat89_obj *grab_object(cat89_enum *enumeration)
{
    cat89_obj_iter *it = NULL;
    const cat89_obj *obj = NULL;
    int done = 0;

    if (cat89_obj_iter_open(enumeration, &it) != CAT89_OK)
    {
        return NULL;
    }
    if (cat89_obj_iter_next(enumeration, it, &obj, &done) != CAT89_OK)
    {
        cat89_obj_iter_close(enumeration, it);
        return NULL;
    }
    cat89_obj_iter_close(enumeration, it);
    return obj;
}

static cat89_status component_id(void *ctx, const cat89_obj *obj,
                                 cat89_mor **out_mor)
{
    cat89_category *category = ctx;

    return cat89_identity(category, obj, out_mor);
}

static void check_identity_component(cat89_category *cat, cat89_nat *nat,
                                     const cat89_obj *obj)
{
    cat89_mor *comp = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *cc = NULL;
    cat89_status st;

    st = cat89_nat_component(nat, obj, &comp);
    T_STATUS(st, CAT89_OK);
    T_ASSERT(comp != NULL);
    cat89_dom(cat, comp, &d);
    cat89_cod(cat, comp, &cc);
    T_ASSERT(cat89_obj_same(cat, d, obj));
    T_ASSERT(cat89_obj_same(cat, cc, obj));
    cat89_mor_release(cat, comp);
}

static void test_whiskers(void)
{
    cat89_category *cat = NULL;
    cat89_category *cat2 = NULL;
    cat89_enum *enumeration = NULL;
    cat89_eq *eq = NULL;
    cat89_functor *f1 = NULL;
    cat89_functor *f2 = NULL;
    cat89_functor *h = NULL;
    cat89_functor *k2 = NULL;
    cat89_nat *alpha = NULL;
    cat89_nat *natL = NULL;
    cat89_nat *natR = NULL;
    cat89_nat_ops ops;
    const cat89_obj *obj = NULL;
    cat89_status st;

    T_STATUS(cat89_fixture_build(CAT89_FIX_COMP2, &cat, &eq, &enumeration),
             CAT89_OK);
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &cat2, NULL, NULL),
             CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);

    T_STATUS(cat89_functor_identity(cat, NULL, &f1), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &f2), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &h), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat2, NULL, &k2), CAT89_OK);

    ops.component = component_id;
    ops.destroy = NULL;
    T_STATUS(cat89_nat_new(f1, f2, &ops, cat, NULL, &alpha), CAT89_OK);

    /* left whisker by the identity functor: components stay identities. */
    st = cat89_nat_whisker_left(h, alpha, NULL, &natL);
    T_STATUS(st, CAT89_OK);
    T_ASSERT(natL != NULL);
    check_identity_component(cat, natL, obj);

    /* right whisker by the identity functor. */
    st = cat89_nat_whisker_right(alpha, h, NULL, &natR);
    T_STATUS(st, CAT89_OK);
    T_ASSERT(natR != NULL);
    check_identity_component(cat, natR, obj);

    cat89_nat_release(natL);
    cat89_nat_release(natR);
    natL = NULL;
    natR = NULL;

    /* mismatched functor (k over a different category) is rejected; use a
     * dummy out so an owned nat is not clobbered. */
    {
        cat89_nat *dummy = NULL;
        st = cat89_nat_whisker_right(alpha, k2, NULL, &dummy);
        T_STATUS(st, CAT89_INVALID);
        T_ASSERT(dummy == NULL);
        st = cat89_nat_whisker_left(k2, alpha, NULL, &dummy);
        T_STATUS(st, CAT89_INVALID);
    }
    cat89_nat_release(alpha);
    cat89_functor_release(f1);
    cat89_functor_release(f2);
    cat89_functor_release(h);
    cat89_functor_release(k2);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
    cat89_category_release(cat2);
}

int main(void)
{
    T_START();
    test_whiskers();
    return T_END() ? 0 : 1;
}
