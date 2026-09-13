# 0001-json-scenarios

SCENARIO: parse a minimal object
GIVEN input `{"a":1}`
WHEN j89_parse is called
THEN it returns a valid OBJECT node with one member "a" = INTEGER 1

SCENARIO: parse nested structures
GIVEN input `{"a":[1,2,{"b":null}],"c":true}`
WHEN j89_parse is called
THEN the tree reflects the nested array/object/bool/null faithfully

SCENARIO: parse strings with escapes
GIVEN input `"a\\\"b\\n\\u0041"`
WHEN j89_parse is called
THEN the decoded string is `a"b\nA`

SCENARIO: string storage is NUL-terminated
GIVEN a parsed string value or object key
WHEN j89_string_value / j89_object_key is read
THEN the returned bytes are NUL-terminated at the reported length

SCENARIO: embedded NUL in a string
GIVEN input `"\u0000"`
WHEN j89_parse is called
THEN the node is a STRING of length 1 whose single byte is 0x00

SCENARIO: parse a negative integer
GIVEN input `-42`
WHEN j89_parse is called
THEN the node is INTEGER -42

SCENARIO: parse the full exact `double` integer range
GIVEN input `9007199254740992` (2^53) or `-9007199254740992` (-2^53)
WHEN j89_parse is called
THEN it returns the exact INTEGER value (no approximation)

SCENARIO: reject integers beyond the exact range
GIVEN input `9007199254740993` or `-9007199254740993` (|n| > 2^53)
WHEN j89_parse is called
THEN it reports an integer out-of-range error and returns J89_BAD

SCENARIO: negative zero parses to zero
GIVEN input `-0`
WHEN j89_parse is called
THEN the node is INTEGER 0

SCENARIO: reject trailing garbage
GIVEN input `{"a":1} x`
WHEN j89_parse is called
THEN it reports an error and returns J89_BAD

SCENARIO: reject malformed JSON
GIVEN input `{"a":}`
WHEN j89_parse is called
THEN it reports an error

SCENARIO: parse a fractional number
GIVEN input `1.5` or `-0.5`
WHEN j89_parse is called
THEN it returns a FLOAT node whose double value is the number

SCENARIO: parse a number with an exponent
GIVEN input `1e3`, `1E3`, `1e+3`, `1e-3`, or `123e65`
WHEN j89_parse is called
THEN it returns a FLOAT node with the correct double value

SCENARIO: an integer stays an integer
GIVEN input `42` or `-42`
WHEN j89_parse is called
THEN it returns an INTEGER node (never a FLOAT), preserving the exact value

SCENARIO: reject an out-of-range double
GIVEN a number whose magnitude exceeds DBL_MAX (e.g. `1e999`)
WHEN j89_parse is called
THEN it reports a range error and returns J89_BAD (no infinity, no NaN)

SCENARIO: reject malformed number grammar
GIVEN `1.`, `.5`, `1e`, `1e+`, `01`, `-`, `Infinity`, or `NaN`
WHEN j89_parse is called
THEN it reports an error and returns J89_BAD

SCENARIO: accept valid multibyte UTF-8 in a string
GIVEN a string or object key containing valid 2/3/4-byte UTF-8 sequences
WHEN j89_parse is called
THEN the raw bytes are preserved as the string content

SCENARIO: reject invalid UTF-8 in a string
GIVEN a string or object key containing invalid UTF-8 (lone continuation byte,
truncated sequence, overlong encoding, encoded surrogate, or code point above
U+10FFFF)
WHEN j89_parse is called
THEN it reports an error and returns J89_BAD

SCENARIO: accept an optional leading UTF-8 BOM
GIVEN input beginning with the bytes EF BB BF followed by a valid document
WHEN j89_parse is called
THEN the BOM is ignored and the document parses normally

SCENARIO: reject an incomplete BOM
GIVEN input beginning with only the first one or two BOM bytes (EF, or EF BB)
WHEN j89_parse is called
THEN it reports an error

SCENARIO: reject duplicate object keys
GIVEN an object with two members of the same key (e.g. `{"a":1,"a":2}`)
WHEN j89_parse is called
THEN it reports a duplicate-key error and returns J89_BAD

SCENARIO: reject unterminated string
GIVEN input `"abc`
WHEN j89_parse is called
THEN it reports an error
