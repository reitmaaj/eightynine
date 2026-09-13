#ifndef CAT89_DIAGRAM_H
#define CAT89_DIAGRAM_H

#include <cat89/alloc.h>
#include <cat89/functor.h>

/* cat89_diagram.h - diagrams.
 *
 * A diagram of shape J in a category C is a functor D : J -> C. It therefore
 * shares the functor representation and lifecycle exactly (no separate object
 * model, no duplicate ABI). The source category J is the diagram shape; the
 * target category is the ambient category. These aliases keep the functor
 * operations usable under diagram names for readability. */

typedef cat89_functor cat89_diagram;

#define cat89_diagram_source cat89_functor_source
#define cat89_diagram_target cat89_functor_target
#define cat89_diagram_map_obj cat89_functor_map_obj
#define cat89_diagram_map_mor cat89_functor_map_mor
#define cat89_diagram_retain cat89_functor_retain
#define cat89_diagram_release cat89_functor_release

#endif
