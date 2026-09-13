/* cat89_test_shape.c - reusable finite diagram shape categories.
 *
 * Builds each finite shape, checks object/morphism cardinalities, and runs the
 * local identity/associativity laws exhaustively over every applicable case so
 * a shape can be trusted as a lawful diagram source. */
#include <stdlib.h>

#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

static unsigned long count_objects(cat89_enum *enumeration)
{
    cat89_obj_iter *it;
    const cat89_obj *obj;
    int done;
    unsigned long n;

    it = NULL;
    obj = NULL;
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

static unsigned long collect_morphs(cat89_category *cat,
                                    cat89_enum *enumeration, cat89_mor ***out)
{
    cat89_mor_iter *it;
    cat89_mor *mor;
    cat89_mor **arr;
    int done;
    unsigned long n;

    it = NULL;
    mor = NULL;
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
    if (n == 0)
    {
        *out = NULL;
        return 0;
    }

    arr = (cat89_mor **)malloc(n * sizeof(cat89_mor *));
    if (arr == NULL)
    {
        return 0;
    }
    it = NULL;
    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        free(arr);
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
            arr[n] = mor;
            n = n + 1;
        }
    }
    cat89_mor_iter_close(enumeration, it);
    (void)cat;
    *out = arr;
    return n;
}

static void release_all(cat89_category *cat, cat89_mor **arr, unsigned long n)
{
    unsigned long i;

    for (i = 0; i < n; i = i + 1)
    {
        cat89_mor_release(cat, arr[i]);
    }
    free(arr);
}

static int composable(cat89_category *cat, const cat89_mor *g,
                      const cat89_mor *f)
{
    const cat89_obj *codf;
    const cat89_obj *domg;

    cat89_cod(cat, f, &codf);
    cat89_dom(cat, g, &domg);
    return cat89_obj_same(cat, codf, domg) != 0;
}

static void check_shape(enum cat89_shape_kind kind, unsigned long expect_obj,
                        unsigned long expect_mor)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *enumeration;
    cat89_mor **arr;
    unsigned long n;
    unsigned long no;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    unsigned long assoc_cases;
    int valid;

    cat = NULL;
    eq = NULL;
    enumeration = NULL;
    T_STATUS(cat89_shape_category_new(kind, NULL, &cat, &eq, &enumeration),
             CAT89_OK);
    T_ASSERT(cat != NULL);

    no = count_objects(enumeration);
    T_EQ_UL(no, expect_obj);
    n = collect_morphs(cat, enumeration, &arr);
    T_EQ_UL(n, expect_mor);

    if (n > 0)
    {
        for (i = 0; i < n; i = i + 1)
        {
            valid = -1;
            T_STATUS(cat89_check_left_identity(cat, eq, arr[i], &valid),
                     CAT89_OK);
            T_EQ_UL(valid, 1);
            valid = -1;
            T_STATUS(cat89_check_right_identity(cat, eq, arr[i], &valid),
                     CAT89_OK);
            T_EQ_UL(valid, 1);
        }

        assoc_cases = 0;
        for (i = 0; i < n; i = i + 1)
        {
            for (j = 0; j < n; j = j + 1)
            {
                for (k = 0; k < n; k = k + 1)
                {
                    if (!composable(cat, arr[j], arr[k]))
                    {
                        continue;
                    }
                    if (!composable(cat, arr[i], arr[j]))
                    {
                        continue;
                    }
                    valid = -1;
                    T_STATUS(cat89_check_associativity(cat, eq, arr[i], arr[j],
                                                       arr[k], &valid),
                             CAT89_OK);
                    T_EQ_UL(valid, 1);
                    assoc_cases = assoc_cases + 1;
                }
            }
        }
        T_ASSERT(assoc_cases > 0);
        release_all(cat, arr, n);
    }

    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    check_shape(CAT89_SHAPE_EMPTY, 0, 0);
    check_shape(CAT89_SHAPE_DISCRETE2, 2, 2);
    check_shape(CAT89_SHAPE_PARALLEL, 2, 4);
    check_shape(CAT89_SHAPE_SPAN, 3, 5);
    check_shape(CAT89_SHAPE_COSPAN, 3, 5);
    return T_END() ? 0 : 1;
}
