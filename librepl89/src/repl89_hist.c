/* repl89_hist.c - instance-owned in-memory history of whole submissions.
 * Adding an entry is atomic: on failure the history is unchanged. A limit of
 * zero disables retention. Every successful add appends exactly one entry
 * (duplicates included); at the limit the oldest entry is evicted. */

#include <string.h>

#include "repl89_internal.h"

void repl89_hist_init(repl89_hist *h, size_t limit)
{
    h->items = NULL;
    h->count = 0;
    h->cap = 0;
    h->limit = limit;
    h->nav = 0;
}

static void hist_release_items(repl89_hist *h)
{
    size_t i;

    for (i = 0; i < h->count; ++i)
    {
        repl89_mem_free(h->items[i].data);
    }
}

void repl89_hist_clear(repl89_hist *h)
{
    hist_release_items(h);
    h->count = 0;
    h->nav = 0;
}

void repl89_hist_free(repl89_hist *h)
{
    hist_release_items(h);
    repl89_mem_free(h->items);
    h->items = NULL;
    h->count = 0;
    h->cap = 0;
    h->nav = 0;
}

static repl89_error hist_reserve(repl89_hist *h, size_t need)
{
    size_t cap;
    repl89_entry *p;

    if (need <= h->cap)
    {
        return REPL89_OK;
    }
    cap = repl89_cap_for(need);
    if (cap < need)
    {
        cap = need;
    }
    p = repl89_mem_realloc(h->items, cap * sizeof(repl89_entry));
    if (p == NULL)
    {
        return REPL89_ENOMEM;
    }
    h->items = p;
    h->cap = cap;
    return REPL89_OK;
}

static char *hist_copy(const char *p, size_t n)
{
    char *q;

    q = repl89_mem_alloc(n + 1);
    if (q == NULL)
    {
        return NULL;
    }
    if (n > 0)
    {
        memcpy(q, p, n);
    }
    q[n] = 0;
    return q;
}

static void entry_move(repl89_hist *h, size_t i)
{
    h->items[i - 1] = h->items[i];
}

static void hist_shift_down(repl89_hist *h)
{
    size_t i;

    repl89_mem_free(h->items[0].data);
    for (i = 1; i < h->count; ++i)
    {
        entry_move(h, i);
    }
    h->count = h->count - 1;
}

repl89_error repl89_hist_add(repl89_hist *h, const char *p, size_t n)
{
    repl89_error err;
    char *copy;
    size_t i;

    err = repl89_content_check(p, n);
    if (err != REPL89_OK)
    {
        return err;
    }
    if (h->limit == 0)
    {
        return REPL89_OK;
    }
    err = hist_reserve(h, h->count + 1);
    if (err != REPL89_OK)
    {
        return err;
    }
    copy = hist_copy(p, n);
    if (copy == NULL)
    {
        return REPL89_ENOMEM;
    }
    if (h->count == h->limit)
    {
        hist_shift_down(h);
    }
    i = h->count;
    h->items[i].data = copy;
    h->items[i].len = n;
    h->count = h->count + 1;
    return REPL89_OK;
}

const repl89_entry *repl89_hist_at(const repl89_hist *h, size_t i)
{
    if (i >= h->count)
    {
        return NULL;
    }
    return &h->items[i];
}

void repl89_hist_nav_reset(repl89_hist *h)
{
    h->nav = h->count;
}

static size_t step_dec(size_t i)
{
    return i - 1;
}

static size_t step_inc(size_t i)
{
    return i + 1;
}

const repl89_entry *repl89_hist_nav_prev(repl89_hist *h)
{
    size_t i;

    if (h->nav == 0)
    {
        return NULL;
    }
    i = step_dec(h->nav);
    h->nav = i;
    return &h->items[i];
}

const repl89_entry *repl89_hist_nav_next(repl89_hist *h)
{
    size_t i;

    if (h->nav >= h->count)
    {
        return NULL;
    }
    i = step_inc(h->nav);
    h->nav = i;
    if (i >= h->count)
    {
        return NULL;
    }
    return &h->items[i];
}
