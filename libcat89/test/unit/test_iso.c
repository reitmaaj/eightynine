/* cat89_test_iso.c - isomorphisms over the C2 groupoid (s is self-inverse). */
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

static void test_c2_iso_inverse_laws(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    const cat89_obj *obj = NULL;
    cat89_mor *e_handle = NULL;
    cat89_mor *s_handle = NULL;
    cat89_mor_iter *it = NULL;
    cat89_mor *mor = NULL;
    int done = 0;
    int equal = -1;
    cat89_iso *iso = NULL;
    cat89_iso *inverted = NULL;
    cat89_iso *unit = NULL;
    cat89_iso *two = NULL;
    cat89_mor *composite = NULL;

    T_STATUS(
        cat89_fixture_build(CAT89_FIX_C2_GROUPOID, &cat, &eq, &enumeration),
        CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);
    T_STATUS(cat89_identity(cat, obj, &e_handle), CAT89_OK);

    /* find the non-identity morphism s */
    it = NULL;
    done = 0;
    s_handle = NULL;
    T_STATUS(cat89_mor_iter_open(enumeration, &it), CAT89_OK);
    while (!done)
    {
        mor = NULL;
        T_STATUS(cat89_mor_iter_next(enumeration, it, &mor, &done), CAT89_OK);
        if (done)
        {
            break;
        }
        equal = -1;
        T_STATUS(cat89_mor_equal(eq, e_handle, mor, &equal), CAT89_OK);
        if (!equal)
        {
            s_handle = mor;
        }
        else
        {
            cat89_mor_release(cat, mor);
        }
    }
    cat89_mor_iter_close(enumeration, it);
    T_ASSERT(s_handle != NULL);

    /* s is its own inverse: iso(s, s). */
    T_STATUS(cat89_iso_new(cat, s_handle, s_handle, NULL, &iso), CAT89_OK);
    T_ASSERT(iso != NULL);
    T_ASSERT(cat89_iso_category(iso) == cat);
    T_ASSERT(cat89_iso_forward(iso) == s_handle);
    T_ASSERT(cat89_iso_inverse(iso) == s_handle);

    /* inverse laws: f o g = 1 and g o f = 1. */
    composite = NULL;
    T_STATUS(cat89_compose(cat, cat89_iso_forward(iso), cat89_iso_inverse(iso),
                           &composite),
             CAT89_OK);
    equal = -1;
    T_STATUS(cat89_mor_equal(eq, e_handle, composite, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    cat89_mor_release(cat, composite);

    /* invert twice restores the same two handles. */
    T_STATUS(cat89_iso_invert(iso, NULL, &inverted), CAT89_OK);
    T_ASSERT(cat89_iso_forward(inverted) == s_handle);
    T_ASSERT(cat89_iso_inverse(inverted) == s_handle);

    /* identity iso: forward is (equal to) the identity morphism. */
    T_STATUS(cat89_iso_identity(cat, obj, NULL, &unit), CAT89_OK);
    equal = -1;
    T_STATUS(cat89_mor_equal(eq, e_handle, cat89_iso_forward(unit), &equal),
             CAT89_OK);
    T_EQ_UL(equal, 1);

    /* iso o iso : forward = s o s = e. */
    T_STATUS(cat89_iso_compose(iso, iso, NULL, &two), CAT89_OK);
    equal = -1;
    T_STATUS(cat89_mor_equal(eq, e_handle, cat89_iso_forward(two), &equal),
             CAT89_OK);
    T_EQ_UL(equal, 1);

    /* non-entanglement: a second iso over the same handles stays valid. */
    {
        cat89_iso *other = NULL;
        T_STATUS(cat89_iso_new(cat, s_handle, s_handle, NULL, &other),
                 CAT89_OK);
        cat89_iso_release(other);
    }
    T_ASSERT(cat89_iso_forward(iso) == s_handle);

    cat89_iso_release(two);
    cat89_iso_release(unit);
    cat89_iso_release(inverted);
    cat89_iso_release(iso);
    cat89_mor_release(cat, e_handle);
    cat89_mor_release(cat, s_handle);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_c2_iso_inverse_laws();
    return T_END() ? 0 : 1;
}
