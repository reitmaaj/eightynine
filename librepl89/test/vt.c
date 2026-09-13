/* vt.c - deliberately small test-only ECMA-48 emulator. It supports exactly
 * the output librepl89 emits: CR, LF, cursor up/forward, cursor addressing,
 * erase line/screen, bracketed-paste toggles, and printable UTF-8. Cell
 * widths use the same libu89 facts as the renderer policy. */

#include <string.h>

#include "vt.h"
#include "u89.h"

static void cell_clear(vt_cell *c)
{
    c->len = 0;
    c->width = 0;
    c->cont = 0;
}

void vt_init(vt_screen *s)
{
    unsigned int r;
    unsigned int c;

    for (r = 0; r < VT_ROWS; ++r) {
        for (c = 0; c < VT_COLS; ++c) {
            cell_clear(&s->cells[r][c]);
        }
    }
    s->cols = VT_COLS;
    s->row = 0;
    s->col = 0;
}

void vt_set_cols(vt_screen *s, unsigned int cols)
{
    if (cols == 0 || cols > VT_COLS) {
        return;
    }
    s->cols = cols;
}

static void vt_scroll(vt_screen *s)
{
    unsigned int r;
    unsigned int c;

    for (r = 1; r < VT_ROWS; ++r) {
        for (c = 0; c < VT_COLS; ++c) {
            s->cells[r - 1][c] = s->cells[r][c];
        }
    }
    for (c = 0; c < VT_COLS; ++c) {
        cell_clear(&s->cells[VT_ROWS - 1][c]);
    }
    s->row = VT_ROWS - 1;
}

static void vt_newline(vt_screen *s)
{
    if (s->row + 1 >= VT_ROWS) {
        vt_scroll(s);
    } else {
        s->row = s->row + 1;
    }
}

static void vt_wrap(vt_screen *s, int width)
{
    if (s->col + (unsigned int)width > s->cols) {
        s->col = 0;
        vt_newline(s);
    }
}

static void vt_place(vt_screen *s, const char *p, size_t n, int width)
{
    vt_cell *cell;

    if (s->col >= VT_COLS) {
        return;
    }
    cell = &s->cells[s->row][s->col];
    cell_clear(cell);
    if (n > VT_CELL_MAX) {
        n = VT_CELL_MAX;
    }
    memcpy(cell->bytes, p, n);
    cell->len = n;
    cell->width = width;
    if (width == 2) {
        if (s->col + 1 < s->cols) {
            cell_clear(&s->cells[s->row][s->col + 1]);
            s->cells[s->row][s->col + 1].cont = 1;
        }
    }
    s->col = s->col + (unsigned int)width;
}

static unsigned int vt_base_cell(const vt_screen *s)
{
    unsigned int c;

    c = s->col;
    while (c > 0) {
        if (!s->cells[s->row][c - 1].cont) {
            return c - 1;
        }
        c = c - 1;
    }
    return 0;
}

static void vt_put_zero(vt_screen *s, const char *p, size_t n)
{
    vt_cell *cell;
    unsigned int c;

    c = vt_base_cell(s);
    cell = &s->cells[s->row][c];
    if (cell->len + n <= VT_CELL_MAX) {
        memcpy(cell->bytes + cell->len, p, n);
        cell->len = cell->len + n;
    }
}

static int cp_cells(u89_cp cp)
{
    int mark;
    int ign;
    u89_eaw eaw;

    mark = u89_is_mark(cp);
    if (mark) {
        return 0;
    }
    ign = u89_default_ignorable(cp);
    if (ign) {
        return 0;
    }
    eaw = u89_east_asian_width(cp);
    if (eaw == U89_EAW_W) {
        return 2;
    }
    if (eaw == U89_EAW_F) {
        return 2;
    }
    if (u89_is_emoji_presentation(cp)) {
        return 2;
    }
    return 1;
}

static size_t vt_put_utf8(vt_screen *s, const char *p, size_t n, size_t i)
{
    u89_cp cp;
    u89_status st;
    size_t next;
    int w;

    cp = 0;
    next = i;
    st = u89_utf8_decode((const unsigned char *)p, n, i, &cp, &next);
    if (st != U89_OK) {
        return n;
    }
    w = cp_cells(cp);
    if (w == 0) {
        vt_put_zero(s, p + i, next - i);
    } else {
        vt_wrap(s, w);
        vt_place(s, p + i, next - i, w);
    }
    return next;
}

static void vt_apply(vt_screen *s, char final, const int *params, int np)
{
    int n;
    unsigned int u;

    n = 1;
    if (np > 0 && params[0] > 0) {
        n = params[0];
    }
    u = (unsigned int)n;
    if (final == 'A') {
        if (s->row < u) {
            s->row = 0;
        } else {
            s->row = s->row - u;
        }
    } else if (final == 'B') {
        s->row = s->row + u;
        while (s->row >= VT_ROWS) {
            vt_scroll(s);
        }
    } else if (final == 'C') {
        s->col = s->col + u;
        if (s->col > s->cols) {
            s->col = s->cols;
        }
    } else if (final == 'D') {
        if (s->col < u) {
            s->col = 0;
        } else {
            s->col = s->col - u;
        }
    } else if (final == 'H' || final == 'f') {
        int r;
        int c;

        r = 1;
        c = 1;
        if (np > 0 && params[0] > 0) {
            r = params[0];
        }
        if (np > 1 && params[1] > 0) {
            c = params[1];
        }
        s->row = (unsigned int)(r - 1);
        s->col = (unsigned int)(c - 1);
        if (s->row >= VT_ROWS) {
            s->row = VT_ROWS - 1;
        }
        if (s->col >= s->cols) {
            s->col = s->cols - 1;
        }
    } else if (final == 'K') {
        unsigned int c;

        if (np > 0 && params[0] == 2) {
            for (c = 0; c < s->cols; ++c) {
                cell_clear(&s->cells[s->row][c]);
            }
        } else {
            for (c = s->col; c < s->cols; ++c) {
                cell_clear(&s->cells[s->row][c]);
            }
        }
    } else if (final == 'J') {
        unsigned int c;

        if (np == 0 || params[0] == 0) {
            for (c = s->col; c < s->cols; ++c) {
                cell_clear(&s->cells[s->row][c]);
            }
        }
    }
}

