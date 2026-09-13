#define _DEFAULT_SOURCE 1

#include <fcntl.h>
#include <pty.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "test.h"
#include "repl89.h"
#include "vt.h"

typedef struct harness {
    int master;
    int slave;
    repl89 *r;
} harness;

static int harness_open(harness *h)
{
    repl89_config cfg;
    struct winsize ws;
    struct termios raw;
    int flags;
    int r;

    h->master = -1;
    h->slave = -1;
    r = openpty(&h->master, &h->slave, NULL, NULL, NULL);
    if (r != 0) {
        return -1;
    }
    memset(&ws, 0, sizeof ws);
    ws.ws_row = 24;
    ws.ws_col = 40;
    ioctl(h->slave, TIOCSWINSZ, &ws);
    flags = fcntl(h->slave, F_GETFL, 0);
    fcntl(h->slave, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(h->master, F_GETFL, 0);
    fcntl(h->master, F_SETFL, flags | O_NONBLOCK);
    memset(&raw, 0, sizeof raw);
    if (tcgetattr(h->slave, &raw) == 0) {
        raw.c_lflag = raw.c_lflag & (tcflag_t) ~(ICANON | ECHO | ISIG);
        raw.c_iflag = raw.c_iflag & (tcflag_t) ~(ICRNL | IXON);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(h->slave, TCSANOW, &raw);
    }
    memset(&cfg, 0, sizeof cfg);
    cfg.input_fd = h->slave;
    cfg.output_fd = h->slave;
    cfg.tab_width = 8;
    cfg.history_limit = 10;
    cfg.continuation_prompt = "... ";
    h->r = repl89_new(&cfg);
    return 0;
}

static void harness_close(harness *h)
{
    repl89_free(h->r);
    close(h->master);
    close(h->slave);
}

static void send(harness *h, const char *p, size_t n)
{
    ssize_t w;

    w = write(h->master, p, n);
    rp_check(w == (ssize_t)n, "send bytes");
}

static void text_is(const repl89 *r, const char *want, const char *what)
{
    repl89_text t;
    size_t wl;
    char ctx[96];

    sprintf(ctx, "%s", what);
    t = repl89_text_get(r);
    wl = strlen(want);
    rp_check_ctx(t.len == wl && memcmp(t.data, want, wl) == 0, "text", ctx);
}

static void test_state(void)
{
    harness h;
    repl89_event ev;
    repl89_error err;

    rp_check(harness_open(&h) == 0, "open state");
    ev = REPL89_EVENT_SUBMIT;
    err = repl89_feed(h.r, &ev);
    rp_check(err == REPL89_ESTATE, "API-04 feed before start");
    rp_check(ev == REPL89_EVENT_NONE, "API-04 no stale event");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start");
    rp_check(repl89_start(h.r, "> ") == REPL89_ESTATE, "API-01 start twice");
    rp_check(repl89_insert(h.r, "abc", 3) == REPL89_OK, "insert");
    text_is(h.r, "abc", "inserted");
    rp_check(repl89_submit(h.r) == REPL89_OK, "submit");
    text_is(h.r, "abc", "API-06 text after submit");
    rp_check(repl89_submit(h.r) == REPL89_ESTATE, "API-02 submit twice");
    rp_check(repl89_insert(h.r, "x", 1) == REPL89_ESTATE, "API-05 insert after");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "ST-08 start again");
    text_is(h.r, "", "buffer cleared");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel");
    text_is(h.r, "", "API-07 text after cancel");
    rp_check(repl89_cancel(h.r) == REPL89_ESTATE, "API-03 cancel twice");
    harness_close(&h);
}

static void test_feed_keys(void)
{
    harness h;
    repl89_event ev;

    rp_check(harness_open(&h) == 0, "open feed");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start feed");

    send(&h, "abc\x7F", 4);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK && ev == REPL89_EVENT_NONE,
             "feed edits");
    text_is(h.r, "ab", "backspace applied");

    send(&h, "\x01" "X", 2);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed home");
    text_is(h.r, "Xab", "home + insert");

    send(&h, "\x1b[3~", 4);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed delete");
    text_is(h.r, "Xb", "delete applied");

    send(&h, "\r", 1);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed enter");
    rp_check(ev == REPL89_EVENT_SUBMIT, "IO-07 submit event");
    text_is(h.r, "Xb", "buffer preserved at submit event");
    rp_check(repl89_submit(h.r) == REPL89_OK, "accept submit");
    harness_close(&h);
}

