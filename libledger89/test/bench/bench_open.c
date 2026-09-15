/* bench_open.c - cold open and recovery cost as batches and parts grow.
 *
 * Usage: bench_open [batches] [records-per-batch] [parts]
 * Builds a ledger, closes it, then times a read-only and a writable open.
 * Not part of `just test`. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ledger89.h>

#include "tmpdir.h"

#define BENCH_PAYLOAD 64u
#define BENCH_SLICES 64ul

static double now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

static int build(const char *path, unsigned long batches,
                 unsigned long per_batch, unsigned long parts)
{
    ledger89 *l;
    ledger89_slice slices[BENCH_SLICES];
    unsigned char payload[BENCH_SLICES][BENCH_PAYLOAD];
    unsigned long i;
    unsigned long rotate_every;
    int rc;

    if (per_batch > BENCH_SLICES)
    {
        per_batch = BENCH_SLICES;
    }
    for (i = 0ul; i < BENCH_SLICES; ++i)
    {
        memset(payload[i], (int)(i & 0xFFul), BENCH_PAYLOAD);
        slices[i].data = payload[i];
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
    rotate_every = 0ul;
    if (parts > 1ul)
    {
        rotate_every = batches / parts;
        if (rotate_every == 0ul)
        {
            rotate_every = 1ul;
        }
    }
    for (i = 0ul; i < batches; ++i)
    {
        rc = ledger89_appendv(l, slices, (size_t)per_batch, NULL);
        if (rc != LEDGER89_OK)
        {
            return 1;
        }
        if (rotate_every != 0ul)
        {
            if ((i + 1ul) % rotate_every == 0ul)
            {
                rc = ledger89_rotate(l);
                if (rc != LEDGER89_OK)
                {
                    return 1;
                }
            }
        }
    }
    rc = ledger89_sync(l, NULL);
    if (rc != LEDGER89_OK)
    {
        return 1;
    }
    ledger89_close(l);
    return 0;
}

int main(int argc, char **argv)
{
    char path[64];
    unsigned long batches;
    unsigned long per_batch;
    unsigned long parts;
    ledger89 *l;
    double t0;
    double t1;
    int rc;

    batches = 1000ul;
    per_batch = 1ul;
    parts = 1ul;
    if (argc > 1)
    {
        batches = strtoul(argv[1], NULL, 0);
    }
    if (argc > 2)
    {
        per_batch = strtoul(argv[2], NULL, 0);
    }
    if (argc > 3)
    {
        parts = strtoul(argv[3], NULL, 0);
    }
    if (tmpdir_create(path, sizeof path) != 0)
    {
        return 1;
    }
    t0 = now_seconds();
    if (build(path, batches, per_batch, parts) != 0)
    {
        return 1;
    }
    t1 = now_seconds();
    printf("build: %lu batches x %lu records, %lu parts in %.3fs\n", batches,
           per_batch, parts, t1 - t0);

    l = NULL;
    t0 = now_seconds();
    rc = ledger89_open(&l, path, LEDGER89_OPEN_RDONLY);
    t1 = now_seconds();
    if (rc != LEDGER89_OK)
    {
        return 1;
    }
    printf("open-ro: %.3fs\n", t1 - t0);
    ledger89_close(l);

    l = NULL;
    t0 = now_seconds();
    rc = ledger89_open(&l, path, LEDGER89_OPEN_RDWR);
    t1 = now_seconds();
    if (rc != LEDGER89_OK)
    {
        return 1;
    }
    printf("open-rw: %.3fs\n", t1 - t0);
    ledger89_close(l);
    return 0;
}
