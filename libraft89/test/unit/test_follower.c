/* test_follower.c - follower AppendEntries handling. Covers scenarios
 * F01-F26 and AppendEntries validation. */
#include <string.h>

#include "test.h"

#include "driver.h"
#include "fixture.h"

static raft89 *create_node(fixture *f)
{
    raft89 *node;
    node = NULL;
    CHECK_EQ(raft89_create(&f->config, &node), RAFT89_OK);
    return node;
}

static raft89 *make_node(fixture *f, raft89_size size, raft89_id self,
                         unsigned long term, raft89_id vote)
{
    fixture_init(f, size, self);
    fake_store_set_hard(&f->store, term, vote);
    return create_node(f);
}

static void entry_set(raft89_entry *entry, unsigned long index,
                      unsigned long term, const void *data, raft89_size size)
{
    entry->index = test_u64(index);
    entry->term = test_u64(term);
    entry->data = data;
    entry->size = size;
}

static void expect_ae_reply(raft89 *node, int success, unsigned long match)
{
    raft89_message msg;
    int rc;
    msg.type = RAFT89_MSG_REQUEST_VOTE;
    rc = driver_take_send(node, &msg);
    CHECK_EQ(rc, 1);
    if (rc != 1)
    {
        return;
    }
    CHECK_EQ(msg.type, RAFT89_MSG_APPEND_ENTRIES_RESPONSE);
    CHECK_EQ(msg.u.append_entries_response.success, success);
    CHECK_U64(msg.u.append_entries_response.match_index, test_u64(match));
}

static void expect_log_append(fake_store *store, raft89 *node,
                              unsigned long first, raft89_size count)
{
    const raft89_action *action;
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action == NULL)
    {
        return;
    }
    CHECK_EQ(action->type, RAFT89_ACT_LOG_APPEND);
    CHECK_EQ(action->u.log_append.entry_count, count);
    if (count != 0u)
    {
        CHECK_U64(action->u.log_append.entries[0].index, test_u64(first));
    }
    driver_store_effect(store, action);
    CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK), RAFT89_OK);
}

static void expect_log_truncate(fake_store *store, raft89 *node,
                                unsigned long first)
{
    const raft89_action *action;
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action == NULL)
    {
        return;
    }
    CHECK_EQ(action->type, RAFT89_ACT_LOG_TRUNCATE);
    CHECK_U64(action->u.log_truncate.first_index, test_u64(first));
    driver_store_effect(store, action);
    CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK), RAFT89_OK);
}

