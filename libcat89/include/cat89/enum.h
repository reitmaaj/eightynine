#ifndef CAT89_ENUM_H
#define CAT89_ENUM_H

#include <cat89/alloc.h>
#include <cat89/core.h>

/* cat89_enum.h - optional enumeration capability, bound to a category.
 *
 * Enumeration is independent of equality and of the category core. A backend
 * may expose object, morphism, or hom enumeration, any subset thereof; an
 * absent operation yields CAT89_NOT_SUPPORTED. Object iterators yield borrowed
 * object handles; morphism iterators yield owned morphism references the
 * caller releases through the category. */

typedef struct cat89_enum cat89_enum;

typedef struct cat89_obj_iter cat89_obj_iter;
typedef struct cat89_mor_iter cat89_mor_iter;

typedef struct cat89_enum_ops
{
    cat89_status (*obj_iter_open)(void *ctx, cat89_obj_iter **out_iter);

    cat89_status (*obj_iter_next)(void *ctx, cat89_obj_iter *iter,
                                  const cat89_obj **out_obj, int *out_done);

    void (*obj_iter_close)(void *ctx, cat89_obj_iter *iter);

    cat89_status (*mor_iter_open)(void *ctx, cat89_mor_iter **out_iter);

    cat89_status (*mor_iter_next)(void *ctx, cat89_mor_iter *iter,
                                  cat89_mor **out_mor, int *out_done);

    void (*mor_iter_close)(void *ctx, cat89_mor_iter *iter);

    cat89_status (*hom_iter_open)(void *ctx, const cat89_obj *dom,
                                  const cat89_obj *cod,
                                  cat89_mor_iter **out_iter);

    void (*destroy)(void *ctx);
} cat89_enum_ops;

/* Construct an enumeration capability over `category`; retains the category.
 * ops may be non-null with any subset of callbacks non-null. */
cat89_status cat89_enum_new(cat89_category *category, const cat89_enum_ops *ops,
                            void *ctx, const cat89_allocator *allocator,
                            cat89_enum **out_enum);

/* Object enumeration (borrowed objects). */
cat89_status cat89_obj_iter_open(const cat89_enum *enumeration,
                                 cat89_obj_iter **out_iter);

cat89_status cat89_obj_iter_next(const cat89_enum *enumeration,
                                 cat89_obj_iter *iter,
                                 const cat89_obj **out_obj, int *out_done);

void cat89_obj_iter_close(const cat89_enum *enumeration, cat89_obj_iter *iter);

/* Morphism enumeration (owned morphism per returned item). */
cat89_status cat89_mor_iter_open(const cat89_enum *enumeration,
                                 cat89_mor_iter **out_iter);

cat89_status cat89_mor_iter_next(const cat89_enum *enumeration,
                                 cat89_mor_iter *iter, cat89_mor **out_mor,
                                 int *out_done);

void cat89_mor_iter_close(const cat89_enum *enumeration, cat89_mor_iter *iter);

/* Hom-set enumeration (owned morphism per returned item). */
cat89_status cat89_hom_iter_open(const cat89_enum *enumeration,
                                 const cat89_obj *dom, const cat89_obj *cod,
                                 cat89_mor_iter **out_iter);

cat89_status cat89_hom_iter_next(const cat89_enum *enumeration,
                                 cat89_mor_iter *iter, cat89_mor **out_mor,
                                 int *out_done);

void cat89_hom_iter_close(const cat89_enum *enumeration, cat89_mor_iter *iter);

/* Borrowed category handle the capability is bound to. */
cat89_category *cat89_enum_category(const cat89_enum *enumeration);

cat89_status cat89_enum_retain(cat89_enum *enumeration);

void cat89_enum_release(cat89_enum *enumeration);

#endif
