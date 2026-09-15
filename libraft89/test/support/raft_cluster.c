/* raft_cluster.c - deterministic cluster of real library nodes. */
#include <string.h>

#include "raft89_inspect.h"
#include "raft_cluster.h"

static cluster_node *node_at(cluster *c, raft89_id id)
{
    return &c->nodes[id - 1u];
}

static const cluster_node *node_at_const(const cluster *c, raft89_id id)
{
    return &c->nodes[id - 1u];
}

static int node_live(const cluster_node *n)
{
    if (n->up == 0)
    {
        return 0;
    }
    if (n->raft == NULL)
    {
        return 0;
    }
    return 1;
}

static void record_applied(cluster_node *n, const raft89_entry *entry)
{
    const unsigned char *data;
    unsigned long size;
    unsigned long i;
    unsigned long index;
    index = (unsigned long)entry->index.lo;
    if (index == 0ul)
    {
        return;
    }
    if (index > (unsigned long)CLUSTER_INDEX_MAX)
    {
        return;
    }
    size = (unsigned long)entry->size;
    if (size >= (unsigned long)PACKET_DATA_MAX)
    {
        size = (unsigned long)PACKET_DATA_MAX - 1u;
    }
    data = (const unsigned char *)entry->data;
    for (i = 0u; i < size; ++i)
    {
        n->applied[index][i] = data[i];
    }
    n->applied[index][size] = 0u;
    n->applied_len[index] = size;
    n->applied_seen[index] = 1;
}

static void remove_packet(cluster *c, unsigned long index)
{
    unsigned long i;
    for (i = index + 1u; i < c->queue_count; ++i)
    {
        packet_copy(&c->queue[i - 1u], &c->queue[i]);
    }
    --c->queue_count;
}

void cluster_init(cluster *c, raft89_size count, unsigned long seed)
{
    raft89_size i;
    raft89_size a;
    raft89_size b;
    unsigned long j;
    memset(c, 0, sizeof(*c));
    c->count = count;
    for (i = 0u; i < count; ++i)
    {
        fixture_init(&c->nodes[i].f, count, i + 1u);
        fake_app_init(&c->nodes[i].app);
        if (seed != 0u)
        {
            for (j = 0u; j < (unsigned long)FAKE_RANDOM_MAX; ++j)
            {
                fake_random_push(&c->nodes[i].f.random,
                                 seed + (unsigned long)i * 7u + j * 13u);
            }
        }
        c->nodes[i].raft = NULL;
        c->nodes[i].up = 1;
        c->nodes[i].crashes = 0u;
        if (raft89_create(&c->nodes[i].f.config, &c->nodes[i].raft) !=
            RAFT89_OK)
        {
            c->nodes[i].raft = NULL;
            c->nodes[i].up = 0;
        }
        oracle_init(&c->nodes[i].oracle, c->nodes[i].raft);
    }
    for (a = 1u; a <= count; ++a)
    {
        for (b = 1u; b <= count; ++b)
        {
            c->link[a][b] = 1;
        }
    }
    c->queue_count = 0u;
}

void cluster_free(cluster *c)
{
    raft89_size i;
    for (i = 0u; i < c->count; ++i)
    {
        if (c->nodes[i].raft != NULL)
        {
            raft89_destroy(c->nodes[i].raft);
            c->nodes[i].raft = NULL;
        }
        c->nodes[i].oracle.raft = NULL;
    }
    c->queue_count = 0u;
}

int cluster_clone(const cluster *src, cluster *dst)
{
    unsigned long i;
    *dst = *src;
    for (i = 0u; i < (unsigned long)src->count; ++i)
    {
        dst->nodes[i].raft = NULL;
        dst->nodes[i].oracle.raft = NULL;
        dst->nodes[i].oracle.action = NULL;
        fixture_rebind(&dst->nodes[i].f);
        if (src->nodes[i].raft != NULL)
        {
            if (raft89_inspect_clone(src->nodes[i].raft, &dst->nodes[i].raft) !=
                RAFT89_OK)
            {
                cluster_free(dst);
                return RAFT89_ERR_NOMEM;
            }
            raft89_inspect_rebind(dst->nodes[i].raft,
                                  &dst->nodes[i].f.config.store,
                                  &dst->nodes[i].f.config.random);
            dst->nodes[i].oracle.raft = dst->nodes[i].raft;
            if (src->nodes[i].oracle.phase != ORACLE_IDLE)
            {
                oracle_peek(&dst->nodes[i].oracle);
                dst->nodes[i].oracle.phase = src->nodes[i].oracle.phase;
            }
        }
    }
    for (i = 0u; i < src->queue_count; ++i)
    {
        packet_copy(&dst->queue[i], &src->queue[i]);
    }
    return RAFT89_OK;
}

