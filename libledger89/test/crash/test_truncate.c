/* test_truncate.c - truncation and discard over the model filesystem:
 * post-state durability and failure recovery. */

#include <string.h>

#include "test.h"

#include "model_fs.h"

static int open_model(mfs *fs, led89_io *io, ledger89 **l,
                      unsigned long records)
{
    ledger89_config cfg;

    mfs_bind(io, fs);
    memset(&cfg, 0, sizeof cfg);
    cfg.path = "ledger";
    cfg.max_segment_bytes = 0ul;
    cfg.max_segment_records = records;
    *l = NULL;
    return led89_open_io(l, &cfg, io);
}

static int append_byte(ledger89 *l, ledger89_index index, unsigned char value)
{
    ledger89_record r;

    r.index = index;
    r.tag = (unsigned long)index;
    r.data = &value;
    r.size = 1u;
    return ledger89_append(l, &r, 1u);
}

static int fill(ledger89 *l, ledger89_index count)
{
    ledger89_index i;

    for (i = 1ul; i <= count; ++i)
    {
        if (append_byte(l, i, (unsigned char)('A' + i)) != LEDGER89_OK)
        {
            return 0;
        }
    }
    return 1;
}

static int read_ok(ledger89 *l, ledger89_index index)
{
    ledger89_view v;

    return ledger89_read(l, index, &v);
}

int main(void)
{
    mfs fs;
    led89_io io;
    ledger89 *l;

    /* Truncate inside a sealed boundary, then power crash. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK(fill(l, 6ul) != 0);
    CHECK_EQ(ledger89_truncate_after(l, 3ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 3ul);
    ledger89_close(l);
    mfs_crash(&fs);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 3ul);
    CHECK_EQ(read_ok(l, 1ul), LEDGER89_OK);
    CHECK_EQ(read_ok(l, 3ul), LEDGER89_OK);
    CHECK_EQ(read_ok(l, 4ul), LEDGER89_ERR_NOTFOUND);
    ledger89_close(l);
    mfs_destroy(&fs);

    /* A failed truncation faults the handle; reopen recovers the pre-state. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK(fill(l, 4ul) != 0);
    fs.fail_unlink = 1;
    CHECK_EQ(ledger89_truncate_after(l, 1ul), LEDGER89_ERR_IO);
    CHECK_EQ(ledger89_truncate_after(l, 1ul), LEDGER89_ERR_FAULTED);
    ledger89_close(l);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 4ul);
    CHECK_EQ(read_ok(l, 4ul), LEDGER89_OK);
    ledger89_close(l);
    mfs_destroy(&fs);

    /* Truncate all: base preserved across power loss. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK(fill(l, 6ul) != 0);
    CHECK_EQ(ledger89_truncate_after(l, 0ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(l), 1ul);
    CHECK_EQ(ledger89_last_index(l), 0ul);
    ledger89_close(l);
    mfs_crash(&fs);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(l), 1ul);
    CHECK_EQ(ledger89_last_index(l), 0ul);
    CHECK_EQ(append_byte(l, 1ul, (unsigned char)'z'), LEDGER89_OK);
    ledger89_close(l);
    mfs_destroy(&fs);

    /* Discard a prefix, then power crash. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK(fill(l, 6ul) != 0);
    CHECK_EQ(ledger89_discard_before(l, 3ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(l), 3ul);
    ledger89_close(l);
    mfs_crash(&fs);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(l), 3ul);
    CHECK_EQ(ledger89_last_index(l), 6ul);
    CHECK_EQ(read_ok(l, 2ul), LEDGER89_ERR_NOTFOUND);
    CHECK_EQ(read_ok(l, 3ul), LEDGER89_OK);
    ledger89_close(l);
    mfs_destroy(&fs);

    /* Discard all: the new base survives power loss. */
    mfs_init(&fs);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK(fill(l, 6ul) != 0);
    CHECK_EQ(ledger89_discard_before(l, 99ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(l), 7ul);
    CHECK_EQ(ledger89_last_index(l), 6ul);
    ledger89_close(l);
    mfs_crash(&fs);
    CHECK_EQ(open_model(&fs, &io, &l, 2ul), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(l), 7ul);
    CHECK_EQ(ledger89_last_index(l), 6ul);
    CHECK_EQ(append_byte(l, 7ul, (unsigned char)'q'), LEDGER89_OK);
    CHECK_EQ(read_ok(l, 7ul), LEDGER89_OK);
    ledger89_close(l);
    mfs_destroy(&fs);

    TEST_END;
}
