/* test_bridge.c - import a built j89.h document into j89_alg. */
#include <stdio.h>
#include <string.h>

#include "j89.h"
#include "j89_alg.h"
#include "j89_alg_bridge.h"

static int failures;

static void fail(const char *label)
{
    fprintf(stderr, "FAIL: %s\n", label);
    failures = failures + 1;
}

typedef unsigned long count_t;

typedef struct cap_ctx
{
    char bytes[64];
    size_t n;
    int seen;
} cap_ctx;

static j89a_status b_null(void *c, void *out)
{
    (void)c;
    *(count_t *)out = 1;
    return J89A_OK;
}

static j89a_status b_bool(void *c, j89a_bool b, void *out)
{
    (void)c;
    (void)b;
    *(count_t *)out = 1;
    return J89A_OK;
}

static j89a_status b_number(void *c, double n, void *out)
{
    (void)c;
    (void)n;
    *(count_t *)out = 1;
    return J89A_OK;
}

static j89a_status b_string(void *c, const j89a_str *s, void *out)
{
    cap_ctx *cc;
    size_t n;
    cc = (cap_ctx *)c;
    n = j89a_str_len(s);
    if (!cc->seen && n < sizeof(cc->bytes))
    {
        memcpy(cc->bytes, j89a_str_data(s), n);
        cc->bytes[n] = '\0';
        cc->n = n;
        cc->seen = 1;
    }
    *(count_t *)out = 1;
    return J89A_OK;
}

static j89a_status b_sum(void *c, const void *e, const void *a, void *out)
{
    (void)c;
    *(count_t *)out = *(const count_t *)e + *(const count_t *)a;
    return J89A_OK;
}

static j89a_status b_copy_member(void *c, const j89a_str *name,
                                 const void *value, void *out)
{
    (void)c;
    (void)name;
    *(count_t *)out = *(const count_t *)value;
    return J89A_OK;
}

static j89a_status b_array(void *c, const j89a_rlist *children, void *out)
{
    count_t zero;
    count_t sum;
    j89a_rtype rt;
    j89a_alloc al;
    (void)c;
    j89a_alloc_init(&al);
    zero = 0;
    rt.size = sizeof(count_t);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_rlist_fold(children, &rt, &zero, b_sum, NULL, &sum, &al) !=
        J89A_OK)
    {
        return J89A_CALLBACK;
    }
    *(count_t *)out = sum + 1;
    return J89A_OK;
}

static j89a_status b_object(void *c, const j89a_robject *members, void *out)
{
    count_t zero;
    count_t sum;
    j89a_rtype rt;
    j89a_alloc al;
    (void)c;
    j89a_alloc_init(&al);
    zero = 0;
    rt.size = sizeof(count_t);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_robject_fold(members, &rt, &zero, b_copy_member, b_sum, NULL, &sum,
                          &al) != J89A_OK)
    {
        return J89A_CALLBACK;
    }
    *(count_t *)out = sum + 1;
    return J89A_OK;
}

typedef struct name_ctx
{
    size_t n;
    char bytes[16];
    int seen;
} name_ctx;

static j89a_status b_capture_name(void *c, const j89a_str *name,
                                  const void *value, void *out)
{
    name_ctx *nc;
    size_t n;
    (void)value;
    nc = (name_ctx *)c;
    n = j89a_str_len(name);
    if (!nc->seen && n < sizeof(nc->bytes))
    {
        memcpy(nc->bytes, j89a_str_data(name), n);
        nc->n = n;
        nc->seen = 1;
    }
    *(count_t *)out = 1;
    return J89A_OK;
}

static j89a_status b_capture_object(void *c, const j89a_robject *members,
                                    void *out)
{
    count_t zero;
    count_t sum;
    j89a_rtype rt;
    j89a_alloc al;
    j89a_alloc_init(&al);
    zero = 0;
    rt.size = sizeof(count_t);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_robject_fold(members, &rt, &zero, b_capture_name, b_sum, c, &sum,
                          &al) != J89A_OK)
    {
        return J89A_CALLBACK;
    }
    *(count_t *)out = sum;
    return J89A_OK;
}

