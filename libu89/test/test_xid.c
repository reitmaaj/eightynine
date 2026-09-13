#include "test.h"
#include "u89.h"

void test_xid(void)
{
    /* ASCII identifier chars */
    u89_check(u89_xid_start(0x0041UL) == 1, "XID_Start 'A'");
    u89_check(u89_xid_continue(0x005FUL) == 1, "XID_Continue '_'");
    u89_check(u89_xid_continue(0x0030UL) == 1, "XID_Continue '0'");

    /* I. reject non-start scalars */
    u89_check(u89_xid_start(0x005FUL) == 0, "XID_Start '_' rejected");
    u89_check(u89_xid_start(0x0030UL) == 0, "XID_Start '0' rejected");
    u89_check(u89_xid_start(0x0020UL) == 0, "XID_Start space rejected");
    u89_check(u89_xid_start(0x0301UL) == 0, "XID_Start combining mark rejected");

    /* C. Greek, CJK start */
    u89_check(u89_xid_start(0x0391UL) == 1, "XID_Start Greek Alpha");
    u89_check(u89_xid_start(0x4E00UL) == 1, "XID_Start CJK ideograph");
    u89_check(u89_xid_continue(0x0391UL) == 1, "XID_Continue Greek");

    /* non-scalar rejected */
    u89_check(u89_xid_start(0xD800UL) == 0, "XID_Start surrogate rejected");
    u89_check(u89_xid_start(0x110000UL) == 0, "XID_Start out-of-range rejected");
}
