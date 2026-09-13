#define _DEFAULT_SOURCE 1

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "test.h"
#include "repl89.h"
#include "repl89_internal.h"
#include "repl89_tty.h"
#include "u89.h"

static void *fail_alloc(size_t n)
{
    (void)n;
    return NULL;
}

static void *fail_grow(void *p, size_t n)
{
    (void)p;
    (void)n;
    return NULL;
}

static void fail_release(void *p)
{
    (void)p;
}

static int fail_getattr(int fd, struct termios *t)
{
    (void)fd;
    (void)t;
    return -1;
}

static int fail_setattr(int fd, int act, const struct termios *t)
{
    (void)fd;
    (void)act;
    (void)t;
    return -1;
}

static int fail_winsize(int fd, struct winsize *ws)
{
    (void)fd;
    (void)ws;
    return -1;
}

static ssize_t fail_read(int fd, void *p, size_t n)
{
    (void)fd;
    (void)p;
    (void)n;
    errno = EIO;
    return -1;
}

static ssize_t fail_write(int fd, const void *p, size_t n)
{
    (void)fd;
    (void)p;
    (void)n;
    errno = EIO;
    return -1;
}

typedef struct fault_harness {
    int master;
    int slave;
    repl89 *r;
} fault_harness;

static int fault_open(fault_harness *h)
{
    repl89_config cfg;
    struct termios raw;
    struct winsize ws;
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
    memset(&raw, 0, sizeof raw);
    if (tcgetattr(h->slave, &raw) == 0) {
        raw.c_lflag = raw.c_lflag & (tcflag_t) ~(ICANON | ECHO | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(h->slave, TCSANOW, &raw);
    }
    memset(&cfg, 0, sizeof cfg);
    cfg.input_fd = h->slave;
    cfg.output_fd = h->slave;
    cfg.tab_width = 8;
    cfg.history_limit = 4;
    cfg.continuation_prompt = "... ";
    h->r = repl89_new(&cfg);
    return 0;
}

static void fault_close(fault_harness *h)
{
    repl89_free(h->r);
    close(h->master);
    close(h->slave);
}

static void test_tty_faults(void)
{
    fault_harness h;
    repl89_event ev;

    rp_check(fault_open(&h) == 0, "open tty faults");

    repl89_tty_set_hooks(fail_getattr, NULL, NULL, NULL, NULL);
    rp_check(repl89_start(h.r, "> ") == REPL89_ETTY, "tcgetattr failure");
    rp_check(repl89_submit(h.r) == REPL89_ESTATE, "inactive after getattr");
    repl89_tty_reset_hooks();

    repl89_tty_set_hooks(NULL, fail_setattr, NULL, NULL, NULL);
    rp_check(repl89_start(h.r, "> ") == REPL89_ETTY, "tcsetattr failure");
    rp_check(repl89_submit(h.r) == REPL89_ESTATE, "inactive after setattr");
    repl89_tty_reset_hooks();

    repl89_tty_set_hooks(NULL, NULL, fail_winsize, NULL, NULL);
    rp_check(repl89_start(h.r, "> ") == REPL89_EIO, "winsize failure");
    repl89_tty_reset_hooks();
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "restart after winsize");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel after winsize");

    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start read fault");
    repl89_tty_set_hooks(NULL, NULL, NULL, fail_read, NULL);
    ev = REPL89_EVENT_NONE;
    rp_check(repl89_feed(h.r, &ev) == REPL89_EIO, "read failure");
    repl89_tty_reset_hooks();
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel after read fault");

    repl89_tty_set_hooks(NULL, NULL, NULL, NULL, fail_write);
    rp_check(repl89_start(h.r, "> ") == REPL89_EIO, "write failure");
    repl89_tty_reset_hooks();
    rp_check(repl89_submit(h.r) == REPL89_ESTATE, "inactive after write");

    fault_close(&h);
}

static void test_alloc_faults(void)
{
    fault_harness h;
    repl89_text t;
    repl89_event ev;

    rp_check(fault_open(&h) == 0, "open alloc faults");

    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start alloc");
    rp_check(repl89_insert(h.r, "abc", 3) == REPL89_OK, "insert alloc");
    repl89_mem_set_hooks(fail_alloc, fail_grow, fail_release);
    rp_check(repl89_insert(h.r, "0123456789012345678901234567890123456789",
                           40) == REPL89_ENOMEM,
             "insert ENOMEM");
    repl89_mem_reset_hooks();
    t = repl89_text_get(h.r);
    rp_check(t.len == 3 && memcmp(t.data, "abc", 3) == 0,
             "buffer unchanged after ENOMEM");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel alloc");

    repl89_mem_set_hooks(fail_alloc, fail_grow, fail_release);
    rp_check(repl89_new(NULL) == NULL, "new ENOMEM");
    repl89_mem_reset_hooks();

    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start paste fault");
    {
        const char *bad = "\x1b[200~\xC0\x80\x1b[201~";
        ssize_t w;
        size_t n;

        n = strlen(bad);
        w = write(h.master, bad, n);
        rp_check(w == (ssize_t)n, "write bad paste");
        ev = REPL89_EVENT_NONE;
        rp_check(repl89_feed(h.r, &ev) == REPL89_EUTF8, "bad paste EUTF8");
        t = repl89_text_get(h.r);
        rp_check(t.len == 0, "buffer unchanged after bad paste");
        rp_check(repl89_submit(h.r) == REPL89_OK, "session usable after paste");
    }

    fault_close(&h);
}

