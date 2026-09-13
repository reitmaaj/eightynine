#include "test.h"
#include "u89.h"

void test_props(void)
{
    /* Default_Ignorable_Code_Point */
    u89_check(u89_default_ignorable(0x00ADUL) == 1, "DIC soft hyphen");
    u89_check(u89_default_ignorable(0x200BUL) == 1, "DIC ZWSP");
    u89_check(u89_default_ignorable(0x2060UL) == 1, "DIC WJ");
    u89_check(u89_default_ignorable(0x034FUL) == 1, "DIC CGJ");
    u89_check(u89_default_ignorable(0x0041UL) == 0, "DIC 'A' excluded");
    u89_check(u89_default_ignorable(0x0020UL) == 0, "DIC space excluded");

    /* Pattern_White_Space */
    u89_check(u89_pattern_whitespace(0x0009UL) == 1, "PWS tab");
    u89_check(u89_pattern_whitespace(0x0020UL) == 1, "PWS space");
    u89_check(u89_pattern_whitespace(0x200EUL) == 1, "PWS LRM");
    u89_check(u89_pattern_whitespace(0x3000UL) == 0, "PWS ideographic space excluded");

    /* Pattern_Syntax */
    u89_check(u89_pattern_syntax(0x002BUL) == 1, "PATS +");
    u89_check(u89_pattern_syntax(0x0024UL) == 1, "PATS $");
    u89_check(u89_pattern_syntax(0x002DUL) == 1, "PATS -");
    u89_check(u89_pattern_syntax(0x005FUL) == 0, "PATS _ excluded");
    u89_check(u89_pattern_syntax(0x0041UL) == 0, "PATS 'A' excluded");

    /* Join_Control */
    u89_check(u89_join_control(0x200CUL) == 1, "JC ZWNJ");
    u89_check(u89_join_control(0x200DUL) == 1, "JC ZWJ");
    u89_check(u89_join_control(0x200EUL) == 0, "JC LRM excluded");

    /* non-scalar rejected for every predicate */
    u89_check(u89_default_ignorable(0xD800UL) == 0, "DIC surrogate rejected");
    u89_check(u89_pattern_syntax(0xD800UL) == 0, "PATS surrogate rejected");
    u89_check(u89_pattern_whitespace(0xD800UL) == 0, "PWS surrogate rejected");
    u89_check(u89_join_control(0x110000UL) == 0, "JC out-of-range rejected");

    /* General_Category */
    u89_check(u89_general_category(0x0041UL) == U89_GC_Lu, "GC 'A' Lu");
    u89_check(u89_general_category(0x0061UL) == U89_GC_Ll, "GC 'a' Ll");
    u89_check(u89_general_category(0x0030UL) == U89_GC_Nd, "GC '0' Nd");
    u89_check(u89_general_category(0x0020UL) == U89_GC_Zs, "GC space Zs");
    u89_check(u89_general_category(0x200DUL) == U89_GC_Cf, "GC ZWJ Cf");
    u89_check(u89_general_category(0xE000UL) == U89_GC_Co, "GC PUA Co");
    u89_check(u89_general_category(0xAC00UL) == U89_GC_Lo, "GC Hangul Lo");
    u89_check(u89_general_category(0x0300UL) == U89_GC_Mn, "GC combining Mn");
    u89_check(u89_general_category(0xD800UL) == U89_GC_Cs, "GC surrogate Cs");
    u89_check(u89_general_category(0x0378UL) == 0, "GC unassigned Cn");
    u89_check(u89_general_category(0x110000UL) == 0, "GC out-of-range 0");

    /* u89_is_control: General_Category Cc only */
    u89_check(u89_is_control(0x0009UL) == 1, "control tab");
    u89_check(u89_is_control(0x000AUL) == 1, "control LF");
    u89_check(u89_is_control(0x001BUL) == 1, "control ESC");
    u89_check(u89_is_control(0x007FUL) == 1, "control DEL");
    u89_check(u89_is_control(0x0085UL) == 1, "control NEL (C1)");
    u89_check(u89_is_control(0x009BUL) == 1, "control CSI (C1)");
    u89_check(u89_is_control(0x0041UL) == 0, "control 'A' excluded");
    u89_check(u89_is_control(0x200DUL) == 0, "control ZWJ excluded (Cf)");
    u89_check(u89_is_control(0xD800UL) == 0, "control surrogate excluded");

    /* u89_is_mark: Mn, Mc, Me only */
    u89_check(u89_is_mark(0x0301UL) == 1, "mark combining acute Mn");
    u89_check(u89_is_mark(0x0903UL) == 1, "mark devanagari visarga Mc");
    u89_check(u89_is_mark(0x20DDUL) == 1, "mark enclosing circle Me");
    u89_check(u89_is_mark(0x0041UL) == 0, "mark 'A' excluded");
    u89_check(u89_is_mark(0x1F3FBUL) == 0, "mark skin tone is Sk");
    u89_check(u89_is_mark(0xD800UL) == 0, "mark surrogate excluded");

    /* East Asian Width classes */
    u89_check(u89_east_asian_width(0x0041UL) == U89_EAW_NA, "EAW 'A' Na");
    u89_check(u89_east_asian_width(0x4E00UL) == U89_EAW_W, "EAW CJK W");
    u89_check(u89_east_asian_width(0xFF21UL) == U89_EAW_F, "EAW fullwidth F");
    u89_check(u89_east_asian_width(0x00A1UL) == U89_EAW_A, "EAW inverted ! A");
    u89_check(u89_east_asian_width(0xFF61UL) == U89_EAW_H, "EAW halfwidth H");
    u89_check(u89_east_asian_width(0x0378UL) == U89_EAW_N, "EAW unassigned N");
    u89_check(u89_east_asian_width(0x1F600UL) == U89_EAW_W, "EAW emoji W");
    u89_check(u89_east_asian_width(0xD800UL) == U89_EAW_N, "EAW surrogate N");

    /* Emoji and Emoji_Presentation */
    u89_check(u89_is_emoji(0x1F600UL) == 1, "emoji grinning");
    u89_check(u89_is_emoji(0x00A9UL) == 1, "emoji copyright");
    u89_check(u89_is_emoji(0x0041UL) == 0, "emoji 'A' excluded");
    u89_check(u89_is_emoji(0xD800UL) == 0, "emoji surrogate excluded");
    u89_check(u89_is_emoji_presentation(0x1F600UL) == 1, "emoji pres grinning");
    u89_check(u89_is_emoji_presentation(0x00A9UL) == 0, "emoji pres copyright no");
    u89_check(u89_is_emoji_presentation(0x2708UL) == 0, "emoji pres airplane no");
    u89_check(u89_is_emoji_presentation(0xD800UL) == 0, "emoji pres surrogate no");
}
