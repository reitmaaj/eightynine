/* raft_adapter_main.c - adapter suite composing libraft89 and libledger89.
 * Built by `just adapters-raft` against the sibling libraft89 tree; not
 * part of the library or of the default test suite.
 *
 * The adapter maps one ledger record to one Raft entry:
 *
 *     [ 8-byte canonical term ][ application payload ]
 *
 * and demonstrates the exact partial-read boundary: the term prefix is
 * read with ledger89_read_at(index, 0, 8) and the payload with
 * ledger89_read_at(index, 8, size - 8). */

#include <stdio.h>
#include <string.h>

#include <raft89.h>

#include "fixture.h"
#include "test.h"

#define ADAPTER_MAX_ENTRIES 8u
#define ADAPTER_MAX_PAYLOAD 4096u
#define ADAPTER_ENVELOPE 8u

static ledger89 *adapter_ledger;
static raft89_hard_state adapter_hard;

/* ------------------------------------------------------------------ */
/* scalar helpers                                                      */
/* ------------------------------------------------------------------ */

static raft89_u64 r_u64(unsigned long v)
{
    return raft89_u64_from_u32((raft89_u32)v);
}

static ledger89_u64 l_u64(unsigned long v)
{
    return ledger89_u64_from_u32((ledger89_u32)v);
}

/* Canonical little-endian term envelope: lo word, then hi word. */
static void put_term(unsigned char *out, raft89_term term)
{
    unsigned int i;

    for (i = 0u; i < 4u; ++i)
    {
        out[i] = (unsigned char)((term.lo >> (8u * i)) & 0xFFu);
        out[4u + i] = (unsigned char)((term.hi >> (8u * i)) & 0xFFu);
    }
}

static raft89_term get_term(const unsigned char *in)
{
    raft89_term term;
    unsigned int i;

    term.hi = 0u;
    term.lo = 0u;
    for (i = 0u; i < 4u; ++i)
    {
        term.lo |= ((raft89_u32)in[i]) << (8u * i);
        term.hi |= ((raft89_u32)in[4u + i]) << (8u * i);
    }
    return term;
}

static raft89_index index_from_ledger(ledger89_u64 v)
{
    raft89_index out;

    out.hi = v.hi;
    out.lo = v.lo;
    return out;
}

static ledger89_u64 index_to_ledger(raft89_index v)
{
    ledger89_u64 out;

    out.hi = v.hi;
    out.lo = v.lo;
    return out;
}

static ledger89_u64 ledger_dec(ledger89_u64 v)
{
    if (v.lo > 0u)
    {
        v.lo = v.lo - 1u;
    }
    else
    {
        v.lo = 0xffffffffu;
        v.hi = v.hi - 1u;
    }
    return v;
}

/* ------------------------------------------------------------------ */
/* store callbacks                                                     */
/* ------------------------------------------------------------------ */

static int store_hard_state(void *ctx, raft89_hard_state *state)
{
    (void)ctx;
    *state = adapter_hard;
    return RAFT89_OK;
}

