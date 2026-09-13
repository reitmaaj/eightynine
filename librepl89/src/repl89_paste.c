/* repl89_paste.c - bracketed-paste sanitization.
 *
 * Paste is the input-normalization boundary: CRLF and lone CR become LF, HT
 * is preserved, every other Unicode Cc scalar (including raw ESC, DEL, and
 * C1 controls) is discarded, and malformed UTF-8 rejects the entire paste.
 * The result is accumulated out-of-band so the caller can insert it as one
 * atomic editor operation; pasted LF never submits. */

#include <string.h>

#include "repl89_internal.h"
#include "u89.h"

void repl89_paste_init(repl89_paste *p)
{
    repl89_buf_init(&p->out);
    p->partial_len = 0;
    p->pending_cr = 0;
    p->error = REPL89_OK;
}

void repl89_paste_free(repl89_paste *p)
{
    repl89_buf_free(&p->out);
    p->partial_len = 0;
    p->pending_cr = 0;
    p->error = REPL89_OK;
}

void repl89_paste_reset(repl89_paste *p)
{
    repl89_buf_clear(&p->out);
    p->partial_len = 0;
    p->pending_cr = 0;
    p->error = REPL89_OK;
}

static void paste_fail(repl89_paste *p, repl89_error err)
{
    p->error = err;
    p->partial_len = 0;
}

static void paste_emit(repl89_paste *p, const char *s, size_t n)
{
    repl89_error err;

    if (p->error != REPL89_OK)
    {
        return;
    }
    err = repl89_buf_insert(&p->out, p->out.len, s, n);
    if (err != REPL89_OK)
    {
        p->error = err;
    }
}

static void paste_emit_cp(repl89_paste *p, u89_cp cp)
{
    unsigned char tmp[4];
    int len;

    len = u89_utf8_encode(cp, tmp);
    if (len > 0)
    {
        paste_emit(p, (const char *)tmp, (size_t)len);
    }
}

static void paste_cr(repl89_paste *p)
{
    paste_emit(p, "\n", 1);
    p->pending_cr = 1;
}

static void paste_take_cp(repl89_paste *p, u89_cp cp)
{
    int swallow_lf;
    int ctl;

    swallow_lf = 0;
    if (p->pending_cr != 0)
    {
        p->pending_cr = 0;
        if (cp == 0x0AUL)
        {
            swallow_lf = 1;
        }
    }
    if (swallow_lf != 0)
    {
        return;
    }
    if (cp == 0x0DUL)
    {
        paste_cr(p);
        return;
    }
    if (cp == 0x0AUL)
    {
        paste_emit(p, "\n", 1);
        return;
    }
    if (cp == 0x09UL)
    {
        paste_emit(p, "\t", 1);
        return;
    }
    ctl = u89_is_control(cp);
    if (ctl)
    {
        return;
    }
    paste_emit_cp(p, cp);
}

static void paste_stash(repl89_paste *p, const char *s, size_t n)
{
    if (n > 4)
    {
        paste_fail(p, REPL89_EUTF8);
        return;
    }
    memcpy(p->partial, s, n);
    p->partial_len = n;
}

static size_t paste_step(repl89_paste *p, const char *s, size_t n, size_t i)
{
    unsigned char b;
    u89_cp cp;
    u89_status st;
    size_t next;
    int need;

    b = (unsigned char)s[i];
    if (b < 0x80)
    {
        paste_take_cp(p, (u89_cp)b);
        return i + 1;
    }
    st = u89_utf8_decode((const unsigned char *)s, n, i, &cp, &next);
    if (st == U89_OK)
    {
        paste_take_cp(p, cp);
        return next;
    }
    need = u89_utf8_seq_len(b);
    if (need > 0)
    {
        if (n - i < (size_t)need)
        {
            paste_stash(p, s + i, n - i);
            return n;
        }
    }
    paste_fail(p, REPL89_EUTF8);
    return i + 1;
}

static void paste_complete(repl89_paste *p, u89_cp cp)
{
    paste_take_cp(p, cp);
    p->partial_len = 0;
}

static size_t paste_partial_step(repl89_paste *p, const char *s, size_t i)
{
    u89_cp cp;
    u89_status st;
    size_t next;
    size_t total;
    int need;

    total = p->partial_len + 1;
    p->partial[p->partial_len] = s[i];
    p->partial_len = total;
    need = u89_utf8_seq_len((unsigned char)p->partial[0]);
    if (need == 0)
    {
        paste_fail(p, REPL89_EUTF8);
        return i + 1;
    }
    if (total < (size_t)need)
    {
        return i + 1;
    }
    st = u89_utf8_decode((const unsigned char *)p->partial, total, 0, &cp,
                         &next);
    if (st == U89_OK)
    {
        if (next == total)
        {
            paste_complete(p, cp);
        }
        return i + 1;
    }
    paste_fail(p, REPL89_EUTF8);
    return i + 1;
}

repl89_error repl89_paste_feed(repl89_paste *p, const char *s, size_t n)
{
    size_t i;

    i = 0;
    while (i < n)
    {
        if (p->partial_len > 0)
        {
            i = paste_partial_step(p, s, i);
        }
        else
        {
            i = paste_step(p, s, n, i);
        }
    }
    return p->error;
}

repl89_error repl89_paste_finish(repl89_paste *p, repl89_text *text)
{
    int valid;

    if (p->error != REPL89_OK)
    {
        return p->error;
    }
    if (p->partial_len != 0)
    {
        return REPL89_EUTF8;
    }
    valid = u89_utf8_valid((const unsigned char *)p->out.data, p->out.len);
    if (!valid)
    {
        return REPL89_EUTF8;
    }
    text->data = p->out.data;
    text->len = p->out.len;
    return REPL89_OK;
}
