/* cat89_test_diagram.c - diagram is a functor (shape -> ambient category). */
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

static void test_diagram(void)
{
    cat89_category *cat = NULL;
    cat89_enum *enumeration = NULL;
    cat89_eq *eq = NULL;
    cat89_diagram *diagram = NULL;
    cat89_functor *as_functor = NULL;
    const cat89_obj *obj = NULL;
    const cat89_obj *mapped = NULL;
    cat89_status st;

    T_STATUS(cat89_fixture_build(CAT89_FIX_COMP2, &cat, &eq, &enumeration),
             CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);

    /* A diagram is a functor; build one with the shape as its source. */
    st = cat89_functor_identity(cat, NULL, &as_functor);
    T_STATUS(st, CAT89_OK);
    diagram = (cat89_diagram *)as_functor;

    T_ASSERT(cat89_diagram_source(diagram) == cat);
    T_ASSERT(cat89_diagram_target(diagram) == cat);

    mapped = NULL;
    T_STATUS(cat89_diagram_map_obj(diagram, obj, &mapped), CAT89_OK);
    T_ASSERT(mapped != NULL);
    T_ASSERT(cat89_obj_same(cat, obj, mapped));

    cat89_diagram_release(diagram);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_diagram();
    return T_END() ? 0 : 1;
}
