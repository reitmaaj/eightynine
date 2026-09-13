/* cat89_test_split_ops.c - split identity/composition over the C2 groupoid. */
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

static cat89_mor *grab_s(cat89_category *cat, cat89_eq *eq,
                         cat89_enum *enumeration, cat89_mor *e_handle)
{
    cat89_mor_iter *it = NULL;
    cat89_mor *mor = NULL;
    int done = 0;
    int equal = -1;
    cat89_mor *s = NULL;

    T_STATUS(cat89_mor_iter_open(enumeration, &it), CAT89_OK);
    done = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (done)
        {
            break;
        }
        T_STATUS(cat89_mor_equal(eq, e_handle, mor, &equal), CAT89_OK);
        if (!equal)
        {
            s = mor;
            break;
        }
        cat89_mor_release(cat, mor);
    }
    cat89_mor_iter_close(enumeration, it);
    return s;
}

static void test_split_ops(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    const cat89_obj *obj = NULL;
    cat89_mor *e = NULL;
    cat89_mor *s = NULL;
    cat89_split_mono *mono_ss = NULL;
    cat89_split_mono *mono_id = NULL;
    cat89_split_mono *mono_comp = NULL;
    cat89_split_epi *epi_id = NULL;
    int valid = -1;

    T_STATUS(
        cat89_fixture_build(CAT89_FIX_C2_GROUPOID, &cat, &eq, &enumeration),
        CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);
    T_STATUS(cat89_identity(cat, obj, &e), CAT89_OK);
    s = grab_s(cat, eq, enumeration, e);
    T_ASSERT(s != NULL);

    T_STATUS(cat89_split_mono_new(cat, s, s, NULL, &mono_ss), CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_split_mono(mono_ss, eq, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);

    /* composition: (s,s) o (s,s) has section s o s = e. */
    T_STATUS(cat89_split_mono_compose(mono_ss, mono_ss, NULL, &mono_comp),
             CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_split_mono(mono_comp, eq, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);

    /* identity split mono/epi are valid. */
    T_STATUS(cat89_split_mono_identity(cat, obj, NULL, &mono_id), CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_split_mono(mono_id, eq, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);

    T_STATUS(cat89_split_epi_identity(cat, obj, NULL, &epi_id), CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_split_epi(epi_id, eq, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);

    cat89_split_mono_release(mono_ss);
    cat89_split_mono_release(mono_comp);
    cat89_split_mono_release(mono_id);
    cat89_split_epi_release(epi_id);
    cat89_mor_release(cat, e);
    cat89_mor_release(cat, s);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_split_ops();
    return T_END() ? 0 : 1;
}