static size_t vt_escape(vt_screen *s, const char *p, size_t n, size_t i)
{
    size_t j;
    int params[4];
    int np;
    int cur;
    int has;
    char final;

    j = i + 1;
    if (j >= n) {
        return n;
    }
    if (p[j] != '[') {
        return j + 1;
    }
    j = j + 1;
    params[0] = 0;
    params[1] = 0;
    params[2] = 0;
    params[3] = 0;
    np = 0;
    cur = 0;
    has = 0;
    while (j < n) {
        char c;

        c = p[j];
        if (c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
            has = 1;
            j = j + 1;
        } else if (c == ';') {
            if (np < 4) {
                params[np] = cur;
                np = np + 1;
            }
            cur = 0;
            has = 0;
            j = j + 1;
        } else if (c == '?') {
            j = j + 1;
        } else {
            break;
        }
    }
    if (j >= n) {
        return n;
    }
    if (has && np < 4) {
        params[np] = cur;
        np = np + 1;
    }
    final = p[j];
    j = j + 1;
    vt_apply(s, final, params, np);
    return j;
}

void vt_feed(vt_screen *s, const char *p, size_t n)
{
    size_t i;
    unsigned char b;

    i = 0;
    while (i < n) {
        b = (unsigned char)p[i];
        if (b == 0x1B) {
            i = vt_escape(s, p, n, i);
        } else if (b == '\r') {
            s->col = 0;
            i = i + 1;
        } else if (b == '\n') {
            vt_newline(s);
            i = i + 1;
        } else if (b < 0x80) {
            vt_wrap(s, 1);
            vt_place(s, p + i, 1, 1);
            i = i + 1;
        } else {
            i = vt_put_utf8(s, p, n, i);
        }
    }
}

void vt_row_text(const vt_screen *s, unsigned int row, char *out, size_t cap)
{
    unsigned int c;
    size_t len;
    size_t n;

    if (cap == 0) {
        return;
    }
    len = 0;
    for (c = 0; c < s->cols; ++c) {
        if (s->cells[row][c].cont) {
            continue;
        }
        n = s->cells[row][c].len;
        if (len + n + 1 > cap) {
            break;
        }
        memcpy(out + len, s->cells[row][c].bytes, n);
        len = len + n;
    }
    out[len] = 0;
}

void vt_resize(vt_screen *s, unsigned int new_cols)
{
    vt_cell out[VT_ROWS][VT_COLS];
    unsigned int out_row;
    unsigned int out_col;
    unsigned int cur_row;
    unsigned int cur_col;
    unsigned int r;
    unsigned int j;
    unsigned int c;
    unsigned int w;
    int cursor_done;

    if (new_cols == 0 || new_cols > VT_COLS) {
        return;
    }
    for (r = 0; r < VT_ROWS; ++r) {
        for (c = 0; c < VT_COLS; ++c) {
            cell_clear(&out[r][c]);
        }
    }
    out_row = 0;
    out_col = 0;
    cur_row = 0;
    cur_col = 0;
    cursor_done = 0;
    for (r = 0; r < VT_ROWS; ++r) {
        for (j = 0; j < s->cols; ++j) {
            if (r == s->row && j == s->col && cursor_done == 0) {
                cur_row = out_row;
                cur_col = out_col;
                cursor_done = 1;
            }
            if (s->cells[r][j].cont) {
                continue;
            }
            if (s->cells[r][j].len == 0) {
                continue;
            }
            w = (unsigned int)s->cells[r][j].width;
            if (out_col + w > new_cols) {
                out_row = out_row + 1;
                out_col = 0;
            }
            if (out_row < VT_ROWS) {
                out[out_row][out_col] = s->cells[r][j];
                if (w == 2) {
                    if (out_col + 1 < VT_COLS) {
                        cell_clear(&out[out_row][out_col + 1]);
                        out[out_row][out_col + 1].cont = 1;
                    }
                }
            }
            out_col = out_col + w;
        }
        if (r == s->row && cursor_done == 0) {
            cur_row = out_row;
            cur_col = out_col;
            cursor_done = 1;
        }
        out_row = out_row + 1;
        out_col = 0;
        if (out_row >= VT_ROWS) {
            break;
        }
    }
    for (r = 0; r < VT_ROWS; ++r) {
        for (c = 0; c < VT_COLS; ++c) {
            s->cells[r][c] = out[r][c];
        }
    }
    s->cols = new_cols;
    s->row = cur_row;
    s->col = cur_col;
}