/* ---- Invariant fuzzing --------------------------------------------------- */

static unsigned long lcg(unsigned long x)
{
    return x * 1103515245UL + 12345UL;
}

static int content_ok(const repl89_edit *e)
{
    size_t pos;
    size_t next;
    u89_cp cp;
    u89_status st;
    int valid;
    int ctl;

    valid = u89_utf8_valid((const unsigned char *)e->buf.data, e->buf.len);
    if (!valid) {
        return 0;
    }
    pos = 0;
    while (pos < e->buf.len) {
        st = u89_utf8_decode((const unsigned char *)e->buf.data, e->buf.len,
                             pos, &cp, &next);
        if (st != U89_OK) {
            return 0;
        }
        ctl = u89_is_control(cp);
        if (ctl && cp != 0x0AUL && cp != 0x09UL) {
            return 0;
        }
        pos = next;
    }
    return 1;
}

static int cursor_ok(const repl89_edit *e)
{
    if (e->cursor > e->buf.len) {
        return 0;
    }
    if (e->cursor == e->buf.len) {
        return 1;
    }
    return u89_grapheme_boundary((const unsigned char *)e->buf.data,
                                 e->buf.len, e->cursor);
}

static void fuzz_step(repl89_edit *e, unsigned long op)
{
    unsigned long kind;

    kind = op % 11;
    if (kind == 0) {
        repl89_edit_insert(e, "a", 1);
    } else if (kind == 1) {
        repl89_edit_insert(e, "\xC3\xA9", 2);
    } else if (kind == 2) {
        repl89_edit_insert(e, "\xF0\x9F\x98\x80", 4);
    } else if (kind == 3) {
        repl89_edit_insert(e, "\n", 1);
    } else if (kind == 4) {
        repl89_edit_delete_prev(e);
    } else if (kind == 5) {
        repl89_edit_delete_next(e);
    } else if (kind == 6) {
        repl89_edit_left(e);
    } else if (kind == 7) {
        repl89_edit_right(e);
    } else if (kind == 8) {
        repl89_edit_home(e);
    } else if (kind == 9) {
        repl89_edit_end(e);
    } else {
        repl89_edit_kill_end(e);
    }
}

static void test_invariants(void)
{
    repl89_edit e;
    unsigned long x;
    unsigned long i;
    int ok;

    repl89_edit_init(&e);
    x = 1;
    ok = 1;
    for (i = 0; i < 5000; i++) {
        x = lcg(x);
        fuzz_step(&e, x >> 16);
        if (!content_ok(&e) || !cursor_ok(&e)) {
            ok = 0;
            break;
        }
    }
    rp_check(ok == 1, "R89-I1/R89-I2/R89-I3 invariants hold");
    repl89_edit_free(&e);
}

static void test_feed_alloc_fault(void)
{
    fault_harness h;
    repl89_text t;
    repl89_event ev;

    rp_check(fault_open(&h) == 0, "open feed fault");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start feed fault");
    repl89_mem_set_hooks(fail_alloc, fail_grow, fail_release);
    rp_check(write(h.master, "x", 1) == 1, "write feed fault");
    ev = REPL89_EVENT_NONE;
    rp_check(repl89_feed(h.r, &ev) == REPL89_ENOMEM, "ST-18 feed ENOMEM");
    repl89_mem_reset_hooks();
    t = repl89_text_get(h.r);
    rp_check(t.len == 0, "ST-18 buffer unchanged");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel feed fault");
    fault_close(&h);
}

static void test_redisplay_faults(void)
{
    fault_harness h;
    repl89_event ev;

    rp_check(fault_open(&h) == 0, "open redisplay faults");
    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start redisplay faults");

    repl89_tty_set_hooks(NULL, NULL, NULL, NULL, fail_write);
    rp_check(write(h.master, "a", 1) == 1, "write redisplay text");
    rp_check(repl89_feed(h.r, &ev) == REPL89_EIO, "FI-11 content write failure");
    repl89_tty_reset_hooks();
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel after content fault");

    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start hide fault");
    repl89_tty_set_hooks(NULL, NULL, NULL, NULL, fail_write);
    rp_check(repl89_hide(h.r) == REPL89_EIO, "FI-12 hide write failure");
    repl89_tty_reset_hooks();
    rp_check(repl89_show(h.r) == REPL89_OK, "show after hide fault");
    rp_check(repl89_cancel(h.r) == REPL89_OK, "cancel after hide fault");

    rp_check(repl89_start(h.r, "> ") == REPL89_OK, "start finalize fault");
    repl89_tty_set_hooks(NULL, NULL, NULL, NULL, fail_write);
    rp_check(repl89_submit(h.r) == REPL89_EIO, "FI-13 submit write failure");
    repl89_tty_reset_hooks();
    rp_check(repl89_submit(h.r) == REPL89_ESTATE, "FI-13 session ended");

    fault_close(&h);
}

void test_fault(void)
{
    test_tty_faults();
    test_alloc_faults();
    test_feed_alloc_fault();
    test_redisplay_faults();
    test_invariants();
}
