/* test_stress_huge.c - ST02: a 1 MiB binary payload and a large batch in one
 * segment; boundary bytes and tags stay exact. */

#include <stdlib.h>
#include <string.h>

#include "test.h"

#include "fixture.h"

#define BIG_BYTES 1048576u
#define BATCH_COUNT 4096u
#define CHUNK 64u

int main(void)
{
    fx f;
    unsigned char *big;
    ledger89_record batch[CHUNK];
    ledger89_view v;
    ledger89_index base;
    size_t i;
    size_t k;
    size_t n;

    big = (unsigned char *)malloc(BIG_BYTES);
    CHECK(big != NULL);
    if (big == NULL)
    {
        TEST_END;
    }
    for (i = 0u; i < (size_t)BIG_BYTES; ++i)
    {
        big[i] = (unsigned char)(i * 31u + 7u);
    }

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    CHECK_EQ(fx_append(&f, 1ul, 77ul, big, (size_t)BIG_BYTES), LEDGER89_OK);

    base = 2ul;
    k = 0u;
    while (k < (size_t)BATCH_COUNT)
    {
        n = 0u;
        while (n < (size_t)CHUNK && k < (size_t)BATCH_COUNT)
        {
            fx_record(&batch[n], base + (ledger89_index)k, (unsigned long)k,
                      big, (size_t)CHUNK);
            ++n;
            ++k;
        }
        CHECK_EQ(ledger89_append(f.l, batch, n), LEDGER89_OK);
    }
    CHECK_EQ(ledger89_sync(f.l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 1ul + (ledger89_index)BATCH_COUNT);

    CHECK_EQ(fx_read(&f, 1ul, &v), LEDGER89_OK);
    CHECK_EQ(v.size, (size_t)BIG_BYTES);
    CHECK_EQ(((const unsigned char *)v.data)[0], (unsigned char)7);
    CHECK_EQ(((const unsigned char *)v.data)[(size_t)BIG_BYTES - 1u],
             (unsigned char)(((size_t)BIG_BYTES - 1u) * 31u + 7u));
    CHECK_EQ(fx_read(&f, 1ul + (ledger89_index)BATCH_COUNT, &v), LEDGER89_OK);
    CHECK_EQ(v.tag, (unsigned long)((size_t)BATCH_COUNT - 1u));
    CHECK_EQ(v.size, (size_t)CHUNK);
    CHECK_EQ(memcmp(v.data, big, (size_t)CHUNK), 0);

    free(big);
    fx_close(&f);

    TEST_END;
}