static void test_queue(void)
{
    harness h;
    repl89_event ev;

    rp_check(harness_open(&h) == 0, "open queue");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start queue");

    send(&h, "abc\rxyz", 7);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed submit");
    rp_check(ev == REPL89_EVENT_SUBMIT, "QI-02 submit");
    text_is(h.r, "abc", "QI-02 text");
    rp_check(repl89_submit(h.r) == REPL89_OK, "submit queued");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "restart queued");
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "QI-04 feed queued");
    rp_check(ev == REPL89_EVENT_NONE, "QI-04 no event");
    text_is(h.r, "xyz", "QI-04 queued text");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel queued");
    harness_close(&h);
}

static void test_session_paste(void)
{
    harness h;
    repl89_event ev;

    rp_check(harness_open(&h) == 0, "open paste");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start paste");

    send(&h, "\x1b[200~a\r\nb\x1b[201~", 16);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed paste");
    rp_check(ev == REPL89_EVENT_NONE, "PS-16 paste no submit");
    text_is(h.r, "a\nb", "PS-03 paste normalized");

    send(&h, "\r", 1);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed enter after paste");
    rp_check(ev == REPL89_EVENT_SUBMIT, "paste then submit");
    rp_check(repl89_submit(h.r) == REPL89_OK, "submit paste");

    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start paste queue");
    send(&h, "\x1b[200~x\x1b[201~yz", 15);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed paste + text");
    rp_check(ev == REPL89_EVENT_NONE, "paste + text no submit");
    text_is(h.r, "xyz", "QI-08 paste followed by text");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel paste queue");
    harness_close(&h);
}

static void drain_output(harness *h, char *buf, size_t cap, size_t *len)
{
    ssize_t n;
    size_t off;

    off = 0;
    for (;;) {
        n = read(h->master, buf + off, cap - off);
        if (n <= 0) {
            break;
        }
        off = off + (size_t)n;
        if (off == cap) {
            break;
        }
    }
    *len = off;
}

static int contains(const char *hay, size_t n, const char *needle)
{
    size_t m;
    size_t i;

    m = strlen(needle);
    if (m > n) {
        return 0;
    }
    for (i = 0; i + m <= n; ++i) {
        if (memcmp(hay + i, needle, m) == 0) {
            return 1;
        }
    }
    return 0;
}

static void test_queued_redraw(void)
{
    harness h;
    repl89_event ev;
    char out[4096];
    size_t len;

    rp_check(harness_open(&h) == 0, "open redraw");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start redraw");
    send(&h, "abc\rxyz", 7);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed submit");
    rp_check(ev == REPL89_EVENT_SUBMIT, "redraw submit event");
    rp_check(repl89_submit(h.r) == REPL89_OK, "submit redraw");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start redraw 2");
    drain_output(&h, out, sizeof out, &len);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed queued edits");
    rp_check(ev == REPL89_EVENT_NONE, "queued edits no event");
    drain_output(&h, out, sizeof out, &len);
    rp_check(contains(out, len, "xyz") == 1, "queued edits redrawn");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel redraw");
    harness_close(&h);
}

