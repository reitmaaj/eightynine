/* raft_adapter_main.c - compiling adapter fixture composing libraft89 and
 * libledger89. Built by `just adapters-raft` against the sibling libraft89
 * tree; not part of the library or of the default test suite. */

#include <stdio.h>
#include <string.h>

#include <raft89.h>

#include "fixture.h"
#include "test.h"

#define ADAPTER_MAX_ENTRY 256u

static ledger89 *adapter_ledger;
static raft89_hard_state adapter_hard;

static void put_u64(unsigned char *out, unsigned long v)
{
    unsigned int i;

    for (i = 0u; i < 8u; ++i)
    {
        out[i] = (unsigned char)((v >> (8u * i)) & 0xFFul);
    }
}

static unsigned long get_u64(const unsigned char *in)
{
    unsigned long v;
    unsigned int i;

    v = 0ul;
    for (i = 0u; i < 8u; ++i)
    {
        v |= ((unsigned long)in[i]) << (8u * i);
    }
    return v;
}

static int store_hard_state(void *ctx, raft89_hard_state *state)
{
    (void)ctx;
    *state = adapter_hard;
    return RAFT89_OK;
}

static int store_log_last(void *ctx, raft89_index *index, raft89_term *term)
{
    ledger89_state st;
    unsigned char buf[8];
    size_t size;

    (void)ctx;
    if (ledger89_get_state(adapter_ledger, &st) != LEDGER89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    if (st.end.hi == 0u && st.end.lo == 1u)
    {
        *index = 0ul;
        *term = 0ul;
        return RAFT89_OK;
    }
    *index = (raft89_index)st.end.lo - 1ul;
    if (ledger89_read(adapter_ledger,
                      ledger89_u64_from_u32((ledger89_u32)*index), buf,
                      sizeof buf, &size) != LEDGER89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    *term = get_u64(buf);
    return RAFT89_OK;
}

static int store_log_term(void *ctx, raft89_index index, raft89_term *term)
{
    unsigned char buf[8];
    size_t size;

    (void)ctx;
    if (ledger89_read(adapter_ledger,
                      ledger89_u64_from_u32((ledger89_u32)index), buf,
                      sizeof buf, &size) != LEDGER89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    *term = get_u64(buf);
    return RAFT89_OK;
}

static int store_log_size(void *ctx, raft89_index index, raft89_size *size)
{
    size_t got;

    (void)ctx;
    if (ledger89_read(adapter_ledger,
                      ledger89_u64_from_u32((ledger89_u32)index), NULL, 0u,
                      &got) != LEDGER89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    *size = (raft89_size)got;
    return RAFT89_OK;
}

static int store_log_read(void *ctx, raft89_index index, void *data,
                          raft89_size size)
{
    size_t got;

    (void)ctx;
    if (ledger89_read(adapter_ledger,
                      ledger89_u64_from_u32((ledger89_u32)index), data,
                      (size_t)size, &got) != LEDGER89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    return RAFT89_OK;
}

static unsigned long adapter_random(void *ctx, unsigned long upper)
{
    (void)ctx;
    return upper / 2ul;
}

static int handle_log_append(const raft89_action *action)
{
    ledger89_state st;
    ledger89_slice slices[8];
    unsigned char envelopes[8][ADAPTER_MAX_ENTRY];
    raft89_size i;

    if (ledger89_get_state(adapter_ledger, &st) != LEDGER89_OK)
    {
        return RAFT89_ACTION_FATAL;
    }
    if (action->u.log_append.entry_count > 8u)
    {
        return RAFT89_ACTION_FATAL;
    }
    for (i = 0u; i < action->u.log_append.entry_count; ++i)
    {
        const raft89_entry *e;
        size_t n;

        e = &action->u.log_append.entries[i];
        n = (size_t)e->size;
        if (n + 8u > ADAPTER_MAX_ENTRY)
        {
            return RAFT89_ACTION_FATAL;
        }
        put_u64(envelopes[i], (unsigned long)e->term);
        if (n > 0u)
        {
            memcpy(envelopes[i] + 8u, e->data, n);
        }
        slices[i].data = envelopes[i];
        slices[i].size = n + 8u;
    }
    if (ledger89_appendv_at(adapter_ledger, st.revision, st.end, slices,
                            (size_t)action->u.log_append.entry_count,
                            NULL) != LEDGER89_OK)
    {
        return RAFT89_ACTION_FATAL;
    }
    if (ledger89_sync(adapter_ledger, NULL) != LEDGER89_OK)
    {
        return RAFT89_ACTION_FATAL;
    }
    return RAFT89_ACTION_OK;
}

static void drain(raft89 *node)
{
    const raft89_action *action;
    int rc;

    for (;;)
    {
        rc = raft89_next_action(node, &action);
        if (rc != RAFT89_OK)
        {
            return;
        }
        if (action->type == RAFT89_ACT_HARD_STATE)
        {
            adapter_hard = action->u.hard_state.state;
            (void)raft89_action_done(node, action->id, RAFT89_ACTION_OK);
        }
        else if (action->type == RAFT89_ACT_LOG_APPEND)
        {
            (void)raft89_action_done(node, action->id,
                                     handle_log_append(action));
        }
        else if (action->type == RAFT89_ACT_LOG_TRUNCATE)
        {
            ledger89_state st;
            int ok;

            ok = RAFT89_ACTION_FATAL;
            if (ledger89_get_state(adapter_ledger, &st) == LEDGER89_OK)
            {
                if (ledger89_u64_equal(st.end, st.stable_end) == 0)
                {
                    (void)ledger89_sync(adapter_ledger, NULL);
                }
                if (ledger89_truncate_from(
                        adapter_ledger, ledger89_u64_from_u32(
                                            (ledger89_u32)action->u.log_truncate
                                                .first_index)) == LEDGER89_OK)
                {
                    ok = RAFT89_ACTION_OK;
                }
            }
            (void)raft89_action_done(node, action->id, ok);
        }
        else
        {
            (void)raft89_action_done(node, action->id, RAFT89_ACTION_OK);
        }
    }
}

int main(void)
{
    fx f;
    raft89_config cfg;
    raft89 *node;
    raft89_id members[1];
    raft89_index proposed;
    unsigned char buf[64];
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    adapter_ledger = f.l;
    adapter_hard.current_term = 0ul;
    adapter_hard.voted_for = RAFT89_ID_NONE;

    members[0] = 1ul;
    memset(&cfg, 0, sizeof cfg);
    cfg.self = 1ul;
    cfg.members = members;
    cfg.member_count = 1ul;
    cfg.heartbeat_interval = 1ul;
    cfg.election_timeout_min = 10ul;
    cfg.election_timeout_max = 20ul;
    cfg.max_append_entries = 8ul;
    cfg.max_append_bytes = 1024ul;
    cfg.store.ctx = NULL;
    cfg.store.hard_state = store_hard_state;
    cfg.store.log_last = store_log_last;
    cfg.store.log_term = store_log_term;
    cfg.store.log_size = store_log_size;
    cfg.store.log_read = store_log_read;
    cfg.random.ctx = NULL;
    cfg.random.next = adapter_random;

    node = NULL;
    CHECK_EQ(raft89_create(&cfg, &node), RAFT89_OK);
    if (node == NULL)
    {
        return 1;
    }
    drain(node);
    CHECK_EQ(raft89_tick(node, 100ul), RAFT89_OK);
    drain(node);

    proposed = 0ul;
    rc = raft89_propose(node, "cmd", 3u, &proposed);
    CHECK_EQ(rc, RAFT89_OK);
    drain(node);

    /* The committed command must be in the ledger with a term envelope. */
    {
        ledger89_state st;
        raft89_status status;

        CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
        CHECK_U64(st.stable_end, st.end);
        CHECK(st.end.lo >= 2u);
        rc = ledger89_read(f.l, ledger89_u64_from_u32(1u), buf, sizeof buf,
                           &size);
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK_EQ(size, 11u);
        CHECK(memcmp(buf + 8u, "cmd", 3u) == 0);
        CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
        CHECK_U64(test_u64((unsigned long)st.end.lo - 1ul),
                  test_u64((unsigned long)status.last_log_index));
    }
    raft89_destroy(node);
    fx_close(&f);
    TEST_END;
}
