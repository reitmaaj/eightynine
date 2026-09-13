/* test_atom_param.c - parameterized effect atoms and the atom domain. */
#include <stdlib.h>

#include "fx_fixture.h"

struct tparam
{
    long value;
};

static unsigned long destroys;

static int tp_compare(void *ud, fx_kind_id kind, const void *a, const void *b)
{
    const struct tparam *pa;
    const struct tparam *pb;
    (void)ud;
    (void)kind;
    pa = (const struct tparam *)a;
    pb = (const struct tparam *)b;
    if (pa->value < pb->value)
    {
        return -1;
    }
    if (pa->value > pb->value)
    {
        return 1;
    }
    return 0;
}

static unsigned long tp_hash(void *ud, fx_kind_id kind, const void *value)
{
    const struct tparam *p;
    (void)ud;
    (void)kind;
    p = (const struct tparam *)value;
    return (unsigned long)p->value;
}

static fx_status tp_copy(void *ud, fx_kind_id kind, const void *value,
                         void **out)
{
    const struct tparam *src;
    struct tparam *dst;
    (void)ud;
    (void)kind;
    src = (const struct tparam *)value;
    dst = (struct tparam *)malloc(sizeof(struct tparam));
    if (dst == NULL)
    {
        return FX_ERR_NOMEM;
    }
    dst->value = src->value;
    *out = dst;
    return FX_OK;
}

static void tp_destroy(void *ud, fx_kind_id kind, void *value)
{
    (void)ud;
    (void)kind;
    free(value);
    destroys = destroys + 1u;
}

static const fx_atom_domain domain = {tp_compare, tp_hash, tp_copy, tp_destroy};

static int make_param(fx_ctx *ctx, fx_kind_id kind, long v, const fx_atom **out)
{
    struct tparam p;
    fx_status st;
    p.value = v;
    st = fx_atom_parameterized(ctx, kind, &p, out);
    return st == FX_OK;
}

static void run_install(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    const fx_atom *a;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "K", 0, &ka);
    CHECK(fx_ctx_set_atom_domain(ctx, &domain, 0) == FX_OK);
    CHECK(make_param(ctx, ka, 10, &a));
    fx_ctx_free(ctx);
}

static void run_equal_params(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    const fx_atom *a;
    const fx_atom *b;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "K", 0, &ka);
    fx_ctx_set_atom_domain(ctx, &domain, 0);
    CHECK(make_param(ctx, ka, 10, &a));
    CHECK(make_param(ctx, ka, 10, &b));
    CHECK(fx_atom_equal(ctx, a, b));
    CHECK(fx_atom_is_parameterized(a));
    fx_ctx_free(ctx);
}

static void run_unequal_params(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    const fx_atom *a;
    const fx_atom *b;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "K", 0, &ka);
    fx_ctx_set_atom_domain(ctx, &domain, 0);
    CHECK(make_param(ctx, ka, 10, &a));
    CHECK(make_param(ctx, ka, 11, &b));
    CHECK(!fx_atom_equal(ctx, a, b));
    fx_ctx_free(ctx);
}

static void run_kind_matters(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    fx_kind_id kb;
    const fx_atom *a;
    const fx_atom *b;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "K1", 0, &ka);
    fx_kind_define(ctx, "K2", 0, &kb);
    fx_ctx_set_atom_domain(ctx, &domain, 0);
    CHECK(make_param(ctx, ka, 10, &a));
    CHECK(make_param(ctx, kb, 10, &b));
    CHECK(!fx_atom_equal(ctx, a, b));
    fx_ctx_free(ctx);
}

static void run_copy_isolation(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    struct tparam p;
    const fx_atom *a;
    const struct tparam *got;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "K", 0, &ka);
    fx_ctx_set_atom_domain(ctx, &domain, 0);
    p.value = 7;
    CHECK(fx_atom_parameterized(ctx, ka, &p, &a) == FX_OK);
    p.value = 999;
    got = (const struct tparam *)fx_atom_parameter(a);
    CHECK(got != NULL);
    if (got != NULL)
    {
        CHECK(got->value == 7);
    }
    fx_ctx_free(ctx);
}

static void run_destroy_accounting(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    long i;
    destroys = 0u;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "K", 0, &ka);
    fx_ctx_set_atom_domain(ctx, &domain, 0);
    for (i = 0; i < 5; ++i)
    {
        const fx_atom *a;
        make_param(ctx, ka, 100 + i, &a);
    }
    fx_ctx_free(ctx);
    CHECK(destroys == 5u);
}

static void run_domain_change_rejected(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    const fx_atom *a;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "K", 0, &ka);
    fx_ctx_set_atom_domain(ctx, &domain, 0);
    CHECK(make_param(ctx, ka, 1, &a));
    CHECK(fx_ctx_set_atom_domain(ctx, &domain, 0) == FX_ERR_INVALID);
    fx_ctx_free(ctx);
}

static void run_missing_domain(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "K", 0, &ka);
    CHECK(fx_atom_parameterized(ctx, ka, 0, 0) == FX_ERR_INVALID);
    fx_ctx_free(ctx);
}

static void run_canonical_order(void)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    const fx_atom *a3;
    const fx_atom *a1;
    const fx_atom *a2;
    const fx_atom *atoms[3];
    const fx_row *row;
    const struct tparam *p0;
    const struct tparam *p1;
    const struct tparam *p2;
    fx_ctx_new(&ctx);
    fx_kind_define(ctx, "K", 0, &ka);
    fx_ctx_set_atom_domain(ctx, &domain, 0);
    CHECK(make_param(ctx, ka, 3, &a3));
    CHECK(make_param(ctx, ka, 1, &a1));
    CHECK(make_param(ctx, ka, 2, &a2));
    atoms[0] = a3;
    atoms[1] = a1;
    atoms[2] = a2;
    CHECK(fx_row_closed(ctx, atoms, 3u, &row) == FX_OK);
    CHECK(fx_row_atom_count(ctx, row) == 3u);
    p0 = (const struct tparam *)fx_atom_parameter(fx_row_atom_at(ctx, row, 0));
    p1 = (const struct tparam *)fx_atom_parameter(fx_row_atom_at(ctx, row, 1));
    p2 = (const struct tparam *)fx_atom_parameter(fx_row_atom_at(ctx, row, 2));
    if (p0 != NULL && p1 != NULL && p2 != NULL)
    {
        CHECK(p0->value == 1);
        CHECK(p1->value == 2);
        CHECK(p2->value == 3);
    }
    fx_ctx_free(ctx);
}

int main(void)
{
    run_install();
    run_equal_params();
    run_unequal_params();
    run_kind_matters();
    run_copy_isolation();
    run_destroy_accounting();
    run_domain_change_rejected();
    run_missing_domain();
    run_canonical_order();
    TEST_END
}
