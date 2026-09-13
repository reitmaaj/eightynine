/* test_adapters_projection.c - AB06..AB09: an observer-fed projection equals
 * a rebuild from iteration alone; a late observer cannot corrupt the rebuild;
 * the observer is advisory; reversed ranges are rejected. */

#include <string.h>

#include "test.h"

#include "fixture.h"

#define PROJ_MAX 8

typedef struct proj
{
    unsigned long tag[PROJ_MAX + 1];
    int present[PROJ_MAX + 1];
    size_t calls;
} proj;

static void proj_put(proj *p, const ledger89_view *v)
{
    if (v->index <= (ledger89_index)PROJ_MAX)
    {
        p->tag[v->index] = v->tag;
        p->present[v->index] = 1;
    }
}

static void proj_observe(void *ctx, const ledger89_view *view)
{
    proj *p;

    p = (proj *)ctx;
    proj_put(p, view);
    ++p->calls;
}

static void proj_rebuild(fx *f, proj *p)
{
    ledger89_iter *it;
    ledger89_view v;
    int rc;

    memset(p, 0, sizeof *p);
    it = NULL;
    CHECK_EQ(ledger89_iter_open(f->l, 0ul, 0ul, &it), LEDGER89_OK);
    for (;;)
    {
        rc = ledger89_iter_next(it, &v);
        if (rc != LEDGER89_OK)
        {
            break;
        }
        proj_put(p, &v);
    }
    CHECK_EQ(rc, LEDGER89_END);
    ledger89_iter_close(it);
}

static void proj_compare(const proj *a, const proj *b, ledger89_index first,
                         ledger89_index last)
{
    ledger89_index i;

    for (i = first; i <= last; ++i)
    {
        CHECK_EQ(a->present[i], b->present[i]);
        CHECK_EQ(a->tag[i], b->tag[i]);
    }
}

static int append_range(fx *f, ledger89_index first, size_t count,
                        unsigned long tag_base)
{
    ledger89_record batch[PROJ_MAX];
    unsigned char data[1];
    size_t i;

    data[0] = (unsigned char)'p';
    for (i = 0u; i < count; ++i)
    {
        fx_record(&batch[i], first + (ledger89_index)i,
                  tag_base + (unsigned long)i, data, sizeof data);
    }
    return ledger89_append(f->l, batch, count);
}

int main(void)
{
    fx f;
    fx g;
    proj obs;
    proj rebuilt;
    ledger89_iter *it;

    /* AB06: observer projection equals iteration rebuild after reopen. */
    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    memset(&obs, 0, sizeof obs);
    CHECK_EQ(ledger89_set_observer(f.l, proj_observe, &obs), LEDGER89_OK);
    CHECK_EQ(append_range(&f, 1ul, 5u, 100ul), LEDGER89_OK);
    CHECK_EQ(obs.calls, 5u);
    CHECK_EQ(obs.tag[1], 100ul);
    CHECK_EQ(obs.tag[5], 104ul);
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    proj_rebuild(&f, &rebuilt);
    CHECK_EQ(rebuilt.calls, 0u);
    proj_compare(&obs, &rebuilt, 1ul, 5ul);
    fx_close(&f);

    /* AB07: a late observer misses earlier records but the rebuild is
     * complete. */
    CHECK_EQ(fx_open(&g), LEDGER89_OK);
    CHECK_EQ(append_range(&g, 1ul, 2u, 200ul), LEDGER89_OK);
    memset(&obs, 0, sizeof obs);
    CHECK_EQ(ledger89_set_observer(g.l, proj_observe, &obs), LEDGER89_OK);
    CHECK_EQ(append_range(&g, 3ul, 2u, 300ul), LEDGER89_OK);
    CHECK_EQ(obs.calls, 2u);
    proj_rebuild(&g, &rebuilt);
    CHECK_EQ(rebuilt.present[1], 1);
    CHECK_EQ(rebuilt.present[4], 1);
    CHECK_EQ(rebuilt.tag[1], 200ul);
    CHECK_EQ(rebuilt.tag[4], 301ul);
    CHECK_EQ(obs.present[1], 0);
    CHECK_EQ(obs.present[3], 1);
    CHECK_EQ(obs.tag[3], 300ul);

    /* AB08: rejected appends never call the observer; NULL clears it. */
    CHECK_EQ(fx_append(&g, 4ul, 999ul, "z", 1u), LEDGER89_ERR_SEQUENCE);
    CHECK_EQ(obs.calls, 2u);
    CHECK_EQ(ledger89_set_observer(g.l, NULL, NULL), LEDGER89_OK);
    CHECK_EQ(append_range(&g, 5ul, 1u, 500ul), LEDGER89_OK);
    CHECK_EQ(obs.calls, 2u);

    /* AB09: reversed explicit iteration range is rejected. */
    it = NULL;
    CHECK_EQ(ledger89_iter_open(g.l, 4ul, 2ul, &it), LEDGER89_ERR_ARG);
    CHECK(it == NULL);

    fx_close(&g);

    TEST_END;
}
