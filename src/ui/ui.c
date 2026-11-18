#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "config.h"
#include "util.h"
#include "ui.h"
#include "term.h"
#include "caps.h"
#include "render.h"
#include "widgets.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

static char lastMessage[256];

#define LOG_PARTS 8

typedef struct {
    char text[96];
    Style st;
} LogPart;

typedef struct {
    LogPart part[LOG_PARTS];
    int count;
} LogLine;

static LogLine logLines[24];
static int logCount = 0;

void ui_init(void)
{
    caps_detect();
    style_init();
    render_init();
    atexit(ui_shutdown);
    term_init();
}

void ui_shutdown(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    term_shutdown();
}

int ui_interactive(void) { return caps_interactive(); }

void set_message(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(lastMessage, sizeof lastMessage, fmt, args);
    va_end(args);
}

const char *last_message(void)
{
    return lastMessage;
}

void ui_log_clear(void) { logCount = 0; }

/* Appends one styled segment to a log line; fallback mode prints the text. */
static void logPart(LogLine *l, const char *text, Style st)
{
    LogPart *p;
    if (l->count >= LOG_PARTS) return;
    p = &l->part[l->count++];
    snprintf(p->text, sizeof p->text, "%s", text);
    p->st = st;
    if (!caps_interactive()) printf("%s", text);
}

void ui_log_style(Style st, const char *fmt, ...)
{
    char buf[192];
    va_list args;
    LogLine *l;

    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    if (logCount < 24) {
        l = &logLines[logCount++];
        l->count = 0;
        logPart(l, buf, st);
    } else if (!caps_interactive()) {
        printf("%s", buf);
    }
    if (!caps_interactive()) printf("\n");
}

void ui_log(const char *fmt, ...)
{
    char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    ui_log_style(ST_TEXT, "%s", buf);
}

void ui_log_event(const char *actor, const char *action, const char *target,
                  const char *value, Style actorSt, Style targetSt, Style valueSt)
{
    LogLine *l;

    if (logCount >= 24) {
        if (!caps_interactive()) {
            printf("%s %s %s\n", actor != NULL ? actor : "", action != NULL ? action : "",
                   target != NULL ? target : "");
        }
        return;
    }
    l = &logLines[logCount++];
    l->count = 0;
    /* one segment per semantic piece: actor, verb, target, value; the spaces
       are baked into the next segment so the count stays at four */
    if (actor != NULL) logPart(l, actor, actorSt);
    if (action != NULL) {
        char verb[96];
        snprintf(verb, sizeof verb, " %s", action);
        logPart(l, verb, ST_TEXT);
    }
    if (target != NULL) {
        char tgt[96];
        snprintf(tgt, sizeof tgt, " %s", target);
        logPart(l, tgt, targetSt);
    }
    if (value != NULL) {
        char val[96];
        snprintf(val, sizeof val, " %s", value);
        logPart(l, val, valueSt);
    }
    if (!caps_interactive()) printf("\n");
}

int ui_log_count(void) { return logCount; }

int ui_log_part_count(int i)
{
    if (i < 0 || i >= logCount) return 0;
    return logLines[i].count;
}

const char *ui_log_part_text(int i, int part)
{
    if (i < 0 || i >= logCount) return "";
    if (part < 0 || part >= logLines[i].count) return "";
    return logLines[i].part[part].text;
}

Style ui_log_part_style(int i, int part)
{
    if (i < 0 || i >= logCount || part < 0 || part >= logLines[i].count) return ST_TEXT;
    return logLines[i].part[part].st;
}

/* Concatenated text of one line, for grep-style checks and plain output. */
const char *ui_log_text(int i)
{
    static char joined[400];
    int k;
    size_t n = 0;

    joined[0] = '\0';
    if (i < 0 || i >= logCount) return joined;
    for (k = 0; k < logLines[i].count && n < sizeof joined - 1; k++) {
        int w = snprintf(joined + n, sizeof joined - n, "%s", logLines[i].part[k].text);
        if (w < 0) break;
        n += (size_t)w;
    }
    return joined;
}

