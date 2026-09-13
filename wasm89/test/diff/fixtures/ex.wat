(module
  (tag $e (param i32))

  ;; unconditional throw (uncaught): every engine must terminate abnormally
  (func (export "boom") (param i32)
    local.get 0 throw $e)

  ;; conditional throw: arg == 0 returns 0, else throws (uncaught)
  (func (export "maybe") (param i32) (result i32)
    local.get 0
    if (result i32)
      local.get 0
      throw $e
    else
      i32.const 0
    end)
)
