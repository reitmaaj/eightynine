/* test_model3.c - bounded exhaustive model checker for a three-node
 * cluster running the real library. See
 * .agent/design/0005-model-checker.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "invariants.h"
#include "raft89_inspect.h"
#include "sim_event.h"

#define MODEL_NODES 3

#ifndef MODEL_MAX_TERM
#define MODEL_MAX_TERM 3ul
#endif
#ifndef MODEL_MAX_INDEX
#define MODEL_MAX_INDEX 3ul
#endif
#ifndef MODEL_MAX_PACKETS
#define MODEL_MAX_PACKETS 6ul
#endif
#ifndef MODEL_MAX_CRASHES
#define MODEL_MAX_CRASHES 2ul
#endif
#ifndef MODEL_MAX_DEPTH
#define MODEL_MAX_DEPTH 6ul
#endif
#ifndef MODEL_STATE_CAP
#define MODEL_STATE_CAP 500000ul
#endif
#ifndef MODEL_ENV
#define MODEL_ENV 1ul
#endif
#ifndef MODEL_SEEDS
#define MODEL_SEEDS 4ul
#endif

#define MODEL_KEY_CAP 65536ul
#define MODEL_NODE_CAP 16384ul
#define MODEL_MSG_CAP 4096ul

typedef struct model_writer
{
    unsigned char *buf;
    unsigned long cap;
    unsigned long len;
    int overflow;
} model_writer;

static unsigned long *visit_h1;
static unsigned long *visit_h2;
static unsigned char *visit_used;
static unsigned long visit_size;
static unsigned long visit_mask;

static unsigned char key_buf[MODEL_KEY_CAP];
static unsigned char node_buf[MODEL_NODE_CAP];
static unsigned char msg_buf[MODEL_MSG_CAP];

static sim_event path[MODEL_MAX_DEPTH + 1ul];
static unsigned long states;
static unsigned long edges;
static unsigned long max_depth;
static unsigned long cover_leaders;
static unsigned long cover_logs;
static unsigned long cover_commits;
static unsigned long cover_applies;
static unsigned long cover_down;
static unsigned long cover_packets;
static unsigned long count_events[SIM_EV_TYPE_COUNT];
static unsigned long current_seed;
static int cap_hit;
static int strict_mode;

static int visit_init(unsigned long cap)
{
    unsigned long size;
    size = 1024ul;
    while (size < cap * 4ul)
    {
        size = size * 2ul;
    }
    visit_size = size;
    visit_mask = size - 1ul;
    visit_h1 = (unsigned long *)calloc(size, sizeof(unsigned long));
    visit_h2 = (unsigned long *)calloc(size, sizeof(unsigned long));
    visit_used = (unsigned char *)calloc(size, sizeof(unsigned char));
    if (visit_h1 == NULL || visit_h2 == NULL || visit_used == NULL)
    {
        return -1;
    }
    return 0;
}

static void visit_free(void)
{
    free(visit_h1);
    free(visit_h2);
    free(visit_used);
    visit_h1 = NULL;
    visit_h2 = NULL;
    visit_used = NULL;
}

static void w_u8(model_writer *w, unsigned long value)
{
    if (w->len >= w->cap)
    {
        w->overflow = 1;
        return;
    }
    w->buf[w->len] = (unsigned char)(value & 0xFFul);
    ++w->len;
}

static void w_ulong(model_writer *w, unsigned long value)
{
    unsigned char bytes[sizeof(unsigned long)];
    unsigned long width;
    unsigned long tmp;
    unsigned long i;
    width = (unsigned long)sizeof(unsigned long);
    tmp = value;
    for (i = 0ul; i < width; ++i)
    {
        bytes[i] = (unsigned char)(tmp & 0xFFul);
        tmp = tmp >> 8;
    }
    for (i = width; i > 0ul; --i)
    {
        w_u8(w, (unsigned long)bytes[i - 1ul]);
    }
}

static unsigned long model_ul(raft89_u64 v)
{
    return (unsigned long)v.lo;
}

static void w_u64(model_writer *w, raft89_u64 value)
{
    w_ulong(w, (unsigned long)value.hi);
    w_ulong(w, (unsigned long)value.lo);
}

static void w_bytes(model_writer *w, const void *data, unsigned long size)
{
    const unsigned char *bytes;
    unsigned long i;
    if (size == 0ul)
    {
        return;
    }
    if (data == NULL)
    {
        w->overflow = 1;
        return;
    }
    bytes = (const unsigned char *)data;
    for (i = 0ul; i < size; ++i)
    {
        w_u8(w, (unsigned long)bytes[i]);
    }
}

static void hash_key(const unsigned char *key, unsigned long len,
                     unsigned long *h1, unsigned long *h2)
{
    unsigned long a;
    unsigned long b;
    unsigned long i;
    a = 2166136261ul;
    b = 5381ul;
    for (i = 0ul; i < len; ++i)
    {
        a = (a ^ (unsigned long)key[i]) * 16777619ul;
        b = b * 33ul + (unsigned long)key[i];
    }
    *h1 = a;
    *h2 = b;
}

static int visited_insert(unsigned long h1, unsigned long h2)
{
    unsigned long i;
    i = h1 & visit_mask;
    for (;;)
    {
        if (visit_used[i] == 0)
        {
            visit_used[i] = 1;
            visit_h1[i] = h1;
            visit_h2[i] = h2;
            return 1;
        }
        if (visit_h1[i] == h1 && visit_h2[i] == h2)
        {
            return 0;
        }
        i = (i + 1ul) & visit_mask;
    }
}

static int model_key(const cluster *c, unsigned char *buf, unsigned long cap,
                     unsigned long *len)
{
    model_writer w;
    const cluster_node *n;
    const packet *p;
    const fake_app_rec *rec;
    unsigned long i;
    unsigned long k;
    unsigned long mlen;
    w.buf = buf;
    w.cap = cap;
    w.len = 0ul;
    w.overflow = 0;
    w_ulong(&w, (unsigned long)c->count);
    for (i = 0ul; i < (unsigned long)c->count; ++i)
    {
        n = &c->nodes[i];
        w_u8(&w, (unsigned long)n->up);
        w_ulong(&w, n->crashes);
        w_ulong(&w, (unsigned long)n->oracle.phase);
        w_u64(&w, n->f.store.hard.current_term);
        w_ulong(&w, n->f.store.hard.voted_for);
        w_ulong(&w, n->f.store.entry_count);
        for (k = 0ul; k < n->f.store.entry_count; ++k)
        {
            w_u64(&w, n->f.store.entries[k].term);
            w_u64(&w, n->f.store.entries[k].index);
            w_ulong(&w, n->f.store.entries[k].size);
            w_bytes(&w, n->f.store.entries[k].data, n->f.store.entries[k].size);
        }
        w_ulong(&w, n->app.value);
        w_ulong(&w, n->app.last_applied);
        for (k = 1ul; k <= (unsigned long)n->app.last_applied &&
                      k <= (unsigned long)FAKE_APP_SLOTS;
             ++k)
        {
            rec = &n->app.rec[k % (unsigned long)FAKE_APP_SLOTS];
            w_u8(&w, (unsigned long)rec->present);
            if (rec->present != 0)
            {
                w_ulong(&w, rec->index);
                w_ulong(&w, rec->term);
                w_ulong(&w, rec->hash);
            }
        }
        if (n->up != 0 && n->raft != NULL)
        {
            if (raft89_inspect_snapshot(n->raft, node_buf, MODEL_NODE_CAP,
                                        &mlen) != RAFT89_OK)
            {
                return RAFT89_ERR_LIMIT;
            }
            w_bytes(&w, node_buf, mlen);
        }
    }
    w_ulong(&w, c->queue_count);
    for (i = 0ul; i < c->queue_count; ++i)
    {
        p = &c->queue[i];
        w_ulong(&w, p->from);
        w_ulong(&w, p->to);
        if (raft89_inspect_message_snapshot(&p->msg, msg_buf, MODEL_MSG_CAP,
                                            &mlen) != RAFT89_OK)
        {
            return RAFT89_ERR_LIMIT;
        }
        w_bytes(&w, msg_buf, mlen);
    }
    for (i = 1ul; i <= (unsigned long)c->count; ++i)
    {
        for (k = 1ul; k <= (unsigned long)c->count; ++k)
        {
            w_u8(&w, (unsigned long)c->link[i][k]);
        }
    }
    *len = w.len;
    if (w.overflow != 0)
    {
        return RAFT89_ERR_LIMIT;
    }
    return RAFT89_OK;
}

static int prune(const cluster *c)
{
    const cluster_node *n;
    const raft89_action *action;
    raft89_status s;
    raft89_inspect_view v;
    unsigned long i;
    unsigned long k;
    if (c->queue_count > MODEL_MAX_PACKETS)
    {
        return 1;
    }
    for (i = 0ul; i < (unsigned long)c->count; ++i)
    {
        n = &c->nodes[i];
        if (model_ul(n->f.store.hard.current_term) > MODEL_MAX_TERM)
        {
            return 1;
        }
        if (n->f.store.entry_count > MODEL_MAX_INDEX)
        {
            return 1;
        }
        if (n->crashes > MODEL_MAX_CRASHES)
        {
            return 1;
        }
        if (n->up == 0 || n->raft == NULL)
        {
            continue;
        }
        if (raft89_status_get(n->raft, &s) != RAFT89_OK)
        {
            return 1;
        }
        if (model_ul(s.current_term) > MODEL_MAX_TERM)
        {
            return 1;
        }
        if (model_ul(s.last_log_index) > MODEL_MAX_INDEX)
        {
            return 1;
        }
        if (model_ul(s.commit_index) > MODEL_MAX_INDEX)
        {
            return 1;
        }
        if (model_ul(s.applied_index) > MODEL_MAX_INDEX)
        {
            return 1;
        }
        raft89_inspect_view_get(n->raft, &v);
        if (model_ul(v.last_log_index) > MODEL_MAX_INDEX)
        {
            return 1;
        }
        action = raft89_inspect_action(n->raft);
        if (action != NULL && action->type == RAFT89_ACT_LOG_APPEND)
        {
            for (k = 0ul; k < (unsigned long)action->u.log_append.entry_count;
                 ++k)
            {
                if (model_ul(action->u.log_append.entries[k].index) >
                    MODEL_MAX_INDEX)
                {
                    return 1;
                }
            }
        }
        if (action != NULL && action->type == RAFT89_ACT_APPLY)
        {
            if (model_ul(action->u.apply.entry.index) > MODEL_MAX_INDEX)
            {
                return 1;
            }
        }
    }
    return 0;
}

static void coverage_update(const cluster *c)
{
    raft89_status s;
    unsigned long i;
    for (i = 0ul; i < (unsigned long)c->count; ++i)
    {
        if (c->nodes[i].up == 0)
        {
            ++cover_down;
            continue;
        }
        if (c->nodes[i].raft == NULL)
        {
            continue;
        }
        if (c->nodes[i].f.store.entry_count > 0ul)
        {
            ++cover_logs;
        }
        if (raft89_status_get(c->nodes[i].raft, &s) == RAFT89_OK)
        {
            if (model_ul(s.commit_index) > 0ul)
            {
                ++cover_commits;
            }
            if (s.role == RAFT89_LEADER)
            {
                ++cover_leaders;
            }
        }
        if (c->nodes[i].app.last_applied > 0ul)
        {
            ++cover_applies;
        }
    }
    if (c->queue_count > cover_packets)
    {
        cover_packets = c->queue_count;
    }
}

static void report_violation(unsigned long depth, const sim_event *ev,
                             const char *violation)
{
    unsigned long i;
    fprintf(stderr, "MODEL VIOLATION: %s\n", violation);
    fprintf(stderr, "seed=%lu\n", current_seed);
    fprintf(stderr, "trace:\n");
    for (i = 0ul; i < depth; ++i)
    {
        sim_event_print(stderr, &path[i]);
    }
    sim_event_print(stderr, ev);
    fprintf(stderr, "states=%lu edges=%lu depth=%lu result=violation\n", states,
            edges, depth);
    fflush(stderr);
    exit(1);
}

static void expand(cluster *state, unsigned long depth);

static void try_event(cluster *state, unsigned long depth, const sim_event *ev)
{
    cluster *child;
    const char *violation;
    unsigned long len;
    unsigned long h1;
    unsigned long h2;
    int rc;
    child = (cluster *)malloc(sizeof(cluster));
    if (child == NULL)
    {
        cap_hit = 1;
        return;
    }
    if (cluster_clone(state, child) != RAFT89_OK)
    {
        free(child);
        cap_hit = 1;
        return;
    }
    rc = sim_event_apply(child, ev);
    if (rc == RAFT89_ERR_PROTOCOL)
    {
        rc = RAFT89_OK;
    }
    if (rc != RAFT89_OK)
    {
        cluster_free(child);
        free(child);
        return;
    }
    ++count_events[ev->type];
    violation = invariants_check(child);
    if (violation == NULL)
    {
        violation = invariants_check_transition(state, child);
    }
    if (violation != NULL)
    {
        report_violation(depth, ev, violation);
    }
    if (prune(child) != 0)
    {
        cluster_free(child);
        free(child);
        return;
    }
    coverage_update(child);
    if (states >= MODEL_STATE_CAP)
    {
        cap_hit = 1;
        cluster_free(child);
        free(child);
        return;
    }
    if (model_key(child, key_buf, MODEL_KEY_CAP, &len) != RAFT89_OK)
    {
        cap_hit = 1;
        cluster_free(child);
        free(child);
        return;
    }
    hash_key(key_buf, len, &h1, &h2);
    ++edges;
    if (visited_insert(h1, h2) == 0)
    {
        cluster_free(child);
        free(child);
        return;
    }
    ++states;
    if (depth + 1ul > max_depth)
    {
        max_depth = depth + 1ul;
    }
    path[depth] = *ev;
    expand(child, depth + 1ul);
    cluster_free(child);
    free(child);
}

static void expand(cluster *state, unsigned long depth)
{
    sim_event events[SIM_EVENT_MAX];
    unsigned long count;
    unsigned long i;
    if (depth >= MODEL_MAX_DEPTH)
    {
        return;
    }
    count = sim_events_collect(state, MODEL_MAX_PACKETS, MODEL_MAX_CRASHES,
                               MODEL_ENV != 0, events, SIM_EVENT_MAX);
    for (i = 0ul; i < count; ++i)
    {
        try_event(state, depth, &events[i]);
    }
}

static int run_self_check(void)
{
    cluster *good;
    cluster *bad;
    const char *v;
    int failures;
    failures = 0;
    good = (cluster *)malloc(sizeof(cluster));
    bad = (cluster *)malloc(sizeof(cluster));
    if (good == NULL || bad == NULL)
    {
        fprintf(stderr, "SELF-CHECK: allocation failure\n");
        free(good);
        free(bad);
        return 1;
    }
    cluster_init(good, MODEL_NODES, 0ul);
    v = invariants_check(good);
    if (v != NULL)
    {
        fprintf(stderr, "SELF-CHECK: valid cluster rejected: %s\n", v);
        ++failures;
    }
    /* I03: a durable vote may not change within one term. */
    fake_store_set_hard(&good->nodes[0].f.store, 1ul, 2ul);
    if (cluster_clone(good, bad) != RAFT89_OK)
    {
        fprintf(stderr, "SELF-CHECK: clone failed\n");
        cluster_free(good);
        free(good);
        free(bad);
        return 1;
    }
    fake_store_set_hard(&bad->nodes[0].f.store, 1ul, 3ul);
    v = invariants_check_transition(good, bad);
    if (v == NULL || strstr(v, "I03") == NULL)
    {
        fprintf(stderr, "SELF-CHECK: I03 not detected\n");
        ++failures;
    }
    cluster_free(bad);
    fake_store_set_hard(&good->nodes[0].f.store, 0ul, 0ul);
    if (cluster_clone(good, bad) != RAFT89_OK)
    {
        fprintf(stderr, "SELF-CHECK: clone failed\n");
        cluster_free(good);
        free(good);
        free(bad);
        return 1;
    }
    bad->nodes[0].f.store.hard.voted_for = 99ul;
    v = invariants_check(bad);
    if (v == NULL || strstr(v, "I04") == NULL)
    {
        fprintf(stderr, "SELF-CHECK: I04 not detected\n");
        ++failures;
    }
    cluster_free(bad);
    cluster_free(good);
    free(good);
    free(bad);
    if (failures != 0)
    {
        fprintf(stderr, "SELF-CHECK FAILED\n");
        return 1;
    }
    printf("SELF-CHECK OK\n");
    return 0;
}

