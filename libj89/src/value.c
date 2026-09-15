/* value.c - arena and node-tree storage for libj89. */
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "u89.h"

/* Record msg in the arena, truncated to the error buffer. */
void j89_set_error(j89_arena *a, const char *msg)
{
    size_t cap;
    size_t n;
    cap = J89_ERR_LEN - 1;
    n = strlen(msg);
    if (n > cap)
    {
        n = cap;
    }
    memcpy(a->err, msg, n);
    a->err[n] = '\0';
}

/* Record msg, mark the arena failed, and return J89_BAD. */
static j89_len j89_bad_utf8(j89_arena *a, const char *msg)
{
    j89_set_error(a, msg);
    a->failed = 1;
    return J89_BAD;
}

int j89_is_bad(j89_len x)
{
    j89_len s;
    int r;
    s = (j89_len)-1;
    r = (x == s);
    return r;
}

static j89_len j89_next_cap(j89_len cur, j89_len need)
{
    j89_len nc;
    nc = cur;
    if (nc == 0)
    {
        nc = 256;
    }
    for (; nc < need; nc = nc * 2)
    {
    }
    return nc;
}

/* Initialize the string-registry head. */
static void j89_init_head(j89_arena *a)
{
    a->strhead = J89_BAD;
}

static int j89_grow(j89_arena *a, j89_len need)
{
    j89_len cur;
    j89_len nc;
    void *m;
    void *np;
    int nnull;
    cur = a->cap;
    nc = j89_next_cap(cur, need);
    m = a->mem;
    np = realloc(m, nc);
    nnull = !np;
    if (nnull)
    {
        a->failed = 1;
        return 0;
    }
    a->mem = np;
    a->cap = nc;
    return 1;
}

void j89_arena_init(j89_arena *a)
{
    void *z;
    z = NULL;
    a->mem = z;
    a->cap = 0;
    a->off = 0;
    a->err[0] = '\0';
    a->failed = 0;
    j89_init_head(a);
}

void j89_arena_destroy(j89_arena *a)
{
    void *m;
    void *z;
    j89_str_release_all(a);
    m = a->mem;
    free(m);
    z = NULL;
    a->mem = z;
    a->cap = 0;
    a->off = 0;
    j89_init_head(a);
}

const char *j89_error(j89_arena *a)
{
    const char *e;
    e = a->err;
    return e;
}

int j89_failed(j89_arena *a)
{
    int f;
    f = a->failed;
    return f;
}

static j89_len j89_align_up(j89_len off)
{
    j89_len r;
    j89_len adj;
    r = off % J89_ALIGN;
    adj = J89_ALIGN - r;
    adj = adj % J89_ALIGN;
    off = off + adj;
    return off;
}

j89_len j89_alloc(j89_arena *a, j89_len size)
{
    j89_len o;
    j89_len need;
    j89_len off;
    j89_len cap;
    int g;
    off = a->off;
    o = j89_align_up(off);
    need = o + size;
    cap = a->cap;
    if (need > cap)
    {
        g = j89_grow(a, need);
        if (!g)
        {
            return J89_BAD;
        }
    }
    a->off = need;
    return o;
}

void *j89_ptr(j89_arena *a, j89_len off)
{
    void *m;
    char *q;
    char *r;
    m = a->mem;
    q = (char *)m;
    r = q + off;
    return (void *)r;
}

/* Registry node: an owned str89 linked into the arena block. */
struct j89_strnode
{
    j89_len next;
    str89 s;
};

j89_len j89_str_register(j89_arena *a, const str89 *s)
{
    j89_len off;
    j89_len head;
    void *vp;
    struct j89_strnode *node;
    int bad;
    off = j89_alloc(a, sizeof(struct j89_strnode));
    bad = j89_is_bad(off);
    if (bad)
    {
        return J89_BAD;
    }
    head = a->strhead;
    vp = j89_ptr(a, off);
    node = (struct j89_strnode *)vp;
    node->next = head;
    node->s = *s;
    a->strhead = off;
    return off;
}

const str89 *j89_str_at(j89_arena *a, j89_len off)
{
    void *vp;
    struct j89_strnode *node;
    vp = j89_ptr(a, off);
    node = (struct j89_strnode *)vp;
    return &node->s;
}

