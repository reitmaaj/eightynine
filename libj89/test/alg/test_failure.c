/* test_failure.c - allocator and callback failure injection for j89_alg.
 *
 * Confirms that an allocation failure or a callback failure propagates as an
 * operational error and never yields a partial success. Run under ASan this
 * also verifies that every partial result is released (no leaks).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "j89_alg.h"

static int failures;

static void fail(const char *label)
{
    fprintf(stderr, "FAIL: %s\n", label);
    failures = failures + 1;
}

/* ---- deterministic fail-on-N allocator ------------------------------------
 */

typedef struct fail_alloc
{
    j89a_alloc al;
    unsigned long count;
    unsigned long fail_at;
} fail_alloc;

static void *fa_alloc(void *c, size_t n)
{
    fail_alloc *f;
    f = (fail_alloc *)c;
    f->count = f->count + 1;
    if (f->fail_at != 0 && f->count == f->fail_at)
    {
        return NULL;
    }
    return malloc(n);
}

static void *fa_realloc(void *c, void *p, size_t n)
{
    (void)c;
    return realloc(p, n);
}

static void fa_free(void *c, void *p)
{
    (void)c;
    free(p);
}

static void fail_alloc_init(fail_alloc *f, unsigned long at)
{
    f->al.alloc = fa_alloc;
    f->al.realloc = fa_realloc;
    f->al.free = fa_free;
    f->al.ctx = f;
    f->al.failed = 0;
    f->count = 0;
    f->fail_at = at;
}

/* ---- builders --------------------------------------------------------------
 */

static j89a_json *wide_array(unsigned n, j89a_alloc *al)
{
    j89a_list *xs;
    unsigned long i;
    j89a_json *v;
    xs = j89a_list_nil(al);
    i = n;
    while (i > 0)
    {
        i = i - 1;
        v = j89a_json_number((double)i, al);
        {
            j89a_list *nx;
            nx = j89a_list_cons(v, xs, al);
            j89a_json_release(v);
            j89a_list_release(xs);
            xs = nx;
        }
    }
    {
        j89a_json *a;
        a = j89a_json_array(xs, al);
        j89a_list_release(xs);
        return a;
    }
}

static j89a_object *dup_object(j89a_alloc *al)
{
    j89a_object *acc;
    j89a_object *m;
    j89a_str *k;
    j89a_json *v;
    int i;
    acc = j89a_object_empty(al);
    for (i = 0; i < 6; i = i + 1)
    {
        k = j89a_str_new("k", 1, al);
        v = j89a_json_number((double)i, al);
        m = j89a_object_member(k, v, al);
        j89a_str_release(k);
        j89a_json_release(v);
        {
            j89a_object *u;
            u = j89a_object_union(acc, m, al);
            j89a_object_release(acc);
            j89a_object_release(m);
            acc = u;
        }
    }
    return acc;
}

/* ---- pure maps -------------------------------------------------------------
 */

static double num_id2(void *c, double n)
{
    (void)c;
    return n;
}

static j89a_str *str_id2(void *c, const j89a_str *s)
{
    j89a_alloc al;
    j89a_str *copy;
    (void)c;
    j89a_alloc_init(&al);
    copy = j89a_str_new(j89a_str_data(s), j89a_str_len(s), &al);
    return copy;
}

/* ---- count monoid callbacks (R = unsigned long) ----------------------------
 */

