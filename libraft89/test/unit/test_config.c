/* test_config.c - configuration validation and restored-state checks.
 * Covers scenarios C01-C22 and C29-C30. */
#include "test.h"

#include "fixture.h"

static void expect_create(fixture *f, int expected)
{
    raft89 *node;
    node = NULL;
    CHECK_EQ(raft89_create(&f->config, &node), expected);
    CHECK(node == NULL);
}

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_size n;

    /* C01-C05: valid clusters of size one through five. */
    for (n = 1u; n <= 5u; ++n)
    {
        fixture_init(&f, n, 1u);
        node = NULL;
        CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
        CHECK(node != NULL);
        raft89_destroy(node);
    }

    /* C06: null configuration. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(NULL, &node), RAFT89_ERR_ARG);
    CHECK(node == NULL);

    /* C07: null output pointer. */
    CHECK_EQ(raft89_create(&f.config, NULL), RAFT89_ERR_ARG);

    /* C08: zero self id. */
    fixture_init(&f, 3u, 1u);
    f.config.self = RAFT89_ID_NONE;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C09: zero member id. */
    fixture_init(&f, 3u, 1u);
    f.members[1] = RAFT89_ID_NONE;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C10: duplicate member. */
    fixture_init(&f, 3u, 1u);
    f.members[1] = 1u;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C11: self absent. */
    fixture_init(&f, 3u, 1u);
    f.config.self = 9u;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C12: self twice. */
    fixture_init(&f, 3u, 1u);
    f.members[1] = 1u;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C13: zero members. */
    fixture_init(&f, 3u, 1u);
    f.config.member_count = 0u;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C14: missing store callbacks. */
    fixture_init(&f, 3u, 1u);
    f.config.store.hard_state = NULL;
    expect_create(&f, RAFT89_ERR_ARG);
    fixture_init(&f, 3u, 1u);
    f.config.store.log_last = NULL;
    expect_create(&f, RAFT89_ERR_ARG);
    fixture_init(&f, 3u, 1u);
    f.config.store.log_term = NULL;
    expect_create(&f, RAFT89_ERR_ARG);
    fixture_init(&f, 3u, 1u);
    f.config.store.log_size = NULL;
    expect_create(&f, RAFT89_ERR_ARG);
    fixture_init(&f, 3u, 1u);
    f.config.store.log_read = NULL;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C15: missing random source. */
    fixture_init(&f, 3u, 1u);
    f.config.random.next = NULL;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C16: zero heartbeat. */
    fixture_init(&f, 3u, 1u);
    f.config.heartbeat_interval = 0u;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C17: election minimum not above heartbeat. */
    fixture_init(&f, 3u, 1u);
    f.config.election_timeout_min = f.config.heartbeat_interval;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C18: election maximum below minimum. */
    fixture_init(&f, 3u, 1u);
    f.config.election_timeout_max = f.config.election_timeout_min - 1u;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C13a: zero append limits. */
    fixture_init(&f, 3u, 1u);
    f.config.max_append_entries = 0u;
    expect_create(&f, RAFT89_ERR_ARG);
    fixture_init(&f, 3u, 1u);
    f.config.max_append_bytes = 0u;
    expect_create(&f, RAFT89_ERR_ARG);

    /* C19: persisted vote for a non-member. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 4u, 9u);
    expect_create(&f, RAFT89_ERR_CORRUPT);

    /* C20: empty log (0,0) is accepted. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    raft89_destroy(node);

    /* C21: empty log with a non-zero term is corrupt. */
    fixture_init(&f, 3u, 1u);
    f.store.override_last = 1;
    f.store.last_index_override = RAFT89_INDEX_NONE;
    f.store.last_term_override = test_u64(5u);
    expect_create(&f, RAFT89_ERR_CORRUPT);

    /* C22: nonempty log with term zero is corrupt. */
    fixture_init(&f, 3u, 1u);
    CHECK_EQ(fake_store_append(&f.store, 0ul, 1u, NULL, 0u), RAFT89_OK);
    expect_create(&f, RAFT89_ERR_CORRUPT);

    /* C29: store read failures. */
    fixture_init(&f, 3u, 1u);
    f.store.fail_hard_state = 1;
    expect_create(&f, RAFT89_ERR_STORE);
    fixture_init(&f, 3u, 1u);
    f.store.fail_log_last = 1;
    expect_create(&f, RAFT89_ERR_STORE);

    TEST_END;
}
