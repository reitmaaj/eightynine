#ifndef CRASH_ORACLE_H
#define CRASH_ORACLE_H

/* crash_oracle.h - test-only crash-point oracle implementing the
 * contract in .agent/design/0001-crash-contract.md. Not part of the
 * library. */

#include <raft89.h>

#include "fake_app.h"
#include "fake_store.h"

enum oracle_phase
{
    ORACLE_IDLE = 0,
    ORACLE_ISSUED,
    ORACLE_EFFECTING,
    ORACLE_EFFECT_DONE,
    ORACLE_ACKED
};

enum oracle_crash_point
{
    ORACLE_CRASH_BEFORE_EFFECT = 0,
    ORACLE_CRASH_DURING_EFFECT,
    ORACLE_CRASH_AFTER_EFFECT,
    ORACLE_CRASH_AFTER_ACK
};

typedef struct crash_oracle
{
    raft89 *raft;
    enum oracle_phase phase;
    const raft89_action *action;
    unsigned long incarnation;
} crash_oracle;

void oracle_init(crash_oracle *o, raft89 *raft);

/* Returns 1 and moves to ISSUED when an action is outstanding, else 0. */
int oracle_peek(crash_oracle *o);

/* Durable effect models. Each requires phase ISSUED or EFFECTING and
 * moves to EFFECT_DONE on success. */
int oracle_effect_full(crash_oracle *o, fake_store *store, fake_app *app);
int oracle_effect_append_partial(crash_oracle *o, fake_store *store,
                                 unsigned long count);
int oracle_effect_truncate_partial(crash_oracle *o, fake_store *store,
                                   unsigned long boundary);
int oracle_effect_hard_partial(crash_oracle *o, fake_store *store, int use_new);
int oracle_effect_apply_partial(crash_oracle *o, fake_app *app, int apply);

/* Acknowledge the outstanding action and return to IDLE. */
int oracle_ack(crash_oracle *o, enum raft89_action_result result);

/* Discard all volatile state: the process crash. */
void oracle_crash(crash_oracle *o);

/* Rebuild the node from durable storage. */
int oracle_restart(crash_oracle *o, const raft89_config *config);

#endif /* CRASH_ORACLE_H */
