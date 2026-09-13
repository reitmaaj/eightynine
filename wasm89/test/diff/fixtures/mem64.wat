(module
  (memory i64 1)
  (func (export "rt") (param i64 i32) (result i32)
    (local i64)
    local.get 0 i64.const 0xfffffffffffffff8 i64.and local.tee 2
    local.get 1 i32.store
    local.get 2 i32.load))
