/* test_scheme.c - monomorphic/quantified schemes, instantiate, generalize. */
#include "test.h"

static int scheme_matches(const hm_scheme *scheme, const char *expected)
{
    char *s;
    int match;
    s = test_scheme_str(scheme);
    if (s == NULL)
    {
        return 0;
    }
    match = (strcmp(s, expected) == 0);
    free(s);
    return match;
}

#define CHECK_SCHEME(scheme, expected)                                         \
    do                                                                         \
    {                                                                          \
        if (scheme_matches((scheme), (expected)) == 0)                         \
        {                                                                      \
            fprintf(stderr, "scheme mismatch %s:%d: want [%s]\n", __FILE__,    \
                    __LINE__, (expected));                                     \
            ++test_failures;                                                   \
        }                                                                      \
    } while (0)

static void run_mono_basics(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *f;
    hm_scheme *m;
    hm_type *r1;
    hm_type *r2;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    f = hm_type_fun(ctx, a, a);
    m = hm_scheme_mono(ctx, f);
    CHECK(m != NULL);
    CHECK(hm_scheme_quantified_count(m) == 0);
    CHECK(hm_scheme_body(m) == f);
    r1 = hm_instantiate(ctx, m);
    r2 = hm_instantiate(ctx, m);
    CHECK(r1 != NULL);
    CHECK(r2 != NULL);
    CHECK(hm_type_equal(r1, f) == 1);
    CHECK(hm_type_equal(r1, r2) == 1);
    hm_ctx_destroy(ctx);
}

static void run_quant_freshness(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *f;
    hm_type *quant[1];
    hm_scheme *s;
    hm_type *r1;
    hm_type *r2;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    f = hm_type_fun(ctx, a, a);
    quant[0] = a;
    s = hm_scheme_new(ctx, 1, quant, f);
    CHECK(s != NULL);
    CHECK(hm_scheme_quantified_count(s) == 1);
    CHECK(hm_scheme_body(s) == f);
    r1 = hm_instantiate(ctx, s);
    r2 = hm_instantiate(ctx, s);
    CHECK(r1 != NULL);
    CHECK(r2 != NULL);
    CHECK(hm_type_equal(r1, r2) == 0);
    hm_ctx_destroy(ctx);
}

static void run_quant_rejects_duplicate(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *f;
    hm_type *quant[2];
    hm_scheme *s;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    f = hm_type_fun(ctx, a, a);
    quant[0] = a;
    quant[1] = a;
    s = hm_scheme_new(ctx, 2, quant, f);
    CHECK(s == NULL);
    hm_ctx_destroy(ctx);
}

static void run_quant_rejects_bound(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *c;
    hm_type *f;
    hm_type *quant[1];
    hm_scheme *s;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    c = hm_type_const(ctx, "Int");
    st = hm_unify(ctx, a, c, NULL);
    CHECK(st == HM_OK);
    f = hm_type_fun(ctx, a, a);
    quant[0] = a;
    s = hm_scheme_new(ctx, 1, quant, f);
    CHECK(s == NULL);
    hm_ctx_destroy(ctx);
}

static void run_generalize_empty(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *f;
    hm_env *env;
    hm_scheme *s;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    f = hm_type_fun(ctx, a, a);
    env = hm_env_new(ctx);
    s = hm_generalize(ctx, env, f);
    CHECK(s != NULL);
    CHECK(hm_scheme_quantified_count(s) == 1);
    CHECK_SCHEME(s, "forall 'a. ->('a,'a)");
    hm_ctx_destroy(ctx);
}

static void run_generalize_env_sensitive(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *b;
    hm_type *f;
    hm_env *env;
    hm_scheme *mono;
    hm_scheme *s;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    b = hm_type_var(ctx);
    f = hm_type_fun(ctx, a, b);
    env = hm_env_new(ctx);
    mono = hm_scheme_mono(ctx, a);
    st = hm_env_bind(env, "x", mono);
    CHECK(st == HM_OK);
    s = hm_generalize(ctx, env, f);
    CHECK(s != NULL);
    CHECK(hm_scheme_quantified_count(s) == 1);
    CHECK_SCHEME(s, "forall 'a. ->('b,'a)");
    hm_ctx_destroy(ctx);
}

static void run_generalize_all_bound(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *f;
    hm_env *env;
    hm_scheme *mono;
    hm_scheme *s;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    f = hm_type_fun(ctx, a, a);
    env = hm_env_new(ctx);
    mono = hm_scheme_mono(ctx, a);
    st = hm_env_bind(env, "x", mono);
    CHECK(st == HM_OK);
    s = hm_generalize(ctx, env, f);
    CHECK(s != NULL);
    CHECK(hm_scheme_quantified_count(s) == 0);
    CHECK_SCHEME(s, "->('a,'a)");
    hm_ctx_destroy(ctx);
}

int main(void)
{
    run_mono_basics();
    run_quant_freshness();
    run_quant_rejects_duplicate();
    run_quant_rejects_bound();
    run_generalize_empty();
    run_generalize_env_sensitive();
    run_generalize_all_bound();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
