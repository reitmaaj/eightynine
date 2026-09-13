/* test_match.c - exhaustiveness and redundancy over closed families. */
#include "test.h"

static void build_option(adt_ctx *ctx, adt_type **type, adt_ctor **none,
                         adt_ctor **some)
{
    hm_type *a;
    hm_type *fields[1];
    *type = adt_type_new(ctx, "option", 1);
    a = adt_type_parameter(*type, 0);
    *none = adt_ctor_add(*type, "None", 0, NULL);
    fields[0] = a;
    *some = adt_ctor_add(*type, "Some", 1, fields);
    CHECK(adt_type_seal(*type) == ADT_OK);
}

static void run_flat(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *none;
    adt_ctor *some;
    adt_pattern *pats[2];
    adt_pattern *args[1];
    adt_pattern *wild;
    adt_match_result result;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    build_option(ctx, &option, &none, &some);
    wild = adt_pattern_wildcard(ctx);
    pats[0] = adt_pattern_ctor(ctx, none, 0, NULL);
    CHECK(adt_match_exhaustive(option, 1, pats, &result) == ADT_OK);
    CHECK(result == ADT_MATCH_NONEXHAUSTIVE);
    args[0] = wild;
    pats[1] = adt_pattern_ctor(ctx, some, 1, args);
    CHECK(adt_match_exhaustive(option, 2, pats, &result) == ADT_OK);
    CHECK(result == ADT_MATCH_EXHAUSTIVE);
    pats[0] = wild;
    CHECK(adt_match_exhaustive(option, 1, pats, &result) == ADT_OK);
    CHECK(result == ADT_MATCH_EXHAUSTIVE);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_wildcard_exhaustive(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *none;
    adt_ctor *some;
    adt_pattern *pats[2];
    adt_match_result result;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    build_option(ctx, &option, &none, &some);
    pats[0] = adt_pattern_ctor(ctx, none, 0, NULL);
    pats[1] = adt_pattern_wildcard(ctx);
    CHECK(adt_match_exhaustive(option, 2, pats, &result) == ADT_OK);
    CHECK(result == ADT_MATCH_EXHAUSTIVE);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_redundant(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *none;
    adt_ctor *some;
    adt_pattern *pats[2];
    unsigned char out[2];
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    build_option(ctx, &option, &none, &some);
    pats[0] = adt_pattern_wildcard(ctx);
    pats[1] = adt_pattern_ctor(ctx, none, 0, NULL);
    CHECK(adt_match_redundant(2, pats, out) == ADT_OK);
    CHECK(out[0] == 0);
    CHECK(out[1] == 1);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_redundant_ctor(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    adt_ctor *none;
    adt_ctor *some;
    adt_pattern *pats[2];
    adt_pattern *args[1];
    adt_pattern *wild;
    unsigned char out[2];
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    build_option(ctx, &option, &none, &some);
    wild = adt_pattern_wildcard(ctx);
    args[0] = wild;
    pats[0] = adt_pattern_ctor(ctx, some, 1, args);
    args[0] = wild;
    pats[1] = adt_pattern_ctor(ctx, some, 1, args);
    CHECK(adt_match_redundant(2, pats, out) == ADT_OK);
    CHECK(out[0] == 0);
    CHECK(out[1] == 1);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void build_bool(adt_ctx *ctx, adt_type **boolean, adt_ctor **f,
                       adt_ctor **t)
{
    *boolean = adt_type_new(ctx, "bool", 0);
    *f = adt_ctor_add(*boolean, "False", 0, NULL);
    *t = adt_ctor_add(*boolean, "True", 0, NULL);
    CHECK(adt_type_seal(*boolean) == ADT_OK);
}

static void build_pair(adt_ctx *ctx, adt_type *boolean, adt_type **pair,
                       adt_ctor **p)
{
    hm_type *bool_type;
    hm_type *fields[2];
    bool_type = adt_type_apply(boolean, 0, NULL);
    *pair = adt_type_new(ctx, "pair", 0);
    fields[0] = bool_type;
    fields[1] = bool_type;
    *p = adt_ctor_add(*pair, "P", 2, fields);
    CHECK(adt_type_seal(*pair) == ADT_OK);
}

static void run_nested_exhaustive(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *boolean;
    adt_type *pair;
    adt_ctor *f;
    adt_ctor *t;
    adt_ctor *p;
    adt_pattern *pats[4];
    adt_pattern *args[2];
    adt_pattern *pf;
    adt_pattern *pt;
    adt_pattern *wild;
    adt_match_result result;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    build_bool(ctx, &boolean, &f, &t);
    build_pair(ctx, boolean, &pair, &p);
    pf = adt_pattern_ctor(ctx, f, 0, NULL);
    pt = adt_pattern_ctor(ctx, t, 0, NULL);
    wild = adt_pattern_wildcard(ctx);
    args[0] = pf;
    args[1] = wild;
    pats[0] = adt_pattern_ctor(ctx, p, 2, args);
    args[0] = pt;
    args[1] = wild;
    pats[1] = adt_pattern_ctor(ctx, p, 2, args);
    CHECK(adt_match_exhaustive(pair, 2, pats, &result) == ADT_OK);
    CHECK(result == ADT_MATCH_EXHAUSTIVE);
    args[0] = pf;
    args[1] = pt;
    pats[0] = adt_pattern_ctor(ctx, p, 2, args);
    args[0] = pt;
    args[1] = pf;
    pats[1] = adt_pattern_ctor(ctx, p, 2, args);
    CHECK(adt_match_exhaustive(pair, 2, pats, &result) == ADT_OK);
    CHECK(result == ADT_MATCH_NONEXHAUSTIVE);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_nested_redundant(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *boolean;
    adt_type *pair;
    adt_ctor *f;
    adt_ctor *t;
    adt_ctor *p;
    adt_pattern *pats[2];
    adt_pattern *args[2];
    adt_pattern *pf;
    adt_pattern *pt;
    adt_pattern *wild;
    unsigned char out[2];
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    build_bool(ctx, &boolean, &f, &t);
    build_pair(ctx, boolean, &pair, &p);
    pf = adt_pattern_ctor(ctx, f, 0, NULL);
    pt = adt_pattern_ctor(ctx, t, 0, NULL);
    wild = adt_pattern_wildcard(ctx);
    args[0] = wild;
    args[1] = wild;
    pats[0] = adt_pattern_ctor(ctx, p, 2, args);
    args[0] = pf;
    args[1] = pt;
    pats[1] = adt_pattern_ctor(ctx, p, 2, args);
    CHECK(adt_match_redundant(2, pats, out) == ADT_OK);
    CHECK(out[0] == 0);
    CHECK(out[1] == 1);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

int main(void)
{
    run_flat();
    run_wildcard_exhaustive();
    run_redundant();
    run_redundant_ctor();
    run_nested_exhaustive();
    run_nested_redundant();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
