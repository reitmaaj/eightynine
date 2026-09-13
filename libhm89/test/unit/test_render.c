/* test_render.c - deterministic rendering and quantifier naming. */
#include <stdlib.h>

#include "test.h"

static void run_type_deterministic(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *b;
    hm_type *ty;
    char *s1;
    char *s2;
    int same;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    b = hm_type_var(ctx);
    ty = hm_type_fun(ctx, a, hm_type_app2(ctx, "Pair", b, a));
    s1 = test_type_str(ty);
    s2 = test_type_str(ty);
    CHECK(s1 != NULL);
    CHECK(s2 != NULL);
    if (s1 != NULL && s2 != NULL)
    {
        same = (strcmp(s1, s2) == 0);
        CHECK(same == 1);
        CHECK(strcmp(s1, "->('a,Pair('b,'a))") == 0);
    }
    free(s1);
    free(s2);
    hm_ctx_destroy(ctx);
}

static void run_scheme_quant_text(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *b;
    hm_type *q[2];
    hm_type *body;
    hm_scheme *s;
    char *text;
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    b = hm_type_var(ctx);
    q[0] = a;
    q[1] = b;
    body = hm_type_fun(ctx, a, hm_type_app2(ctx, "Pair", b, a));
    s = hm_scheme_new(ctx, 2, q, body);
    CHECK(s != NULL);
    text = test_scheme_str(s);
    CHECK(text != NULL);
    if (text != NULL)
    {
        CHECK(strcmp(text, "forall 'a 'b. ->('a,Pair('b,'a))") == 0);
    }
    free(text);
    hm_ctx_destroy(ctx);
}

static void run_many_vars(void)
{
    hm_ctx *ctx;
    enum
    {
        K = 60
    };
    hm_type *vs[K];
    hm_type *acc;
    char *s;
    int i;
    int found_g1;
    ctx = hm_ctx_new(NULL);
    for (i = 0; i < K; ++i)
    {
        vs[i] = hm_type_var(ctx);
    }
    acc = vs[K - 1];
    for (i = K - 2; i >= 0; --i)
    {
        acc = hm_type_fun(ctx, vs[i], acc);
    }
    s = test_type_str(acc);
    CHECK(s != NULL);
    if (s != NULL)
    {
        found_g1 = (strstr(s, "'a1") != NULL);
        CHECK(found_g1 == 1);
    }
    free(s);
    hm_ctx_destroy(ctx);
}

int main(void)
{
    run_type_deterministic();
    run_scheme_quant_text();
    run_many_vars();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
