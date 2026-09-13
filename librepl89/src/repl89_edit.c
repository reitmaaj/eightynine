/* repl89_edit.c - pure editor model: exact insertion, grapheme-safe editing,
 * logical lines, and the content policy (valid UTF-8; LF/HT are the only
 * permitted controls). No tty or rendering state lives here. */

#include "repl89_internal.h"
#include "u89.h"

/* ---- Content policy ------------------------------------------------------ */

static int allowed_control(u89_cp cp)
{
    int ctl;

    ctl = u89_is_control(cp);
    if (!ctl)
    {
        return 1;
    }
    if (cp == 0x0AUL)
    {
        return 1;
    }
    if (cp == 0x09UL)
    {
        return 1;
    }
    return 0;
}

static int scan_allowed(const char *p, size_t n)
{
    size_t pos;
    size_t next;
    u89_cp cp;
    u89_status st;
    int ok;

    pos = 0;
    while (pos < n)
    {
        st = u89_utf8_decode((const unsigned char *)p, n, pos, &cp, &next);
        if (st != U89_OK)
        {
            return 0;
        }
        ok = allowed_control(cp);
        if (!ok)
        {
            return 0;
        }
        pos = next;
    }
    return 1;
}

repl89_error repl89_content_check(const char *p, size_t n)
{
    int valid;
    int ok;

    valid = u89_utf8_valid((const unsigned char *)p, n);
    if (!valid)
    {
        return REPL89_EUTF8;
    }
    ok = scan_allowed(p, n);
    if (!ok)
    {
        return REPL89_EINVAL;
    }
    return REPL89_OK;
}

/* ---- Editor model -------------------------------------------------------- */

void repl89_edit_init(repl89_edit *e)
{
    repl89_buf_init(&e->buf);
    e->cursor = 0;
}

void repl89_edit_free(repl89_edit *e)
{
    repl89_buf_free(&e->buf);
    e->cursor = 0;
}

void repl89_edit_reset(repl89_edit *e)
{
    repl89_buf_clear(&e->buf);
    e->cursor = 0;
}

repl89_error repl89_edit_insert(repl89_edit *e, const char *p, size_t n)
{
    repl89_error err;

    err = repl89_content_check(p, n);
    if (err != REPL89_OK)
    {
        return err;
    }
    err = repl89_buf_insert(&e->buf, e->cursor, p, n);
    if (err != REPL89_OK)
    {
        return err;
    }
    e->cursor = e->cursor + n;
    return REPL89_OK;
}

repl89_error repl89_edit_delete_prev(repl89_edit *e)
{
    size_t prev;
    size_t n;

    if (e->cursor == 0)
    {
        return REPL89_OK;
    }
    prev = u89_grapheme_prev((const unsigned char *)e->buf.data, e->buf.len,
                             e->cursor);
    n = e->cursor - prev;
    repl89_buf_delete(&e->buf, prev, n);
    e->cursor = prev;
    return REPL89_OK;
}

repl89_error repl89_edit_delete_next(repl89_edit *e)
{
    size_t next;
    size_t n;

    if (e->cursor >= e->buf.len)
    {
        return REPL89_OK;
    }
    next = u89_grapheme_next((const unsigned char *)e->buf.data, e->buf.len,
                             e->cursor);
    n = next - e->cursor;
    repl89_buf_delete(&e->buf, e->cursor, n);
    return REPL89_OK;
}

void repl89_edit_left(repl89_edit *e)
{
    if (e->cursor == 0)
    {
        return;
    }
    e->cursor = u89_grapheme_prev((const unsigned char *)e->buf.data,
                                  e->buf.len, e->cursor);
}

void repl89_edit_right(repl89_edit *e)
{
    if (e->cursor >= e->buf.len)
    {
        return;
    }
    e->cursor = u89_grapheme_next((const unsigned char *)e->buf.data,
                                  e->buf.len, e->cursor);
}

static size_t step_back(size_t i)
{
    return i - 1;
}

static size_t step_fwd(size_t i)
{
    return i + 1;
}

static size_t line_begin(const repl89_edit *e)
{
    size_t i;

    i = e->cursor;
    while (i > 0)
    {
        if (e->buf.data[i - 1] == '\n')
        {
            break;
        }
        i = step_back(i);
    }
    return i;
}

static size_t line_end(const repl89_edit *e)
{
    size_t i;

    i = e->cursor;
    while (i < e->buf.len)
    {
        if (e->buf.data[i] == '\n')
        {
            break;
        }
        i = step_fwd(i);
    }
    return i;
}

void repl89_edit_home(repl89_edit *e)
{
    e->cursor = line_begin(e);
}

void repl89_edit_end(repl89_edit *e)
{
    e->cursor = line_end(e);
}

void repl89_edit_kill_begin(repl89_edit *e)
{
    size_t start;

    start = line_begin(e);
    repl89_buf_delete(&e->buf, start, e->cursor - start);
    e->cursor = start;
}

void repl89_edit_kill_end(repl89_edit *e)
{
    size_t end;

    end = line_end(e);
    repl89_buf_delete(&e->buf, e->cursor, end - e->cursor);
}
