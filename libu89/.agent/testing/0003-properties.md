# libu89 testing scenarios — terminal-relevant scalar properties

## 0001 — Control classification
SCENARIO General_Category Cc is a control
GIVEN LF, HT, ESC, DEL, and C1 scalars, plus printable and Cf scalars
WHEN u89_is_control() is asked
THEN Cc scalars are controls and every other scalar is not.

## 0002 — Mark classification
SCENARIO combining marks are marks
GIVEN Mn, Mc, Me, letter, and emoji-modifier scalars
WHEN u89_is_mark() is asked
THEN only Mn/Mc/Me scalars are marks.

## 0003 — Emoji classification
SCENARIO emoji and emoji presentation are distinct
GIVEN scalars with Emoji but not Emoji_Presentation (U+00A9, U+2708) and a
    scalar with both (U+1F600)
WHEN u89_is_emoji() and u89_is_emoji_presentation() are asked
THEN Emoji reports the broader set and Emoji_Presentation the narrower one.

## 0004 — East Asian Width
SCENARIO width classes are reported faithfully
GIVEN representative Na, A, H, W, F, and unassigned scalars
WHEN u89_east_asian_width() is asked
THEN each class is returned and unassigned scalars default to N.

## 0005 — Property table conformance
SCENARIO runtime properties match the pinned UCD
GIVEN the vendored Unicode 17.0.0 data
WHEN the committed tables are re-derived independently
THEN every range and value matches (just gendiff).

## 0006 — Non-scalars
SCENARIO non-scalars are never classified
GIVEN surrogate and out-of-range code points
WHEN any property predicate is asked
THEN it reports the safe negative result.
