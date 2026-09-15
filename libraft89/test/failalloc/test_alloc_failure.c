/* test_alloc_failure.c - allocation-failure injection across create,
 * propose, and receive. Linked with --wrap; see
 * .agent/testing/0009-allocation.md. */
#include "test.h"

#include "driver.h"
#include "fail_alloc.h"
#include "fixture.h"

static void expect_no_live(void)
{
    CHECK_EQ(fail_alloc_live(), 0ul);
}

static void run_create_failures(void)
{
    unsigned long i;
    int saw_success;
    saw_success = 0;
    for (i = 0ul; i < 64ul; ++i)
    {
        fixture f;
        raft89 *node;
        int rc;
        node = NULL;
        fixture_init(&f, 3u, 1u);
        fake_random_push(&f.random, 0u);
        fail_alloc_begin(i);
        rc = raft89_create(&f.config, &node);
        fail_alloc_disable();
        if (rc == RAFT89_OK)
        {
            saw_success = 1;
            CHECK(node != NULL);
            raft89_destroy(node);
        }
        else
        {
            CHECK_EQ(rc, RAFT89_ERR_NOMEM);
            CHECK(node == NULL);
        }
        expect_no_live();
        if (saw_success != 0)
        {
            break;
        }
    }
    CHECK(saw_success != 0);
}

static void run_propose_failures(void)
{
    unsigned long i;
    int saw_success;
    saw_success = 0;
    for (i = 0ul; i < 64ul; ++i)
    {
        fixture f;
        raft89 *node;
        raft89_status st;
        const raft89_action *action;
        int rc;
        node = NULL;
        action = NULL;
        fixture_init(&f, 1u, 1u);
        fake_random_push(&f.random, 0u);
        fail_alloc_disable();
        CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
        CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
        CHECK_EQ(driver_expect_hard_state(node, 1u, 1u), 1);
        CHECK_EQ(raft89_status_get(node, &st), RAFT89_OK);
        CHECK_EQ(st.role, RAFT89_LEADER);
        fail_alloc_begin(i);
        rc = raft89_propose(node, "A", 1u, NULL);
        fail_alloc_disable();
        if (rc == RAFT89_OK)
        {
            saw_success = 1;
            CHECK_EQ(driver_ack_with_effect(&f.store, node), RAFT89_OK);
            if (raft89_next_action(node, &action) == RAFT89_OK)
            {
                CHECK_EQ(action->type, RAFT89_ACT_APPLY);
                CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                         RAFT89_OK);
            }
        }
        else
        {
            CHECK_EQ(rc, RAFT89_ERR_NOMEM);
            CHECK_EQ(raft89_status_get(node, &st), RAFT89_OK);
            CHECK_U64(st.last_log_index, test_u64(0u));
            CHECK_U64(st.commit_index, test_u64(0u));
            CHECK_EQ(st.role, RAFT89_LEADER);
        }
        raft89_destroy(node);
        expect_no_live();
        if (saw_success != 0)
        {
            break;
        }
    }
    CHECK(saw_success != 0);
}

static void run_proposev_failures(void)
{
    unsigned long i;
    int saw_success;
    saw_success = 0;
    for (i = 0ul; i < 64ul; ++i)
    {
        fixture f;
        raft89 *node;
        raft89_command commands[3];
        raft89_status st;
        int rc;
        node = NULL;
        fixture_init(&f, 1u, 1u);
        fake_random_push(&f.random, 0u);
        fail_alloc_disable();
        CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
        CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
        CHECK_EQ(driver_expect_hard_state(node, 1u, 1u), 1);
        commands[0].data = "A";
        commands[0].size = 1u;
        commands[1].data = "BB";
        commands[1].size = 2u;
        commands[2].data = "CCC";
        commands[2].size = 3u;
        fail_alloc_begin(i);
        rc = raft89_proposev(node, commands, 3u, NULL);
        fail_alloc_disable();
        if (rc == RAFT89_OK)
        {
            saw_success = 1;
            CHECK_EQ(driver_ack_with_effect(&f.store, node), RAFT89_OK);
            driver_drain(node);
        }
        else
        {
            CHECK_EQ(rc, RAFT89_ERR_NOMEM);
            CHECK_EQ(raft89_status_get(node, &st), RAFT89_OK);
            CHECK_U64(st.last_log_index, test_u64(0u));
            CHECK_U64(st.commit_index, test_u64(0u));
            CHECK_EQ(st.role, RAFT89_LEADER);
        }
        raft89_destroy(node);
        expect_no_live();
        if (saw_success != 0)
        {
            break;
        }
    }
    CHECK(saw_success != 0);
}

static void run_recv_failures(void)
{
    unsigned long i;
    int saw_success;
    saw_success = 0;
    for (i = 0ul; i < 64ul; ++i)
    {
        fixture f;
        raft89 *node;
        raft89_entry entry;
        raft89_message msg;
        const raft89_action *action;
        int rc;
        node = NULL;
        action = NULL;
        fixture_init(&f, 3u, 1u);
        fake_random_push(&f.random, 0u);
        fail_alloc_disable();
        CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
        entry.term = test_u64(1u);
        entry.index = test_u64(1u);
        entry.data = "A";
        entry.size = 1u;
        driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, &entry, 1u, &msg);
        fail_alloc_begin(i);
        rc = raft89_recv(node, &msg);
        fail_alloc_disable();
        if (rc == RAFT89_OK)
        {
            saw_success = 1;
        }
        else
        {
            CHECK_EQ(rc, RAFT89_ERR_NOMEM);
            CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
        }
        raft89_destroy(node);
        expect_no_live();
        if (saw_success != 0)
        {
            break;
        }
    }
    CHECK(saw_success != 0);
}

int main(void)
{
    fail_alloc_disable();
    run_create_failures();
    run_propose_failures();
    run_proposev_failures();
    run_recv_failures();
    TEST_END;
}
