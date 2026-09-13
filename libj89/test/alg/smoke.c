/* smoke.c - end-to-end smoke test for the j89_alg algebraic JSON core. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "j89_alg.h"

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

/* ---- node-count algebra over R = unsigned long ----------------------------
 */

typedef unsigned long count_t;

static j89a_status c_null(void *c, void *out)
{
    (void)c;
    *(count_t *)out = 1;
    return J89A_OK;
}

static j89a_status c_bool(void *c, j89a_bool b, void *out)
{
    (void)c;
    (void)b;
    *(count_t *)out = 1;
    return J89A_OK;
}

static j89a_status c_number(void *c, double n, void *out)
{
    (void)c;
    (void)n;
    *(count_t *)out = 1;
    return J89A_OK;
}

static j89a_status c_string(void *c, const j89a_str *s, void *out)
{
    (void)c;
    (void)s;
    *(count_t *)out = 1;
    return J89A_OK;
}

static j89a_status c_rsum(void *c, const void *element, const void *acc,
                          void *out)
{
    (void)c;
    *(count_t *)out = *(const count_t *)element + *(const count_t *)acc;
    return J89A_OK;
}

static j89a_status c_rcopy_member(void *c, const j89a_str *name,
                                  const void *value, void *out)
{
    (void)c;
    (void)name;
    *(count_t *)out = *(const count_t *)value;
    return J89A_OK;
}

static j89a_status c_array(void *c, const j89a_rlist *children, void *out)
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
    if (j89a_rlist_fold(children, &rt, &zero, c_rsum, NULL, &sum, &al) !=
        J89A_OK)
    {
        return J89A_CALLBACK;
    }
    *(count_t *)out = sum + 1;
    return J89A_OK;
}

static j89a_status c_object(void *c, const j89a_robject *members, void *out)
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
    if (j89a_robject_fold(members, &rt, &zero, c_rcopy_member, c_rsum, NULL,
                          &sum, &al) != J89A_OK)
    {
        return J89A_CALLBACK;
    }
    *(count_t *)out = sum + 1;
    return J89A_OK;
}

static j89a_json *make_tree(j89a_alloc *al)
{
    j89a_json *num;
    j89a_json *str_json;
    j89a_str *str;
    j89a_list *array;
    j89a_json *arr_json;
    j89a_str *name_x;
    j89a_str *name_s;
    j89a_object *m1;
    j89a_object *m2;
    j89a_object *members;
    j89a_list *nil_list;
    j89a_shape shape;
    j89a_json *result;

    num = j89a_json_number(2.0, al);
    str = j89a_str_new("hi", 2, al);
    str_json = j89a_json_string(str, al);
    nil_list = j89a_list_nil(al);
    array = j89a_list_cons(num, nil_list, al);
    arr_json = j89a_json_array(array, al);
    name_x = j89a_str_new("x", 1, al);
    name_s = j89a_str_new("s", 1, al);
    m1 = j89a_object_member(name_x, arr_json, al);
    m2 = j89a_object_member(name_s, str_json, al);
    members = j89a_object_union(m1, m2, al);
    shape.kind = J89A_OBJECT;
    shape.boolean_value = J89A_FALSE;
    shape.number_value = 0.0;
    shape.string_value = NULL;
    shape.array_value = NULL;
    shape.object_value = members;
    result = j89a_json_embed(&shape, al);

    j89a_json_release(num);
    j89a_str_release(str);
    j89a_json_release(str_json);
    j89a_list_release(nil_list);
    j89a_list_release(array);
    j89a_json_release(arr_json);
    j89a_str_release(name_x);
    j89a_str_release(name_s);
    j89a_object_release(m1);
    j89a_object_release(m2);
    j89a_object_release(members);
    return result;
}

static void test_node_count(void)
{
    j89a_alloc al;
    j89a_json *tree;
    j89a_algebra alg;
    j89a_rtype rt;
    count_t result;

    j89a_alloc_init(&al);
    tree = make_tree(&al);
    rt.size = sizeof(count_t);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    alg.rt = &rt;
    alg.ctx = NULL;
    alg.null_case = c_null;
    alg.boolean_case = c_bool;
    alg.number_case = c_number;
    alg.string_case = c_string;
    alg.array_case = c_array;
    alg.object_case = c_object;
    if (j89a_json_fold(tree, &alg, &result, &al) != J89A_OK)
    {
        fail("node count fold failed");
    }
    else if (result != 4)
    {
        fail("node count fold wrong");
        fprintf(stderr, "  expected 4, got %lu\n", result);
    }
    j89a_json_release(tree);
}

int main(void)
{
    test_node_count();
    if (failures != 0)
    {
        fprintf(stderr, "%d smoke test(s) FAILED\n", failures);
        return 1;
    }
    printf("smoke ok\n");
    return 0;
}
