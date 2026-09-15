/* test_limits.c - integer and size boundaries. Covers scenarios in
 * .agent/testing/0006-limits.md and the v2 overflow policy in
 * .agent/testing/0011-portable-scalars.md. */
#include <limits.h>
#include <string.h>

#include "test.h"

#include "driver.h"
#include "fixture.h"
#include "raft89_inspect.h"
#include "raft89_internal.h"

static raft89_u64 u64(raft89_u32 hi, raft89_u32 lo)
{
    raft89_u64 v;
    v.hi = hi;
    v.lo = lo;
    return v;
}

static void set_hard(fixture *f, raft89_u64 term)
{
    f->store.hard.current_term = term;
    f->store.hard.voted_for = RAFT89_ID_NONE;
}

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    raft89_message msg;
    raft89_entry entries[2];

    /* U05: starting an election at term 2^64-1 is refused without
     * faulting the node and without changing any protocol state. */
    fixture_init(&f, 3u, 1u);
    set_hard(&f, u64(0xffffffffu, 0xffffffffu));
    fake_random_push(&f.random, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_ERR_LIMIT);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.faulted, 0);
    CHECK_EQ(status.role, RAFT89_FOLLOWER);
    CHECK_U64(status.current_term, u64(0xffffffffu, 0xffffffffu));
    CHECK_EQ(status.voted_for, RAFT89_ID_NONE);
    CHECK_U64(status.last_log_index, RAFT89_INDEX_NONE);
    CHECK_U64(status.commit_index, RAFT89_INDEX_NONE);
    CHECK_U64(status.applied_index, RAFT89_INDEX_NONE);
    {
        const raft89_action *action;
        action = NULL;
        CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
        CHECK(action == NULL);
    }
    raft89_destroy(node);

    /* Leadership at the maximum index faults rather than computing +1. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    f.store.override_last = 1;
    f.store.last_index_override = u64(0xffffffffu, 0xffffffffu);
    f.store.last_term_override = u64(0u, 1u);
    fake_random_push(&f.random, 0u);
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
    driver_build_vote_response(2u, 1u, 2u, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_LIMIT);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.faulted, 1);
    CHECK(raft89_inspect_action(node) == NULL);
    CHECK_EQ(raft89_tick(node, 1u), RAFT89_ERR_FAULTED);
    raft89_destroy(node);

    /* U04: an election starting just below 2^32 lands exactly on 2^32. */
    fixture_init(&f, 3u, 1u);
    set_hard(&f, u64(0u, 0xffffffffu));
    fake_random_push(&f.random, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    {
        const raft89_action *action;
        action = NULL;
        CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
        CHECK(action != NULL);
        if (action != NULL)
        {
            CHECK_EQ(action->type, RAFT89_ACT_HARD_STATE);
            CHECK_U64(action->u.hard_state.state.current_term, u64(1u, 0u));
            CHECK_EQ(action->u.hard_state.state.voted_for, 1u);
            CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                     RAFT89_OK);
        }
    }
    driver_drain(node);
    raft89_destroy(node);

    /* U03: durable term and log metadata above 2^32 survive creation. */
    fixture_init(&f, 3u, 1u);
    set_hard(&f, u64(2u, 3u));
    f.store.override_last = 1;
    f.store.last_index_override = u64(1u, 5u);
    f.store.last_term_override = u64(2u, 7u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.current_term, u64(2u, 3u));
    CHECK_U64(status.last_log_index, u64(1u, 5u));
    raft89_destroy(node);

    /* U06: proposing at the maximum index faults the node and emits no
     * action. A single-node cluster grows its log from 2^64-2 to
     * 2^64-1 without any store read of the high index. */
    fixture_init(&f, 1u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    f.store.override_last = 1;
    f.store.last_index_override = u64(0xffffffffu, 0xfffffffeu);
    f.store.last_term_override = u64(0u, 1u);
    fake_random_push(&f.random, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_LEADER);
    {
        raft89_index index;
        index = raft89_u64_zero();
        CHECK_EQ(raft89_propose(node, "x", 1u, &index), RAFT89_OK);
        CHECK_U64(index, u64(0xffffffffu, 0xffffffffu));
        driver_drain(node);
    }
    CHECK_EQ(raft89_propose(node, "y", 1u, NULL), RAFT89_ERR_LIMIT);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.faulted, 1);
    CHECK_U64(status.last_log_index, u64(0xffffffffu, 0xffffffffu));
    CHECK(raft89_inspect_action(node) == NULL);
    CHECK_EQ(raft89_tick(node, 1u), RAFT89_ERR_FAULTED);
    raft89_destroy(node);

    /* An AppendEntries whose batch range would wrap past 2^64-1 is
     * rejected as a protocol error with no action. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    entries[0].index = u64(0xffffffffu, 0xffffffffu);
    entries[0].term = test_u64(1u);
    entries[0].data = "x";
    entries[0].size = 1u;
    driver_build_ae(2u, 1u, 1u, 0ul, 1u, 0u, entries, 1u, &msg);
    msg.u.append_entries.prev_log_index = u64(0xffffffffu, 0xffffffffu);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    {
        const raft89_action *action;
        action = NULL;
        CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
    }

    /* Saturating aggregate sizes are rejected. */
    entries[0].index = test_u64(1u);
    entries[0].term = test_u64(1u);
    entries[0].data = "x";
    entries[0].size = ULONG_MAX;
    entries[1].index = test_u64(2u);
    entries[1].term = test_u64(1u);
    entries[1].data = "y";
    entries[1].size = 1u;
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, entries, 2u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    raft89_destroy(node);

    /* Proposal larger than the byte bound is refused on a leader. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node != NULL)
    {
        raft89_index index;
        index = raft89_u64_zero();
        CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
        CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
        driver_drain(node);
        driver_build_vote_response(2u, 1u, 2u, 1, &msg);
        CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
        driver_drain(node);
        CHECK_EQ(raft89_propose(node, "zzz", 2000u, &index), RAFT89_ERR_LIMIT);
        CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
        CHECK_U64(status.last_log_index, RAFT89_INDEX_NONE);
        raft89_destroy(node);
    }

    TEST_END;
}
