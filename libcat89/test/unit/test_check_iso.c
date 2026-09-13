/* cat89_test_check_iso.c - iso/split witness checkers over the C2 groupoid. */
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

static void test_iso_split_checks(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    const cat89_obj *obj = NULL;
    cat89_mor *e = NULL;
    cat89_mor *s = NULL;
    cat89_mor_iter *it = NULL;
    cat89_mor *mor = NULL;
    int done = 0;
    int equal = -1;
    int valid = -1;
    cat89_iso *iso_good = NULL;
    cat89_iso *iso_bad = NULL;
    cat89_split_mono *mono_good = NULL;
    cat89_split_mono *mono_bad = NULL;

    T_STATUS(
        cat89_fixture_build(CAT89_FIX_C2_GROUPOID, &cat, &eq, &enumeration),
        CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);
    T_STATUS(cat89_identity(cat, obj, &e), CAT89_OK);

    it = NULL;
    done = 0;
    s = NULL;
    T_STATUS(cat89_mor_iter_open(enumeration, &it), CAT89_OK);
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (done)
        {
            break;
        }
        T_STATUS(cat89_mor_equal(eq, e, mor, &equal), CAT89_OK);
        if (!equal)
        {
            s = mor;
        }
        else
        {
            cat89_mor_release(cat, mor);
        }
    }
    cat89_mor_iter_close(enumeration, it);
    T_ASSERT(s != NULL);

    /* iso(s,s) is valid; iso(s,e) is not. */
    T_STATUS(cat89_iso_new(cat, s, s, NULL, &iso_good), CAT89_OK);
    T_STATUS(cat89_iso_new(cat, s, e, NULL, &iso_bad), CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_iso(iso_good, eq, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);
    valid = -1;
    T_STATUS(cat89_check_iso(iso_bad, eq, &valid), CAT89_OK);
    T_EQ_UL(valid, 0);

    /* split mono: (s, s) valid; (s, e) invalid. */
    T_STATUS(cat89_split_mono_new(cat, s, s, NULL, &mono_good), CAT89_OK);
    T_STATUS(cat89_split_mono_new(cat, s, e, NULL, &mono_bad), CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_split_mono(mono_good, eq, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);
    valid = -1;
    T_STATUS(cat89_check_split_mono(mono_bad, eq, &valid), CAT89_OK);
    T_EQ_UL(valid, 0);

    /* NULL handling. */
    valid = -1;
    T_STATUS(cat89_check_iso(NULL, eq, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    T_STATUS(cat89_check_iso(iso_good, NULL, &valid), CAT89_INVALID);
    T_STATUS(cat89_check_split_mono(mono_good, NULL, &valid), CAT89_INVALID);

    cat89_iso_release(iso_good);
    cat89_iso_release(iso_bad);
    cat89_split_mono_release(mono_good);
    cat89_split_mono_release(mono_bad);
    cat89_mor_release(cat, e);
    cat89_mor_release(cat, s);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_iso_split_checks();
    return T_END() ? 0 : 1;
}
