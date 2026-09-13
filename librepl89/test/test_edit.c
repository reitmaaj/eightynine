#include <stdio.h>
#include <string.h>

#include "test.h"
#include "repl89_internal.h"

#define E_ACUTE "e\xCC\x81"
#define FAMILY \
    "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"
#define FAMILY_LEN 18
#define FLAG "\xF0\x9F\x87\xAB\xF0\x9F\x87\xAE"
#define FLAG_LEN 8
#define CJK "\xE4\xB8\xAD"
#define CJK_LEN 3

static void edit_set(repl89_edit *e, const char *text, size_t cursor)
{
    repl89_error err;

    repl89_edit_reset(e);
    err = repl89_edit_insert(e, text, strlen(text));
    rp_check(err == REPL89_OK, "setup insert");
    e->cursor = cursor;
}

static void expect_edit(const repl89_edit *e, const char *text, size_t cursor,
                        const char *what)
{
    char ctx[96];
    size_t n;
    int ok;

    sprintf(ctx, "%s", what);
    n = strlen(text);
    ok = 1;
    if (e->buf.len != n) {
        ok = 0;
    }
    if (ok && n > 0) {
        if (memcmp(e->buf.data, text, n) != 0) {
            ok = 0;
        }
    }
    rp_check_ctx(ok == 1, "buffer matches", ctx);
    rp_check_ctx(e->cursor == cursor, "cursor matches", ctx);
}

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

static void test_insert(void)
{
    repl89_edit e;
    repl89_error err;

    repl89_edit_init(&e);
    rp_check(repl89_edit_insert(&e, "abc", 3) == REPL89_OK, "IN-01 ascii");
    expect_edit(&e, "abc", 3, "IN-01");

    rp_check(repl89_edit_insert(&e, "\xC3\xA9", 2) == REPL89_OK, "IN-02 utf8");
    expect_edit(&e, "abc\xC3\xA9", 5, "IN-02");

    rp_check(repl89_edit_insert(&e, "\n", 1) == REPL89_OK, "IN-03 LF");
    rp_check(repl89_edit_insert(&e, "\t", 1) == REPL89_OK, "IN-04 HT");

    err = repl89_edit_insert(&e, "\r", 1);
    rp_check(err == REPL89_EINVAL, "IN-05 CR rejected");
    err = repl89_edit_insert(&e, "\x01", 1);
    rp_check(err == REPL89_EINVAL, "IN-06 C0 rejected");
    err = repl89_edit_insert(&e, "\x7F", 1);
    rp_check(err == REPL89_EINVAL, "IN-07 DEL rejected");
    err = repl89_edit_insert(&e, "\xC2\x85", 2);
    rp_check(err == REPL89_EINVAL, "IN-08 C1 rejected");
    err = repl89_edit_insert(&e, "\xC0\x80", 2);
    rp_check(err == REPL89_EUTF8, "IN-09 malformed rejected");

    repl89_edit_reset(&e);
    rp_check(repl89_edit_insert(&e, "ac", 2) == REPL89_OK, "IN-11 preload");
    e.cursor = 1;
    rp_check(repl89_edit_insert(&e, "b", 1) == REPL89_OK, "IN-11 mid insert");
    expect_edit(&e, "abc", 2, "IN-11");

    repl89_edit_reset(&e);
    rp_check(repl89_edit_insert(&e, "abc", 3) == REPL89_OK, "IN-10 preload");
    repl89_mem_set_hooks(fail_alloc, fail_grow, fail_release);
    err = repl89_edit_insert(&e, "0123456789012345678901234567890123456789",
                             40);
    rp_check(err == REPL89_ENOMEM, "IN-10 ENOMEM");
    repl89_mem_reset_hooks();
    expect_edit(&e, "abc", 3, "IN-10 unchanged");
    repl89_edit_free(&e);
}

