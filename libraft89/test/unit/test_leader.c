/* test_leader.c - leader replication, proposal, and commit rule.
 * Covers scenarios R01-R20 and K01-K09. */
#include <string.h>

#include "test.h"

#include "driver.h"
#include "fixture.h"

#define CAP_ENTRIES 8
#define CAP_DATA 16

typedef struct captured_ae
{
    raft89_message msg;
    raft89_entry entries[CAP_ENTRIES];
    unsigned char data[CAP_ENTRIES][CAP_DATA];
    unsigned long count;
} captured_ae;

static int take_ae(raft89 *node, captured_ae *cap)
{
    const raft89_action *action;
    unsigned long i;
    action = NULL;
    if (raft89_next_action(node, &action) != RAFT89_OK)
    {
        return 0;
    }
    if (action->type != RAFT89_ACT_SEND)
    {
        return 0;
    }
    cap->msg = action->u.send.message;
    cap->count = 0u;
    if (cap->msg.type == RAFT89_MSG_APPEND_ENTRIES)
    {
        cap->count = cap->msg.u.append_entries.entry_count;
        for (i = 0u; i < cap->count; ++i)
        {
            if (i >= CAP_ENTRIES)
            {
                break;
            }
            cap->entries[i] = cap->msg.u.append_entries.entries[i];
            if (cap->entries[i].size <= CAP_DATA)
            {
                memcpy(cap->data[i], cap->entries[i].data,
                       cap->entries[i].size);
                cap->entries[i].data = cap->data[i];
            }
        }
        cap->msg.u.append_entries.entries = cap->entries;
    }
    if (raft89_action_done(node, action->id, RAFT89_ACTION_OK) != RAFT89_OK)
    {
        return -1;
    }
    return 1;
}

static void expect_no_action(raft89 *node)
{
    const raft89_action *action;
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
    CHECK(action == NULL);
}

static void elect_leader_3(raft89 *node, unsigned long term)
{
    raft89_message msg;
    driver_build_vote_response(2u, 1u, term, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    driver_drain(node);
}

static void propose_one(fake_store *store, raft89 *node, const char *text,
                        unsigned long expected)
{
    const raft89_action *action;
    raft89_index index;
    index = raft89_u64_zero();
    CHECK_EQ(raft89_propose(node, text, 1u, &index), RAFT89_OK);
    CHECK_U64(index, test_u64(expected));
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->type, RAFT89_ACT_LOG_APPEND);
        CHECK_EQ(action->u.log_append.entry_count, 1u);
        CHECK_U64(action->u.log_append.entries[0].index, test_u64(expected));
        driver_store_effect(store, action);
        CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                 RAFT89_OK);
    }
}