const char *j89_str_bytes(j89_arena *a, j89_len off)
{
    const str89 *s;
    s = j89_str_at(a, off);
    return (const char *)s->data;
}

j89_len j89_str_len(j89_arena *a, j89_len off)
{
    const str89 *s;
    s = j89_str_at(a, off);
    return s->len;
}

/* Release the str89 held by one registry node and return the next node. */
static j89_len j89_release_one(j89_arena *a, j89_len cur)
{
    void *vp;
    struct j89_strnode *node;
    j89_len next;
    vp = j89_ptr(a, cur);
    node = (struct j89_strnode *)vp;
    next = node->next;
    str89_free(&node->s, NULL);
    return next;
}

void j89_str_release_all(j89_arena *a)
{
    j89_len cur;
    cur = a->strhead;
    while (cur != J89_BAD)
    {
        cur = j89_release_one(a, cur);
    }
    a->strhead = J89_BAD;
}

static struct j89_node *j89_node_ptr(j89_arena *a, j89_len node)
{
    void *p;
    struct j89_node *n;
    p = j89_ptr(a, node);
    n = (struct j89_node *)p;
    return n;
}

j89_kind j89_node_kind(j89_arena *a, j89_len node)
{
    struct j89_node *n;
    j89_kind k;
    n = j89_node_ptr(a, node);
    k = n->kind;
    return k;
}

j89_len j89_node_n(j89_arena *a, j89_len node)
{
    struct j89_node *n;
    j89_len v;
    n = j89_node_ptr(a, node);
    v = n->n;
    return v;
}

j89_len j89_node_base(j89_arena *a, j89_len node)
{
    struct j89_node *n;
    j89_len v;
    n = j89_node_ptr(a, node);
    v = n->base;
    return v;
}

j89_int j89_node_iv(j89_arena *a, j89_len node)
{
    struct j89_node *n;
    j89_int v;
    n = j89_node_ptr(a, node);
    v = n->iv;
    return v;
}

double j89_node_dv(j89_arena *a, j89_len node)
{
    struct j89_node *n;
    double v;
    n = j89_node_ptr(a, node);
    v = n->dv;
    return v;
}

void j89_node_set_kind(j89_arena *a, j89_len node, j89_kind kind)
{
    struct j89_node *n;
    n = j89_node_ptr(a, node);
    n->kind = kind;
}

void j89_node_set_n(j89_arena *a, j89_len node, j89_len n)
{
    struct j89_node *p;
    p = j89_node_ptr(a, node);
    p->n = n;
}

void j89_node_set_base(j89_arena *a, j89_len node, j89_len base)
{
    struct j89_node *p;
    p = j89_node_ptr(a, node);
    p->base = base;
}

void j89_node_set_iv(j89_arena *a, j89_len node, j89_int iv)
{
    struct j89_node *p;
    p = j89_node_ptr(a, node);
    p->iv = iv;
}

void j89_node_set_dv(j89_arena *a, j89_len node, double dv)
{
    struct j89_node *p;
    p = j89_node_ptr(a, node);
    p->dv = dv;
}

j89_len j89_new_node(j89_arena *a)
{
    j89_len off;
    int bad;
    off = j89_alloc(a, sizeof(struct j89_node));
    bad = j89_is_bad(off);
    if (bad)
    {
        return J89_BAD;
    }
    return off;
}

/* Build an owned, NUL-terminated str89 from already validated bytes. */
static int j89_build_owned(const char *s, j89_len len, str89 *owned)
{
    str89_buf b;
    str89_view v;
    int r;
    str89_buf_init(&b);
    if (len == J89_BAD)
    {
        return STR89_ERANGE;
    }
    r = str89_buf_reserve(&b, NULL, len + 1);
    if (r != STR89_OK)
    {
        str89_buf_free(&b, NULL);
        return r;
    }
    v.data = (const unsigned char *)s;
    v.len = len;
    r = str89_buf_append(&b, NULL, v);
    if (r != STR89_OK)
    {
        str89_buf_free(&b, NULL);
        return r;
    }
    b.data[b.len] = '\0';
    str89_init(owned);
    r = str89_take(owned, &b);
    if (r != STR89_OK)
    {
        str89_buf_free(&b, NULL);
        return r;
    }
    return STR89_OK;
}

