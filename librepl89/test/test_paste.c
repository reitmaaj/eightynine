#include <stdio.h>
#include <string.h>

#include "test.h"
#include "repl89_internal.h"

static void paste_all(repl89_paste *p, const char *s, size_t n,
                      const char *want, const char *what)
{
    repl89_text t;
    repl89_error err;
    size_t wl;
    char ctx[96];

    sprintf(ctx, "%s", what);
    repl89_paste_reset(p);
    err = repl89_paste_feed(p, s, n);
    if (err == REPL89_OK) {
        err = repl89_paste_finish(p, &t);
    }
    rp_check_ctx(err == REPL89_OK, "paste ok", ctx);
    if (err == REPL89_OK) {
        wl = strlen(want);
        rp_check_ctx(t.len == wl && memcmp(t.data, want, wl) == 0,
                     "paste content", ctx);
    }
}

static void paste_bad(repl89_paste *p, const char *s, size_t n,
                      const char *what)
{
    repl89_text t;
    repl89_error err;
    char ctx[96];

    sprintf(ctx, "%s", what);
    repl89_paste_reset(p);
    err = repl89_paste_feed(p, s, n);
    if (err == REPL89_OK) {
        err = repl89_paste_finish(p, &t);
    }
    rp_check_ctx(err == REPL89_EUTF8, "paste rejected", ctx);
}

static void paste_split_check(repl89_paste *p, const char *s, size_t n,
                              const char *want, const char *what)
{
    size_t k;

    for (k = 0; k <= n; k++) {
        repl89_text t;
        repl89_error err;
        size_t wl;
        char ctx[96];

        repl89_paste_reset(p);
        err = repl89_paste_feed(p, s, k);
        if (err == REPL89_OK) {
            err = repl89_paste_feed(p, s + k, n - k);
        }
        if (err == REPL89_OK) {
            err = repl89_paste_finish(p, &t);
        }
        sprintf(ctx, "%s split %u", what, (unsigned)k);
        rp_check_ctx(err == REPL89_OK, "split ok", ctx);
        if (err == REPL89_OK) {
            wl = strlen(want);
            rp_check_ctx(t.len == wl && memcmp(t.data, want, wl) == 0,
                         "split content", ctx);
        }
    }
}

void test_paste(void)
{
    repl89_paste p;
    char big[4096];
    char expect[4097];
    size_t i;

    repl89_paste_init(&p);

    paste_all(&p, "abc", 3, "abc", "PS-01");
    paste_all(&p, "a\nb", 3, "a\nb", "PS-02");
    paste_all(&p, "a\r\nb", 4, "a\nb", "PS-03");
    paste_all(&p, "a\rb", 3, "a\nb", "PS-04");
    paste_all(&p, "a\r\nb\rc\nd", 9, "a\nb\nc\nd", "PS-05");
    paste_all(&p, "a\tb", 3, "a\tb", "PS-06");
    paste_all(&p, "\x01" "a\x02", 3, "a", "PS-07");
    paste_all(&p, "a\x7F" "b", 3, "ab", "PS-08");
    paste_all(&p, "a\xC2\x85" "b", 4, "ab", "PS-09");
    paste_all(&p, "a\x1b" "b", 3, "ab", "PS-10");
    paste_all(&p, "\x1b[31m", 5, "[31m", "PS-10b");
    paste_all(&p, "\xF0\x9F\x98\x80", 4, "\xF0\x9F\x98\x80", "PS-11");
    paste_all(&p, "e\xCC\x81", 3, "e\xCC\x81", "PS-11b");

    paste_bad(&p, "\xC0\x80", 2, "PS-12 overlong");
    paste_bad(&p, "\xE2\x82", 2, "PS-12 truncated");
    paste_bad(&p, "\xED\xA0\x80", 3, "PS-12 surrogate");
    paste_bad(&p, "a\xFF", 2, "PS-12 invalid lead");

    paste_split_check(&p, "a\r\nb", 4, "a\nb", "PS-13 crlf");
    paste_split_check(&p, "a\rb", 3, "a\nb", "PS-13 cr");
    paste_split_check(&p, "a\tb", 3, "a\tb", "PS-13 tab");
    paste_split_check(&p, "e\xCC\x81", 3, "e\xCC\x81", "PS-13 mark");
    paste_split_check(&p, "\xF0\x9F\x98\x80", 4, "\xF0\x9F\x98\x80",
                      "PS-13 emoji");
    paste_split_check(&p, "a\x1b[31mb", 7, "a[31mb", "PS-13 escape");

    /* PS-16: pasted LF never submits; the accumulator has no event. */
    paste_all(&p, "one\ntwo\n", 8, "one\ntwo\n", "PS-16");

    memset(big, 'x', sizeof big);
    memset(expect, 'x', sizeof big);
    expect[sizeof big] = 0;
    paste_all(&p, big, sizeof big, expect, "PS-17 long");
    for (i = 0; i < sizeof big; ++i) {
        big[i] = 'y';
    }
    memset(expect, 'y', sizeof big);
    expect[sizeof big] = 0;
    paste_all(&p, big, sizeof big, expect, "PS-17 long again");

    repl89_paste_free(&p);
}
