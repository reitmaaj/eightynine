/* test_random_model.c - randomized reference-model stress test.
 *
 * Usage: test_random_model [seed] [operations]
 *
 * Maintains a simple model of the visible sequence plus its durable prefix
 * and drives random append, sync, rotate, read, iterate, truncate, discard,
 * reopen, process-crash, and power-loss operations. After every operation
 * (and periodically in full) the ledger must match the model exactly. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"

#include "model_fs.h"

#define RM_MAX 1024
#define RM_PAYLOAD 32
#define RM_WINDOW 256

typedef struct rm_rec
{
    unsigned long tag;
    size_t size;
    unsigned char data[RM_PAYLOAD];
} rm_rec;

typedef struct rm_model
{
    rm_rec live[RM_MAX];
    rm_rec dur[RM_MAX];
    unsigned long first;
    unsigned long last;
    unsigned long dfirst;
    unsigned long dlast;
} rm_model;

static unsigned long rnd_state;
static unsigned long diag_seed;
static unsigned long diag_op;

static unsigned long rnd_next(void)
{
    rnd_state = (rnd_state * 1103515245ul) + 12345ul;
    return (rnd_state >> 8) & 0x00FFFFFFul;
}

static unsigned long rnd_below(unsigned long n)
{
    if (n == 0ul)
    {
        return 1ul;
    }
    return rnd_next() % n;
}

static void rm_init(rm_model *m)
{
    memset(m, 0, sizeof *m);
    m->first = 1ul;
    m->last = 0ul;
    m->dfirst = 1ul;
    m->dlast = 0ul;
}

static void rm_sync(rm_model *m)
{
    memcpy(m->dur, m->live, sizeof m->live);
    m->dfirst = m->first;
    m->dlast = m->last;
}

static void rm_power_crash(rm_model *m)
{
    memcpy(m->live, m->dur, sizeof m->live);
    m->first = m->dfirst;
    m->last = m->dlast;
}

static int rm_check_record(const rm_model *m, unsigned long index,
                           const ledger89_view *v)
{
    const rm_rec *r;

    r = &m->live[index % RM_MAX];
    if (v->index != index)
    {
        return 0;
    }
    if (v->tag != r->tag)
    {
        return 0;
    }
    if (v->size != r->size)
    {
        return 0;
    }
    if (r->size > 0u)
    {
        if (memcmp(v->data, r->data, r->size) != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void rm_compare(const rm_model *m, ledger89 *l)
{
    ledger89_index first;
    ledger89_index last;
    unsigned long i;

    first = ledger89_first_index(l);
    last = ledger89_last_index(l);
    CHECK_EQ(first, m->first);
    CHECK_EQ(last, m->last);
    for (i = m->first; i <= m->last; ++i)
    {
        ledger89_view v;

        CHECK_EQ(ledger89_read(l, i, &v), LEDGER89_OK);
        CHECK(rm_check_record(m, i, &v) != 0);
    }
    {
        ledger89_iter *it;
        ledger89_view v;
        unsigned long expect;
        int rc;

        it = NULL;
        CHECK_EQ(ledger89_iter_open(l, 0ul, 0ul, &it), LEDGER89_OK);
        expect = m->first;
        for (;;)
        {
            rc = ledger89_iter_next(it, &v);
            if (expect > m->last)
            {
                CHECK_EQ(rc, LEDGER89_END);
                break;
            }
            CHECK_EQ(rc, LEDGER89_OK);
            if (rc != LEDGER89_OK)
            {
                break;
            }
            CHECK_EQ(v.index, expect);
            CHECK(rm_check_record(m, expect, &v) != 0);
            ++expect;
        }
        ledger89_iter_close(it);
    }
}

static int open_model(mfs *fs, led89_io *io, ledger89 **l)
{
    ledger89_config cfg;

    mfs_bind(io, fs);
    memset(&cfg, 0, sizeof cfg);
    cfg.path = "ledger";
    cfg.max_segment_bytes = 0ul;
    cfg.max_segment_records = 0ul;
    *l = NULL;
    return led89_open_io(l, &cfg, io);
}

static int op_append(rm_model *m, ledger89 *l)
{
    ledger89_record recs[4];
    unsigned char data[4][RM_PAYLOAD];
    unsigned long count;
    unsigned long i;
    size_t j;

    count = 1ul + rnd_below(4ul);
    for (i = 0ul; i < count; ++i)
    {
        unsigned long index;
        size_t size;
        rm_rec *slot;

        index = m->last + 1ul + i;
        size = (size_t)rnd_below(RM_PAYLOAD + 1ul);
        for (j = 0u; j < size; ++j)
        {
            data[i][j] = (unsigned char)rnd_next();
        }
        recs[i].index = index;
        recs[i].tag = rnd_next();
        recs[i].data = data[i];
        recs[i].size = size;
        slot = &m->live[index % RM_MAX];
        slot->tag = recs[i].tag;
        slot->size = size;
        if (size > 0u)
        {
            memcpy(slot->data, data[i], size);
        }
    }
    if (ledger89_append(l, recs, count) != LEDGER89_OK)
    {
        return 0;
    }
    m->last += count;
    return 1;
}

static int op_read(const rm_model *m, ledger89 *l)
{
    ledger89_view v;
    unsigned long index;

    if (m->last >= m->first)
    {
        index = m->first + rnd_below(m->last - m->first + 1ul);
        if (ledger89_read(l, index, &v) != LEDGER89_OK)
        {
            return 0;
        }
        if (rm_check_record(m, index, &v) == 0)
        {
            return 0;
        }
    }
    index = m->last + 1ul + rnd_below(10ul);
    if (ledger89_read(l, index, &v) != LEDGER89_ERR_NOTFOUND)
    {
        return 0;
    }
    return 1;
}

static int op_iterate(const rm_model *m, ledger89 *l)
{
    ledger89_iter *it;
    ledger89_view v;
    unsigned long lo;
    unsigned long hi;
    unsigned long expect;
    int rc;

    if (m->last >= m->first)
    {
        lo = m->first + rnd_below(m->last - m->first + 1ul);
        hi = lo + rnd_below(m->last - lo + 1ul);
    }
    else
    {
        lo = 1ul;
        hi = 0ul;
    }
    it = NULL;
    if (ledger89_iter_open(l, lo, hi, &it) != LEDGER89_OK)
    {
        return 0;
    }
    expect = lo;
    for (;;)
    {
        rc = ledger89_iter_next(it, &v);
        if (expect > hi)
        {
            break;
        }
        if (rc != LEDGER89_OK)
        {
            break;
        }
        if (v.index != expect)
        {
            ledger89_iter_close(it);
            return 0;
        }
        if (rm_check_record(m, expect, &v) == 0)
        {
            ledger89_iter_close(it);
            return 0;
        }
        ++expect;
    }
    ledger89_iter_close(it);
    if (expect > hi)
    {
        return rc == LEDGER89_END;
    }
    return 0;
}

static int op_rotate(rm_model *m, ledger89 *l)
{
    int rc;

    if (ledger89_sync(l) != LEDGER89_OK)
    {
        return 0;
    }
    rm_sync(m);
    rc = ledger89_rotate(l);
    return rc == LEDGER89_OK;
}

static int op_sync(rm_model *m, ledger89 *l)
{
    if (ledger89_sync(l) != LEDGER89_OK)
    {
        return 0;
    }
    rm_sync(m);
    return 1;
}

static int op_truncate_at(rm_model *m, ledger89 *l, unsigned long n)
{
    int rc;

    if (ledger89_sync(l) != LEDGER89_OK)
    {
        return 0;
    }
    rm_sync(m);
    if (n >= m->last)
    {
        return ledger89_truncate_after(l, n) == LEDGER89_OK;
    }
    if (n < m->first - 1ul)
    {
        return ledger89_truncate_after(l, n) == LEDGER89_ERR_RANGE;
    }
    rc = ledger89_truncate_after(l, n);
    if (rc != LEDGER89_OK)
    {
        return 0;
    }
    m->last = n;
    rm_sync(m);
    return 1;
}

static int op_truncate(rm_model *m, ledger89 *l)
{
    return op_truncate_at(m, l, rnd_below(m->last + 8ul));
}

static int op_discard_at(rm_model *m, ledger89 *l, unsigned long n)
{
    int rc;

    if (ledger89_sync(l) != LEDGER89_OK)
    {
        return 0;
    }
    rm_sync(m);
    if (n <= m->first)
    {
        return ledger89_discard_before(l, n) == LEDGER89_OK;
    }
    if (n > m->last)
    {
        n = m->last + 1ul;
    }
    rc = ledger89_discard_before(l, n);
    if (rc != LEDGER89_OK)
    {
        return 0;
    }
    m->first = n;
    if (n > m->last)
    {
        m->last = n - 1ul;
    }
    rm_sync(m);
    return 1;
}

static int op_discard(rm_model *m, ledger89 *l)
{
    return op_discard_at(m, l, rnd_below(m->last + 8ul));
}

int main(int argc, char **argv)
{
    unsigned long seed;
    unsigned long ops;
    unsigned long op;
    mfs fs;
    led89_io io;
    ledger89 *l;
    rm_model m;

    seed = 1ul;
    ops = 20000ul;
    if (argc > 1)
    {
        seed = strtoul(argv[1], NULL, 10);
    }
    if (argc > 2)
    {
        ops = strtoul(argv[2], NULL, 10);
    }
    rnd_state = seed;
    diag_seed = seed;
    diag_op = 0ul;

    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l), LEDGER89_OK);
    rm_init(&m);

    for (op = 1ul; op <= ops; ++op)
    {
        unsigned long choice;
        int ok;

        diag_op = op;
        if (m.last >= m.first)
        {
            if ((m.last - m.first) > RM_WINDOW)
            {
                if (rnd_below(2ul) == 0ul)
                {
                    ok = op_discard_at(&m, l, m.first + 64ul);
                }
                else
                {
                    ok = op_truncate_at(&m, l, m.last - 64ul);
                }
                CHECK(ok != 0);
                continue;
            }
        }
        choice = rnd_below(100ul);
        if (choice < 30ul)
        {
            ok = op_append(&m, l);
        }
        else if (choice < 45ul)
        {
            ok = op_sync(&m, l);
        }
        else if (choice < 55ul)
        {
            ok = op_rotate(&m, l);
        }
        else if (choice < 70ul)
        {
            ok = op_read(&m, l);
        }
        else if (choice < 80ul)
        {
            ok = op_iterate(&m, l);
        }
        else if (choice < 85ul)
        {
            ok = op_truncate(&m, l);
        }
        else if (choice < 90ul)
        {
            ok = op_discard(&m, l);
        }
        else if (choice < 95ul)
        {
            ledger89_close(l);
            ok = open_model(&fs, &io, &l) == LEDGER89_OK;
        }
        else if (choice < 98ul)
        {
            ledger89_close(l);
            ok = open_model(&fs, &io, &l) == LEDGER89_OK;
        }
        else
        {
            int orc;

            ledger89_close(l);
            mfs_crash(&fs);
            rm_power_crash(&m);
            orc = open_model(&fs, &io, &l);
            ok = orc == LEDGER89_OK;
        }
        CHECK(ok != 0);
        if ((op % 128ul) == 0ul)
        {
            rm_compare(&m, l);
        }
    }
    rm_compare(&m, l);
    ledger89_close(l);
    mfs_destroy(&fs);

    if (test_failures != 0)
    {
        fprintf(stderr, "seed=%lu op=%lu\n", diag_seed, diag_op);
    }

    TEST_END;
}