static j89a_status m_num(void *c, double n, void *out)
{
    (void)c;
    (void)n;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static j89a_status m_null(void *c, void *out)
{
    (void)c;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static j89a_status m_bool(void *c, j89a_bool b, void *out)
{
    (void)c;
    (void)b;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static j89a_status m_str(void *c, const j89a_str *s, void *out)
{
    (void)c;
    (void)s;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static j89a_status m_rsum(void *c, const void *e, const void *a, void *out)
{
    (void)c;
    *(unsigned long *)out =
        *(const unsigned long *)e + *(const unsigned long *)a;
    return J89A_OK;
}

static j89a_status m_rcopy(void *c, const j89a_str *name, const void *value,
                           void *out)
{
    (void)c;
    (void)name;
    *(unsigned long *)out = *(const unsigned long *)value;
    return J89A_OK;
}

static j89a_status m_arr(void *c, const j89a_rlist *children, void *out)
{
    j89a_rtype rt;
    j89a_alloc al;
    unsigned long zero;
    unsigned long sum;
    (void)c;
    j89a_alloc_init(&al);
    zero = 0;
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_rlist_fold(children, &rt, &zero, m_rsum, NULL, &sum, &al) !=
        J89A_OK)
    {
        return J89A_CALLBACK;
    }
    *(unsigned long *)out = sum + 1;
    return J89A_OK;
}

static j89a_status m_obj(void *c, const j89a_robject *members, void *out)
{
    j89a_rtype rt;
    j89a_alloc al;
    unsigned long zero;
    unsigned long sum;
    (void)c;
    j89a_alloc_init(&al);
    zero = 0;
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_robject_fold(members, &rt, &zero, m_rcopy, m_rsum, NULL, &sum,
                          &al) != J89A_OK)
    {
        return J89A_CALLBACK;
    }
    *(unsigned long *)out = sum + 1;
    return J89A_OK;
}

static void fill_algebra(j89a_algebra *alg)
{
    alg->ctx = NULL;
    alg->null_case = m_null;
    alg->boolean_case = m_bool;
    alg->number_case = m_num;
    alg->string_case = m_str;
    alg->array_case = m_arr;
    alg->object_case = m_obj;
}

static j89a_status fold_count(const j89a_json *tree, unsigned long at,
                              unsigned long *result)
{
    j89a_rtype rt;
    j89a_algebra alg;
    fail_alloc f;
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    alg.rt = &rt;
    fill_algebra(&alg);
    fail_alloc_init(&f, at);
    return j89a_json_fold(tree, &alg, result, &f.al);
}

static void test_map_alloc_failure(void)
{
    j89a_alloc al;
    j89a_json *tree;
    unsigned long k;
    int any_fail;
    int any_success;

    j89a_alloc_init(&al);
    tree = wide_array(24, &al);
    any_fail = 0;
    any_success = 0;
    for (k = 1; k <= 200; k = k + 1)
    {
        fail_alloc f;
        j89a_json *res;
        fail_alloc_init(&f, k);
        res = j89a_json_map(tree, num_id2, str_id2, NULL, &f.al);
        if (res == NULL)
        {
            any_fail = 1;
        }
        else
        {
            any_success = 1;
            j89a_json_release(res);
        }
    }
    if (!any_fail)
    {
        fail("map alloc sweep never failed (suspicious)");
    }
    if (!any_success)
    {
        fail("map alloc sweep never succeeded (suspicious)");
    }
    j89a_json_release(tree);
}

static void test_fold_alloc_failure(void)
{
    j89a_alloc al;
    j89a_json *tree;
    unsigned long k;
    int any_fail;
    int any_success;
    unsigned long result;

    j89a_alloc_init(&al);
    tree = wide_array(16, &al);
    any_fail = 0;
    any_success = 0;
    for (k = 1; k <= 200; k = k + 1)
    {
        if (fold_count(tree, k, &result) == J89A_OK)
        {
            if (result != 17)
            {
                fail("fold count wrong on success");
            }
            any_success = 1;
        }
        else
        {
            any_fail = 1;
        }
    }
    if (!any_fail)
    {
        fail("fold alloc sweep never failed");
    }
    if (!any_success)
    {
        fail("fold alloc sweep never succeeded");
    }
    j89a_json_release(tree);
}

/* object.fold allocates two R buffers per union node */
static j89a_status m_obj_member_count(void *c, const j89a_str *name,
                                      const j89a_json *value, void *out)
{
    (void)c;
    (void)name;
    (void)value;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static void test_object_fold_alloc_failure(void)
{
    j89a_alloc al;
    j89a_object *obj;
    unsigned long k;
    int any_fail;
    int any_success;
    j89a_rtype rt;

    j89a_alloc_init(&al);
    obj = dup_object(&al);
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    any_fail = 0;
    any_success = 0;
    for (k = 1; k <= 200; k = k + 1)
    {
        fail_alloc f;
        unsigned long zero;
        unsigned long result;
        zero = 0;
        fail_alloc_init(&f, k);
        if (j89a_object_fold(obj, &rt, &zero, m_obj_member_count, m_rsum, NULL,
                             &result, &f.al) == J89A_OK)
        {
            if (result != 6)
            {
                fail("object fold count wrong");
            }
            any_success = 1;
        }
        else
        {
            any_fail = 1;
        }
    }
    if (!any_fail)
    {
        fail("object fold alloc sweep never failed");
    }
    if (!any_success)
    {
        fail("object fold alloc sweep never succeeded");
    }
    j89a_object_release(obj);
}

/* callback failure propagation: fold algebra that fails on a number */
typedef struct fail_ctx
{
    int armed;
    int failed;
} fail_ctx;

static j89a_status n_bad(void *c, double n, void *out)
{
    fail_ctx *fc;
    fc = (fail_ctx *)c;
    if (fc->armed)
    {
        fc->failed = 1;
        return J89A_CALLBACK;
    }
    (void)n;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static void test_callback_failure(void)
{
    j89a_alloc al;
    j89a_json *tree;
    j89a_rtype rt;
    j89a_algebra alg;
    fail_ctx fc;
    unsigned long result;

    j89a_alloc_init(&al);
    tree = wide_array(4, &al);
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    alg.rt = &rt;
    fill_algebra(&alg);
    alg.number_case = n_bad;
    fc.armed = 1;
    fc.failed = 0;
    alg.ctx = &fc;
    if (j89a_json_fold(tree, &alg, &result, &al) != J89A_CALLBACK)
    {
        fail("json.fold did not propagate callback failure");
    }
    if (!fc.failed)
    {
        fail("callback was not invoked before failure");
    }
    j89a_json_release(tree);
}

static j89a_status map_fail(void *c, const j89a_json *element, j89a_json **out)
{
    (void)c;
    (void)element;
    (void)out;
    return J89A_CALLBACK;
}

static void test_list_map_callback_failure(void)
{
    j89a_alloc al;
    j89a_list *xs;
    j89a_list *out;
    j89a_json *e;
    j89a_status st;
    int i;

    j89a_alloc_init(&al);
    xs = j89a_list_nil(&al);
    for (i = 0; i < 3; i = i + 1)
    {
        j89a_list *nx;
        e = j89a_json_null(&al);
        nx = j89a_list_cons(e, xs, &al);
        j89a_json_release(e);
        j89a_list_release(xs);
        xs = nx;
    }
    out = NULL;
    st = j89a_list_map(xs, map_fail, NULL, &out, &al);
    if (st != J89A_CALLBACK)
    {
        fail("list.map did not propagate callback failure");
    }
    if (out != NULL)
    {
        fail("list.map produced a partial output on callback failure");
    }
    j89a_list_release(xs);
}

/* A 200k-element flat list must release without exhausting the C stack. */
static void test_wide_list_release(void)
{
    j89a_alloc al;
    j89a_list *xs;
    j89a_list *nx;
    j89a_json *e;
    unsigned long i;

    j89a_alloc_init(&al);
    xs = j89a_list_nil(&al);
    for (i = 0; i < 200000; i = i + 1)
    {
        e = j89a_json_null(&al);
        nx = j89a_list_cons(e, xs, &al);
        j89a_json_release(e);
        j89a_list_release(xs);
        xs = nx;
    }
    j89a_list_release(xs);
}

/* count step for list.fold over a wide list (R = unsigned long) */
static j89a_status w_count_step(void *c, const j89a_json *element,
                                const void *acc, void *out)
{
    (void)c;
    (void)element;
    *(unsigned long *)out = *(const unsigned long *)acc + 1;
    return J89A_OK;
}

static j89a_status w_map_null(void *c, const j89a_json *element,
                              j89a_json **out)
{
    j89a_alloc al;
    (void)c;
    (void)element;
    j89a_alloc_init(&al);
    *out = j89a_json_null(&al);
    if (*out == NULL)
    {
        return J89A_NOMEM;
    }
    return J89A_OK;
}

/* Wide iterative fold + map + concat must not exhaust the C stack. */
static void test_wide_ops(void)
{
    j89a_alloc al;
    j89a_list *xs;
    j89a_list *nx;
    j89a_json *e;
    unsigned long i;
    unsigned long count;
    j89a_rtype rt;
    unsigned long zero;
    j89a_list *mapped;

    j89a_alloc_init(&al);
    xs = j89a_list_nil(&al);
    for (i = 0; i < 200000; i = i + 1)
    {
        e = j89a_json_null(&al);
        nx = j89a_list_cons(e, xs, &al);
        j89a_json_release(e);
        j89a_list_release(xs);
        xs = nx;
    }
    zero = 0;
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_list_fold(xs, &rt, &zero, w_count_step, NULL, &count, &al) !=
            J89A_OK ||
        count != 200000)
    {
        fail("wide fold count wrong");
    }
    if (j89a_list_map(xs, w_map_null, NULL, &mapped, &al) != J89A_OK)
    {
        fail("wide map failed");
    }
    else
    {
        j89a_list_release(mapped);
    }
    /* concat two 100k lists -> 200k */
    {
        j89a_list *a;
        j89a_list *b;
        j89a_list *c;
        a = j89a_list_nil(&al);
        b = j89a_list_nil(&al);
        for (i = 0; i < 100000; i = i + 1)
        {
            e = j89a_json_null(&al);
            nx = j89a_list_cons(e, a, &al);
            j89a_json_release(e);
            j89a_list_release(a);
            a = nx;
            e = j89a_json_null(&al);
            nx = j89a_list_cons(e, b, &al);
            j89a_json_release(e);
            j89a_list_release(b);
            b = nx;
        }
        c = j89a_list_concat(a, b, &al);
        j89a_list_release(a);
        j89a_list_release(b);
        if (c == NULL)
        {
            fail("wide concat failed");
        }
        else
        {
            j89a_list_release(c);
        }
    }
    j89a_list_release(xs);
}

/* A 200k-member degenerate (left-skewed) union tree must release without
 * exhausting the C stack. */
static void test_wide_object_release(void)
{
    j89a_alloc al;
    j89a_object *acc;
    j89a_object *u;
    j89a_str *kn;
    j89a_json *v;
    unsigned long i;

    j89a_alloc_init(&al);
    kn = j89a_str_new("k", 1, &al);
    v = j89a_json_null(&al);
    acc = j89a_object_empty(&al);
    for (i = 0; i < 200000; i = i + 1)
    {
        j89a_object *m;
        m = j89a_object_member(kn, v, &al);
        u = j89a_object_union(acc, m, &al);
        j89a_object_release(acc);
        j89a_object_release(m);
        acc = u;
    }
    j89a_str_release(kn);
    j89a_json_release(v);
    j89a_object_release(acc);
}

/* 200k-member degenerate union tree: map and fold must be stack-safe. */
static j89a_str *wide_name_id(void *c, const j89a_str *name)
{
    j89a_alloc al;
    (void)c;
    j89a_alloc_init(&al);
    return j89a_str_new(j89a_str_data(name), j89a_str_len(name), &al);
}

static j89a_status wide_value_null(void *c, const j89a_json *value,
                                   j89a_json **out)
{
    j89a_alloc al;
    j89a_json *n;
    (void)c;
    (void)value;
    j89a_alloc_init(&al);
    n = j89a_json_null(&al);
    if (n == NULL)
    {
        return J89A_NOMEM;
    }
    *out = n;
    return J89A_OK;
}

static void test_wide_object_map_fold(void)
{
    j89a_alloc al;
    j89a_object *acc;
    j89a_object *u;
    j89a_str *kn;
    j89a_json *v;
    unsigned long i;
    unsigned long count;
    j89a_rtype rt;
    unsigned long zero;

    j89a_alloc_init(&al);
    kn = j89a_str_new("k", 1, &al);
    v = j89a_json_null(&al);
    acc = j89a_object_empty(&al);
    for (i = 0; i < 200000; i = i + 1)
    {
        j89a_object *m;
        m = j89a_object_member(kn, v, &al);
        u = j89a_object_union(acc, m, &al);
        j89a_object_release(acc);
        j89a_object_release(m);
        acc = u;
    }
    zero = 0;
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_object_fold(acc, &rt, &zero, m_obj_member_count, m_rsum, NULL,
                         &count, &al) != J89A_OK ||
        count != 200000)
    {
        fail("wide object fold count wrong");
    }
    {
        j89a_object *mapped;
        if (j89a_object_map(acc, wide_name_id, wide_value_null, NULL, &mapped,
                            &al) == J89A_OK)
        {
            if (j89a_object_fold(mapped, &rt, &zero, m_obj_member_count, m_rsum,
                                 NULL, &count, &al) != J89A_OK ||
                count != 200000)
            {
                fail("wide object map result count wrong");
            }
            j89a_object_release(mapped);
        }
    }
    j89a_object_release(acc);
    j89a_str_release(kn);
    j89a_json_release(v);
}

int main(void)
{
    test_map_alloc_failure();
    test_fold_alloc_failure();
    test_object_fold_alloc_failure();
    test_callback_failure();
    test_list_map_callback_failure();
    test_wide_list_release();
    test_wide_ops();
    test_wide_object_release();
    test_wide_object_map_fold();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure test(s) FAILED\n", failures);
        return 1;
    }
    printf("failure injection ok\n");
    return 0;
}
