#ifndef CAT89_FIND_H
#define CAT89_FIND_H

#include <cat89/core.h>
#include <cat89/enum.h>

/* cat89_find.h - finite universal-object finders.
 *
 * Given a finite ambient category with an enumeration capability, search for an
 * object with a universal property and hand back the object together with its
 * unique mediating arrows. `enumeration` must enumerate the objects and
 * morphisms of `category`. Presence is decided by hom-set cardinality alone
 * (exactly one morphism in each required hom-set), so no equality capability is
 * required. */

cat89_status cat89_find_terminal(cat89_category *category,
                                 cat89_enum *enumeration,
                                 const cat89_obj **out_terminal);

cat89_status cat89_find_initial(cat89_category *category,
                                cat89_enum *enumeration,
                                const cat89_obj **out_initial);

/* The unique morphism x -> terminal (terminal is terminal). One owned result;
 * caller releases via cat89_mor_release(category, mor). */
cat89_status cat89_terminal_arrow(cat89_category *category,
                                  cat89_enum *enumeration,
                                  const cat89_obj *terminal, const cat89_obj *x,
                                  cat89_mor **out_mor);

/* The unique morphism initial -> x. One owned result. */
cat89_status cat89_initial_arrow(cat89_category *category,
                                 cat89_enum *enumeration,
                                 const cat89_obj *initial, const cat89_obj *x,
                                 cat89_mor **out_mor);

#endif
