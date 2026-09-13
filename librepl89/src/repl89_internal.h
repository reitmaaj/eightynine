#ifndef REPL89_INTERNAL_H
#define REPL89_INTERNAL_H

#include <stddef.h>

#include "../include/repl89.h"

/* ---- Fault-injectable allocation ---------------------------------------- */

void *repl89_mem_alloc(size_t n);
void *repl89_mem_realloc(void *p, size_t n);
void repl89_mem_free(void *p);
void repl89_mem_set_hooks(void *(*alloc)(size_t),
                          void *(*grow)(void *, size_t),
                          void (*release)(void *));
void repl89_mem_reset_hooks(void);

/* ---- Growable byte buffer ------------------------------------------------ */

typedef struct repl89_buf {
    char *data;
    size_t len;
    size_t cap;
} repl89_buf;

void repl89_buf_init(repl89_buf *b);
void repl89_buf_free(repl89_buf *b);
void repl89_buf_clear(repl89_buf *b);
repl89_error repl89_buf_reserve(repl89_buf *b, size_t extra);
repl89_error repl89_buf_insert(repl89_buf *b, size_t pos, const char *p,
                               size_t n);
repl89_error repl89_buf_delete(repl89_buf *b, size_t pos, size_t n);

/* Round a required capacity up to the next power-of-two growth step. */
size_t repl89_cap_for(size_t need);

/* ---- Content policy ------------------------------------------------------ */

/* REPL89_OK when [p, p+n) is valid UTF-8 containing only LF/HT as controls;
   REPL89_EUTF8 for malformed input; REPL89_EINVAL for a forbidden control. */
repl89_error repl89_content_check(const char *p, size_t n);

/* ---- Editor model -------------------------------------------------------- */

typedef struct repl89_edit {
    repl89_buf buf;
    size_t cursor;
} repl89_edit;

void repl89_edit_init(repl89_edit *e);
void repl89_edit_free(repl89_edit *e);
void repl89_edit_reset(repl89_edit *e);
repl89_error repl89_edit_insert(repl89_edit *e, const char *p, size_t n);
repl89_error repl89_edit_delete_prev(repl89_edit *e);
repl89_error repl89_edit_delete_next(repl89_edit *e);
void repl89_edit_left(repl89_edit *e);
void repl89_edit_right(repl89_edit *e);
void repl89_edit_home(repl89_edit *e);
void repl89_edit_end(repl89_edit *e);
void repl89_edit_kill_begin(repl89_edit *e);
void repl89_edit_kill_end(repl89_edit *e);

/* ---- Output sink --------------------------------------------------------- */

typedef struct repl89_sink {
    void *ctx;
    int (*emit)(void *ctx, const char *p, size_t n);
} repl89_sink;

/* ---- Display updates ----------------------------------------------------- */

/* What a batch of input operations changed about the display. Values are
   ordered by dominance, so several operations collapse into the strongest. */
typedef enum repl89_update {
    REPL89_UPDATE_NONE = 0,
    REPL89_UPDATE_CURSOR,
    REPL89_UPDATE_CONTENT,
    REPL89_UPDATE_RESIZE
} repl89_update;

/* ---- Renderer ------------------------------------------------------------ */

typedef struct repl89_region {
    unsigned int cols;       /* terminal width used to construct the region */
    unsigned int rows;
    unsigned int cursor_row;
    unsigned int cursor_col; /* logical column, 0..cols inclusive */
    unsigned int end_row;
    unsigned int end_col;    /* logical column, 0..cols inclusive */
} repl89_region;

/* Width in terminal cells of the grapheme cluster [start, end) under the v0
   policy: a wide/fullwidth base or an emoji-presentation scalar is two
   cells; marks and default-ignorables add none; otherwise one. */
int repl89_cluster_width(const char *s, size_t start, size_t end);

/* Draw the submission starting at the current cursor position (the region's
   first row, column 0), using the primary prompt for the first logical line
   and cont (may be NULL) for later ones. Fills *out with the region's row
   count and the cursor's region-relative position. */
repl89_error repl89_render_draw(const repl89_edit *e, const char *prompt,
                                const char *cont, unsigned int cols,
                                unsigned int tab_width,
                                const repl89_sink *sink, repl89_region *out);

/* Purely compute the rendered region for the given geometry without
   emitting anything. */
void repl89_layout(const repl89_edit *e, const char *prompt, const char *cont,
                   unsigned int cols, unsigned int tab_width,
                   repl89_region *out);

/* Move the physical cursor from old's recorded cursor position to next's.
   The displayed text must not have changed; only cursor-motion escapes are
   emitted. */
repl89_error repl89_render_cursor(const repl89_sink *sink,
                                  const repl89_region *old,
                                  const repl89_region *next);

/* Repaint the submission over a previously drawn region: replacement rows
   are written before obsolete suffixes are erased, and whole stale rows are
   erased after the new content is complete. old->rows == 0 draws fresh. */
repl89_error repl89_render_content(const repl89_edit *e, const char *prompt,
                                   const char *cont, unsigned int cols,
                                   unsigned int tab_width,
                                   const repl89_sink *sink,
                                   const repl89_region *old,
                                   repl89_region *out);

