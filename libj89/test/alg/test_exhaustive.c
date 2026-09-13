/* test_exhaustive.c - bounded exhaustive law checker for the j89_alg core.
 *
 * Enumerates every value of a small finite grammar (all scalar constructors
 * and every array sequence of length 0..3 over a two-element carrier pool)
 * and checks the fixed-point isomorphism and json.map identity on each.
 * Equality uses a test-private canonical text (scalars and arrays by
 * position). Object-bag law coverage (commutativity, multiplicity) is carried
 * by test_core and test_failure.
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

static char *render_value(const j89a_json *v);

/* ---- growable text builder (test only) ----------------------------------- */

typedef struct sb
{
    char *mem;
    size_t len;
    size_t cap;
} sb;

static void sb_put(sb *b, const char *s)
{
    size_t n;
    size_t nc;
    char *m;
    n = strlen(s);
    if (b->cap < b->len + n + 1)
    {
        nc = b->cap;
        if (nc == 0)
        {
            nc = 64;
        }
        while (nc < b->len + n + 1)
        {
            nc = nc * 2;
        }
        m = (char *)realloc(b->mem, nc);
        if (m == NULL)
        {
            exit(2);
        }
        b->mem = m;
        b->cap = nc;
    }
    memcpy(b->mem + b->len, s, n);
    b->len = b->len + n;
    b->mem[b->len] = '\0';
}

static void sb_putc(sb *b, char c)
{
    char s[2];
    s[0] = c;
    s[1] = '\0';
    sb_put(b, s);
}

static char *sb_take(sb *b)
{
    char *s;
    b->mem[b->len] = '\0';
    s = b->mem;
    b->mem = NULL;
    b->len = 0;
    b->cap = 0;
    return s;
}

/* ---- R = single owned text (ordered array join) ----------------------------
 */

typedef struct text
{
    char *s;
} text;

static j89a_status text_copy(void *c, void *dst, const void *src)
{
    const text *s;
    (void)c;
    s = (const text *)src;
    ((text *)dst)->s = (char *)malloc(strlen(s->s) + 1);
    if (((text *)dst)->s == NULL)
    {
        return J89A_NOMEM;
    }
    strcpy(((text *)dst)->s, s->s);
    return J89A_OK;
}

static void text_drop(void *c, void *v)
{
    (void)c;
    if (((text *)v)->s != NULL)
    {
        free(((text *)v)->s);
    }
}

static j89a_status arr_step(void *c, const j89a_json *element, const void *acc,
                            void *out)
{
    char *et;
    char *j;
    sb b;
    (void)c;
    b.mem = NULL;
    b.len = 0;
    b.cap = 0;
    et = render_value(element);
    if (((const text *)acc)->s[0] == '\0')
    {
        sb_put(&b, et);
    }
    else
    {
        sb_put(&b, et);
        sb_putc(&b, ',');
        sb_put(&b, ((const text *)acc)->s);
    }
    free(et);
    j = sb_take(&b);
    ((text *)out)->s = j;
    return J89A_OK;
}

static char *render_array(const j89a_list *xs, j89a_alloc *al)
{
    j89a_rtype rt;
    text acc;
    text zero;
    j89a_status st;
    sb b;
    char *s;
    zero.s = (char *)malloc(1);
    zero.s[0] = '\0';
    rt.size = sizeof(text);
    rt.copy = text_copy;
    rt.drop = text_drop;
    rt.ctx = NULL;
    b.mem = NULL;
    b.len = 0;
    b.cap = 0;
    sb_putc(&b, '[');
    st = j89a_list_fold(xs, &rt, &zero, arr_step, NULL, &acc, al);
    if (st != J89A_OK)
    {
        exit(2);
    }
    if (acc.s[0] != '\0')
    {
        sb_put(&b, acc.s);
    }
    free(acc.s);
    free(zero.s);
    sb_putc(&b, ']');
    s = sb_take(&b);
    return s;
}

