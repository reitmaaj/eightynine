/* test_apply.c - ordered application of committed entries. Covers
 * scenarios P01-P06. */
#include <string.h>

#include "test.h"

#include "driver.h"
#include "fixture.h"

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    raft89_message msg;
    const raft89_action *action;

    /* P01-P05: strict order, no gaps, exact payloads, one at a time. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 2u, 2u, "bb", 2u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 3u, 3u, "ccc", 3u), RAFT89_OK);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    driver_build_ae(2u, 1u, 1u, 3u, 3u, 3u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);

    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->type, RAFT89_ACT_APPLY);
        CHECK_U64(action->u.apply.entry.index, test_u64(1u));
        CHECK_U64(action->u.apply.entry.term, test_u64(1u));
        CHECK_EQ(action->u.apply.entry.size, 1u);
        CHECK(memcmp(action->u.apply.entry.data, "a", 1u) == 0);
        /* P03: events are busy while the apply is outstanding. */
        CHECK_EQ(raft89_tick(node, 1u), RAFT89_BUSY);
        CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                 RAFT89_OK);
    }

    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->type, RAFT89_ACT_APPLY);
        CHECK_U64(action->u.apply.entry.index, test_u64(2u));
        CHECK_U64(action->u.apply.entry.term, test_u64(2u));
        CHECK_EQ(action->u.apply.entry.size, 2u);
        CHECK(memcmp(action->u.apply.entry.data, "bb", 2u) == 0);
        CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                 RAFT89_OK);
    }

    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->type, RAFT89_ACT_APPLY);
        CHECK_U64(action->u.apply.entry.index, test_u64(3u));
        CHECK_U64(action->u.apply.entry.term, test_u64(3u));
        CHECK_EQ(action->u.apply.entry.size, 3u);
        CHECK(memcmp(action->u.apply.entry.data, "ccc", 3u) == 0);
        CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                 RAFT89_OK);
    }

    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, test_u64(3u));
    CHECK_U64(status.commit_index, test_u64(3u));
    raft89_destroy(node);

    /* P06: FATAL on APPLY faults the node. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    driver_build_ae(2u, 1u, 1u, 1u, 1u, 1u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->type, RAFT89_ACT_APPLY);
        CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_FATAL),
                 RAFT89_OK);
    }
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.faulted, 1);
    CHECK_EQ(raft89_tick(node, 1u), RAFT89_ERR_FAULTED);
    raft89_destroy(node);

    TEST_END;
}
