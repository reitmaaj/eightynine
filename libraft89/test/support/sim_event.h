#ifndef SIM_EVENT_H
#define SIM_EVENT_H

/* sim_event.h - test-only event collection, application, printing, and
 * parsing shared by the model checker and the random simulator. Not part
 * of the library. */

#include <stdio.h>

#include "raft_cluster.h"

#define SIM_EVENT_MAX 128

enum sim_event_type
{
    SIM_EV_TICK = 0,
    SIM_EV_DELIVER,
    SIM_EV_DROP,
    SIM_EV_DUP,
    SIM_EV_CRASH0,
    SIM_EV_CRASH1,
    SIM_EV_RESTART,
    SIM_EV_PROPOSE,
    SIM_EV_LINK_UP,
    SIM_EV_LINK_DOWN,
    SIM_EV_EFFECT,
    SIM_EV_ACK,
    SIM_EV_LOST,
    SIM_EV_TYPE_COUNT
};

typedef struct sim_event
{
    unsigned long type;
    raft89_id a;
    raft89_id b;
    unsigned long u;
    unsigned char cmd;
} sim_event;

/* Collect every applicable event in deterministic protocol-first order.
 * When env is 0, packet loss/duplication/drop, link changes, and crashes
 * are omitted, leaving the pure protocol transitions. Returns the number
 * written (never more than max). */
unsigned long sim_events_collect(const cluster *c, unsigned long max_packets,
                                 unsigned long max_crashes, int env,
                                 sim_event *out, unsigned long max);

/* Apply one event. Returns RAFT89_OK on success, RAFT89_ERR_PROTOCOL when
 * a delivered message is rejected without state change, or another
 * negative error. */
int sim_event_apply(cluster *c, const sim_event *ev);

/* Replayable one-line form: "event TYPE a b u [cmd]". */
void sim_event_print(FILE *out, const sim_event *ev);

/* Parse the line form. Returns 0 on success, -1 when malformed. */
int sim_event_parse(const char *line, sim_event *ev);

const char *sim_event_name(unsigned long type);

#endif /* SIM_EVENT_H */
