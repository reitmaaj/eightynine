#ifndef CAT89_ALIAS_BACKEND_H
#define CAT89_ALIAS_BACKEND_H

/* cat89_alias_backend.h - alias-object category fixture (F04).
 *
 * Distinct object pointers can denote the same categorical object: A0/A1,
 * B0/B1, C0/C1 are pairwise equal under obj_same. The category is the thin
 * poset A -> B -> C with unique homs; morphism endpoints deliberately use
 * alternate representatives (f : A1 -> B1, g : B0 -> C1, h = g o f : A1 -> C1)
 * so pointer identity disagrees with categorical identity. */

#include <cat89/cat89.h>

typedef struct cat89_alias cat89_alias;

enum cat89_alias_obj
{
    CAT89_ALIAS_A0 = 0,
    CAT89_ALIAS_A1,
    CAT89_ALIAS_B0,
    CAT89_ALIAS_B1,
    CAT89_ALIAS_C0,
    CAT89_ALIAS_C1
};

enum cat89_alias_mor
{
    CAT89_ALIAS_IDA = 0,
    CAT89_ALIAS_IDB,
    CAT89_ALIAS_IDC,
    CAT89_ALIAS_F,
    CAT89_ALIAS_G,
    CAT89_ALIAS_H
};

cat89_status cat89_alias_new(const cat89_allocator *allocator,
                             cat89_category **out_category,
                             cat89_alias **out_alias);

void cat89_alias_free(cat89_alias *alias);

const cat89_obj *cat89_alias_obj(cat89_alias *alias,
                                 enum cat89_alias_obj which);

/* One owned canonical handle for the named morphism. */
cat89_status cat89_alias_mor(cat89_alias *alias, enum cat89_alias_mor which,
                             cat89_mor **out_mor);

/* Equality / enumeration capabilities over the alias category. */
cat89_status cat89_alias_eq(cat89_alias *alias, cat89_eq **out_eq);

cat89_status cat89_alias_enum(cat89_alias *alias, cat89_enum **out_enum);

#endif