/* Result of reconstructing an old rendering under a new terminal width:
   the physical rows the old hard rows now occupy and where the old cursor
   now lies. Old hard rows never merge when growing. */
typedef struct repl89_reflow {
    unsigned int rows;
    unsigned int cursor_row;
    unsigned int cursor_col;
} repl89_reflow;

void repl89_layout_reflow(const repl89_edit *e, const char *prompt,
                          const char *cont, unsigned int old_cols,
                          unsigned int new_cols, unsigned int tab_width,
                          repl89_reflow *out);

/* Repaint after a terminal resize. The old region is reinterpreted through
   repl89_layout_reflow, so no assumption is made that its recorded cursor
   position still describes the physical terminal. */
repl89_error repl89_render_resize(const repl89_edit *e, const char *prompt,
                                  const char *cont, unsigned int cols,
                                  unsigned int tab_width,
                                  const repl89_sink *sink,
                                  const repl89_region *old,
                                  repl89_region *out);

/* Clear a previously drawn region. The terminal cursor must be at the
   region's recorded cursor position. Leaves the cursor at the region's
   first row, column 0. */
repl89_error repl89_render_clear(const repl89_sink *sink,
                                 const repl89_region *reg);

/* Move from the region's recorded cursor position to the end of the last
   rendered row and emit CRLF, so application output begins on a new line. */
repl89_error repl89_render_finalize(const repl89_sink *sink,
                                    const repl89_region *reg);

/* ---- Vertical movement --------------------------------------------------- */

#define REPL89_WANT_CURSOR ((unsigned int)-1)

typedef struct repl89_vmove {
    unsigned int cur_row;
    unsigned int cur_col;
    unsigned int rows;
    size_t target;       /* byte offset in the adjacent row (in_range only) */
    unsigned int target_col;
    int in_range;        /* 0 when direction leaves the first/last row */
} repl89_vmove;

/* Query the visual row above (direction < 0) or below (direction > 0) the
   cursor's row, choosing the grapheme boundary closest to want_col (or the
   cursor's own column for REPL89_WANT_CURSOR). Pure layout; no rendering. */
void repl89_layout_vertical(const repl89_edit *e, const char *prompt,
                            const char *cont, unsigned int cols,
                            unsigned int tab_width, int direction,
                            unsigned int want_col, repl89_vmove *out);

/* ---- Bracketed paste ----------------------------------------------------- */

/* Sanitizing paste accumulator: CRLF and CR become LF, HT is preserved,
   every other Cc scalar is discarded, and malformed UTF-8 rejects the whole
   paste. The accumulated bytes are only inserted by the caller after
   repl89_paste_finish() succeeds. */
typedef struct repl89_paste {
    repl89_buf out;
    char partial[4];
    size_t partial_len;
    int pending_cr;
    repl89_error error;
} repl89_paste;

void repl89_paste_init(repl89_paste *p);
void repl89_paste_free(repl89_paste *p);
void repl89_paste_reset(repl89_paste *p);
repl89_error repl89_paste_feed(repl89_paste *p, const char *s, size_t n);
repl89_error repl89_paste_finish(repl89_paste *p, repl89_text *text);

/* ---- Key decoder --------------------------------------------------------- */

typedef enum repl89_key_type {
    REPL89_KEY_IGNORE = 0,
    REPL89_KEY_TEXT,
    REPL89_KEY_LEFT,
    REPL89_KEY_RIGHT,
    REPL89_KEY_UP,
    REPL89_KEY_DOWN,
    REPL89_KEY_HOME,
    REPL89_KEY_END,
    REPL89_KEY_DELETE,
    REPL89_KEY_BACKSPACE,
    REPL89_KEY_LF,
    REPL89_KEY_SUBMIT,
    REPL89_KEY_CANCEL,
    REPL89_KEY_CTRL_D,
    REPL89_KEY_CTRL_U,
    REPL89_KEY_CTRL_K,
    REPL89_KEY_CTRL_P,
    REPL89_KEY_CTRL_N,
    REPL89_KEY_PASTE_BEGIN,
    REPL89_KEY_PASTE_END
} repl89_key_type;

typedef struct repl89_key {
    repl89_key_type type;
    size_t len; /* bytes of REPL89_KEY_TEXT */
} repl89_key;

/* Decode one key from [p, p+n). Returns bytes consumed, or 0 when the input
   is an incomplete escape sequence and more bytes are required. */
size_t repl89_key_next(const char *p, size_t n, repl89_key *key);

/* ---- History ------------------------------------------------------------- */

typedef struct repl89_entry {
    char *data;
    size_t len;
} repl89_entry;

typedef struct repl89_hist {
    repl89_entry *items;
    size_t count;
    size_t cap;
    size_t limit;
    size_t nav;
} repl89_hist;

void repl89_hist_init(repl89_hist *h, size_t limit);
void repl89_hist_free(repl89_hist *h);
void repl89_hist_clear(repl89_hist *h);
repl89_error repl89_hist_add(repl89_hist *h, const char *p, size_t n);
const repl89_entry *repl89_hist_at(const repl89_hist *h, size_t i);
void repl89_hist_nav_reset(repl89_hist *h);
const repl89_entry *repl89_hist_nav_prev(repl89_hist *h);
const repl89_entry *repl89_hist_nav_next(repl89_hist *h);

#endif
