/* cat89_test_universal_fixture.c - validate the purpose-built finite ambients
 * used to exercise universal-construction finders.
 *
 * CAT89_FIX_BINPROD/BINCOPROD are tiny non-thin categories chosen so that the
 * product apex P = A x B and coproduct apex S = A + B are unambiguous and a
 * decoy object X is NOT the universal apex. This test locks their topology
 * (object/morphism cardinalities) and the specific composition equations the
 * finder and its factor rely on, so later finder tests can trust them. */
#include <stdlib.h>

#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

static unsigned long count_objects(cat89_enum *enumeration)
{
    cat89_obj_iter *it;
    const cat89_obj *obj;
    int done;
    unsigned long n;

    it = NULL;
    if (cat89_obj_iter_open(enumeration, &it) != CAT89_OK)
    {
        return 0;
    }
    done = 0;
    n = 0;
    while (!done)
    {
        obj = NULL;
        cat89_obj_iter_next(enumeration, it, &obj, &done);
        if (!done)
        {
            n = n + 1;
        }
    }
    cat89_obj_iter_close(enumeration, it);
    return n;
}

static unsigned long count_morphs(cat89_category *cat, cat89_enum *enumeration)
{
    cat89_mor_iter *it;
    cat89_mor *mor;
    int done;
    unsigned long n;

    it = NULL;
    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        return 0;
    }
    done = 0;
    n = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (!done)
        {
            cat89_mor_release(cat, mor);
            n = n + 1;
        }
    }
    cat89_mor_iter_close(enumeration, it);
    return n;
}

static void check_compose(enum cat89_fixture_kind kind,
                          unsigned long expect_obj, unsigned long expect_mor,
                          unsigned long g, unsigned long f,
                          unsigned long expect_result)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *enumeration;
    cat89_mor_iter *it;
    cat89_mor **arr;
    cat89_mor *composed;
    cat89_mor *mor;
    int done;
    int equal;
    unsigned long i;
    unsigned long n;

    cat = NULL;
    eq = NULL;
    enumeration = NULL;
    arr = NULL;
    T_STATUS(cat89_fixture_build(kind, &cat, &eq, &enumeration), CAT89_OK);
    T_ASSERT(cat != NULL);

    T_EQ_UL(count_objects(enumeration), expect_obj);
    T_EQ_UL(count_morphs(cat, enumeration), expect_mor);

    arr = (cat89_mor **)malloc(expect_mor * sizeof(cat89_mor *));
    T_ASSERT(arr != NULL);
    it = NULL;
    T_STATUS(cat89_mor_iter_open(enumeration, &it), CAT89_OK);
    done = 0;
    n = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (!done)
        {
            arr[n] = mor;
            n = n + 1;
        }
    }
    cat89_mor_iter_close(enumeration, it);

    composed = NULL;
    T_STATUS(cat89_compose(cat, arr[g], arr[f], &composed), CAT89_OK);
    T_ASSERT(composed != NULL);
    equal = -1;
    T_STATUS(cat89_mor_equal(eq, composed, arr[expect_result], &equal),
             CAT89_OK);
    T_EQ_UL(equal, 1);

    cat89_mor_release(cat, composed);
    for (i = 0; i < n; i = i + 1)
    {
        cat89_mor_release(cat, arr[i]);
    }
    free(arr);

    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    check_compose(CAT89_FIX_BINPROD, 4, 9, 4, 8, 6);
    check_compose(CAT89_FIX_BINPROD, 4, 9, 5, 8, 7);
    check_compose(CAT89_FIX_BINCOPROD, 4, 9, 8, 4, 6);
    check_compose(CAT89_FIX_BINCOPROD, 4, 9, 8, 5, 7);
    check_compose(CAT89_FIX_EQUALIZER, 4, 11, 5, 4, 9);
    check_compose(CAT89_FIX_EQUALIZER, 4, 11, 4, 8, 7);
    check_compose(CAT89_FIX_COEQUALIZER, 4, 11, 10, 6, 8);
    check_compose(CAT89_FIX_PULLBACK, 5, 14, 8, 5, 7);
    check_compose(CAT89_FIX_PULLBACK, 5, 14, 5, 13, 10);
    check_compose(CAT89_FIX_PUSHOUT, 5, 14, 7, 5, 9);
    check_compose(CAT89_FIX_PUSHOUT, 5, 14, 13, 7, 10);
    return T_END() ? 0 : 1;
}
