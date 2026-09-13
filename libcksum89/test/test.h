#ifndef CKSUM89_TEST_H
#define CKSUM89_TEST_H

/*
 * test.h - tiny C89 test harness for libcksum89 test programs.
 * Not part of the library; not green-linted. Must still compile
 * warning-clean as strict C89.
 */

#include <stddef.h>

#include "cksum89.h"

extern int cksum89_test_failures;
extern int cksum89_test_checks;

void cksum89_test_check(int cond, const char *what);
void cksum89_test_u16(cksum89_u16 got, cksum89_u16 want, const char *what);
void cksum89_test_u32(cksum89_u32 got, cksum89_u32 want, const char *what);
void cksum89_test_u64(cksum89_u64 got, cksum89_u32 hi, cksum89_u32 lo,
                      const char *what);

void cksum89_test_u16_at(cksum89_u16 got, cksum89_u16 want, const char *what,
                         unsigned long where);
void cksum89_test_u32_at(cksum89_u32 got, cksum89_u32 want, const char *what,
                         unsigned long where);
void cksum89_test_u64_at(cksum89_u64 got, cksum89_u32 hi, cksum89_u32 lo,
                         const char *what, unsigned long where);

void test_vectors(void);
void test_crc32_iso_hdlc(void);
void test_crc32c(void);
void test_inet16(void);
void test_stream(void);
void test_tables(void);

#endif /* CKSUM89_TEST_H */
