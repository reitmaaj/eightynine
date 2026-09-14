/* fsm89_fixtures.c - canonical machine definitions shared by the tests. */

#include "fsm89_fixtures.h"

/* ---- M0: one state, no edges -------------------------------------------- */

static const fsm89_effect m0_leave[] = {11};
static const fsm89_effect m0_enter[] = {10};

static const fsm89_state_def m0_states[] = {
    {0, {m0_leave, 1}, {m0_enter, 1}, 0}};

const fsm89_def fsm89_fixture_m0 = {m0_states, 1, NULL, 0, 0};

/* ---- M1: ordinary transition -------------------------------------------- */

static const fsm89_effect m1_a_leave[] = {10, 11};
static const fsm89_effect m1_edge[] = {20, 21};
static const fsm89_effect m1_b_enter[] = {30, 31};

static const fsm89_state_def m1_states[] = {{0, {m1_a_leave, 2}, {NULL, 0}, 0},
                                            {1, {NULL, 0}, {m1_b_enter, 2}, 0}};

static const fsm89_edge m1_edges[] = {{0, 0, 1, {m1_edge, 2}}};

const fsm89_def fsm89_fixture_m1 = {m1_states, 2, m1_edges, 1, 0};

/* ---- M2: self-transition ------------------------------------------------ */

static const fsm89_effect m2_leave[] = {10};
static const fsm89_effect m2_edge[] = {20};
static const fsm89_effect m2_enter[] = {30};

static const fsm89_state_def m2_states[] = {
    {0, {m2_leave, 1}, {m2_enter, 1}, 0}};

static const fsm89_edge m2_edges[] = {{0, 0, 0, {m2_edge, 1}}};

const fsm89_def fsm89_fixture_m2 = {m2_states, 1, m2_edges, 1, 0};

/* ---- M3: recognizer with accepting, non-terminal states ----------------- */

static const fsm89_state_def m3_states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                            {1, {NULL, 0}, {NULL, 0}, 1},
                                            {2, {NULL, 0}, {NULL, 0}, 1}};

static const fsm89_edge m3_edges[] = {{0, 0, 1, {NULL, 0}},
                                      {1, 0, 1, {NULL, 0}},
                                      {1, 1, 2, {NULL, 0}},
                                      {2, 0, 2, {NULL, 0}}};

const fsm89_def fsm89_fixture_m3 = {m3_states, 3, m3_edges, 4, 0};

/* ---- M4: unreachable component ------------------------------------------ */

static const fsm89_state_def m4_states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                            {1, {NULL, 0}, {NULL, 0}, 0},
                                            {2, {NULL, 0}, {NULL, 0}, 0},
                                            {3, {NULL, 0}, {NULL, 0}, 0}};

static const fsm89_edge m4_edges[] = {{0, 0, 1, {NULL, 0}},
                                      {2, 1, 3, {NULL, 0}}};

const fsm89_def fsm89_fixture_m4 = {m4_states, 4, m4_edges, 2, 0};

/* ---- M5: silent transition ---------------------------------------------- */

static const fsm89_state_def m5_states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                            {1, {NULL, 0}, {NULL, 0}, 0}};

static const fsm89_edge m5_edges[] = {{0, 0, 1, {NULL, 0}}};

const fsm89_def fsm89_fixture_m5 = {m5_states, 2, m5_edges, 1, 0};

/* ---- M6: convergent edges ----------------------------------------------- */

static const fsm89_state_def m6_states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                            {1, {NULL, 0}, {NULL, 0}, 0},
                                            {2, {NULL, 0}, {NULL, 0}, 0}};

static const fsm89_edge m6_edges[] = {
    {0, 0, 2, {NULL, 0}}, {0, 1, 2, {NULL, 0}}, {1, 0, 2, {NULL, 0}}};

const fsm89_def fsm89_fixture_m6 = {m6_states, 3, m6_edges, 3, 0};
