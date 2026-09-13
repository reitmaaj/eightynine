/* bench_main.c - reproducible throughput harness for append, sync, read, and
 * iteration. Run via `just bench`; deliberately not part of `just test`.
 *
 * v1 random reads restart the segment walker, so read cost is O(records in
 * the segment). The harness therefore bounds segment records to keep the read
 * phase linear in practice. */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "fixture.h"

#define OPS 50000ul
#define READ_OPS 5000ul
#define PAYLOAD 64u
#define BATCH 32u
#define SEG_RECORDS 256ul

static double now_seconds(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static void report(const char *what, size_t n, double secs)
{
    double rate;

    rate = secs > 0.0 ? (double)n / secs : 0.0;
    printf("%-9s %lu ops in %.4fs -> %.0f ops/s\n", what, (unsigned long)n,
           secs, rate);
}

static int append_all(fx *f, unsigned char *payload)
{
    ledger89_record batch[BATCH];
    ledger89_index next;
    size_t n;

    next = 1ul;
    while (next <= (ledger89_index)OPS)
    {
        n = 0u;
        while (n < (size_t)BATCH && next <= (ledger89_index)OPS)
        {
            fx_record(&batch[n], next, (unsigned long)next, payload,
                      (size_t)PAYLOAD);
            ++n;
            ++next;
        }
        if (ledger89_append(f->l, batch, n) != LEDGER89_OK)
        {
            return -1;
        }
    }
    return 0;
}

static int read_sample(fx *f)
{
    ledger89_view v;
    ledger89_index index;
    size_t stride;
    size_t i;

    stride = (size_t)OPS / (size_t)READ_OPS;
    for (i = 0u; i < (size_t)READ_OPS; ++i)
    {
        index = (ledger89_index)(i * stride + 1u);
        if (ledger89_read(f->l, index, &v) != LEDGER89_OK)
        {
            return -1;
        }
    }
    return 0;
}

static int iterate_all(fx *f, size_t *count)
{
    ledger89_iter *it;
    ledger89_view v;
    int rc;

    it = NULL;
    if (ledger89_iter_open(f->l, 0ul, 0ul, &it) != LEDGER89_OK)
    {
        return -1;
    }
    *count = 0u;
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
            return -1;
        }
        ++*count;
    }
    ledger89_iter_close(it);
    return 0;
}

int main(void)
{
    fx f;
    unsigned char payload[PAYLOAD];
    size_t count;
    double t0;
    double t1;

    memset(payload, 0xAB, sizeof payload);
    if (fx_open_limits(&f, 0ul, SEG_RECORDS) != LEDGER89_OK)
    {
        fprintf(stderr, "bench: open failed\n");
        return 1;
    }

    t0 = now_seconds();
    if (append_all(&f, payload) != 0)
    {
        fprintf(stderr, "bench: append failed\n");
        return 1;
    }
    t1 = now_seconds();
    report("append", (size_t)OPS, t1 - t0);

    t0 = now_seconds();
    if (ledger89_sync(f.l) != LEDGER89_OK)
    {
        fprintf(stderr, "bench: sync failed\n");
        return 1;
    }
    t1 = now_seconds();
    printf("%-9s %lu records flushed in %.4fs\n", "sync", OPS, t1 - t0);

    t0 = now_seconds();
    if (read_sample(&f) != 0)
    {
        fprintf(stderr, "bench: read failed\n");
        return 1;
    }
    t1 = now_seconds();
    report("read", (size_t)READ_OPS, t1 - t0);

    t0 = now_seconds();
    if (iterate_all(&f, &count) != 0)
    {
        fprintf(stderr, "bench: iteration failed\n");
        return 1;
    }
    t1 = now_seconds();
    report("iterate", count, t1 - t0);

    fx_close(&f);
    return 0;
}