static void expect_apply(raft89 *node, unsigned long index, unsigned long term,
                         const void *data, raft89_size size)
{
    const raft89_action *action;
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    CHECK(action != NULL);
    if (action == NULL)
    {
        return;
    }
    CHECK_EQ(action->type, RAFT89_ACT_APPLY);
    CHECK_U64(action->u.apply.entry.index, test_u64(index));
    CHECK_U64(action->u.apply.entry.term, test_u64(term));
    CHECK_EQ(action->u.apply.entry.size, size);
    if (size != 0u)
    {
        CHECK(memcmp(action->u.apply.entry.data, data, size) == 0);
    }
    CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK), RAFT89_OK);
}

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    raft89_message msg;
    raft89_entry entries[8];

    /* F01/F10: heartbeat against an empty prefix succeeds. */
    node = make_node(&f, 3u, 1u, 1u, 0u);
    CHECK(node != NULL);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_ae_reply(node, 1, 0u);
    raft89_destroy(node);

    /* F02: an existing matching prefix is accepted and commits. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "a", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    driver_build_ae(2u, 1u, 1u, 2u, 1u, 2u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_apply(node, 1u, 1u, "a", 1u);
    expect_apply(node, 2u, 1u, "a", 1u);
    expect_ae_reply(node, 1, 2u);
    raft89_destroy(node);

    /* F03/F05/F06: a missing prefix fails without mutating the log. */
    node = make_node(&f, 3u, 1u, 1u, 0u);
    CHECK(node != NULL);
    driver_build_ae(2u, 1u, 1u, 3u, 1u, 0u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_ae_reply(node, 0, 0u);
    CHECK_EQ(f.store.entry_count, 0u);
    raft89_destroy(node);

    /* F04: an existing index with a different term fails. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "a", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    driver_build_ae(2u, 1u, 1u, 2u, 2u, 0u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_ae_reply(node, 0, 0u);
    CHECK_EQ(f.store.entry_count, 2u);
    raft89_destroy(node);

    /* F07/F08: append one entry to an empty log. */
    node = make_node(&f, 3u, 1u, 1u, 0u);
    CHECK(node != NULL);
    entry_set(&entries[0], 1u, 1u, "x", 1u);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, entries, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_log_append(&f.store, node, 1u, 1u);
    CHECK_EQ(f.store.entry_count, 1u);
    CHECK_U64(f.store.entries[0].term, test_u64(1u));
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.last_log_index, test_u64(1u));
    expect_ae_reply(node, 1, 1u);
    raft89_destroy(node);

    /* F09: append several entries in one action. */
    node = make_node(&f, 3u, 1u, 1u, 0u);
    CHECK(node != NULL);
    entry_set(&entries[0], 1u, 1u, "a", 1u);
    entry_set(&entries[1], 2u, 1u, "b", 1u);
    entry_set(&entries[2], 3u, 1u, "c", 1u);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, entries, 3u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_log_append(&f.store, node, 1u, 3u);
    CHECK_EQ(f.store.entry_count, 3u);
    expect_ae_reply(node, 1, 3u);
    raft89_destroy(node);

    /* F11: identical entries already present are not rewritten. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "b", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    entry_set(&entries[0], 1u, 1u, "a", 1u);
    entry_set(&entries[1], 2u, 1u, "b", 1u);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, entries, 2u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_ae_reply(node, 1, 2u);
    raft89_destroy(node);

    /* F12: only the missing suffix is appended. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    entry_set(&entries[0], 2u, 1u, "b", 1u);
    driver_build_ae(2u, 1u, 1u, 1u, 1u, 1u, entries, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_log_append(&f.store, node, 2u, 1u);
    expect_apply(node, 1u, 1u, "a", 1u);
    expect_ae_reply(node, 1, 2u);
    raft89_destroy(node);

    /* F13/F16/F17/F19: conflict at the first new entry. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 2u, 3u, "b", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 2u, 4u, "b", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 3u, 5u, "c", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    entry_set(&entries[0], 3u, 3u, "d", 1u);
    entry_set(&entries[1], 4u, 3u, "d", 1u);
    driver_build_ae(2u, 1u, 1u, 2u, 1u, 0u, entries, 2u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_log_truncate(&f.store, node, 3u);
    expect_log_append(&f.store, node, 3u, 2u);
    CHECK_EQ(f.store.entry_count, 4u);
    CHECK_U64(f.store.entries[0].term, test_u64(1u));
    CHECK_U64(f.store.entries[1].term, test_u64(1u));
    CHECK_U64(f.store.entries[2].term, test_u64(3u));
    CHECK_U64(f.store.entries[3].term, test_u64(3u));
    expect_ae_reply(node, 1, 4u);
    raft89_destroy(node);

    /* F14: conflict in the middle of a batch. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 2u, 3u, "b", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 2u, 4u, "b", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 3u, 5u, "c", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    entry_set(&entries[0], 2u, 1u, "a", 1u);
    entry_set(&entries[1], 3u, 2u, "b", 1u);
    entry_set(&entries[2], 4u, 3u, "d", 1u);
    entry_set(&entries[3], 5u, 3u, "d", 1u);
    driver_build_ae(2u, 1u, 1u, 1u, 1u, 0u, entries, 4u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_log_truncate(&f.store, node, 4u);
    expect_log_append(&f.store, node, 4u, 2u);
    expect_ae_reply(node, 1, 5u);
    raft89_destroy(node);

    /* F15: conflict at the final entry. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 2u, 3u, "b", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    entry_set(&entries[0], 3u, 3u, "c", 1u);
    driver_build_ae(2u, 1u, 1u, 2u, 1u, 0u, entries, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_log_truncate(&f.store, node, 3u);
    expect_log_append(&f.store, node, 3u, 1u);
    CHECK_EQ(f.store.entry_count, 3u);
    CHECK_U64(f.store.entries[2].term, test_u64(3u));
    expect_ae_reply(node, 1, 3u);
    raft89_destroy(node);

    /* F20: a lower leader_commit does not regress commit. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "a", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    driver_build_ae(2u, 1u, 1u, 2u, 1u, 2u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_apply(node, 1u, 1u, "a", 1u);
    expect_apply(node, 2u, 1u, "a", 1u);
    expect_ae_reply(node, 1, 2u);
    driver_build_ae(2u, 1u, 1u, 2u, 1u, 1u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_ae_reply(node, 1, 2u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.commit_index, test_u64(2u));
    CHECK_U64(status.applied_index, test_u64(2u));
    raft89_destroy(node);

    /* F22: leader_commit beyond the log is clamped. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    driver_build_ae(2u, 1u, 1u, 1u, 1u, 5u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_apply(node, 1u, 1u, "a", 1u);
    expect_ae_reply(node, 1, 1u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.commit_index, test_u64(1u));
    raft89_destroy(node);

    /* F23/F24/F26: commit advances and applies sequentially. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "b", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 3u, "c", 1u), RAFT89_OK);
    node = create_node(&f);
    CHECK(node != NULL);
    driver_build_ae(2u, 1u, 1u, 3u, 1u, 3u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_apply(node, 1u, 1u, "a", 1u);
    expect_apply(node, 2u, 1u, "b", 1u);
    expect_apply(node, 3u, 1u, "c", 1u);
    expect_ae_reply(node, 1, 3u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, test_u64(3u));
    raft89_destroy(node);

    /* F25: log durability precedes application. */
    node = make_node(&f, 3u, 1u, 1u, 0u);
    CHECK(node != NULL);
    entry_set(&entries[0], 1u, 1u, "a", 1u);
    entry_set(&entries[1], 2u, 1u, "b", 1u);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 2u, entries, 2u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_log_append(&f.store, node, 1u, 2u);
    expect_apply(node, 1u, 1u, "a", 1u);
    expect_apply(node, 2u, 1u, "b", 1u);
    expect_ae_reply(node, 1, 2u);
    raft89_destroy(node);

    /* Stale term is refused with the current term and no log change. */
    node = make_node(&f, 3u, 1u, 5u, 0u);
    CHECK(node != NULL);
    driver_build_ae(2u, 1u, 4u, 0u, 0u, 0u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    {
        const raft89_action *action;
        action = NULL;
        CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
        CHECK(action != NULL);
        if (action != NULL)
        {
            CHECK_EQ(action->type, RAFT89_ACT_SEND);
            CHECK_U64(action->u.send.message.u.append_entries_response.term,
                      test_u64(5u));
            CHECK_EQ(action->u.send.message.u.append_entries_response.success,
                     0);
            CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK),
                     RAFT89_OK);
        }
    }
    raft89_destroy(node);

    /* Malformed messages are rejected without state change. */
    node = make_node(&f, 3u, 1u, 1u, 0u);
    CHECK(node != NULL);
    entry_set(&entries[0], 2u, 1u, "x", 1u);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, entries, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    entry_set(&entries[0], 1u, 0u, "x", 1u);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, entries, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    entry_set(&entries[0], 1u, 1u, NULL, 1u);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, entries, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, NULL, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, entries, 9u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    raft89_destroy(node);

    /* An equal-term AppendEntries at a leader is a protocol error. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    node = create_node(&f);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
    driver_drain(node);
    driver_build_vote_response(2u, 1u, 2u, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    driver_drain(node);
    driver_build_ae(2u, 1u, 2u, 0u, 0u, 0u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    raft89_destroy(node);

    TEST_END;
}