static int seed_apply(cluster *c, unsigned long seed)
{
    static const char cmd_a[1] = {'A'};
    static const char cmd_b[1] = {'B'};
    raft89_status s;
    if (seed == 0ul)
    {
        return 0;
    }
    if (cluster_tick(c, 1u, 20u) != RAFT89_OK)
    {
        return -1;
    }
    if (cluster_drain(c, 1u) < 0)
    {
        return -1;
    }
    cluster_deliver_all(c);
    cluster_deliver_all(c);
    if (raft89_status_get(c->nodes[0].raft, &s) != RAFT89_OK)
    {
        return -1;
    }
    if (s.role != RAFT89_LEADER)
    {
        return -1;
    }
    if (seed == 1ul)
    {
        return 0;
    }
    if (cluster_propose(c, 1u, cmd_a, 1u) != RAFT89_OK)
    {
        return -1;
    }
    cluster_drain(c, 1u);
    cluster_deliver_all(c);
    cluster_deliver_all(c);
    if (cluster_tick(c, 1u, 10u) != RAFT89_OK)
    {
        return -1;
    }
    cluster_drain(c, 1u);
    cluster_deliver_all(c);
    cluster_deliver_all(c);
    if (seed == 2ul)
    {
        return 0;
    }
    cluster_link(c, 1u, 3u, 0);
    cluster_link(c, 3u, 1u, 0);
    cluster_link(c, 2u, 3u, 0);
    cluster_link(c, 3u, 2u, 0);
    if (cluster_propose(c, 1u, cmd_b, 1u) != RAFT89_OK)
    {
        return -1;
    }
    cluster_drain(c, 1u);
    cluster_deliver_all(c);
    cluster_deliver_all(c);
    return 0;
}

