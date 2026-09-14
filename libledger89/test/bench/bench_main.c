/* bench_main.c - reproducible throughput baseline (not part of `just test`). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ledger89.h>

#include "tmpdir.h"

#define BENCH_BATCHES 1562ul
#define BENCH_BATCH 32ul
#define BENCH_TOTAL (BENCH_BATCHES * BENCH_BATCH)
#define BENCH_PAYLOAD 64u

static double now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

int main(void)
{
    char path[64];
    ledger89 *l;
    ledger89_slice slices[BENCH_BATCH];
    unsigned char payload[BENCH_BATCH][BENCH_PAYLOAD];
    unsigned char out[BENCH_PAYLOAD];
    unsigned long i;
    size_t size;
    double t0;
    double t1;

    if (tmpdir_create(path, sizeof path) != 0)
    {
        return 1;
    }
    for (i = 0ul; i < BENCH_BATCH; ++i)
    {
        memset(payload[i], (int)(i & 0xFFul), BENCH_PAYLOAD);
        slices[i].data = payload[i];
        slices[i].size = BENCH_PAYLOAD;
    }
    l = NULL;
    if (ledger89_open(&l, path,
                      LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE |
                          LEDGER89_OPEN_EXCL) != LEDGER89_OK)
    {
        return 1;
    }

    t0 = now_seconds();
    for (i = 0ul; i < BENCH_BATCHES; ++i)
    {
        if (ledger89_appendv(l, slices, BENCH_BATCH, NULL) != LEDGER89_OK)
        {
            return 1;
        }
    }
    t1 = now_seconds();
    printf("append: %lu records in %.3fs (%.0f rec/s)\n", BENCH_TOTAL, t1 - t0,
           (double)BENCH_TOTAL / (t1 - t0));

    t0 = now_seconds();
    if (ledger89_sync(l, NULL) != LEDGER89_OK)
    {
        return 1;
    }
    t1 = now_seconds();
    printf("sync: %.3fs\n", t1 - t0);

    t0 = now_seconds();
    for (i = 1ul; i <= BENCH_TOTAL; ++i)
    {
        if (ledger89_read(l, ledger89_u64_from_u32((ledger89_u32)i), out,
                          sizeof out, &size) != LEDGER89_OK)
        {
            return 1;
        }
    }
    t1 = now_seconds();
    printf("read: %lu records in %.3fs (%.0f rec/s)\n", BENCH_TOTAL, t1 - t0,
           (double)BENCH_TOTAL / (t1 - t0));

    t0 = now_seconds();
    {
        ledger89_iter it;
        ledger89_index idx;
        unsigned long seen;

        seen = 0ul;
        if (ledger89_iter_init(&it, l, ledger89_u64_from_u32(1u)) !=
            LEDGER89_OK)
        {
            return 1;
        }
        while (ledger89_iter_next(&it, &idx, NULL, 0u, &size) == LEDGER89_OK)
        {
            ++seen;
        }
        if (seen != BENCH_TOTAL)
        {
            return 1;
        }
    }
    t1 = now_seconds();
    printf("iterate: %lu records in %.3fs (%.0f rec/s)\n", BENCH_TOTAL, t1 - t0,
           (double)BENCH_TOTAL / (t1 - t0));

    ledger89_close(l);
    return 0;
}
