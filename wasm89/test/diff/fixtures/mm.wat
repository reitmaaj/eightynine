(module
  (memory 1 1)
  (memory 1 1)
  (func (export "m1rt") (param i32 i32) (result i32)
    (local i32)
    local.get 0 i32.const 255 i32.and local.tee 2
    local.get 1 i32.store (memory 1)
    local.get 2 i32.load (memory 1))
  (func (export "m0rt") (param i32 i32) (result i32)
    (local i32)
    local.get 0 i32.const 255 i32.and local.tee 2
    local.get 1 i32.store
    local.get 2 i32.load))
