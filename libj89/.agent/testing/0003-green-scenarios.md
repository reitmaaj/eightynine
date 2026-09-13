# 0003-green-scenarios

SCENARIO: source is green under the strict C89∩C23 profile
GIVEN the libj89 translation units src/parse.c, src/value.c, src/print.c
       and src/main.c with their compile contexts recorded for gcc and clang
WHEN `just green` runs `green check`
THEN the GCC C89, GCC C23, Clang C89 and Clang C23 compiler cells all PASS
AND the clang-tidy semantic suite PASSes in both C89 and C23
AND the canonical format check PASSes

SCENARIO: effectful calls form complete transitions
GIVEN a value-returning parser function such as j89_parse_value
WHEN an effectful sub-parse call such as j89_parse_nested must drive the
       return value
THEN the call result is bound by a complete assignment statement before it is
       returned
AND no effectful call appears as the direct operand of `return`

SCENARIO: null pointer constants use the NULL spelling
GIVEN a call that accepts a null pointer argument such as strtod's endptr
WHEN the caller intends a null end pointer
THEN the argument is the NULL macro or a variable bound to NULL in its own
       statement
AND no cast is mixed into a call argument

SCENARIO: redundant casts are removed
GIVEN an assignment between two variables of the same canonical type such as
       size_t-backed j89_len
WHEN a cast is written between them
THEN the cast is removed
AND the source remains strict-C89 clean under both GCC and Clang
