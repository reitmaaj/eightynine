/* cat89_test_exhaustive.c - library exhaustive category-law checker.
 *
 * cat89_check_category_exhaustive must verify left/right identity on every
 * morphism and associativity on every composable triple of an enumerated finite
 * ambient. Over the reference fixtures it must report zero failures and its
 * `checked` count must match an independently written exhaustive loop; a
 * missing eq/enum capability must yield CAT89_NOT_SUPPORTED, never silence. */
#include <stdlib.h>

#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

struct morrec
{
    cat89_mor *mor;
    const cat89_obj *dom;
    const cat89_obj *cod;
};

/* Independent reference: collect morphisms, count identity cases (2 each) and
 * composable triples. Returns expected law-case total via out. */
static void reference_count(cat89_category *cat, cat89_enum *en,
                            struct morrec **out_arr, unsigned long *out_n,
                            unsigned long *out_expected)
{
    cat89_mor_iter *it;
    cat89_mor **tmp;
    struct morrec *arr;
    unsigned long cap;
    unsigned long n;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    unsigned long expected;
    int done;

    tmp = NULL;
    it = NULL;
    cap = 64;
    n = 0;
    tmp = (cat89_mor **)malloc(cap * sizeof(cat89_mor *));
    cat89_mor_iter_open(en, &it);
    done = 0;
    while (!done)
    {
        cat89_mor *m = NULL;
        cat89_mor_iter_next(en, it, &m, &done);
        if (!done)
        {
            if (n == cap)
            {
                break;
            }
            tmp[n] = m;
            n = n + 1;
        }
    }
    cat89_mor_iter_close(en, it);

    arr = (struct morrec *)malloc(n * sizeof(struct morrec));
    for (i = 0; i < n; i = i + 1)
    {
        const cat89_obj *dm = NULL;
        const cat89_obj *cm = NULL;
        cat89_dom(cat, tmp[i], &dm);
        cat89_cod(cat, tmp[i], &cm);
        arr[i].mor = tmp[i];
        arr[i].dom = dm;
        arr[i].cod = cm;
    }
    free(tmp);

    expected = 2 * n;
    for (i = 0; i < n; i = i + 1)
    {
        for (j = 0; j < n; j = j + 1)
        {
            for (k = 0; k < n; k = k + 1)
            {
                if (!cat89_obj_same(cat, arr[k].cod, arr[j].dom))
                {
                    continue;
                }
                if (!cat89_obj_same(cat, arr[j].cod, arr[i].dom))
                {
                    continue;
                }
                expected = expected + 1;
            }
        }
    }

    *out_arr = arr;
    *out_n = n;
    *out_expected = expected;
}

static void check_fixture(enum cat89_fixture_kind kind)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    cat89_check_result res;
    struct morrec *arr;
    unsigned long n;
    unsigned long expected;
    unsigned long i;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    arr = NULL;
    T_STATUS(cat89_fixture_build(kind, &cat, &eq, &enumeration), CAT89_OK);

    res.checked = 0;
    res.failed = 0;
    T_STATUS(cat89_check_category_exhaustive(cat, eq, enumeration, &res),
             CAT89_OK);
    T_EQ_UL(res.failed, 0);
    T_ASSERT(res.checked > 0);

    reference_count(cat, enumeration, &arr, &n, &expected);
    T_EQ_UL(res.checked, expected);
    for (i = 0; i < n; i = i + 1)
    {
        cat89_mor_release(cat, arr[i].mor);
    }
    free(arr);

    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_not_supported(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    cat89_check_result res;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &cat, &eq, &enumeration),
             CAT89_OK);

    res.checked = 99;
    res.failed = 99;
    T_STATUS(cat89_check_category_exhaustive(cat, NULL, enumeration, &res),
             CAT89_NOT_SUPPORTED);
    T_STATUS(cat89_check_category_exhaustive(cat, eq, NULL, &res),
             CAT89_NOT_SUPPORTED);

    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    check_fixture(CAT89_FIX_TERMINAL);
    check_fixture(CAT89_FIX_DISCRETE2);
    check_fixture(CAT89_FIX_ARROW);
    check_fixture(CAT89_FIX_COMP2);
    check_fixture(CAT89_FIX_C2_GROUPOID);
    test_not_supported();
    return T_END() ? 0 : 1;
}
