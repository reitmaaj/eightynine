/* repl89_render.c - terminal cell policy and complete editor redisplay.
 *
 * Unicode facts (grapheme boundaries, East Asian Width, emoji presentation,
 * marks, default-ignorables) come from libu89; this file owns the terminal
 * policy: tab stops, soft wrapping, prompts, and cursor mapping. The
 * renderer draws from the region's first row, column 0, and positions the
 * cursor with relative movements. It never modifies the submission. */

#include <stdio.h>
#include <string.h>

#include "repl89_internal.h"
#include "u89.h"

/* ---- Sink helpers -------------------------------------------------------- */

static int sink_put(const repl89_sink *sink, const char *p, size_t n)
{
    int r;

    if (n == 0)
    {
        return 0;
    }
    r = sink->emit(sink->ctx, p, n);
    return r;
}

/* ---- Cluster width policy ------------------------------------------------ */

typedef struct repl89_flags
{
    int base;
    int wide;
    int emoji;
} repl89_flags;

static repl89_flags flags_of(u89_cp cp)
{
    repl89_flags f;
    int mark;
    int ign;
    int ep;
    u89_eaw eaw;

    f.base = 0;
    f.wide = 0;
    f.emoji = 0;
    mark = u89_is_mark(cp);
    if (mark)
    {
        return f;
    }
    ign = u89_default_ignorable(cp);
    if (ign)
    {
        return f;
    }
    f.base = 1;
    eaw = u89_east_asian_width(cp);
    if (eaw == U89_EAW_W)
    {
        f.wide = 1;
    }
    if (eaw == U89_EAW_F)
    {
        f.wide = 1;
    }
    ep = u89_is_emoji_presentation(cp);
    if (ep)
    {
        f.emoji = 1;
    }
    return f;
}

static void flags_add(repl89_flags *acc, repl89_flags f)
{
    if (f.base)
    {
        acc->base = 1;
    }
    if (f.wide)
    {
        acc->wide = 1;
    }
    if (f.emoji)
    {
        acc->emoji = 1;
    }
}

static int width_of_flags(repl89_flags f)
{
    if (f.wide)
    {
        return 2;
    }
    if (f.emoji)
    {
        return 2;
    }
    if (f.base)
    {
        return 1;
    }
    return 0;
}

static int width_step(const char *s, size_t end, size_t *pos, repl89_flags *acc)
{
    size_t next;
    u89_cp cp;
    u89_status st;
    repl89_flags f;

    st = u89_utf8_decode((const unsigned char *)s, end, *pos, &cp, &next);
    if (st != U89_OK)
    {
        return 0;
    }
    f = flags_of(cp);
    flags_add(acc, f);
    *pos = next;
    return 1;
}

int repl89_cluster_width(const char *s, size_t start, size_t end)
{
    size_t pos;
    repl89_flags acc;
    int go;
    int w;

    acc.base = 0;
    acc.wide = 0;
    acc.emoji = 0;
    pos = start;
    go = 1;
    while (go != 0)
    {
        go = width_step(s, end, &pos, &acc);
    }
    w = width_of_flags(acc);
    return w;
}

/* ---- Prompt width -------------------------------------------------------- */

static int prompt_step(const char *p, size_t n, size_t *pos, unsigned int *col)
{
    size_t next;
    int w;

    if (*pos >= n)
    {
        return 0;
    }
    next = u89_grapheme_next((const unsigned char *)p, n, *pos);
    w = repl89_cluster_width(p, *pos, next);
    *col = *col + (unsigned int)w;
    *pos = next;
    return 1;
}

static unsigned int prompt_cols(const char *p)
{
    size_t n;
    size_t pos;
    unsigned int col;
    int go;

    if (p == NULL)
    {
        return 0;
    }
    n = strlen(p);
    pos = 0;
    col = 0;
    go = 1;
    while (go != 0)
    {
        go = prompt_step(p, n, &pos, &col);
    }
    return col;
}

/* ---- Drawing state ------------------------------------------------------- */

typedef struct repl89_draw
{
    const repl89_edit *e;
    const char *prompt;
    const char *cont;
    unsigned int cols;
    unsigned int tab_width;
    const repl89_sink *sink;
    size_t pos;
    size_t cursor;
    unsigned int row;
    unsigned int col;
    unsigned int cursor_row;
    unsigned int cursor_col;
    int cursor_seen;
    int line_start;
    int line_index;
    int repaint;
    int err;
} repl89_draw;

static void mark_cursor(repl89_draw *d);

