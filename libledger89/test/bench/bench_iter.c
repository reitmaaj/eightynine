/* bench_iter.c - sequential iteration scaling across batch sizes.
 *
 * Usage: bench_iter
 * Builds one batch per size, then times size-only and payload iteration.
 * Prints nanoseconds per record so linear scaling is visible.
 * Not part of `just test`. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ledger89.h>

#include "tmpdir.h"

#define BENCH_PAYLOAD 64u

static double now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

static int iterate(ledger89 *l, int with_payload, double *seconds)
{
    ledger89_iter it;
    ledger89_index idx;
    unsigned char buf[BENCH_PAYLOAD];
    size_t size;
    unsigned long seen;
    double t0;
    double t1;
    int rc;

    rc = ledger89_iter_init(&it, l, ledger89_u64_from_u32(1u));
    if (rc != LEDGER89_OK)
    {
        return 1;
    }
    seen = 0ul;
    t0 = now_seconds();
    for (;;)
    {
        if (with_payload != 0)
        {
            rc = ledger89_iter_next(&it, &idx, buf, sizeof buf, &size);
        }
        else
        {
            rc = ledger89_iter_next(&it, &idx, NULL, 0u, &size);
        }
        if (rc != LEDGER89_OK)
        {
            break;
        }
        ++seen;
    }
    t1 = now_seconds();
    if (rc != LEDGER89_DONE)
    {
        return 1;
    }
    *seconds = t1 - t0;
    return 0;
}

static int run_size(unsigned long records)
{
    char path[64];
    ledger89 *l;
    ledger89_slice *slices;
    unsigned char *payload;
    unsigned long i;
    double null_seconds;
    double payload_seconds;
    double null_ns;
    double payload_ns;
    int rc;

    if (tmpdir_create(path, sizeof path) != 0)
    {
        return 1;
    }
    slices = (ledger89_slice *)malloc((size_t)records * sizeof(ledger89_slice));
    payload = (unsigned char *)malloc((size_t)records * BENCH_PAYLOAD);
    if (slices == NULL || payload == NULL)
    {
        return 1;
    }
    for (i = 0ul; i < records; ++i)
    {
        memset(payload + (i * BENCH_PAYLOAD), (int)(i & 0xFFul), BENCH_PAYLOAD);
        slices[i].data = payload + (i * BENCH_PAYLOAD);
        slices[i].size = BENCH_PAYLOAD;
    }
    l = NULL;
    rc = ledger89_open(&l, path,
                       LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE |
                           LEDGER89_OPEN_EXCL);
    if (rc != LEDGER89_OK)
    {
        return 1;
    }
    rc = ledger89_appendv(l, slices, (size_t)records, NULL);
    if (rc != LEDGER89_OK)
    {
        return 1;
    }
    rc = ledger89_sync(l, NULL);
    if (rc != LEDGER89_OK)
    {
        return 1;
    }
    free(slices);
    free(payload);
    slices = NULL;
    payload = NULL;

    if (iterate(l, 0, &null_seconds) != 0)
    {
        return 1;
    }
    if (iterate(l, 1, &payload_seconds) != 0)
    {
        return 1;
    }
    null_ns = null_seconds * 1000000000.0 / (double)records;
    payload_ns = payload_seconds * 1000000000.0 / (double)records;
    printf("%8lu  %9.3f  %9.3f  %11.1f  %11.1f\n", records, null_seconds * 1e3,
           payload_seconds * 1e3, null_ns, payload_ns);
    ledger89_close(l);
    return 0;
}

int main(void)
{
    static const unsigned long sizes[3] = {1000ul, 4000ul, 16000ul};
    size_t i;

    printf("%8s  %9s  %9s  %11s  %11s\n", "records", "null_ms", "data_ms",
           "null_ns/rec", "data_ns/rec");
    for (i = 0u; i < 3u; ++i)
    {
        if (run_size(sizes[i]) != 0)
        {
            return 1;
        }
    }
    return 0;
}
