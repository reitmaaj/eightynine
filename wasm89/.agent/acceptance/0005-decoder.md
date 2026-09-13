# Acceptance: binary decoder

*Specification reference: WebAssembly Core Spec 3.0, chapter 5.*

## MUST

* `w89_module_decode` accepts a byte buffer and length and fills a
  `w89_module`, returning `W89_ERR_NONE` on success.
* The header MUST be `\00asm` + version `\01\00\00\00`; a truncated
  header fails with "unexpected end", a wrong magic with "magic header
  not detected", a wrong version with "unknown binary version".
* Sections MUST appear in increasing id order with these exceptions:
  custom sections (0) anywhere; the data count section (12) may appear
  after the element section (9) and before the code section (10);
  after the data count section only the code (10), data (11) or tag (13)
  section may follow.
* A section whose declared size exceeds the remaining module bytes fails
  with "length out of bounds"; whose content decodes to a different length
  fails with "section size mismatch"; whose content is truncated fails
  with "unexpected end of section or function".
* Every length read (section size, vector count, name length, data byte
  length, code body size) is checked against the remaining module bytes
  ("length out of bounds"), matching the reference `len32`.
* Section content is read module-bounded; the declared size is verified
  only after the content decodes ("section size mismatch").
* Custom sections' names are validated as `name ::= u32 byte*`.
* End-of-module checks in this order: trailing bytes ("unexpected content
  after last section"), data count consistency, function/code section
  count consistency.
* Type decodings: func (0x60 params results), struct (0x5F counted
  fields), array (0x5E single field), sub (0x50 non-final, 0x4F final,
  with optional supertype list), rec (0x4E list) plus the unary
  shorthands, MUST all decode. Value types MUST be one of 0x7F..0x7B or
  a ref type (0x63/0x64 + heaptype); heaptypes MUST be 0x69..0x74 or a
  non-negative s33.
* Limits flags MUST be 0x00/0x01/0x04/0x05; min MUST NOT exceed max.
* Element flags MUST be 0..7, data flags 0..2, import kinds 0..4, export
  kinds 0..4; export names MUST be unique.
* The code section count MUST equal the function section count; a data
  count section MUST agree with the data section length; total locals
  MUST NOT reach 2^32.
* Function bodies and constant expressions are stored as raw byte ranges
  (instruction decoding is a later milestone).

## MUST NOT

* The decoder MUST NOT accept reserved value/heap type bytes as types.
* The decoder MUST NOT accept out-of-order, repeated, or post-data-count
  sections with smaller ids.
* The decoder MUST NOT accept a negative s33 heaptype.
* The decoder MUST NOT report success when a section size mismatches its
  content or a content is truncated.

* Instruction decoding (spec 5.4): blocktype (typeidx / 0x40 empty /
  valtype including reference types), memarg (flags bit 6 = memidx,
  align = low 6 bits, offset u64), catch clauses, and the full core
  opcode set incl. 0xFC (trunc_sat + bulk memory + table ops). Bodies
  and constant expressions decode into `w89_instr_vec`. GC (0xFB) and
  SIMD (0xFD) return "unsupported feature". Unknown opcodes return
  "illegal opcode XX" with the hex byte.
* Const expressions are decoded with the instruction decoder (no
  byte-scanning); the enclosing section size check enforces bounds.
* Section order is the reference's fixed sequence
  1,2,3,4,5,13,6,7,8,9,12,10,11 (tag sits between memory and global);
  ids > 13 are "malformed section id"; out-of-order valid ids are
  "unexpected content after last section".
* Names (imports, exports, custom section names) must be valid UTF-8
  ("malformed UTF-8 encoding").
* Tag imports carry a 0x00 attribute byte before the type index.