static void draw_emit(repl89_draw *d, const char *p, size_t n)
{
    int r;

    if (d->sink == NULL)
    {
        return;
    }
    r = sink_put(d->sink, p, n);
    if (r != 0)
    {
        d->err = 1;
    }
}

/* End the current visual row. In repaint mode the obsolete suffix is erased
   after the replacement row has been written, never before. */
static void draw_row_end(repl89_draw *d)
{
    if (d->repaint != 0)
    {
        draw_emit(d, "\x1b[K", 3);
    }
    draw_emit(d, "\r\n", 2);
}

static void draw_newline(repl89_draw *d)
{
    draw_row_end(d);
    d->row = d->row + 1;
    d->col = 0;
    d->line_start = 0;
}

static unsigned int tab_advance(unsigned int col, unsigned int tab_width)
{
    unsigned int rem;

    if (tab_width == 0)
    {
        return 0;
    }
    rem = col % tab_width;
    return tab_width - rem;
}

static int need_wrap(unsigned int col, int w, unsigned int cols)
{
    if (col + (unsigned int)w > cols)
    {
        return 1;
    }
    return 0;
}

/* Logical columns run 0..cols inclusive: col == cols is the insertion point
   after the final cell. Its physical target is the final terminal cell. */
static unsigned int physical_col(unsigned int col, unsigned int cols)
{
    if (cols == 0)
    {
        return 0;
    }
    if (col >= cols)
    {
        return cols - 1;
    }
    return col;
}

static void draw_spaces(repl89_draw *d, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < n; ++i)
    {
        draw_emit(d, " ", 1);
    }
}

static unsigned int tab_after_wrap(unsigned int tab_width, unsigned int cols)
{
    if (tab_width > cols)
    {
        return cols;
    }
    return tab_width;
}

static unsigned int draw_wrap_tab(repl89_draw *d)
{
    unsigned int adv;

    draw_newline(d);
    adv = tab_after_wrap(d->tab_width, d->cols);
    return adv;
}

static void draw_tab(repl89_draw *d, int mark)
{
    unsigned int adv;
    int wrap;

    adv = tab_advance(d->col, d->tab_width);
    wrap = need_wrap(d->col, (int)adv, d->cols);
    if (wrap)
    {
        adv = draw_wrap_tab(d);
    }
    if (mark != 0)
    {
        mark_cursor(d);
    }
    draw_spaces(d, adv);
    d->col = d->col + adv;
}

static const char *prompt_for(const repl89_draw *d)
{
    const char *p;
    const char *c;

    p = d->prompt;
    c = d->cont;
    if (d->line_index > 0)
    {
        p = c;
    }
    if (p == NULL)
    {
        p = "";
    }
    return p;
}

static void draw_prompt(repl89_draw *d)
{
    const char *p;
    size_t n;

    p = prompt_for(d);
    n = strlen(p);
    draw_emit(d, p, n);
    d->col = prompt_cols(p);
    d->line_start = 0;
}

static void draw_lf(repl89_draw *d)
{
    draw_row_end(d);
    d->row = d->row + 1;
    d->col = 0;
    d->line_start = 1;
    d->line_index = d->line_index + 1;
}

static void draw_cluster(repl89_draw *d, size_t start, size_t end, int mark)
{
    int w;
    int wrap;

    w = repl89_cluster_width(d->e->buf.data, start, end);
    wrap = need_wrap(d->col, w, d->cols);
    if (wrap)
    {
        draw_newline(d);
    }
    if (mark != 0)
    {
        mark_cursor(d);
    }
    draw_emit(d, d->e->buf.data + start, end - start);
    d->col = d->col + (unsigned int)w;
}

static int cluster_is(size_t start, size_t end, const char *data, char c)
{
    if (end - start != 1)
    {
        return 0;
    }
    if (data[start] != c)
    {
        return 0;
    }
    return 1;
}

static void mark_cursor(repl89_draw *d)
{
    unsigned int r;
    unsigned int c;

    r = d->row;
    c = d->col;
    d->cursor_row = r;
    d->cursor_col = c;
    d->cursor_seen = 1;
}

static int draw_step(repl89_draw *d)
{
    const char *data;
    size_t n;
    size_t start;
    size_t next;
    int mark;
    int is_lf;
    int is_ht;

    n = d->e->buf.len;
    if (d->pos >= n)
    {
        return 0;
    }
    if (d->line_start)
    {
        draw_prompt(d);
    }
    data = d->e->buf.data;
    start = d->pos;
    next = u89_grapheme_next((const unsigned char *)data, n, start);
    is_lf = cluster_is(start, next, data, '\n');
    is_ht = cluster_is(start, next, data, '\t');
    mark = 0;
    if (d->pos == d->cursor)
    {
        mark = 1;
    }
    if (is_lf)
    {
        if (mark != 0)
        {
            mark_cursor(d);
        }
        draw_lf(d);
    }
    else
    {
        if (is_ht)
        {
            draw_tab(d, mark);
        }
        else
        {
            draw_cluster(d, start, next, mark);
        }
    }
    d->pos = next;
    return 1;
}

