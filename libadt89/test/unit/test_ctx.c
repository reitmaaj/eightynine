/* test_ctx.c - context lifecycle, allocator, errors, origin. */
#include "test.h"

static void run_lifecycle(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    CHECK(ctx != NULL);
    CHECK(adt_ctx_error(ctx) == NULL);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_null_args(void)
{
    hm_ctx *hm;
    hm = hm_ctx_new(NULL);
    CHECK(adt_ctx_new(NULL) == NULL);
    adt_ctx_set_origin(NULL, NULL);
    CHECK(adt_ctx_error(NULL) == NULL);
    hm_ctx_destroy(hm);
}

static void run_origin_capture(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    hm_type *args[2];
    const adt_error *err;
    int marker;
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    marker = 42;
    adt_ctx_set_origin(ctx, &marker);
    option = adt_type_new(ctx, "option", 1);
    CHECK(option != NULL);
    args[0] = hm_type_var(hm);
    args[1] = hm_type_var(hm);
    CHECK(adt_type_apply(option, 2, args) == NULL);
    err = adt_ctx_error(ctx);
    CHECK(err != NULL);
    if (err != NULL)
    {
        CHECK(adt_error_kind_of(err) == ADT_ERR_WRONG_TYPE_ARITY);
        CHECK(strcmp(adt_error_message(err), "wrong type-application arity") ==
              0);
        CHECK(adt_error_origin(err) == &marker);
    }
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

static void run_error_sticky(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    adt_type *option;
    hm_type *ok;
    hm_type *one[1];
    hm = hm_ctx_new(NULL);
    ctx = adt_ctx_new(hm);
    option = adt_type_new(ctx, "option", 1);
    CHECK(adt_type_apply(option, 2, NULL) == NULL);
    CHECK(adt_ctx_error(ctx) != NULL);
    one[0] = hm_type_var(hm);
    ok = adt_type_apply(option, 1, one);
    CHECK(ok != NULL);
    adt_ctx_destroy(ctx);
    hm_ctx_destroy(hm);
}

struct count_alloc
{
    size_t live;
    size_t peak;
};

static void *count_alloc_fn(void *userdata, size_t size)
{
    struct count_alloc *c;
    void *p;
    c = userdata;
    p = malloc(size);
    if (p != NULL)
    {
        c->live = c->live + 1;
        if (c->live > c->peak)
        {
            c->peak = c->live;
        }
    }
    return p;
}

static void count_free_fn(void *userdata, void *ptr)
{
    struct count_alloc *c;
    c = userdata;
    free(ptr);
    c->live = c->live - 1;
}

static void run_custom_allocator(void)
{
    hm_ctx *hm;
    adt_ctx *ctx;
    struct count_alloc ca;
    adt_allocator allocator;
    adt_type *option;
    hm_ctx_destroy(NULL);
    hm = hm_ctx_new(NULL);
    ca.live = 0;
    ca.peak = 0;
    allocator.alloc = count_alloc_fn;
    allocator.free = count_free_fn;
    allocator.userdata = &ca;
    ctx = adt_ctx_new_with_allocator(hm, &allocator);
    CHECK(ctx != NULL);
    if (ctx == NULL)
    {
        hm_ctx_destroy(hm);
        return;
    }
    option = adt_type_new(ctx, "option", 1);
    CHECK(option != NULL);
    CHECK(ca.live > 0);
    adt_ctx_destroy(ctx);
    CHECK(ca.live == 0);
    hm_ctx_destroy(hm);
}

int main(void)
{
    run_lifecycle();
    run_null_args();
    run_origin_capture();
    run_error_sticky();
    run_custom_allocator();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
