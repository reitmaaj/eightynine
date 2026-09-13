(module
  (global $g i32 (i32.const 5))
  (global $h i64 (i64.const -7))

  ;; exercises local.set/tee over params (pure, deterministic)
  (func (export "lcl") (param i32 i32) (result i32)
    (local i32 i32)
    local.get 0 local.set 2
    local.get 1 local.set 3
    local.get 2 local.get 3 i32.mul
    local.tee 2
    local.get 2 i32.add)

  ;; exercises immutable global.get (import-free, const-initialized)
  (func (export "gglob") (param i32) (result i32)
    local.get 0 global.get $g i32.mul)

  ;; mixed-width local + global via i64
  (func (export "gglob64") (param i32) (result i64)
    local.get 0 i64.extend_i32_s global.get $h i64.mul)
)