static void emit_up(repl89_draw *d, unsigned int n)
{
    char buf[32];
    int len;

    len = sprintf(buf, "\x1b[%uA", n);
    draw_emit(d, buf, (size_t)len);
}

static void emit_fwd(repl89_draw *d, unsigned int n)
{
    char buf[32];
    int len;

    len = sprintf(buf, "\x1b[%uC", n);
    draw_emit(d, buf, (size_t)len);
}

static void draw_place_cursor(repl89_draw *d, unsigned int up)
{
    unsigned int col;

    if (up > 0)
    {
        emit_up(d, up);
    }
    draw_emit(d, "\r", 1);
    col = physical_col(d->cursor_col, d->cols);
    if (col > 0)
    {
        emit_fwd(d, col);
    }
}

static void draw_finish(repl89_draw *d, repl89_region *out)
{
    unsigned int up;

    if (d->line_start)
    {
        draw_prompt(d);
    }
    if (d->cursor_seen == 0)
    {
        mark_cursor(d);
    }
    out->cols = d->cols;
    out->end_row = d->row;
    out->end_col = d->col;
    up = d->row - d->cursor_row;
    if (d->repaint != 0)
    {
        draw_emit(d, "\x1b[K", 3);
    }
    else
    {
        draw_place_cursor(d, up);
    }
    out->rows = d->row + 1;
    out->cursor_row = d->cursor_row;
    out->cursor_col = d->cursor_col;
}

static int sink_up(const repl89_sink *sink, unsigned int n)
{
    char buf[32];
    int len;
    int r;

    len = sprintf(buf, "\x1b[%uA", n);
    r = sink_put(sink, buf, (size_t)len);
    return r;
}

static int sink_down(const repl89_sink *sink, unsigned int n)
{
    char buf[32];
    int len;
    int r;

    len = sprintf(buf, "\x1b[%uB", n);
    r = sink_put(sink, buf, (size_t)len);
    return r;
}

static int sink_fwd(const repl89_sink *sink, unsigned int n)
{
    char buf[32];
    int len;
    int r;

    len = sprintf(buf, "\x1b[%uC", n);
    r = sink_put(sink, buf, (size_t)len);
    return r;
}

static int clear_row(const repl89_sink *sink, const repl89_region *reg,
                     unsigned int i)
{
    int r;

    r = sink_put(sink, "\x1b[2K", 4);
    if (r != 0)
    {
        return r;
    }
    if (i + 1 < reg->rows)
    {
        r = sink_put(sink, "\r\n", 2);
    }
    return r;
}

static int clear_rows(const repl89_sink *sink, const repl89_region *reg)
{
    unsigned int i;
    int r;

    r = 0;
    for (i = 0; i < reg->rows; ++i)
    {
        r = clear_row(sink, reg, i);
        if (r != 0)
        {
            return r;
        }
    }
    return r;
}

repl89_error repl89_render_clear(const repl89_sink *sink,
                                 const repl89_region *reg)
{
    unsigned int up;
    int r;

    r = 0;
    up = reg->cursor_row;
    if (up > 0)
    {
        r = sink_up(sink, up);
    }
    if (r == 0)
    {
        r = sink_put(sink, "\r", 1);
    }
    if (r == 0)
    {
        r = clear_rows(sink, reg);
    }
    if (r == 0)
    {
        if (reg->rows > 1)
        {
            r = sink_up(sink, reg->rows - 1);
        }
    }
    if (r == 0)
    {
        r = sink_put(sink, "\r", 1);
    }
    if (r != 0)
    {
        return REPL89_EIO;
    }
    return REPL89_OK;
}

repl89_error repl89_render_finalize(const repl89_sink *sink,
                                    const repl89_region *reg)
{
    unsigned int down;
    unsigned int col;
    int r;

    r = 0;
    down = reg->end_row - reg->cursor_row;
    if (down > 0)
    {
        r = sink_down(sink, down);
    }
    if (r == 0)
    {
        r = sink_put(sink, "\r", 1);
    }
    if (r == 0)
    {
        col = physical_col(reg->end_col, reg->cols);
        if (col > 0)
        {
            r = sink_fwd(sink, col);
        }
    }
    if (r == 0)
    {
        r = sink_put(sink, "\r\n", 2);
    }
    if (r != 0)
    {
        return REPL89_EIO;
    }
    return REPL89_OK;
}