static int store_log_last(void *ctx, raft89_index *index, raft89_term *term)
{
    ledger89_state st;
    unsigned char buf[ADAPTER_ENVELOPE];

    (void)ctx;
    if (ledger89_get_state(adapter_ledger, &st) != LEDGER89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    if (st.end.hi == 0u && st.end.lo == 1u)
    {
        *index = raft89_u64_zero();
        *term = raft89_u64_zero();
        return RAFT89_OK;
    }
    *index = index_from_ledger(ledger_dec(st.end));
    if (ledger89_read_at(adapter_ledger, index_to_ledger(*index), 0u, buf,
                         sizeof buf) != LEDGER89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    *term = get_term(buf);
    return RAFT89_OK;
}

static int store_log_term(void *ctx, raft89_index index, raft89_term *term)
{
    unsigned char buf[ADAPTER_ENVELOPE];

    (void)ctx;
    if (ledger89_read_at(adapter_ledger, index_to_ledger(index), 0u, buf,
                         sizeof buf) != LEDGER89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    *term = get_term(buf);
    return RAFT89_OK;
}

static int store_log_size(void *ctx, raft89_index index, raft89_size *size)
{
    size_t got;

    (void)ctx;
    if (ledger89_read(adapter_ledger, index_to_ledger(index), NULL, 0u, &got) !=
        LEDGER89_OK)
    {
        return RAFT89_ERR_STORE;
    }
    if (got < (size_t)ADAPTER_ENVELOPE)
    {
        return RAFT89_ERR_STORE;
    }
    *size = (raft89_size)(got - (size_t)ADAPTER_ENVELOPE);
    return RAFT89_OK;
}

static int store_log_read(void *ctx, raft89_index index, void *data,
                          raft89_size size)
{
    (void)ctx;
    if (size == 0u)
    {
        return RAFT89_OK;
    }
    if (ledger89_read_at(adapter_ledger, index_to_ledger(index),
                         (size_t)ADAPTER_ENVELOPE, data,
                         (size_t)size) != LEDGER89_OK)
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

/* ------------------------------------------------------------------ */
/* host effects                                                        */
/* ------------------------------------------------------------------ */

static int handle_log_append(const raft89_action *action)
{
    ledger89_state st;
    ledger89_slice slices[ADAPTER_MAX_ENTRIES];
    unsigned char envelopes[ADAPTER_MAX_ENTRIES]
                           [ADAPTER_ENVELOPE + ADAPTER_MAX_PAYLOAD];
    raft89_size i;

    if (ledger89_get_state(adapter_ledger, &st) != LEDGER89_OK)
    {
        return RAFT89_ACTION_FATAL;
    }
    if (action->u.log_append.entry_count > ADAPTER_MAX_ENTRIES)
    {
        return RAFT89_ACTION_FATAL;
    }
    for (i = 0u; i < action->u.log_append.entry_count; ++i)
    {
        const raft89_entry *e;
        size_t n;

        e = &action->u.log_append.entries[i];
        n = (size_t)e->size;
        if (n > (size_t)ADAPTER_MAX_PAYLOAD)
        {
            return RAFT89_ACTION_FATAL;
        }
        put_term(envelopes[i], e->term);
        if (n > 0u)
        {
            memcpy(envelopes[i] + ADAPTER_ENVELOPE, e->data, n);
        }
        slices[i].data = envelopes[i];
        slices[i].size = n + (size_t)ADAPTER_ENVELOPE;
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

static int handle_log_truncate(const raft89_action *action)
{
    ledger89_state st;

    if (ledger89_get_state(adapter_ledger, &st) != LEDGER89_OK)
    {
        return RAFT89_ACTION_FATAL;
    }
    if (ledger89_u64_equal(st.end, st.stable_end) == 0)
    {
        if (ledger89_sync(adapter_ledger, NULL) != LEDGER89_OK)
        {
            return RAFT89_ACTION_FATAL;
        }
    }
    if (ledger89_truncate_from(
            adapter_ledger,
            index_to_ledger(action->u.log_truncate.first_index)) != LEDGER89_OK)
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
            (void)raft89_action_done(node, action->id,
                                     handle_log_truncate(action));
        }
        else
        {
            (void)raft89_action_done(node, action->id, RAFT89_ACTION_OK);
        }
    }
}

/* ------------------------------------------------------------------ */
/* suite fixture                                                       */
/* ------------------------------------------------------------------ */

static raft89 *start_node(fx *f)
{
    raft89_config cfg;
    raft89 *node;
    raft89_id members[1];

    adapter_ledger = f->l;
    adapter_hard.current_term = raft89_u64_zero();
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
    cfg.max_append_bytes = 8192ul;
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
        return NULL;
    }
    drain(node);
    CHECK_EQ(raft89_tick(node, 100ul), RAFT89_OK);
    drain(node);
    return node;
}

static int read_at_eq(ledger89 *l, unsigned long index, size_t offset,
                      const void *expect, size_t size)
{
    unsigned char buf[64];
    int ok;

    if (size > sizeof buf)
    {
        return 0;
    }
    if (ledger89_read_at(l, l_u64(index), offset, buf, size) != LEDGER89_OK)
    {
        return 0;
    }
    ok = memcmp(buf, expect, size) == 0;
    return ok;
}

/* ------------------------------------------------------------------ */
/* tests                                                               */
/* ------------------------------------------------------------------ */

static void test_envelope_boundary(void)
{
    fx f;
    raft89 *node;
    raft89_index proposed;
    raft89_term term;
    raft89_size psize;
    unsigned char payload[16];
    ledger89_state st;
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    node = start_node(&f);
    if (node == NULL)
    {
        fx_close(&f);
        return;
    }
    proposed = raft89_u64_zero();
    rc = raft89_propose(node, "cmd", 3u, &proposed);
    CHECK_EQ(rc, RAFT89_OK);
    drain(node);

    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    CHECK_U64(st.stable_end, st.end);

    /* log_term uses only bytes 0..7 and returns the exact term. */
    CHECK_EQ(store_log_term(NULL, r_u64(1u), &term), RAFT89_OK);
    CHECK(raft89_u64_equal(term, r_u64(1u)));

    /* log_size excludes the envelope. */
    psize = 999u;
    CHECK_EQ(store_log_size(NULL, r_u64(1u), &psize), RAFT89_OK);
    CHECK_EQ(psize, 3u);

    /* log_read returns the payload, never the term prefix. */
    memset(payload, 0, sizeof payload);
    CHECK_EQ(store_log_read(NULL, r_u64(1u), payload, psize), RAFT89_OK);
    CHECK(memcmp(payload, "cmd", 3u) == 0);

    /* The stored record itself is [8-byte term][payload]. */
    rc = ledger89_read(f.l, l_u64(1u), payload, sizeof payload, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 11u);
    CHECK(memcmp(payload + 8u, "cmd", 3u) == 0);
    CHECK(read_at_eq(f.l, 1ul, 8u, "cmd", 3u));

    raft89_destroy(node);
    fx_close(&f);
}

static void test_empty_payload(void)
{
    fx f;
    raft89 *node;
    raft89_size psize;
    size_t size;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    node = start_node(&f);
    if (node == NULL)
    {
        fx_close(&f);
        return;
    }
    rc = raft89_propose(node, NULL, 0u, NULL);
    CHECK_EQ(rc, RAFT89_OK);
    drain(node);

    psize = 999u;
    CHECK_EQ(store_log_size(NULL, r_u64(1u), &psize), RAFT89_OK);
    CHECK_EQ(psize, 0u);
    CHECK_EQ(store_log_read(NULL, r_u64(1u), NULL, 0u), RAFT89_OK);
    rc = ledger89_read(f.l, l_u64(1u), NULL, 0u, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 8u);

    raft89_destroy(node);
    fx_close(&f);
}

static void test_large_payload(void)
{
    static unsigned char big[ADAPTER_MAX_PAYLOAD];
    fx f;
    raft89 *node;
    raft89_size psize;
    unsigned char head[8];
    size_t i;

    for (i = 0u; i < sizeof big; ++i)
    {
        big[i] = (unsigned char)(i & 0xffu);
    }
    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    node = start_node(&f);
    if (node == NULL)
    {
        fx_close(&f);
        return;
    }
    CHECK_EQ(raft89_propose(node, big, (raft89_size)sizeof big, NULL),
             RAFT89_OK);
    drain(node);

    /* The term read needs only a fixed 8-byte destination. */
    psize = 0u;
    CHECK_EQ(store_log_size(NULL, r_u64(1u), &psize), RAFT89_OK);
    CHECK_EQ(psize, (raft89_size)sizeof big);
    CHECK_EQ(store_log_read(NULL, r_u64(1u), head, 8u), RAFT89_OK);
    CHECK(memcmp(head, big, 8u) == 0);
    CHECK(read_at_eq(f.l, 1ul, 8u + 1000u, big + 1000u, 16u));

    raft89_destroy(node);
    fx_close(&f);
}

static void test_batch_mapping(void)
{
    fx f;
    raft89 *node;
    raft89_command commands[3];
    raft89_index first;
    ledger89_state st;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    node = start_node(&f);
    if (node == NULL)
    {
        fx_close(&f);
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
    CHECK(raft89_u64_equal(first, r_u64(1u)));
    drain(node);

    /* One LOG_APPEND action produced three ledger records. */
    CHECK_EQ(ledger89_get_state(f.l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(4u));
    CHECK(read_at_eq(f.l, 1ul, 8u, "A", 1u));
    CHECK(read_at_eq(f.l, 2ul, 8u, "BB", 2u));
    CHECK(read_at_eq(f.l, 3ul, 8u, "CCC", 3u));

    raft89_destroy(node);
    fx_close(&f);
}

static void test_64bit_term(void)
{
    fx f;
    raft89 *node;
    raft89_action action;
    raft89_entry entry;
    raft89_term term;
    unsigned char payload[4];

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    node = start_node(&f);
    if (node == NULL)
    {
        fx_close(&f);
        return;
    }

    /* Store one record whose term is 2^32 + 5 through the adapter. */
    memset(&action, 0, sizeof action);
    memset(&entry, 0, sizeof entry);
    entry.term.hi = 1u;
    entry.term.lo = 5u;
    entry.index = r_u64(1u);
    entry.data = "z";
    entry.size = 1u;
    action.type = RAFT89_ACT_LOG_APPEND;
    action.u.log_append.entries = &entry;
    action.u.log_append.entry_count = 1u;
    CHECK_EQ(handle_log_append(&action), RAFT89_ACTION_OK);
    CHECK_EQ(store_log_term(NULL, r_u64(1u), &term), RAFT89_OK);
    CHECK(raft89_u64_equal(term, entry.term));
    CHECK_EQ(store_log_read(NULL, r_u64(1u), payload, 1u), RAFT89_OK);
    CHECK_EQ(payload[0], (unsigned char)'z');

    raft89_destroy(node);
    fx_close(&f);
}

static void test_index_conversion(void)
{
    raft89_index r;
    ledger89_u64 l;

    r.hi = 0xffffffffu;
    r.lo = 0xffffffffu;
    l = index_to_ledger(r);
    CHECK_EQ(l.hi, 0xffffffffu);
    CHECK_EQ(l.lo, 0xffffffffu);
    r = index_from_ledger(l);
    CHECK_EQ(r.hi, 0xffffffffu);
    CHECK_EQ(r.lo, 0xffffffffu);
    l.hi = 1u;
    l.lo = 0u;
    r = index_from_ledger(l);
    CHECK_EQ(r.hi, 1u);
    CHECK_EQ(r.lo, 0u);
}

int main(void)
{
    test_envelope_boundary();
    test_empty_payload();
    test_large_payload();
    test_batch_mapping();
    test_64bit_term();
    test_index_conversion();
    TEST_END;
}
