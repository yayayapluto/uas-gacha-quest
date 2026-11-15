#ifndef RENDER_H
#define RENDER_H

#include "style.h"

#define UI_COLS 50          /* fixed content width, left aligned */
#define UI_ROWS 20          /* minimum terminal height */
#define GRID_MAX_ROWS 64
#define GRID_MAX_COLS 200

typedef struct {
    unsigned int cp;   /* unicode codepoint, 0 = empty cell */
    Style st;
} Cell;

void render_init(void);
void render_clear(void);
void render_text(int row, int col, Style s, const char *fmt, ...);
void render_text_clipped(int row, int col, int maxW, Style s, const char *fmt, ...);
/* Draws text where '~' switches between base and alt style, so one line can
   mix colours without building ANSI strings. Returns the cells written. */
int render_span(int row, int col, int maxW, Style base, Style alt, const char *text);
void render_char(int row, int col, Style s, unsigned int cp);
/* flush: interactive = styled, centered, single write; fallback = plain rows */
void render_flush(void);
int ui_visible_len(const char *s);
int render_max_row(void);   /* lowest row with content, for stacking widgets */
unsigned int render_cell_cp(int row, int col); /* codepoint at a cell, 0 when empty */

#endif
