# libu89 — validating Unicode for the green profile

## Purpose

`libu89` is a self-contained, dependency-free Unicode library that compiles
warning-clean as **strict ISO C89 AND strict C23**, under both GCC and Clang,
plus the `green` semantic checks and the canonical Allman format (verified by
the sibling `green` toolchain: `just check`). It is alloc-free and provides
strict, rejecting primitives ICU does not, on a principled axis.

## Scope

- Unicode scalar-value semantics and strict UTF-8 (validate / iterate / encode),
  plus positional decode/prev with an explicit status type for editor clients.
- UAX #15 normalization (NFC/NFD/NFKC/NFKD), gated by the complete
  `NormalizationTest.txt` conformance suite for the pinned Unicode version.
- UAX #31 identifier primitives (`XID_Start`/`XID_Continue`) and, in vNext,
  whole-string identifier profiles, Pattern_Syntax / Pattern_White_Space and
  Default_Ignorable.
- Terminal display width (East Asian Width) and Unicode-aware word wrap.
- UAX #29 extended grapheme clusters, gated by the complete
  `GraphemeBreakTest.txt` corpus, for grapheme-safe editing (librepl89).
- Terminal-relevant scalar predicates: East Asian Width classification, emoji
  and emoji-presentation, mark categories, control classification, and
  default-ignorable.
- Deferred until a second client needs it: UAX #29 word segmentation.
- vNext roadmap adds (in dependency order): a reusable property substrate,
  full case folding and NFKC_Casefold, grapheme-aware width, UAX #14
  line-break opportunities, a UTS #39 security layer, and UTS #55
  source-security facts. These land as the accompanying spec/ICD/test-design
  stages; breadth never precedes conformance.

## Design principles

- **Green source profile**: C89 AND C23 clean under GCC and Clang; Allman
  formatting; worker/controller structure (no inline computation in nested
  blocks); no function-like macros; no owned type names ending `_t`.
- **Facts, not policy**: the library exposes facts and algorithms. Diagnostics,
  scope decisions, and locale/rendering policy stay above the boundary.
- **Length-based, alloc-free**: every string API is `(const unsigned char *s,
  size_t n)`; no hidden allocation; callers own buffers.
- **Explicit Unicode version**: tables are generated for Unicode 17.0.0 and
  regenerated deterministically from the vendored data under
  `unicode-testdata/17.0.0/`; `unicode-testdata/17.0.0/SOURCES.txt` records
  every consumed file with its sha256, the generator, and the generation
  command (`just tables`).

## Non-goals

Collation/locale casing, CLDR, shaping, bidi layout, regex, dictionary word
segmentation, IDNA, rendering, and general-purpose DB introspection remain out
of scope. UTS #55 *policy* (scope-aware confusables, lexing, highlighting) is a
higher-layer responsibility.