/* Create a string node whose base is the registry offset of owned. On failure
 * the caller still owns *owned; on success the arena owns it. */
j89_len j89_string_node(j89_arena *a, const str89 *owned)
{
    j89_len node;
    j89_len off;
    int bad;
    node = j89_new_node(a);
    bad = j89_is_bad(node);
    if (bad)
    {
        return J89_BAD;
    }
    off = j89_str_register(a, owned);
    bad = j89_is_bad(off);
    if (bad)
    {
        return J89_BAD;
    }
    j89_node_set_kind(a, node, J89_STRING);
    j89_node_set_base(a, node, off);
    j89_node_set_n(a, node, owned->len);
    return node;
}

/* Free a string the arena did not adopt, mark the arena failed, and return
 * the bad node sentinel. */
static j89_len j89_string_fail(j89_arena *a, str89 *owned)
{
    str89_free(owned, NULL);
    a->failed = 1;
    return J89_BAD;
}

j89_len j89_add_string(j89_arena *a, const char *s, j89_len len)
{
    str89 owned;
    j89_len node;
    int bad;
    int r;
    r = j89_build_owned(s, len, &owned);
    if (r != STR89_OK)
    {
        a->failed = 1;
        return J89_BAD;
    }
    node = j89_string_node(a, &owned);
    bad = j89_is_bad(node);
    if (bad)
    {
        node = j89_string_fail(a, &owned);
        return node;
    }
    return node;
}

j89_len j89_add_int(j89_arena *a, j89_int v)
{
    j89_len node;
    int bad;
    node = j89_new_node(a);
    bad = j89_is_bad(node);
    if (bad)
    {
        return J89_BAD;
    }
    j89_node_set_kind(a, node, J89_INTEGER);
    j89_node_set_iv(a, node, v);
    return node;
}

j89_len j89_add_double(j89_arena *a, double v)
{
    j89_len node;
    int bad;
    node = j89_new_node(a);
    bad = j89_is_bad(node);
    if (bad)
    {
        return J89_BAD;
    }
    j89_node_set_kind(a, node, J89_FLOAT);
    j89_node_set_dv(a, node, v);
    return node;
}

static j89_len j89_slot_offset(j89_arena *a, j89_len count, j89_len stride)
{
    j89_len slots;
    j89_len off;
    slots = count * stride;
    off = 0;
    if (count != 0)
    {
        off = j89_alloc(a, slots);
    }
    return off;
}

j89_len j89_make_array(j89_arena *a, j89_len count)
{
    j89_len off;
    j89_len node;
    int bad;
    off = j89_slot_offset(a, count, sizeof(j89_len));
    bad = j89_is_bad(off);
    if (bad)
    {
        return J89_BAD;
    }
    node = j89_new_node(a);
    bad = j89_is_bad(node);
    if (bad)
    {
        return J89_BAD;
    }
    j89_node_set_kind(a, node, J89_ARRAY);
    j89_node_set_base(a, node, off);
    j89_node_set_n(a, node, count);
    return node;
}

j89_len j89_make_object(j89_arena *a, j89_len count)
{
    j89_len off;
    j89_len node;
    int bad;
    off = j89_slot_offset(a, count, sizeof(struct j89_member));
    bad = j89_is_bad(off);
    if (bad)
    {
        return J89_BAD;
    }
    node = j89_new_node(a);
    bad = j89_is_bad(node);
    if (bad)
    {
        return J89_BAD;
    }
    j89_node_set_kind(a, node, J89_OBJECT);
    j89_node_set_base(a, node, off);
    j89_node_set_n(a, node, count);
    return node;
}

void j89_array_set_child(j89_arena *a, j89_len arr, j89_len i, j89_len child)
{
    j89_len base;
    void *v;
    j89_len *p;
    j89_len *q;
    base = j89_node_base(a, arr);
    v = j89_ptr(a, base);
    p = (j89_len *)v;
    q = p + i;
    *q = child;
}

