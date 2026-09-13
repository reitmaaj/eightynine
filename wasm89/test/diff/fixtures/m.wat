(module
  (memory 1)

  ;; copysign with the sign operand taken from an *input* or a finite
  ;; constant (never an internal generated NaN), so both engines agree.
  (func (export "cs") (param f64 f64) (result f64)
    local.get 0 local.get 1 f64.copysign)
  (func (export "csneg") (param f64) (result f64)
    local.get 0 f64.const -1.0 f64.copysign)

  (func (export "sqrts") (param f64) (result f64)
    local.get 0 f64.abs f64.sqrt)

  ;; non-saturating truncation: traps on NaN / out-of-range (agreement)
  (func (export "trunci") (param f64) (result i32)
    local.get 0 i32.trunc_f64_s)

  ;; in-bounds store/load round-trip (address masked to [0,255])
  (func (export "storeload") (param i32) (result i32)
    (local i32)
    local.get 0 i32.const 255 i32.and local.tee 1
    i32.const 42 i32.store
    local.get 1 i32.load)

  ;; deterministic out-of-bounds load trap (address beyond the 1-page memory)
  (func (export "oob") (result i32)
    i32.const 131072 i32.load)

  ;; deterministic unreachable trap
  (func (export "boom") (result i32) unreachable)

  ;; bounded recursion via a normal call (depth masked to <= 64)
  (func $acc (param i32 i32) (result i32)
    local.get 0 i32.eqz
    if (result i32)
      local.get 1
    else
      local.get 0 i32.const 1 i32.sub
      local.get 1 i32.const 1 i32.add
      call $acc
    end)
  (func (export "tcount") (param i32) (result i32)
    local.get 0 i32.const 64 i32.rem_u i32.const 0 call $acc)
)
