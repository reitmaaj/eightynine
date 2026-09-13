/* test_limits.c - integer and size boundaries. Covers scenarios in
 * .agent/testing/0006-limits.md. */
#include <limits.h>
#include <string.h>

#include "test.h"

#include "driver.h"
#include "fixture.h"
#include "raft89_internal.h"

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    raft89_message msg;
    raft89_entry entries[2];

    /* Term overflow faults instead of wrapping. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, ULONG_MAX, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_ERR_LIMIT);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.faulted, 1);
    CHECK_EQ(status.current_term, ULONG_MAX);
    CHECK_EQ(raft89_tick(node, 1u), RAFT89_ERR_FAULTED);
    raft89_destroy(node);

    /* Leadership at the maximum index faults rather than computing +1. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    f.store.override_last = 1;
    f.store.last_index_override = ULONG_MAX;
    f.store.last_term_override = 1u;
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
    raft89_destroy(node);

    /* AppendEntries with an overflowing batch range is rejected. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    entries[0].index = ULONG_MAX;
    entries[0].term = 1u;
    entries[0].data = "x";
    entries[0].size = 1u;
    driver_build_ae(2u, 1u, 1u, ULONG_MAX, 1u, 0u, entries, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    {
        const raft89_action *action;
        action = NULL;
        CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
    }

    /* Saturating aggregate sizes are rejected. */
    entries[0].index = 1u;
    entries[0].term = 1u;
    entries[0].data = "x";
    entries[0].size = ULONG_MAX;
    entries[1].index = 2u;
    entries[1].term = 1u;
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
        index = 0u;
        CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
        CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
        driver_drain(node);
        driver_build_vote_response(2u, 1u, 2u, 1, &msg);
        CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
        driver_drain(node);
        CHECK_EQ(raft89_propose(node, "zzz", 2000u, &index), RAFT89_ERR_LIMIT);
        CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
        CHECK_EQ(status.last_log_index, 0u);
        raft89_destroy(node);
    }

    TEST_END;
}
