(module
  (memory 1)

  ;; memory.fill a single byte then read it back
  (func (export "fillv") (param i32 i32) (result i32)
    (local i32)
    local.get 0 i32.const 255 i32.and local.tee 2
    local.get 1
    i32.const 1
    memory.fill
    local.get 2 i32.load8_u)

  ;; seed a byte, memory.copy it elsewhere, then read the destination
  (func (export "copyv") (param i32 i32) (result i32)
    (local i32 i32)
    local.get 0 i32.const 254 i32.and local.tee 2
    i32.const 0x5a i32.store8
    local.get 0 i32.const 255 i32.and local.tee 3
    local.get 2
    i32.const 1
    memory.copy
    local.get 3 i32.load8_u)
)
