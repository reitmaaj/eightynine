# libadt89 concept

`libadt89` is a companion ADT library to `libhm89`. It declares and analyzes
ordinary ML-style algebraic datatypes while leaving `libhm89` generic and
ignorant of ADTs.

`libhm89` provides type variables, named n-ary type constructors, polymorphic
schemes, environments, generalization, instantiation, and unification. A
value constructor such as `Some` is, to HM, just a value with scheme
`forall a. a -> Option(a)`, and `Option(a)` is just a named constructor
application.

`libadt89` layers the ADT *metadata* and *analysis* above that:

- nominal declaration identity (so `Option` from one module is not another),
- fixed constructor arity and closed constructor families,
- value-constructor metadata and generated polymorphic schemes,
- installation of constructors into an `hm_env`,
- constructor-pattern typing through ordinary unification,
- exhaustiveness and redundancy analysis (Maranget usefulness).

It does not implement a second type system, does not touch `libhm89`, and
does not prescribe a runtime value representation. A compiler lowers
`Some 42` to whatever it likes.

Dependency: `libadt89 -> libhm89`. Never the reverse.