void j89_object_set_member(j89_arena *a, j89_len obj, j89_len i, j89_len ko,
                           j89_len kl, j89_len value)
{
    j89_len base;
    void *v;
    struct j89_member *m;
    struct j89_member *d;
    base = j89_node_base(a, obj);
    v = j89_ptr(a, base);
    m = (struct j89_member *)v;
    d = m + i;
    d->ko = ko;
    d->kl = kl;
    d->value = value;
}

j89_len j89_child_id(j89_arena *a, j89_len base, j89_len i)
{
    void *m;
    j89_len *p;
    j89_len *q;
    j89_len v;
    m = j89_ptr(a, base);
    p = (j89_len *)m;
    q = p + i;
    v = *q;
    return v;
}

j89_len j89_member_ko(j89_arena *a, j89_len base, j89_len i)
{
    void *p;
    struct j89_member *m;
    struct j89_member *d;
    j89_len v;
    p = j89_ptr(a, base);
    m = (struct j89_member *)p;
    d = m + i;
    v = d->ko;
    return v;
}

j89_len j89_member_kl(j89_arena *a, j89_len base, j89_len i)
{
    void *p;
    struct j89_member *m;
    struct j89_member *d;
    j89_len v;
    p = j89_ptr(a, base);
    m = (struct j89_member *)p;
    d = m + i;
    v = d->kl;
    return v;
}

j89_len j89_member_value(j89_arena *a, j89_len base, j89_len i)
{
    void *p;
    struct j89_member *m;
    struct j89_member *d;
    j89_len v;
    p = j89_ptr(a, base);
    m = (struct j89_member *)p;
    d = m + i;
    v = d->value;
    return v;
}

/* --- public node accessors --- */

j89_kind j89_kind_of(j89_arena *a, j89_len node)
{
    j89_kind k;
    k = j89_node_kind(a, node);
    return k;
}

j89_int j89_int_value(j89_arena *a, j89_len node)
{
    j89_int v;
    v = j89_node_iv(a, node);
    return v;
}

double j89_double_value(j89_arena *a, j89_len node)
{
    double v;
    v = j89_node_dv(a, node);
    return v;
}

const char *j89_string_value(j89_arena *a, j89_len node)
{
    j89_len base;
    const char *s;
    base = j89_node_base(a, node);
    s = j89_str_bytes(a, base);
    return s;
}

j89_len j89_string_length(j89_arena *a, j89_len node)
{
    j89_len base;
    j89_len v;
    base = j89_node_base(a, node);
    v = j89_str_len(a, base);
    return v;
}

j89_len j89_array_length(j89_arena *a, j89_len node)
{
    j89_len v;
    v = j89_node_n(a, node);
    return v;
}

j89_len j89_array_get(j89_arena *a, j89_len node, j89_len i)
{
    j89_len base;
    j89_len v;
    base = j89_node_base(a, node);
    v = j89_child_id(a, base, i);
    return v;
}

j89_len j89_object_length(j89_arena *a, j89_len node)
{
    j89_len v;
    v = j89_node_n(a, node);
    return v;
}

const char *j89_object_key(j89_arena *a, j89_len node, j89_len i)
{
    j89_len base;
    j89_len ko;
    const char *s;
    base = j89_node_base(a, node);
    ko = j89_member_ko(a, base, i);
    s = j89_str_bytes(a, ko);
    return s;
}

j89_len j89_object_key_length(j89_arena *a, j89_len node, j89_len i)
{
    j89_len base;
    j89_len ko;
    j89_len v;
    base = j89_node_base(a, node);
    ko = j89_member_ko(a, base, i);
    v = j89_str_len(a, ko);
    return v;
}

j89_len j89_object_value(j89_arena *a, j89_len node, j89_len i)
{
    j89_len base;
    j89_len v;
    base = j89_node_base(a, node);
    v = j89_member_value(a, base, i);
    return v;
}

static int j89_str_eq(j89_arena *a, j89_len ko, j89_len kl, const char *b)
{
    const char *ks;
    size_t blen;
    int eq;
    ks = j89_str_bytes(a, ko);
    blen = strlen(b);
    if (blen != kl)
    {
        return 0;
    }
    if (kl == 0)
    {
        return 1;
    }
    eq = memcmp(ks, b, kl);
    if (eq == 0)
    {
        return 1;
    }
    return 0;
}