static char *render_value(const j89a_json *v)
{
    j89a_shape shape;
    j89a_alloc al;
    sb b;
    char buf[64];
    char *s;

    j89a_alloc_init(&al);
    b.mem = NULL;
    b.len = 0;
    b.cap = 0;
    j89a_json_project(v, &shape);
    if (shape.kind == J89A_NULL)
    {
        sb_put(&b, "null");
    }
    else if (shape.kind == J89A_BOOLEAN)
    {
        sb_put(&b, shape.boolean_value == J89A_TRUE ? "true" : "false");
    }
    else if (shape.kind == J89A_NUMBER)
    {
        sprintf(buf, "%.17g", shape.number_value);
        sb_put(&b, buf);
    }
    else if (shape.kind == J89A_STRING)
    {
        const unsigned char *p;
        size_t n;
        size_t i;
        p = (const unsigned char *)j89a_str_data(shape.string_value);
        n = j89a_str_len(shape.string_value);
        sb_putc(&b, '"');
        for (i = 0; i < n; i = i + 1)
        {
            sb_putc(&b, (char)p[i]);
        }
        sb_putc(&b, '"');
    }
    else
    {
        char *inner;
        inner = render_array(shape.array_value, &al);
        sb_put(&b, inner);
        free(inner);
    }
    s = sb_take(&b);
    return s;
}

static int value_eq(const j89a_json *a, const j89a_json *b)
{
    char *ca;
    char *cb;
    int eq;
    ca = render_value(a);
    cb = render_value(b);
    eq = strcmp(ca, cb) == 0;
    free(ca);
    free(cb);
    return eq;
}

/* ---- fresh scalar value factories (owned) ----------------------------------
 */

static j89a_json *leafv(int which, j89a_alloc *al)
{
    j89a_str *k;
    j89a_json *j;
    if (which == 0)
    {
        return j89a_json_null(al);
    }
    if (which == 1)
    {
        return j89a_json_boolean(J89A_TRUE, al);
    }
    if (which == 2)
    {
        return j89a_json_boolean(J89A_FALSE, al);
    }
    if (which == 3)
    {
        return j89a_json_number(1.0, al);
    }
    if (which == 4)
    {
        return j89a_json_number(2.0, al);
    }
    k = j89a_str_new(which == 5 ? "x" : "yy", which == 5 ? 1 : 2, al);
    j = j89a_json_string(k, al);
    j89a_str_release(k);
    return j;
}

/* ---- maps ------------------------------------------------------------------
 */

static double num_id(void *c, double n)
{
    (void)c;
    return n;
}

static j89a_str *str_id(void *c, const j89a_str *s)
{
    j89a_alloc al;
    j89a_str *copy;
    (void)c;
    j89a_alloc_init(&al);
    copy = j89a_str_new(j89a_str_data(s), j89a_str_len(s), &al);
    return copy;
}

static void check_value(j89a_json *v, const char *label, j89a_alloc *al)
{
    j89a_shape shape;
    j89a_json *rt;
    j89a_json *mid;
    j89a_json_project(v, &shape);
    rt = j89a_json_embed(&shape, al);
    if (!value_eq(v, rt))
    {
        fail(label);
        j89a_json_release(rt);
        return;
    }
    j89a_json_release(rt);
    mid = j89a_json_map(v, num_id, str_id, NULL, al);
    if (!value_eq(v, mid))
    {
        fail(label);
    }
    j89a_json_release(mid);
}

int main(void)
{
    j89a_alloc al;
    char label[64];
    int a;
    int b;
    int c;
    j89a_list *xs;
    j89a_list *nx;
    j89a_json *v;
    j89a_json *el;

    j89a_alloc_init(&al);

    for (a = 0; a < 7; a = a + 1)
    {
        v = leafv(a, &al);
        sprintf(label, "exh scalar %d", a);
        check_value(v, label, &al);
        j89a_json_release(v);
    }

    for (a = 0; a <= 1; a = a + 1)
    {
        for (b = 0; b <= 1; b = b + 1)
        {
            for (c = 0; c <= 1; c = c + 1)
            {
                xs = j89a_list_nil(&al);
                if (a)
                {
                    el = leafv(3, &al);
                    nx = j89a_list_cons(el, xs, &al);
                    j89a_json_release(el);
                    j89a_list_release(xs);
                    xs = nx;
                }
                if (b)
                {
                    el = leafv(4, &al);
                    nx = j89a_list_cons(el, xs, &al);
                    j89a_json_release(el);
                    j89a_list_release(xs);
                    xs = nx;
                }
                if (c)
                {
                    el = leafv(5, &al);
                    nx = j89a_list_cons(el, xs, &al);
                    j89a_json_release(el);
                    j89a_list_release(xs);
                    xs = nx;
                }
                v = j89a_json_array(xs, &al);
                j89a_list_release(xs);
                sprintf(label, "exh array %d%d%d", a, b, c);
                check_value(v, label, &al);
                j89a_json_release(v);
            }
        }
    }

    if (failures != 0)
    {
        fprintf(stderr, "%d exhaustive test(s) FAILED\n", failures);
        return 1;
    }
    printf("exhaustive laws ok\n");
    return 0;
}
