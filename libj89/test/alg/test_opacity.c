/* test_opacity.c - behavioral scalar opacity for j89_alg.
 *
 * The core is built against the concrete carriers N = double and S = byte
 * string. Behavioral opacity means the public interface supports
 * construction, embedding, projection, folding, and mapping over such
 * opaque-style values without requiring any profile. (Source/dependency
 * opacity is enforced separately by `just check-forbidden`.)
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

/* fold: count the number of scalar string occurrences across the value.
 * R = unsigned long. Only uses the public construction/elimination API. */
static j89a_status s_string(void *c, const j89a_str *s, void *out)
{
    size_t n;
    (void)c;
    n = j89a_str_len(s);
    (void)n;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static j89a_status s_null(void *c, void *out)
{
    (void)c;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static j89a_status s_bool(void *c, j89a_bool b, void *out)
{
    (void)c;
    (void)b;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static j89a_status s_number(void *c, double n, void *out)
{
    (void)c;
    (void)n;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static j89a_status s_sum(void *c, const void *e, const void *a, void *out)
{
    (void)c;
    *(unsigned long *)out =
        *(const unsigned long *)e + *(const unsigned long *)a;
    return J89A_OK;
}

static j89a_status s_copy_member(void *c, const j89a_str *name,
                                 const void *value, void *out)
{
    (void)c;
    (void)name;
    *(unsigned long *)out = *(const unsigned long *)value;
    return J89A_OK;
}

static j89a_status s_array(void *c, const j89a_rlist *children, void *out)
{
    unsigned long sum;
    unsigned long zero;
    j89a_rtype rt;
    j89a_alloc al;
    (void)c;
    j89a_alloc_init(&al);
    zero = 0;
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_rlist_fold(children, &rt, &zero, s_sum, NULL, &sum, &al) !=
        J89A_OK)
    {
        return J89A_CALLBACK;
    }
    *(unsigned long *)out = sum;
    return J89A_OK;
}

static j89a_status s_object(void *c, const j89a_robject *members, void *out)
{
    unsigned long sum;
    unsigned long zero;
    j89a_rtype rt;
    j89a_alloc al;
    (void)c;
    j89a_alloc_init(&al);
    zero = 0;
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_robject_fold(members, &rt, &zero, s_copy_member, s_sum, NULL, &sum,
                          &al) != J89A_OK)
    {
        return J89A_CALLBACK;
    }
    *(unsigned long *)out = sum;
    return J89A_OK;
}

static double num_double(void *c, double n)
{
    (void)c;
    return n;
}

static j89a_str *str_pass(void *c, const j89a_str *s)
{
    j89a_alloc al;
    j89a_str *copy;
    (void)c;
    j89a_alloc_init(&al);
    copy = j89a_str_new(j89a_str_data(s), j89a_str_len(s), &al);
    return copy;
}

int main(void)
{
    j89a_alloc al;
    j89a_str *key;
    j89a_str *val;
    j89a_json *value;
    j89a_object *m;
    j89a_object *m2;
    j89a_object *bag;
    j89a_json *obj;
    j89a_json *mapped;
    j89a_algebra alg;
    j89a_rtype rt;
    unsigned long result;

    j89a_alloc_init(&al);

    /* { a: "x", a: [1] } : two string-valued members sharing one name, plus a
     * nested number. Folds to 2 (string scalar occurrences) with no profile. */
    key = j89a_str_new("a", 1, &al);
    val = j89a_str_new("x", 1, &al);
    value = j89a_json_string(val, &al);
    j89a_str_release(val);
    m = j89a_object_member(key, value, &al);
    j89a_str_release(key);
    j89a_json_release(value);

    key = j89a_str_new("a", 1, &al);
    value = j89a_json_number(1.0, &al);
    m2 = j89a_object_member(key, value, &al);
    j89a_str_release(key);
    j89a_json_release(value);

    bag = j89a_object_union(m, m2, &al);
    j89a_object_release(m);
    j89a_object_release(m2);
    obj = j89a_json_object(bag, &al);
    j89a_object_release(bag);

    /* json.map must not disturb the value and must share no state. */
    mapped = j89a_json_map(obj, num_double, str_pass, NULL, &al);
    if (mapped == NULL)
    {
        fail("json.map over opaque carriers failed");
    }

    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    alg.rt = &rt;
    alg.ctx = NULL;
    alg.null_case = s_null;
    alg.boolean_case = s_bool;
    alg.number_case = s_number;
    alg.string_case = s_string;
    alg.array_case = s_array;
    alg.object_case = s_object;
    if (j89a_json_fold(obj, &alg, &result, &al) != J89A_OK)
    {
        fail("json.fold over opaque carriers failed");
    }
    else if (result != 2)
    {
        fail("string-occurrence count wrong");
        fprintf(stderr, "  expected 2, got %lu\n", result);
    }

    j89a_json_release(mapped);
    j89a_json_release(obj);
    if (failures != 0)
    {
        fprintf(stderr, "%d opacity test(s) FAILED\n", failures);
        return 1;
    }
    printf("opacity ok\n");
    return 0;
}
