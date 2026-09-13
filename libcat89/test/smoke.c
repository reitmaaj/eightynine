/* cat89_smoke.c - one end-to-end path through the category core.
 *
 * Builds the instrumented mock category, constructs identities, checks
 * dom/cod, and composes g o f, then releases everything. */
#include <cat89/cat89.h>
#include <cat89_mock_backend.h>

int cat89_test_failures = 0;

int main(void)
{
    cat89_category *cat = NULL;
    cat89_mock *mock = NULL;
    cat89_mor *f = NULL;
    cat89_mor *g = NULL;
    cat89_mor *h = NULL;
    const cat89_obj *a = NULL;
    const cat89_obj *c = NULL;
    const cat89_obj *dom_h = NULL;
    const cat89_obj *cod_h = NULL;

    if (cat89_mock_new(NULL, &cat, &mock) != CAT89_OK)
    {
        return 1;
    }
    a = cat89_mock_obj(mock, CAT89_MOCK_A);
    c = cat89_mock_obj(mock, CAT89_MOCK_C);

    if (cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &f) != CAT89_OK)
    {
        return 1;
    }
    if (cat89_mock_mor(mock, CAT89_MOCK_B, CAT89_MOCK_C, &g) != CAT89_OK)
    {
        return 1;
    }

    if (cat89_compose(cat, g, f, &h) != CAT89_OK)
    {
        return 1;
    }
    if (h == NULL)
    {
        return 1;
    }

    cat89_dom(cat, h, &dom_h);
    cat89_cod(cat, h, &cod_h);
    if (!cat89_obj_same(cat, dom_h, a))
    {
        return 1;
    }
    if (!cat89_obj_same(cat, cod_h, c))
    {
        return 1;
    }

    cat89_mor_release(cat, f);
    cat89_mor_release(cat, g);
    cat89_mor_release(cat, h);
    cat89_category_release(cat);
    cat89_mock_free(mock);
    return 0;
}