/* ---- Cursor-only motion -------------------------------------------------- */

static unsigned int row_span(unsigned int a, unsigned int b)
{
    if (a > b)
    {
        return a - b;
    }
    return b - a;
}

static int cursor_move_row(const repl89_sink *sink, const repl89_region *old,
                           const repl89_region *next)
{
    unsigned int n;
    int r;

    n = row_span(old->cursor_row, next->cursor_row);
    if (n == 0)
    {
        return 0;
    }
    if (next->cursor_row > old->cursor_row)
    {
        r = sink_down(sink, n);
        return r;
    }
    r = sink_up(sink, n);
    return r;
}

static int cursor_move_col(const repl89_sink *sink, unsigned int col,
                           unsigned int cols)
{
    unsigned int pcol;
    int r;

    pcol = physical_col(col, cols);
    r = sink_put(sink, "\r", 1);
    if (pcol == 0)
    {
        return r;
    }
    if (r != 0)
    {
        return r;
    }
    r = sink_fwd(sink, pcol);
    return r;
}

static int cursor_same(const repl89_region *old, const repl89_region *next)
{
    if (old->cursor_row != next->cursor_row)
    {
        return 0;
    }
    if (old->cursor_col != next->cursor_col)
    {
        return 0;
    }
    return 1;
}

repl89_error repl89_render_cursor(const repl89_sink *sink,
                                  const repl89_region *old,
                                  const repl89_region *next)
{
    int same;
    int r;

    same = cursor_same(old, next);
    if (same != 0)
    {
        return REPL89_OK;
    }
    r = cursor_move_row(sink, old, next);
    if (r != 0)
    {
        return REPL89_EIO;
    }
    r = cursor_move_col(sink, next->cursor_col, next->cols);
    if (r != 0)
    {
        return REPL89_EIO;
    }
    return REPL89_OK;
}

void repl89_layout(const repl89_edit *e, const char *prompt, const char *cont,
                   unsigned int cols, unsigned int tab_width,
                   repl89_region *out)
{
    repl89_render_draw(e, prompt, cont, cols, tab_width, NULL, out);
}

/* ---- Vertical movement --------------------------------------------------- */

typedef struct vmove_state
{
    const repl89_edit *e;
    const char *prompt;
    const char *cont;
    unsigned int cols;
    unsigned int tab_width;
    size_t pos;
    size_t cursor;
    unsigned int row;
    unsigned int col;
    int line_start;
    int line_index;
    int found_cursor;
    unsigned int cur_row;
    unsigned int cur_col;
    unsigned int rows;
} vmove_state;

typedef struct vmove_target
{
    unsigned int row;
    unsigned int want_col;
    size_t best;
    unsigned int best_col;
    unsigned int best_dist;
    int found;
} vmove_target;

static void vmove_init(vmove_state *v, const repl89_edit *e, const char *prompt,
                       const char *cont, unsigned int cols,
                       unsigned int tab_width)
{
    v->e = e;
    v->prompt = prompt;
    v->cont = cont;
    v->cols = cols;
    v->tab_width = tab_width;
    v->pos = 0;
    v->cursor = e->cursor;
    v->row = 0;
    v->col = 0;
    v->line_start = 1;
    v->line_index = 0;
    v->found_cursor = 0;
    v->cur_row = 0;
    v->cur_col = 0;
    v->rows = 0;
}

static const char *v_prompt(const vmove_state *v)
{
    const char *p;
    const char *c;

    p = v->prompt;
    c = v->cont;
    if (v->line_index > 0)
    {
        p = c;
    }
    if (p == NULL)
    {
        p = "";
    }
    return p;
}

static void vmove_line_start(vmove_state *v)
{
    const char *p;

    p = v_prompt(v);
    v->col = prompt_cols(p);
    v->line_start = 0;
}

static void vmove_mark_cursor(vmove_state *v)
{
    v->cur_row = v->row;
    v->cur_col = v->col;
    v->found_cursor = 1;
}

static void vmove_wrap(vmove_state *v)
{
    v->row = v->row + 1;
    v->col = 0;
    v->line_start = 0;
}

static unsigned int vmove_wrap_tab(vmove_state *v)
{
    unsigned int adv;

    vmove_wrap(v);
    adv = tab_after_wrap(v->tab_width, v->cols);
    return adv;
}