static void test_grapheme_edits(void)
{
    repl89_edit e;

    repl89_edit_init(&e);

    edit_set(&e, "abc", 3);
    repl89_edit_delete_prev(&e);
    expect_edit(&e, "ab", 2, "ED-01");

    edit_set(&e, "abc", 1);
    repl89_edit_delete_next(&e);
    expect_edit(&e, "ac", 1, "ED-02");

    edit_set(&e, E_ACUTE, 3);
    repl89_edit_delete_prev(&e);
    expect_edit(&e, "", 0, "ED-03");

    edit_set(&e, E_ACUTE, 0);
    repl89_edit_delete_next(&e);
    expect_edit(&e, "", 0, "ED-04");

    edit_set(&e, FAMILY "x", FAMILY_LEN + 1);
    repl89_edit_left(&e);
    expect_edit(&e, FAMILY "x", FAMILY_LEN, "ED-05a");
    repl89_edit_left(&e);
    expect_edit(&e, FAMILY "x", 0, "ED-05b");
    repl89_edit_right(&e);
    expect_edit(&e, FAMILY "x", FAMILY_LEN, "ED-06a");
    repl89_edit_right(&e);
    expect_edit(&e, FAMILY "x", FAMILY_LEN + 1, "ED-06b");
    edit_set(&e, FAMILY "x", FAMILY_LEN);
    repl89_edit_delete_prev(&e);
    expect_edit(&e, "x", 0, "ED-05c");

    edit_set(&e, FLAG, FLAG_LEN);
    repl89_edit_delete_prev(&e);
    expect_edit(&e, "", 0, "ED-07");

    edit_set(&e, CJK "x", CJK_LEN + 1);
    repl89_edit_left(&e);
    expect_edit(&e, CJK "x", CJK_LEN, "ED-08a");
    repl89_edit_left(&e);
    expect_edit(&e, CJK "x", 0, "ED-08b");
    repl89_edit_right(&e);
    expect_edit(&e, CJK "x", CJK_LEN, "ED-08c");

    edit_set(&e, "abc", 0);
    repl89_edit_delete_prev(&e);
    repl89_edit_left(&e);
    expect_edit(&e, "abc", 0, "ED-09");

    edit_set(&e, "abc", 3);
    repl89_edit_delete_next(&e);
    repl89_edit_right(&e);
    expect_edit(&e, "abc", 3, "ED-10");

    repl89_edit_free(&e);
}

static void test_multiline(void)
{
    repl89_edit e;

    repl89_edit_init(&e);

    edit_set(&e, "ab", 1);
    repl89_edit_insert(&e, "\n", 1);
    expect_edit(&e, "a\nb", 2, "ML-01");

    edit_set(&e, "a\n", 2);
    repl89_edit_delete_prev(&e);
    expect_edit(&e, "a", 1, "ML-02");

    edit_set(&e, "a\nb", 1);
    repl89_edit_delete_next(&e);
    expect_edit(&e, "ab", 1, "ML-03");

    edit_set(&e, "ab\ncd", 5);
    repl89_edit_home(&e);
    expect_edit(&e, "ab\ncd", 3, "ML-04");

    edit_set(&e, "ab\ncd", 0);
    repl89_edit_end(&e);
    expect_edit(&e, "ab\ncd", 2, "ML-05");

    edit_set(&e, "a\n\nb", 4);
    expect_edit(&e, "a\n\nb", 4, "ML-06");

    edit_set(&e, "\nx", 2);
    repl89_edit_home(&e);
    expect_edit(&e, "\nx", 1, "ML-07");
    repl89_edit_left(&e);
    expect_edit(&e, "\nx", 0, "ML-07b");

    edit_set(&e, "x\n", 0);
    repl89_edit_end(&e);
    expect_edit(&e, "x\n", 1, "ML-08");

    edit_set(&e, "abc\ndef", 6);
    repl89_edit_kill_begin(&e);
    expect_edit(&e, "abc\nf", 4, "CTRL-U");

    edit_set(&e, "abc\ndef", 4);
    repl89_edit_kill_end(&e);
    expect_edit(&e, "abc\n", 4, "CTRL-K");

    repl89_edit_free(&e);
}

void test_edit(void)
{
    test_insert();
    test_grapheme_edits();
    test_multiline();
}
