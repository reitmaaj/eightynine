/* test_read_iter.c - RD01..RD10 and IT01..IT14: random read and ordered
 * iteration. */

#include <string.h>

#include "test.h"

#include "fixture.h"

static int iter_collect(fx *f, ledger89_index first, ledger89_index last,
                        ledger89_index *out, size_t cap, size_t *count)
{
    ledger89_iter *it;
    ledger89_view v;
    size_t n;
    int rc;

    it = NULL;
    *count = 0u;
    rc = ledger89_iter_open(f->l, first, last, &it);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    n = 0u;
    for (;;)
    {
        rc = ledger89_iter_next(it, &v);
        if (rc == LEDGER89_END)
        {
            break;
        }
        if (rc != LEDGER89_OK)
        {
            ledger89_iter_close(it);
            return rc;
        }
        if (n >= cap)
        {
            ledger89_iter_close(it);
            return LEDGER89_ERR_RANGE;
        }
        out[n] = v.index;
        ++n;
    }
    ledger89_iter_close(it);
    *count = n;
    return LEDGER89_OK;
}

int main(void)
{
    fx f;
    ledger89_view v;
    ledger89_index got[8];
    size_t count;
    unsigned char payload[1];
    ledger89_index i;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    for (i = 1ul; i <= 5ul; ++i)
    {
        payload[0] = (unsigned char)('0' + (int)i);
        CHECK_EQ(fx_append(&f, i, (unsigned long)(i * 10ul), payload, 1u),
                 LEDGER89_OK);
    }
    CHECK_EQ(ledger89_last_index(f.l), 5ul);

    /* RD01/RD02/RD03: first, middle, last. */
    CHECK_EQ(fx_read(&f, 1ul, &v), LEDGER89_OK);
    CHECK_EQ(v.index, 1ul);
    CHECK_EQ(v.tag, 10ul);
    CHECK_EQ(v.size, 1u);
    CHECK_EQ(((const unsigned char *)v.data)[0], (unsigned char)'1');

    CHECK_EQ(fx_read(&f, 3ul, &v), LEDGER89_OK);
    CHECK_EQ(v.index, 3ul);
    CHECK_EQ(v.tag, 30ul);

    CHECK_EQ(fx_read(&f, 5ul, &v), LEDGER89_OK);
    CHECK_EQ(v.index, 5ul);

    /* RD04/RD05/RD08: missing and stable repeats. */
    CHECK_EQ(fx_read(&f, 0ul, &v), LEDGER89_ERR_ARG);
    CHECK_EQ(fx_read(&f, 6ul, &v), LEDGER89_ERR_NOTFOUND);
    CHECK_EQ(fx_read(&f, 2ul, &v), LEDGER89_OK);
    CHECK_EQ(fx_read(&f, 2ul, &v), LEDGER89_OK);
    CHECK_EQ(v.index, 2ul);

    /* IT03: iterate all. */
    CHECK_EQ(iter_collect(&f, 0ul, 0ul, got, 8u, &count), LEDGER89_OK);
    CHECK_EQ(count, 5u);
    CHECK_EQ(got[0], 1ul);
    CHECK_EQ(got[4], 5ul);

    /* IT04: subrange. */
    CHECK_EQ(iter_collect(&f, 2ul, 4ul, got, 8u, &count), LEDGER89_OK);
    CHECK_EQ(count, 3u);
    CHECK_EQ(got[0], 2ul);
    CHECK_EQ(got[2], 4ul);

    /* IT05/IT06: single-element and wildcard bounds. */
    CHECK_EQ(iter_collect(&f, 4ul, 4ul, got, 8u, &count), LEDGER89_OK);
    CHECK_EQ(count, 1u);
    CHECK_EQ(got[0], 4ul);
    CHECK_EQ(iter_collect(&f, 3ul, 0ul, got, 8u, &count), LEDGER89_OK);
    CHECK_EQ(count, 3u);
    CHECK_EQ(got[0], 3ul);
    CHECK_EQ(iter_collect(&f, 0ul, 2ul, got, 8u, &count), LEDGER89_OK);
    CHECK_EQ(count, 2u);
    CHECK_EQ(got[1], 2ul);

    /* IT10/IT11: clamping. */
    CHECK_EQ(iter_collect(&f, 1ul, 99ul, got, 8u, &count), LEDGER89_OK);
    CHECK_EQ(count, 5u);
    CHECK_EQ(iter_collect(&f, 6ul, 9ul, got, 8u, &count), LEDGER89_OK);
    CHECK_EQ(count, 0u);

    /* IT12: reversed explicit range. */
    {
        ledger89_iter *it;

        it = NULL;
        CHECK_EQ(ledger89_iter_open(f.l, 4ul, 2ul, &it), LEDGER89_ERR_ARG);
        CHECK(it == NULL);
    }

    /* IT14: independent simultaneous iterators. */
    {
        ledger89_iter *a;
        ledger89_iter *b;
        ledger89_view va;
        ledger89_view vb;

        a = NULL;
        b = NULL;
        CHECK_EQ(ledger89_iter_open(f.l, 1ul, 5ul, &a), LEDGER89_OK);
        CHECK_EQ(ledger89_iter_open(f.l, 1ul, 5ul, &b), LEDGER89_OK);
        CHECK_EQ(ledger89_iter_next(a, &va), LEDGER89_OK);
        CHECK_EQ(va.index, 1ul);
        CHECK_EQ(ledger89_iter_next(b, &vb), LEDGER89_OK);
        CHECK_EQ(vb.index, 1ul);
        CHECK_EQ(ledger89_iter_next(a, &va), LEDGER89_OK);
        CHECK_EQ(va.index, 2ul);
        CHECK_EQ(ledger89_iter_next(b, &vb), LEDGER89_OK);
        CHECK_EQ(vb.index, 2ul);
        ledger89_iter_close(a);
        ledger89_iter_close(b);
    }

    fx_close(&f);

    /* IT01: iterating an empty ledger ends immediately. */
    {
        fx e;
        ledger89_iter *it;
        ledger89_view ev;

        CHECK_EQ(fx_open(&e), LEDGER89_OK);
        it = NULL;
        CHECK_EQ(ledger89_iter_open(e.l, 0ul, 0ul, &it), LEDGER89_OK);
        CHECK_EQ(ledger89_iter_next(it, &ev), LEDGER89_END);
        ledger89_iter_close(it);
        fx_close(&e);
    }

    TEST_END;
}
