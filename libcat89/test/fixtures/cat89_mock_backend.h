#ifndef CAT89_MOCK_BACKEND_H
#define CAT89_MOCK_BACKEND_H

/* cat89_mock_backend.h - instrumented, fault-injectable structural backend.
 *
 * Used for API-mechanics, ownership, and status-propagation tests independent
 * of mathematical content. It does NOT claim category laws; the lawful oracle
 * is the finite backend (test/fixtures). Three objects (A,B,C) are provided;
 * morphisms carry a borrowed dom/cod object and reference counting. */
#include <cat89/core.h>

typedef struct cat89_mock cat89_mock;

enum cat89_mock_obj {
    CAT89_MOCK_A = 0,
    CAT89_MOCK_B,
    CAT89_MOCK_C
};

/* Create a mock category (owns the mock; released via category_release).
 * The returned `mock` is an observer that survives the category; free it with
 * cat89_mock_free once all counters have been read. */
cat89_status cat89_mock_new(
    const cat89_allocator *allocator,
    cat89_category **out_category,
    cat89_mock **out_mock);

/* Free an observer returned by cat89_mock_new (after the category is released
 * and counters read). No-op on NULL. */
void cat89_mock_free(cat89_mock *mock);

/* Borrowed object handle for one of the three objects. */
const cat89_obj *cat89_mock_obj(
    cat89_mock *mock,
    enum cat89_mock_obj which);

/* Owned generic morphism dom->cod (not an identity). Caller must release. */
cat89_status cat89_mock_mor(
    cat89_mock *mock,
    enum cat89_mock_obj dom,
    enum cat89_mock_obj cod,
    cat89_mor **out_mor);

/* Counters. */
unsigned long cat89_mock_dom_calls(cat89_mock *mock);
unsigned long cat89_mock_cod_calls(cat89_mock *mock);
unsigned long cat89_mock_compose_calls(cat89_mock *mock);
unsigned long cat89_mock_identity_calls(cat89_mock *mock);
unsigned long cat89_mock_retain_calls(cat89_mock *mock);
unsigned long cat89_mock_release_calls(cat89_mock *mock);
unsigned long cat89_mock_obj_same_calls(cat89_mock *mock);
unsigned long cat89_mock_owns_obj_calls(cat89_mock *mock);
unsigned long cat89_mock_owns_mor_calls(cat89_mock *mock);
unsigned long cat89_mock_mor_free_calls(cat89_mock *mock);
unsigned long cat89_mock_destroy_calls(cat89_mock *mock);

/* Zero every observable callback counter (not the live registry). */
void cat89_mock_reset_calls(cat89_mock *mock);

/* Programmable failure: after the first `after` calls of the named op, return
 * `status` (CAT89_OK disables). */
void cat89_mock_fail_dom(
    cat89_mock *mock,
    unsigned long after,
    cat89_status status);

void cat89_mock_fail_cod(
    cat89_mock *mock,
    unsigned long after,
    cat89_status status);

void cat89_mock_fail_compose(
    cat89_mock *mock,
    unsigned long after,
    cat89_status status);

void cat89_mock_fail_identity(
    cat89_mock *mock,
    unsigned long after,
    cat89_status status);

void cat89_mock_fail_retain(
    cat89_mock *mock,
    unsigned long after,
    cat89_status status);

#endif