static void test_cursor_only(void)
{
    harness h;
    repl89_event ev;
    char out[4096];
    size_t len;

    rp_check(harness_open(&h) == 0, "open cursor-only");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start cursor-only");
    send(&h, "abc", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed text");
    drain_output(&h, out, sizeof out, &len);
    rp_check(contains(out, len, "abc") == 1, "ST-16 content drawn");

    send(&h, "\x1b[D", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed left");
    drain_output(&h, out, sizeof out, &len);
    rp_check(len > 0, "ST-14 motion emitted");
    rp_check(rp_only_motion(out, len) == 1, "ST-14 motion only");
    rp_check(contains(out, len, "abc") == 0, "ST-14 no content");
    rp_check(contains(out, len, "\x1b[2K") == 0, "ST-14 no erase");

    send(&h, "\x1b[C", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed right");
    drain_output(&h, out, sizeof out, &len);
    rp_check(rp_only_motion(out, len) == 1, "ST-14 right motion only");

    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel cursor-only");
    harness_close(&h);
}

static void test_cursor_only_vertical(void)
{
    harness h;
    repl89_event ev;
    char out[4096];
    char text[128];
    size_t len;
    size_t i;

    rp_check(harness_open(&h) == 0, "open cursor vertical");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start cursor vertical");
    for (i = 0; i < 100; i++) {
        text[i] = 'a';
    }
    text[100] = 0;
    rp_check(repl89_insert(h.r, text, 100) == REPL89_OK, "insert long");
    drain_output(&h, out, sizeof out, &len);

    send(&h, "\x1b[A", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed up");
    drain_output(&h, out, sizeof out, &len);
    rp_check(rp_only_motion(out, len) == 1, "ST-14 up motion only");
    rp_check(contains(out, len, "\x1b[1A") == 1, "ST-14 up escape");

    send(&h, "\x1b[B", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed down");
    drain_output(&h, out, sizeof out, &len);
    rp_check(rp_only_motion(out, len) == 1, "ST-14 down motion only");

    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel cursor vertical");
    harness_close(&h);
}

static void test_repaint_strategy(void)
{
    harness h;
    repl89_event ev;
    char out[4096];
    size_t len;

    rp_check(harness_open(&h) == 0, "open repaint");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start repaint");
    send(&h, "abc", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed text");
    drain_output(&h, out, sizeof out, &len);
    rp_check(contains(out, len, "abc") == 1, "repaint content drawn");

    send(&h, "\x7f", 1);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed backspace");
    drain_output(&h, out, sizeof out, &len);
    rp_check(contains(out, len, "ab") == 1, "ST-19 content overwritten");
    rp_check(contains(out, len, "\x1b[K") == 1, "ST-19 suffix erased");
    rp_check(contains(out, len, "\x1b[2K") == 0, "ST-19 no clear first");

    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel repaint");
    harness_close(&h);
}

static void test_noop_emits_nothing(void)
{
    harness h;
    repl89_event ev;
    char out[1024];
    size_t len;

    rp_check(harness_open(&h) == 0, "open noop");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start noop");
    drain_output(&h, out, sizeof out, &len);

    send(&h, "\x1b[D", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed left at start");
    drain_output(&h, out, sizeof out, &len);
    rp_check(len == 0, "ST-15 left at start silent");

    send(&h, "\x1b[3~", 4);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed delete at end");
    drain_output(&h, out, sizeof out, &len);
    rp_check(len == 0, "ST-15 delete at end silent");

    send(&h, "\x10", 1);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed ctrl-p empty");
    drain_output(&h, out, sizeof out, &len);
    rp_check(len == 0, "ST-15 ctrl-p empty silent");

    send(&h, "\x1b[Z", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed ignored sequence");
    drain_output(&h, out, sizeof out, &len);
    rp_check(len == 0, "ST-15 ignored sequence silent");

    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel noop");
    harness_close(&h);
}

static void test_ctrl_d(void)
{
    harness h;
    repl89_event ev;

    rp_check(harness_open(&h) == 0, "open ctrl-d");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start ctrl-d");

    send(&h, "ab\x01\x04", 4);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed ctrl-d delete");
    rp_check(ev == REPL89_EVENT_NONE, "ST-06 nonempty deletes");
    text_is(h.r, "b", "ST-06 deleted next grapheme");

    send(&h, "\x04", 1);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed ctrl-d delete last");
    rp_check(ev == REPL89_EVENT_NONE, "ST-06 second delete");
    text_is(h.r, "", "ST-06 buffer empty");

    send(&h, "\x04", 1);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed ctrl-d empty");
    rp_check(ev == REPL89_EVENT_EOF, "ST-05 empty EOF");
    rp_check(repl89_submit(h.r) == REPL89_ESTATE, "EOF ended session");
    harness_close(&h);
}

static void test_read(void)
{
    harness h;
    repl89_text t;
    repl89_error err;
    repl89_result res;

    rp_check(harness_open(&h) == 0, "open read");
    send(&h, "hello\r", 6);
    res = repl89_read(h.r, "> ", &t, &err);
    rp_check(res == REPL89_RESULT_SUBMIT, "read submit");
    rp_check(err == REPL89_OK, "read no error");
    rp_check(t.len == 5 && memcmp(t.data, "hello", 5) == 0, "read text");

    send(&h, "\x03", 1);
    res = repl89_read(h.r, "> ", &t, &err);
    rp_check(res == REPL89_RESULT_CANCEL, "read cancel");

    send(&h, "\x04", 1);
    res = repl89_read(h.r, "> ", &t, &err);
    rp_check(res == REPL89_RESULT_EOF, "read EOF");

    res = repl89_read(h.r, "\xC0\x80", &t, &err);
    rp_check(res == REPL89_RESULT_ERROR && err == REPL89_EUTF8,
             "read bad prompt");
    harness_close(&h);
}

static void test_terminal_restore(void)
{
    harness h;
    struct termios before;
    struct termios during;
    struct termios after;

    rp_check(harness_open(&h) == 0, "open restore");
    rp_check(tcgetattr(h.slave, &before) == 0, "before");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start restore");
    rp_check(tcgetattr(h.slave, &during) == 0, "during");
    rp_check((during.c_lflag & ICANON) == 0, "raw during");
    repl89_free(h.r);
    rp_check(tcgetattr(h.slave, &after) == 0, "after");
    rp_check(memcmp(&before, &after, sizeof before) == 0,
             "TY-04 free restores termios");
    close(h.master);
    close(h.slave);
}

static void test_resize_hide_show(void)
{
    harness h;

    rp_check(harness_open(&h) == 0, "open view");
    rp_check(repl89_resize(h.r) == REPL89_ESTATE, "RS-01 inactive resize");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start view");
    rp_check(repl89_resize(h.r) == REPL89_OK, "resize");
    rp_check(repl89_hide(h.r) == REPL89_OK, "hide");
    rp_check(repl89_hide(h.r) == REPL89_ESTATE, "API-09 double hide");
    rp_check(repl89_show(h.r) == REPL89_OK, "show");
    rp_check(repl89_show(h.r) == REPL89_ESTATE, "API-10 show visible");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel view");
    harness_close(&h);
}

static void test_resize_reflow(void)
{
    harness h;
    struct winsize ws;
    vt_screen scr;
    char out[8192];
    char row[128];
    char text[128];
    size_t len;
    size_t i;

    rp_check(harness_open(&h) == 0, "open resize");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start resize");
    for (i = 0; i < 100; i++) {
        text[i] = 'a';
    }
    text[100] = 0;
    rp_check(repl89_insert(h.r, text, 100) == REPL89_OK, "insert long");
    drain_output(&h, out, sizeof out, &len);
    vt_init(&scr);
    vt_set_cols(&scr, 40);
    vt_feed(&scr, out, len);
    rp_check(scr.row == 2 && scr.col == 22, "resize initial cursor");

    /* RS-03: shrink 40 -> 20 reflows and repaints */
    memset(&ws, 0, sizeof ws);
    ws.ws_row = 24;
    ws.ws_col = 20;
    rp_check(ioctl(h.slave, TIOCSWINSZ, &ws) == 0, "shrink winsize");
    vt_resize(&scr, 20);
    rp_check(repl89_resize(h.r) == REPL89_OK, "resize shrink");
    drain_output(&h, out, sizeof out, &len);
    vt_feed(&scr, out, len);
    vt_row_text(&scr, 0, row, sizeof row);
    rp_check(strlen(row) == 20, "RS-03 row0 width");
    vt_row_text(&scr, 5, row, sizeof row);
    rp_check(strcmp(row, "aa") == 0, "RS-03 last row");
    rp_check(scr.row == 5 && scr.col == 2, "RS-03 cursor");

    /* RS-04: grow 20 -> 40 clears the rows the old layout left behind */
    memset(&ws, 0, sizeof ws);
    ws.ws_row = 24;
    ws.ws_col = 40;
    rp_check(ioctl(h.slave, TIOCSWINSZ, &ws) == 0, "grow winsize");
    vt_resize(&scr, 40);
    rp_check(repl89_resize(h.r) == REPL89_OK, "resize grow");
    drain_output(&h, out, sizeof out, &len);
    vt_feed(&scr, out, len);
    vt_row_text(&scr, 2, row, sizeof row);
    rp_check(strlen(row) == 22, "RS-04 last row width");
    vt_row_text(&scr, 3, row, sizeof row);
    rp_check(strlen(row) == 0, "RS-04 stale row cleared");
    rp_check(scr.row == 2 && scr.col == 22, "RS-04 cursor");

    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel resize");
    harness_close(&h);
}

static void test_vertical(void)
{
    harness h;
    repl89_event ev;
    repl89_text t;
    char text[128];
    size_t i;

    rp_check(harness_open(&h) == 0, "open vertical");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start vertical");
    for (i = 0; i < 100; i++) {
        text[i] = 'a';
    }
    text[100] = 0;
    rp_check(repl89_insert(h.r, text, 100) == REPL89_OK, "insert long");

    send(&h, "\x1b[A", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed up");
    rp_check(ev == REPL89_EVENT_NONE, "up no event");
    rp_check(repl89_insert(h.r, "X", 1) == REPL89_OK, "insert X");
    t = repl89_text_get(h.r);
    rp_check(t.len == 101 && t.data[60] == 'X', "ST-10 up target");

    send(&h, "\x1b[A", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed up 2");
    rp_check(repl89_insert(h.r, "Y", 1) == REPL89_OK, "insert Y");
    t = repl89_text_get(h.r);
    rp_check(t.len == 102 && t.data[21] == 'Y', "ST-11 prefer reset");

    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel vertical");

    rp_check(repl89_history_add(h.r, "first", 5) == REPL89_OK, "hist add");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start vertical 2");
    send(&h, "\x1b[A", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed up hist");
    t = repl89_text_get(h.r);
    rp_check(t.len == 5 && memcmp(t.data, "first", 5) == 0,
             "ST-12 up history");
    send(&h, "\x1b[B", 3);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed down draft");
    t = repl89_text_get(h.r);
    rp_check(t.len == 0, "ST-12 down draft");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel vertical 2");
    harness_close(&h);
}

static void test_history_session(void)
{
    harness h;
    repl89_event ev;

    rp_check(harness_open(&h) == 0, "open hist");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start hist");
    rp_check(repl89_insert(h.r, "first", 5) == REPL89_OK, "insert first");
    rp_check(repl89_submit(h.r) == REPL89_OK, "submit first");
    rp_check(repl89_history_add(h.r, "first", 5) == REPL89_OK, "history add");

    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start hist 2");
    rp_check(repl89_insert(h.r, "draft", 5) == REPL89_OK, "insert draft");
    send(&h, "\x10", 1);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed ctrl-p");
    text_is(h.r, "first", "HI-07 history prev");
    send(&h, "\x0E", 1);
    rp_check(repl89_feed(h.r, &ev) == REPL89_OK, "feed ctrl-n");
    text_is(h.r, "draft", "HI-10 draft restored");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel hist");
    harness_close(&h);
}

void test_session(void)
{
    test_state();
    test_feed_keys();
    test_queue();
    test_queued_redraw();
    test_cursor_only();
    test_cursor_only_vertical();
    test_repaint_strategy();
    test_noop_emits_nothing();
    test_session_paste();
    test_ctrl_d();
    test_read();
    test_terminal_restore();
    test_resize_hide_show();
    test_resize_reflow();
    test_vertical();
    test_history_session();
}
