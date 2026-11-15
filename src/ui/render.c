#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "render.h"
#include "caps.h"
#include "term.h"

static Cell grid[GRID_MAX_ROWS][GRID_MAX_COLS];
static char outBuf[32 * 1024];

/* visible column width of a UTF-8 string: ANSI escapes skipped, 1 col per codepoint */
int ui_visible_len(const char *s)
{
    int n = 0;
    while (*s) {
        if (*s == '\033') {
            s++;
            if (*s == '[') {
                s++;
                while (*s && !isalpha((unsigned char)*s)) s++;
                if (*s) s++;
            }
            continue;
        }
        n++;
        s++;
        while ((*s & 0xC0) == 0x80) s++;
    }
    return n;
}

static int decodeCp(const char *s, int *adv)
{
    unsigned char c = (unsigned char)s[0];
    unsigned int cp;
    int a = 1;
    if (c < 0x80) cp = c;
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; a = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; a = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; a = 4; }
    else { cp = '?'; a = 1; }
    {
        int i;
        for (i = 1; i < a; i++) {
            unsigned char cc = (unsigned char)s[i];
            if ((cc & 0xC0) != 0x80) { *adv = i; return '?'; }
            cp = (cp << 6) | (cc & 0x3F);
        }
    }
    *adv = a;
    return (int)cp;
}

static int encodeCp(unsigned int cp, char *out)
{
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

void render_init(void) { render_clear(); }

void render_clear(void)
{
    int r, c;
    for (r = 0; r < GRID_MAX_ROWS; r++)
        for (c = 0; c < GRID_MAX_COLS; c++) {
            grid[r][c].cp = 0;
            grid[r][c].st = ST_TEXT;
        }
}

void render_char(int row, int col, Style s, unsigned int cp)
{
    if (row < 0 || row >= GRID_MAX_ROWS || col < 0 || col >= GRID_MAX_COLS) return;
    grid[row][col].cp = cp;
    grid[row][col].st = s;
}

void render_text(int row, int col, Style s, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    const char *p;
    int c = col;

    if (row < 0 || row >= GRID_MAX_ROWS || col < 0 || col >= GRID_MAX_COLS) return;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    for (p = buf; *p;) {
        int adv, cp;
        if (*p == '\033') { /* embedded escape: change style of the rest */
            const char *q = p + 1;
            if (*q == '[') {
                q++;
                while (*q && !isalpha((unsigned char)*q)) q++;
                if (*q) q++;
                p = q;
                continue; /* styles come from the style argument, skip raw codes */
            }
        }
        cp = decodeCp(p, &adv);
        render_char(row, c, s, (unsigned int)cp);
        c++;
        p += adv;
    }
}

void render_flush(void)
{
    int r;
    size_t n = 0;
    int interactive = caps_interactive();
    int xoff = 0;

    if (interactive) {
        if (caps_cols() < UI_COLS || caps_rows() < UI_ROWS) {
            int col = caps_cols() > 0 ? (caps_cols() - 28) / 2 : 0;
            if (col < 0) col = 0;
            printf("\033[H\033[2J\033[0m%s", "Perbesar terminal (min 80x24)");
            if (caps_cols() >= 28) printf("\033[1;%dH", col + 1);
            printf("\033[H");
            fflush(stdout);
            return;
        }
        xoff = (caps_cols() - UI_COLS) / 2;
        n += (size_t)sprintf(outBuf + n, "\033[H");
    }

    for (r = 0; r < UI_ROWS; r++) {
        int last = -1;
        Style cur = ST_TEXT;
        int started = 0;
        int c;
        for (c = UI_COLS - 1; c >= 0; c--)
            if (grid[r][c].cp != 0) { last = c; break; }
        if (last < 0 && !interactive) continue;
        if (interactive) {
            if (r > 0) n += (size_t)sprintf(outBuf + n, "\033[%d;%dH", r + 1, xoff + 1);
            n += (size_t)sprintf(outBuf + n, "\033[2K");
        }
        cur = ST_TEXT;
        for (c = 0; c <= last; c++) {
            Cell *cell = &grid[r][c];
            char enc[8];
            int len;
            if (interactive) {
                if (!started || cur != cell->st) {
                    n += (size_t)sprintf(outBuf + n, "\033[0m");
                    if (cell->st == ST_SELECTED) {
                        n += (size_t)sprintf(outBuf + n, "%s", style_bg(ST_SELECTED));
                        n += (size_t)sprintf(outBuf + n, "%s", style_fg(ST_ACCENT));
                    } else {
                        n += (size_t)sprintf(outBuf + n, "%s", style_fg(cell->st));
                        if (cell->st == ST_TITLE) n += (size_t)sprintf(outBuf + n, "\033[1m");
                    }
                    cur = cell->st;
                }
            }
            if (cell->cp == 0) {
                if (!started) continue;
                enc[0] = ' ';
                len = 1;
            } else {
                started = 1;
                len = encodeCp(cell->cp, enc);
            }
            memcpy(outBuf + n, enc, (size_t)len);
            n += (size_t)len;
        }
        if (started || interactive) {
            if (interactive) n += (size_t)sprintf(outBuf + n, "\033[0m");
            if (!interactive) outBuf[n++] = '\n';
        }
        if (n > sizeof outBuf - 64) break;
    }
    if (n > 0) {
        fwrite(outBuf, 1, n, stdout);
        fflush(stdout);
    }
}
