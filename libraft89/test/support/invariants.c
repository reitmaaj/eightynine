/* invariants.c - global Raft safety invariants over a simulated cluster.
 * See .agent/design/0000-design.md for I01..I20. */
#include <string.h>

#include "invariants.h"
#include "raft89_inspect.h"

static unsigned long inv_ul(raft89_u64 v)
{
    return (unsigned long)v.lo;
}

static const fake_store_entry *store_entry(const fake_store *store,
                                           unsigned long index)
{
    if (index == 0ul || index > store->entry_count)
    {
        return NULL;
    }
    return &store->entries[index - 1u];
}

static int live_status(const cluster_node *n, raft89_status *status)
{
    if (n->up == 0 || n->raft == NULL)
    {
        return -1;
    }
    if (raft89_status_get(n->raft, status) != RAFT89_OK)
    {
        return -1;
    }
    return 0;
}

static int is_member(const cluster *c, raft89_id id)
{
    raft89_size i;
    for (i = 0u; i < c->count; ++i)
    {
        if (c->nodes[i].f.config.self == id)
        {
            return 1;
        }
    }
    return 0;
}

static int entry_equal(const fake_store_entry *a, const fake_store_entry *b)
{
    if (!raft89_u64_equal(a->term, b->term))
    {
        return 0;
    }
    if (a->size != b->size)
    {
        return 0;
    }
    if (a->size == 0u)
    {
        return 1;
    }
    return memcmp(a->data, b->data, a->size) == 0;
}

static const char *check_leaders(const cluster *c)
{
    raft89_size i;
    raft89_size j;
    raft89_status si;
    raft89_status sj;
    for (i = 0u; i < c->count; ++i)
    {
        if (live_status(&c->nodes[i], &si) != 0)
        {
            continue;
        }
        if (si.role != RAFT89_LEADER)
        {
            continue;
        }
        for (j = i + 1u; j < c->count; ++j)
        {
            if (live_status(&c->nodes[j], &sj) != 0)
            {
                continue;
            }
            if (sj.role != RAFT89_LEADER)
            {
                continue;
            }
            if (raft89_u64_equal(si.current_term, sj.current_term))
            {
                return "I01: two leaders in one term";
            }
        }
    }
    return NULL;
}

static const char *check_votes(const cluster *c)
{
    raft89_size i;
    raft89_status s;
    const fake_store *si;
    for (i = 0u; i < c->count; ++i)
    {
        si = &c->nodes[i].f.store;
        if (si->hard.voted_for != RAFT89_ID_NONE &&
            is_member(c, si->hard.voted_for) == 0)
        {
            return "I04: voted_for names a non-member";
        }
        if (live_status(&c->nodes[i], &s) == 0)
        {
            if (s.voted_for != RAFT89_ID_NONE && is_member(c, s.voted_for) == 0)
            {
                return "I04: status voted_for names a non-member";
            }
        }
    }
    return NULL;
}

static const char *check_store_shape(const cluster *c)
{
    raft89_size i;
    unsigned long k;
    const fake_store *store;
    for (i = 0u; i < c->count; ++i)
    {
        store = &c->nodes[i].f.store;
        for (k = 0u; k < store->entry_count; ++k)
        {
            if (inv_ul(store->entries[k].index) != k + 1u)
            {
                return "I05: non-contiguous durable log";
            }
            if (raft89_u64_equal(store->entries[k].term, RAFT89_TERM_NONE))
            {
                return "I05: zero-term durable entry";
            }
        }
    }
    return NULL;
}

static const char *check_log_matching(const cluster *c)
{
    raft89_size i;
    raft89_size j;
    unsigned long n;
    unsigned long k;
    unsigned long m;
    const fake_store *a;
    const fake_store *b;
    for (i = 0u; i < c->count; ++i)
    {
        for (j = i + 1u; j < c->count; ++j)
        {
            a = &c->nodes[i].f.store;
            b = &c->nodes[j].f.store;
            n = a->entry_count;
            if (b->entry_count < n)
            {
                n = b->entry_count;
            }
            for (k = 0u; k < n; ++k)
            {
                if (!raft89_u64_equal(a->entries[k].term, b->entries[k].term))
                {
                    continue;
                }
                for (m = 0u; m <= k; ++m)
                {
                    if (entry_equal(&a->entries[m], &b->entries[m]) == 0)
                    {
                        return "I05: log matching";
                    }
                }
            }
        }
    }
    return NULL;
}

