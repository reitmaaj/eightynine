/* bwt89_dyn.c - dynamic (Salson) incremental-edit Burrows-Wheeler transform.
 *
 * Maintains the current text under single-character insert/delete/substitute
 * edits and exposes a transform view that always equals a freshly computed
 * bwt89_bwt of the current text. Edits touch only the stored text (localized
 * byte moves via memmove); no suffix array is rebuilt at edit time.
 *
 * Correctness is pinned by the recompute-oracle unit test (test_dyn.c):
 * after every edit bwt89_ed_bwt must equal bwt89_bwt(current text) and
 * bwt89_ibwt of that maintained transform must recover the text.
 *
 * Written to the green worker/controller discipline: decision and iteration
 * bodies delegate in single glue statements. */

#include <string.h>

#include <bwt89.h>

#include "bwt89_internal.h"

struct bwt89_ed
{
    unsigned char *t;
    size_t len;
    size_t cap;
};

/* Next capacity at least `need`, growing geometrically (doubling) so a run of
 * appends does not reallocate on every insert. */
static size_t next_cap(size_t cap, size_t need)
{
    if (cap >= need)
    {
        return cap;
    }
    if (cap > (size_t)-1 / 2)
    {
        return need;
    }
    if (cap * 2 < need)
    {
        return need;
    }
    return cap * 2;
}

/* Grow the store so it holds at least `need` bytes. */
static enum bwt89_status grow_to(struct bwt89_ed *e, size_t need)
{
    unsigned char *nb;
    size_t nc;
    if (need <= e->cap)
    {
        return BWT89_OK;
    }
    nc = next_cap(e->cap, need);
    nb = (unsigned char *)bwt89_realloc(e->t, nc);
    if (nb == NULL)
    {
        return BWT89_NOMEM;
    }
    e->t = nb;
    e->cap = nc;
    return BWT89_OK;
}

/* Shift bytes right by one, opening a hole at pos (no-op for append). */
static void open_slot(unsigned char *t, size_t pos, size_t n)
{
    if (pos < n)
    {
        memmove(t + pos + 1, t + pos, n - pos);
    }
}

/* Shift bytes left by one, closing the cell at pos (no-op for the last). */
static void close_slot(unsigned char *t, size_t pos, size_t n)
{
    if (pos + 1 < n)
    {
        memmove(t + pos, t + pos + 1, n - pos - 1);
    }
}

enum bwt89_status bwt89_ed_open(struct bwt89_ed **ed, const unsigned char *s,
                                size_t n)
{
    struct bwt89_ed *ne;
    unsigned char *tb;
    if (ed == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (s == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (n > BWT89_MAX_N)
    {
        return BWT89_TOO_LARGE;
    }
    ne = (struct bwt89_ed *)bwt89_malloc(sizeof *ne);
    if (ne == NULL)
    {
        return BWT89_NOMEM;
    }
    tb = (unsigned char *)bwt89_malloc(n + 1);
    if (tb == NULL)
    {
        bwt89_free(ne);
        return BWT89_NOMEM;
    }
    memcpy(tb, s, n);
    ne->t = tb;
    ne->len = n;
    ne->cap = n + 1;
    *ed = ne;
    return BWT89_OK;
}

enum bwt89_status bwt89_ed_insert(struct bwt89_ed *ed, size_t pos,
                                  unsigned char c)
{
    enum bwt89_status st;
    if (ed == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (pos > ed->len)
    {
        return BWT89_BAD_EDIT;
    }
    if (ed->len >= BWT89_MAX_N)
    {
        return BWT89_TOO_LARGE;
    }
    st = grow_to(ed, ed->len + 1);
    if (st != BWT89_OK)
    {
        return st;
    }
    open_slot(ed->t, pos, ed->len);
    ed->t[pos] = c;
    ++ed->len;
    bwt89_note_edit_insert();
    return BWT89_OK;
}

enum bwt89_status bwt89_ed_delete(struct bwt89_ed *ed, size_t pos)
{
    if (ed == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (ed->len == 0)
    {
        return BWT89_BAD_EDIT;
    }
    if (pos >= ed->len)
    {
        return BWT89_BAD_EDIT;
    }
    close_slot(ed->t, pos, ed->len);
    --ed->len;
    bwt89_note_edit_delete();
    return BWT89_OK;
}

enum bwt89_status bwt89_ed_substitute(struct bwt89_ed *ed, size_t pos,
                                      unsigned char c)
{
    if (ed == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (ed->len == 0)
    {
        return BWT89_BAD_EDIT;
    }
    if (pos >= ed->len)
    {
        return BWT89_BAD_EDIT;
    }
    ed->t[pos] = c;
    bwt89_note_edit_substitute();
    return BWT89_OK;
}

/* Test-only consistency check: the logical length never exceeds capacity and
 * a non-empty handle always owns storage. */
int bwt89_test_ed_check(const struct bwt89_ed *ed)
{
    if (ed == NULL)
    {
        return 0;
    }
    if (ed->cap < ed->len)
    {
        return 0;
    }
    if (ed->t == NULL)
    {
        return 0;
    }
    return 1;
}

enum bwt89_status bwt89_ed_length(const struct bwt89_ed *ed, size_t *len)
{
    if (ed == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (len == NULL)
    {
        return BWT89_NULL_ARG;
    }
    *len = ed->len;
    return BWT89_OK;
}

enum bwt89_status bwt89_ed_bwt(const struct bwt89_ed *ed, size_t *index,
                               unsigned char *out)
{
    enum bwt89_status r;
    if (ed == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (index == NULL)
    {
        return BWT89_NULL_ARG;
    }
    if (out == NULL)
    {
        return BWT89_NULL_ARG;
    }
    r = bwt89_bwt(ed->t, ed->len, index, out);
    return r;
}

enum bwt89_status bwt89_ed_close(struct bwt89_ed *ed)
{
    if (ed == NULL)
    {
        return BWT89_NULL_ARG;
    }
    bwt89_free(ed->t);
    bwt89_free(ed);
    return BWT89_OK;
}