static void vmove_tab(vmove_state *v, int mark)
{
    unsigned int adv;
    int wrap;

    adv = tab_advance(v->col, v->tab_width);
    wrap = need_wrap(v->col, (int)adv, v->cols);
    if (wrap)
    {
        adv = vmove_wrap_tab(v);
    }
    if (mark != 0)
    {
        vmove_mark_cursor(v);
    }
    v->col = v->col + adv;
}

static void vmove_lf(vmove_state *v)
{
    v->row = v->row + 1;
    v->col = 0;
    v->line_start = 1;
    v->line_index = v->line_index + 1;
}

static void vmove_cluster(vmove_state *v, const char *data, size_t start,
                          size_t end, int mark)
{
    int w;
    int wrap;

    w = repl89_cluster_width(data, start, end);
    wrap = need_wrap(v->col, w, v->cols);
    if (wrap)
    {
        vmove_wrap(v);
    }
    if (mark != 0)
    {
        vmove_mark_cursor(v);
    }
    v->col = v->col + (unsigned int)w;
}

/* Advance over one cluster; returns 0 at end of input. */
static int vmove_step(vmove_state *v)
{
    const char *data;
    size_t n;
    size_t start;
    size_t next;
    int mark;
    int is_lf;
    int is_ht;

    n = v->e->buf.len;
    if (v->pos >= n)
    {
        return 0;
    }
    if (v->line_start)
    {
        vmove_line_start(v);
    }
    data = v->e->buf.data;
    start = v->pos;
    next = u89_grapheme_next((const unsigned char *)data, n, start);
    is_lf = cluster_is(start, next, data, '\n');
    is_ht = cluster_is(start, next, data, '\t');
    mark = 0;
    if (v->pos == v->cursor)
    {
        if (v->found_cursor == 0)
        {
            mark = 1;
        }
    }
    if (is_lf)
    {
        if (mark != 0)
        {
            vmove_mark_cursor(v);
        }
        vmove_lf(v);
    }
    else
    {
        if (is_ht)
        {
            vmove_tab(v, mark);
        }
        else
        {
            vmove_cluster(v, data, start, next, mark);
        }
    }
    v->pos = next;
    return 1;
}

static void vmove_locate(vmove_state *v)
{
    int go;

    go = 1;
    while (go != 0)
    {
        go = vmove_step(v);
    }
    if (v->found_cursor == 0)
    {
        if (v->line_start)
        {
            vmove_line_start(v);
        }
        vmove_mark_cursor(v);
    }
    v->rows = v->row + 1;
}

static unsigned int want_or_cursor(unsigned int want, const vmove_state *v)
{
    if (want == REPL89_WANT_CURSOR)
    {
        return v->cur_col;
    }
    return want;
}

static unsigned int row_before(unsigned int row)
{
    return row - 1;
}

static unsigned int row_after(unsigned int row)
{
    return row + 1;
}

static void vmove_set_best(vmove_target *t, size_t off, unsigned int col,
                           unsigned int dist)
{
    t->best = off;
    t->best_col = col;
    t->best_dist = dist;
    t->found = 1;
}

static unsigned int col_dist(unsigned int a, unsigned int b)
{
    if (a > b)
    {
        return a - b;
    }
    return b - a;
}

static void vmove_consider(vmove_target *t, size_t off, unsigned int col)
{
    unsigned int dist;

    dist = col_dist(col, t->want_col);
    if (t->found == 0)
    {
        vmove_set_best(t, off, col, dist);
        return;
    }
    if (dist < t->best_dist)
    {
        vmove_set_best(t, off, col, dist);
    }
}

/* Consider every grapheme boundary of the target row. Returns 0 when the row
   has been passed or input is exhausted. */
static int vmove_scan_step(vmove_state *v, vmove_target *t)
{
    const char *data;
    size_t n;
    size_t start;
    size_t next;
    int is_lf;
    int is_ht;

    n = v->e->buf.len;
    if (v->pos >= n)
    {
        return 0;
    }
    if (v->line_start)
    {
        vmove_line_start(v);
    }
    if (v->row == t->row)
    {
        vmove_consider(t, v->pos, v->col);
    }
    data = v->e->buf.data;
    start = v->pos;
    next = u89_grapheme_next((const unsigned char *)data, n, start);
    is_lf = cluster_is(start, next, data, '\n');
    is_ht = cluster_is(start, next, data, '\t');
    if (is_lf)
    {
        vmove_lf(v);
    }
    else
    {
        if (is_ht)
        {
            vmove_tab(v, 0);
        }
        else
        {
            vmove_cluster(v, data, start, next, 0);
        }
    }
    v->pos = next;
    if (v->row == t->row)
    {
        vmove_consider(t, v->pos, v->col);
    }
    if (v->row > t->row)
    {
        return 0;
    }
    return 1;
}

