/* cat89_test_fixtures.c - exhaustive law verification over finite fixtures.
 *
 * The exhaustive identity/associativity sweep now lives in the library
 * (cat89_check_category_exhaustive, W5); this test drives it over every
 * reference fixture and independently cross-checks the object/morphism
 * cardinalities. test_exhaustive.c additionally compares the library's law-case
 * count against an independently written loop. */
#include <stdlib.h>

#include "finite_categories.h"
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

static unsigned long count_objects(cat89_enum *enumeration)
{
    cat89_obj_iter *it = NULL;
    const cat89_obj *obj = NULL;
    int done = 0;
    unsigned long n = 0;

    if (cat89_obj_iter_open(enumeration, &it) != CAT89_OK)
    {
        return 0;
    }
    done = 0;
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

/* Count all morphisms (owned) and release them. */
static unsigned long count_morphisms(cat89_category *cat,
                                     cat89_enum *enumeration)
{
    cat89_mor_iter *it = NULL;
    cat89_mor *mor = NULL;
    int done = 0;
    unsigned long n = 0;

    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        return 0;
    }
    done = 0;
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

static void check_fixture(enum cat89_fixture_kind kind,
                          unsigned long expect_obj, unsigned long expect_mor)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_check_result res;
    unsigned long no;

    T_STATUS(cat89_fixture_build(kind, &cat, &eq, &enumeration), CAT89_OK);
    T_ASSERT(cat != NULL);

    no = count_objects(enumeration);
    T_EQ_UL(no, expect_obj);
    T_EQ_UL(count_morphisms(cat, enumeration), expect_mor);

    /* the exhaustive law sweep now lives in the library */
    res.checked = 0;
    res.failed = 0;
    T_STATUS(cat89_check_category_exhaustive(cat, eq, enumeration, &res),
             CAT89_OK);
    T_EQ_UL(res.failed, 0);
    T_ASSERT(res.checked > 0);

    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    check_fixture(CAT89_FIX_TERMINAL, 1, 1);
    check_fixture(CAT89_FIX_DISCRETE2, 2, 2);
    check_fixture(CAT89_FIX_ARROW, 2, 3);
    check_fixture(CAT89_FIX_COMP2, 3, 6);
    check_fixture(CAT89_FIX_C2_GROUPOID, 1, 2);
    return T_END() ? 0 : 1;
}
