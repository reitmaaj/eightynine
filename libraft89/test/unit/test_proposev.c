/* test_proposev.c - batched proposals. Covers scenarios PV01-PV11 in
 * .agent/testing/0013-proposev.md. */
#include <limits.h>
#include <string.h>

#include "test.h"

#include "driver.h"
#include "fixture.h"
#include "raft89_inspect.h"
#include "raft89_internal.h"

static unsigned char big[2048];

static raft89_u64 pub_u64(raft89_u32 hi, raft89_u32 lo)
{
    raft89_u64 v;
    v.hi = hi;
    v.lo = lo;
    return v;
}

static void become_leader(fixture *f, raft89 **node)
{
    fake_random_push(&f->random, 0u);
    *node = NULL;
    CHECK_EQ(raft89_create(&f->config, node), RAFT89_OK);
    CHECK(*node != NULL);
    if (*node == NULL)
    {
        return;
    }
    CHECK_EQ(raft89_tick(*node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(*node, 1u, 1u), 1);
    driver_drain(*node);
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

static void ack_append_with_effect(fixture *f, raft89 *node)
{
    const raft89_action *action;

    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    if (action == NULL)
    {
        return;
    }
    CHECK_EQ(action->type, RAFT89_ACT_LOG_APPEND);
    driver_store_effect(&f->store, action);
    CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK), RAFT89_OK);
}

static void test_basic_batch(void)
{
    fixture f;
    raft89 *node;
    raft89_command commands[3];
    raft89_index first;
    const raft89_action *action;
    unsigned long i;

    fixture_init(&f, 1u, 1u);
    become_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    commands[0].data = "A";
    commands[0].size = 1u;
    commands[1].data = "BB";
    commands[1].size = 2u;
    commands[2].data = "CCC";
    commands[2].size = 3u;
    first = raft89_u64_zero();
    CHECK_EQ(raft89_proposev(node, commands, 3u, &first), RAFT89_OK);
    CHECK_U64(first, test_u64(1u));

    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->type, RAFT89_ACT_LOG_APPEND);
        CHECK_EQ(action->u.log_append.entry_count, 3u);
        for (i = 0u; i < 3u; ++i)
        {
            CHECK_U64(action->u.log_append.entries[i].index, test_u64(i + 1ul));
            CHECK_U64(action->u.log_append.entries[i].term, test_u64(1u));
            CHECK_EQ(action->u.log_append.entries[i].size, commands[i].size);
            CHECK(memcmp(action->u.log_append.entries[i].data, commands[i].data,
                         commands[i].size) == 0);
        }
        driver_store_effect(&f.store, action);
        CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                 RAFT89_OK);
    }
    expect_apply(node, 1u);
    expect_apply(node, 2u);
    expect_apply(node, 3u);
    driver_drain(node);
    expect_no_action(node);
    raft89_destroy(node);
}

static void test_empty_and_args(void)
{
    fixture f;
    raft89 *node;
    raft89_command command;
    raft89_status status;

    fixture_init(&f, 1u, 1u);
    become_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    command.data = "x";
    command.size = 1u;
    CHECK_EQ(raft89_proposev(node, &command, 0u, NULL), RAFT89_ERR_ARG);
    CHECK_EQ(raft89_proposev(node, NULL, 1u, NULL), RAFT89_ERR_ARG);
    command.data = NULL;
    CHECK_EQ(raft89_proposev(node, &command, 1u, NULL), RAFT89_ERR_ARG);
    expect_no_action(node);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.last_log_index, test_u64(0u));

    /* A zero-length payload with NULL data is valid. */
    command.data = NULL;
    command.size = 0u;
    CHECK_EQ(raft89_proposev(node, &command, 1u, NULL), RAFT89_OK);
    ack_append_with_effect(&f, node);
    expect_apply(node, 1u);
    driver_drain(node);
    raft89_destroy(node);
}

static void test_limits(void)
{
    fixture f;
    raft89 *node;
    raft89_command commands[9];
    const raft89_action *action;
    unsigned long i;

    fixture_init(&f, 1u, 1u);
    become_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    for (i = 0u; i < 9u; ++i)
    {
        commands[i].data = "x";
        commands[i].size = 1u;
    }

    /* Entry-count and byte limits are rejected without mutation. */
    CHECK_EQ(raft89_proposev(node, commands, 9u, NULL), RAFT89_ERR_LIMIT);
    commands[0].size = ULONG_MAX;
    CHECK_EQ(raft89_proposev(node, commands, 2u, NULL), RAFT89_ERR_LIMIT);
    commands[0].size = 1025u;
    commands[0].data = big;
    commands[1].size = 0u;
    commands[1].data = NULL;
    CHECK_EQ(raft89_proposev(node, commands, 1u, NULL), RAFT89_ERR_LIMIT);
    expect_no_action(node);
    raft89_destroy(node);

    /* Byte-boundary acceptance: max_append_bytes - 1 and max_append_bytes. */
    fixture_init(&f, 1u, 1u);
    become_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    commands[0].data = big;
    commands[0].size = 1023u;
    CHECK_EQ(raft89_proposev(node, commands, 1u, NULL), RAFT89_OK);
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->u.log_append.entries[0].size, 1023u);
    }
    raft89_destroy(node);

    fixture_init(&f, 1u, 1u);
    become_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    commands[0].data = big;
    commands[0].size = 1024u;
    CHECK_EQ(raft89_proposev(node, commands, 1u, NULL), RAFT89_OK);
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->u.log_append.entries[0].size, 1024u);
    }
    raft89_destroy(node);

    /* Entry-count boundary: exactly max_append_entries is accepted. */
    fixture_init(&f, 1u, 1u);
    become_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    for (i = 0u; i < 8u; ++i)
    {
        commands[i].data = "x";
        commands[i].size = 1u;
    }
    CHECK_EQ(raft89_proposev(node, commands, 8u, NULL), RAFT89_OK);
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action != NULL)
    {
        CHECK_EQ(action->u.log_append.entry_count, 8u);
    }
    raft89_destroy(node);
}

