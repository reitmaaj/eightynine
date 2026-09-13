# 0006 — owned str89 storage scenarios

SCENARIO embedded-NUL value survives a round trip
  GIVEN a builder string value containing `a`, U+0000, `b`
  WHEN the tree is rendered and reparsed
  THEN the value length is 3, the bytes are exactly `a 00 b`, and the byte
    after the value is NUL

SCENARIO distinct embedded-NUL keys
  GIVEN `{"a\u0000x":1,"a\u0000y":2}`
  WHEN the object is parsed
  THEN it has two members whose key lengths are 3 and whose bytes differ at
    the third byte, and a lookup for `"a"` does not match

SCENARIO canonically equivalent but byte-distinct keys
  GIVEN `{"\u00e9":1,"e\u0301":2}`
  WHEN the object is parsed
  THEN it has two members: U+00E9 and U+0065 U+0301

SCENARIO surrogate escapes
  GIVEN `{"\ud834\udd1e":"\udbff\udfff"}`
  WHEN the object is parsed
  THEN the key is U+1D11E and the value is U+10FFFF encoded as UTF-8

SCENARIO escape round trip
  GIVEN a string containing quote, backslash, LF, TAB, U+0000, U+00E9, and
    U+10348
  WHEN it is rendered and reparsed
  THEN the exact bytes are preserved

SCENARIO builder NUL keys
  GIVEN two builder members with keys `a 00 x` and `a 00 y`
  WHEN the object is rendered and reparsed
  THEN both members survive with their full key bytes

SCENARIO failed allocation leaves no leak
  GIVEN an arena whose block allocation or string allocation fails
  WHEN the operation fails
  THEN the arena is marked failed, previously registered strings remain owned
    by the arena, and destroy releases every block exactly once
