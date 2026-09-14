#ifndef FSM89_FIXTURES_H
#define FSM89_FIXTURES_H

#include "fsm89.h"

/* M0 - one state, no edges. Leave [11], enter [10]. */
extern const fsm89_def fsm89_fixture_m0;

/* M1 - A --X--> B with leave [10, 11], edge [20, 21], enter [30, 31]. */
extern const fsm89_def fsm89_fixture_m1;

/* M2 - A --X--> A with leave [10], edge [20], enter [30]. */
extern const fsm89_def fsm89_fixture_m2;

/* M3 - recognizer: START --digit--> INTEGER(accepting),
 * INTEGER --digit--> INTEGER, INTEGER --dot--> FRACTION(accepting),
 * FRACTION --digit--> FRACTION. */
extern const fsm89_def fsm89_fixture_m3;

/* M4 - unreachable component: A --X--> B, C --Y--> D. */
extern const fsm89_def fsm89_fixture_m4;

/* M5 - silent transition A --X--> B with no effects. */
extern const fsm89_def fsm89_fixture_m5;

/* M6 - convergent edges: A --X--> C, A --Y--> C, B --X--> C. */
extern const fsm89_def fsm89_fixture_m6;

#endif