static j89_len j89_match_value(j89_arena *a, j89_len base, j89_len i,
                               const char *key, int *ismatch)
{
    j89_len ko;
    j89_len kl;
    int eq;
    j89_len val;
    val = J89_BAD;
    ko = j89_member_ko(a, base, i);
    kl = j89_member_kl(a, base, i);
    eq = j89_str_eq(a, ko, kl, key);
    *ismatch = eq;
    if (eq)
    {
        val = j89_member_value(a, base, i);
    }
    return val;
}

j89_len j89_object_find(j89_arena *a, j89_len node, const char *key)
{
    j89_len base;
    j89_len count;
    j89_len i;
    base = j89_node_base(a, node);
    count = j89_node_n(a, node);
    for (i = 0; i < count; i = i + 1)
    {
        int m;
        j89_len v;
        v = j89_match_value(a, base, i, key, &m);
        if (m)
        {
            return v;
        }
    }
    return J89_BAD;
}

/* --- public builder API --- */

j89_len j89_object_new(j89_arena *a, j89_len count)
{
    j89_len node;
    node = j89_make_object(a, count);
    return node;
}

j89_len j89_array_new(j89_arena *a, j89_len count)
{
    j89_len node;
    node = j89_make_array(a, count);
    return node;
}

j89_len j89_string_new(j89_arena *a, const char *bytes, j89_len len)
{
    j89_len node;
    str89_view v;
    int ok;
    ok = str89_view_init(&v, (const unsigned char *)bytes, len);
    if (ok != STR89_OK)
    {
        node = j89_bad_utf8(a, "invalid UTF-8 in string");
        return node;
    }
    node = j89_add_string(a, bytes, len);
    return node;
}

j89_len j89_integer_new(j89_arena *a, j89_int v)
{
    j89_len node;
    node = j89_add_int(a, v);
    return node;
}

j89_len j89_double_new(j89_arena *a, double v)
{
    j89_len node;
    node = j89_add_double(a, v);
    return node;
}

j89_len j89_bool_new(j89_arena *a, int v)
{
    j89_len node;
    j89_kind k;
    if (v)
    {
        k = J89_TRUE;
    }
    else
    {
        k = J89_FALSE;
    }
    node = j89_new_node(a);
    j89_node_set_kind(a, node, k);
    return node;
}

j89_len j89_null_new(j89_arena *a)
{
    j89_len node;
    int bad;
    node = j89_new_node(a);
    bad = j89_is_bad(node);
    if (bad)
    {
        return J89_BAD;
    }
    j89_node_set_kind(a, node, J89_NULL);
    return node;
}

void j89_array_set(j89_arena *a, j89_len array, j89_len index, j89_len child)
{
    int badarray;
    int badchild;
    badarray = j89_is_bad(array);
    badchild = j89_is_bad(child);
    if (badarray)
    {
        a->failed = 1;
        return;
    }
    if (badchild)
    {
        a->failed = 1;
        return;
    }
    j89_array_set_child(a, array, index, child);
}

void j89_object_set(j89_arena *a, j89_len object, j89_len index,
                    const char *key, j89_len keylen, j89_len value)
{
    j89_len ko;
    j89_len keynode;
    str89_view kv;
    int ok;
    int badobj;
    int badval;
    int badkeynode;
    badobj = j89_is_bad(object);
    badval = j89_is_bad(value);
    if (badobj)
    {
        a->failed = 1;
        return;
    }
    if (badval)
    {
        a->failed = 1;
        return;
    }
    ok = str89_view_init(&kv, (const unsigned char *)key, keylen);
    if (ok != STR89_OK)
    {
        j89_bad_utf8(a, "invalid UTF-8 in object key");
        return;
    }
    keynode = j89_add_string(a, key, keylen);
    badkeynode = j89_is_bad(keynode);
    if (badkeynode)
    {
        a->failed = 1;
        return;
    }
    ko = j89_node_base(a, keynode);
    j89_object_set_member(a, object, index, ko, keylen, value);
}
