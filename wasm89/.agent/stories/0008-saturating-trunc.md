# Stories: saturating float-to-integer conversion

## 0011 - User of saturating conversions gets defined results

As a user relying on `i32.trunc_sat_f32_s`/`_u`, `i32.trunc_sat_f64_s`/`_u`,
`i64.trunc_sat_f32_s`/`_u`, and `i64.trunc_sat_f64_s`/`_u`,
I want these eight instructions to evaluate in wasm89 rather than crash with
"unsupported bulk instruction in evaluator",
so that programs using saturating truncation run correctly and deterministically.

## 0012 - Maintainer keeps decode/validate/eval consistent

As a maintainer,
I want the evaluator to execute every instruction the decoder and validator
already accept,
so that there is no gap between what validates and what runs.
