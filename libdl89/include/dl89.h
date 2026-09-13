#ifndef DL89_H
#define DL89_H

#include <stddef.h>

typedef unsigned long dl89_const;
typedef unsigned long dl89_rel;
typedef unsigned long dl89_var;

enum dl89_status
{
    DL89_OK = 0,
    DL89_EINVAL,
    DL89_ENOMEM,
    DL89_EPROGRAM,
    DL89_ESTORE,
    DL89_EBUSY
};

enum dl89_term_kind
{
    DL89_TERM_CONST = 1,
    DL89_TERM_VAR = 2
};

typedef struct
{
    int kind;

    union
    {
        dl89_const constant;
        dl89_var variable;
    } u;
} dl89_term;

typedef struct
{
    dl89_rel relation;
    size_t arity;
    const dl89_term *terms;
} dl89_atom;

typedef struct
{
    dl89_atom head;
    size_t body_count;
    const dl89_atom *body;
} dl89_rule;

typedef struct dl89_scan dl89_scan;

typedef struct
{
    int (*insert)(void *ctx, dl89_rel relation, size_t arity,
                  const dl89_const *tuple, int *inserted);

    int (*scan_open)(void *ctx, dl89_rel relation, size_t arity,
                     const dl89_const *values, const unsigned char *bound,
                     dl89_scan **scan);

    int (*scan_next)(void *ctx, dl89_scan *scan, dl89_const *tuple, int *found);

    void (*scan_close)(void *ctx, dl89_scan *scan);
} dl89_store_ops;

typedef struct
{
    void *ctx;
    const dl89_store_ops *ops;
} dl89_store;

typedef struct
{
    dl89_store store;
} dl89_eval_config;

typedef struct dl89_eval dl89_eval;

int dl89_eval_create(const dl89_eval_config *config, dl89_eval **out);

void dl89_eval_destroy(dl89_eval *eval);

int dl89_eval_add_rule(dl89_eval *eval, const dl89_rule *rule);

int dl89_eval_add_fact(dl89_eval *eval, dl89_rel relation, size_t arity,
                       const dl89_const *tuple);

int dl89_eval_run(dl89_eval *eval);

#endif
