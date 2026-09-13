/* cat89_test_find.c - finite terminal/initial object finders. */
#include "finite_categories.h"
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

static void check_unique_arrows(cat89_category *cat, cat89_enum *enumeration,
                                const cat89_obj *universal, int from_universal)
{
    cat89_obj_iter *it;
    const cat89_obj *obj;
    cat89_mor *mor;
    int done;
    cat89_status st;

    it = NULL;
    if (cat89_obj_iter_open(enumeration, &it) != CAT89_OK)
    {
        return;
    }
    done = 0;
    while (!done)
    {
        obj = NULL;
        cat89_obj_iter_next(enumeration, it, &obj, &done);
        if (done)
        {
            break;
        }
        mor = NULL;
        if (from_universal)
        {
            st = cat89_initial_arrow(cat, enumeration, universal, obj, &mor);
        }
        else
        {
            st = cat89_terminal_arrow(cat, enumeration, universal, obj, &mor);
        }
        T_STATUS(st, CAT89_OK);
        T_ASSERT(mor != NULL);
        cat89_mor_release(cat, mor);
    }
    cat89_obj_iter_close(enumeration, it);
}

static void run(enum cat89_fixture_kind kind, int has_term, int has_init)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *enumeration;
    const cat89_obj *term;
    const cat89_obj *init;
    cat89_status st;

    cat = NULL;
    eq = NULL;
    enumeration = NULL;
    T_STATUS(cat89_fixture_build(kind, &cat, &eq, &enumeration), CAT89_OK);

    term = NULL;
    st = cat89_find_terminal(cat, enumeration, &term);
    if (has_term)
    {
        T_STATUS(st, CAT89_OK);
        T_ASSERT(term != NULL);
        check_unique_arrows(cat, enumeration, term, 0);
    }
    else
    {
        T_STATUS(st, CAT89_NOT_FOUND);
    }

    init = NULL;
    st = cat89_find_initial(cat, enumeration, &init);
    if (has_init)
    {
        T_STATUS(st, CAT89_OK);
        T_ASSERT(init != NULL);
        check_unique_arrows(cat, enumeration, init, 1);
    }
    else
    {
        T_STATUS(st, CAT89_NOT_FOUND);
    }

    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    run(CAT89_FIX_ARROW, 1, 1);
    run(CAT89_FIX_COMP2, 1, 1);
    run(CAT89_FIX_DISCRETE2, 0, 0);
    run(CAT89_FIX_C2_GROUPOID, 0, 0);
    return T_END() ? 0 : 1;
}