int cluster_sync(cluster *c, raft89_id id)
{
    cluster_node *n;
    n = node_at(c, id);
    if (node_live(n) == 0)
    {
        return CLUSTER_NA;
    }
    if (n->oracle.phase == ORACLE_IDLE)
    {
        oracle_peek(&n->oracle);
    }
    return RAFT89_OK;
}

int cluster_tick(cluster *c, raft89_id id, raft89_time elapsed)
{
    cluster_node *n;
    int rc;
    n = node_at(c, id);
    if (node_live(n) == 0)
    {
        return CLUSTER_NA;
    }
    if (n->oracle.phase != ORACLE_IDLE)
    {
        return CLUSTER_NA;
    }
    rc = raft89_tick(n->raft, elapsed);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    return cluster_sync(c, id);
}

int cluster_propose(cluster *c, raft89_id id, const void *data,
                    raft89_size size)
{
    cluster_node *n;
    int rc;
    n = node_at(c, id);
    if (node_live(n) == 0)
    {
        return CLUSTER_NA;
    }
    if (n->oracle.phase != ORACLE_IDLE)
    {
        return CLUSTER_NA;
    }
    rc = raft89_propose(n->raft, data, size, NULL);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    return cluster_sync(c, id);
}

int cluster_effect(cluster *c, raft89_id id)
{
    cluster_node *n;
    const raft89_action *action;
    packet pkt;
    int rc;
    n = node_at(c, id);
    if (node_live(n) == 0)
    {
        return CLUSTER_NA;
    }
    if (n->oracle.phase != ORACLE_ISSUED)
    {
        return CLUSTER_NA;
    }
    action = n->oracle.action;
    if (action->type == RAFT89_ACT_SEND)
    {
        if (packet_from_send(n->f.config.self, action, &pkt) != 0)
        {
            return RAFT89_ERR_LIMIT;
        }
        rc = cluster_enqueue(c, &pkt);
        if (rc != RAFT89_OK)
        {
            return rc;
        }
    }
    rc = oracle_effect_full(&n->oracle, &n->f.store, &n->app);
    if (rc != 0)
    {
        return RAFT89_ERR;
    }
    if (action->type == RAFT89_ACT_APPLY)
    {
        record_applied(n, &action->u.apply.entry);
    }
    return RAFT89_OK;
}

int cluster_ack(cluster *c, raft89_id id)
{
    cluster_node *n;
    int rc;
    n = node_at(c, id);
    if (node_live(n) == 0)
    {
        return CLUSTER_NA;
    }
    if (n->oracle.phase != ORACLE_EFFECT_DONE)
    {
        return CLUSTER_NA;
    }
    rc = oracle_ack(&n->oracle, RAFT89_ACTION_OK);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    return cluster_sync(c, id);
}

int cluster_send_lost(cluster *c, raft89_id id)
{
    cluster_node *n;
    int rc;
    n = node_at(c, id);
    if (node_live(n) == 0)
    {
        return CLUSTER_NA;
    }
    if (n->oracle.phase != ORACLE_ISSUED)
    {
        return CLUSTER_NA;
    }
    if (n->oracle.action->type != RAFT89_ACT_SEND)
    {
        return CLUSTER_NA;
    }
    rc = oracle_ack(&n->oracle, RAFT89_ACTION_LOST);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    return cluster_sync(c, id);
}

int cluster_crash(cluster *c, raft89_id id, int after_effect)
{
    cluster_node *n;
    n = node_at(c, id);
    if (node_live(n) == 0)
    {
        return CLUSTER_NA;
    }
    if (after_effect != 0 && n->oracle.phase == ORACLE_ISSUED)
    {
        if (cluster_effect(c, id) != RAFT89_OK)
        {
            return RAFT89_ERR;
        }
    }
    oracle_crash(&n->oracle);
    n->raft = NULL;
    n->up = 0;
    ++n->crashes;
    return RAFT89_OK;
}

