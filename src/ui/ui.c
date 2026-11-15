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

typedef struct {
    char text[192];
    Style st;
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

void ui_log_style(Style st, const char *fmt, ...)
{
    va_list args;
    if (logCount < 24) {
        LogLine *l = &logLines[logCount];
        va_start(args, fmt);
        vsnprintf(l->text, sizeof l->text, fmt, args);
        va_end(args);
        l->st = st;
        logCount++;
    }
    if (!caps_interactive()) {
        /* fallback mode prints the line right away, exactly like the old printf */
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }
}

void ui_log(const char *fmt, ...)
{
    va_list args;
    char buf[192];
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    ui_log_style(ST_TEXT, "%s", buf);
}

int ui_log_count(void) { return logCount; }

const char *ui_log_text(int i)
{
    if (i < 0 || i >= logCount) return "";
    return logLines[i].text;
}

Style ui_log_style_at(int i)
{
    if (i < 0 || i >= logCount) return ST_TEXT;
    return logLines[i].st;
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
    ui_hp_bar(row + 3, col + 5, w - 6, p->hp, p->maxHp);
    render_text(row + 4, col + 2, ST_TEXT, "Gold   : ");
    snprintf(num, sizeof num, "%d", p->gold);
    render_text(row + 4, col + 2 + 9, ST_GOLD, "%s", num);
    off = 2 + 9 + ui_visible_len(num);
    render_text(row + 4, col + off, ST_TEXT, "   Potion: %d", p->potion);
}

void ui_wait_enter(const char *text)
{
    if (!caps_interactive()) {
        char buf[16];
        if (!isatty(fileno(stdin))) return;
        printf("\033[1;36m%s%s", text, ANSI_RESET);
        fflush(stdout);
        if (!readLine(buf, sizeof buf)) clearerr(stdin);
        return;
    }
    {
        char ch;
        /* the pause hint must be readable: it is the only clue that arrow
           keys do nothing until Enter is pressed */
        render_text(render_max_row() + 2, 0, ST_ACCENT, "%s", text);
        ui_flush();
        for (;;) {
            TermKey k = term_getkey(&ch);
            if (k == KEY_ENTER || k == KEY_ESC) break;
            if (k == KEY_EOF) break;
        }
    }
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
                default:
                    break;
                }
            }
        }
    }
}

static char inputScreen[64] = "Input";
static char inputPrompt[192];
static int promptRow, promptCol;

void ui_input_screen(const char *name)
{
    snprintf(inputScreen, sizeof inputScreen, "%s", name);
}

/* Input frame: single-line header, panel with label, accent "> " prompt.
   The prompt sits on grid row 3 (baris 4), independent of terminal size. */
static void drawInputFrame(void)
{
    render_clear();
    if (caps_unicode())
        render_text(0, 0, ST_TITLE, "GACHA QUEST · %s", inputScreen);
    else
        render_text(0, 0, ST_TITLE, "GACHA QUEST > %s", inputScreen);
    ui_panel(1, 0, 50, 5, "Input");
    render_text(2, 2, ST_TEXT, "%s", inputPrompt);
    promptRow = 3;
    promptCol = 2;
    render_text(promptRow, 0, ST_ACCENT, "> ");
    render_text(render_max_row() + 2, 0, ST_DIM, "Enter konfirmasi  Esc batal");
}

void ui_line_begin(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(inputPrompt, sizeof inputPrompt, fmt, args);
    va_end(args);
    if (!caps_interactive()) {
        printf("%s", inputPrompt);
        fflush(stdout);
        return;
    }
    drawInputFrame();
    ui_flush();
    printf("\033[%d;%dH", promptRow + 1, promptCol + 1);
    fflush(stdout);
    term_cursor(1);
    term_cooked();
}

void ui_line_end(void)
{
    if (caps_interactive()) {
        term_cursor(0);
        term_raw();
    }
}

void ui_line_prompt(void)
{
    if (!caps_interactive()) {
        printf("%s", inputPrompt);
        fflush(stdout);
        return;
    }
    printf("\033[%d;%dH", promptRow + 1, promptCol + 1);
    fflush(stdout);
}

/* Shared tail: redraw the frame, print one line below the prompt, put the
   cursor back. The frame redraw wipes the old prompt and typed text. */
static void lineNote(Style st, const char *buf)
{
    drawInputFrame();
    render_text(promptRow + 1, 0, st, "%s", buf);
    ui_flush();
    printf("\033[%d;%dH", promptRow + 1, promptCol + 1);
    fflush(stdout);
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

/* Row picker for tables. Keys: arrows move, Enter confirms, Esc cancels. */
int ui_pick_row(int count, UiDrawFn draw, void *ctx)
{
    int sel = 0;
    char ch;
    if (!caps_interactive() || count <= 0) return -1;
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