Style ui_log_style_at(int i)
{
    if (i < 0 || i >= logCount || logLines[i].count == 0) return ST_TEXT;
    return logLines[i].part[0].st;
}

void ui_log_draw(int row, int col, int maxW, int i)
{
    int k, c = 0;

    if (i < 0 || i >= logCount) return;
    for (k = 0; k < logLines[i].count && c < maxW; k++)
        c += render_span(row, col + c, maxW - c, logLines[i].part[k].st,
                         logLines[i].part[k].st, logLines[i].part[k].text);
}

void ui_frame(const char *screen)
{
    const Glyphs *g = caps_glyphs();
    int i;

    render_clear();
    if (caps_unicode())
        render_text(0, 0, ST_TITLE, "GACHA QUEST · %s", screen);
    else
        render_text(0, 0, ST_TITLE, "GACHA QUEST > %s", screen);
    for (i = 0; i < UI_COLS; i++)
        render_text(1, i, ST_BORDER, "%s", g->hz);
}

void ui_flush(void) { render_flush(); }

void ui_panel(int row, int col, int w, int h, const char *title)
{
    widget_panel(row, col, w, h, title);
}

void ui_hp_bar(int row, int col, int w, int hp, int maxHp)
{
    widget_hp_bar(row, col, w, hp, maxHp);
}

void ui_hp_bar_n(int row, int col, int w, int hp, int maxHp)
{
    widget_hp_bar_n(row, col, w, hp, maxHp);
}

void ui_bar(int row, int col, int w, int cur, int max, Style fill)
{
    widget_bar(row, col, w, cur, max, fill);
}

void ui_menu_v(int row, int col, const char *const *items, const int *nums, int n, int sel)
{
    widget_menu_v(row, col, items, nums, n, sel);
}

void ui_menu_h(int row, int col, const char *const *items, const int *nums, int n, int sel)
{
    widget_menu_h(row, col, items, nums, n, sel);
}

void ui_footer(const char *extra)
{
    widget_footer(extra);
}

void ui_footer_line(const char *text)
{
    widget_footer_line(text);
}

void ui_message(int row)
{
    if (caps_interactive()) widget_msg(row, 0, UI_COLS, lastMessage);
}

/* Player status panel: Pemain/Level/HP bar/Gold+Potion inside a bordered panel. */
void ui_status_panel_at(const Player *p, int row, int col, int w)
{
    char num[16];
    int off;
    ui_panel(row, col, w, 6, "Status");
    render_text(row + 1, col + 2, ST_TEXT, "Pemain : %s", p->name);
    render_text(row + 2, col + 2, ST_TEXT, "Level  : %d (Skor %d)", p->level, p->score);
    render_text(row + 3, col + 2, ST_TEXT, "HP ");
    /* the ratio sits in the right gutter of the HP row, so the bar keeps its
       full width and the number is readable without counting blocks */
    snprintf(num, sizeof num, "%d/%d", p->hp, p->maxHp);
    ui_hp_bar(row + 3, col + 5, w - 6 - 2 - ui_visible_len(num), p->hp, p->maxHp);
    render_text(row + 3, col + w - 2 - ui_visible_len(num), style_hp(p->hp, p->maxHp),
                "%s", num);
    render_text(row + 4, col + 2, ST_TEXT, "Gold   : ");
    snprintf(num, sizeof num, "%d", p->gold);
    render_text(row + 4, col + 2 + 9, ST_GOLD, "%s", num);
    off = 2 + 9 + ui_visible_len(num);
    render_text(row + 4, col + off, ST_TEXT, "   Potion: %d", p->potion);
}

/* Fallback: identical text to the old screens (items, message, prompt). */
static int menuPickFallback(const char *const *items, const int *nums, int n,
                            int minNum, int maxNum, UiDrawFn draw, void *ctx)
{
    int i;
    draw(ctx, -1);
    ui_flush();
    for (i = 0; i < n; i++)
        printf(" %d. %s\n", nums[i], items[i]);
    if (lastMessage[0] != '\0')
        printf("%s\n", lastMessage);
    printf("Pilih: ");
    return readInt("", minNum, maxNum);
}