static void ack_ae_response(raft89 *node, raft89_id from, unsigned long term,
                            int success, unsigned long match,
                            unsigned long conflict_term,
                            unsigned long conflict_index)
{
    raft89_message msg;
    driver_build_ae_response(from, 1u, term, success, match, conflict_term,
                             conflict_index, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
}

static void expect_apply(raft89 *node, unsigned long index)
{
    const raft89_action *action;
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->type, RAFT89_ACT_APPLY);
        CHECK_U64(action->u.apply.entry.index, test_u64(index));
        CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                 RAFT89_OK);
    }
}

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    captured_ae cap;
    raft89_index index;
    unsigned long i;

    /* R01/R02/R03/R04/R06: proposal order, term, and replication. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
    driver_drain(node);
    elect_leader_3(node, 2u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_LEADER);

    /* R04: local append precedes replication. */
    index = raft89_u64_zero();
    CHECK_EQ(raft89_propose(node, "x", 1u, &index), RAFT89_OK);
    CHECK_U64(index, test_u64(1u));
    {
        const raft89_action *action;
        action = NULL;
        CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
        CHECK(action != NULL);
        if (action != NULL)
        {
            CHECK_EQ(action->type, RAFT89_ACT_LOG_APPEND);
            driver_store_effect(&f.store, action);
            CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                     RAFT89_OK);
        }
    }

    /* R06: replication follows the acknowledged append. */
    CHECK_EQ(raft89_propose(node, "y", 1u, &index), RAFT89_BUSY);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_EQ(cap.msg.type, RAFT89_MSG_APPEND_ENTRIES);
    CHECK_EQ(cap.msg.u.append_entries.entry_count, 1u);
    CHECK_U64(cap.entries[0].index, test_u64(1u));
    CHECK(memcmp(cap.entries[0].data, "x", 1u) == 0);
    CHECK_EQ(take_ae(node, &cap), 1);

    /* Argument validation with no outstanding action. */
    CHECK_EQ(raft89_propose(node, NULL, 1u, &index), RAFT89_ERR_ARG);
    CHECK_EQ(raft89_propose(node, "y", 2000u, &index), RAFT89_ERR_LIMIT);
    raft89_destroy(node);

    /* A follower refuses proposals. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node != NULL)
    {
        CHECK_EQ(raft89_propose(node, "z", 1u, &index), RAFT89_NOT_LEADER);
        raft89_destroy(node);
    }

    /* R06/R07/R08/R17: replication, match advance, caught-up heartbeat. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
    driver_drain(node);
    elect_leader_3(node, 2u);
    propose_one(&f.store, node, "a", 1u);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_EQ(cap.msg.u.append_entries.entry_count, 1u);
    CHECK_U64(cap.entries[0].index, test_u64(1u));
    CHECK_U64(cap.entries[0].term, test_u64(2u));
    CHECK(memcmp(cap.entries[0].data, "a", 1u) == 0);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_EQ(cap.msg.u.append_entries.entry_count, 1u);

    /* Success from peer 2 forms a quorum and commits. */
    ack_ae_response(node, 2u, 2u, 1, 1u, 0u, 0u);
    expect_apply(node, 1u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.commit_index, test_u64(1u));

    /* Duplicate and stale successes change nothing. */
    ack_ae_response(node, 2u, 2u, 1, 1u, 0u, 0u);
    expect_no_action(node);
    ack_ae_response(node, 2u, 2u, 1, 0u, 0u, 0u);
    expect_no_action(node);

    /* Heartbeat now carries no entries for peer 2. */
    CHECK_EQ(raft89_tick(node, 10u), RAFT89_OK);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_U64(cap.msg.u.append_entries.prev_log_index, test_u64(1u));
    CHECK_EQ(cap.msg.u.append_entries.entry_count, 0u);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_EQ(cap.msg.u.append_entries.entry_count, 1u);
    raft89_destroy(node);

    /* K02/K03: old-term majority alone must not commit. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "old", 3u), RAFT89_OK);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 6u, 1u), 1);
    driver_drain(node);
    elect_leader_3(node, 6u);
    ack_ae_response(node, 2u, 6u, 1, 1u, 0u, 0u);
    expect_no_action(node);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.commit_index, test_u64(0u));

    /* A current-term entry commits and drags the old entry with it. */
    propose_one(&f.store, node, "new", 2u);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_EQ(take_ae(node, &cap), 1);
    ack_ae_response(node, 2u, 6u, 1, 2u, 0u, 0u);
    expect_apply(node, 1u);
    expect_apply(node, 2u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.commit_index, test_u64(2u));
    raft89_destroy(node);

    /* K04/K06: minority and duplicates never form a quorum. */
    fixture_init(&f, 5u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
    driver_drain(node);
    {
        raft89_message vote;
        driver_build_vote_response(2u, 1u, 2u, 1, &vote);
        CHECK_EQ(raft89_recv(node, &vote), RAFT89_OK);
        driver_build_vote_response(3u, 1u, 2u, 1, &vote);
        CHECK_EQ(raft89_recv(node, &vote), RAFT89_OK);
        driver_drain(node);
    }
    propose_one(&f.store, node, "q", 1u);
    for (i = 0u; i < 5u; ++i)
    {
        if (take_ae(node, &cap) != 1)
        {
            break;
        }
    }
    ack_ae_response(node, 2u, 2u, 1, 1u, 0u, 0u);
    ack_ae_response(node, 2u, 2u, 1, 1u, 0u, 0u);
    expect_no_action(node);
    ack_ae_response(node, 3u, 2u, 1, 1u, 0u, 0u);
    expect_apply(node, 1u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.commit_index, test_u64(1u));
    raft89_destroy(node);

    /* R11/R12/R13/R14: failure backoff discovers the prefix. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "b", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 3u, "c", 1u), RAFT89_OK);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 6u, 1u), 1);
    driver_drain(node);
    elect_leader_3(node, 6u);
    /* next_index for peer 2 is 4: conflict hints jump it to 2, then 1. */
    ack_ae_response(node, 2u, 6u, 0, 0u, 0u, 2u);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_U64(cap.msg.u.append_entries.prev_log_index, test_u64(1u));
    CHECK_EQ(cap.msg.u.append_entries.entry_count, 2u);
    ack_ae_response(node, 2u, 6u, 0, 0u, 0u, 1u);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_U64(cap.msg.u.append_entries.prev_log_index, test_u64(0u));
    CHECK_EQ(cap.msg.u.append_entries.entry_count, 3u);
    ack_ae_response(node, 2u, 6u, 1, 3u, 0u, 0u);
    expect_no_action(node);
    raft89_destroy(node);

    /* R15/R16: batch bounds. */
    fixture_init(&f, 3u, 1u);
    f.config.max_append_entries = 2u;
    f.config.max_append_bytes = 16u;
    fake_store_set_hard(&f.store, 5u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "b", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 3u, "c", 1u), RAFT89_OK);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 6u, 1u), 1);
    driver_drain(node);
    elect_leader_3(node, 6u);
    ack_ae_response(node, 2u, 6u, 0, 0u, 0u, 1u);
    CHECK_EQ(take_ae(node, &cap), 1);
    ack_ae_response(node, 2u, 6u, 0, 0u, 0u, 1u);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_EQ(cap.msg.u.append_entries.entry_count, 2u);
    ack_ae_response(node, 2u, 6u, 0, 0u, 0u, 1u);
    CHECK_EQ(take_ae(node, &cap), 1);
    CHECK_EQ(cap.msg.u.append_entries.entry_count, 2u);
    raft89_destroy(node);

    /* R19/R20: a higher-term response steps the leader down. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
    driver_drain(node);
    elect_leader_3(node, 2u);
    ack_ae_response(node, 2u, 3u, 1, 0u, 0u, 0u);
    CHECK_EQ(driver_expect_hard_state(node, 3u, RAFT89_ID_NONE), 1);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_FOLLOWER);
    CHECK_U64(status.current_term, test_u64(3u));
    raft89_destroy(node);

    /* Non-member and malformed responses are rejected. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node != NULL)
    {
        raft89_message bad;
        driver_build_ae_response(9u, 1u, 1u, 1, 0u, 0u, 0u, &bad);
        CHECK_EQ(raft89_recv(node, &bad), RAFT89_ERR_PROTOCOL);
        driver_build_ae_response(2u, 1u, 1u, 2, 0u, 0u, 0u, &bad);
        CHECK_EQ(raft89_recv(node, &bad), RAFT89_ERR_PROTOCOL);
        driver_build_ae_response(2u, 1u, 1u, 1, 0u, 1u, 0u, &bad);
        CHECK_EQ(raft89_recv(node, &bad), RAFT89_ERR_PROTOCOL);
        driver_build_ae_response(2u, 1u, 1u, 1, 0u, 0u, 1u, &bad);
        CHECK_EQ(raft89_recv(node, &bad), RAFT89_ERR_PROTOCOL);
        driver_build_ae_response(2u, 1u, 1u, 0, 0u, 0u, 0u, &bad);
        CHECK_EQ(raft89_recv(node, &bad), RAFT89_ERR_PROTOCOL);
        driver_build_ae_response(2u, 1u, 1u, 0, 3u, 0u, 1u, &bad);
        CHECK_EQ(raft89_recv(node, &bad), RAFT89_ERR_PROTOCOL);
        raft89_destroy(node);
    }

    TEST_END;
}
