/* repl89.c - public object and the start/feed/submit/cancel state machine.
 *
 * One editor owns the submission, cursor, history, paste accumulator, and
 * the tty session. `feed` drains queued bytes first and then performs at most
 * one read(2); bytes read after a semantic event stay queued for the next
 * session. `read` is a blocking convenience over the same machine. */

#include <errno.h>
#include <string.h>

#include "repl89_internal.h"
#include "repl89_tty.h"
#include "u89.h"

#define REPL89_DEFAULT_TAB 8
#define REPL89_DEFAULT_HISTORY 100
#define REPL89_INBUF 4096
#define REPL89_MARK_LEN 6

static const char DEFAULT_CONT[] = "... ";

typedef enum repl89_state
{
    REPL89_INACTIVE = 0,
    REPL89_ACTIVE
} repl89_state;

struct repl89
{
    repl89_config config;
    const char *cont;
    const char *prompt;
    repl89_edit edit;
    repl89_hist hist;
    repl89_paste paste;
    repl89_buf draft;
    repl89_tty tty;
    repl89_state state;
    int visible;
    int drawn;
    int in_paste;
    char inbuf[REPL89_INBUF];
    size_t in_len;
    repl89_error error;
    repl89_region region;
    unsigned int prefer_col;
    int prefer_active;
};

/* ---- Lifetime ------------------------------------------------------------ */

static void config_copy(repl89_config *dst, const repl89_config *src)
{
    *dst = *src;
}

static void config_defaults(repl89_config *c)
{
    c->input_fd = 0;
    c->output_fd = 1;
    c->tab_width = REPL89_DEFAULT_TAB;
    c->history_limit = REPL89_DEFAULT_HISTORY;
    c->continuation_prompt = DEFAULT_CONT;
}

repl89 *repl89_new(const repl89_config *config)
{
    repl89 *r;

    r = repl89_mem_alloc(sizeof(repl89));
    if (r == NULL)
    {
        return NULL;
    }
    if (config != NULL)
    {
        config_copy(&r->config, config);
    }
    else
    {
        config_defaults(&r->config);
    }
    if (r->config.input_fd < 0)
    {
        repl89_mem_free(r);
        return NULL;
    }
    if (r->config.output_fd < 0)
    {
        repl89_mem_free(r);
        return NULL;
    }
    if (r->config.tab_width == 0)
    {
        r->config.tab_width = REPL89_DEFAULT_TAB;
    }
    r->cont = r->config.continuation_prompt;
    if (r->cont == NULL)
    {
        r->cont = DEFAULT_CONT;
    }
    r->prompt = "";
    repl89_edit_init(&r->edit);
    repl89_hist_init(&r->hist, r->config.history_limit);
    repl89_paste_init(&r->paste);
    repl89_buf_init(&r->draft);
    r->state = REPL89_INACTIVE;
    r->visible = 0;
    r->drawn = 0;
    r->in_paste = 0;
    r->in_len = 0;
    r->error = REPL89_OK;
    memset(&r->region, 0, sizeof r->region);
    return r;
}

/* ---- Session drawing ----------------------------------------------------- */

static int session_emit(void *ctx, const char *p, size_t n)
{
    repl89 *r;
    int w;

    r = (repl89 *)ctx;
    w = repl89_tty_write(&r->tty, p, n);
    return w;
}

static void session_sink(repl89 *r, repl89_sink *sink)
{
    sink->ctx = r;
    sink->emit = session_emit;
}

static repl89_error session_finalize(repl89 *r)
{
    repl89_sink sink;
    repl89_error err;

    if (r->drawn == 0)
    {
        return REPL89_OK;
    }
    session_sink(r, &sink);
    err = repl89_render_finalize(&sink, &r->region);
    return err;
}

static repl89_error session_clear(repl89 *r)
{
    repl89_sink sink;
    repl89_error err;

    if (r->drawn == 0)
    {
        return REPL89_OK;
    }
    session_sink(r, &sink);
    err = repl89_render_clear(&sink, &r->region);
    return err;
}

static repl89_error session_draw(repl89 *r)
{
    repl89_sink sink;
    repl89_error err;

    if (r->visible == 0)
    {
        return REPL89_OK;
    }
    session_sink(r, &sink);
    err = session_clear(r);
    if (err != REPL89_OK)
    {
        return err;
    }
    err = repl89_render_draw(&r->edit, r->prompt, r->cont, r->tty.cols,
                             r->config.tab_width, &sink, &r->region);
    if (err != REPL89_OK)
    {
        return err;
    }
    r->drawn = 1;
    return REPL89_OK;
}