static void keyWait(int key)
{
    if (key < 0) {
#ifdef _WIN32
        Sleep(10);
#else
        struct timespec ts = { 0, 10000000 };
        nanosleep(&ts, NULL);
#endif
    }
}

int ui_menu_pick(const char *const *items, const int *nums, int n,
                 int minNum, int maxNum, UiDrawFn draw, void *ctx)
{
    if (!caps_interactive())
        return menuPickFallback(items, nums, n, minNum, maxNum, draw, ctx);
    {
        int sel = 0;
        char ch;
        for (;;) {
            draw(ctx, sel);
            ui_footer(NULL);
            ui_flush();
            {
                TermKey k = term_getkey(&ch);
                keyWait((int)k);
                switch (k) {
                case KEY_UP: sel = (sel + n - 1) % n; break;
                case KEY_DOWN: sel = (sel + 1) % n; break;
                case KEY_ENTER: return nums[sel];
                case KEY_ESC: {
                    int i, hasBack = 0;
                    for (i = 0; i < n; i++) if (nums[i] == 0) hasBack = 1;
                    if (hasBack) return 0;
                    break;
                }
                case KEY_CHAR: {
                    if (ch >= '0' && ch <= '9') {
                        int d = ch - '0', i;
                        for (i = 0; i < n; i++)
                            if (nums[i] == d && d >= minNum && d <= maxNum) return d;
                    } else if (ch == 'w' || ch == 'k') {
                        sel = (sel + n - 1) % n;
                    } else if (ch == 's' || ch == 'j') {
                        sel = (sel + 1) % n;
                    }
                    break;
                }
                case KEY_RESIZE:
                    draw(ctx, sel);
                    ui_flush();
                    break;
                case KEY_NONE:
                    /* poll timeout: nothing was consumed, so redrawing here
                       would only burn CPU and eat keys queued behind it */
                    break;
                default:
                    break;
                }
            }
        }
    }
}

/* Inline confirmation: keep the screen behind it and add one box at the
   bottom. The answer is chosen with Y/N, then confirmed with Enter so a
   buffered newline cannot leak into the next menu. */
int ui_confirm_box(const char *question)
{
    char qRows[3][192];
    int cols, termRows, contentW, qCount, height, top, i;
    int picked = -1;

    if (!caps_interactive()) return 0;
    cols = caps_cols();
    if (cols > UI_COLS) cols = UI_COLS;
    termRows = caps_rows();
    contentW = cols - 4;
    if (contentW < 8) contentW = 8;
    qCount = wrap_teks(question, contentW, qRows, 3);
    if (qCount < 1) qCount = 1;
    height = qCount + 3;

    top = termRows - height - 1;
    if (top < 2) top = 2;

    ui_panel(top, 0, cols, height, "Konfirmasi");
    {
        int r, c;
        for (r = 1; r < height - 1; r++)
            for (c = 1; c < cols - 1; c++)
                render_text(top + r, c, ST_TEXT, " ");
    }
    for (i = 0; i < qCount; i++)
        render_text_clipped(top + 1 + i, 2, contentW, ST_TEXT, "%s", qRows[i]);
    render_text_clipped(top + 1 + qCount, 2, contentW, ST_DIM,
                        "[Y] Ya   [N] Tidak   Esc Batal");
    ui_flush();

    while (picked < 0) {
        char ch;
        TermKey k = term_getkey(&ch);
        if (k == KEY_EOF || k == KEY_ESC) return 0;
        if (k == KEY_RESIZE) continue;
        if (k != KEY_CHAR) continue;
        if (ch == 'y' || ch == 'Y') picked = 1;
        else if (ch == 'n' || ch == 'N') picked = 0;
    }

    render_text_clipped(top + 1 + qCount, 2, contentW, ST_ACCENT,
                        "Pilihan: %s. Enter untuk lanjut, Esc batal.",
                        picked ? "Ya" : "Tidak");
    ui_flush();

    for (;;) {
        char ch;
        TermKey k = term_getkey(&ch);
        if (k == KEY_EOF || k == KEY_ESC) return 0;
        if (k == KEY_ENTER) return picked;
        if (k == KEY_CHAR && (ch == '\r' || ch == '\n')) return picked;
    }
}

