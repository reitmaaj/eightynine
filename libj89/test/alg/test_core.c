/* test_core.c - law tests for the j89_alg algebraic JSON core.
 *
 * A canonical text renderer (test-only oracle) turns a Json value into a
 * deterministic string: arrays by position, objects by sorted member text so
 * member order never matters, member multiplicity is preserved. Algebraic
 * laws are checked by comparing canonical texts.
 *
 * All helpers that build composite values return OWNED results and release
 * every temporary handle, so the suite is leak-clean under sanitizers.
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

/* ---- growable text builder (test only) ----------------------------------- */

typedef struct sb
{
    char *mem;
    size_t len;
    size_t cap;
} sb;

static void sb_reserve(sb *b, size_t need)
{
    size_t nc;
    char *m;
    if (b->cap >= need)
    {
        return;
    }
    nc = b->cap;
    if (nc == 0)
    {
        nc = 64;
    }
    while (nc < need)
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

static void sb_put(sb *b, const char *s)
{
    size_t n;
    n = strlen(s);
    sb_reserve(b, b->len + n + 1);
    memcpy(b->mem + b->len, s, n);
    b->len = b->len + n;
    b->mem[b->len] = '\0';
}

static void sb_putc(sb *b, char c)
{
    sb_reserve(b, b->len + 2);
    b->mem[b->len] = c;
    b->len = b->len + 1;
    b->mem[b->len] = '\0';
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

/* ---- tiny builders with correct ownership ---------------------------------
 */

static j89a_json *vnum(double n, j89a_alloc *al)
{
    return j89a_json_number(n, al);
}

/* one JSON string value from a C string; releases the temporary carrier */
static j89a_json *vstr(const char *s, j89a_alloc *al)
{
    j89a_str *k;
    j89a_json *j;
    k = j89a_str_new(s, strlen(s), al);
    j = j89a_json_string(k, al);
    j89a_str_release(k);
    return j;
}

/* build a List<Json> of the given scalar numbers, releasing intermediates */
static j89a_list *num_list(const double *nums, size_t n, j89a_alloc *al)
{
    j89a_list *xs;
    size_t i;
    j89a_list *item;
    j89a_json *v;
    xs = j89a_list_nil(al);
    i = n;
    while (i > 0)
    {
        i = i - 1;
        v = vnum(nums[i], al);
        item = j89a_list_cons(v, xs, al);
        j89a_json_release(v);
        j89a_list_release(xs);
        xs = item;
    }
    return xs;
}

/* one JSON string value from raw bytes (owned string carrier released) */
static j89a_str *skey(const char *s, j89a_alloc *al)
{
    return j89a_str_new(s, strlen(s), al);
}

static j89a_json *obj_with_dup(const double *first, size_t nf, const char *kn1,
                               double d1, const char *kn2, double d2,
                               j89a_alloc *al)
{
    j89a_list *xs;
    j89a_json *arr;
    j89a_json *v;
    j89a_str *k;
    j89a_object *a;
    j89a_object *b;
    j89a_object *u;

    xs = num_list(first, nf, al);
    arr = j89a_json_array(xs, al);
    j89a_list_release(xs);
    k = skey(kn1, al);
    a = j89a_object_member(k, arr, al);
    j89a_str_release(k);
    j89a_json_release(arr);

    k = skey(kn2, al);
    v = vnum(d1, al);
    b = j89a_object_member(k, v, al);
    j89a_str_release(k);
    j89a_json_release(v);

    /* a genuine duplicate: a second member sharing kn2, holding d2. */
    k = skey(kn2, al);
    v = vnum(d2, al);
    {
        j89a_object *c;
        c = j89a_object_member(k, v, al);
        j89a_str_release(k);
        j89a_json_release(v);
        u = j89a_object_union(b, c, al);
        j89a_object_release(b);
        j89a_object_release(c);
    }
    {
        j89a_object *full;
        full = j89a_object_union(a, u, al);
        j89a_object_release(a);
        j89a_object_release(u);
        u = full;
    }

    {
        j89a_json *ow;
        ow = j89a_json_object(u, al);
        j89a_object_release(u);
        return ow;
    }
}

/* ---- R = single owned text ------------------------------------------------
 */

typedef struct text
{
    char *s;
} text;

static j89a_status text_copy(void *c, void *dst, const void *src)
{
    text *d;
    const text *s;
    (void)c;
    d = (text *)dst;
    s = (const text *)src;
    d->s = (char *)malloc(strlen(s->s) + 1);
    if (d->s == NULL)
    {
        return J89A_NOMEM;
    }
    strcpy(d->s, s->s);
    return J89A_OK;
}

static void text_drop(void *c, void *v)
{
    text *t;
    (void)c;
    t = (text *)v;
    if (t->s != NULL)
    {
        free(t->s);
    }
}

/* ---- R = sorted multiset of entry strings (objects) -----------------------
 */

typedef struct entry_bag
{
    char **items;
    size_t n;
    size_t cap;
} entry_bag;

static void eb_free(entry_bag *e)
{
    size_t i;
    for (i = 0; i < e->n; i = i + 1)
    {
        free(e->items[i]);
    }
    free(e->items);
    e->items = NULL;
    e->n = 0;
    e->cap = 0;
}

static void eb_push(entry_bag *e, const char *s)
{
    char *copy;
    char **m;
    size_t nc;
    copy = (char *)malloc(strlen(s) + 1);
    if (copy == NULL)
    {
        exit(2);
    }
    strcpy(copy, s);
    if (e->cap <= e->n)
    {
        nc = e->cap;
        if (nc == 0)
        {
            nc = 4;
        }
        else
        {
            nc = nc * 2;
        }
        m = (char **)realloc(e->items, nc * sizeof(char *));
        if (m == NULL)
        {
            exit(2);
        }
        e->items = m;
        e->cap = nc;
    }
    e->items[e->n] = copy;
    e->n = e->n + 1;
}

static j89a_status eb_copy(void *c, void *dst, const void *src)
{
    entry_bag *d;
    const entry_bag *s;
    size_t i;
    (void)c;
    d = (entry_bag *)dst;
    s = (const entry_bag *)src;
    d->items = NULL;
    d->n = 0;
    d->cap = 0;
    for (i = 0; i < s->n; i = i + 1)
    {
        eb_push(d, s->items[i]);
    }
    return J89A_OK;
}

static void eb_drop(void *c, void *v)
{
    (void)c;
    eb_free((entry_bag *)v);
}

/* ---- canonical renderer ----------------------------------------------------
 */

static char *render_value(const j89a_json *v);

static void render_escaped(sb *b, const j89a_str *s)
{
    const unsigned char *p;
    size_t n;
    size_t i;
    char esc[8];
    p = (const unsigned char *)j89a_str_data(s);
    n = j89a_str_len(s);
    sb_putc(b, '"');
    for (i = 0; i < n; i = i + 1)
    {
        if (p[i] == '"' || p[i] == '\\')
        {
            sb_putc(b, '\\');
            sb_putc(b, (char)p[i]);
        }
        else if (p[i] < 0x20)
        {
            sprintf(esc, "\\u%04x", (unsigned)p[i]);
            sb_put(b, esc);
        }
        else
        {
            sb_putc(b, (char)p[i]);
        }
    }
    sb_putc(b, '"');
}

static j89a_status arr_step(void *c, const j89a_json *element, const void *acc,
                            void *out)
{
    text *o;
    const text *a;
    char *et;
    sb b;
    (void)c;
    o = (text *)out;
    a = (const text *)acc;
    et = render_value(element);
    b.mem = NULL;
    b.len = 0;
    b.cap = 0;
    if (a->s[0] == '\0')
    {
        sb_put(&b, et);
    }
    else
    {
        sb_put(&b, et);
        sb_putc(&b, ',');
        sb_put(&b, a->s);
    }
    free(et);
    o->s = sb_take(&b);
    return J89A_OK;
}

static j89a_status obj_member_fn(void *c, const j89a_str *name,
                                 const j89a_json *value, void *out)
{
    entry_bag *o;
    sb b;
    char *vt;
    char *s;
    (void)c;
    o = (entry_bag *)out;
    b.mem = NULL;
    b.len = 0;
    b.cap = 0;
    render_escaped(&b, name);
    sb_putc(&b, ':');
    vt = render_value(value);
    sb_put(&b, vt);
    free(vt);
    o->items = NULL;
    o->n = 0;
    o->cap = 0;
    s = sb_take(&b);
    eb_push(o, s);
    free(s);
    return J89A_OK;
}

static j89a_status obj_combine_fn(void *c, const void *left, const void *right,
                                  void *out)
{
    entry_bag *o;
    const entry_bag *a;
    const entry_bag *b;
    size_t i;
    size_t j;
    (void)c;
    o = (entry_bag *)out;
    a = (const entry_bag *)left;
    b = (const entry_bag *)right;
    o->items = NULL;
    o->n = 0;
    o->cap = 0;
    i = 0;
    j = 0;
    while (i < a->n && j < b->n)
    {
        if (strcmp(a->items[i], b->items[j]) <= 0)
        {
            eb_push(o, a->items[i]);
            i = i + 1;
        }
        else
        {
            eb_push(o, b->items[j]);
            j = j + 1;
        }
    }
    while (i < a->n)
    {
        eb_push(o, a->items[i]);
        i = i + 1;
    }
    while (j < b->n)
    {
        eb_push(o, b->items[j]);
        j = j + 1;
    }
    return J89A_OK;
}

static char *render_object(const j89a_object *o, j89a_alloc *al)
{
    entry_bag bag;
    entry_bag empty;
    j89a_rtype rt;
    sb b;
    size_t i;
    char *s;

    empty.items = NULL;
    empty.n = 0;
    empty.cap = 0;
    rt.size = sizeof(entry_bag);
    rt.copy = eb_copy;
    rt.drop = eb_drop;
    rt.ctx = NULL;
    b.mem = NULL;
    b.len = 0;
    b.cap = 0;
    sb_putc(&b, '{');
    if (j89a_object_fold(o, &rt, &empty, obj_member_fn, obj_combine_fn, NULL,
                         &bag, al) != J89A_OK)
    {
        exit(2);
    }
    for (i = 0; i < bag.n; i = i + 1)
    {
        if (i != 0)
        {
            sb_putc(&b, ',');
        }
        sb_put(&b, bag.items[i]);
    }
    sb_putc(&b, '}');
    eb_free(&bag);
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
        render_escaped(&b, shape.string_value);
    }
    else if (shape.kind == J89A_ARRAY)
    {
        text acc;
        text zero;
        j89a_rtype rt;
        sb inner;
        sb_putc(&b, '[');
        zero.s = (char *)malloc(1);
        zero.s[0] = '\0';
        rt.size = sizeof(text);
        rt.copy = text_copy;
        rt.drop = text_drop;
        rt.ctx = NULL;
        inner.mem = NULL;
        inner.len = 0;
        inner.cap = 0;
        if (j89a_list_fold(shape.array_value, &rt, &zero, arr_step, NULL, &acc,
                           &al) != J89A_OK)
        {
            exit(2);
        }
        if (acc.s[0] != '\0')
        {
            sb_put(&inner, acc.s);
        }
        sb_put(&b, inner.mem != NULL ? inner.mem : "");
        if (inner.mem != NULL)
        {
            free(inner.mem);
        }
        free(acc.s);
        free(zero.s);
        sb_putc(&b, ']');
    }
    else
    {
        char *os;
        os = render_object(shape.object_value, &al);
        sb_put(&b, os);
        free(os);
    }
    s = sb_take(&b);
    return s;
}

static void expect_eq(const char *label, const j89a_json *a, const j89a_json *b)
{
    char *sa;
    char *sb_;
    int eq;
    sa = render_value(a);
    sb_ = render_value(b);
    eq = strcmp(sa, sb_) == 0;
    if (!eq)
    {
        fail(label);
        fprintf(stderr, "  left  <%s>\n  right <%s>\n", sa, sb_);
    }
    free(sa);
    free(sb_);
}

/* ---- mapping helpers -------------------------------------------------------
 */

typedef struct plus_ctx
{
    double k;
} plus_ctx;

static double num_plus(void *c, double n)
{
    plus_ctx *p;
    p = (plus_ctx *)c;
    return n + p->k;
}

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

/* ---- seeded pseudo-random generator (xorshift64) ---------------------------
 */

static unsigned long prng_state = 2463534242UL;

static unsigned long prng_next(void)
{
    unsigned long x;
    x = prng_state;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    x = x ^ (x << 5);
    prng_state = x;
    return x;
}

static j89a_json *gen_json(unsigned depth, j89a_alloc *al);

static const char *pool_names[] = {"a", "b", "c"};
static const char *pool_strings[] = {"x", "y", "zz"};

static j89a_json *gen_leaf(j89a_alloc *al)
{
    unsigned long r;
    r = prng_next() % 4;
    if (r == 0)
    {
        return j89a_json_null(al);
    }
    if (r == 1)
    {
        return j89a_json_boolean((prng_next() & 1) ? J89A_TRUE : J89A_FALSE,
                                 al);
    }
    if (r == 2)
    {
        return j89a_json_number((double)(prng_next() % 5), al);
    }
    return vstr(pool_strings[(prng_next() % 3)], al);
}

/* array with 0..2 elements */
static j89a_list *gen_list(unsigned depth, j89a_alloc *al)
{
    j89a_list *xs;
    j89a_list *nx;
    j89a_json *child;
    unsigned long n;
    unsigned long i;
    xs = j89a_list_nil(al);
    n = prng_next() % 3;
    i = 0;
    while (i < n)
    {
        child = gen_json(depth, al);
        nx = j89a_list_cons(child, xs, al);
        j89a_json_release(child);
        j89a_list_release(xs);
        xs = nx;
        i = i + 1;
    }
    return xs;
}

/* object whose member names may repeat (bag semantics exercised) */
static j89a_object *gen_object(unsigned depth, j89a_alloc *al)
{
    j89a_object *acc;
    j89a_object *next;
    j89a_json *value;
    j89a_str *kn;
    unsigned long n;
    unsigned long i;
    unsigned prev;
    acc = j89a_object_empty(al);
    n = 1 + (prng_next() % 3);
    prev = 0;
    i = 0;
    while (i < n)
    {
        if (i == 0 || (prng_next() & 1))
        {
            prev = (unsigned)(prng_next() % 3);
        }
        kn = j89a_str_new(pool_names[prev], 1, al);
        value = gen_json(depth, al);
        next = j89a_object_member(kn, value, al);
        j89a_str_release(kn);
        j89a_json_release(value);
        {
            j89a_object *u;
            u = j89a_object_union(acc, next, al);
            j89a_object_release(acc);
            j89a_object_release(next);
            acc = u;
        }
        i = i + 1;
    }
    return acc;
}

static j89a_json *gen_json(unsigned depth, j89a_alloc *al)
{
    unsigned long r;
    j89a_list *xs;
    j89a_object *ms;
    j89a_json *v;
    if (depth == 0 || (prng_next() % 10) < 5)
    {
        return gen_leaf(al);
    }
    r = prng_next() % 3;
    if (r == 0)
    {
        xs = gen_list(depth - 1, al);
        v = j89a_json_array(xs, al);
        j89a_list_release(xs);
        return v;
    }
    ms = gen_object(depth - 1, al);
    v = j89a_json_object(ms, al);
    j89a_object_release(ms);
    return v;
}

static void run_random(void)
{
    j89a_alloc al;
    int i;
    j89a_shape sh;
    plus_ctx p1;
    plus_ctx p2;
    plus_ctx p3;
    char label[64];

    j89a_alloc_init(&al);
    p1.k = 1.0;
    p2.k = 2.0;
    p3.k = 3.0;
    for (i = 0; i < 200; i = i + 1)
    {
        j89a_json *t;
        j89a_json *rt;
        j89a_json *mid;
        j89a_json *map_id;
        j89a_json *m2;
        j89a_json *m2b;
        j89a_json *m3;
        j89a_json *m3b;

        t = gen_json(4, &al);
        sprintf(label, "random %d embed/project", i);
        j89a_json_project(t, &sh);
        rt = j89a_json_embed(&sh, &al);
        expect_eq(label, t, rt);
        j89a_json_release(rt);

        sprintf(label, "random %d map identity", i);
        map_id = j89a_json_map(t, num_id, str_id, NULL, &al);
        expect_eq(label, t, map_id);
        j89a_json_release(map_id);

        /* (map+1) then (map+2) equals map+3 */
        mid = j89a_json_map(t, num_plus, str_id, &p1, &al);
        m2 = j89a_json_map(mid, num_plus, str_id, &p2, &al);
        j89a_json_release(mid);
        m2b = j89a_json_map(t, num_plus, str_id, &p2, &al);
        m3 = j89a_json_map(m2b, num_plus, str_id, &p1, &al);
        j89a_json_release(m2b);
        m3b = j89a_json_map(t, num_plus, str_id, &p3, &al);
        sprintf(label, "random %d map composition", i);
        expect_eq(label, m2, m3b);
        j89a_json_release(m2);
        j89a_json_release(m3);
        j89a_json_release(m3b);

        j89a_json_release(t);
    }
}

/* ---- hardening checks: overflow, failure ABI, monoid laws, multiplicity ----
 */

typedef struct fal
{
    j89a_alloc al;
    unsigned long count;
    unsigned long fail_at;
} fal;

static void *fa_alloc(void *c, size_t n)
{
    fal *f;
    f = (fal *)c;
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

static void fal_init(fal *f, unsigned long at)
{
    f->al.alloc = fa_alloc;
    f->al.realloc = fa_realloc;
    f->al.free = fa_free;
    f->al.ctx = f;
    f->al.failed = 0;
    f->count = 0;
    f->fail_at = at;
}

static j89a_object *mem_obj(const char *kn, double d, j89a_alloc *al)
{
    j89a_str *k;
    j89a_json *v;
    j89a_object *m;
    k = skey(kn, al);
    v = vnum(d, al);
    m = j89a_object_member(k, v, al);
    j89a_str_release(k);
    j89a_json_release(v);
    return m;
}

static j89a_status cm_member(void *c, const j89a_str *name,
                             const j89a_json *value, void *out)
{
    (void)c;
    (void)name;
    (void)value;
    *(unsigned long *)out = 1;
    return J89A_OK;
}

static j89a_status cm_combine(void *c, const void *l, const void *r, void *out)
{
    (void)c;
    *(unsigned long *)out =
        *(const unsigned long *)l + *(const unsigned long *)r;
    return J89A_OK;
}

static unsigned long obj_count(j89a_object *o, j89a_alloc *al)
{
    j89a_rtype rt;
    unsigned long zero;
    unsigned long res;
    zero = 0;
    rt.size = sizeof(unsigned long);
    rt.copy = NULL;
    rt.drop = NULL;
    rt.ctx = NULL;
    if (j89a_object_fold(o, &rt, &zero, cm_member, cm_combine, NULL, &res,
                         al) != J89A_OK)
    {
        return 0;
    }
    return res;
}

static j89a_object *obj_union2(j89a_object *l, j89a_object *r, j89a_alloc *al)
{
    j89a_object *u;
    u = j89a_object_union(l, r, al);
    j89a_object_release(l);
    j89a_object_release(r);
    return u;
}

static j89a_object *obj_union3(j89a_object *x, j89a_object *y, j89a_object *z,
                               j89a_alloc *al)
{
    j89a_object *u;
    j89a_object *v;
    u = j89a_object_union(x, y, al);
    j89a_object_release(x);
    j89a_object_release(y);
    v = j89a_object_union(u, z, al);
    j89a_object_release(u);
    j89a_object_release(z);
    return v;
}

static j89a_status copy_nomem(void *c, void *dst, const void *src)
{
    (void)c;
    (void)dst;
    (void)src;
    return J89A_NOMEM;
}

static j89a_status memb_garbage(void *c, const j89a_str *name,
                                const j89a_json *value, void *out)
{
    (void)c;
    (void)name;
    (void)value;
    *(unsigned long *)out = 0xDEADBEEFUL;
    return J89A_CALLBACK;
}

static j89a_json *arr_of(const double *d, size_t n, j89a_alloc *al)
{
    j89a_list *xs;
    j89a_json *j;
    xs = num_list(d, n, al);
    j = j89a_json_array(xs, al);
    j89a_list_release(xs);
    return j;
}

static void run_hardening(void)
{
    j89a_alloc al;
    j89a_str *s;
    j89a_object *emp;
    j89a_object *a;
    j89a_object *b;
    j89a_object *c;
    j89a_object *u;
    j89a_rtype rt;
    unsigned long zero;
    unsigned long res;
    j89a_list *xs;
    fal f;
    static const double l1[] = {1.0};
    static const double l2[] = {2.0, 3.0};
    static const double l3[] = {4.0};
    static const double big[] = {1.0, 2.0, 3.0};
    j89a_json *ja;
    j89a_json *jb;
    j89a_json *jc;
    plus_ctx p1;
    plus_ctx p2;
    plus_ctx p3;

    j89a_alloc_init(&al);
    zero = 0;

    /* overflow-sized string must fail, not wrap. */
    al.failed = 0;
    s = j89a_str_new(NULL, (size_t)-1, &al);
    if (s != NULL || !al.failed)
    {
        fail("overflow string not rejected");
    }

    /* rtype.copy returning NOMEM propagates as NOMEM, not CALLBACK. */
    rt.size = sizeof(unsigned long);
    rt.copy = copy_nomem;
    rt.drop = NULL;
    rt.ctx = NULL;
    emp = j89a_object_empty(&al);
    res = 0;
    if (j89a_object_fold(emp, &rt, &zero, cm_member, cm_combine, NULL, &res,
                         &al) != J89A_NOMEM)
    {
        fail("rtype.copy NOMEM was not propagated");
    }
    j89a_object_release(emp);

    /* callback failing after writing garbage is transactional (no crash). */
    rt.copy = NULL;
    a = mem_obj("a", 1.0, &al);
    res = 0;
    if (j89a_object_fold(a, &rt, &zero, memb_garbage, cm_combine, NULL, &res,
                         &al) != J89A_CALLBACK)
    {
        fail("transactional garbage callback not propagated");
    }
    j89a_object_release(a);

    /* NULL never denotes NIL: OOM empty list reports failure. */
    fal_init(&f, 1);
    xs = j89a_list_nil(&f.al);
    if (xs != NULL || !f.al.failed)
    {
        fail("nil OOM did not report failure");
    }

    /* object union: left identity. */
    a = mem_obj("a", 1.0, &al);
    emp = j89a_object_empty(&al);
    u = obj_union2(emp, a, &al);
    if (obj_count(u, &al) != 1)
    {
        fail("object union identity count");
    }
    j89a_object_release(u);

    /* object union: commutativity + multiplicity (2 occurrences of a:1). */
    a = mem_obj("a", 1.0, &al);
    b = mem_obj("a", 1.0, &al);
    u = obj_union2(a, b, &al);
    if (obj_count(u, &al) != 2)
    {
        fail("object union multiplicity not preserved");
    }
    j89a_object_release(u);

    a = mem_obj("a", 1.0, &al);
    b = mem_obj("b", 2.0, &al);
    u = j89a_object_union(a, b, &al);
    j89a_object_release(a);
    j89a_object_release(b);
    if (obj_count(u, &al) != 2)
    {
        fail("object union commutative members not counted");
    }
    j89a_object_release(u);

    /* object union: associativity and fold invariance under rebracketing. */
    u = obj_union3(mem_obj("a", 1.0, &al), mem_obj("a", 2.0, &al),
                   mem_obj("b", 3.0, &al), &al);
    a = obj_union2(mem_obj("a", 1.0, &al), mem_obj("b", 3.0, &al), &al);
    c = mem_obj("a", 2.0, &al);
    {
        j89a_object *v;
        v = j89a_object_union(a, c, &al);
        j89a_object_release(a);
        j89a_object_release(c);
        if (obj_count(u, &al) != obj_count(v, &al))
        {
            fail("object fold not invariant under rebracketing");
        }
        j89a_object_release(v);
    }
    j89a_object_release(u);

    /* list concat identity + associativity (json-level oracle). */
    ja = arr_of(l1, 1, &al);
    {
        j89a_list *x;
        j89a_list *nil;
        j89a_list *y;
        x = num_list(l1, 1, &al);
        nil = j89a_list_nil(&al);
        y = j89a_list_concat(nil, x, &al);
        j89a_list_release(nil);
        j89a_list_release(x);
        jb = j89a_json_array(y, &al);
        j89a_list_release(y);
        expect_eq("list concat left identity", ja, jb);
        j89a_json_release(jb);
    }
    j89a_json_release(ja);

    ja = arr_of(l1, 1, &al);
    {
        j89a_list *l;
        j89a_list *m;
        j89a_list *r;
        j89a_list *left;
        j89a_list *right;
        j89a_list *res1;
        j89a_list *res2;
        l = num_list(l1, 1, &al);
        m = num_list(l2, 2, &al);
        r = num_list(l3, 1, &al);
        left = j89a_list_concat(l, m, &al);
        j89a_list_release(l);
        j89a_list_release(m);
        res1 = j89a_list_concat(left, r, &al);
        j89a_list_release(left);
        j89a_list_release(r);
        l = num_list(l1, 1, &al);
        m = num_list(l2, 2, &al);
        r = num_list(l3, 1, &al);
        right = j89a_list_concat(m, r, &al);
        j89a_list_release(m);
        j89a_list_release(r);
        res2 = j89a_list_concat(l, right, &al);
        j89a_list_release(l);
        j89a_list_release(right);
        jb = j89a_json_array(res1, &al);
        j89a_list_release(res1);
        jc = j89a_json_array(res2, &al);
        j89a_list_release(res2);
        expect_eq("list concat associativity", jb, jc);
        j89a_json_release(jb);
        j89a_json_release(jc);
    }
    j89a_json_release(ja);

    /* json.map identity + composition on an array of numbers. */
    ja = arr_of(big, 3, &al);
    p1.k = 1.0;
    p2.k = 2.0;
    p3.k = 3.0;
    {
        j89a_json *id;
        j89a_json *step;
        j89a_json *comp;
        j89a_json *one;
        id = j89a_json_map(ja, num_id, str_id, NULL, &al);
        expect_eq("list map identity", ja, id);
        j89a_json_release(id);
        step = j89a_json_map(ja, num_plus, str_id, &p1, &al);
        comp = j89a_json_map(step, num_plus, str_id, &p2, &al);
        j89a_json_release(step);
        one = j89a_json_map(ja, num_plus, str_id, &p3, &al);
        expect_eq("json.map composition", comp, one);
        j89a_json_release(comp);
        j89a_json_release(one);
    }
    j89a_json_release(ja);
}

int main(void)
{
    j89a_alloc al;
    static const double first[] = {1.0, 2.0};
    j89a_json *obj;
    j89a_json *other;
    j89a_json *rt;
    j89a_json *map_id;
    j89a_json *mapped_a;
    j89a_json *mapped_b;
    j89a_json *mapped_c;
    j89a_shape sh;
    plus_ctx p1;
    plus_ctx p2;

    j89a_alloc_init(&al);

    obj = obj_with_dup(first, 2, "x", 3.0, "y", 4.0, &al);
    other = obj_with_dup(first, 2, "x", 3.0, "y", 4.0, &al);

    /* determinism of canonical builder */
    expect_eq("canonical determinism", obj, other);
    j89a_json_release(other);

    /* embed/project round trip */
    j89a_json_project(obj, &sh);
    rt = j89a_json_embed(&sh, &al);
    expect_eq("embed/project round trip", obj, rt);
    j89a_json_release(rt);

    /* json.map identity */
    map_id = j89a_json_map(obj, num_id, str_id, NULL, &al);
    expect_eq("json.map identity", obj, map_id);
    j89a_json_release(map_id);

    /* json.map composition: (+1 then +2) == (+3) */
    p1.k = 1.0;
    p2.k = 2.0;
    mapped_c = j89a_json_map(obj, num_plus, str_id, &p1, &al);
    mapped_a = j89a_json_map(mapped_c, num_plus, str_id, &p2, &al);
    j89a_json_release(mapped_c);
    mapped_b = j89a_json_map(obj, num_plus, str_id, &p2, &al);
    mapped_c = j89a_json_map(mapped_b, num_plus, str_id, &p1, &al);
    j89a_json_release(mapped_b);
    expect_eq("json.map composition", mapped_a, mapped_c);
    j89a_json_release(mapped_a);
    j89a_json_release(mapped_c);

    j89a_json_release(obj);

    run_random();
    run_hardening();

    if (failures != 0)
    {
        fprintf(stderr, "%d law test(s) FAILED\n", failures);
        return 1;
    }
    printf("core laws ok\n");
    return 0;
}