/* Move the cursor only: the displayed text has not changed. */
static repl89_error session_cursor(repl89 *r)
{
    repl89_sink sink;
    repl89_region next;
    repl89_error err;

    session_sink(r, &sink);
    repl89_layout(&r->edit, r->prompt, r->cont, r->tty.cols,
                  r->config.tab_width, &next);
    err = repl89_render_cursor(&sink, &r->region, &next);
    if (err != REPL89_OK)
    {
        return err;
    }
    r->region = next;
    return REPL89_OK;
}

/* Repaint changed content over the region currently on screen. */
static repl89_error session_content(repl89 *r)
{
    repl89_sink sink;
    repl89_region next;
    repl89_error err;

    session_sink(r, &sink);
    err = repl89_render_content(&r->edit, r->prompt, r->cont, r->tty.cols,
                                r->config.tab_width, &sink, &r->region, &next);
    if (err != REPL89_OK)
    {
        return err;
    }
    r->region = next;
    r->drawn = 1;
    return REPL89_OK;
}

/* Repaint after a resize using the old region's own width to reconstruct
   where it ended up physically under the new geometry. */
static repl89_error session_resize(repl89 *r)
{
    repl89_sink sink;
    repl89_region next;
    repl89_error err;

    session_sink(r, &sink);
    err = repl89_render_resize(&r->edit, r->prompt, r->cont, r->tty.cols,
                               r->config.tab_width, &sink, &r->region, &next);
    if (err != REPL89_OK)
    {
        return err;
    }
    r->region = next;
    r->drawn = 1;
    return REPL89_OK;
}

/* Apply the strongest display effect accumulated while draining input. */
static repl89_error session_update(repl89 *r, repl89_update update)
{
    repl89_error err;

    if (r->visible == 0)
    {
        return REPL89_OK;
    }
    if (update == REPL89_UPDATE_NONE)
    {
        return REPL89_OK;
    }
    if (r->drawn == 0)
    {
        err = session_draw(r);
        return err;
    }
    if (update == REPL89_UPDATE_CURSOR)
    {
        err = session_cursor(r);
        return err;
    }
    if (update == REPL89_UPDATE_CONTENT)
    {
        err = session_content(r);
        return err;
    }
    err = session_resize(r);
    return err;
}

static repl89_error session_end(repl89 *r, int finalize)
{
    repl89_error err;
    repl89_error leave;

    err = REPL89_OK;
    if (finalize != 0)
    {
        err = session_finalize(r);
    }
    leave = repl89_tty_leave(&r->tty);
    if (err == REPL89_OK)
    {
        err = leave;
    }
    r->state = REPL89_INACTIVE;
    r->visible = 0;
    r->drawn = 0;
    return err;
}

void repl89_free(repl89 *r)
{
    if (r == NULL)
    {
        return;
    }
    if (r->state == REPL89_ACTIVE)
    {
        session_end(r, 0);
    }
    repl89_edit_free(&r->edit);
    repl89_hist_free(&r->hist);
    repl89_paste_free(&r->paste);
    repl89_buf_free(&r->draft);
    repl89_mem_free(r);
}

/* ---- Prompt validation --------------------------------------------------- */

/* 0 = done, 1 = continue, -1 = malformed UTF-8, -2 = control scalar. */
static int prompt_step(const char *p, size_t n, size_t *pos)
{
    u89_cp cp;
    u89_status st;
    size_t next;
    int ctl;

    if (*pos >= n)
    {
        return 0;
    }
    st = u89_utf8_decode((const unsigned char *)p, n, *pos, &cp, &next);
    if (st != U89_OK)
    {
        return -1;
    }
    ctl = u89_is_control(cp);
    if (ctl)
    {
        return -2;
    }
    *pos = next;
    return 1;
}

static repl89_error prompt_check(const char *p)
{
    size_t n;
    size_t pos;
    int r;

    if (p == NULL)
    {
        return REPL89_OK;
    }
    n = strlen(p);
    pos = 0;
    r = 1;
    while (r == 1)
    {
        r = prompt_step(p, n, &pos);
    }
    if (r == 0)
    {
        return REPL89_OK;
    }
    if (r == -1)
    {
        return REPL89_EUTF8;
    }
    return REPL89_EINVAL;
}

