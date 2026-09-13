# 0004-algebraic-json-scenarios

## List

SCENARIO: nil is a left and right identity
GIVEN an empty list and any list x
WHEN concat(empty, x) or concat(x, empty) is computed
THEN the result is observationally equal to x

SCENARIO: concat is associative and preserves order
GIVEN lists x, y, z of symbolic elements
WHEN concat(concat(x,y),z) and concat(x,concat(y,z)) are computed
THEN both equal the same ordered sequence
AND element multiplicity is preserved

SCENARIO: fold is the right catamorphism
GIVEN a list [A,B,C], zero Z, and combine Node
WHEN list_fold runs
THEN the result is Node(A, Node(B, Node(C, Z)))
AND fold(nil, Z, f) equals Z with zero combine calls

SCENARIO: map is a functor
GIVEN a list x and finite pure functions f, g
WHEN map(x, id) and map(map(x,f),g) are computed
THEN map(x, id) equals x
AND map(map(x,f),g) equals map(x, g after f)
AND order and multiplicity are preserved

## Object

SCENARIO: empty is a commutative-monoid identity
GIVEN an object x
WHEN union(empty, x) and union(x, empty) are computed
THEN both equal x

SCENARIO: union is associative and commutative but not idempotent
GIVEN objects x, y, z
WHEN the three union orders are computed
THEN all are equal
AND union(p,p) has multiplicity two for a single occurrence p

SCENARIO: duplicate member names and occurrences survive
GIVEN member(S0,A) and member(S0,A)
WHEN they are unioned and passed through union, map, embed, project, fold, map
THEN exactly two occurrences remain

SCENARIO: member names are never compared
GIVEN two occurrences sharing a name S0
WHEN they are stored in one object
THEN no lookup, shadowing, or collapse occurs

SCENARIO: fold with a commutative monoid is a homomorphism
GIVEN a commutative monoid (R, zero, combine) and inject on members
WHEN fold(union(x,y)) is computed
THEN it equals combine(fold(x), fold(y))

SCENARIO: fold exposes no member order
GIVEN the same bag built through several union trees
WHEN folded through valid commutative monoids
THEN every result is the same

SCENARIO: a non-commutative combine is a caller-contract violation
GIVEN a caller-supplied combine that is not commutative
WHEN object.fold runs
THEN the result is unspecified and is not a conformance assertion

## JSON

SCENARIO: embed and project are mutual inverses
GIVEN any shape and any value
WHEN project(embed(shape)) and embed(project(value)) are computed
THEN each equals its input
AND project removes exactly one layer without recursing

SCENARIO: six constructors stay disjoint
GIVEN Null, Boolean(false), Number, String, Array([]), Object(empty)
WHEN project is applied
THEN six distinct shape variants result
AND Array([]) never equals Object(empty), nor String(S0) Null

SCENARIO: fold satisfies the catamorphism equation
GIVEN a one-layer shape s and a JSON algebra alpha
WHEN fold(embed(s)) is computed
THEN it equals alpha(F(fold)(s))
AND fold terminates on every finite tree

SCENARIO: fold invokes exactly the matching branch
GIVEN a value of each constructor
WHEN json.fold runs with an algebra whose other branches signal
THEN only the matching branch callback executes
AND every child folds before a container branch is invoked

SCENARIO: json.map maps the shared string carrier everywhere
GIVEN an object { S0 : "S1" } and mappings fS(S0)=T0, fS(S1)=T1
WHEN json.map runs
THEN the result is { T0 : "T1" }
AND both the member name and the string value passed through the same callback

SCENARIO: json.map is a functor
GIVEN a value j and finite pure number/string maps
WHEN map(j, id, id) and the two-step map compose
THEN identity and composition hold, including for nested and duplicate members

## Rejection (unacceptable behavior)

SCENARIO: no member multiplicity collapse is ever performed
GIVEN an object containing two identical occurrences
WHEN any core operation runs
THEN both occurrences remain present

SCENARIO: no scalar semantics leak into the core
GIVEN opaque number and string carriers exposing only construct/copy/destroy
WHEN list/object/json construction, embedding, projection, mapping, and folding run
THEN the core invokes no comparison, ordering, hashing, arithmetic, or Unicode decode

SCENARIO: callback failure never becomes JSON
GIVEN a callback that fails at a chosen invocation
WHEN a callback-bearing core operation runs
THEN the exact failure propagates
AND no further semantically dependent callback runs
AND no partial result escapes as success

SCENARIO: no excluded operation enters the public core
GIVEN the algebraic-core public namespace
WHEN it is audited
THEN it exposes only the mandatory list/object/shape/recursive-json set
AND no lookup, indexing, length, filter, search, path, equality, merge,
ordering, parse, render, arithmetic, or Unicode operation is mandatory
