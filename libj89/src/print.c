/* print.c - JSON rendering for libj89. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

struct j89_writer
{
    j89_arena *a;
    j89_len depth; /* current array/object nesting depth */
};

static int w_reserve(struct j89_writer *w, j89_len need)
{
    j89_arena *a;
    j89_len cap;
    void *np;
    a = w->a;
    cap = a->cap;
    if (need <= cap)
    {
        return 0;
    }
    if (cap == 0)
    {
        cap = 256;
    }
    for (; cap < need; cap = cap * 2)
    {
        ;
    }
    np = realloc(a->mem, cap);
    if (np == NULL)
    {
        a->failed = 1;
        return 1;
    }
    a->mem = np;
    a->cap = cap;
    return 0;
}

static void w_byte(struct j89_writer *w, char c)
{
    j89_arena *a;
    j89_len off;
    j89_len need;
    void *m;
    char *p;
    int st;
    a = w->a;
    off = a->off;
    need = off + 1;
    st = w_reserve(w, need);
    if (st != 0)
    {
        return;
    }
    m = a->mem;
    p = (char *)m;
    p[off] = c;
    a->off = need;
}

static void w_text(struct j89_writer *w, const char *s, j89_len len)
{
    j89_len i;
    for (i = 0; i < len; i = i + 1)
    {
        w_byte(w, s[i]);
    }
}

static char w_hex_digit(unsigned int v, j89_len i)
{
    static const char hex[] = "0123456789abcdef";
    unsigned int ix;
    ix = (v >> (12 - i * 4)) & 0xF;
    return hex[ix];
}

static void w_hex4(struct j89_writer *w, unsigned int v)
{
    char tmp[4];
    tmp[0] = w_hex_digit(v, 0);
    tmp[1] = w_hex_digit(v, 1);
    tmp[2] = w_hex_digit(v, 2);
    tmp[3] = w_hex_digit(v, 3);
    w_text(w, tmp, 4);
}

static void w_escape_hex(struct j89_writer *w, unsigned char uc)
{
    unsigned int u;
    w_text(w, "\\u", 2);
    u = (unsigned int)uc;
    w_hex4(w, u);
}

static void w_escape_char(struct j89_writer *w, char c)
{
    unsigned char uc;
    uc = (unsigned char)c;
    if (c == '"')
    {
        w_text(w, "\\\"", 2);
    }
    else if (c == '\\')
    {
        w_text(w, "\\\\", 2);
    }
    else if (c == '\n')
    {
        w_text(w, "\\n", 2);
    }
    else if (c == '\t')
    {
        w_text(w, "\\t", 2);
    }
    else if (c == '\r')
    {
        w_text(w, "\\r", 2);
    }
    else if (c == '\b')
    {
        w_text(w, "\\b", 2);
    }
    else if (c == '\f')
    {
        w_text(w, "\\f", 2);
    }
    else if (uc < 0x20)
    {
        w_escape_hex(w, uc);
    }
    else
    {
        w_byte(w, c);
    }
}

static void w_escape_string(struct j89_writer *w, const char *s, j89_len len)
{
    j89_len i;
    w_byte(w, '"');
    for (i = 0; i < len; i = i + 1)
    {
        w_escape_char(w, s[i]);
    }
    w_byte(w, '"');
}

static int w_err_char(struct j89_writer *w, const char *msg, j89_len i,
                      j89_len cap)
{
    char m;
    m = msg[i];
    if (m == '\0')
    {
        return 1;
    }
    if (i >= cap)
    {
        return 1;
    }
    w->a->err[i] = m;
    return 0;
}

static void w_set_err(struct j89_writer *w, const char *msg)
{
    j89_len cap;
    j89_len i;
    int st;
    cap = J89_ERR_LEN - 1;
    for (i = 0;; i = i + 1)
    {
        st = w_err_char(w, msg, i, cap);
        if (st != 0)
        {
            break;
        }
    }
    w->a->err[i] = '\0';
}

static int w_unwind_depth(struct j89_writer *w, int r)
{
    j89_len cur;
    j89_len dec;
    cur = w->depth;
    dec = cur - 1;
    w->depth = dec;
    return r;
}

static int w_render_int(struct j89_writer *w, j89_arena *a, j89_len node)
{
    j89_int v;
    char buf[64];
    size_t sl;
    j89_len bl;
    v = j89_node_iv(a, node);
    sprintf(buf, "%.17g", v);
    sl = strlen(buf);
    bl = sl;
    w_text(w, buf, bl);
    return 0;
}

static int w_render_float(struct j89_writer *w, j89_arena *a, j89_len node)
{
    double v;
    char buf[64];
    size_t sl;
    j89_len bl;
    v = j89_node_dv(a, node);
    sprintf(buf, "%.17g", v);
    sl = strlen(buf);
    bl = sl;
    w_text(w, buf, bl);
    return 0;
}