static char inputScreen[64] = "Input";
static char inputPrompt[192];
static char inputBuf[256];
static char inputNote[192];
static Style inputNoteStyle = ST_DANGER;
static int inputCursorRow, inputCursorCol;
#define INPUT_MAX_ROWS 8

/* Byte length of the UTF-8 codepoint starting at s (>=1; clamped to n). */
static int cpLenAt(const char *s, int n)
{
    unsigned char c = (unsigned char)s[0];
    int len = 1;

    if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    while (len > 1 && (len > n || ((unsigned char)s[len - 1] & 0xC0) != 0x80)) len--;
    return len;
}

/* Display columns of n bytes: CJK/Hangul/fullwidth/emoji take two cells. */
static int widthOfBytes(const char *s, int n)
{
    int w = 0, i = 0;

    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        unsigned int cp;
        int len = cpLenAt(s + i, n - i);
        int j;

        if (c < 0x80) {
            cp = c;
        } else {
            cp = c & (len == 2 ? 0x1Fu : len == 3 ? 0x0Fu : 0x07u);
            for (j = 1; j < len; j++)
                cp = (cp << 6) | ((unsigned int)s[i + j] & 0x3Fu);
        }
        if (cp >= 0x0300 && cp <= 0x036F) {
            /* combining mark: no column of its own */
        } else if ((cp >= 0x1100 && cp <= 0x115F)
                   || (cp >= 0x2E80 && cp <= 0xA4CF)
                   || (cp >= 0xAC00 && cp <= 0xD7A3)
                   || (cp >= 0xF900 && cp <= 0xFAFF)
                   || (cp >= 0xFE30 && cp <= 0xFE4F)
                   || (cp >= 0xFF00 && cp <= 0xFF60)
                   || (cp >= 0xFFE0 && cp <= 0xFFE6)
                   || (cp >= 0x1F300 && cp <= 0x1FAFF)
                   || cp >= 0x20000) {
            w += 2;
        } else {
            w += 1;
        }
        i += len;
    }
    return w;
}

int lebar_tampil(const char *s)
{
    return s == NULL ? 0 : widthOfBytes(s, (int)strlen(s));
}

/* Splits text into at most maxRows lines of at most width columns, breaking
   on spaces. A single word wider than the line is split on the column budget,
   so nothing is dropped. Returns the row count. */
int wrap_teks(const char *text, int width, char rows[][192], int maxRows)
{
    const char *p = text != NULL ? text : "";
    int used = 1;

    if (width < 1 || maxRows <= 0) return 0;
    rows[0][0] = '\0';
    while (*p != '\0') {
        const char *w;
        int wordLen, off = 0, curW;

        while (*p == ' ') p++;
        if (*p == '\0') break;
        w = p;
        while (*p != '\0' && *p != ' ') p++;
        wordLen = (int)(p - w);
        curW = lebar_tampil(rows[used - 1]);

        if (curW > 0 && curW + 1 + widthOfBytes(w, wordLen) <= width) {
            snprintf(rows[used - 1] + curW, 192 - (size_t)curW, " %.*s", wordLen, w);
            continue;
        }
        if (curW > 0) {
            if (used >= maxRows) break;
            used++;
            rows[used - 1][0] = '\0';
        }
        while (off < wordLen) {
            int take = 0, tw = 0;
            while (off + take < wordLen) {
                int cl = cpLenAt(w + off + take, wordLen - off - take);
                int cw = widthOfBytes(w + off + take, cl);
                if (tw + cw > width) break;
                tw += cw;
                take += cl;
            }
            if (take == 0) take = cpLenAt(w + off, wordLen - off);
            if (take > wordLen - off) take = wordLen - off;
            snprintf(rows[used - 1], 192, "%.*s", take, w + off);
            off += take;
            if (off < wordLen) {
                if (used >= maxRows) return used;
                used++;
                rows[used - 1][0] = '\0';
            }
        }
    }
    return used;
}

