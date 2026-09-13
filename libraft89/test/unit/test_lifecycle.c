/* test_lifecycle.c - create, restore, status, and teardown.
 * Covers scenarios C23-C30. */
#include "test.h"

#include "fixture.h"

static void expect_fresh_follower(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    const raft89_action *action;

    fixture_init(&f, 3u, 2u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.self, 2u);
    CHECK_EQ(status.role, RAFT89_FOLLOWER);
    CHECK_EQ(status.leader_id, RAFT89_ID_NONE);
    CHECK_EQ(status.current_term, RAFT89_TERM_NONE);
    CHECK_EQ(status.voted_for, RAFT89_ID_NONE);
    CHECK_EQ(status.commit_index, RAFT89_INDEX_NONE);
    CHECK_EQ(status.applied_index, RAFT89_INDEX_NONE);
    CHECK_EQ(status.faulted, 0);

    /* C23: create emits no action. */
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
    CHECK(action == NULL);
    raft89_destroy(node);
}

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    unsigned long i;

    /* C24-C25: initial role and leader. */
    expect_fresh_follower();

    /* C26-C27: restored term and vote. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 12u, 3u);
    fake_random_push(&f.random, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node != NULL)
    {
        CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
        CHECK_EQ(status.current_term, 12u);
        CHECK_EQ(status.voted_for, 3u);
        CHECK_EQ(status.commit_index, RAFT89_INDEX_NONE);
        CHECK_EQ(status.applied_index, RAFT89_INDEX_NONE);
        raft89_destroy(node);
    }

    /* Restored log position. */
    fixture_init(&f, 3u, 1u);
    CHECK_EQ(fake_store_append(&f.store, 2u, 1u, NULL, 0u), RAFT89_OK);
    CHECK_EQ(fake_store_append(&f.store, 2u, 2u, NULL, 0u), RAFT89_OK);
    CHECK_EQ(fake_store_append(&f.store, 5u, 3u, NULL, 0u), RAFT89_OK);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node != NULL)
    {
        CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
        CHECK_EQ(status.last_log_index, 3u);
        CHECK_EQ(status.commit_index, RAFT89_INDEX_NONE);
        CHECK_EQ(status.applied_index, RAFT89_INDEX_NONE);
        raft89_destroy(node);
    }

    /* C28: repeated create/destroy cycles. */
    for (i = 0u; i < 200u; ++i)
    {
        fixture_init(&f, 5u, 3u);
        node = NULL;
        CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
        CHECK(node != NULL);
        raft89_destroy(node);
    }

    /* Destroy NULL is a no-op. */
    raft89_destroy(NULL);

    /* Null arguments for status and action queries. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_status_get(NULL, &status), RAFT89_ERR_ARG);
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node != NULL)
    {
        CHECK_EQ(raft89_status_get(node, NULL), RAFT89_ERR_ARG);
        raft89_destroy(node);
    }

    TEST_END;
}
