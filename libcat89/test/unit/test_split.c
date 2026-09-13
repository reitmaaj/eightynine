/* cat89_test_split.c - split mono/epi and iso adapters over the C2 groupoid. */
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

static cat89_mor *find_nonidentity(cat89_category *cat, cat89_eq *eq,
                                   cat89_enum *enumeration, cat89_mor *e_handle)
{
    cat89_mor_iter *it = NULL;
    cat89_mor *mor = NULL;
    int done = 0;
    int equal = -1;
    cat89_mor *found = NULL;

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
            found = mor;
            break;
        }
        cat89_mor_release(cat, mor);
    }
    cat89_mor_iter_close(enumeration, it);
    return found;
}

static void test_split_over_c2(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    const cat89_obj *obj = NULL;
    cat89_mor *e_handle = NULL;
    cat89_mor *s_handle = NULL;
    cat89_split_mono *mono = NULL;
    cat89_split_epi *epi = NULL;
    cat89_iso *iso = NULL;
    cat89_mor *composite = NULL;
    int equal = -1;

    T_STATUS(
        cat89_fixture_build(CAT89_FIX_C2_GROUPOID, &cat, &eq, &enumeration),
        CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);
    T_STATUS(cat89_identity(cat, obj, &e_handle), CAT89_OK);
    s_handle = find_nonidentity(cat, eq, enumeration, e_handle);
    T_ASSERT(s_handle != NULL);

    /* split mono over (s, s): r o s = s o s = e holds. */
    T_STATUS(cat89_split_mono_new(cat, s_handle, s_handle, NULL, &mono),
             CAT89_OK);
    T_ASSERT(mono != NULL);
    T_ASSERT(cat89_split_mono_section(mono) == s_handle);
    T_ASSERT(cat89_split_mono_retraction(mono) == s_handle);
    T_ASSERT(cat89_split_mono_category(mono) == cat);

    composite = NULL;
    T_STATUS(cat89_compose(cat, cat89_split_mono_retraction(mono),
                           cat89_split_mono_section(mono), &composite),
             CAT89_OK);
    equal = -1;
    T_STATUS(cat89_mor_equal(eq, e_handle, composite, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    cat89_mor_release(cat, composite);

    /* split epi over the same pair. */
    T_STATUS(cat89_split_epi_new(cat, s_handle, s_handle, NULL, &epi),
             CAT89_OK);
    T_ASSERT(cat89_split_epi_retraction(epi) == s_handle);

    /* iso -> split adapters preserve the two morphisms. */
    T_STATUS(cat89_iso_new(cat, s_handle, s_handle, NULL, &iso), CAT89_OK);
    {
        cat89_split_mono *m2 = NULL;
        cat89_split_epi *e2 = NULL;
        T_STATUS(cat89_iso_as_split_mono(iso, NULL, &m2), CAT89_OK);
        T_STATUS(cat89_iso_as_split_epi(iso, NULL, &e2), CAT89_OK);
        T_ASSERT(cat89_split_mono_section(m2) == s_handle);
        T_ASSERT(cat89_split_epi_retraction(e2) == s_handle);
        cat89_split_mono_release(m2);
        cat89_split_epi_release(e2);
    }

    /* non-entanglement: releasing mono does not invalidate the iso. */
    cat89_split_mono_release(mono);
    cat89_split_epi_release(epi);
    T_ASSERT(cat89_iso_forward(iso) == s_handle);

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
    test_split_over_c2();
    return T_END() ? 0 : 1;
}
