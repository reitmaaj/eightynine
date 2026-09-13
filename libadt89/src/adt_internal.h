#ifndef ADT_INTERNAL_H
#define ADT_INTERNAL_H

/* adt_internal.h - private representation and cross-module helpers.
 * Included only by the library translation units under src/. */

#include <adt.h>

/* Canonical (opaque) HM type-constructor name for a datatype. The format is
 * private; only distinctness within the process is guaranteed. The count is
 * drawn from a process-wide counter so declarations in different adt_ctx
 * instances never collide. */
#define ADT_HM_NAME_PREFIX "adt_"

struct adt_type
{
    struct adt_ctx *ctx;

    char *name;
    char *hm_name;

    size_t parameter_count;
    struct hm_type **parameters;

    struct adt_ctor **ctors;
    size_t ctor_count;
    size_t ctor_capacity;

    int sealed;

    void *origin;
};

struct adt_ctor
{
    struct adt_type *owner;

    char *name;

    size_t field_count;
    struct hm_type **field_types;

    struct hm_scheme *scheme;

    void *origin;
};

struct adt_pattern
{
    int kind;

    union
    {
        struct
        {
            struct adt_ctor *ctor;
            size_t count;
            struct adt_pattern **args;
        } ctor;
    } u;

    void *origin;
};

struct adt_error
{
    adt_error_kind kind;
    const char *message;

    struct adt_type *type;
    struct adt_ctor *ctor;

    const struct hm_error *hm_error;

    void *origin;
};

struct adt_ctx
{
    hm_ctx *hm;
    adt_allocator alloc;

    unsigned long next_type_id;

    struct adt_type **types;
    size_t type_count;
    size_t type_capacity;

    struct adt_pattern **patterns;
    size_t pattern_count;
    size_t pattern_capacity;

    void *origin;

    struct adt_error *last_error;
};

/* Allocation through the context allocator (owned, individually freed). */
void *adt_i_alloc(struct adt_ctx *ctx, size_t size);
char *adt_i_strdup(struct adt_ctx *ctx, const char *src);

/* Grow an owned pointer array; returns 1 on success. */
int adt_i_grow(struct adt_ctx *ctx, void ***items, size_t *count,
               size_t *capacity);

/* Append objects to the context registries for later destruction. */
int adt_i_register_type(struct adt_ctx *ctx, struct adt_type *type);
int adt_i_register_pattern(struct adt_ctx *ctx, struct adt_pattern *pattern);

/* Destroy an ADT-owned type and all of its constructors (not its HM nodes). */
void adt_i_destroy_type(struct adt_ctx *ctx, struct adt_type *type);

/* Free an ADT-owned constructor (name, field types, struct), not its owner
 * list nor any HM nodes. */
void adt_i_ctor_discard(struct adt_ctx *ctx, struct adt_ctor *ctor);

/* Record a structured failure as the context's last error and return the
 * matching adt_status. Pass NULL hm_error when none applies. */
adt_status adt_i_fail(struct adt_ctx *ctx, adt_status status,
                      adt_error_kind kind, const char *message,
                      struct adt_type *type, struct adt_ctor *ctor,
                      const struct hm_error *hm_err);

/* True when an open declaration has a constructor with this name. */
int adt_i_ctx_has_ctor_name(struct adt_ctx *ctx, const char *name);

#endif /* ADT_INTERNAL_H */