/* Input box: header at the top, boxed prompt directly under it, so the
   question never floats away from the title. Content width comes from the
   terminal at render time, so a resize re-wraps instead of pushing text over
   the border. */
void render_input_box(const char *question, const char *buffer)
{
    int cols = caps_interactive() ? caps_cols() : UI_COLS;
    int termRows = caps_interactive() ? caps_rows() : UI_ROWS;
    int pad = cols < 20 ? 0 : 1;
    int contentW = cols - 2 - 2 * pad;
    int prefixW = 2;                               /* "> " */
    char qRows[INPUT_MAX_ROWS][192];
    char nRows[INPUT_MAX_ROWS][192];
    int qCount = 0, nCount = 0, height, top, i;
    int avail, bufW, shownW = 0, off = 0;
    const char *shown;

    if (contentW < 4) contentW = 4;
    memset(qRows, 0, sizeof qRows);
    memset(nRows, 0, sizeof nRows);
    qCount = wrap_teks(question, contentW, qRows, INPUT_MAX_ROWS);
    if (inputNote[0] != '\0')
        nCount = wrap_teks(inputNote, contentW, nRows, INPUT_MAX_ROWS);

    height = qCount + nCount + 1 + 2;
    top = 2;                       /* first row under the header line */
    if (top + height > termRows) top = termRows - height;
    if (top < 2) top = 2;

    ui_frame(inputScreen);

    ui_panel(top, 0, cols, height, "Input");
    for (i = 0; i < qCount; i++)
        render_text_clipped(top + 1 + i, 1 + pad, contentW, ST_TEXT, "%s", qRows[i]);
    for (i = 0; i < nCount; i++)
        render_text_clipped(top + 1 + qCount + i, 1 + pad, contentW, inputNoteStyle,
                            "%s", nRows[i]);

    /* horizontal scroll: show the rightmost part that still leaves the cursor
       on screen */
    /* one column is kept free for the cursor, so it never sits on the border */
    avail = contentW - prefixW - 1;
    if (avail < 1) avail = 1;
    bufW = lebar_tampil(buffer);
    shown = buffer;
    while (buffer[off] != '\0' && bufW - shownW > avail) {
        int adv = cpLenAt(buffer + off, (int)strlen(buffer) - off);
        shownW += widthOfBytes(buffer + off, adv);
        off += adv;
        shown = buffer + off;
    }
    render_text(top + 1 + qCount + nCount, 1 + pad, ST_ACCENT, "%s", "> ");
    render_text_clipped(top + 1 + qCount + nCount, 1 + pad + prefixW, avail, ST_TEXT,
                        "%s", shown);

    inputCursorRow = top + 1 + qCount + nCount;
    inputCursorCol = 1 + pad + prefixW + (bufW - shownW);
    if (inputCursorCol > 1 + pad + contentW) inputCursorCol = 1 + pad + contentW;
}

/* Redraws the box, flushes the frame and parks the terminal cursor inside the
   input row, right after the typed text. */
static void drawInputFrame(void)
{
    render_clear();
    render_input_box(inputPrompt, inputBuf);
    ui_flush();
    printf("\033[%d;%dH", inputCursorRow + 1, inputCursorCol + 1);
    fflush(stdout);
}

void ui_input_screen(const char *name)
{
    snprintf(inputScreen, sizeof inputScreen, "%s", name);
}