static int w_render_string(struct j89_writer *w, j89_arena *a, j89_len node)
{
    j89_len base;
    j89_len len;
    void *vp;
    const char *s;
    base = j89_node_base(a, node);
    len = j89_node_n(a, node);
    vp = j89_ptr(a, base);
    s = (const char *)vp;
    w_escape_string(w, s, len);
    return 0;
}

static int w_render(struct j89_writer *w, j89_arena *a, j89_len node);

static int w_render_elem(struct j89_writer *w, j89_arena *a, j89_len base,
                         j89_len i)
{
    j89_len child;
    int r;
    if (i != 0)
    {
        w_byte(w, ',');
    }
    child = j89_child_id(a, base, i);
    r = w_render(w, a, child);
    return r;
}

static int w_render_member(struct j89_writer *w, j89_arena *a, j89_len base,
                           j89_len i)
{
    j89_len ko;
    j89_len kl;
    j89_len value;
    void *vp;
    const char *ks;
    int r;
    if (i != 0)
    {
        w_byte(w, ',');
    }
    ko = j89_member_ko(a, base, i);
    kl = j89_member_kl(a, base, i);
    vp = j89_ptr(a, ko);
    ks = (const char *)vp;
    w_escape_string(w, ks, kl);
    w_byte(w, ':');
    value = j89_member_value(a, base, i);
    r = w_render(w, a, value);
    return r;
}

static int w_render_array(struct j89_writer *w, j89_arena *a, j89_len node)
{
    j89_len cur;
    j89_len nd;
    j89_len dec;
    j89_len count;
    j89_len base;
    j89_len i;
    int over;
    cur = w->depth;
    nd = cur + 1;
    over = (nd > J89_MAX_DEPTH);
    if (over)
    {
        return 1;
    }
    w->depth = nd;
    count = j89_node_n(a, node);
    base = j89_node_base(a, node);
    w_byte(w, '[');
    for (i = 0; i < count; i = i + 1)
    {
        int res;
        res = w_render_elem(w, a, base, i);
        if (res != 0)
        {
            int rr;
            rr = w_unwind_depth(w, res);
            return rr;
        }
    }
    w_byte(w, ']');
    cur = w->depth;
    dec = cur - 1;
    w->depth = dec;
    return 0;
}

static int w_render_object(struct j89_writer *w, j89_arena *a, j89_len node)
{
    j89_len cur;
    j89_len nd;
    j89_len dec;
    j89_len count;
    j89_len base;
    j89_len i;
    int over;
    cur = w->depth;
    nd = cur + 1;
    over = (nd > J89_MAX_DEPTH);
    if (over)
    {
        return 1;
    }
    w->depth = nd;
    count = j89_node_n(a, node);
    base = j89_node_base(a, node);
    w_byte(w, '{');
    for (i = 0; i < count; i = i + 1)
    {
        int res;
        res = w_render_member(w, a, base, i);
        if (res != 0)
        {
            int rr;
            rr = w_unwind_depth(w, res);
            return rr;
        }
    }
    w_byte(w, '}');
    cur = w->depth;
    dec = cur - 1;
    w->depth = dec;
    return 0;
}

static int w_render(struct j89_writer *w, j89_arena *a, j89_len node)
{
    j89_kind k;
    int r;
    k = j89_node_kind(a, node);
    if (k == J89_NULL)
    {
        w_text(w, "null", 4);
        return 0;
    }
    else if (k == J89_TRUE)
    {
        w_text(w, "true", 4);
        return 0;
    }
    else if (k == J89_FALSE)
    {
        w_text(w, "false", 5);
        return 0;
    }
    else if (k == J89_INTEGER)
    {
        r = w_render_int(w, a, node);
        return r;
    }
    else if (k == J89_FLOAT)
    {
        r = w_render_float(w, a, node);
        return r;
    }
    else if (k == J89_STRING)
    {
        r = w_render_string(w, a, node);
        return r;
    }
    else if (k == J89_ARRAY)
    {
        r = w_render_array(w, a, node);
        return r;
    }
    else
    {
        r = w_render_object(w, a, node);
        return r;
    }
}

int j89_render(j89_arena *a, j89_len node, int compact, j89_arena *out)
{
    struct j89_writer w;
    int r;
    int failed;
    (void)compact;
    w.a = out;
    w.depth = 0;
    r = w_render(&w, a, node);
    if (r != 0)
    {
        w_set_err(&w, "maximum depth exceeded");
        return -1;
    }
    failed = out->failed;
    if (failed)
    {
        return -1;
    }
    return 0;
}