/* ---- History navigation -------------------------------------------------- */

static repl89_error edit_load(repl89 *r, const repl89_entry *e)
{
    repl89_error err;

    repl89_edit_reset(&r->edit);
    if (e == NULL)
    {
        return REPL89_OK;
    }
    err = repl89_edit_insert(&r->edit, e->data, e->len);
    return err;
}

static void draft_save(repl89 *r)
{
    repl89_buf_clear(&r->draft);
    repl89_buf_insert(&r->draft, 0, r->edit.buf.data, r->edit.buf.len);
}

static repl89_error draft_restore(repl89 *r)
{
    repl89_error err;

    repl89_edit_reset(&r->edit);
    if (r->draft.len == 0)
    {
        return REPL89_OK;
    }
    err = repl89_edit_insert(&r->edit, r->draft.data, r->draft.len);
    return err;
}

static repl89_update hist_up(repl89 *r)
{
    const repl89_entry *e;
    repl89_error err;

    if (r->hist.nav == r->hist.count)
    {
        draft_save(r);
    }
    e = repl89_hist_nav_prev(&r->hist);
    if (e == NULL)
    {
        return REPL89_UPDATE_NONE;
    }
    err = edit_load(r, e);
    if (err != REPL89_OK)
    {
        r->error = err;
        return REPL89_UPDATE_NONE;
    }
    return REPL89_UPDATE_CONTENT;
}

static repl89_update hist_down_draft(repl89 *r, size_t before)
{
    repl89_error err;

    if (before == r->hist.nav)
    {
        return REPL89_UPDATE_NONE;
    }
    if (r->hist.nav != r->hist.count)
    {
        return REPL89_UPDATE_NONE;
    }
    err = draft_restore(r);
    if (err != REPL89_OK)
    {
        r->error = err;
        return REPL89_UPDATE_NONE;
    }
    return REPL89_UPDATE_CONTENT;
}

static repl89_update hist_down(repl89 *r)
{
    const repl89_entry *e;
    repl89_error err;
    repl89_update u;
    size_t before;

    before = r->hist.nav;
    e = repl89_hist_nav_next(&r->hist);
    if (e == NULL)
    {
        u = hist_down_draft(r, before);
        return u;
    }
    err = edit_load(r, e);
    if (err != REPL89_OK)
    {
        r->error = err;
        return REPL89_UPDATE_NONE;
    }
    return REPL89_UPDATE_CONTENT;
}

static unsigned int prefer_col_of(const repl89 *r)
{
    if (r->prefer_active != 0)
    {
        return r->prefer_col;
    }
    return REPL89_WANT_CURSOR;
}

static void prefer_begin(repl89 *r, unsigned int col)
{
    r->prefer_col = col;
    r->prefer_active = 1;
}

static repl89_update leave_vertical(repl89 *r, int direction)
{
    repl89_update u;

    r->prefer_active = 0;
    if (direction < 0)
    {
        u = hist_up(r);
        return u;
    }
    u = hist_down(r);
    return u;
}

static repl89_update vertical_move(repl89 *r, int direction)
{
    repl89_vmove vm;
    repl89_update u;
    unsigned int want;

    want = prefer_col_of(r);
    repl89_layout_vertical(&r->edit, r->prompt, r->cont, r->tty.cols,
                           r->config.tab_width, direction, want, &vm);
    if (vm.in_range == 0)
    {
        u = leave_vertical(r, direction);
        return u;
    }
    if (r->prefer_active == 0)
    {
        prefer_begin(r, vm.cur_col);
    }
    if (vm.target == r->edit.cursor)
    {
        return REPL89_UPDATE_NONE;
    }
    r->edit.cursor = vm.target;
    return REPL89_UPDATE_CURSOR;
}

/* ---- Paste --------------------------------------------------------------- */

