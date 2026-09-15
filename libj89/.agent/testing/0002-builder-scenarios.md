# 0002-builder-scenarios

SCENARIO: build a minimal object
GIVEN an empty libj89 arena
WHEN j89_object_new is called with count 1, a member key "a" and a string value "x" are set via j89_object_set
THEN j89_render produces the text `{"a":"x"}`
AND the rendered text re-parses to an OBJECT with member "a" = STRING "x"

SCENARIO: build nested structures
GIVEN an arena
WHEN an object containing an array of two integers and a nested object is built
THEN j89_render produces a faithful, valid JSON text
AND re-parsing yields the same logical tree

SCENARIO: build strings with special characters
GIVEN a string value containing `"`, `\`, newline, tab, and a control byte
WHEN j89_string_new and j89_object_set build it into an object
THEN j89_render escapes those characters correctly
AND re-parsing recovers the original bytes

SCENARIO: build integer and double numbers
GIVEN j89_integer_new(42) and j89_double_new(0.5)
WHEN they are placed in an object and rendered
THEN the integer renders as `42` and the double as a valid JSON number
AND re-parsing yields INTEGER 42 and FLOAT 0.5 respectively

SCENARIO: build with a fixed count
GIVEN j89_object_new(a, 2) and j89_array_new(a, 3)
WHEN every declared slot is filled exactly once
THEN rendering is well-formed and order matches the slot order

SCENARIO: builder failure is reported
GIVEN an arena whose allocation fails (or a builder call is given an invalid
object/array index)
WHEN a builder function cannot complete
THEN it records an error via j89_error / marks the arena failed rather than
corrupting the tree

SCENARIO: build a null value
GIVEN an empty libj89 arena
WHEN j89_null_new is called
THEN it returns a node whose kind is NULL
AND j89_render produces the text `null`

SCENARIO: builder output is NUL-terminated
GIVEN a rendered object
WHEN the output arena is read via j89_render
THEN the produced bytes are NUL-terminated and may be used as a C string