static void print_result(const char *result)
{
    printf("states=%lu edges=%lu depth=%lu leader=%lu logs=%lu commits=%lu "
           "applies=%lu down=%lu packets=%lu result=%s\n",
           states, edges, max_depth, cover_leaders, cover_logs, cover_commits,
           cover_applies, cover_down, cover_packets, result);
    printf("events tick=%lu deliver=%lu drop=%lu dup=%lu crash0=%lu crash1=%lu "
           "restart=%lu propose=%lu linkup=%lu linkdown=%lu effect=%lu "
           "ack=%lu lost=%lu\n",
           count_events[SIM_EV_TICK], count_events[SIM_EV_DELIVER],
           count_events[SIM_EV_DROP], count_events[SIM_EV_DUP],
           count_events[SIM_EV_CRASH0], count_events[SIM_EV_CRASH1],
           count_events[SIM_EV_RESTART], count_events[SIM_EV_PROPOSE],
           count_events[SIM_EV_LINK_UP], count_events[SIM_EV_LINK_DOWN],
           count_events[SIM_EV_EFFECT], count_events[SIM_EV_ACK],
           count_events[SIM_EV_LOST]);
}

int main(int argc, char **argv)
{
    cluster *c;
    const char *violation;
    unsigned long len;
    unsigned long h1;
    unsigned long h2;
    unsigned long seed;
    int i;
    for (i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--strict") == 0)
        {
            strict_mode = 1;
        }
        else if (strcmp(argv[i], "--self-check") == 0)
        {
            return run_self_check();
        }
        else
        {
            fprintf(stderr, "usage: test_model3 [--strict] [--self-check]\n");
            return 2;
        }
    }
    if (visit_init(MODEL_STATE_CAP) != 0)
    {
        fprintf(stderr, "INCONCLUSIVE: cannot allocate the visited set\n");
        return strict_mode ? 2 : 0;
    }
    for (i = 0; i < (int)MODEL_SEEDS; ++i)
    {
        seed = MODEL_SEEDS - 1ul - (unsigned long)i;
        c = (cluster *)malloc(sizeof(cluster));
        if (c == NULL)
        {
            cap_hit = 1;
            break;
        }
        cluster_init(c, MODEL_NODES, 0ul);
        if (seed_apply(c, seed) != 0)
        {
            fprintf(stderr, "MODEL: seed %lu failed\n", seed);
            cluster_free(c);
            free(c);
            visit_free();
            return 1;
        }
        current_seed = seed;
        coverage_update(c);
        violation = invariants_check(c);
        if (violation != NULL)
        {
            fprintf(stderr, "MODEL VIOLATION at seed %lu: %s\n", seed,
                    violation);
            cluster_free(c);
            free(c);
            visit_free();
            return 1;
        }
        if (model_key(c, key_buf, MODEL_KEY_CAP, &len) != RAFT89_OK)
        {
            cap_hit = 1;
        }
        else
        {
            hash_key(key_buf, len, &h1, &h2);
            if (visited_insert(h1, h2) != 0)
            {
                ++states;
                expand(c, 0ul);
            }
        }
        cluster_free(c);
        free(c);
        if (cap_hit != 0)
        {
            break;
        }
    }
    visit_free();
    if (cap_hit != 0)
    {
        print_result("INCONCLUSIVE");
        return strict_mode ? 2 : 0;
    }
    print_result("pass");
    return 0;
}