static void vmove_scan(vmove_state *v, vmove_target *t)
{
    int go;

    go = 1;
    while (go != 0)
    {
        go = vmove_scan_step(v, t);
    }
    if (v->row == t->row)
    {
        if (v->line_start)
        {
            vmove_line_start(v);
        }
        vmove_consider(t, v->pos, v->col);
    }
}

void repl89_layout_vertical(const repl89_edit *e, const char *prompt,
                            const char *cont, unsigned int cols,
                            unsigned int tab_width, int direction,
                            unsigned int want_col, repl89_vmove *out)
{
    vmove_state v;
    vmove_target t;
    unsigned int target_row;
    unsigned int want;

    vmove_init(&v, e, prompt, cont, cols, tab_width);
    vmove_locate(&v);
    out->cur_row = v.cur_row;
    out->cur_col = v.cur_col;
    out->rows = v.rows;
    want = want_or_cursor(want_col, &v);
    if (direction < 0)
    {
        if (v.cur_row == 0)
        {
            out->in_range = 0;
            return;
        }
        target_row = row_before(v.cur_row);
    }
    else
    {
        if (v.cur_row + 1 >= v.rows)
        {
            out->in_range = 0;
            return;
        }
        target_row = row_after(v.cur_row);
    }
    vmove_init(&v, e, prompt, cont, cols, tab_width);
    t.row = target_row;
    t.want_col = want;
    t.best = 0;
    t.best_col = 0;
    t.best_dist = 0;
    t.found = 0;
    vmove_scan(&v, &t);
    out->in_range = 1;
    out->target = t.best;
    out->target_col = t.best_col;
}

static void draw_init(repl89_draw *d, const repl89_edit *e, const char *prompt,
                      const char *cont, unsigned int cols,
                      unsigned int tab_width, const repl89_sink *sink,
                      int repaint)
{
    d->e = e;
    d->prompt = prompt;
    d->cont = cont;
    d->cols = cols;
    d->tab_width = tab_width;
    d->sink = sink;
    d->pos = 0;
    d->cursor = e->cursor;
    d->row = 0;
    d->col = 0;
    d->cursor_row = 0;
    d->cursor_col = 0;
    d->cursor_seen = 0;
    d->line_start = 1;
    d->line_index = 0;
    d->repaint = repaint;
    d->err = 0;
}

static repl89_error draw_run(const repl89_edit *e, const char *prompt,
                             const char *cont, unsigned int cols,
                             unsigned int tab_width, const repl89_sink *sink,
                             int repaint, repl89_region *out)
{
    repl89_draw d;
    int go;

    draw_init(&d, e, prompt, cont, cols, tab_width, sink, repaint);
    go = 1;
    while (go != 0)
    {
        go = draw_step(&d);
    }
    if (d.err != 0)
    {
        return REPL89_EIO;
    }
    draw_finish(&d, out);
    if (d.err != 0)
    {
        return REPL89_EIO;
    }
    return REPL89_OK;
}

repl89_error repl89_render_draw(const repl89_edit *e, const char *prompt,
                                const char *cont, unsigned int cols,
                                unsigned int tab_width, const repl89_sink *sink,
                                repl89_region *out)
{
    repl89_error err;

    err = draw_run(e, prompt, cont, cols, tab_width, sink, 0, out);
    return err;
}

/* ---- Content repaint ----------------------------------------------------- */

static unsigned int max_rows(unsigned int a, unsigned int b)
{
    if (a > b)
    {
        return a;
    }
    return b;
}

static repl89_error repaint_top(const repl89_sink *sink, unsigned int row)
{
    int r;

    if (row > 0)
    {
        r = sink_up(sink, row);
        if (r != 0)
        {
            return REPL89_EIO;
        }
    }
    r = sink_put(sink, "\r", 1);
    if (r != 0)
    {
        return REPL89_EIO;
    }
    return REPL89_OK;
}

static void stale_row(const repl89_sink *sink)
{
    sink_put(sink, "\r\n", 2);
    sink_put(sink, "\x1b[2K", 4);
}

static void clear_stale(const repl89_sink *sink, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < n; ++i)
    {
        stale_row(sink);
    }
}

static repl89_error repaint_place(const repl89_sink *sink, unsigned int up,
                                  unsigned int col, unsigned int cols)
{
    int r;

    if (up > 0)
    {
        r = sink_up(sink, up);
        if (r != 0)
        {
            return REPL89_EIO;
        }
    }
    r = cursor_move_col(sink, col, cols);
    if (r != 0)
    {
        return REPL89_EIO;
    }
    return REPL89_OK;
}

