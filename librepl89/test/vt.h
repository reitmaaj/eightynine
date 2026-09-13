#ifndef VT_H
#define VT_H

#include <stddef.h>

#define VT_ROWS 24
#define VT_COLS 80
#define VT_CELL_MAX 8

typedef struct vt_cell {
    char bytes[VT_CELL_MAX];
    size_t len;
    int width;
    int cont;
} vt_cell;

typedef struct vt_screen {
    vt_cell cells[VT_ROWS][VT_COLS];
    unsigned int cols;
    unsigned int row;
    unsigned int col;
} vt_screen;

void vt_init(vt_screen *s);

/* Set the logical width of an empty screen before feeding output that was
   rendered for that width. */
void vt_set_cols(vt_screen *s, unsigned int cols);

/* Reflow the screen as an xterm-like terminal does on resize: cells within
   each hard row re-wrap at the new width while hard rows never merge. */
void vt_resize(vt_screen *s, unsigned int new_cols);

void vt_feed(vt_screen *s, const char *p, size_t n);
void vt_row_text(const vt_screen *s, unsigned int row, char *out, size_t cap);

#endif
