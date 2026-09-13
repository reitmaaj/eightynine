(module
  (memory 1)
  (type $binop (func (param i32 i32) (result i32)))
  (table 2 funcref)
  (elem (i32.const 0) $add $sub)
  (func $add (type $binop) (param i32 i32) (result i32)
    local.get 0 local.get 1 i32.add)
  (func $sub (type $binop) (param i32 i32) (result i32)
    local.get 0 local.get 1 i32.sub)

  ;; memory round-trip with an in-bounds masked address
  (func (export "memrt") (param i32 i32) (result i32)
    (local i32)
    local.get 0 i32.const 0xfffffff8 i32.and local.tee 0
    local.get 1 i32.store
    local.get 0 i32.load)

  ;; loop: sum 1..(n masked to <= 63)
  (func (export "lsum") (param i32) (result i32)
    (local i32)
    local.get 0 i32.const 63 i32.and local.set 1
    block $out
      loop $l
        local.get 1 i32.eqz br_if $out
        local.get 0 local.get 1 i32.add local.set 0
        local.get 1 i32.const 1 i32.sub local.set 1
        br $l
      end
    end
    local.get 0)

  ;; table dispatch via call_indirect (index masked to table size)
  (func (export "dispatch") (param i32 i32 i32) (result i32)
    local.get 1 local.get 2 local.get 0 i32.const 1 i32.and
    call_indirect (type $binop))

  ;; bounded recursion: fac(n), n clamped to <= 12
  (func $fac_inner (param i32 i32) (result i32)
    local.get 0 i32.eqz
    if (result i32)
      local.get 1
    else
      local.get 0 i32.const 1 i32.sub
      local.get 1 local.get 0 i32.mul
      call $fac_inner
    end)
  (func (export "fac") (param i32) (result i32)
    local.get 0 i32.const 12 i32.gt_u
    if (result i32)
      i32.const 12
    else
      local.get 0 i32.const 1 call $fac_inner
    end)
)
