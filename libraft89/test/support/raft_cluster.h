#ifndef RAFT_CLUSTER_H
#define RAFT_CLUSTER_H

/* raft_cluster.h - test-only deterministic cluster of real library
 * nodes over fake durable stores, a deep-copied packet queue, and a
 * directed link map. Not part of the library. */

#include <raft89.h>

#include "crash_oracle.h"
#include "fake_app.h"
#include "fixture.h"
#include "packet_queue.h"

#define CLUSTER_MAX_NODES 5
#define CLUSTER_MAX_PACKETS 16
#define CLUSTER_INDEX_MAX 16

/* Positive results below are "not applicable"; use RAFT89_OK for a
 * completed event and negative values for errors. */
#define CLUSTER_NA 1

typedef struct cluster_node
{
    fixture f;
    fake_app app;
    raft89 *raft;
    crash_oracle oracle;
    int up;
    unsigned long crashes;
    unsigned char applied[CLUSTER_INDEX_MAX][PACKET_DATA_MAX];
    unsigned long applied_len[CLUSTER_INDEX_MAX];
    int applied_seen[CLUSTER_INDEX_MAX];
} cluster_node;

typedef struct cluster
{
    raft89_size count;
    cluster_node nodes[CLUSTER_MAX_NODES];
    packet queue[CLUSTER_MAX_PACKETS];
    unsigned long queue_count;
    int link[CLUSTER_MAX_NODES + 1][CLUSTER_MAX_NODES + 1];
} cluster;

/* Create a fully connected cluster. seed == 0 leaves the random source
 * empty (a constant timeout); any other seed scripts distinct values. */
void cluster_init(cluster *c, raft89_size count, unsigned long seed);
void cluster_free(cluster *c);

/* Deep-copy the whole cluster, including nodes, packets, and links. */
int cluster_clone(const cluster *src, cluster *dst);

int cluster_sync(cluster *c, raft89_id id);
int cluster_tick(cluster *c, raft89_id id, raft89_time elapsed);
int cluster_propose(cluster *c, raft89_id id, const void *data,
                    raft89_size size);
int cluster_effect(cluster *c, raft89_id id);
int cluster_ack(cluster *c, raft89_id id);
int cluster_send_lost(cluster *c, raft89_id id);
int cluster_crash(cluster *c, raft89_id id, int after_effect);
int cluster_restart(cluster *c, raft89_id id);

/* Perform effects and acknowledgements until the node is idle. */
int cluster_drain(cluster *c, raft89_id id);

int cluster_enqueue(cluster *c, const packet *pkt);
int cluster_deliverable(const cluster *c, unsigned long index);
int cluster_deliver(cluster *c, unsigned long index);
void cluster_deliver_all(cluster *c);
void cluster_drop(cluster *c, unsigned long index);
void cluster_clear_queue(cluster *c);
int cluster_duplicate(cluster *c, unsigned long index);

void cluster_link(cluster *c, raft89_id a, raft89_id b, int up);
int cluster_linked(const cluster *c, raft89_id a, raft89_id b);

/* Recorded applied payload for an index, or NULL when absent. The
 * payload is NUL-terminated when it fits. */
const unsigned char *cluster_applied(const cluster *c, raft89_id id,
                                     unsigned long index);
unsigned long cluster_applied_len(const cluster *c, raft89_id id,
                                  unsigned long index);

#endif /* RAFT_CLUSTER_H */