/* The physical cursor starts at old_cursor_row relative to the top of a
   region occupying old_rows rows. Replacement rows are written over the old
   ones first; only then are obsolete suffixes and whole stale rows erased. */
static repl89_error repaint(const repl89_edit *e, const char *prompt,
                            const char *cont, unsigned int cols,
                            unsigned int tab_width, const repl89_sink *sink,
                            unsigned int old_rows, unsigned int old_cursor_row,
                            repl89_region *out)
{
    repl89_error err;
    unsigned int occupied;
    unsigned int up;

    err = repaint_top(sink, old_cursor_row);
    if (err != REPL89_OK)
    {
        return err;
    }
    err = draw_run(e, prompt, cont, cols, tab_width, sink, 1, out);
    if (err != REPL89_OK)
    {
        return err;
    }
    if (old_rows > out->rows)
    {
        clear_stale(sink, old_rows - out->rows);
    }
    occupied = max_rows(old_rows, out->rows);
    up = occupied - 1 - out->cursor_row;
    err = repaint_place(sink, up, out->cursor_col, out->cols);
    return err;
}

repl89_error repl89_render_content(const repl89_edit *e, const char *prompt,
                                   const char *cont, unsigned int cols,
                                   unsigned int tab_width,
                                   const repl89_sink *sink,
                                   const repl89_region *old, repl89_region *out)
{
    repl89_error err;

    if (old->rows == 0)
    {
        err = repl89_render_draw(e, prompt, cont, cols, tab_width, sink, out);
        return err;
    }
    err = repaint(e, prompt, cont, cols, tab_width, sink, old->rows,
                  old->cursor_row, out);
    return err;
}

/* ---- Resize reflow ------------------------------------------------------- */

typedef struct repl89_reflow_state
{
    unsigned int cols;
    unsigned int rows;
    unsigned int base_row;
    unsigned int subrow;
    unsigned int col;
    unsigned int cursor_row;
    unsigned int cursor_col;
} repl89_reflow_state;

typedef struct repl89_reflow_walk
{
    const repl89_edit *e;
    const char *prompt;
    const char *cont;
    unsigned int old_cols;
    unsigned int tab_width;
    repl89_reflow_state rs;
    size_t pos;
    unsigned int old_col;
    int line_start;
    int line_index;
    int cursor_seen;
} repl89_reflow_walk;

static void reflow_mark(repl89_reflow_state *rs)
{
    rs->cursor_row = rs->base_row + rs->subrow;
    rs->cursor_col = rs->col;
}

static void reflow_wrap(repl89_reflow_state *rs)
{
    rs->subrow = rs->subrow + 1;
    rs->col = 0;
}

static void reflow_atom(repl89_reflow_state *rs, unsigned int width, int mark)
{
    if (rs->col + width > rs->cols)
    {
        reflow_wrap(rs);
    }
    if (mark != 0)
    {
        reflow_mark(rs);
    }
    rs->col = rs->col + width;
}

static void reflow_row_end(repl89_reflow_state *rs)
{
    rs->base_row = rs->base_row + rs->subrow + 1;
    rs->subrow = 0;
    rs->col = 0;
}

static unsigned int reflow_total(const repl89_reflow_state *rs)
{
    return rs->base_row + rs->subrow + 1;
}

static const char *rw_prompt_text(const repl89_reflow_walk *w)
{
    const char *p;
    const char *c;

    p = w->prompt;
    c = w->cont;
    if (w->line_index > 0)
    {
        p = c;
    }
    if (p == NULL)
    {
        p = "";
    }
    return p;
}

static void rw_atom(repl89_reflow_walk *w, unsigned int width, int mark)
{
    reflow_atom(&w->rs, width, mark);
    w->old_col = w->old_col + width;
}

static void rw_prompt_one(repl89_reflow_walk *w, const char *p, size_t n,
                          size_t *pos)
{
    size_t next;
    unsigned int width;

    next = u89_grapheme_next((const unsigned char *)p, n, *pos);
    width = (unsigned int)repl89_cluster_width(p, *pos, next);
    rw_atom(w, width, 0);
    *pos = next;
}

static void rw_prompt_run(repl89_reflow_walk *w, const char *p, size_t n)
{
    size_t pos;

    pos = 0;
    while (pos < n)
    {
        rw_prompt_one(w, p, n, &pos);
    }
}

static void rw_prompt(repl89_reflow_walk *w)
{
    const char *p;
    size_t n;

    p = rw_prompt_text(w);
    n = strlen(p);
    rw_prompt_run(w, p, n);
    w->line_start = 0;
}