void ui_line_begin(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(inputPrompt, sizeof inputPrompt, fmt, args);
    va_end(args);
    inputBuf[0] = '\0';
    inputNote[0] = '\0';
    inputNoteStyle = ST_DANGER;
    if (!caps_interactive()) {
        printf("%s", inputPrompt);
        fflush(stdout);
        return;
    }
    drawInputFrame();
    term_cursor(1);
}

void ui_line_end(void)
{
    if (caps_interactive()) term_cursor(0);
}

void ui_line_prompt(void)
{
    if (!caps_interactive()) {
        printf("%s", inputPrompt);
        fflush(stdout);
        return;
    }
    drawInputFrame();
}

/* Shared tail: set the note line, redraw the box with whatever has been typed
   so far, and park the cursor back on the input row. */
static void lineNote(Style st, const char *buf)
{
    snprintf(inputNote, sizeof inputNote, "%s", buf);
    inputNoteStyle = st;
    drawInputFrame();
}

void ui_line_error(const char *fmt, ...)
{
    va_list args;
    char buf[192];
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    if (!caps_interactive()) {
        fputs(buf, stdout);
        printf("\n");
        fflush(stdout);
        return;
    }
    lineNote(ST_DANGER, buf);
}

void ui_line_note(const char *fmt, ...)
{
    va_list args;
    char buf[192];
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    if (!caps_interactive()) {
        printf("%s\n", buf);
        fflush(stdout);
        return;
    }
    lineNote(ST_SUCCESS, buf);
}

/* Reads one line inside the input box. 1 line, 0 EOF, -1 Esc. */
int ui_line_read(char *buf, int max)
{
    int len;

    if (!caps_interactive()) {
        if (fgets(buf, max, stdin) == NULL) return 0;
        buf[strcspn(buf, "\r\n")] = '\0';
        return 1;
    }
    if (max < 2) {
        buf[0] = '\0';
        return 1;
    }
    len = (int)strlen(inputBuf);
    for (;;) {
        char ch;
        TermKey k = term_getkey(&ch);

        if (k == KEY_EOF) {
            snprintf(buf, (size_t)max, "%s", inputBuf);
            return 0;
        }
        if (k == KEY_RESIZE) {
            term_size_query();
            drawInputFrame();
            continue;
        }
        if (k == KEY_ENTER) {
            snprintf(buf, (size_t)max, "%s", inputBuf);
            return 1;
        }
        if (k == KEY_ESC) {
            buf[0] = '\0';
            inputBuf[0] = '\0';
            return -1;
        }
        if (k != KEY_CHAR) continue;
        if (ch == '\r' || ch == '\n') {
            snprintf(buf, (size_t)max, "%s", inputBuf);
            return 1;
        }
        if (ch == 0x7f || ch == '\b') {
            if (len > 0) {
                len--;
                while (len > 0 && ((unsigned char)inputBuf[len] & 0xC0) == 0x80) len--;
                inputBuf[len] = '\0';
                drawInputFrame();
            }
            continue;
        }
        if ((unsigned char)ch >= 0x20 && len < max - 1) {
            inputBuf[len++] = ch;
            inputBuf[len] = '\0';
            drawInputFrame();
        }
    }
}

/* Row picker for tables. Keys: arrows move, Enter confirms, Esc cancels. */
int ui_pick_row(int count, UiDrawFn draw, void *ctx)
{
    int sel = 0;
    char ch;
    if (count <= 0) return -1;
    if (!caps_interactive()) {
        /* piped input: draw once, then read a 1-based row number; 0 cancels */
        draw(ctx, 0);
        ui_flush();
        sel = readInt("Pilih baris (0 batal): ", 0, count);
        return sel > 0 ? sel - 1 : -1;
    }
    for (;;) {
        draw(ctx, sel);
        ui_flush();
        {
            TermKey k = term_getkey(&ch);
            keyWait((int)k);
            switch (k) {
            case KEY_UP: sel = (sel + count - 1) % count; break;
            case KEY_DOWN: sel = (sel + 1) % count; break;
            case KEY_ENTER: return sel;
            case KEY_ESC: return -1;
            case KEY_RESIZE: break;
            case KEY_CHAR:
                if (ch == 'w' || ch == 'k') sel = (sel + count - 1) % count;
                else if (ch == 's' || ch == 'j') sel = (sel + 1) % count;
                break;
            default: break;
            }
        }
    }
}

