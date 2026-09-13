#include <stdio.h>
#include <string.h>

#include "test.h"
#include "repl89_internal.h"

static void expect_key(const char *p, size_t n, repl89_key_type want,
                       size_t want_used, const char *what)
{
    repl89_key k;
    size_t used;
    char ctx[96];

    sprintf(ctx, "%s", what);
    k.type = REPL89_KEY_IGNORE;
    k.len = 0;
    used = repl89_key_next(p, n, &k);
    rp_check_ctx(used == want_used, "consumed", ctx);
    rp_check_ctx(k.type == want, "key type", ctx);
}

static void expect_text(const char *p, size_t n, size_t want_len,
                        const char *what)
{
    repl89_key k;
    size_t used;
    char ctx[96];

    sprintf(ctx, "%s", what);
    k.type = REPL89_KEY_IGNORE;
    k.len = 0;
    used = repl89_key_next(p, n, &k);
    rp_check_ctx(used == want_len, "text consumed", ctx);
    rp_check_ctx(k.type == REPL89_KEY_TEXT, "text type", ctx);
    rp_check_ctx(k.len == want_len, "text len", ctx);
}

static void expect_partial(const char *p, size_t n, const char *what)
{
    repl89_key k;
    size_t used;
    char ctx[96];

    sprintf(ctx, "%s", what);
    k.type = REPL89_KEY_IGNORE;
    k.len = 0;
    used = repl89_key_next(p, n, &k);
    rp_check_ctx(used == 0, "partial returns 0", ctx);
}

static void split_check(const char *s, repl89_key_type want, const char *what)
{
    size_t len;
    size_t k;
    char ctx[96];

    len = strlen(s);
    for (k = 1; k < len; k++) {
        repl89_key key;
        size_t used;

        key.type = REPL89_KEY_IGNORE;
        key.len = 0;
        used = repl89_key_next(s, k, &key);
        if (used == 0) {
            repl89_key key2;
            size_t used2;

            key2.type = REPL89_KEY_IGNORE;
            key2.len = 0;
            used2 = repl89_key_next(s, len, &key2);
            sprintf(ctx, "%s split %u", what, (unsigned)k);
            rp_check_ctx(used2 == len && key2.type == want, "split decode",
                         ctx);
        }
    }
}

static size_t decode_all(const char *s, size_t n, repl89_key *out, size_t max)
{
    size_t pos;
    size_t no;

    pos = 0;
    no = 0;
    while (pos < n && no < max) {
        repl89_key k;
        size_t used;

        used = repl89_key_next(s + pos, n - pos, &k);
        if (used == 0) {
            break;
        }
        out[no] = k;
        no++;
        pos += used;
    }
    return no;
}

static size_t decode_chunked(const char *s, size_t n, size_t chunk,
                             repl89_key *out, size_t max)
{
    char pend[128];
    size_t plen;
    size_t pos;
    size_t no;

    plen = 0;
    pos = 0;
    no = 0;
    while (pos < n && plen < sizeof pend) {
        size_t take;
        size_t used;
        repl89_key k;

        take = chunk;
        if (take > n - pos) {
            take = n - pos;
        }
        memcpy(pend + plen, s + pos, take);
        plen += take;
        pos += take;
        for (;;) {
            used = repl89_key_next(pend, plen, &k);
            if (used == 0) {
                break;
            }
            if (no < max) {
                out[no] = k;
            }
            no++;
            memmove(pend, pend + used, plen - used);
            plen -= used;
        }
    }
    return no;
}

static void test_long_csi(void)
{
    char seq[64];
    repl89_key whole[8];
    repl89_key part[8];
    size_t n;
    size_t nw;
    size_t nc;
    size_t i;

    n = 0;
    seq[n] = 0x1B;
    n++;
    seq[n] = '[';
    n++;
    for (i = 0; i < 30; i++) {
        seq[n] = '0';
        n++;
    }
    seq[n] = 'A';
    n++;

    nw = decode_all(seq, n, whole, 8);
    nc = decode_chunked(seq, n, 1, part, 8);
    rp_check(nw == nc, "long csi fragment count");
    for (i = 0; i < nw && i < nc; i++) {
        rp_check(whole[i].type == part[i].type
                     && whole[i].len == part[i].len,
                 "long csi fragment events");
    }
}

