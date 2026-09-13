/* cat89_test_check_cone.c - check_cone/check_cocone over identity diagrams. */
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

static cat89_status leg_identity(void *ctx, const cat89_obj *shape_obj,
                                 cat89_mor **out_mor)
{
    cat89_category *category = ctx;

    return cat89_identity(category, shape_obj, out_mor);
}

static void check_valid(int valid, const char *label)
{
    T_EQ_UL(valid, 1);
    (void)label;
}

static void test_cone_checks(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_functor *diagram = NULL;
    cat89_cone *cone = NULL;
    cat89_cocone *cocone = NULL;
    cat89_cone_ops ops;
    const cat89_obj *obj = NULL;
    int valid = -1;

    /* terminal fixture: only identity shape morphisms, cone is vacuous. */
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &cat, &eq, &enumeration),
             CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);
    T_STATUS(cat89_functor_identity(cat, NULL, &diagram), CAT89_OK);
    ops.leg = leg_identity;
    ops.destroy = NULL;
    T_STATUS(cat89_cone_new(diagram, obj, &ops, cat, NULL, &cone), CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_cone(cone, eq, enumeration, &valid), CAT89_OK);
    check_valid(valid, "terminal cone");
    T_STATUS(cat89_cocone_new(diagram, obj, &ops, cat, NULL, &cocone),
             CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_cocone(cocone, eq, enumeration, &valid), CAT89_OK);
    check_valid(valid, "terminal cocone");

    cat89_cone_release(cone);
    cat89_cocone_release(cocone);
    cat89_functor_release(diagram);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);

    /* C2: the non-identity s makes identity-leg cone fail commutativity. */
    cat = NULL;
    eq = NULL;
    enumeration = NULL;
    diagram = NULL;
    cone = NULL;
    T_STATUS(
        cat89_fixture_build(CAT89_FIX_C2_GROUPOID, &cat, &eq, &enumeration),
        CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);
    T_STATUS(cat89_functor_identity(cat, NULL, &diagram), CAT89_OK);
    T_STATUS(cat89_cone_new(diagram, obj, &ops, cat, NULL, &cone), CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_cone(cone, eq, enumeration, &valid), CAT89_OK);
    T_EQ_UL(valid, 0);

    cat89_cone_release(cone);
    cat89_functor_release(diagram);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_cone_checks();
    return T_END() ? 0 : 1;
}
