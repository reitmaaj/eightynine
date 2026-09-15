/* jrpc89_internal.c - shared helpers. */
#include <jrpc89.h>

#include "id.h"
#include "jrpc89_internal.h"

int jrpc89_has_node(j89_len n)
{
    j89_len bad;
    int eq;
    int r;
    bad = J89_BAD;
    eq = (n == bad);
    r = (eq == 0);
    return r;
}

int jrpc89_arena_dirty(j89_arena *a)
{
    const char *e;
    int failed;
    failed = j89_failed(a);
    if (failed)
    {
        return 1;
    }
    e = j89_error(a);
    if (e[0] != '\0')
    {
        return 1;
    }
    return 0;
}

jrpc89_status jrpc89_status_from_arena(j89_arena *a)
{
    const char *e;
    e = j89_error(a);
    if (e[0] != '\0')
    {
        return JRPC89_EINVAL;
    }
    return JRPC89_ENOMEM;
}

int jrpc89_builder_failed(j89_arena *a)
{
    int failed;
    failed = j89_failed(a);
    return failed;
}

jrpc89_status jrpc89_builder_check(j89_arena *a)
{
    int failed;
    jrpc89_status st;
    failed = jrpc89_builder_failed(a);
    if (failed)
    {
        st = jrpc89_status_from_arena(a);
        return st;
    }
    return JRPC89_OK;
}

int jrpc89_str_is(j89_arena *a, j89_len node, const char *expect,
                  j89_len expect_len)
{
    j89_kind k;
    j89_len len;
    const char *s;
    j89_len i;
    k = j89_kind_of(a, node);
    if (k != J89_STRING)
    {
        return 0;
    }
    len = j89_string_length(a, node);
    if (len != expect_len)
    {
        return 0;
    }
    s = j89_string_value(a, node);
    for (i = 0; i < len; i = i + 1)
    {
        if (s[i] != expect[i])
        {
            return 0;
        }
    }
    return 1;
}

int jrpc89_member_has(j89_arena *a, j89_len node, const char *key)
{
    j89_len v;
    int r;
    v = j89_object_find(a, node, key);
    r = jrpc89_has_node(v);
    return r;
}

static void jrpc89_id_set_int(j89_arena *a, j89_len node, jrpc89_id *out)
{
    out->kind = JRPC89_ID_INT;
    out->num = j89_int_value(a, node);
}

static void jrpc89_id_set_string(j89_arena *a, j89_len node, jrpc89_id *out)
{
    out->kind = JRPC89_ID_STRING;
    out->str = j89_string_value(a, node);
    out->len = j89_string_length(a, node);
}

jrpc89_status jrpc89_id_from_node(j89_arena *a, j89_len node, jrpc89_id *out)
{
    j89_kind k;
    k = j89_kind_of(a, node);
    if (k == J89_INTEGER)
    {
        jrpc89_id_set_int(a, node, out);
        return JRPC89_OK;
    }
    if (k == J89_STRING)
    {
        jrpc89_id_set_string(a, node, out);
        return JRPC89_OK;
    }
    if (k == J89_NULL)
    {
        out->kind = JRPC89_ID_NULL;
        return JRPC89_OK;
    }
    return JRPC89_EPROTO;
}

jrpc89_status jrpc89_check_head(j89_arena *a, j89_len node)
{
    j89_len version;
    j89_kind k;
    int has;
    int ok;
    k = j89_kind_of(a, node);
    if (k != J89_OBJECT)
    {
        return JRPC89_EPROTO;
    }
    version = j89_object_find(a, node, "jsonrpc");
    has = jrpc89_has_node(version);
    if (!has)
    {
        return JRPC89_EPROTO;
    }
    ok = jrpc89_str_is(a, version, "2.0", 3);
    if (!ok)
    {
        return JRPC89_EPROTO;
    }
    return JRPC89_OK;
}

jrpc89_status jrpc89_set_version(j89_arena *a, j89_len obj, j89_len slot)
{
    j89_len ver;
    jrpc89_status st;
    ver = j89_string_new(a, "2.0", 3);
    j89_object_set(a, obj, slot, "jsonrpc", 7, ver);
    st = jrpc89_builder_check(a);
    return st;
}

jrpc89_status jrpc89_set_id_member(j89_arena *a, j89_len obj, j89_len slot,
                                   const jrpc89_id *id)
{
    j89_len idnode;
    jrpc89_status st;
    idnode = jrpc89_id_node(a, id);
    j89_object_set(a, obj, slot, "id", 2, idnode);
    st = jrpc89_builder_check(a);
    return st;
}
