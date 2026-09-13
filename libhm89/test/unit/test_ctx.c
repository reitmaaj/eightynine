/* test_ctx.c - context lifecycle, allocator, origin, and no-memory paths. */
#include "test.h"

#include <stdlib.h>

struct counting_alloc
{
    int allocs;
    int frees;
};

static void *counting_alloc(void *userdata, size_t size)
{
    struct counting_alloc *ca;
    ca = userdata;
    ++ca->allocs;
    return malloc(size);
}

static void counting_free(void *userdata, void *ptr)
{
    struct counting_alloc *ca;
    ca = userdata;
    ++ca->frees;
    free(ptr);
}

static void *failing_alloc(void *userdata, size_t size)
{
    (void)userdata;
    (void)size;
    return NULL;
}

static void plain_free(void *userdata, void *ptr)
{
    (void)userdata;
    free(ptr);
}

static void run_default_lifecycle(void)
{
    hm_ctx *ctx;
    hm_type *a;
    hm_type *b;
    ctx = hm_ctx_new(NULL);
    CHECK(ctx != NULL);
    a = hm_type_var(ctx);
    b = hm_type_var(ctx);
    CHECK(a != NULL);
    CHECK(b != NULL);
    CHECK(a != b);
    hm_ctx_destroy(ctx);
}

static void run_custom_lifecycle(void)
{
    struct counting_alloc ca;
    hm_allocator alloc;
    hm_ctx *ctx;
    hm_type *x;
    hm_type *y;
    int balanced;
    ca.allocs = 0;
    ca.frees = 0;
    alloc.alloc = counting_alloc;
    alloc.free = counting_free;
    alloc.userdata = &ca;
    ctx = hm_ctx_new(&alloc);
    CHECK(ctx != NULL);
    x = hm_type_var(ctx);
    y = hm_type_var(ctx);
    hm_type_app2(ctx, "Pair", x, y);
    hm_ctx_destroy(ctx);
    balanced = (ca.allocs == ca.frees);
    CHECK(balanced);
}

static void run_origin(void)
{
    hm_ctx *ctx;
    hm_error *error;
    hm_error_kind kind;
    void *origin;
    int value;
    hm_status st;
    hm_type *a;
    hm_type *arg;
    ctx = hm_ctx_new(NULL);
    value = 42;
    a = hm_type_var(ctx);
    hm_ctx_set_origin(ctx, &value);
    arg = hm_type_app1(ctx, "List", a);
    error = NULL;
    st = hm_unify(ctx, a, arg, &error);
    CHECK(st == HM_ERROR_OCCURS);
    CHECK(error != NULL);
    kind = hm_error_kind_of(error);
    CHECK(kind == HM_ERR_OCCURS);
    origin = hm_error_origin(error);
    CHECK(origin == &value);
    CHECK(hm_error_left(error) == a);
    hm_ctx_destroy(ctx);
}

static void run_nomem(void)
{
    hm_allocator alloc;
    hm_ctx *ctx;
    hm_type *a;
    hm_error *error;
    hm_status st;
    hm_type *arg;
    alloc.alloc = failing_alloc;
    alloc.free = plain_free;
    alloc.userdata = NULL;
    ctx = hm_ctx_new(&alloc);
    CHECK(ctx == NULL);
    ctx = hm_ctx_new(NULL);
    a = hm_type_var(ctx);
    arg = hm_type_app1(ctx, "List", a);
    error = NULL;
    st = hm_unify(ctx, a, arg, &error);
    CHECK(st == HM_ERROR_OCCURS);
    CHECK(error != NULL);
    hm_ctx_destroy(ctx);
}

int main(void)
{
    run_default_lifecycle();
    run_custom_lifecycle();
    run_origin();
    run_nomem();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
