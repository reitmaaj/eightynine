/* test_applied_checkpoint.c - recovered application checkpoint. Covers
 * scenarios AP01-AP09 in .agent/testing/0012-applied-checkpoint.md. */
#include "test.h"

#include "driver.h"
#include "fixture.h"

static void seed(fixture *f, unsigned long count)
{
    unsigned long i;

    for (i = 1ul; i <= count; ++i)
    {
        CHECK_EQ(fake_store_put(&f->store, (i + 1ul) / 2ul, i, NULL, 0u),
                 RAFT89_OK);
    }
}

static void expect_no_action(raft89 *node)
{
    const raft89_action *action;

    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
    CHECK(action == NULL);
}

static void expect_apply(raft89 *node, unsigned long index)
{
    const raft89_action *action;

    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    if (action == NULL)
    {
        return;
    }
    CHECK_EQ(action->type, RAFT89_ACT_APPLY);
    CHECK_U64(action->u.apply.entry.index, test_u64(index));
    CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK), RAFT89_OK);
}

/* Deliver one AppendEntries that appends and commits `index`. */
static void commit_one(raft89 *node, unsigned long index)
{
    raft89_message msg;
    raft89_entry entry;

    entry.index = test_u64(index);
    entry.term = test_u64((index + 1ul) / 2ul);
    entry.data = NULL;
    entry.size = 0u;
    driver_build_ae(2u, 1u, 1u, index - 1ul, index / 2ul, index, &entry, 1u,
                    &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 1u, RAFT89_ID_NONE), 1);
    expect_apply(node, index);
    driver_drain(node);
}

static void test_zero(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;

    fixture_init(&f, 3u, 1u);
    seed(&f, 5ul);
    f.config.applied_index = raft89_u64_zero();
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, RAFT89_INDEX_NONE);
    CHECK_U64(status.commit_index, RAFT89_INDEX_NONE);
    CHECK_U64(status.last_log_index, test_u64(5u));
    expect_no_action(node);
    raft89_destroy(node);
}

static void test_valid_checkpoint(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;

    fixture_init(&f, 3u, 1u);
    seed(&f, 5ul);
    f.config.applied_index = test_u64(3u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, test_u64(3u));
    CHECK_U64(status.commit_index, test_u64(3u));
    CHECK_U64(status.last_log_index, test_u64(5u));
    expect_no_action(node);
    raft89_destroy(node);

    /* At the log end. */
    f.config.applied_index = test_u64(5u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, test_u64(5u));
    expect_no_action(node);
    raft89_destroy(node);
}

static void test_bad_checkpoint(void)
{
    fixture f;
    raft89 *node;

    fixture_init(&f, 3u, 1u);
    seed(&f, 5ul);
    f.config.applied_index = test_u64(6u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_ERR_CORRUPT);
    CHECK(node == NULL);

    f.config.applied_index = test_u64(3u);
    f.store.fail_log_term = 1;
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_ERR_STORE);
    CHECK(node == NULL);
}

static void test_no_replay_below_floor(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;

    fixture_init(&f, 3u, 1u);
    seed(&f, 5ul);
    f.config.applied_index = test_u64(3u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }

    /* A heartbeat that commits through 5 but carries no new entries cannot
     * advance match beyond the checkpoint; append 4 and 5 to commit. */
    {
        raft89_message msg;
        raft89_entry entries[2];

        entries[0].index = test_u64(4u);
        entries[0].term = test_u64(2u);
        entries[0].data = NULL;
        entries[0].size = 0u;
        entries[1].index = test_u64(5u);
        entries[1].term = test_u64(3u);
        entries[1].data = NULL;
        entries[1].size = 0u;
        driver_build_ae(2u, 1u, 1u, 3ul, 2u, 5u, entries, 2u, &msg);
        CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
        CHECK_EQ(driver_expect_hard_state(node, 1u, RAFT89_ID_NONE), 1);
        expect_apply(node, 4u);
        expect_apply(node, 5u);
        driver_drain(node);
    }
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, test_u64(5u));
    CHECK_U64(status.commit_index, test_u64(5u));
    raft89_destroy(node);
}

static void test_restart_repetition(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    unsigned long c;

    fixture_init(&f, 3u, 1u);
    seed(&f, 4ul);
    for (c = 1ul; c < 4ul; ++c)
    {
        f.config.applied_index = test_u64(c);
        node = NULL;
        CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
        CHECK(node != NULL);
        if (node == NULL)
        {
            return;
        }
        CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
        CHECK_U64(status.applied_index, test_u64(c));
        CHECK_U64(status.commit_index, test_u64(c));
        commit_one(node, c + 1ul);
        CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
        CHECK_U64(status.applied_index, test_u64(c + 1ul));
        raft89_destroy(node);
    }
}

static void test_crash_before_checkpoint(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;

    fixture_init(&f, 3u, 1u);
    seed(&f, 4ul);
    f.config.applied_index = test_u64(3u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    commit_one(node, 4u);
    raft89_destroy(node);

    /* The checkpoint was not persisted: APPLY 4 may repeat. */
    f.config.applied_index = test_u64(3u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, test_u64(3u));
    commit_one(node, 4u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, test_u64(4u));
    raft89_destroy(node);
}

static void test_role_transitions(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    raft89_message msg;

    fixture_init(&f, 3u, 1u);
    seed(&f, 5ul);
    f.config.applied_index = test_u64(3u);
    fake_random_push(&f.random, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }

    /* Election and drain must not lower the floor. */
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    driver_drain(node);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK(raft89_u64_cmp(status.applied_index, test_u64(3u)) >= 0);
    CHECK(raft89_u64_cmp(status.commit_index, test_u64(3u)) >= 0);

    /* A higher-term heartbeat steps the node down without lowering it. */
    driver_build_ae(2u, 1u, 9u, 3ul, 2u, 3u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    driver_drain(node);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, test_u64(3u));
    CHECK_U64(status.commit_index, test_u64(3u));
    CHECK_EQ(status.role, RAFT89_FOLLOWER);
    raft89_destroy(node);
}

int main(void)
{
    test_zero();
    test_valid_checkpoint();
    test_bad_checkpoint();
    test_no_replay_below_floor();
    test_restart_repetition();
    test_crash_before_checkpoint();
    test_role_transitions();
    TEST_END;
}
