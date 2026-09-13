#ifndef TEST_H
#define TEST_H

extern int failures;
extern int checks;

void u89_check(int cond, const char *what);
void u89_check_ctx(int cond, const char *what, const char *ctx);

void test_utf8(void);
void test_grapheme(void);
void test_xid(void);
void test_id(void);
void test_props(void);
void test_casefold(void);
void test_width(void);
void test_nfc(void);
void test_consistency(void);

#endif