static const char *check_applies(const cluster *c)
{
    raft89_size i;
    raft89_size j;
    unsigned long k;
    unsigned long last;
    const fake_app_rec *ra;
    const fake_app_rec *rb;
    const fake_app *app;
    for (i = 0u; i < c->count; ++i)
    {
        for (j = i + 1u; j < c->count; ++j)
        {
            for (k = 0u; k < (unsigned long)FAKE_APP_SLOTS; ++k)
            {
                ra = &c->nodes[i].app.rec[k];
                if (ra->present == 0)
                {
                    continue;
                }
                rb = &c->nodes[j]
                          .app.rec[ra->index % (unsigned long)FAKE_APP_SLOTS];
                if (rb->present == 0)
                {
                    continue;
                }
                if (rb->index != ra->index)
                {
                    continue;
                }
                if (ra->term != rb->term || ra->hash != rb->hash)
                {
                    return "I08: divergent applies";
                }
            }
        }
    }
    for (i = 0u; i < c->count; ++i)
    {
        app = &c->nodes[i].app;
        last = app->last_applied;
        if (last > (unsigned long)FAKE_APP_SLOTS)
        {
            last = (unsigned long)FAKE_APP_SLOTS;
        }
        for (k = 1u; k <= last; ++k)
        {
            ra = &app->rec[k % (unsigned long)FAKE_APP_SLOTS];
            if (ra->present == 0 || ra->index != k)
            {
                return "I09: apply gap";
            }
        }
    }
    return NULL;
}

static const char *check_status_bounds(const cluster *c)
{
    raft89_size i;
    raft89_status s;
    raft89_inspect_view view;
    const fake_store *store;
    const fake_store_entry *e;
    const raft89_action *action;
    for (i = 0u; i < c->count; ++i)
    {
        if (live_status(&c->nodes[i], &s) != 0)
        {
            continue;
        }
        raft89_inspect_view_get(c->nodes[i].raft, &view);
        if (raft89_u64_cmp(s.applied_index, s.commit_index) > 0)
        {
            return "I16: applied beyond commit";
        }
        if (s.faulted != 0)
        {
            return "FAULT: node is faulted";
        }
        if (raft89_u64_cmp(s.commit_index, s.last_log_index) > 0)
        {
            return "I10: commit beyond last log index";
        }
        store = &c->nodes[i].f.store;
        if (raft89_u64_cmp(s.current_term, store->hard.current_term) > 0)
        {
            return "I14: acknowledged term leads durable term";
        }
        if (raft89_u64_equal(s.current_term, store->hard.current_term) &&
            s.voted_for != RAFT89_ID_NONE &&
            s.voted_for != store->hard.voted_for)
        {
            return "I14: acknowledged vote leads durable vote";
        }
        if (inv_ul(s.last_log_index) > store->entry_count)
        {
            action = raft89_inspect_action(c->nodes[i].raft);
            if (action == NULL || action->type != RAFT89_ACT_LOG_TRUNCATE)
            {
                return "I14: acknowledged log leads durable log";
            }
            if (store->entry_count <
                inv_ul(action->u.log_truncate.first_index) - 1u)
            {
                return "I14: truncate removed the acknowledged prefix";
            }
        }
        if (!raft89_u64_equal(s.last_log_index, RAFT89_INDEX_NONE))
        {
            e = store_entry(store, inv_ul(s.last_log_index));
            if (e != NULL && !raft89_u64_equal(e->term, view.last_log_term))
            {
                return "I14: acknowledged last entry mismatch";
            }
        }
    }
    return NULL;
}

static const char *check_leader_completeness(const cluster *c)
{
    raft89_size i;
    raft89_size j;
    raft89_status lead;
    raft89_status other;
    const fake_store_entry *le;
    const fake_store_entry *oe;
    for (i = 0u; i < c->count; ++i)
    {
        if (live_status(&c->nodes[i], &lead) != 0)
        {
            continue;
        }
        if (lead.role != RAFT89_LEADER)
        {
            continue;
        }
        for (j = 0u; j < c->count; ++j)
        {
            if (j == i)
            {
                continue;
            }
            if (live_status(&c->nodes[j], &other) != 0)
            {
                continue;
            }
            if (raft89_u64_equal(other.commit_index, RAFT89_INDEX_NONE))
            {
                continue;
            }
            if (raft89_u64_cmp(lead.current_term, other.current_term) < 0)
            {
                continue;
            }
            if (raft89_u64_cmp(lead.last_log_index, other.commit_index) < 0)
            {
                return "I07: leader missing a committed entry";
            }
            le = store_entry(&c->nodes[i].f.store, inv_ul(other.commit_index));
            oe = store_entry(&c->nodes[j].f.store, inv_ul(other.commit_index));
            if (le == NULL || oe == NULL || entry_equal(le, oe) == 0)
            {
                return "I07: leader committed entry mismatch";
            }
        }
    }
    return NULL;
}