static int rw_take_mark(repl89_reflow_walk *w)
{
    if (w->cursor_seen != 0)
    {
        return 0;
    }
    if (w->pos != w->e->cursor)
    {
        return 0;
    }
    w->cursor_seen = 1;
    return 1;
}

static void rw_wrap(repl89_reflow_walk *w)
{
    reflow_row_end(&w->rs);
    w->old_col = 0;
}

static void rw_lf(repl89_reflow_walk *w)
{
    int mark;

    mark = rw_take_mark(w);
    if (mark != 0)
    {
        reflow_mark(&w->rs);
    }
    rw_wrap(w);
    w->line_start = 1;
    w->line_index = w->line_index + 1;
}

static void rw_spaces(repl89_reflow_walk *w, unsigned int n, int mark)
{
    unsigned int i;

    if (n > 0)
    {
        rw_atom(w, 1, mark);
    }
    for (i = 1; i < n; ++i)
    {
        rw_atom(w, 1, 0);
    }
}

static unsigned int rw_wrap_tab(repl89_reflow_walk *w)
{
    unsigned int adv;

    rw_wrap(w);
    adv = tab_after_wrap(w->tab_width, w->old_cols);
    return adv;
}

static void rw_tab(repl89_reflow_walk *w)
{
    unsigned int adv;
    int mark;

    adv = tab_advance(w->old_col, w->tab_width);
    if (w->old_col + adv > w->old_cols)
    {
        adv = rw_wrap_tab(w);
    }
    mark = rw_take_mark(w);
    rw_spaces(w, adv, mark);
}

static void rw_cluster(repl89_reflow_walk *w, size_t start, size_t end)
{
    unsigned int width;
    int mark;

    width = (unsigned int)repl89_cluster_width(w->e->buf.data, start, end);
    if (w->old_col + width > w->old_cols)
    {
        rw_wrap(w);
    }
    mark = rw_take_mark(w);
    rw_atom(w, width, mark);
}

static void rw_step(repl89_reflow_walk *w)
{
    const char *data;
    size_t n;
    size_t start;
    size_t next;
    int is_lf;
    int is_ht;

    data = w->e->buf.data;
    n = w->e->buf.len;
    if (w->line_start)
    {
        rw_prompt(w);
    }
    start = w->pos;
    next = u89_grapheme_next((const unsigned char *)data, n, start);
    is_lf = cluster_is(start, next, data, '\n');
    is_ht = cluster_is(start, next, data, '\t');
    if (is_lf)
    {
        rw_lf(w);
    }
    else if (is_ht)
    {
        rw_tab(w);
    }
    else
    {
        rw_cluster(w, start, next);
    }
    w->pos = next;
}

static void rw_run(repl89_reflow_walk *w)
{
    size_t n;

    n = w->e->buf.len;
    while (w->pos < n)
    {
        rw_step(w);
    }
    if (w->line_start)
    {
        rw_prompt(w);
    }
    if (w->cursor_seen == 0)
    {
        reflow_mark(&w->rs);
    }
    w->rs.rows = reflow_total(&w->rs);
}

void repl89_layout_reflow(const repl89_edit *e, const char *prompt,
                          const char *cont, unsigned int old_cols,
                          unsigned int new_cols, unsigned int tab_width,
                          repl89_reflow *out)
{
    repl89_reflow_walk w;

    w.e = e;
    w.prompt = prompt;
    w.cont = cont;
    w.old_cols = old_cols;
    w.tab_width = tab_width;
    w.pos = 0;
    w.old_col = 0;
    w.line_start = 1;
    w.line_index = 0;
    w.cursor_seen = 0;
    w.rs.cols = new_cols;
    w.rs.base_row = 0;
    w.rs.subrow = 0;
    w.rs.col = 0;
    w.rs.cursor_row = 0;
    w.rs.cursor_col = 0;
    rw_run(&w);
    out->rows = w.rs.rows;
    out->cursor_row = w.rs.cursor_row;
    out->cursor_col = w.rs.cursor_col;
}

repl89_error repl89_render_resize(const repl89_edit *e, const char *prompt,
                                  const char *cont, unsigned int cols,
                                  unsigned int tab_width,
                                  const repl89_sink *sink,
                                  const repl89_region *old, repl89_region *out)
{
    repl89_reflow rf;
    repl89_error err;

    repl89_layout_reflow(e, prompt, cont, old->cols, cols, tab_width, &rf);
    err = repaint(e, prompt, cont, cols, tab_width, sink, rf.rows,
                  rf.cursor_row, out);
    return err;
}
