/* smoke.c - end-to-end smoke over the primitive path: build a polymorphic
 * identity, generalize it, instantiate it twice, apply it to Int and Bool,
 * and verify the combined result renders as Pair(Int,Bool). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hm.h>

struct capture
{
    char *mem;
    size_t used;
    size_t cap;
};

static int capture_write(void *userdata, const char *data, size_t size)
{
    struct capture *c;
    char *nm;
    size_t nc;
    size_t i;
    c = userdata;
    if (c->used + size + 1 > c->cap)
    {
        nc = c->used + size + 1;
        nm = realloc(c->mem, nc);
        if (nm == NULL)
        {
            return 0;
        }
        c->mem = nm;
        c->cap = nc;
    }
    for (i = 0; i < size; ++i)
    {
        c->mem[c->used + i] = data[i];
    }
    c->used = c->used + size;
    c->mem[c->used] = '\0';
    return 1;
}

static char *type_str(hm_type *type)
{
    struct capture c;
    c.mem = NULL;
    c.used = 0;
    c.cap = 0;
    hm_type_write(type, capture_write, &c);
    if (c.mem == NULL)
    {
        c.mem = malloc(1);
        if (c.mem != NULL)
        {
            c.mem[0] = '\0';
        }
    }
    return c.mem;
}

static int report_failure(const char *what)
{
    fprintf(stderr, "smoke failed: %s\n", what);
    return 1;
}

static int check_ok(hm_status st)
{
    if (st == HM_OK)
    {
        return 1;
    }
    return 0;
}

int main(void)
{
    hm_ctx *ctx;
    hm_env *env;
    hm_type *a;
    hm_type *id_val;
    hm_type *inst1;
    hm_type *inst2;
    hm_type *tr1;
    hm_type *tr2;
    hm_type *apply1;
    hm_type *apply2;
    hm_type *arg_int;
    hm_type *arg_bool;
    hm_type *p1;
    hm_type *p2;
    hm_type *pair;
    hm_scheme *poly;
    hm_status st;
    char *s;
    int match;
    ctx = hm_ctx_new(NULL);
    if (ctx == NULL)
    {
        return report_failure("ctx_new");
    }
    env = hm_env_new(ctx);
    if (env == NULL)
    {
        hm_ctx_destroy(ctx);
        return report_failure("env_new");
    }
    a = hm_type_var(ctx);
    id_val = hm_type_fun(ctx, a, a);
    poly = hm_generalize(ctx, env, id_val);
    if (poly == NULL)
    {
        hm_ctx_destroy(ctx);
        return report_failure("generalize");
    }
    arg_int = hm_type_const(ctx, "Int");
    arg_bool = hm_type_const(ctx, "Bool");
    inst1 = hm_instantiate(ctx, poly);
    inst2 = hm_instantiate(ctx, poly);
    if (inst1 == NULL || inst2 == NULL)
    {
        hm_ctx_destroy(ctx);
        return report_failure("instantiate");
    }
    tr1 = hm_type_var(ctx);
    apply1 = hm_type_fun(ctx, arg_int, tr1);
    st = hm_unify(ctx, inst1, apply1, NULL);
    if (check_ok(st) == 0)
    {
        hm_ctx_destroy(ctx);
        return report_failure("unify id on Int");
    }
    tr2 = hm_type_var(ctx);
    apply2 = hm_type_fun(ctx, arg_bool, tr2);
    st = hm_unify(ctx, inst2, apply2, NULL);
    if (check_ok(st) == 0)
    {
        hm_ctx_destroy(ctx);
        return report_failure("unify id on Bool");
    }
    p1 = hm_type_prune(tr1);
    p2 = hm_type_prune(tr2);
    pair = hm_type_app2(ctx, "Pair", p1, p2);
    s = type_str(pair);
    if (s == NULL)
    {
        hm_ctx_destroy(ctx);
        return report_failure("render pair");
    }
    match = (strcmp(s, "Pair(Int,Bool)") == 0);
    free(s);
    hm_ctx_destroy(ctx);
    if (match == 0)
    {
        return report_failure("expected Pair(Int,Bool)");
    }
    printf("smoke ok\n");
    return 0;
}