static void test_index_overflow(void)
{
    fixture f;
    raft89 *node;
    raft89_command commands[2];
    raft89_status status;

    /* Private test setup: a leader whose last index is 2^64-2, without
     * durable entries at that artificial position. */
    fixture_init(&f, 1u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    node->role = RAFT89_LEADER;
    node->last_log_index =
        raft89__from_public(pub_u64(0xffffffffu, 0xfffffffeu));
    node->last_log_term = raft89__from_public(test_u64(1u));
    commands[0].data = "x";
    commands[0].size = 1u;
    commands[1].data = "y";
    commands[1].size = 1u;
    CHECK_EQ(raft89_proposev(node, commands, 2u, NULL), RAFT89_ERR_LIMIT);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.faulted, 1);
    CHECK(raft89_inspect_action(node) == NULL);
    raft89_destroy(node);
}

static void test_role_and_busy(void)
{
    fixture f;
    raft89 *node;
    raft89_command command;

    command.data = "x";
    command.size = 1u;

    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node != NULL)
    {
        CHECK_EQ(raft89_proposev(node, &command, 1u, NULL), RAFT89_NOT_LEADER);
        expect_no_action(node);
        raft89_destroy(node);
    }

    fixture_init(&f, 1u, 1u);
    become_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    CHECK_EQ(raft89_proposev(node, &command, 1u, NULL), RAFT89_OK);
    CHECK_EQ(raft89_proposev(node, &command, 1u, NULL), RAFT89_BUSY);
    raft89_destroy(node);
}

static void test_multi_node_replication(void)
{
    fixture f;
    raft89 *node;
    raft89_command commands[3];
    raft89_message msg;
    unsigned long i;

    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    fake_random_push(&f.random, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
    driver_drain(node);
    driver_build_vote_response(2u, 1u, 2u, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    driver_drain(node);

    commands[0].data = "A";
    commands[0].size = 1u;
    commands[1].data = "BB";
    commands[1].size = 2u;
    commands[2].data = "CCC";
    commands[2].size = 3u;
    CHECK_EQ(raft89_proposev(node, commands, 3u, NULL), RAFT89_OK);
    ack_append_with_effect(&f, node);

    /* The outstanding replication send carries the complete batch. */
    {
        const raft89_action *action;

        action = NULL;
        CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
        CHECK(action != NULL);
        if (action != NULL)
        {
            CHECK_EQ(action->type, RAFT89_ACT_SEND);
            msg = action->u.send.message;
            CHECK_EQ(msg.type, RAFT89_MSG_APPEND_ENTRIES);
            CHECK_EQ(msg.u.append_entries.entry_count, 3u);
            for (i = 0u; i < 3u; ++i)
            {
                CHECK_U64(msg.u.append_entries.entries[i].index,
                          test_u64(i + 1ul));
                CHECK_U64(msg.u.append_entries.entries[i].term, test_u64(2u));
                CHECK_EQ(msg.u.append_entries.entries[i].size,
                         commands[i].size);
            }
            CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                     RAFT89_OK);
        }
    }
    driver_drain(node);
    raft89_destroy(node);
}

static void test_equivalence(void)
{
    fixture fa;
    fixture fb;
    raft89 *na;
    raft89 *nb;
    raft89_command command;
    raft89_index ia;
    raft89_index ib;
    const raft89_action *aa;
    const raft89_action *ab;

    fixture_init(&fa, 1u, 1u);
    fixture_init(&fb, 1u, 1u);
    become_leader(&fa, &na);
    become_leader(&fb, &nb);
    if (na == NULL || nb == NULL)
    {
        raft89_destroy(na);
        raft89_destroy(nb);
        return;
    }
    ia = raft89_u64_zero();
    ib = raft89_u64_zero();
    CHECK_EQ(raft89_propose(na, "X", 1u, &ia), RAFT89_OK);
    command.data = "X";
    command.size = 1u;
    CHECK_EQ(raft89_proposev(nb, &command, 1u, &ib), RAFT89_OK);
    CHECK_U64(ia, ib);
    aa = NULL;
    ab = NULL;
    CHECK_EQ(raft89_next_action(na, &aa), RAFT89_OK);
    CHECK_EQ(raft89_next_action(nb, &ab), RAFT89_OK);
    CHECK(aa != NULL && ab != NULL);
    if (aa != NULL && ab != NULL)
    {
        CHECK_EQ(aa->type, ab->type);
        CHECK_EQ(aa->u.log_append.entry_count, ab->u.log_append.entry_count);
        CHECK_U64(aa->u.log_append.entries[0].index,
                  ab->u.log_append.entries[0].index);
        CHECK_U64(aa->u.log_append.entries[0].term,
                  ab->u.log_append.entries[0].term);
        CHECK_EQ(aa->u.log_append.entries[0].size,
                 ab->u.log_append.entries[0].size);
    }
    raft89_destroy(na);
    raft89_destroy(nb);
}

int main(void)
{
    test_basic_batch();
    test_empty_and_args();
    test_limits();
    test_index_overflow();
    test_role_and_busy();
    test_multi_node_replication();
    test_equivalence();
    TEST_END;
}