static const char *check_success_before_durability(const cluster *c)
{
    raft89_size i;
    const raft89_action *action;
    const fake_store *store;
    for (i = 0u; i < c->count; ++i)
    {
        if (c->nodes[i].up == 0 || c->nodes[i].raft == NULL)
        {
            continue;
        }
        action = raft89_inspect_action(c->nodes[i].raft);
        if (action == NULL)
        {
            continue;
        }
        store = &c->nodes[i].f.store;
        if (action->type == RAFT89_ACT_SEND &&
            action->u.send.message.type == RAFT89_MSG_REQUEST_VOTE_RESPONSE &&
            action->u.send.message.u.request_vote_response.vote_granted != 0)
        {
            if (!raft89_u64_equal(
                    store->hard.current_term,
                    action->u.send.message.u.request_vote_response.term))
            {
                return "I15: vote response before durable term";
            }
            if (store->hard.voted_for == RAFT89_ID_NONE)
            {
                return "I15: granted vote before durable vote";
            }
        }
        if (action->type == RAFT89_ACT_SEND &&
            action->u.send.message.type == RAFT89_MSG_APPEND_ENTRIES_RESPONSE &&
            action->u.send.message.u.append_entries_response.success != 0)
        {
            if (store->entry_count <
                inv_ul(action->u.send.message.u.append_entries_response
                           .match_index))
            {
                return "I15: append response before durable log";
            }
        }
    }
    return NULL;
}

static const char *check_one_action(const cluster *c)
{
    raft89_size i;
    raft89_inspect_view view;
    for (i = 0u; i < c->count; ++i)
    {
        if (c->nodes[i].up == 0 || c->nodes[i].raft == NULL)
        {
            continue;
        }
        raft89_inspect_view_get(c->nodes[i].raft, &view);
        if (view.has_action != 0 && view.has_action != 1)
        {
            return "I17: action count is not a flag";
        }
        if ((c->nodes[i].oracle.phase == ORACLE_IDLE) != (view.has_action == 0))
        {
            return "I17: oracle phase and action disagree";
        }
    }
    return NULL;
}

const char *invariants_check(const cluster *c)
{
    const char *violation;
    violation = check_leaders(c);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_votes(c);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_store_shape(c);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_log_matching(c);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_applies(c);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_status_bounds(c);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_leader_completeness(c);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_success_before_durability(c);
    if (violation != NULL)
    {
        return violation;
    }
    return check_one_action(c);
}

static const char *check_term_monotonic(const cluster *prev,
                                        const cluster *next)
{
    raft89_size i;
    raft89_status sp;
    raft89_status sn;
    for (i = 0u; i < prev->count; ++i)
    {
        if (live_status(&prev->nodes[i], &sp) != 0)
        {
            continue;
        }
        if (live_status(&next->nodes[i], &sn) != 0)
        {
            continue;
        }
        if (raft89_u64_cmp(sn.current_term, sp.current_term) < 0)
        {
            return "I02: acknowledged term decreased";
        }
        if (raft89_u64_cmp(next->nodes[i].f.store.hard.current_term,
                           prev->nodes[i].f.store.hard.current_term) < 0)
        {
            return "I02: durable term decreased";
        }
    }
    return NULL;
}

static const char *check_commit_applied_monotonic(const cluster *prev,
                                                  const cluster *next)
{
    raft89_size i;
    raft89_status sp;
    raft89_status sn;
    for (i = 0u; i < prev->count; ++i)
    {
        if (live_status(&prev->nodes[i], &sp) != 0)
        {
            continue;
        }
        if (live_status(&next->nodes[i], &sn) != 0)
        {
            continue;
        }
        if (raft89_u64_cmp(sn.commit_index, sp.commit_index) < 0)
        {
            return "I11: commit_index decreased";
        }
        if (raft89_u64_cmp(sn.applied_index, sp.applied_index) < 0)
        {
            return "I12: applied_index decreased";
        }
    }
    return NULL;
}

static const char *check_leader_append_only(const cluster *prev,
                                            const cluster *next)
{
    raft89_size i;
    unsigned long k;
    raft89_status sp;
    raft89_status sn;
    const fake_store *a;
    const fake_store *b;
    for (i = 0u; i < prev->count; ++i)
    {
        if (live_status(&prev->nodes[i], &sp) != 0)
        {
            continue;
        }
        if (live_status(&next->nodes[i], &sn) != 0)
        {
            continue;
        }
        if (sp.role != RAFT89_LEADER || sn.role != RAFT89_LEADER)
        {
            continue;
        }
        if (!raft89_u64_equal(sp.current_term, sn.current_term))
        {
            continue;
        }
        a = &prev->nodes[i].f.store;
        b = &next->nodes[i].f.store;
        if (b->entry_count < a->entry_count)
        {
            return "I06: leader durable log shrank";
        }
        for (k = 0u; k < a->entry_count; ++k)
        {
            if (entry_equal(&a->entries[k], &b->entries[k]) == 0)
            {
                return "I06: leader durable log changed";
            }
        }
    }
    return NULL;
}

