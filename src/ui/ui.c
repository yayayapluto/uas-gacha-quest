#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "config.h"
#include "util.h"
#include "ui.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

static char lastMessage[256];

void ui_init(void)
{
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode))
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(65001);
#endif
}

void clear_screen(void)
{
    fputs("\033[2J\033[H", stdout);
}

int ui_utf8_enabled(void)
{
    return 1;
}

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

void ui_wait_enter(const char *text)
{
    char buf[16];
    if (!isatty(fileno(stdin))) return;
    printf("%s%s%s", ANSI_DIM, text, ANSI_RESET);
    fflush(stdout);
    if (!readLine(buf, sizeof buf)) clearerr(stdin);
}

/* Printable width of a string, ignoring ANSI escape sequences. */
static int visible_len(const char *s)
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
    }
    return n;
}

void draw_box_top(const char *title)
{
    int width = UI_WIDTH - 2 - visible_len(title);
    if (width < 0) width = 0;
    printf("%s+%s%s%s%s", ANSI_CYAN, ANSI_RESET, title, ANSI_CYAN, ANSI_RESET);
    while (width-- > 0) putchar('-');
    printf("%s+%s\n", ANSI_CYAN, ANSI_RESET);
}

void draw_box_bottom(void)
{
    int i;
    putchar('+');
    for (i = 0; i < UI_WIDTH - 2; i++) putchar('-');
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
    width = UI_WIDTH - 4 - visible_len(line);
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
