/* test_random_model.c - randomized reference-model comparison. */

#include <stdlib.h>
#include <string.h>

#include "crash_util.h"
#include "test.h"

#define RM_MAX_RECORDS 400ul
#define RM_MAX_PARTS 64ul

typedef struct model
{
    unsigned char data[RM_MAX_RECORDS][4];
    unsigned long size[RM_MAX_RECORDS];
    unsigned long first;
    unsigned long stable_end;
    unsigned long end;
    unsigned long revision;
    unsigned long p_first[RM_MAX_PARTS];
    unsigned long p_end[RM_MAX_PARTS];
    unsigned long p_count;
    unsigned long active_first;
} model;

static unsigned long rng_state;

static unsigned long rng_next(unsigned long bound)
{
    rng_state = (rng_state * 1103515245ul) + 12345ul;
    return (rng_state >> 16) % bound;
}

static void model_init(model *m)
{
    memset(m, 0, sizeof *m);
    m->first = 1ul;
    m->stable_end = 1ul;
    m->end = 1ul;
    m->active_first = 1ul;
}

static void model_sync(model *m)
{
    m->stable_end = m->end;
}

static void model_append(model *m, unsigned long count)
{
    unsigned long i;

    for (i = 0ul; i < count; ++i)
    {
        unsigned long idx;
        unsigned long k;

        idx = m->end - 1ul;
        m->size[idx] = rng_next(5ul);
        for (k = 0ul; k < m->size[idx]; ++k)
        {
            m->data[idx][k] = (unsigned char)((idx * 7ul) + k);
        }
        ++m->end;
    }
}

static void model_rotate(model *m)
{
    if (m->end == m->active_first)
    {
        return;
    }
    m->stable_end = m->end;
    if (m->p_count < RM_MAX_PARTS)
    {
        m->p_first[m->p_count] = m->active_first;
        m->p_end[m->p_count] = m->end;
        ++m->p_count;
    }
    m->active_first = m->end;
}

static void model_truncate(model *m, unsigned long from)
{
    unsigned long keep;
    unsigned long boundary_first;
    unsigned long i;

    if (from == m->end)
    {
        return;
    }
    keep = 0ul;
    while (keep < m->p_count && m->p_end[keep] <= from)
    {
        ++keep;
    }
    boundary_first = (keep < m->p_count) ? m->p_first[keep] : m->active_first;
    if (from > boundary_first && m->p_count < RM_MAX_PARTS)
    {
        m->p_first[keep] = boundary_first;
        m->p_end[keep] = from;
        keep += 1ul;
    }
    for (i = keep; i < m->p_count; ++i)
    {
        m->p_first[i] = 0ul;
        m->p_end[i] = 0ul;
    }
    m->p_count = keep;
    m->active_first = from;
    m->end = from;
    m->stable_end = from;
    ++m->revision;
}

static unsigned long model_prune(model *m, unsigned long requested)
{
    unsigned long drop;
    unsigned long i;

    if (requested <= m->first)
    {
        return m->first;
    }
    drop = 0ul;
    while (drop < m->p_count && m->p_end[drop] <= requested)
    {
        ++drop;
    }
    if (drop == 0ul)
    {
        return m->first;
    }
    m->first = (drop < m->p_count) ? m->p_first[drop] : m->active_first;
    for (i = 0ul; i + drop < m->p_count; ++i)
    {
        m->p_first[i] = m->p_first[i + drop];
        m->p_end[i] = m->p_end[i + drop];
    }
    m->p_count -= drop;
    return m->first;
}

static void model_crash(model *m)
{
    m->end = m->stable_end;
}

static unsigned char *model_record(model *m, unsigned long index)
{
    return m->data[index - 1ul];
}