int cluster_restart(cluster *c, raft89_id id)
{
    cluster_node *n;
    n = node_at(c, id);
    if (n->up != 0)
    {
        return CLUSTER_NA;
    }
    if (oracle_restart(&n->oracle, &n->f.config) != RAFT89_OK)
    {
        return RAFT89_ERR_NOMEM;
    }
    n->raft = n->oracle.raft;
    n->up = 1;
    return cluster_sync(c, id);
}

int cluster_drain(cluster *c, raft89_id id)
{
    cluster_node *n;
    int count;
    int rc;
    n = node_at(c, id);
    if (node_live(n) == 0)
    {
        return CLUSTER_NA;
    }
    count = 0;
    for (;;)
    {
        if (n->oracle.phase == ORACLE_IDLE)
        {
            if (oracle_peek(&n->oracle) == 0)
            {
                return count;
            }
        }
        rc = cluster_effect(c, id);
        if (rc != RAFT89_OK)
        {
            return -1;
        }
        rc = cluster_ack(c, id);
        if (rc != RAFT89_OK)
        {
            return -1;
        }
        ++count;
    }
}

int cluster_enqueue(cluster *c, const packet *pkt)
{
    if (c->queue_count >= (unsigned long)CLUSTER_MAX_PACKETS)
    {
        return RAFT89_ERR_LIMIT;
    }
    packet_copy(&c->queue[c->queue_count], pkt);
    ++c->queue_count;
    return RAFT89_OK;
}

int cluster_deliverable(const cluster *c, unsigned long index)
{
    const packet *pkt;
    const cluster_node *dst;
    if (index >= c->queue_count)
    {
        return 0;
    }
    pkt = &c->queue[index];
    dst = node_at_const(c, pkt->to);
    if (c->link[pkt->from][pkt->to] == 0)
    {
        return 0;
    }
    if (node_live(dst) == 0)
    {
        return 0;
    }
    if (dst->oracle.phase != ORACLE_IDLE)
    {
        return 0;
    }
    return 1;
}

int cluster_deliver(cluster *c, unsigned long index)
{
    packet pkt;
    cluster_node *dst;
    int rc;
    if (cluster_deliverable(c, index) == 0)
    {
        return CLUSTER_NA;
    }
    packet_copy(&pkt, &c->queue[index]);
    dst = node_at(c, pkt.to);
    remove_packet(c, index);
    rc = raft89_recv(dst->raft, &pkt.msg);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    return cluster_sync(c, pkt.to);
}

void cluster_deliver_all(cluster *c)
{
    unsigned long head;
    raft89_id to;
    head = 0u;
    while (head < c->queue_count)
    {
        if (cluster_deliverable(c, head) == 0)
        {
            ++head;
            continue;
        }
        to = c->queue[head].to;
        if (cluster_deliver(c, head) == RAFT89_OK)
        {
            cluster_drain(c, to);
        }
    }
    c->queue_count = 0u;
}

void cluster_drop(cluster *c, unsigned long index)
{
    if (index >= c->queue_count)
    {
        return;
    }
    remove_packet(c, index);
}

void cluster_clear_queue(cluster *c)
{
    c->queue_count = 0u;
}

int cluster_duplicate(cluster *c, unsigned long index)
{
    packet pkt;
    if (index >= c->queue_count)
    {
        return CLUSTER_NA;
    }
    if (c->queue_count >= (unsigned long)CLUSTER_MAX_PACKETS)
    {
        return CLUSTER_NA;
    }
    packet_copy(&pkt, &c->queue[index]);
    return cluster_enqueue(c, &pkt);
}

void cluster_link(cluster *c, raft89_id a, raft89_id b, int up)
{
    c->link[a][b] = up;
}

int cluster_linked(const cluster *c, raft89_id a, raft89_id b)
{
    return c->link[a][b];
}

const unsigned char *cluster_applied(const cluster *c, raft89_id id,
                                     unsigned long index)
{
    const cluster_node *n;
    n = node_at_const(c, id);
    if (index == 0ul || index > (unsigned long)CLUSTER_INDEX_MAX)
    {
        return NULL;
    }
    if (n->applied_seen[index] == 0)
    {
        return NULL;
    }
    return n->applied[index];
}

unsigned long cluster_applied_len(const cluster *c, raft89_id id,
                                  unsigned long index)
{
    const cluster_node *n;
    n = node_at_const(c, id);
    if (index == 0ul || index > (unsigned long)CLUSTER_INDEX_MAX)
    {
        return 0u;
    }
    if (n->applied_seen[index] == 0)
    {
        return 0u;
    }
    return n->applied_len[index];
}