static int find_marker(const char *p, size_t n)
{
    static const char mark[] = "\x1b[201~";
    size_t i;
    int same;

    if (n < REPL89_MARK_LEN)
    {
        return -1;
    }
    for (i = 0; i + REPL89_MARK_LEN <= n; ++i)
    {
        same = memcmp(p + i, mark, REPL89_MARK_LEN);
        if (same == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

static repl89_update paste_insert(repl89 *r)
{
    repl89_text t;
    repl89_error err;

    err = repl89_paste_finish(&r->paste, &t);
    if (err != REPL89_OK)
    {
        r->error = err;
        return REPL89_UPDATE_NONE;
    }
    if (t.len == 0)
    {
        repl89_paste_reset(&r->paste);
        return REPL89_UPDATE_NONE;
    }
    err = repl89_edit_insert(&r->edit, t.data, t.len);
    if (err != REPL89_OK)
    {
        r->error = err;
        return REPL89_UPDATE_NONE;
    }
    r->prefer_active = 0;
    repl89_paste_reset(&r->paste);
    return REPL89_UPDATE_CONTENT;
}

static repl89_update paste_take(repl89 *r, size_t at)
{
    repl89_update u;

    repl89_paste_feed(&r->paste, r->inbuf, at);
    memmove(r->inbuf, r->inbuf + at + REPL89_MARK_LEN,
            r->in_len - at - REPL89_MARK_LEN);
    r->in_len = r->in_len - at - REPL89_MARK_LEN;
    r->in_paste = 0;
    u = paste_insert(r);
    return u;
}

/* Returns 1 when progress was made, 0 when more input is required. */
static int paste_consume(repl89 *r, repl89_update *update)
{
    int at;
    size_t feed;

    at = find_marker(r->inbuf, r->in_len);
    if (at >= 0)
    {
        *update = paste_take(r, (size_t)at);
        return 1;
    }
    if (r->in_len <= 5)
    {
        return 0;
    }
    feed = r->in_len - 5;
    repl89_paste_feed(&r->paste, r->inbuf, feed);
    memmove(r->inbuf, r->inbuf + feed, r->in_len - feed);
    r->in_len = r->in_len - feed;
    return 1;
}

static void paste_begin(repl89 *r, size_t used)
{
    r->in_paste = 1;
    repl89_paste_reset(&r->paste);
    memmove(r->inbuf, r->inbuf + used, r->in_len - used);
    r->in_len = r->in_len - used;
}

/* ---- Input processing ---------------------------------------------------- */

static void consume(repl89 *r, size_t n)
{
    memmove(r->inbuf, r->inbuf + n, r->in_len - n);
    r->in_len = r->in_len - n;
}

static repl89_update cursor_change(size_t before, size_t after)
{
    if (before == after)
    {
        return REPL89_UPDATE_NONE;
    }
    return REPL89_UPDATE_CURSOR;
}

static repl89_update content_change(size_t before, size_t after)
{
    if (before == after)
    {
        return REPL89_UPDATE_NONE;
    }
    return REPL89_UPDATE_CONTENT;
}

static repl89_update apply_cursor(repl89 *r, repl89_key_type type)
{
    repl89_update u;
    size_t before;

    before = r->edit.cursor;
    if (type == REPL89_KEY_LEFT)
    {
        repl89_edit_left(&r->edit);
    }
    else if (type == REPL89_KEY_RIGHT)
    {
        repl89_edit_right(&r->edit);
    }
    else if (type == REPL89_KEY_HOME)
    {
        repl89_edit_home(&r->edit);
    }
    else
    {
        repl89_edit_end(&r->edit);
    }
    u = cursor_change(before, r->edit.cursor);
    return u;
}

static repl89_update apply_delete(repl89 *r, repl89_key_type type)
{
    repl89_update u;
    size_t before;

    before = r->edit.buf.len;
    if (type == REPL89_KEY_BACKSPACE)
    {
        repl89_edit_delete_prev(&r->edit);
    }
    else if (type == REPL89_KEY_DELETE)
    {
        repl89_edit_delete_next(&r->edit);
    }
    else if (type == REPL89_KEY_CTRL_U)
    {
        repl89_edit_kill_begin(&r->edit);
    }
    else
    {
        repl89_edit_kill_end(&r->edit);
    }
    u = content_change(before, r->edit.buf.len);
    return u;
}

static repl89_update apply_insert(repl89 *r, const char *p, size_t n)
{
    repl89_error err;

    err = repl89_edit_insert(&r->edit, p, n);
    if (err != REPL89_OK)
    {
        r->error = err;
        return REPL89_UPDATE_NONE;
    }
    if (n == 0)
    {
        return REPL89_UPDATE_NONE;
    }
    return REPL89_UPDATE_CONTENT;
}

static int is_cursor_key(repl89_key_type type)
{
    if (type == REPL89_KEY_LEFT)
    {
        return 1;
    }
    if (type == REPL89_KEY_RIGHT)
    {
        return 1;
    }
    if (type == REPL89_KEY_HOME)
    {
        return 1;
    }
    if (type == REPL89_KEY_END)
    {
        return 1;
    }
    return 0;
}

static int is_delete_key(repl89_key_type type)
{
    if (type == REPL89_KEY_BACKSPACE)
    {
        return 1;
    }
    if (type == REPL89_KEY_DELETE)
    {
        return 1;
    }
    if (type == REPL89_KEY_CTRL_U)
    {
        return 1;
    }
    if (type == REPL89_KEY_CTRL_K)
    {
        return 1;
    }
    return 0;
}

static repl89_update apply_edit_key(repl89 *r, const repl89_key *key)
{
    repl89_update u;
    int cursor;
    int del;

    cursor = is_cursor_key(key->type);
    if (cursor != 0)
    {
        u = apply_cursor(r, key->type);
        return u;
    }
    del = is_delete_key(key->type);
    if (del != 0)
    {
        u = apply_delete(r, key->type);
        return u;
    }
    if (key->type == REPL89_KEY_CTRL_P)
    {
        u = hist_up(r);
        return u;
    }
    if (key->type == REPL89_KEY_CTRL_N)
    {
        u = hist_down(r);
        return u;
    }
    return REPL89_UPDATE_NONE;
}

static repl89_update apply_plain_key(repl89 *r, const repl89_key *key)
{
    repl89_update u;

    if (key->type == REPL89_KEY_TEXT)
    {
        u = apply_insert(r, r->inbuf, key->len);
        return u;
    }
    if (key->type == REPL89_KEY_LF)
    {
        u = apply_insert(r, "\n", 1);
        return u;
    }
    u = apply_edit_key(r, key);
    return u;
}

static repl89_update apply_key(repl89 *r, const repl89_key *key)
{
    repl89_update u;

    if (key->type == REPL89_KEY_UP)
    {
        u = vertical_move(r, -1);
        return u;
    }
    if (key->type == REPL89_KEY_DOWN)
    {
        u = vertical_move(r, 1);
        return u;
    }
    r->prefer_active = 0;
    u = apply_plain_key(r, key);
    return u;
}

static void feed_apply(repl89 *r, const repl89_key *key, size_t used,
                       repl89_update *update)
{
    *update = apply_key(r, key);
    consume(r, used);
}

static int feed_event(repl89 *r, repl89_event *event, size_t used,
                      repl89_event ev)
{
    consume(r, used);
    *event = ev;
    return 2;
}

static int eof_event(repl89 *r, repl89_event *event)
{
    repl89_error err;

    err = session_end(r, 1);
    *event = REPL89_EVENT_EOF;
    if (err != REPL89_OK)
    {
        r->error = err;
        return -1;
    }
    return 2;
}

static repl89_update update_join(repl89_update a, repl89_update b)
{
    if ((int)a > (int)b)
    {
        return a;
    }
    return b;
}

static repl89_update delete_next_update(repl89 *r)
{
    repl89_update u;
    size_t before;

    before = r->edit.buf.len;
    repl89_edit_delete_next(&r->edit);
    u = content_change(before, r->edit.buf.len);
    return u;
}

/* 0 = need more input, 1 = processed, 2 = event, -1 = error. */
static int feed_step(repl89 *r, repl89_event *event, repl89_update *update)
{
    repl89_key key;
    size_t used;
    int st;

    *update = REPL89_UPDATE_NONE;
    if (r->in_len == 0)
    {
        return 0;
    }
    if (r->in_paste != 0)
    {
        st = paste_consume(r, update);
        if (r->error != REPL89_OK)
        {
            return -1;
        }
        if (st == 0)
        {
            return 0;
        }
        return 1;
    }
    used = repl89_key_next(r->inbuf, r->in_len, &key);
    if (used == 0)
    {
        return 0;
    }
    if (key.type == REPL89_KEY_PASTE_BEGIN)
    {
        paste_begin(r, used);
        return 1;
    }
    if (key.type == REPL89_KEY_SUBMIT)
    {
        st = feed_event(r, event, used, REPL89_EVENT_SUBMIT);
        return st;
    }
    if (key.type == REPL89_KEY_CANCEL)
    {
        st = feed_event(r, event, used, REPL89_EVENT_CANCEL);
        return st;
    }
    if (key.type == REPL89_KEY_CTRL_D)
    {
        consume(r, used);
        if (r->edit.buf.len == 0)
        {
            st = eof_event(r, event);
            return st;
        }
        *update = delete_next_update(r);
        return 1;
    }
    if (key.type == REPL89_KEY_PASTE_END)
    {
        consume(r, used);
        return 1;
    }
    if (key.type == REPL89_KEY_IGNORE)
    {
        consume(r, used);
        return 1;
    }
    feed_apply(r, &key, used, update);
    if (r->error != REPL89_OK)
    {
        return -1;
    }
    return 1;
}

static int drain_one(repl89 *r, repl89_event *event, repl89_update *update)
{
    repl89_update one;
    int st;

    st = feed_step(r, event, &one);
    *update = update_join(*update, one);
    return st;
}

static int drain(repl89 *r, repl89_event *event, repl89_update *update)
{
    int st;

    *update = REPL89_UPDATE_NONE;
    st = drain_one(r, event, update);
    while (st == 1)
    {
        st = drain_one(r, event, update);
    }
    return st;
}

static repl89_error read_error(void)
{
    if (errno == EINTR)
    {
        return REPL89_OK;
    }
    if (errno == EAGAIN)
    {
        return REPL89_OK;
    }
    if (errno == EWOULDBLOCK)
    {
        return REPL89_OK;
    }
    return REPL89_EIO;
}

/* ---- Public session operations ------------------------------------------- */

repl89_error repl89_start(repl89 *r, const char *prompt)
{
    repl89_error err;

    if (r->state == REPL89_ACTIVE)
    {
        return REPL89_ESTATE;
    }
    err = prompt_check(prompt);
    if (err != REPL89_OK)
    {
        return err;
    }
    err = repl89_tty_enter(&r->tty, r->config.input_fd, r->config.output_fd);
    if (err != REPL89_OK)
    {
        return err;
    }
    err = repl89_tty_size(&r->tty);
    if (err != REPL89_OK)
    {
        repl89_tty_leave(&r->tty);
        return err;
    }
    r->prompt = prompt;
    if (r->prompt == NULL)
    {
        r->prompt = "";
    }
    repl89_edit_reset(&r->edit);
    repl89_paste_reset(&r->paste);
    repl89_hist_nav_reset(&r->hist);
    r->in_paste = 0;
    r->prefer_active = 0;
    r->state = REPL89_ACTIVE;
    r->visible = 1;
    r->drawn = 0;
    memset(&r->region, 0, sizeof r->region);
    err = session_update(r, REPL89_UPDATE_CONTENT);
    if (err != REPL89_OK)
    {
        session_end(r, 0);
        return err;
    }
    return REPL89_OK;
}

repl89_error repl89_feed(repl89 *r, repl89_event *event)
{
    int st;
    ssize_t got;
    repl89_update update;
    repl89_error err;

    if (event == NULL)
    {
        return REPL89_EINVAL;
    }
    *event = REPL89_EVENT_NONE;
    if (r->state != REPL89_ACTIVE)
    {
        return REPL89_ESTATE;
    }
    r->error = REPL89_OK;
    st = drain(r, event, &update);
    if (st == -1)
    {
        return r->error;
    }
    err = session_update(r, update);
    if (err != REPL89_OK)
    {
        return err;
    }
    if (st == 2)
    {
        return REPL89_OK;
    }
    got = repl89_tty_read(&r->tty, r->inbuf + r->in_len,
                          sizeof r->inbuf - r->in_len);
    if (got < 0)
    {
        err = read_error();
        return err;
    }
    if (got == 0)
    {
        eof_event(r, event);
        return REPL89_OK;
    }
    r->in_len = r->in_len + (size_t)got;
    st = drain(r, event, &update);
    if (st == -1)
    {
        return r->error;
    }
    err = session_update(r, update);
    return err;
}

static void text_clear(repl89_text *text)
{
    if (text == NULL)
    {
        return;
    }
    text->data = NULL;
    text->len = 0;
}

static void set_error(repl89_error *error, repl89_error err)
{
    if (error != NULL)
    {
        *error = err;
    }
}

repl89_result repl89_read(repl89 *r, const char *prompt, repl89_text *text,
                          repl89_error *error)
{
    repl89_error err;
    repl89_event ev;

    set_error(error, REPL89_OK);
    text_clear(text);
    err = repl89_start(r, prompt);
    if (err != REPL89_OK)
    {
        set_error(error, err);
        return REPL89_RESULT_ERROR;
    }
    for (;;)
    {
        err = repl89_feed(r, &ev);
        if (err != REPL89_OK)
        {
            if (r->state == REPL89_ACTIVE)
            {
                repl89_cancel(r);
            }
            set_error(error, err);
            return REPL89_RESULT_ERROR;
        }
        if (ev == REPL89_EVENT_SUBMIT)
        {
            err = repl89_submit(r);
            if (err != REPL89_OK)
            {
                set_error(error, err);
                return REPL89_RESULT_ERROR;
            }
            if (text != NULL)
            {
                *text = repl89_text_get(r);
            }
            return REPL89_RESULT_SUBMIT;
        }
        if (ev == REPL89_EVENT_CANCEL)
        {
            err = repl89_cancel(r);
            if (err != REPL89_OK)
            {
                set_error(error, err);
                return REPL89_RESULT_ERROR;
            }
            return REPL89_RESULT_CANCEL;
        }
        if (ev == REPL89_EVENT_EOF)
        {
            return REPL89_RESULT_EOF;
        }
    }
}

repl89_error repl89_submit(repl89 *r)
{
    repl89_error err;

    if (r->state != REPL89_ACTIVE)
    {
        return REPL89_ESTATE;
    }
    err = session_end(r, 1);
    return err;
}

repl89_error repl89_cancel(repl89 *r)
{
    repl89_error err;

    if (r->state != REPL89_ACTIVE)
    {
        return REPL89_ESTATE;
    }
    err = session_end(r, 1);
    repl89_edit_reset(&r->edit);
    return err;
}

repl89_error repl89_insert(repl89 *r, const char *text, size_t len)
{
    repl89_error err;

    if (r->state != REPL89_ACTIVE)
    {
        return REPL89_ESTATE;
    }
    r->prefer_active = 0;
    err = repl89_edit_insert(&r->edit, text, len);
    if (err != REPL89_OK)
    {
        return err;
    }
    err = session_update(r, REPL89_UPDATE_CONTENT);
    return err;
}

repl89_text repl89_text_get(const repl89 *r)
{
    repl89_text t;

    t.data = r->edit.buf.data;
    t.len = r->edit.buf.len;
    return t;
}

repl89_error repl89_history_add(repl89 *r, const char *text, size_t len)
{
    repl89_error err;

    err = repl89_hist_add(&r->hist, text, len);
    return err;
}

void repl89_history_clear(repl89 *r)
{
    repl89_hist_clear(&r->hist);
}

repl89_error repl89_resize(repl89 *r)
{
    unsigned int old_cols;
    unsigned int old_rows;
    repl89_error err;

    if (r->state != REPL89_ACTIVE)
    {
        return REPL89_ESTATE;
    }
    old_cols = r->tty.cols;
    old_rows = r->tty.rows;
    err = repl89_tty_size(&r->tty);
    if (err != REPL89_OK)
    {
        return err;
    }
    if (r->tty.cols == old_cols)
    {
        if (r->tty.rows == old_rows)
        {
            return REPL89_OK;
        }
    }
    err = session_update(r, REPL89_UPDATE_RESIZE);
    return err;
}

repl89_error repl89_hide(repl89 *r)
{
    repl89_error err;

    if (r->state != REPL89_ACTIVE)
    {
        return REPL89_ESTATE;
    }
    if (r->visible == 0)
    {
        return REPL89_ESTATE;
    }
    err = session_clear(r);
    r->visible = 0;
    r->drawn = 0;
    return err;
}

repl89_error repl89_show(repl89 *r)
{
    repl89_error err;

    if (r->state != REPL89_ACTIVE)
    {
        return REPL89_ESTATE;
    }
    if (r->visible != 0)
    {
        return REPL89_ESTATE;
    }
    r->visible = 1;
    err = session_update(r, REPL89_UPDATE_CONTENT);
    return err;
}
