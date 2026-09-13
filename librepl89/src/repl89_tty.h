#ifndef REPL89_TTY_H
#define REPL89_TTY_H

#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>

#include "repl89_internal.h"

typedef struct repl89_tty {
    int in_fd;
    int out_fd;
    struct termios saved;
    int raw;
    unsigned int rows;
    unsigned int cols;
} repl89_tty;

/* Enter raw mode on in_fd, enable bracketed paste on out_fd, and remember
   the previous termios state. Returns REPL89_ETTY when in_fd is not a tty. */
repl89_error repl89_tty_enter(repl89_tty *t, int in_fd, int out_fd);

/* Disable bracketed paste and restore the saved termios state. Safe to call
   when raw mode is not active. */
repl89_error repl89_tty_leave(repl89_tty *t);

/* Query TIOCGWINSZ on out_fd. Returns REPL89_EIO on failure. */
repl89_error repl89_tty_size(repl89_tty *t);

/* Write all bytes to out_fd. Returns 0 on success, -1 on error. */
int repl89_tty_write(const repl89_tty *t, const char *p, size_t n);

/* One read(2) from in_fd. */
ssize_t repl89_tty_read(const repl89_tty *t, char *p, size_t n);

/* Fault-injection hooks for tests. reset restores the real syscalls. */
void repl89_tty_set_hooks(int (*getattr)(int, struct termios *),
                          int (*setattr)(int, int, const struct termios *),
                          int (*winsize)(int, struct winsize *),
                          ssize_t (*rd)(int, void *, size_t),
                          ssize_t (*wr)(int, const void *, size_t));
void repl89_tty_reset_hooks(void);

#endif
