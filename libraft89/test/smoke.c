/* smoke.c - end-to-end lifecycle smoke over libraft89: create a node
 * over an in-memory store, inspect restored status, confirm no action is
 * emitted, and destroy it. Exercises config -> store read -> restore ->
 * status -> action query -> teardown. */
#include "test.h"

#include "fixture.h"

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    const raft89_action *action;

    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 7u, 0u);
    fake_random_push(&f.random, 0u);

    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }

    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.self, 1u);
    CHECK_EQ(status.role, RAFT89_FOLLOWER);
    CHECK_U64(status.current_term, test_u64(7u));
    CHECK_EQ(status.voted_for, RAFT89_ID_NONE);
    CHECK_EQ(status.leader_id, RAFT89_ID_NONE);
    CHECK_U64(status.last_log_index, RAFT89_INDEX_NONE);
    CHECK_U64(status.commit_index, RAFT89_INDEX_NONE);
    CHECK_U64(status.applied_index, RAFT89_INDEX_NONE);
    CHECK_EQ(status.faulted, 0);

    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
    CHECK(action == NULL);

    raft89_destroy(node);
    raft89_destroy(NULL);

    TEST_END;
}
