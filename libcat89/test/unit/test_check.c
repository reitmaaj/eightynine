/* cat89_test_check.c - local law checkers over the finite backend. */
#include <cat89/cat89.h>
#include <cat89_internal.h>
#include <test.h>

int cat89_test_failures = 0;

static const cat89_obj *sole_object(cat89_enum *enumeration)
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

/* Monoid on one object: e (identity), a, a o a = a. */
static void test_monoid_laws_hold(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id obj = 0;
    cat89_finite_mor_id e = 0;
    cat89_finite_mor_id a = 0;
    const cat89_obj *o = NULL;
    cat89_mor *e_handle = NULL;
    cat89_mor *mors[2];
    cat89_mor *a_handle = NULL;
    cat89_mor_iter *it = NULL;
    cat89_mor *mor = NULL;
    cat89_mor *first = NULL;
    cat89_mor *second = NULL;
    int done = 0;
    int equal = -1;
    int valid = -1;
    int idx;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    cat89_finite_add_object(b, &obj);
    cat89_finite_add_morphism(b, obj, obj, &e);
    cat89_finite_add_morphism(b, obj, obj, &a);
    cat89_finite_set_identity(b, obj, e);
    cat89_finite_set_composition(b, e, e, e);
    cat89_finite_set_composition(b, e, a, a);
    cat89_finite_set_composition(b, a, e, a);
    cat89_finite_set_composition(b, a, a, a);
    T_STATUS(cat89_finite_build_unchecked(b, &cat, &eq, &enumeration),
             CAT89_OK);
    cat89_finite_builder_release(b);

    o = sole_object(enumeration);
    T_ASSERT(o != NULL);
    T_STATUS(cat89_identity(cat, o, &e_handle), CAT89_OK);

    it = NULL;
    done = 0;
    first = NULL;
    second = NULL;
    T_STATUS(cat89_mor_iter_open(enumeration, &it), CAT89_OK);
    while (!done)
    {
        mor = NULL;
        T_STATUS(cat89_mor_iter_next(enumeration, it, &mor, &done), CAT89_OK);
        if (done)
        {
            break;
        }
        if (first == NULL)
        {
            first = mor;
        }
        else
        {
            second = mor;
        }
    }
    cat89_mor_iter_close(enumeration, it);
    T_ASSERT(first != NULL);
    T_ASSERT(second != NULL);

    T_STATUS(cat89_mor_equal(eq, e_handle, first, &equal), CAT89_OK);
    if (equal)
    {
        a_handle = second;
    }
    else
    {
        a_handle = first;
    }
    mors[0] = first;
    mors[1] = second;

    /* left / right identity hold on every morphism. */
    for (idx = 0; idx < 2; idx = idx + 1)
    {
        valid = -1;
        T_STATUS(cat89_check_left_identity(cat, eq, mors[idx], &valid),
                 CAT89_OK);
        T_EQ_UL(valid, 1);
        valid = -1;
        T_STATUS(cat89_check_right_identity(cat, eq, mors[idx], &valid),
                 CAT89_OK);
        T_EQ_UL(valid, 1);
    }

    /* associativity holds on (a, a, a). */
    valid = -1;
    T_STATUS(cat89_check_associativity(cat, eq, a_handle, a_handle, a_handle,
                                       &valid),
             CAT89_OK);
    T_EQ_UL(valid, 1);

    /* NULL handling. */
    valid = -1;
    T_STATUS(cat89_check_left_identity(NULL, eq, a_handle, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    T_STATUS(cat89_check_left_identity(cat, NULL, a_handle, &valid),
             CAT89_INVALID);
    T_STATUS(
        cat89_check_associativity(cat, eq, a_handle, a_handle, NULL, &valid),
        CAT89_INVALID);

    cat89_mor_release(cat, e_handle);
    cat89_mor_release(cat, first);
    cat89_mor_release(cat, second);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

/* A one-object category violating associativity: a o a = b, b o a = a,
 * a o b = b, so (a o a) o a = a but a o (a o a) = b. */
static void test_associativity_violation_detected(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id obj = 0;
    cat89_finite_mor_id e = 0;
    cat89_finite_mor_id a = 0;
    cat89_finite_mor_id bb = 0;
    const cat89_obj *o = NULL;
    cat89_mor *e_handle = NULL;
    cat89_mor_iter *it = NULL;
    cat89_mor *mor = NULL;
    cat89_mor *a_handle = NULL;
    cat89_mor *first = NULL;
    cat89_mor *second = NULL;
    cat89_mor *third = NULL;
    int done = 0;
    int equal = -1;
    int valid = -1;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    cat89_finite_add_object(b, &obj);
    cat89_finite_add_morphism(b, obj, obj, &e);
    cat89_finite_add_morphism(b, obj, obj, &a);
    cat89_finite_add_morphism(b, obj, obj, &bb);
    cat89_finite_set_identity(b, obj, e);
    cat89_finite_set_composition(b, e, e, e);
    cat89_finite_set_composition(b, e, a, a);
    cat89_finite_set_composition(b, e, bb, bb);
    cat89_finite_set_composition(b, a, e, a);
    cat89_finite_set_composition(b, bb, e, bb);
    cat89_finite_set_composition(b, a, a, bb);
    cat89_finite_set_composition(b, bb, a, a);
    cat89_finite_set_composition(b, a, bb, bb);
    cat89_finite_set_composition(b, bb, bb, bb);
    T_STATUS(cat89_finite_build_unchecked(b, &cat, &eq, &enumeration),
             CAT89_OK);
    cat89_finite_builder_release(b);

    o = sole_object(enumeration);
    T_ASSERT(o != NULL);
    T_STATUS(cat89_identity(cat, o, &e_handle), CAT89_OK);

    it = NULL;
    done = 0;
    first = NULL;
    second = NULL;
    third = NULL;
    T_STATUS(cat89_mor_iter_open(enumeration, &it), CAT89_OK);
    while (!done)
    {
        mor = NULL;
        T_STATUS(cat89_mor_iter_next(enumeration, it, &mor, &done), CAT89_OK);
        if (done)
        {
            break;
        }
        if (first == NULL)
        {
            first = mor;
        }
        else if (second == NULL)
        {
            second = mor;
        }
        else
        {
            third = mor;
        }
    }
    cat89_mor_iter_close(enumeration, it);
    T_ASSERT(first != NULL);
    T_ASSERT(second != NULL);
    T_ASSERT(third != NULL);

    /* Enumeration yields morphisms in index order: e, a, b. first is e. */
    T_STATUS(cat89_mor_equal(eq, e_handle, first, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    a_handle = second;

    valid = -1;
    T_STATUS(cat89_check_associativity(cat, eq, a_handle, a_handle, a_handle,
                                       &valid),
             CAT89_OK);
    T_EQ_UL(valid, 0);

    cat89_mor_release(cat, e_handle);
    cat89_mor_release(cat, first);
    cat89_mor_release(cat, second);
    cat89_mor_release(cat, third);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_monoid_laws_hold();
    test_associativity_violation_detected();
    return T_END() ? 0 : 1;
}
