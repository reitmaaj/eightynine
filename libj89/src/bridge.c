/* bridge.c - import the practical j89 document model into j89_alg. */
#include "j89_alg_bridge.h"

static j89a_status convert_node(j89_arena *src, j89_len node, j89a_json **out,
                                j89a_alloc *al);

static j89a_status leaf_null(j89a_alloc *al, j89a_json **out)
{
    j89a_json *v;
    v = j89a_json_null(al);
    if (v == NULL)
    {
        return J89A_NOMEM;
    }
    *out = v;
    return J89A_OK;
}

static j89a_status leaf_bool(j89a_bool b, j89a_alloc *al, j89a_json **out)
{
    j89a_json *v;
    v = j89a_json_boolean(b, al);
    if (v == NULL)
    {
        return J89A_NOMEM;
    }
    *out = v;
    return J89A_OK;
}

static j89a_status leaf_number(double d, j89a_alloc *al, j89a_json **out)
{
    j89a_json *v;
    v = j89a_json_number(d, al);
    if (v == NULL)
    {
        return J89A_NOMEM;
    }
    *out = v;
    return J89A_OK;
}

static j89a_status leaf_string(j89_arena *src, j89_len node, j89a_alloc *al,
                               j89a_json **out)
{
    const char *bytes;
    j89_len n;
    j89a_str *s;
    j89a_json *v;
    bytes = j89_string_value(src, node);
    n = j89_string_length(src, node);
    s = j89a_str_new(bytes, n, al);
    if (s == NULL)
    {
        return J89A_NOMEM;
    }
    v = j89a_json_string(s, al);
    j89a_str_release(s);
    if (v == NULL)
    {
        return J89A_NOMEM;
    }
    *out = v;
    return J89A_OK;
}

static j89a_status scalar_integer(j89_arena *src, j89_len node, j89a_alloc *al,
                                  j89a_json **out)
{
    double d;
    j89a_status st;
    d = j89_int_value(src, node);
    st = leaf_number(d, al, out);
    return st;
}

static j89a_status scalar_float(j89_arena *src, j89_len node, j89a_alloc *al,
                                j89a_json **out)
{
    double d;
    j89a_status st;
    d = j89_double_value(src, node);
    st = leaf_number(d, al, out);
    return st;
}

static j89a_status convert_scalar(j89_arena *src, j89_len node, j89a_json **out,
                                  j89a_alloc *al)
{
    j89_kind kind;
    j89a_status st;
    kind = j89_kind_of(src, node);
    if (kind == J89_NULL)
    {
        st = leaf_null(al, out);
        return st;
    }
    if (kind == J89_FALSE)
    {
        st = leaf_bool(J89A_FALSE, al, out);
        return st;
    }
    if (kind == J89_TRUE)
    {
        st = leaf_bool(J89A_TRUE, al, out);
        return st;
    }
    if (kind == J89_INTEGER)
    {
        st = scalar_integer(src, node, al, out);
        return st;
    }
    if (kind == J89_FLOAT)
    {
        st = scalar_float(src, node, al, out);
        return st;
    }
    st = leaf_string(src, node, al, out);
    return st;
}

/* Convert one array element (at index *i - 1) and prepend it to *acc. */
static j89a_status array_step(j89_arena *src, j89_len node, j89a_list **acc,
                              j89_len *i, j89a_alloc *al)
{
    j89_len idx;
    j89_len child;
    j89a_json *cv;
    j89a_list *result;
    j89a_status st;
    idx = *i - 1;
    child = j89_array_get(src, node, idx);
    st = convert_node(src, child, &cv, al);
    if (st != J89A_OK)
    {
        return st;
    }
    result = j89a_list_cons(cv, *acc, al);
    j89a_json_release(cv);
    if (result == NULL)
    {
        return J89A_NOMEM;
    }
    j89a_list_release(*acc);
    *acc = result;
    *i = idx;
    return J89A_OK;
}

static j89a_status convert_array(j89_arena *src, j89_len node, j89a_json **out,
                                 j89a_alloc *al)
{
    j89a_list *xs;
    j89a_json *v;
    j89_len n;
    j89_len i;
    j89a_status st;
    xs = j89a_list_nil(al);
    if (xs == NULL)
    {
        return J89A_NOMEM;
    }
    n = j89_array_length(src, node);
    i = n;
    while (i > 0)
    {
        st = array_step(src, node, &xs, &i, al);
        if (st != J89A_OK)
        {
            j89a_list_release(xs);
            return st;
        }
    }
    v = j89a_json_array(xs, al);
    j89a_list_release(xs);
    if (v == NULL)
    {
        return J89A_NOMEM;
    }
    *out = v;
    return J89A_OK;
}

/* Convert one object member (at index *ip) and union it into *acc. */
static j89a_status object_step(j89_arena *src, j89_len node, j89_len *ip,
                               j89a_object **acc, j89a_alloc *al)
{
    j89a_json *value;
    j89a_str *key;
    j89a_object *m;
    j89a_object *u;
    const char *k;
    size_t klen;
    j89_len child;
    j89a_status st;
    k = j89_object_key(src, node, *ip);
    klen = j89_object_key_length(src, node, *ip);
    key = j89a_str_new(k, klen, al);
    if (key == NULL)
    {
        return J89A_NOMEM;
    }
    child = j89_object_value(src, node, *ip);
    st = convert_node(src, child, &value, al);
    if (st != J89A_OK)
    {
        j89a_str_release(key);
        return st;
    }
    m = j89a_object_member(key, value, al);
    j89a_str_release(key);
    j89a_json_release(value);
    if (m == NULL)
    {
        return J89A_NOMEM;
    }
    u = j89a_object_union(*acc, m, al);
    j89a_object_release(m);
    if (u == NULL)
    {
        return J89A_NOMEM;
    }
    j89a_object_release(*acc);
    *acc = u;
    *ip = *ip + 1;
    return J89A_OK;
}

static j89a_status convert_object(j89_arena *src, j89_len node, j89a_json **out,
                                  j89a_alloc *al)
{
    j89a_object *ms;
    j89a_json *v;
    j89_len n;
    j89_len i;
    j89a_status st;
    ms = j89a_object_empty(al);
    if (ms == NULL)
    {
        return J89A_NOMEM;
    }
    n = j89_object_length(src, node);
    i = 0;
    while (i < n)
    {
        st = object_step(src, node, &i, &ms, al);
        if (st != J89A_OK)
        {
            j89a_object_release(ms);
            return st;
        }
    }
    v = j89a_json_object(ms, al);
    j89a_object_release(ms);
    if (v == NULL)
    {
        return J89A_NOMEM;
    }
    *out = v;
    return J89A_OK;
}

static j89a_status convert_node(j89_arena *src, j89_len node, j89a_json **out,
                                j89a_alloc *al)
{
    j89_kind kind;
    j89a_status st;
    *out = NULL;
    kind = j89_kind_of(src, node);
    if (kind == J89_ARRAY)
    {
        st = convert_array(src, node, out, al);
        return st;
    }
    if (kind == J89_OBJECT)
    {
        st = convert_object(src, node, out, al);
        return st;
    }
    st = convert_scalar(src, node, out, al);
    return st;
}

j89a_status j89a_import_j89(j89_arena *src, j89_len root, j89a_json **out,
                            j89a_alloc *al)
{
    j89a_status st;
    st = convert_node(src, root, out, al);
    return st;
}
