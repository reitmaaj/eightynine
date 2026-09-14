#ifndef FSM89_H
#define FSM89_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* libfsm89 - deterministic labelled transition systems for strict C89.
     *
     * libfsm89 maps (state, event) to (next state, emissions) and nothing else.
     * It does not execute I/O, maintain queues, schedule timers, allocate
     * application objects, or interpret emissions. One representation covers
     * recognizers, Moore/Mealy transducers, protocol machines, parsers, and
     * effect-driving controllers.
     *
     * A successful transition emits, in order:
     *
     *   leave(source) || edge(source, event) || enter(destination)
     *
     * Startup emits only enter(initial). A self-transition performs both leave
     * and enter. Effects are declarative outputs; the library never executes
     * them. There is no mutable FSM object: fsm89_step proposes a transition
     * and the caller commits it.
     *
     * Everything is caller-owned and immutable. All effect spans returned by
     * fsm89_start or stored in a successful fsm89_step_result borrow storage
     * from the definition; they remain valid only while the definition and the
     * referenced effect arrays remain alive and unmodified. The library never
     * allocates and offers no destroy operation.
     */

    typedef unsigned long fsm89_state;
    typedef unsigned long fsm89_event;
    typedef unsigned long fsm89_effect;

    /* Ordered borrowed effect span. When n == 0, v may be NULL or non-NULL.
     * When n > 0, v must be non-NULL. */
    typedef struct fsm89_effects
    {
        const fsm89_effect *v;
        size_t n;
    } fsm89_effects;

    /* One state. `accepting` carries no finality: an accepting state may have
     * outgoing edges. */
    typedef struct fsm89_state_def
    {
        fsm89_state id;
        fsm89_effects leave;
        fsm89_effects enter;
        int accepting;
    } fsm89_state_def;

    /* One deterministic transition. The unique edge key is (from, event). */
    typedef struct fsm89_edge
    {
        fsm89_state from;
        fsm89_event event;
        fsm89_state to;
        fsm89_effects effects;
    } fsm89_edge;

    /* A complete machine definition: caller-owned immutable tables. */
    typedef struct fsm89_def
    {
        const fsm89_state_def *states;
        size_t state_count;
        const fsm89_edge *edges;
        size_t edge_count;
        fsm89_state initial;
    } fsm89_def;

    /* One evaluated transition. leave, edge, and enter borrow definition
     * storage and concatenate in exactly that order. */
    typedef struct fsm89_step_result
    {
        fsm89_state from;
        fsm89_event event;
        fsm89_state to;
        fsm89_effects leave;
        fsm89_effects edge;
        fsm89_effects enter;
    } fsm89_step_result;

    /* FSM89_NO_TRANSITION is positive: it is an ordinary machine outcome, not
     * an error. FSM89_ESTATE is the only runtime error. The remaining
     * negative codes report definition-validation failures. */
    typedef enum fsm89_status
    {
        FSM89_OK = 0,
        FSM89_NO_TRANSITION = 1,
        FSM89_ESTATE = -1,
        FSM89_EDEF = -2,
        FSM89_EEFFECTS = -3,
        FSM89_EDUP_STATE = -4,
        FSM89_EINITIAL = -5,
        FSM89_EEDGE_FROM = -6,
        FSM89_EEDGE_TO = -7,
        FSM89_EDUP_EDGE = -8
    } fsm89_status;

    /* Prove structural consistency and determinism. This checks only whether
     * the definition denotes a well-formed deterministic FSM; it imposes no
     * reachability, terminality, or completeness policy. On success the
     * definition may be used with fsm89_start and fsm89_step.
     *
     * Returns FSM89_OK, or the first applicable error in this order:
     *
     *   FSM89_EDEF        NULL def, zero states, NULL states, or
     *                     edge_count > 0 with NULL edges
     *   FSM89_EEFFECTS    a nonempty span with NULL v
     *   FSM89_EDUP_STATE  duplicate state ID
     *   FSM89_EINITIAL    initial state absent
     *   FSM89_EEFFECTS    a nonempty edge span with NULL v
     *   FSM89_EEDGE_FROM  edge source absent
     *   FSM89_EEDGE_TO    edge destination absent
     *   FSM89_EDUP_EDGE   duplicate (from, event) edge
     *
     * Unreachable states, accepting states with outgoing edges, and empty
     * machines validate FSM89_OK.
     */
    fsm89_status fsm89_validate(const fsm89_def *def);

    /* Set *state to def->initial and *enter to the initial state's enter
     * span. No leave or edge effect occurs.
     *
     * Preconditions:
     *   def != NULL, state != NULL, enter != NULL
     *   fsm89_validate(def) == FSM89_OK
     *
     * Returns FSM89_OK. *state and *enter are modified only on FSM89_OK.
     * Returned storage is borrowed from def.
     */
    fsm89_status fsm89_start(const fsm89_def *def, fsm89_state *state,
                             fsm89_effects *enter);

    /* Evaluate one transition from state under event.
     *
     * Preconditions:
     *   def != NULL, result != NULL
     *   fsm89_validate(def) == FSM89_OK
     *
     * Returns:
     *   FSM89_OK             a transition exists; *result is populated
     *   FSM89_NO_TRANSITION  state exists, but no edge exists for
     *                        (state, event); *result is unchanged
     *   FSM89_ESTATE         state does not name a defined state;
     *                        *result is unchanged
     *
     * On FSM89_OK, *result describes leave, edge, and enter emissions in that
     * order. No effect is executed and no caller state is mutated. Returned
     * storage is borrowed from def.
     */
    fsm89_status fsm89_step(const fsm89_def *def, fsm89_state state,
                            fsm89_event event, fsm89_step_result *result);

    /* Nonzero exactly when state names a state whose accepting field is
     * nonzero; 0 when the state is not defined. Accepting does not mean
     * terminal.
     *
     * Preconditions:
     *   def != NULL
     *   fsm89_validate(def) == FSM89_OK
     */
    int fsm89_accepting(const fsm89_def *def, fsm89_state state);

#ifdef __cplusplus
}
#endif

#endif
