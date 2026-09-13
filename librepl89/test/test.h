#ifndef TEST_H
#define TEST_H

extern int failures;
extern int checks;

void rp_check(int cond, const char *what);
void rp_check_ctx(int cond, const char *what, const char *ctx);

/* 1 when [p, p+n) consists only of CR and CUU/CUD/CUF escapes. */
int rp_only_motion(const char *p, size_t n);

void test_buf(void);
void test_edit(void);
void test_history(void);
void test_render(void);
void test_key(void);
void test_paste(void);
void test_tty(void);
void test_session(void);
void test_fault(void);
void test_example(void);

#endif