static void compare(ledger89 *l, model *m)
{
    ledger89_state st;
    unsigned long i;

    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.first, test_u64(m->first));
    CHECK_U64(st.end, test_u64(m->end));
    CHECK_U64(st.stable_end, test_u64(m->stable_end));
    CHECK_EQ(st.revision.lo, (ledger89_u32)m->revision);
    for (i = m->first; i < m->end; ++i)
    {
        unsigned char buf[4];
        size_t size;

        if (ledger89_read(l, test_u64(i), buf, sizeof buf, &size) !=
            LEDGER89_OK)
        {
            CHECK(0);
            return;
        }
        if (size != (size_t)m->size[i - 1ul] ||
            memcmp(buf, model_record(m, i), size) != 0)
        {
            CHECK(0);
            return;
        }
    }
    /* Iteration must visit exactly [first,end). */
    {
        ledger89_iter it;
        unsigned long seen;
        ledger89_index idx;
        size_t size;

        seen = 0ul;
        if (ledger89_iter_init(&it, l, test_u64(m->first)) == LEDGER89_OK)
        {
            for (;;)
            {
                if (ledger89_iter_next(&it, &idx, NULL, 0u, &size) !=
                    LEDGER89_OK)
                {
                    break;
                }
                ++seen;
            }
        }
        CHECK_EQ(seen, m->end - m->first);
    }
}

static void run(unsigned long seed, unsigned long ops)
{
    mfs fs;
    led89_io io;
    ledger89 *l;
    model m;
    unsigned long i;

    rng_state = seed;
    mfs_init(&fs);
    CHECK_EQ(cu_open(&fs, &io, &l), LEDGER89_OK);
    model_init(&m);
    compare(l, &m);

    for (i = 0ul; i < ops; ++i)
    {
        unsigned long op;

        op = rng_next(10ul);
        if (op < 4ul && m.end + 4ul <= RM_MAX_RECORDS)
        {
            ledger89_slice s[4];
            unsigned long count;
            unsigned long k;

            count = 1ul + rng_next(4ul);
            model_append(&m, count);
            for (k = 0ul; k < count; ++k)
            {
                unsigned long idx;

                idx = m.end - count + k - 1ul;
                s[k].data = (m.size[idx] > 0ul) ? m.data[idx] : NULL;
                s[k].size = (size_t)m.size[idx];
            }
            CHECK_EQ(ledger89_appendv(l, s, (size_t)count, NULL), LEDGER89_OK);
        }
        else if (op < 6ul)
        {
            model_sync(&m);
            CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
        }
        else if (op < 7ul && m.end > m.active_first)
        {
            model_rotate(&m);
            CHECK_EQ(ledger89_rotate(l), LEDGER89_OK);
        }
        else if (op < 8ul)
        {
            unsigned long from;

            from = m.first + rng_next((m.end - m.first) + 1ul);
            model_sync(&m);
            CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
            model_truncate(&m, from);
            CHECK_EQ(ledger89_truncate_from(l, test_u64(from)), LEDGER89_OK);
        }
        else if (op < 9ul)
        {
            unsigned long requested;
            ledger89_index actual;

            requested = m.first + rng_next((m.stable_end - m.first) + 1ul);
            CHECK_EQ(ledger89_prune_before(l, test_u64(requested), &actual),
                     LEDGER89_OK);
            CHECK_U64(actual, test_u64(model_prune(&m, requested)));
        }
        else
        {
            ledger89_state st;

            mfs_crash(&fs);
            model_crash(&m);
            ledger89_close(l);
            if (cu_reopen(&fs, &io, &l, &st) == 0)
            {
                CHECK(0);
                return;
            }
        }
        compare(l, &m);
    }
    ledger89_close(l);
    mfs_destroy(&fs);
}

int main(int argc, char **argv)
{
    unsigned long seed;
    unsigned long ops;

    seed = 12345ul;
    ops = 400ul;
    if (argc > 1)
    {
        seed = (unsigned long)strtoul(argv[1], NULL, 10);
    }
    if (argc > 2)
    {
        ops = (unsigned long)strtoul(argv[2], NULL, 10);
    }
    run(seed, ops);
    TEST_END;
}
