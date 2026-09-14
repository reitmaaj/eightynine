/* test_stress_huge.c - large payloads and large batches. */

#include <stdlib.h>
#include <string.h>

#include "fixture.h"
#include "test.h"

#define HUGE_BYTES 1048576u
#define BATCH_RECORDS 4096u

int main(void)
{
    fx f;
    unsigned char *huge;
    unsigned char *batch;
    ledger89_slice *slices;
    ledger89_index first;
    ledger89_state st;
    size_t size;
    unsigned long i;
    int rc;

    huge = (unsigned char *)malloc(HUGE_BYTES);
    batch = (unsigned char *)malloc(BATCH_RECORDS);
    slices = (ledger89_slice *)malloc(BATCH_RECORDS * sizeof(ledger89_slice));
    CHECK(huge != NULL && batch != NULL && slices != NULL);
    if (huge == NULL || batch == NULL || slices == NULL)
    {
        return 1;
    }
    for (i = 0ul; i < HUGE_BYTES; ++i)
    {
        huge[i] = (unsigned char)(i & 0xFFul);
    }
    for (i = 0ul; i < BATCH_RECORDS; ++i)
    {
        batch[i] = (unsigned char)(i & 0xFFul);
        slices[i].data = &batch[i];
        slices[i].size = 1u;
    }

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    {
        ledger89_slice s;

        s.data = huge;
        s.size = HUGE_BYTES;
        rc = ledger89_appendv(f.l, &s, 1u, &first);
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK_U64(first, test_u64(1));
    }
    rc = ledger89_appendv(f.l, slices, BATCH_RECORDS, &first);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(first, test_u64(2));
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);

    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.end, test_u64(BATCH_RECORDS + 2ul));

    /* Verify the huge payload through a read. */
    {
        unsigned char *out;

        out = (unsigned char *)malloc(HUGE_BYTES);
        CHECK(out != NULL);
        if (out != NULL)
        {
            rc = ledger89_read(f.l, test_u64(1), out, HUGE_BYTES, &size);
            CHECK_EQ(rc, LEDGER89_OK);
            CHECK_EQ(size, HUGE_BYTES);
            CHECK(memcmp(out, huge, HUGE_BYTES) == 0);
            free(out);
        }
    }
    /* Spot-check the large batch. */
    {
        unsigned char out[1];

        rc = ledger89_read(f.l, test_u64(2), out, sizeof out, &size);
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK_EQ(out[0], 0u);
        rc = ledger89_read(f.l, test_u64(BATCH_RECORDS + 1ul), out, sizeof out,
                           &size);
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK_EQ(out[0], (unsigned char)((BATCH_RECORDS - 1u) & 0xFFu));
    }
    fx_close(&f);

    free(slices);
    free(batch);
    free(huge);
    TEST_END;
}
