/* smoke.c - one end-to-end path through the public API. */

#include "test.h"
#include "tmpdir.h"

int main(void)
{
    char path[64];
    ledger89 *l;
    ledger89_slice slices[3];
    ledger89_index first;
    ledger89_state st;
    unsigned char buf[8];
    size_t size;
    int rc;

    if (tmpdir_create(path, sizeof path) != 0)
    {
        fprintf(stderr, "tmpdir failed\n");
        return 1;
    }
    l = NULL;
    rc = ledger89_open(&l, path,
                       LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE |
                           LEDGER89_OPEN_EXCL);
    CHECK_EQ(rc, LEDGER89_OK);
    if (rc != LEDGER89_OK)
    {
        return 1;
    }
    slices[0].data = "one";
    slices[0].size = 3u;
    slices[1].data = "two";
    slices[1].size = 3u;
    slices[2].data = NULL;
    slices[2].size = 0u;
    rc = ledger89_appendv(l, slices, 3u, &first);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(first, test_u64(1));
    rc = ledger89_sync(l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.stable_end, test_u64(4));
    CHECK_U64(st.end, test_u64(4));
    CHECK_EQ(st.revision.hi, 0u);
    CHECK_EQ(st.revision.lo, 0u);
    ledger89_close(l);
    l = NULL;
    rc = ledger89_open(&l, path, LEDGER89_OPEN_RDWR);
    CHECK_EQ(rc, LEDGER89_OK);
    if (rc != LEDGER89_OK)
    {
        return 1;
    }
    rc = ledger89_read(l, test_u64(2), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 3u);
    CHECK(memcmp(buf, "two", 3u) == 0);
    rc = ledger89_read(l, test_u64(3), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 0u);
    ledger89_close(l);
    TEST_END
}