/* Legacy print-style API: only the screens not yet migrated still use these. */
int ui_utf8_enabled(void) { return caps_unicode(); }

void clear_screen(void)
{
    fputs("\033[2J\033[H", stdout);
}

void draw_box_top(const char *title)
{
    int width = 50 - 2 - (int)strlen(title);
    if (width < 0) width = 0;
    printf("%s+%s%s%s%s", ANSI_CYAN, ANSI_RESET, title, ANSI_CYAN, ANSI_RESET);
    while (width-- > 0) putchar('-');
    printf("%s+%s\n", ANSI_CYAN, ANSI_RESET);
}

void draw_box_bottom(void)
{
    int i;
    putchar('+');
    for (i = 0; i < 48; i++) putchar('-');
    printf("+\n");
}

void draw_box_line(const char *fmt, ...)
{
    va_list args;
    char line[256];
    int width, i;
    va_start(args, fmt);
    vsnprintf(line, sizeof line, fmt, args);
    va_end(args);
    width = 50 - 4 - (int)ui_visible_len(line);
    if (width < 0) width = 0;
    printf("| %s", line);
    for (i = 0; i < width; i++) putchar(' ');
    printf(" |\n");
}

/* Renders "HP [####------] 80/100" into buf. */
void format_hp_bar(char *buf, int bufSize, int hp, int maxHp)
{
    int filled = maxHp > 0 ? hp * 10 / maxHp : 0;
    int i, n = 0;
    if (filled < 0) filled = 0;
    if (filled > 10) filled = 10;
    n += snprintf(buf + n, bufSize - n, "HP [%s", hp_color(hp, maxHp));
    for (i = 0; i < filled; i++) buf[n++] = '#';
    for (i = filled; i < 10; i++) buf[n++] = '-';
    snprintf(buf + n, bufSize - n, "%s] %d/%d%s", ANSI_RESET, hp, maxHp, ANSI_RESET);
}

void draw_header(const char *title, const Player *p)
{
    draw_box_top(title);
    draw_box_line("Pemain : %s", p->name);
    draw_box_line("Level  : %d (Skor %d)", p->level, p->score);
    {
        char bar[128];
        format_hp_bar(bar, sizeof bar, p->hp, p->maxHp);
        draw_box_line("%s", bar);
    }
    draw_box_line("Gold   : %s%d%s   Potion: %d", ANSI_YELLOW, p->gold, ANSI_RESET, p->potion);
    draw_box_bottom();
}

void draw_hp_bar(int hp, int maxHp)
{
    int filled = maxHp > 0 ? hp * 10 / maxHp : 0;
    int i;
    if (filled < 0) filled = 0;
    if (filled > 10) filled = 10;
    printf("HP [%s", hp_color(hp, maxHp));
    for (i = 0; i < filled; i++) putchar('#');
    for (i = filled; i < 10; i++) putchar('-');
    printf("%s] %d/%d%s\n", ANSI_RESET, hp, maxHp, ANSI_RESET);
}

void draw_menu(int number, const char *label)
{
    printf(" %d. %s\n", number, label);
}

const char *rarity_color(int rarity)
{
    switch (rarity) {
    case RARITY_LEGENDARY: return ANSI_MAGENTA;
    case RARITY_EPIC: return ANSI_YELLOW;
    case RARITY_RARE: return ANSI_CYAN;
    default: return ANSI_RESET;
    }
}

const char *hp_color(int hp, int maxHp)
{
    int pct = maxHp > 0 ? hp * 100 / maxHp : 0;
    if (pct > 60) return ANSI_GREEN;
    if (pct > 30) return ANSI_YELLOW;
    return ANSI_RED;
}
