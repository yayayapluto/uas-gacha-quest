#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "render.h"
#include "caps.h"
#include "term.h"
#include "widgets.h"

static Cell grid[GRID_MAX_ROWS][GRID_MAX_COLS];
static char outBuf[32 * 1024];
static int maxUsedRow = -1;

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
    maxUsedRow = -1;
    widget_footer_reset();
}

void render_char(int row, int col, Style s, unsigned int cp)
{
    if (row < 0 || row >= GRID_MAX_ROWS || col < 0 || col >= GRID_MAX_COLS) return;
    grid[row][col].cp = cp;
    grid[row][col].st = s;
    if (row > maxUsedRow) maxUsedRow = row;
}

int render_max_row(void) { return maxUsedRow; }

unsigned int render_cell_cp(int row, int col)
{
    if (row < 0 || row >= GRID_MAX_ROWS || col < 0 || col >= GRID_MAX_COLS) return 0;
    return grid[row][col].cp;
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

/* Same as render_text but stops after maxW cells, so a long log line cannot
   overwrite the border of the panel it is drawn in. */
void render_text_clipped(int row, int col, int maxW, Style s, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    const char *p;
    int c = 0;

    if (maxW <= 0) return;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    for (p = buf; *p && c < maxW;) {
        int adv, cp;
        if (*p == '\033') {
            const char *q = p + 1;
            if (*q == '[') {
                q++;
                while (*q && !isalpha((unsigned char)*q)) q++;
                if (*q) q++;
                p = q;
                continue;
            }
        }
        cp = decodeCp(p, &adv);
        render_char(row, col + c, s, (unsigned int)cp);
        c++;
        p += adv;
    }
}

/* Mixed-style text: '~' toggles between base and alt, and is not drawn. */
int render_span(int row, int col, int maxW, Style base, Style alt, const char *text)
{
    int c = 0;
    int useAlt = 0;
    const char *p = text;

    if (maxW <= 0 || text == NULL) return 0;
    while (*p && c < maxW) {
        int adv, cp;
        if (*p == '~') { useAlt = !useAlt; p++; continue; }
        cp = decodeCp(p, &adv);
        render_char(row, col + c, useAlt ? alt : base, (unsigned int)cp);
        c++;
        p += adv;
    }
    return c;
}

void render_flush(void)
{
    int r;
    size_t n = 0;
    int interactive = caps_interactive();
    int maxRow = render_max_row();

    if (interactive) {
        if (caps_cols() < MIN_COLS || caps_rows() < MIN_ROWS) {
            printf("\033[H\033[2J\033[0mPerbesar terminal (min %dx%d)", MIN_COLS, MIN_ROWS);
            fflush(stdout);
            return;
        }
        n += (size_t)sprintf(outBuf + n, "\033[H\033[2J");
    }

    for (r = 0; r <= maxRow; r++) {
        int last = -1;
        Style cur = ST_TEXT;
        int started = 0;
        int c;
        for (c = GRID_MAX_COLS - 1; c >= 0; c--)
            if (grid[r][c].cp != 0) { last = c; break; }
        if (last < 0 && !interactive) continue;
        if (interactive)
            n += (size_t)sprintf(outBuf + n, "\033[%d;1H", r + 1);
        cur = ST_TEXT;
        for (c = 0; c <= last; c++) {
            Cell *cell = &grid[r][c];
            char enc[8];
            int len;
            if (interactive) {
                if (!started || cur != cell->st) {
                    n += (size_t)sprintf(outBuf + n, "\033[0m");
                    if (cell->st >= ST_BIOME_BASE && cell->st < ST_BIOME_BASE + BIOME_COUNT) {
                        /* biome cells carry their own background, so a wall
                           reads as a solid block instead of a lone glyph */
                        n += (size_t)sprintf(outBuf + n, "%s", style_bg(cell->st));
                        n += (size_t)sprintf(outBuf + n, "%s", style_fg(cell->st));
                    } else if (cell->st == ST_SELECTED) {
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
