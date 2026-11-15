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
void render_char(int row, int col, Style s, unsigned int cp);
/* flush: interactive = styled, centered, single write; fallback = plain rows */
void render_flush(void);
int ui_visible_len(const char *s);
int render_max_row(void);   /* lowest row with content, for stacking widgets */

#endif
