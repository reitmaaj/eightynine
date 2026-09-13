#include <stdio.h>
#include <string.h>

#include "test.h"
#include "repl89_internal.h"
#include "u89.h"
#include "vt.h"

typedef struct capture {
    char buf[8192];
    size_t len;
} capture;

static void set_text(repl89_edit *e, const char *text, size_t cursor);

static int cap_emit(void *ctx, const char *p, size_t n)
{
    capture *c;

    c = (capture *)ctx;
    if (c->len + n > sizeof c->buf) {
        return -1;
    }
    memcpy(c->buf + c->len, p, n);
    c->len = c->len + n;
    return 0;
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

static int find_at(const char *hay, size_t n, const char *needle)
{
    size_t m;
    size_t i;

    m = strlen(needle);
    if (m > n) {
        return -1;
    }
    for (i = 0; i + m <= n; ++i) {
        if (memcmp(hay + i, needle, m) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void repaint_matches_draw(const char *old_text, size_t old_cursor,
                                 const char *new_text, size_t new_cursor,
                                 unsigned int cols, const char *what)
{
    repl89_edit e;
    capture cap;
    repl89_sink sink;
    repl89_region r1;
    repl89_region r2;
    vt_screen got;
    vt_screen want;
    char a[256];
    char b[256];
    char ctx[96];
    unsigned int i;

    sprintf(ctx, "%s", what);
    repl89_edit_init(&e);
    cap.len = 0;
    sink.ctx = &cap;
    sink.emit = cap_emit;
    set_text(&e, old_text, old_cursor);
    vt_init(&got);
    rp_check_ctx(repl89_render_draw(&e, "> ", "... ", cols, 8, &sink, &r1)
                     == REPL89_OK,
                 "rp old draw", ctx);
    vt_feed(&got, cap.buf, cap.len);

    cap.len = 0;
    set_text(&e, new_text, new_cursor);
    rp_check_ctx(repl89_render_content(&e, "> ", "... ", cols, 8, &sink, &r1,
                                       &r2) == REPL89_OK,
                 "rp repaint", ctx);
    vt_feed(&got, cap.buf, cap.len);

    cap.len = 0;
    vt_init(&want);
    rp_check_ctx(repl89_render_draw(&e, "> ", "... ", cols, 8, &sink, &r2)
                     == REPL89_OK,
                 "rp fresh draw", ctx);
    vt_feed(&want, cap.buf, cap.len);

    for (i = 0; i < 6; i++) {
        vt_row_text(&got, i, a, sizeof a);
        vt_row_text(&want, i, b, sizeof b);
        rp_check_ctx(strcmp(a, b) == 0, "rp rows match", ctx);
    }
    rp_check_ctx(got.row == want.row, "rp cursor row", ctx);
    rp_check_ctx(got.col == want.col, "rp cursor col", ctx);
    repl89_edit_free(&e);
}

static void test_repaint(void)
{
    repl89_edit e;
    capture cap;
    repl89_sink sink;
    repl89_region r1;
    repl89_region r2;
    repl89_region r3;
    vt_screen scr;
    char row0[256];
    char row1[256];
    int at;

    repl89_edit_init(&e);
    cap.len = 0;
    sink.ctx = &cap;
    sink.emit = cap_emit;

    /* VT-23: shrink from three rows to one clears the stale rows */
    set_text(&e, "abcd\nef", 7);
    vt_init(&scr);
    rp_check(repl89_render_draw(&e, "> ", "", 5, 8, &sink, &r1) == REPL89_OK,
             "RP-01 draw");
    vt_feed(&scr, cap.buf, cap.len);
    rp_check(r1.rows == 3, "RP-01 old rows");

    cap.len = 0;
    set_text(&e, "xy", 2);
    rp_check(repl89_render_content(&e, "> ", "", 5, 8, &sink, &r1, &r2)
                 == REPL89_OK,
             "RP-01 repaint");
    vt_feed(&scr, cap.buf, cap.len);
    vt_row_text(&scr, 0, row0, sizeof row0);
    vt_row_text(&scr, 1, row1, sizeof row1);
    rp_check(strcmp(row0, "> xy") == 0, "RP-01 row0");
    rp_check(strcmp(row1, "") == 0, "RP-01 stale cleared");
    rp_check(scr.row == 0 && scr.col == 4, "RP-01 cursor");
    rp_check(contains(cap.buf, cap.len, "\x1b[2K") == 1, "RP-01 stale erase");
    rp_check(contains(cap.buf, cap.len, "\x1b[K") == 1, "RP-01 suffix erase");

    /* VT-22: a same-height repaint writes before it erases */
    cap.len = 0;
    set_text(&e, "abc", 3);
    rp_check(repl89_render_content(&e, "> ", "", 5, 8, &sink, &r2, &r3)
                 == REPL89_OK,
             "RP-02 repaint");
    at = find_at(cap.buf, cap.len, "\x1b[K");
    rp_check(at > 0, "RP-02 suffix erase present");
    rp_check(find_at(cap.buf, cap.len, "\x1b[2K") == -1, "RP-02 no full clear");
    rp_check(find_at(cap.buf, cap.len, "abc") < at, "RP-02 write before erase");

    repaint_matches_draw("abc", 3, "abd", 3, 20, "RP-03 same row");
    repaint_matches_draw("abcd\nef", 6, "x", 1, 5, "RP-04 shrink");
    repaint_matches_draw("x", 1, "abcdefghij", 10, 5, "RP-05 grow");
    repaint_matches_draw("abcdefgh", 8, "ab", 2, 5, "RP-06 rewrap");
    repaint_matches_draw("ab\ncd", 5, "ab\ncd", 2, 20, "RP-07 cursor back");
    repaint_matches_draw("a\nb\nc", 5, "a\nb\nc\nd", 7, 20, "RP-08 add row");

    repl89_edit_free(&e);
}

static void test_cursor_render(void)
{
    capture cap;
    repl89_sink sink;
    repl89_region oldr;
    repl89_region newr;

    cap.len = 0;
    sink.ctx = &cap;
    sink.emit = cap_emit;

    oldr.cols = 10;
    oldr.rows = 3;
    oldr.cursor_row = 2;
    oldr.cursor_col = 5;
    oldr.end_row = 2;
    oldr.end_col = 5;

    /* VT-20: up and back */
    newr = oldr;
    newr.cursor_row = 1;
    newr.cursor_col = 2;
    rp_check(repl89_render_cursor(&sink, &oldr, &newr) == REPL89_OK, "VT-20 ok");
    rp_check(cap.len > 0, "VT-20 emitted");
    rp_check(rp_only_motion(cap.buf, cap.len) == 1, "VT-20 motion only");
    rp_check(contains(cap.buf, cap.len, "\x1b[1A") == 1, "VT-20 up");
    rp_check(contains(cap.buf, cap.len, "\x1b[2C") == 1, "VT-20 fwd");

    /* VT-20: down and forward */
    cap.len = 0;
    newr = oldr;
    newr.cursor_row = 4;
    newr.cursor_col = 7;
    rp_check(repl89_render_cursor(&sink, &oldr, &newr) == REPL89_OK, "VT-20b ok");
    rp_check(rp_only_motion(cap.buf, cap.len) == 1, "VT-20b motion only");
    rp_check(contains(cap.buf, cap.len, "\x1b[2B") == 1, "VT-20b down");
    rp_check(contains(cap.buf, cap.len, "\x1b[7C") == 1, "VT-20b fwd");

    /* VT-20: same row to column 0 is a bare CR */
    cap.len = 0;
    newr = oldr;
    newr.cursor_col = 0;
    rp_check(repl89_render_cursor(&sink, &oldr, &newr) == REPL89_OK, "VT-20c ok");
    rp_check(cap.len == 1 && cap.buf[0] == '\r', "VT-20c CR only");

    /* VT-21: identical positions emit nothing */
    cap.len = 0;
    rp_check(repl89_render_cursor(&sink, &oldr, &oldr) == REPL89_OK, "VT-21 ok");
    rp_check(cap.len == 0, "VT-21 nothing");
}

static void render(const repl89_edit *e, const char *prompt, const char *cont,
                   unsigned int cols, unsigned int tab, vt_screen *scr,
                   repl89_region *reg)
{
    capture cap;
    repl89_sink sink;
    repl89_error err;

    cap.len = 0;
    sink.ctx = &cap;
    sink.emit = cap_emit;
    vt_init(scr);
    err = repl89_render_draw(e, prompt, cont, cols, tab, &sink, reg);
    rp_check(err == REPL89_OK, "render returns OK");
    vt_feed(scr, cap.buf, cap.len);
}

static void expect_row(const vt_screen *s, unsigned int row, const char *text,
                       const char *what)
{
    char got[512];
    char ctx[96];

    sprintf(ctx, "%s", what);
    vt_row_text(s, row, got, sizeof got);
    rp_check_ctx(strcmp(got, text) == 0, "row text", ctx);
}

static void expect_cursor(const vt_screen *s, unsigned int row,
                          unsigned int col, const char *what)
{
    char ctx[96];

    sprintf(ctx, "%s", what);
    rp_check_ctx(s->row == row, "cursor row", ctx);
    rp_check_ctx(s->col == col, "cursor col", ctx);
}

static void set_text(repl89_edit *e, const char *text, size_t cursor)
{
    repl89_edit_reset(e);
    rp_check(repl89_edit_insert(e, text, strlen(text)) == REPL89_OK, "setup");
    e->cursor = cursor;
}

static void layout_case(const repl89_edit *e, const char *prompt,
                        const char *cont, unsigned int cols,
                        unsigned int tab, int dir, unsigned int want,
                        int want_in_range, size_t want_target,
                        unsigned int want_col, const char *what)
{
    repl89_vmove vm;
    char ctx[96];

    sprintf(ctx, "%s", what);
    repl89_layout_vertical(e, prompt, cont, cols, tab, dir, want, &vm);
    rp_check_ctx(vm.in_range == want_in_range, "vm in range", ctx);
    if (want_in_range) {
        rp_check_ctx(vm.target == want_target, "vm target", ctx);
        rp_check_ctx(vm.target_col == want_col, "vm target col", ctx);
        rp_check_ctx(u89_grapheme_boundary(
                         (const unsigned char *)e->buf.data, e->buf.len,
                         vm.target) == 1,
                     "vm target boundary", ctx);
    }
}

static void test_vertical_layout(void)
{
    repl89_edit e;

    repl89_edit_init(&e);

    /* VM-01/VM-06: soft wrapping */
    set_text(&e, "abcdefghijk", 7);
    layout_case(&e, "", "", 5, 8, -1, 2, 1, 2, 2, "VM-01 up");
    layout_case(&e, "", "", 5, 8, 1, 2, 1, 11, 1, "VM-01 down");
    layout_case(&e, "", "", 5, 8, -1, REPL89_WANT_CURSOR, 1, 2, 2,
                "VM-01 up cursor col");
    set_text(&e, "abc", 3);
    layout_case(&e, "", "", 5, 8, -1, 0, 0, 0, 0, "VM-06 up oob");
    layout_case(&e, "", "", 5, 8, 1, 0, 0, 0, 0, "VM-06 down oob");

    /* VM-02: logical newlines */
    set_text(&e, "abc\ndef", 7);
    layout_case(&e, "", "", 20, 8, -1, 3, 1, 3, 3, "VM-02 up line");

    /* VM-03: short intervening line keeps the preferred column */
    set_text(&e, "abcdef\nx\nabcdef", 15);
    layout_case(&e, "", "", 20, 8, -1, 6, 1, 8, 1, "VM-03 clamp");
    layout_case(&e, "", "", 20, 8, -1, REPL89_WANT_CURSOR, 1, 8, 1,
                "VM-03 clamp cursor col");

    /* VM-04: wide clusters */
    set_text(&e, "\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD", 9);
    layout_case(&e, "", "", 4, 8, -1, 2, 1, 3, 2, "VM-04 wide");

    /* VM-05: tab stops */
    set_text(&e, "a\nb\tc", 5);
    layout_case(&e, "", "", 20, 4, -1, 5, 1, 1, 1, "VM-05 tab up");
    set_text(&e, "a\nb\tc", 1);
    layout_case(&e, "", "", 20, 4, 1, 5, 1, 5, 5, "VM-05 tab down");

    /* VM-07: empty lines and trailing LF */
    set_text(&e, "a\n\nb", 4);
    layout_case(&e, "", "", 20, 8, -1, 1, 1, 2, 0, "VM-07 empty line");
    set_text(&e, "a\n", 2);
    layout_case(&e, "> ", "... ", 20, 8, -1, 2, 1, 0, 2,
                "VM-07 trailing lf");

    repl89_edit_free(&e);
}

static void test_layout_edges(void)
{
    repl89_edit e;
    repl89_region reg;

    repl89_edit_init(&e);

    /* VT-24: exact right edge leaves the logical column at cols */
    set_text(&e, "abcd", 4);
    repl89_layout(&e, "", "", 4, 8, &reg);
    rp_check(reg.cols == 4, "LE-01 cols");
    rp_check(reg.rows == 1, "LE-01 rows");
    rp_check(reg.cursor_row == 0, "LE-01 cursor row");
    rp_check(reg.cursor_col == 4, "LE-01 cursor col");
    rp_check(reg.end_col == 4, "LE-01 end col");

    /* VT-25: cursor before the wrapping atom lands on the next row */
    set_text(&e, "abcde", 4);
    repl89_layout(&e, "", "", 4, 8, &reg);
    rp_check(reg.rows == 2, "LE-02 rows");
    rp_check(reg.cursor_row == 1, "LE-02 cursor row");
    rp_check(reg.cursor_col == 0, "LE-02 cursor col");

    /* cursor after the wrapped atom */
    set_text(&e, "abcde", 5);
    repl89_layout(&e, "", "", 4, 8, &reg);
    rp_check(reg.cursor_row == 1, "LE-03 cursor row");
    rp_check(reg.cursor_col == 1, "LE-03 cursor col");

    /* VT-25: cursor before LF stays at the end of the preceding row */
    set_text(&e, "abc\ndef", 3);
    repl89_layout(&e, "", "", 3, 8, &reg);
    rp_check(reg.cursor_row == 0, "LE-04 cursor row");
    rp_check(reg.cursor_col == 3, "LE-04 cursor col");

    /* cursor after LF belongs to the next row */
    set_text(&e, "abc\ndef", 4);
    repl89_layout(&e, "", "", 3, 8, &reg);
    rp_check(reg.cursor_row == 1, "LE-05 cursor row");
    rp_check(reg.cursor_col == 0, "LE-05 cursor col");

    /* wide grapheme wraps whole; cursor before it follows the wrap */
    set_text(&e, "abc\xE4\xB8\xAD", 3);
    repl89_layout(&e, "", "", 4, 8, &reg);
    rp_check(reg.rows == 2, "LE-06 rows");
    rp_check(reg.cursor_row == 1, "LE-06 cursor row");
    rp_check(reg.cursor_col == 0, "LE-06 cursor col");

    /* wide grapheme filling the edge exactly stays on the row */
    set_text(&e, "ab\xE4\xB8\xAD", 5);
    repl89_layout(&e, "", "", 4, 8, &reg);
    rp_check(reg.rows == 1, "LE-07 rows");
    rp_check(reg.cursor_col == 4, "LE-07 cursor col");

    repl89_edit_free(&e);
}

static void test_physical_edge(void)
{
    repl89_edit e;
    vt_screen scr;
    repl89_region reg;

    repl89_edit_init(&e);

    /* VT-26: logical column 4 at width 4 emits motion to physical cell 3 */
    set_text(&e, "abcd", 4);
    render(&e, "", "", 4, 8, &scr, &reg);
    expect_row(&scr, 0, "abcd", "VT-26 row");
    rp_check(reg.end_col == 4, "VT-26 logical end");
    expect_cursor(&scr, 0, 3, "VT-26 physical cursor");

    repl89_edit_free(&e);
}

static void reflow_case(const char *text, size_t cursor, unsigned int old_cols,
                        unsigned int new_cols, unsigned int tab,
                        unsigned int want_rows, unsigned int want_cursor_row,
                        unsigned int want_cursor_col, const char *what)
{
    repl89_edit e;
    repl89_reflow rf;
    char ctx[96];

    sprintf(ctx, "%s", what);
    repl89_edit_init(&e);
    set_text(&e, text, cursor);
    repl89_layout_reflow(&e, "", "", old_cols, new_cols, tab, &rf);
    rp_check_ctx(rf.rows == want_rows, "RF rows", ctx);
    rp_check_ctx(rf.cursor_row == want_cursor_row, "RF cursor row", ctx);
    rp_check_ctx(rf.cursor_col == want_cursor_col, "RF cursor col", ctx);
    rp_check_ctx(rf.rows >= 1, "RF has a row", ctx);
    rp_check_ctx(rf.cursor_row < rf.rows, "RF cursor in range", ctx);
    rp_check_ctx(rf.cursor_col <= new_cols, "RF col in range", ctx);
    repl89_edit_free(&e);
}

static void test_reflow(void)
{
    reflow_case("abc", 3, 3, 3, 8, 1, 0, 3, "RF-01");
    reflow_case("abc", 3, 3, 6, 8, 1, 0, 3, "RF-02");
    reflow_case("abc", 3, 3, 2, 8, 2, 1, 1, "RF-03");
    reflow_case("abcd", 3, 4, 4, 8, 1, 0, 3, "RF-04");
    reflow_case("abcde", 4, 5, 4, 8, 2, 1, 0, "RF-05");
    reflow_case("abcd", 4, 4, 4, 8, 1, 0, 4, "RF-06");
    reflow_case("abcd\nefgh", 5, 4, 4, 8, 2, 1, 0, "RF-07");
    reflow_case("abc\xE4\xB8\xAD", 6, 5, 4, 8, 2, 1, 2, "RF-08");
    reflow_case("ab\xE4\xB8\xAD", 2, 4, 3, 8, 2, 1, 0, "RF-09");
    reflow_case("ab\xE4\xB8\xAD", 5, 4, 4, 8, 1, 0, 4, "RF-10");
    reflow_case("a\t", 2, 4, 4, 4, 1, 0, 4, "RF-11");
    reflow_case("ab\t", 2, 4, 3, 4, 2, 0, 2, "RF-12");
    reflow_case("abc\ndef", 3, 3, 3, 8, 2, 0, 3, "RF-13");
    reflow_case("abc\ndef", 4, 3, 3, 8, 2, 1, 0, "RF-14");
    reflow_case("abcd\nefgh", 9, 4, 2, 8, 4, 3, 2, "RF-15");
    reflow_case("ab\ncd", 5, 2, 8, 8, 2, 1, 2, "RF-16");
    reflow_case("abcde\nxy", 7, 5, 3, 8, 3, 2, 1, "RF-17");
    reflow_case("\nabc", 4, 4, 2, 8, 3, 2, 1, "RF-18");
}

static void test_redraw(void)
{
    repl89_edit e;
    capture cap;
    repl89_sink sink;
    repl89_region r1;
    repl89_region r2;
    vt_screen first;
    vt_screen redrawn;
    char t1[256];
    char t2[256];
    unsigned int i;

    repl89_edit_init(&e);
    set_text(&e, "abc\ndefgh", 5);
    cap.len = 0;
    sink.ctx = &cap;
    sink.emit = cap_emit;
    vt_init(&first);
    rp_check(repl89_render_draw(&e, "> ", "... ", 5, 8, &sink, &r1)
                 == REPL89_OK,
             "redraw draw 1");
    vt_feed(&first, cap.buf, cap.len);
    repl89_render_clear(&sink, &r1);
    rp_check(repl89_render_draw(&e, "> ", "... ", 5, 8, &sink, &r2)
                 == REPL89_OK,
             "redraw draw 2");
    vt_init(&redrawn);
    vt_feed(&redrawn, cap.buf, cap.len);
    for (i = 0; i < r2.rows; i++) {
        vt_row_text(&first, i, t1, sizeof t1);
        vt_row_text(&redrawn, i, t2, sizeof t2);
        rp_check(strcmp(t1, t2) == 0, "redraw rows match");
    }
    rp_check(first.row == redrawn.row, "redraw cursor row");
    rp_check(first.col == redrawn.col, "redraw cursor col");
    repl89_edit_free(&e);
}

void test_render(void)
{
    repl89_edit e;
    vt_screen scr;
    repl89_region reg;

    repl89_edit_init(&e);

    /* VT-01 single line, cursor at end */
    set_text(&e, "abc", 3);
    render(&e, "> ", "... ", 20, 8, &scr, &reg);
    expect_row(&scr, 0, "> abc", "VT-01");
    expect_cursor(&scr, 0, 5, "VT-01");
    rp_check(reg.rows == 1, "VT-01 rows");

    /* VT-02 cursor mid-line */
    set_text(&e, "abc", 1);
    render(&e, "> ", "... ", 20, 8, &scr, &reg);
    expect_row(&scr, 0, "> abc", "VT-02");
    expect_cursor(&scr, 0, 3, "VT-02");

    /* VT-03 empty submission shows the prompt */
    set_text(&e, "", 0);
    render(&e, "> ", "... ", 20, 8, &scr, &reg);
    expect_row(&scr, 0, "> ", "VT-03");
    expect_cursor(&scr, 0, 2, "VT-03");
    rp_check(reg.rows == 1, "VT-03 rows");

    /* VT-04 soft wrapping: the cursor at the right edge is physically on the
       last cell, since the logical column equals the width. */
    set_text(&e, "abcdefgh", 8);
    render(&e, "> ", "... ", 5, 8, &scr, &reg);
    expect_row(&scr, 0, "> abc", "VT-04 row0");
    expect_row(&scr, 1, "defgh", "VT-04 row1");
    expect_cursor(&scr, 1, 4, "VT-04");
    rp_check(reg.rows == 2, "VT-04 rows");

    /* VT-05 explicit newline uses the continuation prompt */
    set_text(&e, "a\nb", 3);
    render(&e, "> ", "... ", 20, 8, &scr, &reg);
    expect_row(&scr, 0, "> a", "VT-05 row0");
    expect_row(&scr, 1, "... b", "VT-05 row1");
    expect_cursor(&scr, 1, 5, "VT-05");

    /* VT-06 cursor returns to an earlier row */
    set_text(&e, "ab\ncd", 2);
    render(&e, "> ", ".. ", 20, 8, &scr, &reg);
    expect_row(&scr, 0, "> ab", "VT-06 row0");
    expect_row(&scr, 1, ".. cd", "VT-06 row1");
    expect_cursor(&scr, 0, 4, "VT-06");

    /* VT-07 multiline and wrapping together */
    set_text(&e, "abcd\nef", 7);
    render(&e, "> ", "", 4, 8, &scr, &reg);
    expect_row(&scr, 0, "> ab", "VT-07 row0");
    expect_row(&scr, 1, "cd", "VT-07 row1");
    expect_row(&scr, 2, "ef", "VT-07 row2");
    expect_cursor(&scr, 2, 2, "VT-07");
    rp_check(reg.rows == 3, "VT-07 rows");

    /* VT-08 CJK base occupies two cells */
    set_text(&e, "\xE4\xB8\xADx", 4);
    render(&e, ">", "", 20, 8, &scr, &reg);
    expect_row(&scr, 0, ">\xE4\xB8\xADx", "VT-08");
    expect_cursor(&scr, 0, 4, "VT-08");

    /* VT-09 combining sequence is one cell */
    set_text(&e, "e\xCC\x81", 3);
    render(&e, "", "", 20, 8, &scr, &reg);
    expect_row(&scr, 0, "e\xCC\x81", "VT-09");
    expect_cursor(&scr, 0, 1, "VT-09");

    /* VT-10 emoji presentation occupies two cells */
    set_text(&e, "\xF0\x9F\x98\x80", 4);
    render(&e, "", "", 20, 8, &scr, &reg);
    expect_row(&scr, 0, "\xF0\x9F\x98\x80", "VT-10");
    expect_cursor(&scr, 0, 2, "VT-10");

    /* VT-11 tab advances to the next tab stop */
    set_text(&e, "\tx", 2);
    render(&e, "", "", 20, 4, &scr, &reg);
    expect_row(&scr, 0, "    x", "VT-11");
    expect_cursor(&scr, 0, 5, "VT-11");

    /* VT-12 trailing LF leaves a continuation-prompt row */
    set_text(&e, "a\n", 2);
    render(&e, "> ", ". ", 20, 8, &scr, &reg);
    expect_row(&scr, 0, "> a", "VT-12 row0");
    expect_row(&scr, 1, ". ", "VT-12 row1");
    expect_cursor(&scr, 1, 2, "VT-12");
    rp_check(reg.rows == 2, "VT-12 rows");

    /* VT-13 cursor on a wrapped first row */
    set_text(&e, "abcdefgh", 2);
    render(&e, "> ", "", 5, 8, &scr, &reg);
    expect_row(&scr, 0, "> abc", "VT-13 row0");
    expect_row(&scr, 1, "defgh", "VT-13 row1");
    expect_cursor(&scr, 0, 4, "VT-13");

    /* VT-14 wide prompt */
    set_text(&e, "x", 1);
    render(&e, "\xE4\xB8\xAD", "", 20, 8, &scr, &reg);
    expect_row(&scr, 0, "\xE4\xB8\xADx", "VT-14");
    expect_cursor(&scr, 0, 3, "VT-14");

    test_vertical_layout();
    test_layout_edges();
    test_physical_edge();
    test_reflow();
    test_redraw();
    test_repaint();
    test_cursor_render();

    repl89_edit_free(&e);
}