static j89_len build_doc(j89_arena *a)
{
    j89_len root;
    j89_len s;
    j89_len n;
    j89_len arr;
    j89_len e1;
    j89_len e2;
    root = j89_object_new(a, 3);
    s = j89_string_new(a, "hi", 2);
    n = j89_integer_new(a, 7.0);
    e1 = j89_integer_new(a, 1.0);
    e2 = j89_double_new(a, 2.5);
    arr = j89_array_new(a, 2);
    j89_array_set(a, arr, 0, e1);
    j89_array_set(a, arr, 1, e2);
    j89_object_set(a, root, 0, "s", 1, s);
    j89_object_set(a, root, 1, "n", 1, n);
    j89_object_set(a, root, 2, "a", 1, arr);
    return root;
}

int main(void)
{
    j89_arena ja;
    j89a_alloc al;
    j89_len root;
    j89a_json *v;
    j89a_algebra alg;
    j89a_rtype rt;
    cap_ctx cc;
    count_t result;

    j89_arena_init(&ja);
    j89a_alloc_init(&al);
    root = build_doc(&ja);

    if (j89a_import_j89(&ja, root, &v, &al) != J89A_OK || v == NULL)
    {
        fail("bridge import failed");
        j89_arena_destroy(&ja);
        return 1;
    }

    memset(&cc, 0, sizeof(cc));
    rt.size = sizeof(count_t);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    alg.rt = &rt;
    alg.ctx = &cc;
    alg.null_case = b_null;
    alg.boolean_case = b_bool;
    alg.number_case = b_number;
    alg.string_case = b_string;
    alg.array_case = b_array;
    alg.object_case = b_object;
    if (j89a_json_fold(v, &alg, &result, &al) != J89A_OK)
    {
        fail("bridge import fold failed");
    }
    else if (result != 6)
    {
        fail("bridge import node count wrong");
        fprintf(stderr, "  expected 6, got %lu\n", result);
    }
    if (cc.seen != 1 || cc.n != 2 || strcmp(cc.bytes, "hi") != 0)
    {
        fail("bridge string carrier wrong");
    }

    j89a_json_release(v);
    j89_arena_destroy(&ja);

    /* An embedded-NUL object key must survive the bridge byte-for-byte. */
    {
        j89_arena ka;
        j89a_alloc kal;
        j89_len kroot;
        j89a_json *kv;
        j89a_algebra kalg;
        j89a_rtype krt;
        name_ctx nc;
        count_t kresult;
        static const char nulkey[] = {'k', '\0', 'y'};

        j89_arena_init(&ka);
        j89a_alloc_init(&kal);
        kroot = j89_object_new(&ka, 1);
        j89_object_set(&ka, kroot, 0, nulkey, 3, j89_integer_new(&ka, 1));
        if (j89a_import_j89(&ka, kroot, &kv, &kal) != J89A_OK || kv == NULL)
        {
            fail("bridge nul-key import failed");
        }
        else
        {
            memset(&nc, 0, sizeof(nc));
            krt.size = sizeof(count_t);
            krt.copy = NULL;
            krt.drop = NULL;
            krt.ctx = NULL;
            kalg.rt = &krt;
            kalg.ctx = &nc;
            kalg.null_case = b_null;
            kalg.boolean_case = b_bool;
            kalg.number_case = b_number;
            kalg.string_case = b_string;
            kalg.array_case = b_array;
            kalg.object_case = b_capture_object;
            if (j89a_json_fold(kv, &kalg, &kresult, &kal) != J89A_OK)
            {
                fail("bridge nul-key fold failed");
            }
            else if (nc.seen != 1 || nc.n != 3 || nc.bytes[0] != 'k' ||
                     nc.bytes[1] != '\0' || nc.bytes[2] != 'y')
            {
                fail("bridge nul-key carrier truncated");
            }
            j89a_json_release(kv);
        }
        j89_arena_destroy(&ka);
    }

    if (failures != 0)
    {
        fprintf(stderr, "%d bridge test(s) FAILED\n", failures);
        return 1;
    }
    printf("bridge ok\n");
    return 0;
}