static const char *check_match_monotonic(const cluster *prev,
                                         const cluster *next)
{
    raft89_size i;
    raft89_size p;
    raft89_status sp;
    raft89_status sn;
    raft89_id idp;
    raft89_id idn;
    raft89_index nxp;
    raft89_index nxn;
    raft89_index mxp;
    raft89_index mxn;
    for (i = 0u; i < prev->count; ++i)
    {
        if (live_status(&prev->nodes[i], &sp) != 0)
        {
            continue;
        }
        if (live_status(&next->nodes[i], &sn) != 0)
        {
            continue;
        }
        if (sp.role != RAFT89_LEADER || sn.role != RAFT89_LEADER)
        {
            continue;
        }
        if (!raft89_u64_equal(sp.current_term, sn.current_term))
        {
            continue;
        }
        for (p = 0u; p < prev->count; ++p)
        {
            if (raft89_inspect_peer(prev->nodes[i].raft, p, &idp, &nxp, &mxp) !=
                0)
            {
                continue;
            }
            if (raft89_inspect_peer(next->nodes[i].raft, p, &idn, &nxn, &mxn) !=
                0)
            {
                continue;
            }
            if (idp != idn)
            {
                return "I19: peer order changed";
            }
            if (raft89_u64_cmp(mxp, mxn) > 0)
            {
                return "I19/I20: match_index regressed";
            }
        }
    }
    return NULL;
}

static const char *check_vote_stability(const cluster *prev,
                                        const cluster *next)
{
    raft89_size i;
    const fake_store *a;
    const fake_store *b;
    for (i = 0u; i < prev->count; ++i)
    {
        a = &prev->nodes[i].f.store;
        b = &next->nodes[i].f.store;
        if (!raft89_u64_equal(a->hard.current_term, b->hard.current_term))
        {
            continue;
        }
        if (a->hard.voted_for == RAFT89_ID_NONE)
        {
            continue;
        }
        if (b->hard.voted_for != a->hard.voted_for)
        {
            return "I03: vote changed within one term";
        }
    }
    return NULL;
}

static const char *check_commit_advance(const cluster *prev,
                                        const cluster *next)
{
    raft89_size i;
    raft89_size p;
    unsigned long votes;
    unsigned long quorum;
    raft89_status sp;
    raft89_status sn;
    raft89_id id;
    raft89_index nx;
    raft89_index mx;
    const fake_store_entry *e;
    quorum = (unsigned long)prev->count / 2u + 1u;
    for (i = 0u; i < prev->count; ++i)
    {
        if (live_status(&prev->nodes[i], &sp) != 0)
        {
            continue;
        }
        if (live_status(&next->nodes[i], &sn) != 0)
        {
            continue;
        }
        if (raft89_u64_cmp(sn.commit_index, sp.commit_index) <= 0)
        {
            continue;
        }
        e = store_entry(&next->nodes[i].f.store, inv_ul(sn.commit_index));
        if (e == NULL)
        {
            return "I13: commit beyond the durable log";
        }
        if (sp.role != RAFT89_LEADER || sn.role != RAFT89_LEADER)
        {
            continue;
        }
        if (!raft89_u64_equal(sp.current_term, sn.current_term))
        {
            continue;
        }
        if (!raft89_u64_equal(e->term, sn.current_term))
        {
            return "I13: committed an old-term entry directly";
        }
        votes = 1u;
        for (p = 0u; p < prev->count; ++p)
        {
            if (raft89_inspect_peer(next->nodes[i].raft, p, &id, &nx, &mx) != 0)
            {
                continue;
            }
            if (raft89_u64_cmp(mx, sn.commit_index) >= 0)
            {
                ++votes;
            }
        }
        if (votes < quorum)
        {
            return "I13: commit without a current-term quorum";
        }
    }
    return NULL;
}

const char *invariants_check_transition(const cluster *prev,
                                        const cluster *next)
{
    const char *violation;
    violation = check_term_monotonic(prev, next);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_vote_stability(prev, next);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_commit_applied_monotonic(prev, next);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_leader_append_only(prev, next);
    if (violation != NULL)
    {
        return violation;
    }
    violation = check_match_monotonic(prev, next);
    if (violation != NULL)
    {
        return violation;
    }
    return check_commit_advance(prev, next);
}
