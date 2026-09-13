/* test_env.c - environments: lookup, child shadowing, parent chain. */
#include "test.h"

static void run_lookup_present(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_scheme *mono;
    hm_scheme *found;
    hm_type *c;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    c = hm_type_const(ctx, "Int");
    mono = hm_scheme_mono(ctx, c);
    st = hm_env_bind(env, "x", mono);
    CHECK(st == HM_OK);
    found = hm_env_lookup(env, "x");
    CHECK(found == mono);
    hm_ctx_destroy(ctx);
}

static void run_lookup_absent(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_scheme *found;
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    found = hm_env_lookup(env, "nope");
    CHECK(found == NULL);
    hm_ctx_destroy(ctx);
}

static void run_child_shadow(void)
{
    hm_ctx *ctx;
    hm_env *parent;
    hm_env *child;
    hm_scheme *s1;
    hm_scheme *s2;
    hm_scheme *found;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    parent = hm_env_new(ctx);
    child = hm_env_child(ctx, parent);
    s1 = hm_scheme_mono(ctx, hm_type_const(ctx, "Int"));
    s2 = hm_scheme_mono(ctx, hm_type_const(ctx, "Bool"));
    st = hm_env_bind(parent, "x", s1);
    CHECK(st == HM_OK);
    st = hm_env_bind(child, "x", s2);
    CHECK(st == HM_OK);
    found = hm_env_lookup(child, "x");
    CHECK(found == s2);
    found = hm_env_lookup(parent, "x");
    CHECK(found == s1);
    hm_ctx_destroy(ctx);
}

static void run_parent_chain(void)
{
    hm_ctx *ctx;
    hm_env *root;
    hm_env *mid;
    hm_env *leaf;
    hm_scheme *s;
    hm_scheme *found;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    root = hm_env_new(ctx);
    mid = hm_env_child(ctx, root);
    leaf = hm_env_child(ctx, mid);
    s = hm_scheme_mono(ctx, hm_type_const(ctx, "Int"));
    st = hm_env_bind(root, "deep", s);
    CHECK(st == HM_OK);
    found = hm_env_lookup(leaf, "deep");
    CHECK(found == s);
    hm_ctx_destroy(ctx);
}

static void run_bind_name_copied(void)
{
    hm_ctx *ctx;
    hm_env *env;
    char name[8];
    hm_scheme *s;
    hm_scheme *found;
    hm_status st;
    ctx = hm_ctx_new(NULL);
    env = hm_env_new(ctx);
    s = hm_scheme_mono(ctx, hm_type_const(ctx, "Int"));
    name[0] = 'a';
    name[1] = 'b';
    name[2] = 'c';
    name[3] = '\0';
    st = hm_env_bind(env, name, s);
    CHECK(st == HM_OK);
    name[0] = 'z';
    found = hm_env_lookup(env, "abc");
    CHECK(found == s);
    hm_ctx_destroy(ctx);
}

int main(void)
{
    run_lookup_present();
    run_lookup_absent();
    run_child_shadow();
    run_parent_chain();
    run_bind_name_copied();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
