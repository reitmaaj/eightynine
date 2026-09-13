/* cat89_test_capability.c - eq/enum capability object lifecycle tests. */
#include <cat89/cat89.h>
#include <cat89_mock_backend.h>
#include <test.h>

int cat89_test_failures = 0;

struct cap_ctx
{
    unsigned long eq_destroyed;
    unsigned long enum_destroyed;
};

static void eq_destroy_fn(void *ctx)
{
    struct cap_ctx *c = ctx;
    c->eq_destroyed = c->eq_destroyed + 1;
}

static void enum_destroy_fn(void *ctx)
{
    struct cap_ctx *c = ctx;
    c->enum_destroyed = c->enum_destroyed + 1;
}

static cat89_status obj_equal_fn(void *ctx, const cat89_obj *a,
                                 const cat89_obj *b, int *out_equal)
{
    (void)ctx;
    *out_equal = a == b ? 1 : 0;
    return CAT89_OK;
}

static void test_eq_constructor_and_dispatch(void)
{
    cat89_category *cat = NULL;
    cat89_mock *mock = NULL;
    cat89_eq *eq = NULL;
    cat89_eq *eq2 = NULL;
    struct cap_ctx ctx;
    const cat89_obj *a;
    const cat89_obj *b;
    int equal = -1;

    ctx.eq_destroyed = 0;
    ctx.enum_destroyed = 0;

    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    a = cat89_mock_obj(mock, CAT89_MOCK_A);
    b = cat89_mock_obj(mock, CAT89_MOCK_B);

    {
        cat89_eq_ops ops;
        ops.obj_equal = obj_equal_fn;
        ops.mor_equal = NULL;
        ops.destroy = eq_destroy_fn;
        T_STATUS(cat89_eq_new(cat, &ops, &ctx, NULL, &eq), CAT89_OK);
    }
    T_ASSERT(eq != NULL);
    T_ASSERT(cat89_eq_category(eq) == cat);

    equal = -1;
    T_STATUS(cat89_obj_equal(eq, a, a, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    equal = -1;
    T_STATUS(cat89_obj_equal(eq, a, b, &equal), CAT89_OK);
    T_EQ_UL(equal, 0);

    /* mor_equal direction is unsupported -> NOT_SUPPORTED, out reset to 0. */
    {
        cat89_mor *mf = NULL;
        cat89_mor *mg = NULL;
        T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &mf),
                 CAT89_OK);
        T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &mg),
                 CAT89_OK);
        equal = -1;
        T_STATUS(cat89_mor_equal(eq, mf, mg, &equal), CAT89_NOT_SUPPORTED);
        T_EQ_UL(equal, 0);
        cat89_mor_release(cat, mf);
        cat89_mor_release(cat, mg);
    }

    /* NULL out params. */
    T_STATUS(cat89_obj_equal(eq, a, b, NULL), CAT89_INVALID);
    T_STATUS(cat89_obj_equal(NULL, a, b, &equal), CAT89_INVALID);

    T_STATUS(cat89_eq_new(NULL, NULL, NULL, NULL, &eq2), CAT89_INVALID);
    T_ASSERT(eq2 == NULL);
    T_STATUS(cat89_eq_new(cat, NULL, NULL, NULL, &eq2), CAT89_INVALID);
    T_ASSERT(eq2 == NULL);

    cat89_eq_release(eq);
    cat89_category_release(cat);
    T_EQ_UL(ctx.eq_destroyed, 1);
    T_EQ_UL(cat89_mock_destroy_calls(mock), 1);
    cat89_mock_free(mock);
}

static void test_enum_not_supported_and_lifecycle(void)
{
    cat89_category *cat = NULL;
    cat89_mock *mock = NULL;
    cat89_enum *enumeration = NULL;
    cat89_enum *enum2 = NULL;
    struct cap_ctx ctx;
    cat89_obj_iter *it = NULL;

    ctx.eq_destroyed = 0;
    ctx.enum_destroyed = 0;

    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);

    {
        cat89_enum_ops ops;
        ops.obj_iter_open = NULL;
        ops.obj_iter_next = NULL;
        ops.obj_iter_close = NULL;
        ops.mor_iter_open = NULL;
        ops.mor_iter_next = NULL;
        ops.mor_iter_close = NULL;
        ops.hom_iter_open = NULL;
        ops.destroy = enum_destroy_fn;
        T_STATUS(cat89_enum_new(cat, &ops, &ctx, NULL, &enumeration), CAT89_OK);
    }
    T_ASSERT(cat89_enum_category(enumeration) == cat);

    /* No object iterator exposed -> NOT_SUPPORTED. */
    T_STATUS(cat89_obj_iter_open(enumeration, &it), CAT89_NOT_SUPPORTED);
    T_ASSERT(it == NULL);
    T_STATUS(cat89_enum_new(cat, NULL, NULL, NULL, &enum2), CAT89_INVALID);
    T_ASSERT(enum2 == NULL);

    cat89_enum_release(enumeration);
    cat89_category_release(cat);
    T_EQ_UL(ctx.enum_destroyed, 1);
    T_EQ_UL(cat89_mock_destroy_calls(mock), 1);
    cat89_mock_free(mock);
}

int main(void)
{
    T_START();
    test_eq_constructor_and_dispatch();
    test_enum_not_supported_and_lifecycle();
    return T_END() ? 0 : 1;
}
