(module
  (tag $e (param i32))

  ;; catch-and-return via try_table: argument 0 returns -1 on the normal
  ;; path; any other argument throws and is caught, returning the payload.
  (func (export "m") (param i32) (result i32)
    block $handler (result i32)
      try_table (catch $e $handler)
        local.get 0
        i32.eqz
        if
          nop
        else
          local.get 0
          throw $e
        end
      end
      i32.const -1
    end)
)