void test_key(void)
{
    static const char *arrows[4] = { "\x1b[A", "\x1b[B", "\x1b[C", "\x1b[D" };
    static const repl89_key_type arrow_types[4] = {
        REPL89_KEY_UP, REPL89_KEY_DOWN, REPL89_KEY_RIGHT, REPL89_KEY_LEFT
    };
    int i;

    expect_text("a", 1, 1, "ascii");
    expect_text("\xC3\xA9", 2, 2, "2-byte");
    expect_text("\xE4\xB8\xAD", 3, 3, "3-byte");
    expect_text("\xF0\x9F\x98\x80", 4, 4, "4-byte");
    expect_text("\t", 1, 1, "tab text");
    split_check("\xC3\xA9", REPL89_KEY_TEXT, "2-byte text");
    split_check("\xE4\xB8\xAD", REPL89_KEY_TEXT, "3-byte text");
    split_check("\xF0\x9F\x98\x80", REPL89_KEY_TEXT, "4-byte text");

    for (i = 0; i < 4; i++) {
        expect_key(arrows[i], 3, arrow_types[i], 3, "csi arrow");
        split_check(arrows[i], arrow_types[i], "csi arrow");
    }
    expect_key("\x1bOA", 3, REPL89_KEY_UP, 3, "ss3 up");
    expect_key("\x1bOD", 3, REPL89_KEY_LEFT, 3, "ss3 left");
    split_check("\x1bOA", REPL89_KEY_UP, "ss3 up");

    expect_key("\x1b[H", 3, REPL89_KEY_HOME, 3, "home csi");
    expect_key("\x1b[1~", 4, REPL89_KEY_HOME, 4, "home tilde");
    expect_key("\x1bOH", 3, REPL89_KEY_HOME, 3, "home ss3");
    expect_key("\x1b[F", 3, REPL89_KEY_END, 3, "end csi");
    expect_key("\x1b[4~", 4, REPL89_KEY_END, 4, "end tilde");
    expect_key("\x1bOF", 3, REPL89_KEY_END, 3, "end ss3");
    expect_key("\x1b[3~", 4, REPL89_KEY_DELETE, 4, "delete");
    split_check("\x1b[3~", REPL89_KEY_DELETE, "delete");

    expect_key("\x7F", 1, REPL89_KEY_BACKSPACE, 1, "del backspace");
    expect_key("\x08", 1, REPL89_KEY_BACKSPACE, 1, "ctrl-h backspace");
    expect_key("\r", 1, REPL89_KEY_SUBMIT, 1, "enter");
    expect_key("\n", 1, REPL89_KEY_LF, 1, "ctrl-j");
    expect_key("\x1b\r", 2, REPL89_KEY_LF, 2, "alt-enter");
    expect_key("\x03", 1, REPL89_KEY_CANCEL, 1, "ctrl-c");
    expect_key("\x04", 1, REPL89_KEY_CTRL_D, 1, "ctrl-d");
    expect_key("\x01", 1, REPL89_KEY_HOME, 1, "ctrl-a");
    expect_key("\x05", 1, REPL89_KEY_END, 1, "ctrl-e");
    expect_key("\x15", 1, REPL89_KEY_CTRL_U, 1, "ctrl-u");
    expect_key("\x0B", 1, REPL89_KEY_CTRL_K, 1, "ctrl-k");
    expect_key("\x10", 1, REPL89_KEY_CTRL_P, 1, "ctrl-p");
    expect_key("\x0E", 1, REPL89_KEY_CTRL_N, 1, "ctrl-n");

    expect_key("\x1b[200~", 6, REPL89_KEY_PASTE_BEGIN, 6, "paste begin");
    expect_key("\x1b[201~", 6, REPL89_KEY_PASTE_END, 6, "paste end");
    split_check("\x1b[200~", REPL89_KEY_PASTE_BEGIN, "paste begin");
    split_check("\x1b[201~", REPL89_KEY_PASTE_END, "paste end");

    expect_key("\x1b[99Z", 5, REPL89_KEY_IGNORE, 5, "unknown csi");
    expect_key("\x1b[?25h", 6, REPL89_KEY_IGNORE, 6, "private csi");
    expect_key("\x1bq", 2, REPL89_KEY_IGNORE, 2, "unknown esc");

    expect_partial("\x1b", 1, "esc alone");
    expect_partial("\x1b[", 2, "csi open");
    expect_partial("\x1b[3", 3, "csi digit");
    expect_partial("\x1b[3;", 4, "csi semi");
    expect_partial("\x1bO", 2, "ss3 open");

    {
        const char *s = "ab\x1b[A";
        repl89_key k;
        size_t used;
        size_t pos;

        pos = 0;
        used = repl89_key_next(s + pos, 5 - pos, &k);
        rp_check(used == 1 && k.type == REPL89_KEY_TEXT, "multi first");
        pos += used;
        used = repl89_key_next(s + pos, 5 - pos, &k);
        rp_check(used == 1 && k.type == REPL89_KEY_TEXT, "multi second");
        pos += used;
        used = repl89_key_next(s + pos, 5 - pos, &k);
        rp_check(used == 3 && k.type == REPL89_KEY_UP, "multi third");
        pos += used;
        rp_check(pos == 5, "multi consumed all");
    }

    test_long_csi();
}
