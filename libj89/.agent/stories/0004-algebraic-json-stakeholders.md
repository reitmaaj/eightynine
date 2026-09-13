# 0004-algebraic-json-stakeholders

STORY: a caller builds JSON whose objects may legitimately contain repeated
member names
AS a consumer of a policy-neutral JSON construction algebra
I WANT object members to form a bag of occurrences that preserves multiplicity
SO THAT no duplicate-resolution policy (reject/first/last) is baked into the
library and I can decide it at a higher layer.

STORY: a caller folds arbitrary JSON into its own result type
AS a user of a universal recursive eliminator
I WANT a catamorphism whose result carrier is my own type, driven by my own
algebra callbacks
SO THAT node counting, depth, structural reconstruction, and symbolic-tree
algebras are all expressible without the core knowing my result type.

STORY: a caller maps numbers and strings inside JSON while leaving structure alone
AS a user of the functorial JSON action
I WANT json.map to transform every number and every string (including every
object member name) through one shared mapping rule
SO THAT representation-changing morphisms between compatible carriers compose
and obey identity.

STORY: a caller needs arrays to behave as finite ordered sequences
AS a user of the list algebra
I WANT nil, cons, concat, map, and the right fold to preserve exact order and
multiplicity
SO THAT the free-monoid and list-functor laws hold observationally.

STORY: a caller needs objects to behave as finite free commutative monoids
AS a user of the object-bag algebra
I WANT empty, member, union, and the commutative fold to preserve multiplicity
and expose no member order
SO THAT object order never becomes an observable contract.

STORY: a caller needs every value to decompose and recombine by exactly one layer
AS a user of the fixed-point structure
I WANT embed and project to be mutual inverses over six constructors
SO THAT construction and one-layer decomposition are exact and recursion never
happens implicitly.

STORY: a caller depends on operational failures being reported, not swallowed
AS a user of callback- and allocation-bearing operations
I WANT allocation and callback failures to propagate unchanged and never to
become ordinary JSON values
SO THAT no result masquerades as success and no partial value escapes.
